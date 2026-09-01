const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const crypto = require('crypto');
const { Client, GatewayIntentBits, ActionRowBuilder, ButtonBuilder, ButtonStyle, EmbedBuilder, StringSelectMenuBuilder } = require('discord.js');

const config = require('./config');
const License = require('./models/License');
const Guild = require('./models/Guild');
const guildUtils = require('./utils/guildUtils');
const socketStore = require('./utils/socketStore');
const { loadCommands, registerCommands, registerForGuild, handleInteraction } = require('./handlers/commandHandler');
const db = require('./db');

// Windows konsolunda emoji/Turkce duzgun gorunsun
if(process.platform==='win32'){ try{ require('child_process').execSync('chcp 65001', {stdio:'ignore'}); }catch{} }

// --- HWID ONAY YAPILANDIRMASI (yalnızca sahibin sunucusu) ---
const OWNER_ID = '1065352859195015260';          // sadece bu kullanıcı onaylayıp reddedebilir
const OWNER_GUILD_ID = '1079118636729520139';    // butonlar bu sunucuda anlamlı
const OWNER_LOG_CHANNEL_ID = '1537910799287459880'; // bildirimler bu kanala otomatik düşer

// --- GROLE/GRANK YETKI KONTROLU (DB -> Guild.allowedRoleId) ---
async function hasGrolePermission(interaction){
    try{
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if(!guild || !guild.allowedRoleId) return true; // kisitlama yoksa herkes
        if(interaction.member.roles.cache.has(guild.allowedRoleId)) return true;
        if(interaction.member.permissions.has('Administrator') || interaction.member.permissions.has('ManageGuild')) return true;
        if(interaction.guild.ownerId === interaction.user.id) return true;
        return false;
    }catch{ return true; }
}

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
    // /grole interaktif: uye secimi -> rol secimi (DB izinli rol kontrolu)
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grole_member_select:')) {
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: '❌ Bu menüyü kullanmaya yetkin yok.', ephemeral: true });
        const requesterId = interaction.customId.split(':')[1];
        const selectedMember = interaction.values[0];
        if (selectedMember === '__none__') return interaction.reply({ content: '❌ Üye bulunamadı.', ephemeral: true });
        const cache = groleMemberCache.get(requesterId);
        groleSelections.set(interaction.user.id, { target: selectedMember, guildId: interaction.guildId, hwid: cache ? cache.hwid : '' });
        const roleSelect = new StringSelectMenuBuilder()
            .setCustomId(`grole_role_select:${requesterId}:${encodeURIComponent(selectedMember)}`)
            .setPlaceholder('Rol seç...')
            .addOptions([
                { label: 'Member', value: 'Member' },
                { label: 'Co-Leader', value: 'Co-Leader' },
            ]);
        const row = new ActionRowBuilder().addComponents(roleSelect);
        const embed = new EmbedBuilder()
            .setTitle(`👤 ${selectedMember}`)
            .setDescription('Bu üyeye vermek istediğin rolü seç:')
            .setColor(0x2ECC71);
        return interaction.update({ embeds: [embed], components: [row] });
    }
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grole_role_select:')) {
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: '❌ Bu menüyü kullanmaya yetkin yok.', ephemeral: true });
        const parts = interaction.customId.split(':');
        const requesterId = parts[1];
        const member = decodeURIComponent(parts[2] || '');
        const role = interaction.values[0];
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.update({ content: '❌ Bağlı RakSAMP istemcisi yok.', embeds: [], components: [] });
        }
        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'grole', target: member, role, requesterId: interaction.id });
        setPendingInteraction(interaction.id, interaction);
        // 5 dk sonra otomatik kapat
        setTimeout(() => { interaction.editReply({ content: '✅ İşlem tamamlandı.', embeds: [], components: [] }).catch(()=>{}); removePendingInteraction(interaction.id); }, 2*60*1000);
        const embed = new EmbedBuilder()
            .setTitle('✅ Gönderildi')
            .setDescription(`\`/grole ${member} → ${role}\` oyuna iletildi — paginated listede aranıp uygulanacak.`)
            .setColor(0x00FF00);
        return interaction.update({ embeds: [embed], components: [] });
    }
    // Sayfali uye listesi butonlari (rate-limit dostu: tek edit ile sayfa degisir)
    if (interaction.isButton() && interaction.customId.startsWith('grole_page_')) {
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: '❌ Bu menüyü kullanmaya yetkin yok.', ephemeral: true });
        const [, dir, requesterId, pageStr] = interaction.customId.split(':');
        const cache = groleMemberCache.get(requesterId);
        if (!cache) return interaction.reply({ content: '❌ Liste süresi doldu, yeniden /grole çalıştır.', ephemeral: true });
        let page = parseInt(pageStr, 10) || 0;
        if (dir === 'prev') page = Math.max(0, page - 1);
        else if (dir === 'next') page = page + 1;
        grolePageCache.set(requesterId, page);
        const msg = buildGroleMemberMessage(requesterId, page);
        if (!msg) return interaction.reply({ content: '❌ Liste bulunamadı.', ephemeral: true });
        return interaction.update(msg);
    }
    if (interaction.isButton() && interaction.customId.startsWith('grank_page_')) {
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: '❌ Bu menüyü kullanmaya yetkin yok.', ephemeral: true });
        const [, dir, requesterId, pageStr] = interaction.customId.split(':');
        const cache = grankMemberCache.get(requesterId);
        if (!cache) return interaction.reply({ content: '❌ Liste süresi doldu, yeniden /grank çalıştır.', ephemeral: true });
        let page = parseInt(pageStr, 10) || 0;
        if (dir === 'prev') page = Math.max(0, page - 1);
        else if (dir === 'next') page = page + 1;
        grankPageCache.set(requesterId, page);
        const msg = buildGrankMemberMessage(requesterId, page);
        if (!msg) return interaction.reply({ content: '❌ Liste bulunamadı.', ephemeral: true });
        return interaction.update(msg);
    }
    // /grank interaktif: uye secimi -> rank secimi (modal veya select)
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grank_member_select:')) {
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: '❌ Bu menüyü kullanmaya yetkin yok.', ephemeral: true });
        const requesterId = interaction.customId.split(':')[1];
        const selectedMember = interaction.values[0];
        if (selectedMember === '__none__') return interaction.reply({ content: '❌ Üye bulunamadı.', ephemeral: true });
        // Rank secimi icin modal goster (rate-limit dostu: tek rank listesi yerine yazi girisi)
        // Veya rank listesini statik goster — 5 dk sonra otomatik kapanacak
        const rankSelect = new StringSelectMenuBuilder()
            .setCustomId(`grank_rank_select:${requesterId}:${encodeURIComponent(selectedMember)}`)
            .setPlaceholder('Rank seç...')
            .addOptions([
                { label: 'Member', value: 'Member' },
                { label: 'Co-Leader', value: 'Co-Leader' },
                { label: 'Leader', value: 'Leader' },
                { label: 'Elder', value: 'Elder' },
                { label: 'Veteran', value: 'Veteran' },
                { label: 'Recruit', value: 'Recruit' },
            ]);
        const row = new ActionRowBuilder().addComponents(rankSelect);
        const embed = new EmbedBuilder()
            .setTitle(`👤 ${selectedMember}`)
            .setDescription('Bu üyeye vermek istediğin rankı seç (5 dk içinde kapanır):')
            .setColor(0xE67E22);
        return interaction.update({ embeds: [embed], components: [row] });
    }
    if (interaction.isStringSelectMenu() && interaction.customId.startsWith('grank_rank_select:')) {
        if(!(await hasGrolePermission(interaction))) return interaction.reply({ content: '❌ Bu menüyü kullanmaya yetkin yok.', ephemeral: true });
        const parts = interaction.customId.split(':');
        const requesterId = parts[1];
        const member = decodeURIComponent(parts[2] || '');
        const rank = interaction.values[0];
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.update({ content: '❌ Bağlı RakSAMP istemcisi yok.', embeds: [], components: [] });
        }
        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'grank', target: member, rank, requesterId: interaction.id });
        setPendingInteraction(interaction.id, interaction);
        setTimeout(() => { interaction.editReply({ content: '✅ İşlem tamamlandı.', embeds: [], components: [] }).catch(()=>{}); removePendingInteraction(interaction.id); }, 2*60*1000);
        const embed = new EmbedBuilder()
            .setTitle('✅ Gönderildi')
            .setDescription(`\`/grank ${member} → ${rank}\` oyuna iletildi.`)
            .setColor(0x00FF00);
        return interaction.update({ embeds: [embed], components: [] });
    }
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

