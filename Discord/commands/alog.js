const { interactionTranslator } = require('../utils/i18n');
const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');
const db = require('../db');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('alog')
        .setNameLocalizations({ tr: 'alog', 'en-US': 'alog' })
        .setDescription("Shows the player's admin log history (ban/mute/kick, mode, reason)")
        .setDescriptionLocalizations({ tr: 'Oyuncunun admin log geçmişini gösterir (ban/mute/kick, mod, sebep)', 'en-US': "Shows the player's admin log history (ban/mute/kick, mode, reason)" })
        .addStringOption(o => o.setName('oyuncu').setDescription("Player name or ID").setDescriptionLocalizations({"tr": "Oyuncu adı veya ID", "en-US": "Player name or ID"}).setRequired(true)),
    async execute(interaction) {
        const T = await interactionTranslator(interaction);
        const q = interaction.options.getString('oyuncu').trim();
        await interaction.deferReply({ ephemeral: true });

        let rows;
        if (/^\d+$/.test(q)) {
            rows = await db.execute(
                `SELECT id, admin_name, target_name, target_id, action, mode, duration_min, reason, created_at
                 FROM admin_logs
                 WHERE target_id = ?
                 ORDER BY id DESC
                 LIMIT 25`,
                [Number(q)]
            );
        } else {
            rows = await db.execute(
                `SELECT id, admin_name, target_name, target_id, action, mode, duration_min, reason, created_at
                 FROM admin_logs
                 WHERE LOWER(target_name) LIKE LOWER(?)
                 ORDER BY id DESC
                 LIMIT 25`,
                [`%${q}%`]
            );
        }

        if (!rows || rows.length === 0) {
            return interaction.editReply({ content: T(`❌ Kayıt yok: **${q}**`, `❌ No records: **${q}**`) });
        }

        const lines = rows.map(r => {
            const actions = { banned: T('Yasaklandı', 'Banned'), muted: T('Susturuldu', 'Muted'), kicked: T('Atıldı', 'Kicked'), jailed: T('Hapsedildi', 'Jailed'), disarmed: T('Silahsızlandırıldı', 'Disarmed') };
            const parts = [actions[r.action] || r.action || '?'];
            if (r.mode) parts.push(`**${r.mode}**`);
            if (r.duration_min) parts.push(T(`${r.duration_min} dk`, `${r.duration_min} min`));
            if (r.reason) parts.push(`*${r.reason}*`);
            const ts = r.created_at ? new Date(r.created_at).toLocaleString(T('tr-TR', 'en-GB'), { timeZone: 'Europe/Istanbul' }) : '';
            return `\`${r.id}\` \`${ts}\` ${parts.join(' | ')}`;
        }).join('\n');

        const embed = new EmbedBuilder()
            .setTitle(`📋 ${T('Yönetici Kayıtları', 'Admin Log')} — ${rows[0].target_name}${rows[0].target_id ? ` (${rows[0].target_id})` : ''}`)
            .setDescription(T(`Son **${rows.length}** kayıt:\n\n${lines}`, `Last **${rows.length}** records:\n\n${lines}`))
            .setColor(0xFF5555)
            .setTimestamp();

        await interaction.editReply({ embeds: [embed] });
    }
};