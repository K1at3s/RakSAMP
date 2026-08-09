const mongoose = require('mongoose');

// Discord sunucusu başına ayarlar
// Kanal ID'leri ayarlanmadıysa null döner
const GuildSchema = new mongoose.Schema({
    guildId: { type: String, required: true, unique: true },
    linkedHwids: { type: [String], default: [] },

    // Kanal ID'leri (ayarlanmamışsa null)
    warChannelId: { type: String, default: null },
    gstatsChannelId: { type: String, default: null },
    logChannelId: { type: String, default: null },
    groupChatChannelId: { type: String, default: null },
    zoneWarChannelId: { type: String, default: null },  // ZONE WAR bildirimleri
    backupChannelId: { type: String, default: null },   // ?1414 backup çağrıları
    welcomeChannelId: { type: String, default: null },  // welcome back mesajları

    // Grup chat <-> Discord köprüsü (oyun içi ?discordchat ile aç/kapat, MongoDB'de saklanır)
    groupChatEnabled: { type: Boolean, default: false },

    createdAt: { type: Date, default: Date.now }
});

module.exports = mongoose.model('Guild', GuildSchema);
