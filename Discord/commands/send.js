const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, PermissionFlagsBits } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { isOnline, replyOffline } = require('../utils/clientPresence');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('send')
        .setNameLocalizations({ tr: 'gonder', 'en-US': 'send' })
        .setDescription('Sends a command/chat message to the in-game RakSAMP client')
        .setDescriptionLocalizations({ tr: 'Oyun içindeki RakSAMP istemcisine komut/sohbet mesajı iletir', 'en-US': 'Sends a command/chat message to the in-game RakSAMP client' })
        .addStringOption(o => o.setName('komut')
            .setDescription("Command or chat message to send").setDescriptionLocalizations({"tr": "İletilecek komut veya sohbet mesajı", "en-US": "Command or chat message to send"})
            .setRequired(true))
        .setDefaultMemberPermissions(PermissionFlagsBits.Administrator),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const cmd = interaction.options.getString('komut').trim();
        const guild = await Guild.findOne({ guildId: interaction.guildId });

        if (!guild || !guild.linkedHwids || guild.linkedHwids.length === 0) {
            return interaction.reply({
                content: T("❌ Bu sunucu henuz bir RakSAMP istemcisine bagli degil. Once `/link <hwid>` kullanin.", "❌ No RakSAMP client is linked to this server. Use `/link <hwid>` first."),
                ephemeral: true
            });
        }

        const hwid = guild.linkedHwids[0];
        if (!isOnline(hwid)) return replyOffline(interaction, guild);
        queueCommand(hwid, { type: 'command', cmd, author: interaction.user.username });

        await interaction.reply(T(`✅ Komut kuyruğa alındı. **\`${hwid}\`** istemcisine iletiliyor: \`${cmd.slice(0, 150)}\``, `✅ Command queued for client **\`${hwid}\`**: \`${cmd.slice(0, 150)}\``));
    }
};
