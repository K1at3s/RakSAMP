const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setchannel')
        .setDescription('Kanal ayarlar (ayarlanmazsa null olarak kalır)')
        .addStringOption(o => o.setName('type')
            .setDescription('Kanal türü')
            .setRequired(true)
            .addChoices(
                { name: '🛡️ Lisans Bildirimleri', value: 'log' },
                { name: '⚔️ War Mesajları', value: 'war' },
                { name: '📊 GStats Verileri', value: 'gstats' },
                { name: '💬 Grup Chat Köprüsü', value: 'group-chat' },
                { name: '⚔️ ZONE WAR Bildirimleri', value: 'zone-war' },
                { name: '🆘 Backup Çağrıları (?1414)', value: 'backup' },
                { name: '👋 Welcome Back Mesajları', value: 'welcome' }
            ))
        .addChannelOption(o => o.setName('kanal')
            .setDescription('Hedef kanal')
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const type = interaction.options.getString('type');
        const channel = interaction.options.getChannel('kanal');

        const fieldMap = {
            log: 'logChannelId',
            war: 'warChannelId',
            gstats: 'gstatsChannelId',
            'group-chat': 'groupChatChannelId',
            'zone-war': 'zoneWarChannelId',
            backup: 'backupChannelId',
            welcome: 'welcomeChannelId'
        };
        const field = fieldMap[type] || `${type}ChannelId`;

        await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $set: { [field]: channel.id } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(`✅ **${type}** kanalı <#${channel.id}> olarak ayarlandı.`);
    }
};
