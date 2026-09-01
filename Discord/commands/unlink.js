const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('unlink')
        .setDescription('Bir HWID baglantisini kaldirir')
        .addStringOption(o => o.setName('hwid')
            .setDescription('Kaldirilacak HWID kodu')
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
            `✅ **\`${hwid}\`** baglantisi kaldirildi.\n` +
            `Bagli HWID\'ler: ${guild.linkedHwids.map(h => `\`${h}\``).join(', ') || 'yok'}`
        );
    }
};
