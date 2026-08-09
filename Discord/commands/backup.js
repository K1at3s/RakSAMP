const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');
const { sendToGuildChannel } = require('../utils/discordNotifier');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('backup')
        .setDescription('Backup çağrısı gönderir (backup kanalına @here acil yardım)')
        .addStringOption(o => o.setName('grup').setDescription('Grup adı').setRequired(true))
        .addStringOption(o => o.setName('mesaj').setDescription('Ek mesaj').setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) {
            return interaction.reply({ content: '❌ Sunucu kaydı bulunamadı.', ephemeral: true });
        }

        const group = interaction.options.getString('grup');
        const extra = interaction.options.getString('mesaj') || '';

        const content = `@here **${group} backup istiyor!**${extra ? `\n${extra}` : ''}`;

        const channelId = await sendToGuildChannel(guild, 'backupChannelId', { content });
        await interaction.reply({
            content: channelId
                ? `✅ Backup çağrısı <#${channelId}> kanalına gönderildi.`
                : '⚠️ backup kanalı ayarlanmamış. `/setchannel backup <kanal>` ile ayarlayın.',
            ephemeral: true
        });
    }
};
