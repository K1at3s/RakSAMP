const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const crypto = require('crypto');
const { Client, GatewayIntentBits, ActionRowBuilder, ButtonBuilder, ButtonStyle, EmbedBuilder } = require('discord.js');

const config = require('./config');
const License = require('./models/License');
const Guild = require('./models/Guild');
const guildUtils = require('./utils/guildUtils');
const socketStore = require('./utils/socketStore');
const { loadCommands, registerCommands, registerForGuild, handleInteraction } = require('./handlers/commandHandler');
const db = require('./db');

// --- HWID ONAY YAPILANDIRMASI (yalnızca sahibin sunucusu) ---
const OWNER_ID = '1065352859195015260';          // sadece bu kullanıcı onaylayıp reddedebilir
const OWNER_GUILD_ID = '1079118636729520139';    // butonlar bu sunucuda anlamlı
const OWNER_LOG_CHANNEL_ID = '1537910799287459880'; // bildirimler bu kanala otomatik düşer

// --- EXPRESS & SOCKET.IO SETUP ---
const app = express();
app.use(express.json());
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: "*" } });

// --- MYSQL (db.js shim) ---
require('./db').init().then(() => console.log(`✔ Veri katmanı hazır (DB=${config.DB})`)).catch(err => console.error(err));

// --- DISCORD BOT SETUP ---
const client = new Client({
    intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildMessages, GatewayIntentBits.MessageContent]
});

client.once('clientReady', async () => {
    loadCommands();
    await registerCommands(client);
    client.user.setPresence({
        status: 'online',
        activities: [{ name: 'RakSAMP', type: 0 }] // type 0 = "Playing" (oynuyor)
    });
    console.log(`🤖 Discord Bot Aktif: ${client.user.tag}`);
    // Botun ekli olduğu sunucularda eksik log kanallarını otomatik oluştur
    for (const g of client.guilds.cache.values()) {
        await ensureGuildChannels(g.id);
    }
});

// Yeni sunucuya eklendiğinde komutları o sunucuya kaydet + log kanallarını oluştur
client.on('guildCreate', async (guild) => {
    const data = loadCommands().map(c => c.data.toJSON());
    await registerForGuild(guild, data);
    await ensureGuildChannels(guild.id);
});

client.on('interactionCreate', async (interaction) => {
    // Lisans Onaylama Butonları (yalnızca sahip; başkası basarsa uyar)
    if (interaction.isButton() && (interaction.customId.startsWith('approve_') || interaction.customId.startsWith('reject_'))) {
        if (interaction.user.id !== OWNER_ID || interaction.guildId !== OWNER_GUILD_ID) {
            return interaction.reply({ content: 'Bu butonları yalnızca sunucu sahibi kullanabilir.', ephemeral: true });
        }
        const hwid = interaction.customId.replace(/^(approve|reject)_/, '');

        if (interaction.customId.startsWith('approve_')) {
            await License.findOneAndUpdate({ hwid }, { isApproved: true }, { upsert: true });
            const embed = new EmbedBuilder()
                .setTitle("✅ HWID Lisansı Onaylandı")
                .setDescription(`**HWID:** \`${hwid}\` cihazına erişim izni verildi.`)
                .setColor(0x00FF00)
                .setTimestamp();
            await interaction.update({ embeds: [embed], components: [] });
        } else {
            await License.findOneAndUpdate({ hwid }, { isApproved: false }, {});
            const embed = new EmbedBuilder()
                .setTitle("❌ HWID Lisansı Reddedildi")
                .setDescription(`**HWID:** \`${hwid}\` cihazına erişim izni verilmedi.`)
                .setColor(0xFF0000)
                .setTimestamp();
            await interaction.update({ embeds: [embed], components: [] });
        }
        return;
    }

    await handleInteraction(interaction);
});

client.login(config.DISCORD_BOT_TOKEN).catch(console.error);

// --- YARDIMCI FONKSİYONLAR ---

