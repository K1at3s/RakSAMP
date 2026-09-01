// hwid -> oyuna iletilecek bekleyen Discord komutlari (gecici, bellekte)
// C++ istemci httplib kullandigi icin socket.io degil, /api/command/poll ile ceker.
const commandOutbox = new Map();

function queueCommand(hwid, payload) {
    if (!hwid) return;
    const list = commandOutbox.get(hwid) || [];
    if (list.length >= 50) list.shift(); // tasmayi onle
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
