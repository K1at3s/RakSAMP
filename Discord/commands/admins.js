const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models/Guild');
const { queueCommand } = require('../utils/commandOutbox');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('admins')
        .setDescription('Bağlı RakSAMP istemcisinden online admin listesini ister'),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henüz bir RakSAMP istemcisine bağlı değil. Önce `/link <hwid>` kullanın.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];

        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = '/admins';
        queueCommand(hwid, { type: 'admins', cmd, requesterId: interaction.id });

        console.log(`🛡️ [ADMINS] Komut kuyruğa alındı (requester=${interaction.id})`);
    }
};
