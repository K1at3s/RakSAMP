const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models/Guild');
const { queueCommand } = require('../utils/commandOutbox');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('gstats')
        .setDescription('Bağlı RakSAMP istemcisinden grup istatistikleri ister')
        .addStringOption(o => o.setName('group_id').setDescription('Belirli bir grubun ID si (bos birakilirsa kendi grubun)').setRequired(false)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: '❌ Bu sunucu henüz bir RakSAMP istemcisine bağlı değil. Önce `/link <hwid>` kullanın.',
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        const groupId = interaction.options.getString('group_id') || '';

        // İlk mesaj yerine "düşünüyor..." göster; oyundan yanıt gelince editReply ile sonuç atılır.
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);

        const cmd = groupId ? `/gstats ${groupId}` : '/gstats';
        queueCommand(hwid, { type: 'gstats', cmd, requesterId: interaction.id });

        console.log(`📊 [GSTATS] Komut kuyruğa alındı (group_id="${groupId}", requester=${interaction.id})`);
    }
};