// Discord'a giden her kanal tipi için otomatik kanal oluşturur (yoksa) ve guild
// kaydına (guildId ile) yazar. Neler discord'a gidiyorsa hepsi burada: lisans log,
// war, gstats, grup chat, zone war, backup, welcome. Kanal adları bot-<tip>.
const AUTO_CHANNELS = [
    { field: 'logChannelId', name: 'bot-log', topic: 'Lisans bildirimleri' },
    { field: 'warChannelId', name: 'bot-war', topic: 'War mesajları' },
    { field: 'gstatsChannelId', name: 'bot-gstats', topic: 'Grup istatistikleri' },
    { field: 'groupChatChannelId', name: 'bot-groupchat', topic: 'Grup chat köprüsü' },
    { field: 'zoneWarChannelId', name: 'bot-zonewar', topic: 'ZONE WAR bildirimleri' },
    { field: 'backupChannelId', name: 'bot-backup', topic: 'Backup çağrıları' },
    { field: 'welcomeChannelId', name: 'bot-welcome', topic: 'Welcome back mesajları' }
];

async function ensureGuildChannels(guildId) {
    try {
        const guild = await client.guilds.fetch(guildId);
        if (!guild) return;
        const rec = await guildUtils.getGuild(guildId);
        const updates = {};
        for (const ac of AUTO_CHANNELS) {
            if (rec[ac.field]) continue; // zaten ayarlı
            const existing = guild.channels.cache.find(ch => ch.name === ac.name && ch.type === 0);
            let chId = existing ? existing.id : null;
            if (!chId) {
                const created = await guild.channels.create({ name: ac.name, topic: ac.topic, type: 0 });
                chId = created.id;
            }
            updates[ac.field] = chId;
        }
        if (Object.keys(updates).length) {
            await Guild.findOneAndUpdate({ guildId }, { $set: updates }, { upsert: true, setDefaultsOnInsert: true });
            console.log(`📁 OTO-KANAL guild=${guildId}: ${Object.keys(updates).join(', ')} oluşturuldu/atandı`);
        }
    } catch (e) {
        console.error('Otomatik kanal oluşturma hatası:', e.message);
    }
}

// hwid -> oyun içine iletilecek bekleyen Discord mesajları (geçici, bellekte)
const groupChatOutbox = new Map();

function queueOutbox(hwid, payload) {
    if (!hwid) return;
    const list = groupChatOutbox.get(hwid) || [];
    if (list.length >= 50) list.shift(); // taşmayı önle
    list.push(payload);
    groupChatOutbox.set(hwid, list);
}

// Ortak kanal mesajı gönderim yardımcısı (slash komutlar da kullanır)
const { setClient, sendToGuildChannel } = require('./utils/discordNotifier');
setClient(client);

// RakSAMP isteğini bağlı Discord sunucusuna eşle
async function resolveGuildForRakSAMP(body) {
    if (body.groupId) return guildUtils.getGuild(body.groupId);

    let hwid = body.hwid || null;
    if (!hwid && body.sessionToken) {
        try {
            hwid = decryptSession(body.sessionToken).hwid;
        } catch (e) {}
    }
    if (hwid) return guildUtils.getGuildByHwid(hwid);

    return null;
}

// HWID lisans bildirimi — sahibin sabit log kanalına butonlu embed atar (otomatik).
// Sunucu/kanal YAPILANDIRMADAN sabit: tek sahibin tek kanalı (users'ın kendi sunucusu).
async function notifyUnapprovedDevice(hwid) {
    const channelId = OWNER_LOG_CHANNEL_ID;

    try {
        const channel = await client.channels.fetch(channelId);
        if (!channel) {
            console.log(`⚠️ Lisanssız cihaz (HWID: ${hwid}) — log kanalı (${channelId}) bulunamadı, bildirim atlanıyor.`);
            return;
        }

        const row = new ActionRowBuilder().addComponents(
            new ButtonBuilder()
                .setCustomId(`approve_${hwid}`)
                .setLabel('✅ Lisans Onayla')
                .setStyle(ButtonStyle.Success),
            new ButtonBuilder()
                .setCustomId(`reject_${hwid}`)
                .setLabel('❌ Reddet')
                .setStyle(ButtonStyle.Danger)
        );

        const embed = new EmbedBuilder()
            .setTitle("⚠️ Lisanssız Cihaz Girişi Belirlendi")
            .setDescription(`Yetkisiz bir RakSAMP istemcisi bağlanmaya çalıştı.\n\n**HWID:** \`${hwid}\``)
            .setColor(0xFF0000)
            .setTimestamp();

        await channel.send({ embeds: [embed], components: [row] });
    } catch (e) {
        console.error("Discord bildirim hatası:", e);
    }
}