// arka plan görevleri başlasın

// Admin eylem logu — oyun içi "ADMIN: X banned Y ... (REASON: Z)" yakalaması
// DB'ye kaydedilir. Discord embed yok (kalabalık yapmasın); /alog ile sorgulanır.
app.post('/api/adminlog', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const targetName = String(body.targetName || '').trim().slice(0, 64);
    const action = String(body.action || '').trim().slice(0, 16);
    if (!targetName || !action) {
        return res.json({ success: false, message: "targetName/action zorunlu." });
    }

    await db.execute(
        `INSERT INTO admin_logs (admin_name, target_name, target_id, action, mode, duration_min, reason)
         VALUES (?, ?, ?, ?, ?, ?, ?)`,
        [
            String(body.adminName || '').trim().slice(0, 64),
            targetName,
            body.targetId ? Number(body.targetId) : null,
            action,
            String(body.mode || '').trim().slice(0, 64),
            body.durationMin ? Number(body.durationMin) : null,
            String(body.reason || '').trim().slice(0, 255)
        ]
    );
    console.log(`📝 ADMINLOG (guild=${guild.guildId}): ${body.adminName || '?'} ${action} ${targetName} (mode=${body.mode || '-'}, reason=${body.reason || '-'})`);
    return res.json({ success: true });
});

