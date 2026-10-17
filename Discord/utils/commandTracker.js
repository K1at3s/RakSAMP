const { queueCommand, commandEvents } = require('./commandOutbox');

const TERMINAL = new Set(['succeeded', 'failed', 'timeout']);
const REASONS = {
    busy: ['İstemci başka bir grup işlemiyle meşgul; tekrar dene.', 'Client is busy with another group action; try again.'],
    member_not_found: ['Üye bulunamadı.', 'Member not found.'],
    option_not_found: ['İstenen rol veya rank bulunamadı.', 'Requested role or rank not found.'],
    permission_denied: ['Botun bu işlem için oyun içi yetkisi yetersiz.', 'The bot lacks in-game permission for this action.'],
    invalid_target: ['Oyuncu bulunamadı veya hedef geçersiz.', 'Player not found or invalid target.'],
    queue_full: ['Komut kuyruğu doldu; işlem gönderilemedi.', 'Command queue overflow; action was not sent.'],
    game_timeout: ['Oyundan doğrulama gelmedi; işlem uygulanmış olabilir.', 'No game confirmation; the action may have been applied.'],
    interrupted: ['Başka bir grup işlemi başladı; oyundaki sonucu kontrol et.', 'Another group action started; check the result in game.'],
    unlinked: ['Botun Discord bağlantısı kaldırıldı; bekleyen işlem iptal edildi.', 'Discord link removed; pending action cancelled.'],
};

function createTracker({ schedule = setTimeout, cancel = clearTimeout, now = Date.now } = {}) {
    const entries = new Map();
    function payload(entry) {
        const tr = entry.lang === 'tr';
        const labels = {
            queued: ['⏳ Bekliyor — komut sıraya alındı.', '⏳ Queued — waiting for delivery.'],
            delivered: ['📨 İletildi — istemcinin işleme alması bekleniyor.', '📨 Delivered — waiting for the client to start.'],
            processing: ['🔄 İşleniyor — oyun sonucu bekleniyor.', '🔄 Processing — waiting for game confirmation.'],
            succeeded: ['✅ Tamamlandı — oyun tarafından doğrulandı.', '✅ Completed — confirmed by the game.'],
            failed: ['❌ Başarısız.', '❌ Failed.'],
            timeout: ['⌛ Sonuç alınamadı.', '⌛ No result received.'],
        };
        const reason = REASONS[entry.reason];
        const detail = reason ? `\n${reason[tr ? 0 : 1]}` : '';
        const target = String(entry.command.target || '').replace(/[`\r\n]/g, '').slice(0, 64);
        const value = String(entry.command.role || entry.command.rank || '').replace(/[`\r\n]/g, '').slice(0, 64);
        return { content: `${labels[entry.status][tr ? 0 : 1]}\n\`/${entry.command.type} ${target}${value ? ` → ${value}` : ''}\`${detail}`, embeds: [], components: [], allowedMentions: { parse: [] } };
    }
    function render(entry) {
        const message = payload(entry);
        // Serialize edits so a slow processing update cannot overwrite the final result.
        entry.edits = entry.edits.then(() => entry.interaction.editReply(message)).catch(err => console.error('Komut durum mesaji guncellenemedi:', err.message));
        return entry.edits;
    }
    function update(id, hwid, status, reason = '') {
        const entry = entries.get(id);
        if (!entry || entry.hwid !== hwid || TERMINAL.has(entry.status)) return false;
        const order = { queued: 0, delivered: 1, processing: 2 };
        if (!TERMINAL.has(status) && (!(status in order) || order[status] <= order[entry.status])) return false;
        // A result is meaningful only after the client has collected the command.
        if (entry.status === 'queued' && status === 'succeeded') return false;
        entry.status = status;
        entry.reason = reason;
        if (TERMINAL.has(status)) {
            cancel(entry.timer);
            entries.delete(id);
        }
        render(entry);
        return true;
    }
    async function submit(interaction, guild, hwid, command, { component = false } = {}) {
        if (component) await interaction.deferUpdate();
        else await interaction.deferReply({ ephemeral: true });
        const id = interaction.id;
        const entry = { interaction, hwid, lang: guild?.lang || 'en', command, status: 'queued', reason: '', edits: Promise.resolve() };
        await interaction.editReply(payload(entry));
        entries.set(id, entry);
        entry.timer = schedule(() => update(id, hwid, 'timeout', entry.status === 'queued' ? '' : 'game_timeout'), 120000);
        entry.timer?.unref?.();
        queueCommand(hwid, { ...command, requesterId: id, expiresAt: now() + 120000, tracked: true });
        return id;
    }
    return { submit, update, entries };
}

const tracker = createTracker();
commandEvents.on('dropped', (hwid, command) => tracker.update(command.requesterId, hwid, 'failed', 'queue_full'));
module.exports = { ...tracker, createTracker };