// Session token'dan hwid çöz (AES)
function decryptSession(sessionToken) {
    const [ivHex, encrypted] = sessionToken.split(':');
    const iv = Buffer.from(ivHex, 'hex');
    const decipher = crypto.createDecipheriv('aes-256-cbc', config.AES_SECRET_KEY, iv);
    let decrypted = decipher.update(encrypted, 'hex', 'utf8');
    decrypted += decipher.final('utf8');
    return JSON.parse(decrypted);
}

// --- REST API: HANDSHAKE & SECURITY ---
app.post('/api/handshake', async (req, res) => {
    const { hwid } = req.body;

    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    let record = await License.findOne({ hwid });

    if (!record || !record.isApproved) {
        if (!record) {
            await License.create({ hwid, isApproved: false });
        }

        await notifyUnapprovedDevice(hwid);

        return res.status(403).json({ success: false, message: "Cihaz lisanslı değil veya onay bekliyor." });
    }

    // Lisanslı Cihaz İçin Güvenli Oturum Token'ı Üret (AES)
    const sessionData = JSON.stringify({ hwid, timestamp: Date.now() });
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipheriv('aes-256-cbc', config.AES_SECRET_KEY, iv);
    let encrypted = cipher.update(sessionData, 'utf8', 'hex');
    encrypted += cipher.final('hex');
    const sessionToken = iv.toString('hex') + ':' + encrypted;

    return res.json({
        success: true,
        sessionToken,
        config: {
            commandPrefix: "?"
        }
    });
});

// RakSAMP /gstats verisi geldiğinde bağlı sunucunun gstats kanalına embed olarak yaz.
// requesterId geldiyse sonuç, komutu çalıştıran Discord kullanıcısına reply olarak gönderilir
// (kanala ayrıca atılmaz).
app.post('/api/gstats', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    // SAMP renk kodlarını ({RRGGBB} gibi) temizle
    const stripColor = (t) => String(t || '').replace(/\{[0-9a-fA-F]{6}\}/g, '').trim();
    const statsText = stripColor(body.statsText);
    const groupName = body.groupName || '';
    const playerName = body.playerName || 'Bir oyuncu';
    const requesterId = body.requesterId || null;

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const channelId = guild.gstatsChannelId || null;
    console.log(`📊 GStats (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${statsText.slice(0, 200)}`);

    // Önce interaction'a reply dene
    let replied = false;
    if (requesterId) {
        const interaction = getPendingInteraction(requesterId);
        if (interaction) {
            try {
                const embed = new EmbedBuilder()
                    .setTitle("📊 Group Stats")
                    .setDescription(statsText.slice(0, 3000))
                    .setColor(0x3498DB)
                    .setTimestamp();
                if (groupName) embed.setAuthor({ name: groupName });
                if (playerName) embed.setFooter({ text: `${playerName} tarafından istendi` });
                await interaction.editReply({ embeds: [embed] });
                removePendingInteraction(requesterId);
                replied = true;
            } catch (e) {
                console.error("GStats interaction reply hatası:", e);
            }
        }
    }

    if (!replied && channelId) {
        try {
            const channel = await client.channels.fetch(channelId);
            if (channel) {
                const embed = new EmbedBuilder()
                    .setTitle("📊 Group Stats")
                    .setDescription(statsText.slice(0, 3000))
                    .setColor(0x3498DB)
                    .setTimestamp();

                if (groupName) embed.setAuthor({ name: groupName });
                if (playerName) embed.setFooter({ text: `${playerName} tarafından istendi` });

                await channel.send({ embeds: [embed] });
            }
        } catch (e) {
            console.error("GStats Discord gönderim hatası:", e);
        }
    }

    return res.json({ success: true, channelId, replied });
});

