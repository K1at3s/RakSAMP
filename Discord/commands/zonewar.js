const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionFlagsBits, EmbedBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { sendToGuildChannel } = require('../utils/discordNotifier');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('zonewar')
        .setNameLocalizations({ tr: 'bolgesavasi', 'en-US': 'zonewar' })
        .setDescription('Manually sends a ZONE WAR notification to the zonewar channel')
        .setDescriptionLocalizations({ tr: 'ZONE WAR bildirimini manuel olarak zonewar kanalına gönderir', 'en-US': 'Manually sends a ZONE WAR notification to the zonewar channel' })
        .addStringOption(o => o.setName('grup').setDescription("Attacking group").setDescriptionLocalizations({"tr": "Saldıran grup", "en-US": "Attacking group"}).setRequired(true))
        .addStringOption(o => o.setName('durum')
            .setDescription("War outcome").setDescriptionLocalizations({"tr": "Savaş durumu", "en-US": "War outcome"})
            .setRequired(true)
            .addChoices(
                { name: "Kept zone", name_localizations: {"tr": "Bölgeyi korudu", "en-US": "Kept zone"}, value: 'keeps' },
                { name: "Captured zone", name_localizations: {"tr": "Bölgeyi aldı", "en-US": "Captured zone"}, value: 'takes' },
                { name: "Won", name_localizations: {"tr": "Kazandı", "en-US": "Won"}, value: 'wins' },
                { name: "Lost", name_localizations: {"tr": "Kaybetti", "en-US": "Lost"}, value: 'loses' }
            ))
        .addStringOption(o => o.setName('rakip').setDescription("Enemy group (for wins/loses)").setDescriptionLocalizations({"tr": "Rakip grup (kazanma/kaybetme için)", "en-US": "Enemy group (for wins/loses)"}).setRequired(false))
        .addStringOption(o => o.setName('bolge').setDescription("Zone name").setDescriptionLocalizations({"tr": "Bölge adı", "en-US": "Zone name"}).setRequired(false))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) {
            return interaction.reply({ content: T("❌ Sunucu kaydi bulunamadi.", "❌ No record for this server."), ephemeral: true });
        }

        const ownGroup = interaction.options.getString('grup');
        const action = interaction.options.getString('durum');
        const enemyGroup = interaction.options.getString('rakip') || '';
        const zoneName = interaction.options.getString('bolge') || '';

        const actionText = {
            keeps: T('bölgeyi korudu', 'keeps zone'),
            takes: T('bölgeyi ele geçirdi', 'takes over the zone'),
            wins: T('rakibini yendi:', 'wins against'),
            loses: T('rakibine yenildi:', 'loses to')
        }[action] || action;

        let description;
        if (action === 'wins' || action === 'loses') {
            description = `**${ownGroup}** ${actionText} **${enemyGroup || '?'}**`;
        } else {
            description = `**${ownGroup}** ${actionText}${zoneName ? ` **${zoneName}**` : ''}`;
        }

        const embed = new EmbedBuilder()
            .setTitle(T('⚔️ BÖLGE SAVAŞI', '⚔️ ZONE WAR'))
            .setDescription(description)
            .setColor(action === 'loses' ? 0xFF0000 : 0xFFA500)
            .setTimestamp();

        const channelId = await sendToGuildChannel(guild, 'zoneWarChannelId', { embed });
        await interaction.reply({
            content: channelId
                ? T(`✅ Bölge savaşı bildirimi <#${channelId}> kanalına gönderildi.`, `✅ Zone-war notification sent to <#${channelId}>.`)
                : T("⚠️ zonewar kanali ayarlanmamis. `/setchannel zone-war <kanal>` ile ayarlayin.", "⚠️ No zone-war channel set. Use `/setchannel zone-war <channel>`."),
            ephemeral: true
        });
    }
};
