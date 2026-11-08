process.env.DISCORD_BOT_TOKEN ||= 'selftest-placeholder';
process.env.RAKSAMP_AES_SECRET ||= 'selftest-only-secret-material-at-least-32-characters';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const { Guild } = require('./models');
const db = require('./db');
const { translator, language } = require('./utils/i18n');

const source = fs.readFileSync(require.resolve('./server'), 'utf8');
const originalFind = Guild.findOne;
const originalExecute = db.execute;
let selectedGuild;
Guild.findOne = async () => selectedGuild;
db.execute = async () => [];

function interaction(options = {}) {
    return {
        guildId: 'localization-test', locale: 'en-US', user: { username: 'Tester', id: 'test' },
        member: { permissions: { has: () => false }, roles: { cache: { has: () => false } } },
        guild: { ownerId: 'someone-else' }, messages: [],
        options: { getString: () => null, getInteger: () => null, ...options },
        async reply(value) { this.messages.push(value); },
        async editReply(value) { this.messages.push(value); },
        async deferReply() {},
    };
}

// Execute the actual HTTP route code with in-memory dependencies; no bot login or network.
async function route(path, body, guild) {
    let handler;
    const messages = [];
    const pending = interaction();
    pending.editReply = async value => messages.push(value);
    const start = source.indexOf(`app.post('${path}',`);
    assert.ok(start >= 0, path);
    const end = source.indexOf('\n});', start) + 4;
    const context = { app: { post: (_, fn) => { handler = fn; } }, Guild, db,
        console: { log() {}, error() {} }, resolveGuildForRakSAMP: async () => guild,
        L: (record, tr, en) => translator(record)(tr, en),
        getPendingInteraction: () => pending, removePendingInteraction() {},
        EmbedBuilder: require('discord.js').EmbedBuilder,
        sendToGuildChannel: async (_, field, value) => { messages.push(value); return 'channel'; },
        client: { channels: { fetch: async () => ({ send: async value => messages.push(value) }) } },
    };
    vm.runInNewContext(source.slice(start, end), context);
    let result;
    await handler({ body }, { json: value => { result = value; return value; } });
    return { messages, result };
}

async function roster(lang, existing) {
    const writes = [];
    const api = { spreadsheets: {
        get: async () => ({ data: { properties: { title: 'Test - Members List' }, sheets: [
            { properties: { sheetId: 0, title: 'Members', gridProperties: { rowCount: 1000, columnCount: 26 } } },
            { properties: { sheetId: 1, title: 'Summary' } },
        ] } }),
        batchUpdate: async () => ({}),
        values: {
            get: async ({ range }) => ({ data: { values: range === 'A4:G500' && existing ? [['1', 'Ali', '2', 'Member', 'Member', '', 'Online']] : [] } }),
            update: async value => { writes.push(value); }, clear: async () => ({}),
        },
    } };
    const module = { exports: {} };
    vm.runInNewContext(fs.readFileSync(require.resolve('./sheets'), 'utf8'), {
        module, __dirname, console: { log() {}, error() {} },
        require: name => {
            if (name === 'googleapis') return { google: { auth: { JWT: class {} }, sheets: () => api } };
            if (name === './config') return { GOOGLE_SHEETS: { serviceAccountFile: 'mock.json' } };
            if (name === 'fs') return { existsSync: () => true, readFileSync: () => '{}' };
            return require(name);
        },
    });
    await module.exports.writeRoster('test-sheet', 'Test', [{ name: 'Ali', accountId: '2', rank: 'Member', role: 'Member', vip: '', lastActive: 'Online' }], lang);
    const heading = writes.find(w => w.range === 'A1' || w.range === 'A1:G3');
    assert.equal(heading.requestBody.values[2][1], lang === 'tr' ? 'İsim' : 'Name');
    assert.match(heading.requestBody.values[1][0], lang === 'tr' ? /üye -- Güncellendi/ : /members -- Updated/);
    const summary = writes.find(w => w.range === 'Summary!A1:B3');
    assert.equal(summary.requestBody.values[0][0], lang === 'tr' ? 'Toplam Üye' : 'Total Members');
}

(async () => {
    assert.equal(language({ lang: 'tr' }, { locale: 'en-US' }), 'tr');
    assert.equal(language({ lang: 'en' }, { locale: 'tr' }), 'en');
    assert.equal(language(null, { locale: 'tr' }), 'tr');
    assert.equal(language(null, { locale: 'en-GB' }), 'en');
    let commandCount = 0;
    for (const file of fs.readdirSync(__dirname + '/commands').filter(f => f.endsWith('.js'))) {
        const data = require('./commands/' + file).data.toJSON();
        for (const item of [data, ...(data.options || [])]) {
            assert.ok(item.description_localizations?.tr, file + ': Turkish description');
            assert.ok(item.description_localizations?.['en-US'], file + ': English description');
        }
        commandCount++;
    }
    for (const lang of ['tr', 'en']) {
        await roster(lang, false);
        await roster(lang, true);
        selectedGuild = { lang, linkedHwids: [], allowedRoleId: 'restricted', guildId: 'test' };
        for (const name of ['admins', 'astats', 'stats', 'gstats', 'gmembers', 'send', 'pm']) {
            const i = interaction({ getString: () => 'test' });
            await require('./commands/' + name).execute(i);
            assert.match(i.messages[0].content, lang === 'tr' ? /ba[gğ]l[iı]|istemci/ : /linked|client/i, name);
        }
        for (const name of ['grole', 'grank', 'gkick', 'ginv', 'bridge', 'gname', 'glog', 'gzt', 'zscan', 'announce']) {
            const i = interaction();
            await require('./commands/' + name).execute(i);
            assert.match(i.messages[0].content, lang === 'tr' ? /yetkin|ba[gğ]l[iı]/ : /permission|linked/i, name);
        }
        const unlink = interaction();
        await require('./commands/unlink').execute(unlink);
        assert.match(unlink.messages[0].content, lang === 'tr' ? /bağlı bot yok/ : /No bot is linked/);
        const backup = await route('/api/backup', { playerName: 'Ali', ownGroup: 'Test' }, selectedGuild);
        assert.match(backup.messages[0].content, lang === 'tr' ? /destek istiyor/ : /needs backup/);
        const pm = await route('/api/pm/reply', { requesterId: 'test', author: 'Ali', reply: 'hello' }, selectedGuild);
        assert.equal(pm.messages[0].embeds[0].data.title, lang === 'tr' ? '📥 Oyun İçinden Yanıt' : '📥 Reply from the Game');
        const stats = await route('/api/gstats', { statsText: 'data', requesterId: 'test' }, selectedGuild);
        assert.equal(stats.messages[0].embeds[0].data.title, lang === 'tr' ? '📊 Grup İstatistikleri' : '📊 Group Stats');
        const admins = await route('/api/admins', { adminsText: 'data', requesterId: 'test' }, selectedGuild);
        assert.equal(admins.messages[0].embeds[0].data.title, lang === 'tr' ? '🛡️ Çevrimiçi Yöneticiler' : '🛡️ Online Admins');
    }
    console.log(`Localization: OK — ${commandCount} command schemas, TR/EN command errors, backup, PM, stats and admin replies.`);
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(() => {
    Guild.findOne = originalFind;
    db.execute = originalExecute;
});