// Grup rol değişikliği logu — "(Ad)(accid) changed the rank of ..." yakalaması
app.post('/api/grouplog', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    await db.execute(
        `INSERT INTO group_logs (actor_name, actor_acc, target_name, target_acc, old_rank, new_rank)
         VALUES (?, ?, ?, ?, ?, ?)`,
        [
            String(body.actorName || '').trim().slice(0, 64),
            body.actorAcc ? Number(body.actorAcc) : null,
            String(body.targetName || '').trim().slice(0, 64),
            body.targetAcc ? Number(body.targetAcc) : null,
            String(body.oldRank || '').trim().slice(0, 64),
            String(body.newRank || '').trim().slice(0, 64)
        ]
    );
    console.log(`📝 GROUPLOG (guild=${guild.guildId}): ${body.actorName || '?'} ${body.oldRank || '?'} -> ${body.newRank || '?'} (${body.targetName || '?'})`);
    return res.json({ success: true });
});

// ?post — son rol değişikliği + gerekçe Discord'a embed olarak gider
app.post('/api/grouppost', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const reason = String(body.reason || '').trim().slice(0, 255);
    if (!reason) {
        return res.json({ success: false, message: "Gerekçe boş." });
    }

    // son log kaydına gerekçeyi de işle (varsa)
    await db.execute(
        `UPDATE group_logs SET reason = ? WHERE id = (SELECT MAX(id) FROM group_logs)`,
        [reason]
    ).catch(() => {});

    const embed = new EmbedBuilder()
        .setTitle("📋 Rol Değişikliği Gerekçesi")
        .setDescription(`**${body.actorName || 'Bir yönetici'}** rol değişikliği yaptı.`)
        .setColor(0x0099FF)
        .addFields(
            { name: 'Oyuncu', value: `${body.targetName || '?'}${body.targetAcc ? ` (acc ${body.targetAcc})` : ''}`, inline: true },
            { name: 'Değişiklik', value: `\`${body.oldRank || '-'}\` → \`${body.newRank || '-'}\``, inline: true },
            { name: 'Gerekçe', value: reason }
        )
        .setFooter({ text: `${body.playerName || 'Oyun içi'}` })
        .setTimestamp();

    const channelId = await sendToGuildChannel(guild, 'groupChatChannelId', { embed });
    console.log(`📨 GROUPPOST (guild=${guild.guildId}, kanal=${channelId || 'null'}): ${body.targetName} ${body.oldRank}->${body.newRank} (${reason})`);
    return res.json({ success: true, channelId });
});

