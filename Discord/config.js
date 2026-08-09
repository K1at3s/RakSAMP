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
    DISCORD_BOT_TOKEN: process.env.DISCORD_BOT_TOKEN || "MTUzNTM0NzEyMDgzMDI4ODA0Mw.GjAI6z.kdFOUp4wAPrrni9pJOYO2AAR5imCcwkVY5klkw",
    // SRV cozumlemesi bu makinede kirik oldugu icin shard hostname'leri acikca yazilir;
    // server.js mongoLookup ile bunlari IP'lere cevirir.
    MONGO_URI: process.env.MONGO_URI || "mongodb://haki:burhan.30112006@ac-vcpmnvc-shard-00-00.1e01cf9.mongodb.net:27017,ac-vcpmnvc-shard-00-01.1e01cf9.mongodb.net:27017,ac-vcpmnvc-shard-00-02.1e01cf9.mongodb.net:27017/?ssl=true&authSource=admin&replicaSet=atlas-1384dz-shard-0&retryWrites=true&w=majority&serverSelectionTimeoutMS=10000",
    MONGO_OPTIONS: { lookup: mongoLookup },

    AES_SECRET_KEY: crypto.createHash('sha256').update('BURHAN_RAKSAMP_SECRET_2026').digest()
};
