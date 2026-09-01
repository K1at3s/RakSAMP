const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models/Guild');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setchannel')
        .setDescription('Kanal ayarlar (ayarlanmazsa null olarak kalir)')
        .addStringOption(o => o.setName('type')
            .setDescription('Kanal turu')
            .setRequired(true)
            .addChoices(
                { name: '🛡️ Lisans Bildirimleri', value: 'log' },
                { name: '⚔️ War Mesajlari', value: 'war' },
                { name: '📊 GStats Verileri', value: 'gstats' },
                { name: '💬 Grup Chat Koprusu', value: 'group-chat' },
                { name: '⚔️ ZONE WAR Bildirimleri', value: 'zone-war' },
                { name: '🆘 Backup Cagrilari (?1414)', value: 'backup' },
                { name: '👋 Welcome Back Mesajlari', value: 'welcome' }
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

        await interaction.reply(`✅ **${type}** kanali <#${channel.id}> olarak ayarlandi.`);
    }
};