// ?alog sorgusu — oyuncu adı / id / accid'e göre admin log sayı özeti
app.post('/api/alog', async (req, res) => {
    const body = req.body || {};
    const guild = await resolveGuildForRakSAMP(body);
    if (!guild) {
        return res.json({ success: false, message: "Bağlı Discord sunucusu bulunamadı." });
    }

    const q = String(body.q || '').trim();
    if (!q) {
        return res.json({ success: false, message: "Sorgu boş." });
    }

    // isim (büyük/küçük duyarsız parça) veya ID/accid tam eşleşme
    let rows;
    if (/^\d+$/.test(q)) {
        rows = await db.execute(
            `SELECT action, mode, COUNT(*) AS cnt
             FROM admin_logs
             WHERE target_id = ?
             GROUP BY action, mode
             ORDER BY cnt DESC, action, mode`,
            [Number(q)]
        );
    } else {
        rows = await db.execute(
            `SELECT action, mode, COUNT(*) AS cnt
             FROM admin_logs
             WHERE LOWER(target_name) LIKE LOWER(?)
             GROUP BY action, mode
             ORDER BY cnt DESC, action, mode`,
            [`%${q}%`]
        );
    }

    if (!rows || rows.length === 0) {
        return res.json({ success: true, summary: `Kayıt yok: ${q}` });
    }

    // "ban: 3 (CNR: 2, PTP: 1) | mute: 2" gibi kompakt özet
    const byName = {};
    for (const r of rows) {
        const k = r.action || 'bilinmeyen';
        if (!byName[k]) byName[k] = [];
        byName[k].push({ mode: r.mode || '-', cnt: Number(r.cnt) });
    }
    const parts = Object.entries(byName).map(([act, list]) => {
        const total = list.reduce((s, x) => s + x.cnt, 0);
        const detail = list.map(x => `${x.mode}: ${x.cnt}`).join(', ');
        return `${act} ${total} (${detail})`;
    });
    const summary = parts.join(' | ').slice(0, 200);
    console.log(`📊 ALOG (guild=${guild.guildId}): q="${q}" -> ${rows.length} satır`);
    return res.json({ success: true, summary });
});
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

// --- Kisi bazli hos geldin (B): hwid + hedef isim -> ozel mesaj ---
app.post('/api/welcome/peruser', async (req, res) => {
    const { hwid, target, message } = req.body || {};
    if (!hwid || !target) return res.status(400).json({ success: false, message: "hwid ve target gerekli" });
    const t = String(target).trim().slice(0, 64);
    const msg = String(message || '').trim().slice(0, 255);
    if (!msg) {
        await db.execute('DELETE FROM welcome_per_user WHERE hwid=? AND target_name=?', [hwid, t]);
        console.log(`WELCOME PER-USER silindi (hwid=${hwid.slice(0,8)}, target=${t})`);
        return res.json({ success: true, deleted: true });
    }
    await db.execute('INSERT INTO welcome_per_user (hwid, target_name, custom_message, updated_at) VALUES (?,?,?,?) ON DUPLICATE KEY UPDATE custom_message=VALUES(custom_message), updated_at=VALUES(updated_at)', [hwid, t, msg, new Date()]);
    console.log(`WELCOME PER-USER (hwid=${hwid.slice(0,8)}, target=${t}): ${msg}`);
    return res.json({ success: true });
});

app.get('/api/welcome/peruser', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    const target = String(req.query.target || '').trim();
    if (!hwid || !target) return res.json({ success: false, message: '' });
    const rows = await db.execute('SELECT custom_message FROM welcome_per_user WHERE hwid=? AND target_name=? LIMIT 1', [hwid, target]);
    const msg = (rows && rows[0] && rows[0].custom_message) ? rows[0].custom_message : '';
    return res.json({ success: true, message: msg });
});

