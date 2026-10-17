const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { submit: submitTrackedCommand } = require('../utils/commandTracker');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('grole')
        .setNameLocalizations({ tr: 'grol', 'en-US': 'grole' })
        .setDescription('Give a group member a role (Member / Co-Leader) — interactive or direct')
        .setDescriptionLocalizations({ tr: 'Grup üyesine rol ver (Member / Co-Leader) — interaktif veya direkt', 'en-US': 'Give a group member a role (Member / Co-Leader) — interactive or direct' })
        .addStringOption(o => o.setName('name').setDescription("Player name (leave blank to choose from the list)").setDescriptionLocalizations({"tr": "Oyuncu adı (boş bırakırsan listeden seçersin)", "en-US": "Player name (leave blank to choose from the list)"}).setRequired(false))
        .addStringOption(o => o.setName('role').setDescription("Role: mem (Member) or co (Co-Leader)").setDescriptionLocalizations({"tr": "Rol: mem (Üye) veya co (Yardımcı Lider)", "en-US": "Role: mem (Member) or co (Co-Leader)"}).setRequired(false)
            .addChoices(
                { name: 'Member (mem)', name_localizations: { tr: 'Üye (mem)' }, value: 'mem' },
                { name: 'Co-Leader (co)', name_localizations: { tr: 'Yardımcı Lider (co)' }, value: 'co' }
            )),
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
        const role = interaction.options.getString('role');

        // Direkt mod: name + role verildiyse direk otomasyona gonder
        if (name && role) {
            const roleMap = { mem: 'Member', co: 'Co-Leader' };
            const fullRole = roleMap[role] || role;
            await submitTrackedCommand(interaction, guild, hwid, { type: 'grole', target: name, role: fullRole });

            console.log(`👤 [GROLE] Direkt: ${name} -> ${fullRole} (guild=${interaction.guildId})`);
            return;
        }
        // Interaktif mod: tum uyeleri cek, select menu goster
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);
        queueCommand(hwid, { type: 'grole_list', requesterId: interaction.id, guildId: interaction.guildId, hwid });
        await interaction.editReply({ content: T("⏳ Uye listesi oyundan cekiliyor...", "⏳ Fetching the member list from the game...") });
        console.log(`👥 [GROLE-LIST] Istek kuyruga alindi (guild=${interaction.guildId}, req=${interaction.id.slice(0,6)})`);
    }
};
