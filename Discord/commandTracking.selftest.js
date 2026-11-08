const assert = require('node:assert/strict');
const trackerModule = require('./utils/commandTracker');
const { createTracker } = trackerModule;
const { commandOutbox, drainCommands, queueCommand } = require('./utils/commandOutbox');

let serial = 0;
function interaction() {
    return {
        id: `tracking-test-${++serial}`, messages: [],
        async deferReply(options) { this.deferred = true; this.private = options.ephemeral; },
        async deferUpdate() { this.deferred = true; this.component = true; },
        async editReply(message) { assert.ok(this.deferred); this.messages.push(message); },
    };
}
const flush = () => new Promise(resolve => setImmediate(resolve));

(async () => {
    const timers = [];
    const tracker = createTracker({ schedule: fn => { const t = { fn }; timers.push(t); return t; }, cancel: t => { t.cancelled = true; } });
    const i = interaction();
    await tracker.submit(i, { lang: 'tr' }, 'test-device', { type: 'grole', target: 'Ali', role: 'Co-Leader' });
    assert.equal(i.private, true);
    assert.match(i.messages.at(-1).content, /Bekliyor/);
    assert.equal(tracker.update(i.id, 'wrong-device', 'succeeded'), false);
    assert.equal(tracker.update(i.id, 'test-device', 'succeeded'), false);
    assert.equal(tracker.update(i.id, 'test-device', 'unknown'), false);
    assert.equal(tracker.update(i.id, 'test-device', 'delivered'), true);
    assert.equal(tracker.update(i.id, 'test-device', 'processing'), true);
    assert.equal(tracker.update(i.id, 'test-device', 'delivered'), false);
    assert.equal(tracker.update(i.id, 'test-device', 'succeeded'), true);
    assert.equal(tracker.update(i.id, 'test-device', 'failed'), false);
    await flush();
    assert.match(i.messages.at(-1).content, /Tamamlandı/);
    assert.match(i.messages.at(-1).content, /Ali → Co-Leader/);
    assert.equal(timers[0].cancelled, true);
    const audit = tracker.getAudit();
    assert.deepEqual(audit.map(item => item.status), ['queued', 'delivered', 'processing', 'succeeded']);
    assert.ok(audit.every(item => !('target' in item) && !('hwid' in item)));
    timers[0].fn();
    await flush();
    assert.match(i.messages.at(-1).content, /Tamamlandı/);

    const failed = interaction();
    await tracker.submit(failed, { lang: 'en' }, 'test-device', { type: 'gkick', target: 'Bob' }, { component: true });
    assert.equal(failed.component, true);
    tracker.update(failed.id, 'test-device', 'delivered');
    tracker.update(failed.id, 'test-device', 'failed', 'member_not_found');
    await flush();
    assert.match(failed.messages.at(-1).content, /Member not found/);

    const timeout = interaction();
    await tracker.submit(timeout, { lang: 'tr' }, 'test-device', { type: 'grank', target: 'Ali', rank: 'Veteran' });
    tracker.update(timeout.id, 'test-device', 'processing');
    timers.at(-1).fn();
    await flush();
    assert.match(timeout.messages.at(-1).content, /uygulanmış olabilir/);
    assert.equal(tracker.entries.size, 0);
    assert.equal(tracker.update(timeout.id, 'test-device', 'succeeded'), false);

    // A slow edit must finish before a later final edit starts.
    const race = interaction();
    await tracker.submit(race, { lang: 'en' }, 'test-race', { type: 'ginvite', target: '5' });
    const originalEdit = race.editReply.bind(race);
    let release;
    race.editReply = async message => {
        if (message.content.includes('Processing')) await new Promise(resolve => { release = resolve; });
        return originalEdit(message);
    };
    tracker.update(race.id, 'test-race', 'processing');
    await flush();
    tracker.update(race.id, 'test-race', 'succeeded');
    release();
    await flush();
    assert.match(race.messages.at(-1).content, /Completed/);

    queueCommand('test-expired', { requesterId: 'duplicate', cmd: 'first' });
    queueCommand('test-expired', { requesterId: 'duplicate', cmd: 'retry' });
    assert.deepEqual(drainCommands('test-expired'), [{ requesterId: 'duplicate', cmd: 'first' }]);
    queueCommand('test-expired', { expiresAt: Date.now() - 1 });
    queueCommand('test-expired', { cmd: 'valid' });
    assert.deepEqual(drainCommands('test-expired'), [{ cmd: 'valid' }]);

    const overflow = interaction();
    await trackerModule.submit(overflow, { lang: 'en' }, 'test-overflow', { type: 'grole', target: 'Ali' });
    for (let n = 0; n < 50; n++) queueCommand('test-overflow', { cmd: String(n) });
    await flush();
    assert.match(overflow.messages.at(-1).content, /queue overflow/);
    assert.equal(trackerModule.entries.has(overflow.id), false);

    // Exercise all four real slash handlers without connecting to Discord or MySQL.
    const { Guild } = require('./models');
    const originalFind = Guild.findOne;
    Guild.findOne = async () => ({ linkedHwids: ['test-slash'], lang: 'tr' });
    const { markOnline } = require('./utils/clientPresence');
    markOnline('test-slash');
    try {
        for (const name of ['grole', 'grank', 'gkick', 'ginv']) {
            const slash = interaction();
            slash.guildId = 'test-guild';
            slash.options = { getString: key => ({ name: 'Ali', role: 'co', rank: 'Veteran', reason: 'test' })[key], getInteger: () => 5 };
            await require(`./commands/${name}`).execute(slash);
            const [command] = drainCommands('test-slash');
            assert.equal(command.requesterId, slash.id);
            assert.equal(command.tracked, true);
            assert.equal(command.type, name === 'ginv' ? 'ginvite' : name);
            trackerModule.update(slash.id, 'test-slash', 'processing');
            trackerModule.update(slash.id, 'test-slash', 'succeeded');
        }
        await flush();
    } finally { Guild.findOne = originalFind; }
    commandOutbox.clear();
    console.log('Command tracking: OK — lifecycle, HWID isolation, timeout, edit ordering, overflow, expiry, four slash handlers.');
})().catch(err => { console.error(err); process.exitCode = 1; });
