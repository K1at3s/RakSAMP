const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('unlink')
        .setDescription('Bir HWID bağlantısını kaldırır')
        .addStringOption(o => o.setName('hwid')
            .setDescription('Kaldırılacak HWID kodu')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const hwid = interaction.options.getString('hwid').trim().toUpperCase();

        const guild = await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $pull: { linkedHwids: hwid } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(
            `✅ **\`${hwid}\`** bağlantısı kaldırıldı.\n` +
            `Bağlı HWID\'ler: ${guild.linkedHwids.map(h => `\`${h}\``).join(', ') || 'yok'}`
        );
    }
};
