const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setchannel')
        .setNameLocalizations({ tr: 'kanalayarla', 'en-US': 'setchannel' })
        .setDescription('Sets a channel (stays null if not set)')
        .setDescriptionLocalizations({ tr: 'Kanal ayarlar (ayarlanmazsa null olarak kalır)', 'en-US': 'Sets a channel (stays null if not set)' })
        .addStringOption(o => o.setName('type')
            .setDescription("Channel type").setDescriptionLocalizations({"tr": "Kanal türü", "en-US": "Channel type"})
            .setRequired(true)
            .addChoices(
                { name: "⚔️ War Messages", name_localizations: {"tr": "⚔️ Savaş Mesajları", "en-US": "⚔️ War Messages"}, value: 'war' },
                { name: "📊 Group Stats", name_localizations: {"tr": "📊 Grup İstatistikleri", "en-US": "📊 Group Stats"}, value: 'gstats' },
                { name: "💬 Group Chat Bridge", name_localizations: {"tr": "💬 Grup Sohbet Köprüsü", "en-US": "💬 Group Chat Bridge"}, value: 'group-chat' },
                { name: "⚔️ Zone War Notifications", name_localizations: {"tr": "⚔️ Bölge Savaşı Bildirimleri", "en-US": "⚔️ Zone War Notifications"}, value: 'zone-war' },
                { name: "🆘 Backup Calls (?1414)", name_localizations: {"tr": "🆘 Destek Çağrıları (?1414)", "en-US": "🆘 Backup Calls (?1414)"}, value: 'backup' },
                { name: "📋 Group Logs (?post)", name_localizations: {"tr": "📋 Grup Kayıtları (?post)", "en-US": "📋 Group Logs (?post)"}, value: 'group-log' },
                { name: "🤖 Bot Commands Channel", name_localizations: {"tr": "🤖 Bot Komut Kanalı", "en-US": "🤖 Bot Commands Channel"}, value: 'bot-commands' }
            ))
        .addChannelOption(o => o.setName('kanal')
            .setDescription("Target channel").setDescriptionLocalizations({"tr": "Hedef kanal", "en-US": "Target channel"})
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const type = interaction.options.getString('type');
        const channel = interaction.options.getChannel('kanal');

        const fieldMap = {
            war: 'warChannelId',
            gstats: 'gstatsChannelId',
            'group-chat': 'groupChatChannelId',
            'zone-war': 'zoneWarChannelId',
            backup: 'backupChannelId',
            'group-log': 'groupLogChannelId',
            'bot-commands': 'botCommandsChannelId'
        };
        const field = fieldMap[type];
        if (!field) return interaction.reply({ content: T("Bu kanal türü desteklenmiyor. HWID bildirimleri yalnızca merkez sunucudaki sabit kanala gönderilir.", "This channel type is unsupported. HWID notifications are sent only to the fixed channel in the central server."), ephemeral: true });

        await Guild.findOneAndUpdate(
            { guildId: interaction.guildId },
            { $set: { [field]: channel.id } },
            { upsert: true, new: true, setDefaultsOnInsert: true }
        );

        await interaction.reply(T(`✅ **${type}** kanalı <#${channel.id}> olarak ayarlandı.`, `✅ **${type}** channel set to <#${channel.id}>.`));
    }
};
