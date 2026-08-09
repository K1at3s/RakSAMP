const Guild = require('../models/Guild');

// Sunucu kaydını getir, yoksa varsayılan değerlerle oluştur
async function getGuild(guildId) {
    if (!guildId) return null;
    let guild = await Guild.findOne({ guildId });
    if (!guild) {
        guild = await Guild.create({ guildId });
    }
    return guild;
}

// HWID ile bağlı olduğu sunucuyu bul
async function getGuildByHwid(hwid) {
    if (!hwid) return null;
    return await Guild.findOne({ linkedHwids: hwid });
}

// Kanal ID'sini veritabanından çek, ayarlanmamışsa null
async function getChannelId(guildId, type) {
    const guild = await getGuild(guildId);
    if (!guild) return null;
    return guild[`${type}ChannelId`] || null;
}

module.exports = { getGuild, getGuildByHwid, getChannelId };