// RakSAMP /admins verisi geldiğinde bağlı sunucunun kanalına embed olarak yaz.
// requesterId geldiyse sonuç, komutu çalıştıran Discord kullanıcısına reply olarak gönderilir.
app.post('/api/admins', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    const stripColor = (t) => String(t || '').replace(/\{[0-9a-fA-F]{6}\}/g, '').trim();
    const adminsText = stripColor(body.adminsText);
    const playerName = body.playerName || 'Bir oyuncu';
    const requesterId = body.requesterId || null;

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    console.log(`🛡️ Admins (guild=${guild.guildId}): ${adminsText.slice(0, 200)}`);

    let replied = false;
    if (requesterId) {
        const interaction = getPendingInteraction(requesterId);
        if (interaction) {
            try {
                const embed = new EmbedBuilder()
                    .setTitle("🛡️ Online Adminler")
                    .setDescription(adminsText.slice(0, 3000))
                    .setColor(0xE67E22)
                    .setTimestamp();
                if (playerName) embed.setFooter({ text: `${playerName} tarafından istendi` });
                await interaction.editReply({ embeds: [embed] });
                removePendingInteraction(requesterId);
                replied = true;
            } catch (e) {
                console.error("Admins interaction reply hatası:", e);
            }
        }
    }

    if (!replied && guild.logChannelId) {
        try {
            const channel = await client.channels.fetch(guild.logChannelId);
            if (channel) {
                const embed = new EmbedBuilder()
                    .setTitle("🛡️ Online Adminler")
                    .setDescription(adminsText.slice(0, 3000))
                    .setColor(0xE67E22)
                    .setTimestamp();
                if (playerName) embed.setFooter({ text: `${playerName} tarafından istendi` });
                await channel.send({ embeds: [embed] });
            }
        } catch (e) {
            console.error("Admins Discord gönderim hatası:", e);
        }
    }

    return res.json({ success: true, replied });
});

// Oyun içi ?w (War Help) — Discord'a @everyone ile yardım çağrısı
app.post('/api/warhelp', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const playerName = body.playerName || 'Bir oyuncu';
    const ownGroup = body.ownGroup || '';
    const enemyGroup = body.enemyGroup || '';

    let text;
    if (ownGroup && enemyGroup) {
        text = `@everyone **${playerName}** - Needs help in group zone\n**${ownGroup}** vs **${enemyGroup}**`;
    } else {
        text = `@everyone **${playerName}** - Needs help in group zone`;
    }

    // Kanal ayarlanmadıysa null → sadece logla
    const channelId = guild.warChannelId || null;
    console.log(`⚔️ WARHELP (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${text}`);

    if (channelId) {
        try {
            const channel = await client.channels.fetch(channelId);
            if (channel) {
                await channel.send(text);
            }
        } catch (e) {
            console.error("WarHelp Discord gönderim hatası:", e);
        }
    }

    return res.json({ success: true, channelId });
});

// ZONE WAR bildirimi (oyun içi zone war algılandığında)
// action: keeps | takes | wins | loses
app.post('/api/zonewar', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const playerName = body.playerName || 'Bir oyuncu';
    const ownGroup = body.ownGroup || '';
    const enemyGroup = body.enemyGroup || '';
    const action = body.action || 'keeps';
    const zoneName = body.zoneName || '';

    const actionText = {
        keeps: 'keeps zone',
        takes: 'takes over the zone',
        wins: 'wins against',
        loses: 'loses to',
        join: 'is fighting a Zone War'
    }[action] || action;

    let description;
    if (action === 'wins' || action === 'loses') {
        description = `**${ownGroup || playerName}** ${actionText} **${enemyGroup || '?'}**`;
    } else if (action === 'join') {
        description = `**${ownGroup || playerName}** ${actionText}`;
    } else {
        description = `**${ownGroup || playerName}** ${actionText}${zoneName ? ` **${zoneName}**` : ''}`;
    }

    const embed = new EmbedBuilder()
        .setTitle("⚔️ ZONE WAR")
        .setDescription(description)
        .setColor(action === 'loses' ? 0xFF0000 : 0xFFA500)
        .setTimestamp();

    const channelId = await sendToGuildChannel(guild, 'zoneWarChannelId', { embed });
    console.log(`⚔️ ZONEWAR (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${description}`);

    return res.json({ success: true, channelId });
});

