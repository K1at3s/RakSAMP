const db = require('../db');
const { commandOutbox } = require('./commandOutbox');
const tracker = require('./commandTracker');
let clearChat = () => {};

function createUnlinkDevice(execute, cleanup = () => {}) {
    return async function unlinkDevice(hwid, guildId = null) {
        if (!hwid || typeof hwid !== 'string') throw new Error('HWID required');
        let removed = 0;
        // Compare-and-swap prevents a concurrent /link from losing other HWIDs.
        for (let attempt = 0; attempt < 5; attempt++) {
            const rows = await execute(`SELECT guild_id, linked_hwids FROM guilds WHERE JSON_CONTAINS(linked_hwids, JSON_QUOTE(?))${guildId ? ' AND guild_id=?' : ''}`, guildId ? [hwid, guildId] : [hwid]);
            if (!rows.length) { cleanup(hwid); return { removed }; }
            for (const row of rows) {
                const remaining = JSON.parse(row.linked_hwids || '[]').filter(id => id !== hwid);
                const empty = remaining.length === 0;
                const result = await execute(`UPDATE guilds SET linked_hwids=?${empty ? ", group_name='', group_chat_enabled=0" : ''} WHERE guild_id=? AND linked_hwids=?`, [JSON.stringify(remaining), row.guild_id, row.linked_hwids]);
                removed += result.affectedRows || 0;
            }
        }
        throw new Error('Concurrent link changes; retry unlink');
    };
}

const unlinkDevice = createUnlinkDevice(db.execute, hwid => {
    commandOutbox.delete(hwid);
    clearChat(hwid);
    for (const [id, entry] of tracker.entries) {
        if (entry.hwid === hwid) tracker.update(id, hwid, 'failed', 'unlinked');
    }
});
module.exports = { unlinkDevice, createUnlinkDevice, setChatCleanup: fn => { clearChat = fn; } };
