const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;

module.exports = {
    data: new SlashCommandBuilder()
        .setName('link')
        .setNameLocalizations({ tr: 'bagla', 'en-US': 'link' })
        .setDescription("Links this server to your RakSAMP HWID")
        .setDescriptionLocalizations({ tr: "Bu sunucuyu RakSAMP HWID'nize bağlar", 'en-US': "Links this server to your RakSAMP HWID" })
        .addStringOption(o => o.setName('hwid')
            .setDescription("HWID shown by RakSAMP").setDescriptionLocalizations({"tr": "RakSAMP’in gösterdiği HWID kodu", "en-US": "HWID shown by RakSAMP"})
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const hwid = interaction.options.getString('hwid').trim().toUpperCase();
        if (!hwid) {
            return interaction.reply({ content: T("❌ HWID bos olamaz.", "❌ HWID cannot be empty."), ephemeral: true });
        }

        const guild = await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $addToSet: { linkedHwids: hwid } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(
            T(`✅ Bu sunucu **\`${hwid}\`** RakSAMP istemcisine bağlandı.\n`, `✅ This server is now linked to RakSAMP client **\`${hwid}\`**.\n`) +
            T("Bağlı HWID’ler: ", "Linked HWIDs: ") + (guild.linkedHwids.map(h => `\`${h}\``).join(', ') || T('yok', 'none'))
        );
    }
};