// GZ sonucu / kill list — oyun içi GZ bittiğinde toplanan textdraw sonucu
// (gz sonucl.txt) zonewar kanalına embed olarak gönderilir.
app.post('/api/killlist', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const killList = String(body.killList || '').trim().slice(0, 4000);
    const playerName = body.playerName || 'Bir oyuncu';
    const ownGroup = body.ownGroup || '';
    const enemyGroup = body.enemyGroup || '';

    if (!killList) {
        return res.json({ success: false, message: "Kill list boş." });
    }

    // 1024 karakteri aşarsa embed sığmaz -> text dosyası olarak at (Discord
    // embed description limiti 4096 ama 1024'ten sonra okunmaz hale gelir).
    const embed = new EmbedBuilder()
        .setTitle("⚔️ GZ Sonuc / Kill List")
        .setDescription(`\`\`\`\n${killList}\n\`\`\``)
        .setColor(0xFFA500)
        .setTimestamp();

    if (ownGroup) embed.setAuthor({ name: ownGroup });
    if (enemyGroup) embed.addFields({ name: 'Rakip Grup', value: enemyGroup, inline: true });
    embed.setFooter({ text: `${playerName} tarafından alındı` });

    let channelId;
    if (killList.length > 1024) {
        channelId = await sendToGuildChannel(guild, 'zoneWarChannelId', {
            files: [{
                attachment: Buffer.from(killList, 'utf8'),
                name: `gz-sonuc-${Date.now()}.txt`
            }]
        });
        console.log(`📄 KILLLIST (guild=${guild.guildId}, kanal=${channelId || 'null'}): 1024+ karakter, dosya olarak gönderildi (${killList.length} bayt)`);
    } else {
        channelId = await sendToGuildChannel(guild, 'zoneWarChannelId', { embed });
        console.log(`⚔️ KILLLIST (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${killList.slice(0, 200)}`);
    }

    return res.json({ success: true, channelId });
});

// ?1414 backup çağrısı — backup kanalına @here ile acil yardım
app.post('/api/backup', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const playerName = body.playerName || 'Bir oyuncu';
    const ownGroup = body.ownGroup || '';

    const channelId = await sendToGuildChannel(guild, 'backupChannelId', {
        content: `@here **${ownGroup || 'Grup'} backup istiyor!**\n${playerName} oyun içinde acil destek çağırıyor.`
    });
    console.log(`🆘 BACKUP (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${playerName} (${ownGroup})`);

    return res.json({ success: true, channelId });
});

// welcome back mesajı (GROUP: X logged in → oyun içi welcome)
app.post('/api/welcome', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);

    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const playerName = body.playerName || 'Bir oyuncu';
    const groupName = body.groupName || '';

    // Kişiye özel welcome mesajı (oyun içi ?welcomemsg ile ayarlanır)
    const welcomeMessage = String(body.welcomeMessage || '').trim().slice(0, 300);

    // HWID'e bağlı welcome açık/kapalı durumunu kontrol et (oyun içi ?greet)
    let enabled = true;
    let hwid = body.hwid || null;
    if (hwid) {
        const lic = await License.findOne({ hwid });
        enabled = lic ? lic.welcomeEnabled : true;
    }

    if (!enabled) {
        return res.json({ success: true, enabled: false, message: "Welcome back mesajları kapalı." });
    }

    let description = `**${playerName}**${groupName ? ` — ${groupName}` : ''} oyuna geri döndü.`;
    if (welcomeMessage) {
        description += `\n\n*"${welcomeMessage}"*`;
    }

    const embed = new EmbedBuilder()
        .setTitle("👋 Welcome Back")
        .setDescription(description)
        .setColor(0x00AEFF)
        .setTimestamp();

    const channelId = await sendToGuildChannel(guild, 'welcomeChannelId', { embed });
    console.log(`👋 WELCOME (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${playerName}`);

    return res.json({ success: true, enabled: true, channelId });
});