app.get('/api/welcome/peruser/list', async (req, res) => {
    const hwid = String(req.query.hwid || '');
    if (!hwid) return res.json({ success: false, list: [] });
    const rows = await db.execute('SELECT target_name, custom_message FROM welcome_per_user WHERE hwid=? ORDER BY target_name', [hwid]);
    return res.json({ success: true, list: rows || [] });
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

// --- /api/grole: Discord'dan grup rol degisikligi (paginated /grole otomasyonu) ---
app.post('/api/grole', async (req, res) => {
    const { hwid, target, role, requesterId } = req.body || {};
    if (!hwid || !target || !role) return res.status(400).json({ error: 'hwid, target, role gerekli' });
    const roleMap = { mem: 'Member', co: 'Co-Leader', memb: 'Member' };
    const fullRole = roleMap[role] || role;
    queueCommand(hwid, { type: 'grole', target, role: fullRole, requesterId: requesterId || '' });
    console.log(`👤 [GROLE] Kuyruğa alındı: ${target} -> ${fullRole} (hwid=${hwid.slice(0,8)})`);
    return res.json({ success: true });
});

// --- /api/grank: Discord'dan grup rank degisikligi (paginated /grank otomasyonu) ---
app.post('/api/grank', async (req, res) => {
    const { hwid, target, rank, requesterId } = req.body || {};
    if (!hwid || !target || !rank) return res.status(400).json({ error: 'hwid, target, rank gerekli' });
    queueCommand(hwid, { type: 'grank', target, rank, requesterId: requesterId || '' });
    console.log(`🏷️ [GRANK] Kuyruğa alındı: ${target} -> ${rank} (hwid=${hwid.slice(0,8)})`);
    return res.json({ success: true });
});

// --- /grole interaktif: uye listesini oyundan al, Discord select menu goster (butonlu sayfalama, rate-limit uyumlu) ---
const groleMemberCache = new Map(); // requesterId -> { members, guildId, hwid }
const groleSelections = new Map(); // userId -> { target, guildId, hwid }
const grolePageCache = new Map(); // requesterId -> page

function buildGroleMemberMessage(requesterId, page){
    const cache = groleMemberCache.get(requesterId);
    if(!cache) return null;
    const members = cache.members;
    const totalPages = Math.max(1, Math.ceil(members.length / 25));
    const p = Math.max(0, Math.min(page, totalPages-1));
    const slice = members.slice(p*25, p*25+25);
    const options = slice.map(m => ({ label: m.slice(0,100), value: m.slice(0,100) }));
    const select = new StringSelectMenuBuilder()
        .setCustomId(`grole_member_select:${requesterId}`)
        .setPlaceholder(`Üye seç (sayfa ${p+1}/${totalPages})`)
        .addOptions(options.length ? options : [{ label: 'Üye bulunamadı', value: '__none__' }]);
    const rows = [new ActionRowBuilder().addComponents(select)];
    if(totalPages > 1){
        const prev = new ButtonBuilder().setCustomId(`grole_page_prev:${requesterId}:${p}`).setLabel('◀ Önceki').setStyle(ButtonStyle.Secondary).setDisabled(p===0);
        const next = new ButtonBuilder().setCustomId(`grole_page_next:${requesterId}:${p}`).setLabel('Sonraki ▶').setStyle(ButtonStyle.Secondary).setDisabled(p===totalPages-1);
        const info = new ButtonBuilder().setCustomId('grole_page_info').setLabel(`${p+1}/${totalPages}`).setStyle(ButtonStyle.Secondary).setDisabled(true);
        rows.push(new ActionRowBuilder().addComponents(prev, info, next));
    }
    const embed = new EmbedBuilder()
        .setTitle('👥 Üye Seç')
        .setDescription(`${members.length} üye — sayfa ${p+1}/${totalPages}. Menü 5 dk sonra kapanır.`)
        .setColor(0x3498DB);
    return { embeds: [embed], components: rows };
}

app.post('/api/grole/list', async (req, res) => {
    const { requesterId, members, hwid, guildId } = req.body || {};
    if (!requesterId || !Array.isArray(members)) return res.json({ success: false });
    const interaction = getPendingInteraction(requesterId);
    if (!interaction) {
        console.log(`⚠️ [GROLE-LIST] Pending interaction bulunamadi: ${requesterId}`);
        return res.json({ success: false });
    }
    const cleanMembers = members.map(m => String(m).replace(/\{[0-9a-fA-F]{6}\}/g,'').trim()).filter(Boolean).slice(0, 150);
    groleMemberCache.set(requesterId, { members: cleanMembers, hwid: hwid || '', guildId: guildId || '' });
    grolePageCache.set(requesterId, 0);
    console.log(`👥 [GROLE-LIST] ${cleanMembers.length} uye alindi (req=${requesterId.slice(0,6)})`);
    try {
        const msg = buildGroleMemberMessage(requesterId, 0);
        await interaction.editReply(msg);
        // 5 dk sonra otomatik kapat (rate-limit dostu: tek edit)
        setTimeout(async () => {
            try{
                const still = getPendingInteraction(requesterId);
                if(still) await still.editReply({ content: '⏰ Menü süresi doldu.', embeds: [], components: [] }).catch(()=>{});
                groleMemberCache.delete(requesterId);
                grolePageCache.delete(requesterId);
            }catch{}
        }, 5*60*1000);
    } catch(e){
        console.error('GROLE-LIST edit hatasi:', e.message);
        try{ await interaction.editReply({ content: `❌ Menü gösterilemedi: ${e.message}` }); }catch{}
    }
    return res.json({ success: true });
});

// --- /grank interaktif: rank listesi (grole gibi) ---
const grankMemberCache = new Map();
const grankPageCache = new Map();
const grankRankCache = new Map(); // requesterId -> ranks

function buildGrankMemberMessage(requesterId, page){
    const cache = grankMemberCache.get(requesterId);
    if(!cache) return null;
    const members = cache.members;
    const totalPages = Math.max(1, Math.ceil(members.length / 25));
    const p = Math.max(0, Math.min(page, totalPages-1));
    const slice = members.slice(p*25, p*25+25);
    const options = slice.map(m => ({ label: m.slice(0,100), value: m.slice(0,100) }));
    const select = new StringSelectMenuBuilder()
        .setCustomId(`grank_member_select:${requesterId}`)
        .setPlaceholder(`Üye seç (sayfa ${p+1}/${totalPages})`)
        .addOptions(options.length ? options : [{ label: 'Üye bulunamadı', value: '__none__' }]);
    const rows = [new ActionRowBuilder().addComponents(select)];
    if(totalPages > 1){
        const prev = new ButtonBuilder().setCustomId(`grank_page_prev:${requesterId}:${p}`).setLabel('◀ Önceki').setStyle(ButtonStyle.Secondary).setDisabled(p===0);
        const next = new ButtonBuilder().setCustomId(`grank_page_next:${requesterId}:${p}`).setLabel('Sonraki ▶').setStyle(ButtonStyle.Secondary).setDisabled(p===totalPages-1);
        const info = new ButtonBuilder().setCustomId('grank_page_info').setLabel(`${p+1}/${totalPages}`).setStyle(ButtonStyle.Secondary).setDisabled(true);
        rows.push(new ActionRowBuilder().addComponents(prev, info, next));
    }
    const embed = new EmbedBuilder()
        .setTitle('👥 Üye Seç (Grank)')
        .setDescription(`${members.length} üye — sayfa ${p+1}/${totalPages}.`)
        .setColor(0xE67E22);
    return { embeds: [embed], components: rows };
}

app.post('/api/grank/list', async (req, res) => {
    const { requesterId, members, hwid, guildId } = req.body || {};
    if (!requesterId || !Array.isArray(members)) return res.json({ success: false });
    const interaction = getPendingInteraction(requesterId);
    if (!interaction) return res.json({ success: false });
    const cleanMembers = members.map(m => String(m).replace(/\{[0-9a-fA-F]{6}\}/g,'').trim()).filter(Boolean).slice(0, 150);
    grankMemberCache.set(requesterId, { members: cleanMembers, hwid: hwid||'', guildId: guildId||'' });
    grankPageCache.set(requesterId, 0);
    console.log(`👥 [GRANK-LIST] ${cleanMembers.length} uye alindi (req=${requesterId.slice(0,6)})`);
    try{
        const msg = buildGrankMemberMessage(requesterId, 0);
        await interaction.editReply(msg);
        setTimeout(async () => {
            try{
                const still = getPendingInteraction(requesterId);
                if(still) await still.editReply({ content: '⏰ Menü süresi doldu.', embeds: [], components: [] }).catch(()=>{});
                grankMemberCache.delete(requesterId);
                grankPageCache.delete(requesterId);
            }catch{}
        }, 5*60*1000);
    }catch(e){ console.error('GRANK-LIST edit hatasi:', e.message); }
    return res.json({ success: true });
});

app.post('/api/grank/ranklist', async (req, res) => {
    const { requesterId, ranks } = req.body || {};
    if (!requesterId || !Array.isArray(ranks)) return res.json({ success: false });
    const interaction = getPendingInteraction(requesterId);
    // grank member seciminden sonra rank listesi geliyor — ayni interaction uzerinden rank secimi goster
    // interaction zaten member seciminde update edildi, yeni bir deferred yok — editReply ile devam
    // Bu endpoint dogrudan interaction.editReply yapamaz cunku member secimi interaction'i farkli (component interaction)
    // Bu yuzden rank listesini cache'leyip member secim handler'i rank menuyu gosterecek
    // Simdilik direkt cache'le
    grankRankCache.set(requesterId, ranks.map(r=>String(r).replace(/\{[0-9a-fA-F]{6}\}/g,'').trim()).filter(Boolean));
    console.log(`🏷️ [GRANK-RANKLIST] ${ranks.length} rank alindi (req=${requesterId.slice(0,6)})`);
    return res.json({ success: true });
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

// RakSAMP client: ogrenilen grup adi + kisa tag + KALICI sahip olunan zone
// id'leri tek satir (group_name üzerinden UPSERT). zones JSON = o grubun
// sahip oldugu TUM zone'lar (flashing filtresi yok — kalici harita).
// Client acilista GET /api/groups ile BUGUNKU kayitlari geri yükler;
// boylece dunun verisi otomatik olur, zones.txt gibi dosya YOK.
app.post('/api/groups', async (req, res) => {
    const groups = (req.body || {}).groups;
    if (!Array.isArray(groups)) return res.status(400).json({ success: false, message: "groups bekleniyor" });
    const seen = new Set();
    const rows = [];
    for (const g of groups) {
        const name = String(g.name || '').trim().slice(0, 96);
        if (!name || seen.has(name)) continue;
        seen.add(name);
        const zones = Array.isArray(g.zones)
            ? JSON.stringify(g.zones.map(Number).filter(n => Number.isInteger(n) && n >= 0 && n < 2048))
            : '[]';
        rows.push([name, String(g.tag || '').slice(0, 32), zones, new Date()]);
    }
    try {
        if (!rows.length) return res.json({ success: true, count: 0 });
        await db.query(
            `INSERT INTO gangs (group_name, tag, zones, updated_at) VALUES ?
             ON DUPLICATE KEY UPDATE tag=VALUES(tag), zones=VALUES(zones), updated_at=VALUES(updated_at)`,
            [rows]
        );
        console.log(`🏷️ GROUPS: ${rows.length} grup upsert edildi (kalici zone haritasiyla)`);
        return res.json({ success: true, count: rows.length });
    } catch (err) {
        console.error('GROUPS hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// Client acilista BUGUNKU grup+tag+zone haritasini buradan geri yükler.
// DATE(updated_at)=CURDATE() filtresi sayesinde dunun verisi otomatik elenir.
app.get('/api/groups', async (req, res) => {
    try {
        // db.query() zaten dogrudan satir dizisini dondurur (tekrar destructure ETME).
        const rows = await db.query(
            'SELECT group_name, tag, zones FROM gangs WHERE DATE(updated_at) = CURDATE()'
        );
        const out = (Array.isArray(rows) ? rows : []).map(r => ({
            name: r.group_name,
            tag: r.tag || '',
            zones: r.zones ? JSON.parse(r.zones) : []
        }));
        return res.json({ success: true, groups: out });
    } catch (err) {
        console.error('GROUPS-GET hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// RakSAMP client: her botun XML'deki ismiyle (name) eşleşen satırı upsert eder
// (her bot için DB'de tek satır). Body: { name, data, groupName, zscan }.
app.post('/api/bot/status', async (req, res) => {
    const body = req.body || {};
    const name = String(body.name || '').trim().slice(0, 64);
    if (!name) return res.json({ success: false, message: "name bekleniyor" });
    const data = (body.data !== undefined) ? JSON.stringify(body.data) : null;
    const groupName = String(body.groupName || '').slice(0, 255);
    const zscan = body.zscan ? 1 : 0;
    const gzt = (body.gzt === undefined) ? 1 : (body.gzt ? 1 : 0);
    const zgrp = (body.zgrp === undefined) ? 1 : (body.zgrp ? 1 : 0);
    try {
        await db.execute(
            `INSERT INTO bots (name, group_name, zscan_enabled, gzt_enabled, zgrp_enabled, data, updated_at) VALUES (?,?,?,?,?,?,?)
             ON DUPLICATE KEY UPDATE group_name=VALUES(group_name), zscan_enabled=VALUES(zscan_enabled), gzt_enabled=VALUES(gzt_enabled), zgrp_enabled=VALUES(zgrp_enabled), data=VALUES(data), updated_at=VALUES(updated_at)`,
            [name, groupName, zscan, gzt, zgrp, data, new Date()]
        );
        console.log(`🤖 BOT-STATUS: '${name}' DB'ye yazildi (zscan=${zscan}, gzt=${gzt}, zgrp=${zgrp})`);
        return res.json({ success: true });
    } catch (err) {
        console.error('BOT-STATUS hatasi:', err.message);
        return res.status(500).json({ success: false, message: err.message });
    }
});

// Bot'un kayitli grup adini oku (acilis: dosya yerine DB'den). Yoksa bos doner,
// client /stats ile ogrenip POST /api/bot/status uzerinden tekrar yazar.
app.get('/api/bot/status', async (req, res) => {
    const name = String(req.query.name || '').trim().slice(0, 64);
    if (!name) return res.json({ success: false, groupName: '' });
    try {
        const rows = await db.execute('SELECT name, group_name FROM bots WHERE name=? LIMIT 1', [name]);
        const r = (rows && rows[0]) || {};
        return res.json({ success: true, groupName: r.group_name || '' });
    } catch (err) {
        console.error('BOT-STATUS GET hatasi:', err.message);
        return res.status(500).json({ success: false, groupName: '', message: err.message });
    }
});

// ?zscan / ?gzt / ?zgrp on/off tercihlerini gruba gore oku: client acilisinda
// grubunun durumunu ceker.
app.get('/api/bot/toggles', async (req, res) => {
    const group = String(req.query.group || '').slice(0, 255);
    if (!group) return res.json({ success: false, zscan: 0, gzt: 1, zgrp: 1, message: "group bekleniyor" });
    try {
        const rows = await db.execute('SELECT zscan_enabled, gzt_enabled, zgrp_enabled FROM bots WHERE group_name=? ORDER BY updated_at DESC LIMIT 1', [group]);
        const r = (rows && rows[0]) || {};
        const zscan = r.zscan_enabled ? 1 : 0;
        const gzt = (r.gzt_enabled === undefined) ? 1 : (r.gzt_enabled ? 1 : 0);
        const zgrp = (r.zgrp_enabled === undefined) ? 1 : (r.zgrp_enabled ? 1 : 0);
        return res.json({ success: true, zscan, gzt, zgrp });
    } catch (err) {
        console.error('BOT-TOGGLES hatasi:', err.message);
        return res.status(500).json({ success: false, zscan: 0, gzt: 1, zgrp: 1, message: err.message });
    }
});

server.listen(config.PORT, () => {
    console.log(`🚀 VDS Backend Aktif! Port: ${config.PORT}`);
});
