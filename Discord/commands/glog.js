const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');
const db = require('../db');
const Guild = require('../models').Guild;
const { hasGroupPermission } = require('../utils/permissions');

// /glog <oyuncu|id> — group_logs tablosundan rol/rank degisiklik gecmisi.
// Kaynak: client'in "(Ad)(accid) changed the rank of ..." yakalamasi (POST /api/grouplog).
module.exports = {
    data: new SlashCommandBuilder()
        .setName('glog')
        .setNameLocalizations({ tr: 'glog', 'en-US': 'glog' })
        .setDescription('Shows the group role/rank change history of a member')
        .setDescriptionLocalizations({ tr: 'Üyenin grup rol/rank değişiklik geçmişini gösterir', 'en-US': 'Shows the group role/rank change history of a member' })
        .addStringOption(o => o
            .setName('oyuncu')
            .setNameLocalizations({ tr: 'oyuncu', 'en-US': 'player' })
            .setDescription('Player name or account ID')
            .setDescriptionLocalizations({ tr: 'Oyuncu adı veya account ID', 'en-US': 'Player name or account ID' })
            .setRequired(true)),
    async execute(interaction) {
        const guild = await Guild.findOne({ guildId: interaction.guildId });
        if (!hasGroupPermission(interaction, guild)) {
            return interaction.reply({ content: '❌ Bu komutu kullanmaya yetkin yok.', ephemeral: true });
        }
        const isTr = guild ? guild.lang === 'tr' : true;
        const q = interaction.options.getString('oyuncu').trim();
        await interaction.deferReply({ ephemeral: true });

        const cols = 'id, actor_name, actor_acc, target_name, target_acc, old_rank, new_rank, created_at';
        const hwids = (guild && Array.isArray(guild.linkedHwids)) ? guild.linkedHwids.filter(h => h) : [];
        if (!hwids.length) {
            return interaction.editReply({ content: isTr ? '❌ Bu sunucuya bağlı bot yok — önce ?guildid ile bağla.' : '❌ No linked bot for this server — link with ?guildid first.' });
        }
        const inPh = hwids.map(() => '?').join(',');
        let rows;
        if (/^\d+$/.test(q)) {
            rows = await db.execute(
                `SELECT ${cols} FROM group_logs
                 WHERE hwid IN (${inPh}) AND (target_acc = ? OR actor_acc = ?)
                 ORDER BY id DESC LIMIT 25`,
                [...hwids, Number(q), Number(q)]
            );
        } else {
            rows = await db.execute(
                `SELECT ${cols} FROM group_logs
                 WHERE hwid IN (${inPh}) AND (LOWER(target_name) LIKE LOWER(?) OR LOWER(actor_name) LIKE LOWER(?))
                 ORDER BY id DESC LIMIT 25`,
                [...hwids, `%${q}%`, `%${q}%`]
            );
        }

        if (!rows || rows.length === 0) {
            return interaction.editReply({ content: isTr ? `❌ Grup log kaydı yok: **${q}**` : `❌ No group log record: **${q}**` });
        }

        const lines = rows.map(r => {
            const ts = r.created_at ? new Date(r.created_at).toLocaleString('tr-TR', { timeZone: 'Europe/Istanbul' }) : '';
            const actor = `${r.actor_name || '?'}${r.actor_acc ? ` (${r.actor_acc})` : ''}`;
            const target = `${r.target_name || '?'}${r.target_acc ? ` (${r.target_acc})` : ''}`;
            const change = `\`${r.old_rank || '-'}\` → \`${r.new_rank || '-'}\``;
            return `\`${r.id}\` \`${ts}\` ${actor} → ${target} | ${change}`;
        }).join('\n');

        const embed = new EmbedBuilder()
            .setTitle(isTr ? `📝 Grup Log — ${q}` : `📝 Group Log — ${q}`)
            .setDescription((isTr ? `Son **${rows.length}** kayıt:\n\n` : `Last **${rows.length}** records:\n\n`) + lines.slice(0, 3900))
            .setColor(0xFF8C00)
            .setTimestamp();

        await interaction.editReply({ embeds: [embed] });
        console.log(`📝 [GLOG] guild=${interaction.guildId} q="${q}" -> ${rows.length} kayit`);
    }
};