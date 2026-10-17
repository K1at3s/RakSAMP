const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { submit: submitTrackedCommand } = require('../utils/commandTracker');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { setPendingInteraction } = require('../utils/pendingReplies');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('grank')
        .setNameLocalizations({ tr: 'gran', 'en-US': 'grank' })
        .setDescription('Give a group member a rank — interactive or direct')
        .setDescriptionLocalizations({ tr: 'Grup üyesine rank ver — interaktif veya direkt', 'en-US': 'Give a group member a rank — interactive or direct' })
        .addStringOption(o => o.setName('name').setDescription("Player name (leave blank to choose from the list)").setDescriptionLocalizations({"tr": "Oyuncu adı (boş bırakırsan listeden seçersin)", "en-US": "Player name (leave blank to choose from the list)"}).setRequired(false))
        .addStringOption(o => o.setName('rank').setDescription("Rank name (e.g. Co-Leader, Member)").setDescriptionLocalizations({"tr": "Rütbe adı (ör. Co-Leader, Member)", "en-US": "Rank name (e.g. Co-Leader, Member)"}).setRequired(false)),
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
        const rank = interaction.options.getString('rank');
        if (name && rank) {
            await submitTrackedCommand(interaction, guild, hwid, { type: 'grank', target: name, rank });

            console.log(`🏷️ [GRANK] Direkt: ${name} -> ${rank} (guild=${interaction.guildId})`);
            return;
        }
        await interaction.deferReply({ ephemeral: true });
        setPendingInteraction(interaction.id, interaction);
        queueCommand(hwid, { type: 'grank_list', requesterId: interaction.id, guildId: interaction.guildId, hwid });
        await interaction.editReply({ content: T("⏳ Uye listesi oyundan cekiliyor...", "⏳ Fetching the member list from the game...") });
        console.log(`👥 [GRANK-LIST] Istek kuyruga alindi (guild=${interaction.guildId}, req=${interaction.id.slice(0,6)})`);
    }
};
