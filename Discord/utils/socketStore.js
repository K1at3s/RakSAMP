// hwid -> aktif socket.io bağlantısı (çoklu RakSAMP istemcisi desteği)
const hwidSockets = new Map();
const socketHwids = new Map();

function register(hwid, socket) {
    if (!hwid) return;
    if (socketHwids.has(socket.id)) {
        hwidSockets.delete(socketHwids.get(socket.id));
    }
    hwidSockets.set(hwid, socket);
    socketHwids.set(socket.id, hwid);
}

function unregister(socket) {
    const hwid = socketHwids.get(socket.id);
    if (hwid && hwidSockets.get(hwid) === socket) {
        hwidSockets.delete(hwid);
    }
    socketHwids.delete(socket.id);
}

function getSocket(hwid) {
    return hwid ? hwidSockets.get(hwid) : undefined;
}

// Birden fazla HWID bağlıysa ilk çevrimiçi olanı döndür
function getFirstSocket(hwids) {
    for (const hwid of hwids || []) {
        const socket = hwidSockets.get(hwid);
        if (socket) return socket;
    }
    return null;
}

module.exports = { register, unregister, getSocket, getFirstSocket };
