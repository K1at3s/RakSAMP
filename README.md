# RakSAMP — UIF Grup Botu

RakSAMP, **UIF (United Islands Freeroam)** sunucusu için geliştirilmiş otomatik grup botudur. SA-MP fake client altyapısı üzerine kurulu, Node.js/Discord backend ile entegre çalışır.

**Bot:** `hakixld` — otomatik zone tarama, savaş takibi, grup tag harvest, Discord entegrasyonu.

---

## 🎮 Özellikler

- **Otomatik zone tarama** — flashing zone'ları tek tek teleport + `/gzinfo` ile öğrenir, sahiplik haritasını MySQL'e (`gangs` tablosu) kalıcı yazar. Normal tur bilinmeyen-only, her 4. tur tam kontrol.
- **Grup tag harvest (`?zgrp`)** — `/groups` + `/gstats <ID>` zinciri ile eksik kısa tag'leri toplar, DB'ye yazar.
- **Savaş takibi** — zone war başlangıç/bitiş, `?gzt` geri sayım (09:10/09:03/09:01/00:10/00:03/00:01), kill-list textdraw toplama + Discord'a embed.
- **Discord entegrasyonu** — grup chat köprüsü, `/grole` `/grank` interaktif menüler, per-user welcome, admin/war logları.
- **UIF login fix** — GTA-client formatında dialog submit (BYTE uzunluk), tmp hesaba düşmeden gerçek hesaba giriş.
- **Grup adı öğrenme** — `/stats` → "Group" butonu → ikinci diyalogdan doğru grup adı.
- **Virtual world fix** — spawn menüsü "Freeroam" cevabı GTA-client formatında, bot gerçek freeroam'da spawn olur.

---

## 🚀 Kurulum

1. **MySQL** — `raksamp` veritabanı oluştur (`Discord/schema.sql`), tablolar otomatik kurulur.
2. **Backend** — `Discord/` klasöründe `bun .` ile başlat (Discord bot token + MySQL ayarları `config.js`).
3. **Client** — `client/bin/RakSAMPClient.exe` çalıştır:
   - `RakSAMPClient.xml` — sunucu adresi, bot ismi (`hakixld`), `normal_pos` spawn pozisyonu.
   - İlk açılışta konsola `/login <şifre>` yazarak hesaba gir (sunucu oturumu sonra hatırlar).
   - Backend kapalıysa client kendini kapatır.

---

## ⌨️ Oyun İçi Komutlar (grup chat)

Tüm komutlar grup chat'ine `?` önekiyle yazılır. `!` önekli mesajlar Discord'a iletilir.

### Genel
| Komut | Açıklama |
|-------|----------|
| `?help` | Komut listesi |
| `?test` | Bot çalışıyor mu kontrol |
| `?info` | Bot sürüm bilgisi |
| `?disc` | Discord sunucu linki |
| `?tr` / `?en` | Bot dili Türkçe/İngilizce |
| `?m` | Test mesajı |

### Oyuncu / İstatistik
| Komut | Açıklama |
|-------|----------|
| `?ping` | Ping kontrol |
| `?w` | War help (Discord @everyone, 5 dk cooldown) |
| `?pl [id]` | Oyuncu istatistiği (packetloss/FPS/ping) |
| `?gst` | Bulunulan zone + aktif oyuncu sayısı |
| `?gstats [id]` | Grup istatistikleri (Discord'a gönderir) |
| `?astats <id>` | Account istatistikleri |
| `?alog <id>` | Admin log özeti |
| `?az` | Serbest zone bilgisi |

### Zone / Tarama
| Komut | Açıklama |
|-------|----------|
| `?zlist` / `?zwho` | Bulunulan zone bilgisi (/gzinfo) |
| `?zscan on/off` | Otomatik zone tarama aç/kapa |
| `?zgo <#N\|tag\|isim>` | Zone'a ışınlan, 40 sn bekle, geri dön |
| `?za` / `?zattack` | Zone saldırısı (/gz) |
| `?zsnip [zone]` | Zone sniper (savaş bitince /gz, Co-Leader yetki) |
| `?zf` | Flashing zone dağılımı + bilinmeyenleri tara |

### Grup / Tag
| Komut | Açıklama |
|-------|----------|
| `?zgrp on/off` | Grup tag harvest (tek seferlik) |
| `?tagscan` | `?zgrp` alias |
| `?tag <isim> <tag>` | Grup tag'ini elle kaydet (yerel + DB) |
| `?gname` | Grup adını yeniden öğren |

### Savaş
| Komut | Açıklama |
|-------|----------|
| `?gzt on/off/time` | Savaş geri sayım duyuruları |
| `?gz` / `?gz2` | Savaşa katıl / yedek çağır |
| `?post <neden>` | Rol değişikliği gerekçesi (Discord embed) |

### Ayarlar
| Komut | Açıklama |
|-------|----------|
| `?greet on/off` | Welcome mesajları |
| `?welcomemsg <isim> <mesaj>` | Per-user welcome mesajı (kişi bazlı) |
| `?gm <mesaj>` | Global welcome mesajı alias |
| `?discordchat on/off` | Discord grup chat köprüsü |
| `?guildid <id>` | HWID'i Discord sunucusuna bağla |

---

## 🤖 Discord Slash Komutları

| Komut | Açıklama |
|-------|----------|
| `/link <hwid>` | HWID'i Discord sunucusuna bağla |
| `/unlink` | Bağlantıyı kaldır |
| `/setup` | Log kanallarını otomatik oluştur |
| `/setchannel` | Kanal atamaları |
| `/gstats [group_id]` | Grup istatistikleri |
| `/admins` | Online adminler |
| `/send <mesaj>` | Oyun içine mesaj gönder |
| `/backup` | Acil durum çağrısı (`@here`) |
| `/grole [name] [role]` | Grup üyesine rol ver (Member/Co-Leader) — interaktif üye/rol seçimi |
| `/grank [name] [rank]` | Grup üyesine rank ver — interaktif |
| `/setallowedrole <rol>` | Grole/Grank menülerini kullanabilecek rolü ayarla |
| `/stats` | Bot/oyuncu istatistikleri |
| `/zonewar` | Zone savaşları kanalı ayarı |

---

## 🗄️ Veritabanı (MySQL `raksamp`)

| Tablo | Açıklama |
|-------|----------|
| `gangs` | Grup adı (PK), tag, zones JSON — kalıcı zone sahiplik haritası |
| `bots` | Bot başına ayarlar: group_name, zscan/gzt/zgrp on-off |
| `licenses` | HWID lisansları + welcome mesajı/enabled |
| `welcome_per_user` | Kişi bazlı welcome mesajları (hwid + isim) |
| `guilds` | Discord sunucusu ayarları (kanallar, allowed_role_id) |
| `players` | Oyuncu istatistik cache (packetloss/FPS/ping) |
| `admin_logs` | Admin eylem logları |
| `group_logs` | Grup rol değişikliği logları |

---

## 📁 Proje Yapısı

```
client/           C++ client (RakSAMPClient.exe + XML config)
  src/            netrpc.cpp, Security.cpp, cmds.cpp, main.cpp...
  bin/            RakSAMPClient.exe, RakSAMPClient.xml, group_colors.txt
raknet/           RakNet + SA-MP RPC katmanı (SAMPRPC.cpp)
Discord/          Node.js backend (server.js, db.js, commands/, models/)
CHANGELOG.md      Sürüm geçmişi
```