const { SlashCommandBuilder, PermissionFlagsBits, ChannelType } = require('discord.js');
const Guild = require('../models').Guild;

// /kurulum (setup): BARE = kanallari olusturur. Kapatmak icin islem:kapat (EN: off).
//   TR: /kurulum            -> olustur
//   TR: /kurulum işlem:kapat -> sil
//   EN: /setup              -> create
//   EN: /setup action:off   -> close
// Kanallar otomatik OLUSTURULMAZ; yalnizca bu komutla kurulur.
const CATEGORY = 'RakSAMP';
const CHANNELS = [
    { key: 'logChannelId', tr: '📜-lisans-log', en: '📜-license-log' },
    { key: 'warChannelId', tr: '⚔️-savas', en: '⚔️-war' },
    { key: 'gstatsChannelId', tr: '📊-gstats', en: '📊-gstats' },
    { key: 'groupChatChannelId', tr: '💬-grup-chat', en: '💬-group-chat' },
    { key: 'zoneWarChannelId', tr: '⚔️-bolge-savas', en: '⚔️-zone-war' },
    { key: 'backupChannelId', tr: '🆘-yardim', en: '🆘-backup' },
    { key: 'groupLogChannelId', tr: '📋-grup-log', en: '📋-group-log' },
    { key: 'botCommandsChannelId', tr: '🤖-bot-komut', en: '🤖-bot-commands' }
];
const CHANNEL_FIELDS = CHANNELS.map(c => c.key);

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setup')
        .setNameLocalizations({ tr: 'kurulum', 'en-US': 'setup' })
        .setDescription('Botun kanallarini olusturur veya kapatir')
        .setDescriptionLocalizations({ tr: 'Botun kanallarını oluşturur veya kapatır', 'en-US': 'Creates or closes the bot channels' })
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
        .addStringOption(o => o
            .setName('islem')
            .setNameLocalizations({ tr: 'islem', 'en-US': 'action' })
            .setDescription('Kapatma islemi (bos = kanallari olustur)')
            .setDescriptionLocalizations({ tr: 'Kapatma işlemi (boş = kanalları oluştur)', 'en-US': 'Close action (empty = create channels)' })
            .setRequired(false)
            .addChoices(
                { name: 'Kapat (kanallari sil)', value: 'kapat' },
                { name: 'Close (delete channels)', value: 'off' }
            )),
    async execute(interaction) {
        const action = interaction.options.getString('islem') || '';
        if (action === 'kapat' || action === 'off') return close(interaction);
        return create(interaction);
    }
};

async function create(interaction) {
    await interaction.deferReply();
    const guild = interaction.guild;
    const gRec = await Guild.findOne({ guildId: guild.id });
    const isTr = gRec ? gRec.lang === 'tr' : true;

    const updates = {};
    const created = [];
    const skipped = [];

    // Once kategori: kanallar onun altinda acilir. Everyone'a kapali
    // (kullanici sonradan kendisi ayarlar); izinli rol varsa o rol gorur.
    let parentId = null;
    const roleId0 = gRec ? gRec.allowedRoleId : null;
    const baseOverwrites = [{ id: guild.roles.everyone.id, deny: [PermissionFlagsBits.ViewChannel] }];
    if (roleId0) baseOverwrites.push({ id: roleId0, allow: [PermissionFlagsBits.ViewChannel, PermissionFlagsBits.SendMessages, PermissionFlagsBits.ReadMessageHistory] });
    try {
        const cat = await guild.channels.create({ name: CATEGORY, type: ChannelType.GuildCategory, permissionOverwrites: baseOverwrites });
        parentId = cat.id;
        updates.categoryId = cat.id;
    } catch (e) { /* kategorisiz devam */ }

    for (const c of CHANNELS) {
        const cname = isTr ? c.tr : c.en;
        try {
            const ch = await guild.channels.create({ name: cname, type: ChannelType.GuildText, ...(parentId ? { parent: parentId } : {}) });
            try { if (parentId) await ch.lockPermissions(); } catch (e) {}
            updates[c.key] = ch.id;
            created.push(cname);
        } catch (e) {
            skipped.push(cname);
        }
    }

    if (Object.keys(updates).length) {
        await Guild.findOneAndUpdate(
            { guildId: guild.id },
            { $set: updates },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );
    }

    let msg;
    if (isTr) {
        msg = `✅ **${created.length}** kanal olusturuldu ve bu sunucuya baglandi.`;
        if (skipped.length) msg += `\n⚠️ Olusturulamayan: ${skipped.join(', ')} (elle /kanalayarla ile ayarla)`;
        if (!created.length) msg = '❌ Hic kanal olusturulamadi. Botun bu sunucuda kanal acma yetkisi olmali.';
    } else {
        msg = `✅ **${created.length}** channels created and linked to this server.`;
        if (skipped.length) msg += `\n⚠️ Could not create: ${skipped.join(', ')} (set manually with /setchannel)`;
        if (!created.length) msg = '❌ No channels could be created. The bot needs channel permission in this server.';
    }
    await interaction.editReply(msg);
}

async function close(interaction) {
    await interaction.deferReply();
    const guild = interaction.guild;
    const gRec = await Guild.findOne({ guildId: guild.id });
    const isTr = gRec ? gRec.lang === 'tr' : true;

    if (!gRec) {
        return interaction.editReply(isTr ? '❌ Bu sunucu icin kayit yok.' : '❌ No record for this server.');
    }

    const cleared = [];
    const failed = [];
    const updates = {};

    for (const f of CHANNEL_FIELDS) {
        const id = gRec[f];
        if (!id) continue;
        try {
            const ch = await guild.channels.fetch(id);
            if (ch) await ch.delete('RakSAMP /kurulum kapat');
            cleared.push(f);
        } catch (e) {
            failed.push(f); // kanal zaten silinmis olabilir — DB yine de temizlenir
        }
        updates[f] = null;
    }

    // Kategori DB'deki ID ile silinir.
    if (gRec.categoryId) {
        try {
            const cat = await guild.channels.fetch(gRec.categoryId);
            if (cat) await cat.delete('RakSAMP /kurulum kapat');
            cleared.push('kategori');
        } catch (e) {
            failed.push('kategori');
        }
        updates.categoryId = null;
    }

    if (Object.keys(updates).length) {
        await Guild.findOneAndUpdate({ guildId: guild.id }, { $set: updates });
    }

    let msg;
    if (isTr) {
        msg = `✅ **${cleared.length}** kanal silindi ve DB den temizlendi.`;
        if (failed.length) msg += `\n⚠️ Discord da bulunamayan (DB temizlendi): ${failed.join(', ')}`;
        if (!Object.keys(updates).length) msg = 'ℹ️ DB de kayitli kanal yok, silinecek bir sey yok.';
    } else {
        msg = `✅ **${cleared.length}** channels deleted and cleared from the DB.`;
        if (failed.length) msg += `\n⚠️ Not found on Discord (DB cleared): ${failed.join(', ')}`;
        if (!Object.keys(updates).length) msg = 'ℹ️ No channels in the DB, nothing to delete.';
    }
    await interaction.editReply(msg);
}