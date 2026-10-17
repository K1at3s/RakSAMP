const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;
const { unlinkDevice } = require('../utils/unlinkDevice');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('unlink')
        .setNameLocalizations({ tr: 'cikar', 'en-US': 'unlink' })
        .setDescription('Bir HWID baglantisini kaldirir')
        .setDescriptionLocalizations({ tr: 'Bir HWID bağlantısını kaldırır', 'en-US': 'Removes an HWID link' })
        .addStringOption(o => o.setName('hwid')
            .setDescription('Kaldirilacak HWID kodu')
            .setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        const linked = guild?.linkedHwids || [];
        const hwid = interaction.options.getString('hwid')?.trim().toUpperCase() || (linked.length === 1 ? linked[0] : '');
        if (!hwid) return interaction.reply({ content: linked.length ? 'Birden fazla bot bağlı; kaldırılacak HWID bilgisini belirt.' : 'Bu sunucuya bağlı bot yok.', ephemeral: true });
        if (!linked.includes(hwid)) return interaction.reply({ content: 'Bu HWID bu sunucuya bağlı değil.', ephemeral: true });
        await interaction.deferReply({ ephemeral: true });
        await unlinkDevice(hwid, interaction.guildId);
        const remaining = linked.filter(id => id !== hwid);
        await interaction.editReply(
            `✅ **\`${hwid}\`** baglantisi kaldirildi.\n` +
            `Bagli HWID\'ler: ${remaining.map(h => `\`${h}\``).join(', ') || 'yok'}`
        );
    }
};
