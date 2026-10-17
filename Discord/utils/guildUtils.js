const Guild = require('../models').Guild;
const db = require('../db');
const { shouldWarn } = require('./clientPresence');

// Sunucu kaydini getir, yoksa varsayilan degerlerle olustur
async function getGuild(guildId) {
    if (!guildId) return null;
    let guild = await Guild.findOne({ guildId });
    if (!guild) {
        guild = await Guild.create({ guildId });
    }
    return guild;
}

// HWID ile bagli oldugu sunucuyu bul
async function getGuildByHwid(hwid) {
    if (!hwid) return null;
    const rows = await db.execute('SELECT guild_id FROM guilds WHERE JSON_CONTAINS(linked_hwids, JSON_QUOTE(?)) LIMIT 2', [hwid]);
    if (rows.length !== 1) {
        if (rows.length > 1 && shouldWarn(`ambiguous-hwid:${hwid}`)) console.error(`AMBIGUOUS-HWID: ${hwid}; multiple guild links, use a bot-specific HWID.`);
        return null;
    }
    return Guild.findOne({ guildId: rows[0].guild_id });
}

module.exports = { getGuild, getGuildByHwid };
