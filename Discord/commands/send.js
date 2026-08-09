const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');
const { queueCommand } = require('../utils/commandOutbox');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('send')
        .setDescription('Oyun içindeki RakSAMP istemcisine komut/sohbet mesajı iletir')
        .addStringOption(o => o.setName('komut')
            .setDescription('İletilecek komut (örn. ?au Merhaba, /gstats, selam)')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const cmd = interaction.options.getString('komut').trim();
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henüz bir RakSAMP istemcisine bağlı değil. Önce `/link <hwid>` kullanın.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        queueCommand(hwid, { type: 'command', cmd, author: interaction.user.username });

        await interaction.reply(`✅ Komut kuyruğa alındı. **\`${hwid}\`** istemcisine iletiliyor: \`${cmd.slice(0, 150)}\``);
    }
};
