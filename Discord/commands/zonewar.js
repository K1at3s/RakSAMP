const { SlashCommandBuilder, PermissionFlagsBits, EmbedBuilder } = require('discord.js');
const Guild = require('../models/Guild');
const { sendToGuildChannel } = require('../utils/discordNotifier');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('zonewar')
        .setDescription('ZONE WAR bildirimini manuel olarak zonewar kanalına gönderir')
        .addStringOption(o => o.setName('grup').setDescription('Saldıran grup').setRequired(true))
        .addStringOption(o => o.setName('durum')
            .setDescription('Savaş durumu')
            .setRequired(true)
            .addChoices(
                { name: 'Zone tuttu', value: 'keeps' },
                { name: 'Zone aldı', value: 'takes' },
                { name: 'Kazandı', value: 'wins' },
                { name: 'Kaybetti', value: 'loses' }
            ))
        .addStringOption(o => o.setName('rakip').setDescription('Rakip grup (wins/loses için)').setRequired(false))
        .addStringOption(o => o.setName('bölge').setDescription('Bölge adı').setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) {
            return interaction.reply({ content: '❌ Sunucu kaydı bulunamadı.', ephemeral: true });
        }

        const ownGroup = interaction.options.getString('grup');
        const action = interaction.options.getString('durum');
        const enemyGroup = interaction.options.getString('rakip') || '';
        const zoneName = interaction.options.getString('bölge') || '';

        const actionText = {
            keeps: 'keeps zone',
            takes: 'takes over the zone',
            wins: 'wins against',
            loses: 'loses to'
        }[action] || action;

        let description;
        if (action === 'wins' || action === 'loses') {
            description = `**${ownGroup}** ${actionText} **${enemyGroup || '?'}**`;
        } else {
            description = `**${ownGroup}** ${actionText}${zoneName ? ` **${zoneName}**` : ''}`;
        }

        const embed = new EmbedBuilder()
            .setTitle("⚔️ ZONE WAR")
            .setDescription(description)
            .setColor(action === 'loses' ? 0xFF0000 : 0xFFA500)
            .setTimestamp();

        const channelId = await sendToGuildChannel(guild, 'zoneWarChannelId', { embed });
        await interaction.reply({
            content: channelId
                ? `✅ ZONE WAR bildirimi <#${channelId}> kanalına gönderildi.`
                : '⚠️ zonewar kanalı ayarlanmamış. `/setchannel zone-war <kanal>` ile ayarlayın.',
            ephemeral: true
        });
    }
};
