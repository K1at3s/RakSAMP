// hwid -> oyuna iletilecek bekleyen Discord komutlari (gecici, bellekte)
// C++ istemci httplib kullandigi icin socket.io degil, /api/command/poll ile ceker.
const commandOutbox = new Map();
const commandEvents = new (require('node:events').EventEmitter)();

function queueCommand(hwid, payload) {
    if (!hwid) return;
    const list = commandOutbox.get(hwid) || [];
    // Poll retries must never enqueue the same Discord interaction twice.
    if (payload?.requesterId && list.some(command => command.requesterId === payload.requesterId)) return;
    if (list.length >= 50) commandEvents.emit('dropped', hwid, list.shift());
    list.push(payload);
    commandOutbox.set(hwid, list);
}

function drainCommands(hwid) {
    if (!hwid) return [];
    const list = (commandOutbox.get(hwid) || []).filter(command => !command.expiresAt || command.expiresAt > Date.now());
    commandOutbox.delete(hwid);
    return list;
}

module.exports = { queueCommand, drainCommands, commandOutbox, commandEvents };
