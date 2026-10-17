const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { submit: submitTrackedCommand } = require('../utils/commandTracker');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('gkick')
        .setNameLocalizations({ tr: 'gkick', 'en-US': 'gkick' })
        .setDescription('Kick a group member — interactive or direct (reason optional)')
        .setDescriptionLocalizations({ tr: 'Grup üyesini kickle — interaktif veya direkt (sebep opsiyonel)', 'en-US': 'Kick a group member — interactive or direct (reason optional)' })
        .addStringOption(o => o.setName('name').setDescription("Player name (leave blank to choose from the list)").setDescriptionLocalizations({"tr": "Oyuncu adı (boş bırakırsan listeden seçersin)", "en-US": "Player name (leave blank to choose from the list)"}).setRequired(false))
        .addStringOption(o => o.setName('reason').setDescription("Kick reason (optional)").setDescriptionLocalizations({"tr": "Atılma sebebi (isteğe bağlı)", "en-US": "Kick reason (optional)"}).setRequired(false)),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({ content: T("❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil.", "❌ No RakSAMP client is linked to this server."), ephemeral: true });
        }
        if (guild.allowedRoleId && !interaction.member.roles.cache.has(guild.allowedRoleId) && !interaction.member.permissions.has('Administrator') && !interaction.member.permissions.has('ManageGuild') && interaction.guild.ownerId !== interaction.user.id) {
            return interaction.reply({ content: T("❌ Bu komutu kullanmaya yetkin yok.", "❌ You do not have permission to use this command."), ephemeral: true });
        }
        const hwid = guild.linkedHwids[0];
        if (!isOnline(hwid)) return replyOffline(interaction, guild);
        const name = interaction.options.getString('name');
        const reason = interaction.options.getString('reason') || '';

        // Direkt mod: name verildiyse direk otomasyona gonder (sebep bos olabilir)
        if (name) {
            await submitTrackedCommand(interaction, guild, hwid, { type: 'gkick', target: name, reason });

            console.log(`👢 [GKICK] Direkt: ${name} (sebep: ${reason || '-'}) (guild=${interaction.guildId})`);
            return;
        }
        // Interaktif mod: once uye listesi, secimden sonra sebep modal
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);
        queueCommand(hwid, { type: 'gkick_list', requesterId: interaction.id, guildId: interaction.guildId, hwid });
        await interaction.editReply({ content: T("⏳ Uye listesi oyundan cekiliyor...", "⏳ Fetching the member list from the game...") });
        console.log(`👥 [GKICK-LIST] Istek kuyruga alindi (guild=${interaction.guildId}, req=${interaction.id.slice(0,6)})`);
    }
};
