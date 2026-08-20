const { SlashCommandBuilder, PermissionFlagsBits, ChannelType } = require('discord.js');
const Guild = require('../models/Guild');

// /setup: botun tüm kanallarını bu sunucuda otomatik oluşturur ve guild kaydına yazar.
// Böylece kullanıcı her kanalı tek tek /setchannel ile ayarlamak zorunda kalmaz.
const CHANNELS = [
    { key: 'logChannelId', name: '📜-lisans-log' },
    { key: 'warChannelId', name: '⚔️-war' },
    { key: 'gstatsChannelId', name: '📊-gstats' },
    { key: 'groupChatChannelId', name: '💬-grup-chat' },
    { key: 'zoneWarChannelId', name: '⚔️-zone-war' },
    { key: 'backupChannelId', name: '🆘-backup' },
    { key: 'welcomeChannelId', name: '👋-welcome' }
];

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setup')
        .setDescription('Botun kanallarını otomatik oluşturur ve bu sunucuya bağlar')
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        await interaction.deferReply();
        const guild = interaction.guild;

        const updates = {};
        const created = [];
        const skipped = [];

        for (const c of CHANNELS) {
            try {
                const ch = await guild.channels.create({ name: c.name, type: ChannelType.GuildText });
                updates[c.key] = ch.id;
                created.push(c.name);
            } catch (e) {
                skipped.push(c.name);
            }
        }

        if (Object.keys(updates).length) {
            await Guild.findOneAndUpdate(
                { guildId: guild.id },
                { $set: updates },
                { upsert: true, new: true, setDefaultsOnInsert: true }
            );
        }

        let msg = `✅ **${created.length}** kanal oluşturuldu ve bu sunucuya bağlandı.`;
        if (skipped.length) msg += `\n⚠️ Oluşturulamayan: ${skipped.join(', ')} (elle /setchannel ile ayarla)`;
        if (!created.length) msg = '❌ Hiç kanal oluşturulamadı. Botun bu sunucuda kanal açma yetkisi olmalı.';

        await interaction.editReply(msg);
    }
};
