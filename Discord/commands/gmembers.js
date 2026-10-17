const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('gmembers')
        .setNameLocalizations({ tr: 'gmembers', 'en-US': 'gmembers' })
        .setDescription('Collects member list + account IDs + VIP and writes to the Google Sheets roster (harvest)')
        .setDescriptionLocalizations({ tr: 'Üye listesi + account ID + VIP toplayıp Google Sheets roster tablosuna yazar (harvest)', 'en-US': 'Collects member list + account IDs + VIP and writes to the Google Sheets roster (harvest)' })
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: T("❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanın.", "❌ No RakSAMP client is linked to this server. Use `/link <hwid>` first."),
                ephemeral: true
            });
        }

        const L = (tr, en) => (guild.lang === 'tr' ? tr : en);
        const hwid = guild.linkedHwids[0];
        if (!isOnline(hwid)) return replyOffline(interaction, guild);
        queueCommand(hwid, { type: 'gmembers', requesterId: interaction.id });
        setPendingInteraction(interaction.id, interaction);

        await interaction.reply({
            content: `✅ ${L('GMembers harvest başlatıldı. Bitince tablo linki bu kanala gelecek.', 'GMembers harvest started. The sheet link will be posted in this channel when done.')}`,
            ephemeral: true
        });
    }
};