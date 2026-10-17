const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('gstats')
        .setNameLocalizations({ tr: 'gistatistik', 'en-US': 'gstats' })
        .setDescription('Requests group stats from the linked RakSAMP client')
        .setDescriptionLocalizations({ tr: 'Bağlı RakSAMP istemcisinden grup istatistikleri ister', 'en-US': 'Requests group stats from the linked RakSAMP client' })
        .addStringOption(o => o.setName('group_id').setDescription("Group ID (leave blank for your own group)").setDescriptionLocalizations({"tr": "Grup ID’si (boş bırakırsan kendi grubun)", "en-US": "Group ID (leave blank for your own group)"}).setRequired(false)),
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
        const groupId = interaction.options.getString('group_id') || '';

        // Ilk mesaj yerine "dusunuyor..." goster; oyundan yanit gelince editReply ile sonuc atilir.
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = groupId ? `/gstats ${groupId}` : '/gstats';
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [GSTATS] Komut kuyruga alindi (group_id="${groupId}", requester=${interaction.id})`);
    }
};
