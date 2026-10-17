const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;
const { unlinkDevice } = require('../utils/unlinkDevice');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('unlink')
        .setNameLocalizations({ tr: 'cikar', 'en-US': 'unlink' })
        .setDescription('Removes an HWID link')
        .setDescriptionLocalizations({ tr: 'Bir HWID bağlantısını kaldırır', 'en-US': 'Removes an HWID link' })
        .addStringOption(o => o.setName('hwid')
            .setDescription("HWID to unlink").setDescriptionLocalizations({"tr": "Kaldırılacak HWID kodu", "en-US": "HWID to unlink"})
            .setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const linked = guild?.linkedHwids || [];
        const hwid = interaction.options.getString('hwid')?.trim().toUpperCase() || (linked.length === 1 ? linked[0] : '');
        if (!hwid) return interaction.reply({ content: linked.length ? T("Birden fazla bot bağlı; kaldırılacak HWID bilgisini belirt.", "Multiple bots are linked; specify the HWID to unlink.") : T("Bu sunucuya bağlı bot yok.", "No bot is linked to this server."), ephemeral: true });
        if (!linked.includes(hwid)) return interaction.reply({ content: T("Bu HWID bu sunucuya bağlı değil.", "This HWID is not linked to this server."), ephemeral: true });
        await interaction.deferReply({ ephemeral: true });
        await unlinkDevice(hwid, interaction.guildId);
        const remaining = linked.filter(id => id !== hwid);
        await interaction.editReply(
            T(`✅ **\`${hwid}\`** bağlantısı kaldırıldı.\n`, `✅ **\`${hwid}\`** unlinked.\n`) +
            T("Bağlı HWID’ler: ", "Linked HWIDs: ") + (remaining.map(h => `\`${h}\``).join(', ') || T('yok', 'none'))
        );
    }
};
