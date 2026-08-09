const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models/Guild');
const { queueCommand } = require('../utils/commandOutbox');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('astats')
        .setDescription('RakSAMP hesap istatistikleri (account ID gerekli)')
        .addIntegerOption(o => o.setName('account_id').setDescription('Hesap (account) ID si').setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henüz bir RakSAMP istemcisine bağlı değil. Önce `/link <hwid>` kullanın.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        const accountId = interaction.options.getInteger('account_id');

        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = `/astats ${accountId}`;
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [ASTATS] Komut kuyruğa alındı (account_id="${accountId}", requester=${interaction.id})`);
    }
};
