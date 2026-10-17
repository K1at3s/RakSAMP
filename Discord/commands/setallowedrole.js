const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionsBitField } = require('discord.js');
const Guild = require('../models').Guild;

module.exports = {
    data: new SlashCommandBuilder()
        .setName('setallowedrole')
        .setNameLocalizations({ tr: 'iznirol', 'en-US': 'setallowedrole' })
        .setDescription('Sets the role allowed to use the Grole/Grank menus (DB)')
        .setDescriptionLocalizations({ tr: 'Grole/Grank menülerini kullanabilecek rolü ayarla (DB)', 'en-US': 'Sets the role allowed to use the Grole/Grank menus (DB)' })
        .addRoleOption(o => o.setName('role').setDescription("Allowed role (leave blank for everyone)").setDescriptionLocalizations({"tr": "İzin verilecek rol (boş bırakırsan herkes)", "en-US": "Allowed role (leave blank for everyone)"}).setRequired(false)),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        if (!interaction.member.permissions.has(PermissionsBitField.Flags.ManageGuild) && interaction.guild.ownerId !== interaction.user.id) {
            return interaction.reply({ content: T("❌ Bu komutu sadece sunucu sahibi veya ManageGuild yetkisi olanlar kullanabilir.", "❌ Only the server owner or members with Manage Server permission can use this command."), ephemeral: true });
        }
        const role = interaction.options.getRole('role');
        let guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!guild) guild = await Guild.create({ guildId: interaction.guildId });
        guild.allowedRoleId = role ? role.id : null;
        await guild.save();
        if (role) {
            await interaction.reply({ content: T(`✅ Grole/Grank menülerini artık sadece <@&${role.id}> rolündekiler kullanabilir.`, `✅ Only members with <@&${role.id}> can now use the Grole/Grank menus.`), ephemeral: true });
        } else {
            await interaction.reply({ content: T("✅ Kisitlama kaldirildi — herkes kullanabilir.", "✅ Restriction removed — everyone can use it."), ephemeral: true });
        }
        console.log(`🔐 [ALLOWED-ROLE] guild=${interaction.guildId} -> ${role ? role.id : 'null'} (by ${interaction.user.tag})`);
    }
};
