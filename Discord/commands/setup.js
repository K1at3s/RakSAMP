const { SlashCommandBuilder, PermissionFlagsBits, ChannelType } = require('discord.js');
const Guild = require('../models/Guild');

// /setup: botun tum kanallarini bu sunucuda otomatik olusturur ve guild kaydina yazar.
// Boylece kullanici her kanali tek tek /setchannel ile ayarlamak zorunda kalmaz.
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
        .setDescription('Botun kanallarini otomatik olusturur ve bu sunucuya baglar')
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

        let msg = `✅ **${created.length}** kanal olusturuldu ve bu sunucuya baglandi.`;
        if (skipped.length) msg += `\n⚠️ Olusturulamayan: ${skipped.join(', ')} (elle /setchannel ile ayarla)`;
        if (!created.length) msg = '❌ Hic kanal olusturulamadi. Botun bu sunucuda kanal acma yetkisi olmali.';

        await interaction.editReply(msg);
    }
};
