const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { submit: submitTrackedCommand } = require('../utils/commandTracker');
const { isOnline, replyOffline } = require('../utils/clientPresence');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('ginv')
        .setDescription('Gruba oyuncu davet et (/ginvite id)')
        .setDescriptionLocalizations({ tr: 'Gruba oyuncu davet et (/ginvite id)', 'en-US': 'Invite a player to the group (/ginvite id)' })
        .addIntegerOption(o => o.setName('id').setDescription('Oyuncu ID').setMinValue(0).setMaxValue(999).setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({ content: '❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil.', ephemeral: true });
        }
        if (guild.allowedRoleId && !interaction.member.roles.cache.has(guild.allowedRoleId) && !interaction.member.permissions.has('Administrator') && !interaction.member.permissions.has('ManageGuild') && interaction.guild.ownerId !== interaction.user.id) {
            return interaction.reply({ content: '❌ Bu komutu kullanmaya yetkin yok.', ephemeral: true });
        }
        const hwid = guild.linkedHwids[0];
        if (!isOnline(hwid)) return replyOffline(interaction, guild);
        const id = interaction.options.getInteger('id');
        await submitTrackedCommand(interaction, guild, hwid, { type: 'ginvite', target: String(id) });

        console.log(`✉️ [GINV] Direkt: ${id} (guild=${interaction.guildId})`);
    }
};
