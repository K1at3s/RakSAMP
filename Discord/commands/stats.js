const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('stats')
        .setNameLocalizations({ tr: 'istatistik', 'en-US': 'stats' })
        .setDescription('RakSAMP stats: blank for own, or a player ID')
        .setDescriptionLocalizations({ tr: 'RakSAMP istatistikleri: boş bırakılırsa kendi, ID verilirse o oyuncu', 'en-US': 'RakSAMP stats: blank for own, or a player ID' })
        .addIntegerOption(o => o.setName('id').setDescription("Player/account ID (leave blank for your own stats)").setDescriptionLocalizations({"tr": "Oyuncu/hesap ID’si (boş bırakırsan kendi istatistiklerin)", "en-US": "Player/account ID (leave blank for your own stats)"}).setRequired(false)),
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
        const id = interaction.options.getInteger('id');

        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = id ? `/stats ${id}` : '/stats';
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [STATS] Komut kuyruga alindi (id="${id ?? 'kendi'}", requester=${interaction.id})`);
    }
};
