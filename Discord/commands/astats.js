const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('astats')
        .setNameLocalizations({ tr: 'aistatistik', 'en-US': 'astats' })
        .setDescription('RakSAMP account stats (account ID required)')
        .setDescriptionLocalizations({ tr: 'RakSAMP hesap istatistikleri (account ID gerekli)', 'en-US': 'RakSAMP account stats (account ID required)' })
        .addStringOption(o => o.setName('account_id').setDescription("Account ID").setDescriptionLocalizations({"tr": "Hesap ID’si", "en-US": "Account ID"}).setRequired(true)),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: T("❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanin.", "❌ No RakSAMP client is linked to this server. Use `/link <hwid>` first."),
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        if (!isOnline(hwid)) return replyOffline(interaction, guild);
        const accountId = interaction.options.getString('account_id');

        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = `/astats ${accountId}`;
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [ASTATS] Komut kuyruga alindi (account_id="${accountId}", requester=${interaction.id})`);
    }
};