// ?greet — welcome back mesajlarını aç/kapat (kişiye özel, HWID bazlı)
app.post('/api/welcome/toggle', async (req, res) => {
    const body = req.body || {};
    const hwid = body.hwid || null;

    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    let lic = await License.findOne({ hwid });
    if (!lic) {
        lic = await License.create({ hwid, isApproved: false });
    }

    lic.welcomeEnabled = !lic.welcomeEnabled;
    await lic.save();

    console.log(`👋 WELCOME TOGGLE (hwid=${hwid}): ${lic.welcomeEnabled ? 'AÇILDI' : 'KAPANDI'}`);
    return res.json({ success: true, enabled: !!lic.welcomeEnabled });
});

// ?welcomemsg <mesaj> — kişiye özel welcome mesajı ayarla (HWID bazlı)
app.post('/api/welcome/message', async (req, res) => {
    const body = req.body || {};
    const hwid = body.hwid || null;
    const welcomeMessage = String(body.welcomeMessage || '').trim().slice(0, 300);

    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    let lic = await License.findOne({ hwid });
    if (!lic) {
        lic = await License.create({ hwid, isApproved: false });
    }

    lic.welcomeMessage = welcomeMessage;
    await lic.save();

    console.log(`✉️ WELCOME MSG (hwid=${hwid}): ${welcomeMessage || '(silindi)'}`);
    return res.json({ success: true, welcomeMessage });
});

// Welcome ayarını çek (client başlangıçta kişisel mesajı yükler)
app.get('/api/welcome/state', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    if (!hwid) {
        return res.status(400).json({ success: false, message: "HWID eksik." });
    }

    const lic = await License.findOne({ hwid });
    return res.json({
        success: true,
        welcomeEnabled: lic ? !!lic.welcomeEnabled : true,
        welcomeMessage: lic ? (lic.welcomeMessage || '') : ''
    });
});

// Discord'dan oyuna komut iletme (client /api/command/poll ile çeker)
const { queueCommand, drainCommands } = require('./utils/commandOutbox');
const { getPendingInteraction, removePendingInteraction } = require('./utils/pendingReplies');

// Komut bekleme kuyruğunu oyundan çek (polling)
app.get('/api/command/poll', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    const commands = drainCommands(hwid);
    return res.json({ commands });
});

// --- SOCKET.IO REALTIME EVENTS ---
io.on('connection', (socket) => {
    console.log(`🔌 Yeni RakSAMP İstemcisi Bağlandı: ${socket.id}`);

    // İstemci HWID'sini bildirir (war/gstats hedeflemesi için)
    socket.on('register', (data) => {
        const hwid = (data && data.hwid) ? String(data.hwid) : null;
        socketStore.register(hwid, socket);
        console.log(`🎯 İstemci kaydedildi: ${socket.id} → HWID: ${hwid || 'bilinmiyor'}`);
    });

    socket.on('gstats_response', (data) => {
        console.log(`📊 /gstats Verisi Alındı:`, data);
        // C++ client httplib kullandığı için socket.emit almaz; burada socket üzerinden gelen
        // veriyi doğrudan /api/gstats aynı kanal mantığıyla işleyelim (fallback).
        const { guildId, statsText } = data || {};
        if (guildId && statsText) {
            resolveGuildForRakSAMP({ groupId: guildId }).then(async (guild) => {
                if (!guild) return;
                const channelId = guild.gstatsChannelId || null;
                if (channelId) {
                    try {
                        const channel = await client.channels.fetch(channelId);
                        if (channel) {
                            const clean = String(statsText).replace(/\{[0-9a-fA-F]{6}\}/g, '').trim();
                            const embed = new EmbedBuilder()
                                .setTitle("📊 Group Stats")
                                .setDescription(clean.slice(0, 3000))
                                .setColor(0x3498DB)
                                .setTimestamp();
                            await channel.send({ embeds: [embed] });
                        }
                    } catch (e) {
                        console.error("GStats socket gönderim hatası:", e);
                    }
                }
            });
        }
    });

    socket.on('disconnect', () => {
        socketStore.unregister(socket);
        console.log(`❌ İstemci Ayrıldı: ${socket.id}`);
    });
});

