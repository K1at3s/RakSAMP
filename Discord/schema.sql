-- RakSAMP MySQL/ProxySQL şeması. ProxySQL'e (6033) veya doğrudan MySQL'e (3306) uygula:
--   mysql -h 127.0.0.1 -P 6033 -u raksamp -p < schema.sql
CREATE DATABASE IF NOT EXISTS raksamp CHARACTER SET utf8mb4;
USE raksamp;

CREATE TABLE IF NOT EXISTS guilds (
    guild_id VARCHAR(255) PRIMARY KEY,
    linked_hwids TEXT NULL,               -- JSON dizisi: ["HWID1","HWID2"]
    log_channel_id VARCHAR(255) NULL,
    war_channel_id VARCHAR(255) NULL,
    gstats_channel_id VARCHAR(255) NULL,
    group_chat_channel_id VARCHAR(255) NULL,
    zone_war_channel_id VARCHAR(255) NULL,
    backup_channel_id VARCHAR(255) NULL,
    welcome_channel_id VARCHAR(255) NULL,
    group_chat_enabled TINYINT NOT NULL DEFAULT 0,
    lang VARCHAR(8) NOT NULL DEFAULT 'en',
    group_name VARCHAR(255) NOT NULL DEFAULT '',   -- botun oyun içi grubu (poll'dan öğrenilir)
    created_at VARCHAR(64) NULL
);

CREATE TABLE IF NOT EXISTS licenses (
    hwid VARCHAR(255) PRIMARY KEY,
    is_approved TINYINT NOT NULL DEFAULT 0,
    welcome_enabled TINYINT NOT NULL DEFAULT 1,
    welcome_message VARCHAR(255) NULL DEFAULT '',
    created_at VARCHAR(64) NULL
);

-- RakSAMP client'ından push edilen oyuncu istatistik cache'i (?pl için)
CREATE TABLE IF NOT EXISTS players (
    id INT PRIMARY KEY,                -- SAMP playerid (0-999)
    name VARCHAR(64) NULL,             -- grup başlığından öğrenilen gerçek ad
    packetloss DOUBLE NULL,
    fps INT NULL,
    ping INT NULL,
    updated_at DATETIME NULL
);

-- RakSAMP client'ından push edilen grup renkleri (zone etiketi için)
CREATE TABLE IF NOT EXISTS group_colors (
    color INT UNSIGNED PRIMARY KEY,    -- 0xRRGGBBAA olarak saklanan RGBA
    tag VARCHAR(96) NULL,
    updated_at DATETIME NULL
);

-- ProxySQL için önerilen tek sorgu kuralı: tümü raksamp DB'sine yönlenir.