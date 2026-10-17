const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('admins')
        .setNameLocalizations({ tr: 'adminler', 'en-US': 'admins' })
        .setDescription('Requests the online admin list from the linked RakSAMP client')
        .setDescriptionLocalizations({ tr: 'Bağlı RakSAMP istemcisinden online admin listesini ister', 'en-US': 'Requests the online admin list from the linked RakSAMP client' }),
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

        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = '/admins';
        queueCommand(hwid, { type: 'admins', cmd, requesterId: interaction.id });

        console.log(`🛡️ [ADMINS] Komut kuyruga alindi (requester=${interaction.id})`);
    }
};
