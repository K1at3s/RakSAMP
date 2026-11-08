const assert = require('node:assert/strict');
process.env.DISCORD_BOT_TOKEN ||= 'selftest-placeholder';
process.env.RAKSAMP_AES_SECRET ||= 'selftest-only-secret-material-at-least-32-characters';
const { validateKillListContext, createResultDeduplicator } = require('./utils/killListContext');
const db = require('./db');
const { Guild } = require('./models');
const { getGuildByHwid } = require('./utils/guildUtils');

(async () => {
    const capture = { hwid: 'DEVICE-CROMM', playerName: 'volkswagenlover', ownGroup: 'CROMM', redName: 'CROMM', blueName: 'Seamless Gate', warId: 'war-69', wonKnown: true };
    assert.equal(validateKillListContext(capture, 'CROMM'), null);
    assert.equal(validateKillListContext(capture, 'The Youngsters Family'), 'wrong-bot-group');
    assert.equal(validateKillListContext({ ...capture, ownGroup: 'The Youngsters Family' }), 'wrong-war-teams');
    assert.equal(validateKillListContext({ ...capture, wonKnown: false }), 'missing-war-context');
    assert.equal(validateKillListContext({ ...capture, warId: '' }), 'missing-war-context');
    let now = 100;
    const results = createResultDeduplicator(() => now);
    assert.equal(results.reserve(capture.hwid, capture.warId), true);
    assert.equal(results.reserve(capture.hwid, capture.warId), false);
    assert.equal(results.reserve('DEVICE-HAKIXLD', capture.warId), true);
    results.release(capture.hwid, capture.warId);
    assert.equal(results.reserve(capture.hwid, capture.warId), true);
    now += 86400001;
    assert.equal(results.reserve(capture.hwid, capture.warId), true);

    const originalExecute = db.execute;
    const originalFind = Guild.findOne;
    const originalError = console.error;
    try {
        db.execute = async (sql, values) => {
            assert.match(sql, /JSON_CONTAINS/);
            assert.deepEqual(values, ['DEVICE']);
            return [{ guild_id: 'a' }, { guild_id: 'b' }];
        };
        Guild.findOne = async () => { throw new Error('Ambiguous HWID must never choose a guild'); };
        console.error = () => {};
        assert.equal(await getGuildByHwid('DEVICE'), null);
        db.execute = async () => [{ guild_id: 'b' }];
        Guild.findOne = async filter => { assert.deepEqual(filter, { guildId: 'b' }); return { guildId: 'b', lang: 'tr' }; };
        assert.deepEqual(await getGuildByHwid('DEVICE-CROMM'), { guildId: 'b', lang: 'tr' });
        db.execute = async () => [];
        assert.equal(await getGuildByHwid('unknown'), null);
    } finally { db.execute = originalExecute; Guild.findOne = originalFind; console.error = originalError; }
    console.log('War isolation: OK — ambiguous HWIDs, team ownership, unknown outcomes, per-bot deduplication, retry.');
})().catch(err => { console.error(err); process.exitCode = 1; });