// Discord grup chat kanalında ! ile başlayan mesajları oyun içine ilet
client.on('messageCreate', async (message) => {
    if (message.author.bot) return;
    if (!message.guild) return;
    if (!message.content || !message.content.startsWith('!')) return;

    const guild = await guildUtils.getGuild(message.guild.id);
    if (!guild || !guild.groupChatEnabled || !guild.groupChatChannelId) return;
    if (message.channel.id !== guild.groupChatChannelId) return;

    const text = message.content.slice(1).trim();
    if (!text) return;

    console.log(`💬 DISCORD→OYUN (guild=${guild.guildId}): ${message.author.username}: ${text}`);
    const payload = { text, author: message.author.username };
    for (const hwid of guild.linkedHwids || []) {
        queueOutbox(hwid, payload);
    }
});

// --- GRUP CHAT <-> DISCORD KÖPRÜSÜ ---
// Oyun içi ?discordchat — MongoDB'deki aç/kapa durumunu değiştir
app.post('/api/groupchat/toggle', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    guild.groupChatEnabled = !guild.groupChatEnabled;
    await guild.save();

    console.log(`💬 GROUPCHAT (guild=${guild.guildId}): ${guild.groupChatEnabled ? 'AÇILDI' : 'KAPANDI'} (kanal=${guild.groupChatChannelId || 'null'})`);
    return res.json({ success: true, enabled: !!guild.groupChatEnabled, channelConfigured: !!guild.groupChatChannelId });
});

// Oyun içinden gelen ! mesajını Discord grup chat kanalına yaz
// Kanal ayarlanmamışsa needChannel=true döner, istemci gruba uyarı mesajı atar
app.post('/api/groupchat', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, enabled: false, needChannel: false });
    }
    if (!guild.groupChatEnabled) {
        return res.json({ success: false, enabled: false, needChannel: false });
    }

    const channelId = guild.groupChatChannelId || null;
    const text = String(body.text || '').slice(0, 200);
    const author = String(body.author || 'Oyuncu');

    if (!channelId) {
        console.log(`⚠️ GROUPCHAT (guild=${guild.guildId}): kanal ayarlanmamış, mesaj atlanıyor.`);
        return res.json({ success: false, enabled: true, needChannel: true });
    }

    try {
        const channel = await client.channels.fetch(channelId);
        if (channel) {
            await channel.send(`🎮 **${author}** (Oyun): ${text}`);
        }
    } catch (e) {
        console.error("GroupChat Discord gönderim hatası:", e);
    }

    return res.json({ success: true, enabled: true, channelConfigured: true });
});

// Oyun içi ?tr/?en — bot mesaj dilini MongoDB'de sakla (default "en")
app.post('/api/lang/set', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, lang: 'en' });
    }
    guild.lang = (String(body.lang || 'en') === 'tr') ? 'tr' : 'en';
    await guild.save();
    console.log(`🌐 LANG (guild=${guild.guildId}): ${guild.lang}`);
    return res.json({ success: true, lang: guild.lang });
});

// RakSAMP istemcisi bekleyen Discord mesajlarını ve köprü durumunu çeker (polling)
app.get('/api/groupchat/poll', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    const guild = await guildUtils.getGuildByHwid(hwid);
    const enabled = guild ? !!guild.groupChatEnabled : false;
    const channelConfigured = guild ? !!guild.groupChatChannelId : false;
    const messages = (groupChatOutbox.get(hwid) || []).slice();
    groupChatOutbox.delete(hwid);
    const out = { enabled, channelConfigured, messages };

    // Bot'un oyun içi grubunu guild kaydına yaz (poll'dan öğrenilir): log kanalı
    // eşleşmesi/dil bu grup adı üzerinden yürür, guild yoksa hiçbir şey değişmez.
    if (guild && req.query.group) {
        const grp = String(req.query.group).slice(0, 255);
        if (guild.groupName !== grp) {
            guild.groupName = grp;
            await guild.save();
            console.log(`🏷️ GROUP(guild=${guild.guildId}): ${grp}`);
        }
    }
    // Guild yoksa lang alanı GÖNDERİLMEZ: istemci lokal ?tr/?en seçimini korur
    // (guild.lang varsayılanı 'en' yoksa ezerdi).
    if (guild) out.lang = guild.lang;
    return res.json(out);
});

