const crypto = require('crypto');
const dns = require('dns');

// Atlas shard hostname -> IP eşlemesi.
// Not: Bu makinede bun/node DNS'i (127.0.0.1) kırık oldugu icin SRV cozumlemesi
// (mongodb+srv://) ECONNREFUSED veriyor. Driver SNI'yi host IP degilse gonderir,
// bu yuzden hostname tabanli URI + ozel lookup (IP eşlemesi) kullaniliyor.
// Atlas IP'leri degisirse: nslookup ac-vcpmnvc-shard-00-0X.1e01cf9.mongodb.net
const MONGO_IP_MAP = {
    'ac-vcpmnvc-shard-00-00.1e01cf9.mongodb.net': '89.192.252.49',
    'ac-vcpmnvc-shard-00-01.1e01cf9.mongodb.net': '89.192.252.61',
    'ac-vcpmnvc-shard-00-02.1e01cf9.mongodb.net': '89.192.252.71'
};

// net/tls lookup fonksiyonu: bilinen shard'lari IP'ye sabitler, gerisini DNS'e birakir
function mongoLookup(hostname, options, callback) {
    const ip = MONGO_IP_MAP[hostname];
    if (ip) {
        if (options && options.all) {
            const r = { address: ip, family: 4 };
            if (typeof callback === 'function') callback(null, [r]);
            else return Promise.resolve([r]);
        } else {
            if (typeof callback === 'function') callback(null, ip, 4);
            else return Promise.resolve({ address: ip, family: 4 });
        }
        return;
    }
    return dns.lookup(hostname, options, callback);
}

module.exports = {
    PORT: process.env.PORT || 3000,

    // Ortam değişkeni yoksa eski sabitler kullanılır (üretimde .env önerilir)
    DISCORD_BOT_TOKEN: process.env.DISCORD_BOT_TOKEN || "REDACTED_DISCORD_BOT_TOKEN",
    // SRV cozumlemesi bu makinede kirik oldugu icin shard hostname'leri acikca yazilir;
    // server.js mongoLookup ile bunlari IP'lere cevirir.
    MONGO_URI: process.env.MONGO_URI || "REDACTED_MONGO_URI",
    MONGO_OPTIONS: { lookup: mongoLookup },

    AES_SECRET_KEY: crypto.createHash('sha256').update('REDACTED_AES_SECRET').digest()
};
