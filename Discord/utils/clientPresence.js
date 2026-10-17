// Client canlilik: poll'lar (~2 sn: /api/command/poll, /api/groupchat/poll)
// geldigi surece bot oyunda sayilir; 15 sn sessizlik = kapali.
// Discord -> oyun yonu (slash komutlar, ! koprusu) gonderim oncesi buradan kontrol edilir.
const lastPoll = new Map();
const warnAt = new Map();
const TIMEOUT_MS = 15000;

function markOnline(hwid) {
    if (hwid) lastPoll.set(String(hwid), Date.now());
}

function isOnline(hwid) {
    if (!hwid) return false;
    const t = lastPoll.get(String(hwid));
    return !!t && (Date.now() - t) < TIMEOUT_MS;
}

// Kanal uyari spam onleme: key basina 60 sn'de bir izin.
function shouldWarn(key, ms) {
    const now = Date.now();
    if (now - (warnAt.get(key) || 0) < (ms || 60000)) return false;
    warnAt.set(key, now);
    return true;
}

// Slash komutlar icin tek satirlik kapali-kontrolu (defer uyumlu cevaplar).
async function replyOffline(interaction, guild) {
    const isTr = require('./i18n').language(guild, interaction) === 'tr';
    const msg = isTr
        ? '❌ Client kapalı — bot oyunda değil, komut iletilemedi.'
        : '❌ Client offline — bot is not in game, command not delivered.';
    try {
        if (interaction.deferred || interaction.replied) await interaction.editReply(msg);
        else await interaction.reply({ content: msg, ephemeral: true });
    } catch {}
    return null;
}

module.exports = { markOnline, isOnline, shouldWarn, replyOffline, TIMEOUT_MS };
