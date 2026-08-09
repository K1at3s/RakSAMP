// hwid -> oyuna iletilecek bekleyen Discord komutları (geçici, bellekte)
// C++ istemci httplib kullandığı için socket.io değil, /api/command/poll ile çeker.
const commandOutbox = new Map();

function queueCommand(hwid, payload) {
    if (!hwid) return;
    const list = commandOutbox.get(hwid) || [];
    if (list.length >= 50) list.shift(); // taşmayı önle
    list.push(payload);
    commandOutbox.set(hwid, list);
}

function drainCommands(hwid) {
    if (!hwid) return [];
    const list = (commandOutbox.get(hwid) || []).slice();
    commandOutbox.delete(hwid);
    return list;
}

module.exports = { queueCommand, drainCommands, commandOutbox };
