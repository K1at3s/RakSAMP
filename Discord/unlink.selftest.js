const assert = require('node:assert/strict');
const { createUnlinkDevice } = require('./utils/unlinkDevice');

function fixture() {
    const rows = new Map([
        ['a', { guild_id: 'a', linked_hwids: JSON.stringify(['BOT', 'OTHER']), group_name: 'Group A', group_chat_enabled: 1 }],
        ['b', { guild_id: 'b', linked_hwids: JSON.stringify(['BOT']), group_name: 'Group B', group_chat_enabled: 1 }],
        ['c', { guild_id: 'c', linked_hwids: JSON.stringify(['THIRD']), group_name: 'Group C', group_chat_enabled: 1 }],
    ]);
    let conflict = false;
    let fail = false;
    const cleanups = [];
    const execute = async (sql, values) => {
        if (fail) throw new Error('offline');
        if (sql.startsWith('SELECT')) {
            return [...rows.values()].filter(row => JSON.parse(row.linked_hwids).includes(values[0]) && (!values[1] || row.guild_id === values[1])).map(row => ({ ...row }));
        }
        const row = rows.get(values[1]);
        if (conflict) {
            row.linked_hwids = JSON.stringify([...JSON.parse(row.linked_hwids), 'NEW']);
            conflict = false;
            return { affectedRows: 0 };
        }
        if (row.linked_hwids !== values[2]) return { affectedRows: 0 };
        row.linked_hwids = values[0];
        if (sql.includes('group_chat_enabled=0')) { row.group_chat_enabled = 0; row.group_name = ''; }
        return { affectedRows: 1 };
    };
    return { rows, cleanups, unlink: createUnlinkDevice(execute, hwid => cleanups.push(hwid)), conflict: () => { conflict = true; }, fail: () => { fail = true; } };
}
(async () => {
    let test = fixture();
    assert.deepEqual(await test.unlink('BOT', 'a'), { removed: 1 });
    assert.deepEqual(JSON.parse(test.rows.get('a').linked_hwids), ['OTHER']);
    assert.equal(test.rows.get('a').group_chat_enabled, 1);
    assert.deepEqual(JSON.parse(test.rows.get('b').linked_hwids), ['BOT']);

    test = fixture();
    assert.deepEqual(await test.unlink('BOT'), { removed: 2 });
    assert.deepEqual(JSON.parse(test.rows.get('a').linked_hwids), ['OTHER']);
    assert.equal(test.rows.get('b').group_name, '');
    assert.equal(test.rows.get('b').group_chat_enabled, 0);
    assert.deepEqual(JSON.parse(test.rows.get('c').linked_hwids), ['THIRD']);
    assert.deepEqual(await test.unlink('BOT'), { removed: 0 });
    assert.deepEqual(test.cleanups, ['BOT', 'BOT']);

    test = fixture();
    test.conflict();
    await test.unlink('BOT', 'a');
    assert.deepEqual(JSON.parse(test.rows.get('a').linked_hwids), ['OTHER', 'NEW']);
    test = fixture();
    test.fail();
    await assert.rejects(test.unlink('BOT'), /offline/);
    assert.deepEqual(test.cleanups, []);
    console.log('Unlink: OK — guild scope, own device only, idempotency, queue cleanup, concurrent links, DB failure.');
})().catch(err => { console.error(err); process.exitCode = 1; });
