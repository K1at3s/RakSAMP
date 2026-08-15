const crypto = require('crypto');

module.exports = {
    PORT: process.env.PORT || 3000,

    // Ortam değişkeni yoksa eski sabitler kullanılır (üretimde .env önerilir)
    DISCORD_BOT_TOKEN: process.env.DISCORD_BOT_TOKEN || "MTUzNTM0NzEyMDgzMDI4ODA0Mw.GjAI6z.kdFOUp4wAPrrni9pJOYO2AAR5imCcwkVY5klkw",

    // Veri katmanı: yalnızca MySQL (Mongo katmanı kaldırıldı)
    DB: 'mysql',
    MYSQL: {
        host: process.env.MYSQL_HOST || '127.0.0.1',
        port: parseInt(process.env.MYSQL_PORT || '3306', 10), // doğrudan MySQL; ProxySQL eklenirse 6033
        user: process.env.MYSQL_USER || 'raksamp',
        password: process.env.MYSQL_PASSWORD || 'raksamp',
        database: process.env.MYSQL_DB || 'raksamp',
        waitForConnections: true,
        connectionLimit: 5
    },

    AES_SECRET_KEY: crypto.createHash('sha256').update('BURHAN_RAKSAMP_SECRET_2026').digest()
};
