const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;
const { sendToGuildChannel } = require('../utils/discordNotifier');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('backup')
        .setNameLocalizations({ tr: 'destek', 'en-US': 'backup' })
        .setDescription('Sends a backup call (@here urgent help to the backup channel)')
        .setDescriptionLocalizations({ tr: 'Backup çağrısı gönderir (backup kanalına @here acil yardım)', 'en-US': 'Sends a backup call (@here urgent help to the backup channel)' })
        .addStringOption(o => o.setName('grup').setDescription("Group name").setDescriptionLocalizations({"tr": "Grup adı", "en-US": "Group name"}).setRequired(true))
        .addStringOption(o => o.setName('mesaj').setDescription("Additional message").setDescriptionLocalizations({"tr": "Ek mesaj", "en-US": "Additional message"}).setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) {
            return interaction.reply({ content: T("❌ Sunucu kaydi bulunamadi.", "❌ No record for this server."), ephemeral: true });
        }

        const group = interaction.options.getString('grup');
        const extra = interaction.options.getString('mesaj') || '';

        const content = T(`@here **${group} destek istiyor!**`, `@here **${group} needs backup!**`) + (extra ? `\n${extra}` : '');

        const channelId = await sendToGuildChannel(guild, 'backupChannelId', { content });
        await interaction.reply({
            content: channelId
                ? T(`✅ Destek çağrısı <#${channelId}> kanalına gönderildi.`, `✅ Backup call sent to <#${channelId}>.`)
                : T("⚠️ backup kanali ayarlanmamis. `/setchannel backup <kanal>` ile ayarlayin.", "⚠️ No backup channel set. Use `/setchannel backup <channel>`."),
            ephemeral: true
        });
    }
};
