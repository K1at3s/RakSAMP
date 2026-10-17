const { language } = require('../utils/i18n');
const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder } = require('discord.js');
const Guild = require('../models').Guild;
const { queueCommand } = require('../utils/commandOutbox');
const { isOnline, replyOffline } = require('../utils/clientPresence');
const { hasGroupPermission, getLinkedHwid } = require('../utils/permissions');

// /gname — grup adini yeniden ogrenmeye zorlar (oyun ici ?gname ile ayni).
// Bot grup adini /stats -> "Group" butonu zinciriyle ogrenir; isim degistiginde
// ya da yanlis ogrenildiginde bu komut ogrenme akisini bastan baslatir.
module.exports = {
    data: new SlashCommandBuilder()
        .setName('gname')
        .setNameLocalizations({ tr: 'gname', 'en-US': 'gname' })
        .setDescription('Re-learns the group name (restarts the in-game learning flow)')
        .setDescriptionLocalizations({ tr: 'Grup adını yeniden öğrenir (oyun içi öğrenme akışını baştan başlatır)', 'en-US': 'Re-learns the group name (restarts the in-game learning flow)' }),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!hasGroupPermission(interaction, guild)) {
            return interaction.reply({ content: T("❌ Bu komutu kullanmaya yetkin yok.", "❌ You do not have permission to use this command."), ephemeral: true });
        }
        const isTr = language(guild, interaction) === 'tr';
        const hwid = getLinkedHwid(guild);
        if (hwid && !isOnline(hwid)) return replyOffline(interaction, guild);
        if (!hwid) {
            return interaction.reply({ content: isTr ? T("❌ Bağlı RakSAMP istemcisi yok.", "❌ No linked RakSAMP client.") : '❌ No linked RakSAMP client.', ephemeral: true });
        }

        queueCommand(hwid, { type: 'command', cmd: '?gname', author: interaction.user.username });
        await interaction.reply({
            content: isTr
                ? '✅ Grup adı yeniden öğreniliyor (sonuç grup chatinde ve `/botdurum` kaydında görünür).'
                : '✅ Re-learning the group name (the result appears in the group chat and in `/botstatus`).',
            ephemeral: true
        });
        console.log(`🏷️ [GNAME] guild=${interaction.guildId} hwid=${hwid.slice(0, 8)}`);
    }
};