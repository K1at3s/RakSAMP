const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('link')
        .setDescription('Bu sunucuyu RakSAMP HWID\'nize bağlar')
        .addStringOption(o => o.setName('hwid')
            .setDescription('RakSAMP\'ın gösterdiği HWID kodu (örn. BURHAN-XXXX)')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const hwid = interaction.options.getString('hwid').trim().toUpperCase();
        if (!hwid) {
            return interaction.reply({ content: '❌ HWID boş olamaz.', ephemeral: true });
        }

        const guild = await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $addToSet: { linkedHwids: hwid } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(
            `✅ Bu sunucu **\`${hwid}\`** HWID\'li RakSAMP istemcisine bağlandı.\n` +
            `Bağlı HWID\'ler: ${guild.linkedHwids.map(h => `\`${h}\``).join(', ') || 'yok'}`
        );
    }
};