// Oyun içi ?guildid: bu HWID'i Discord sunucusuna bağlar (linkedHwids).
// Boylece botu ekleyen HER grup kendi sunucusuyla kullanabilir;
// mevcut tüm hwid tabanlı çağrılar (lang, groupchat, welcome...) otomatik çözülür.
app.post('/api/guildid', async (req, res) => {
    const body = req.body || {};
    const hwid = String(body.hwid || '').trim();
    const guildId = String(body.guildId || '').trim();
    if (!hwid || !/^\d+$/.test(guildId)) {
        return res.json({ success: false, message: "Gecersiz guild id." });
    }
    const guild = await guildUtils.getGuild(guildId);
    if (!guild.linkedHwids.includes(hwid)) {
        guild.linkedHwids.push(hwid);
        await guild.save();
    }
    console.log(`🔗 GUILDID (hwid=${hwid} -> guild=${guildId})`);
    return res.json({ success: true, guildId: guild.guildId });
});

// RakSAMP client'ı: oyuncu istatistik cache'ini DB'ye push eder (?pl verisi).
// Body: { players: [{ id, name, packetloss, fps, ping }, ...] } — id üzerinden upsert.

app.post('/api/player/cache', async (req, res) => {
    const players = (req.body || {}).players;
    if (!Array.isArray(players)) return res.status(400).json({ success: false, message: "players bekleniyor" });
    const rows = players
        .filter(p => Number.isInteger(p.id) && p.id >= 0 && p.id < 1000)
        .map(p => [p.id, String(p.name || '').slice(0, 64), Number(p.packetloss) || 0, Number(p.fps) || 0, Number(p.ping) || 0, new Date()]);
    if (!rows.length) return res.json({ success: true, count: 0 });
    try {
        // Cache dosyası tek doğru kaynak olduğundan tablo her push'ta sıfırlanıp
        // yeniden yazılır (eski/çıkmış oyuncu verisi burada kalmaz).
        await db.execute('DELETE FROM players');
        await db.query(
            `INSERT INTO players (id, name, packetloss, fps, ping, updated_at) VALUES ?`,
            [rows]
        );
        console.log(`🎮 PLAYER-CACHE: ${rows.length} oyuncu DB'ye yazildi`);
        return res.json({ success: true, count: rows.length });
    } catch (err) {
        console.error('PLAYER-CACHE hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// RakSAMP client'ı: grup renk haritasını DB'ye push eder.
// Body: { colors: [{ color, tag }, ...] } — color üzerinden upsert.
app.post('/api/group/colors', async (req, res) => {
    const colors = (req.body || {}).colors;
    if (!Array.isArray(colors)) return res.status(400).json({ success: false, message: "colors bekleniyor" });
    const rows = colors
        .filter(c => Number.isInteger(c.color) && c.color > 0)
        .map(c => [c.color >>> 0, String(c.tag || '').slice(0, 96), new Date()]);
    if (!rows.length) return res.json({ success: true, count: 0 });
    try {
        // Renk haritası da sil-üstüne-yaz: eski grup kayıtları veride kalmaz.
        await db.execute('DELETE FROM group_colors');
        await db.query(
            `INSERT INTO group_colors (color, tag, updated_at) VALUES ?`,
            [rows]
        );
        console.log(`🎨 GROUP-COLORS: ${rows.length} renk DB'ye yazildi`);
        return res.json({ success: true, count: rows.length });
    } catch (err) {
        console.error('GROUP-COLORS hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

server.listen(config.PORT, () => {
    console.log(`🚀 VDS Backend Aktif! Port: ${config.PORT}`);
});
