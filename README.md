RakSAMP
=======
RakSAMP is a fake client and server for SA-MP, extended into a full-featured **UIF (United Islands Freeroam) group bot** with a Node.js/Discord backend.

**Bot:** `hakixld` — otomatik zone tarama, savaş takibi, grup tag harvest, Discord entegrasyonu.

**Authors:** jlfm, bartekdvd, P3ti.  
**Special thanks:** balika011, .silent, n3ptun0.

---

## 🎮 UIF Bot Özellikleri (v1.5+)

RakSAMP, UIF sunucusunda otomatik çalışan bir grup botu olarak genişletildi:

- **Otomatik zone tarama** — flashing zone'ları tek tek teleport + `/gzinfo` ile öğrenir, sahiplik haritasını MySQL'e (`gangs` tablosu) kalıcı yazar. Normal tur bilinmeyen-only, her 4. tur tam kontrol.
- **Grup tag harvest (`?zgrp`)** — `/groups` + `/gstats <ID>` zinciri ile eksik kısa tag'leri toplar, DB'ye yazar.
- **Savaş takibi** — zone war başlangıç/bitiş, `?gzt` geri sayım (09:10/09:03/09:01/00:10/00:03/00:01), kill-list textdraw toplama + Discord'a embed.
- **Discord entegrasyonu** — grup chat köprüsü, `/grole` `/grank` interaktif menüler, per-user welcome, admin/war logları.
- **UIF login fix** — GTA-client formatında dialog submit (BYTE uzunluk), tmp hesaba düşmeden gerçek hesaba giriş.
- **Grup adı öğrenme** — `/stats` → "Group" butonu → ikinci diyalogdan doğru grup adı.

### Kurulum
1. `client/bin/RakSAMPClient.xml` — sunucu adresi, bot ismi, `normal_pos` spawn pozisyonu.
2. `Discord/` — `bun .` ile backend başlat (MySQL `raksamp` DB + Discord bot token).
3. `client/bin/RakSAMPClient.exe` — bot başlat (backend açık olmalı, kapalıysa client kapanır).

### Arka uç (Discord/backend)
`Discord/server.js` — Express + Socket.IO + discord.js + MySQL. Endpoint'ler: `/api/handshake`, `/api/groups`, `/api/bot/status`, `/api/bot/toggles`, `/api/welcome/*`, `/api/grole`, `/api/grank`, `/api/grole/list`, `/api/command/poll`, `/api/groupchat/*` ve daha fazlası.

---

## ⌨️ Oyun İçi Komutlar (grup chat)

Tüm komutlar grup chat'ine `?` önekiyle yazılır. `!` önekli mesajlar Discord'a iletilir.

| Komut | Açıklama |
|-------|----------|
| `?help` | Komut listesi |
| `?test` | Bot çalışıyor mu kontrol |
| `?info` | Bot sürüm bilgisi |
| `?disc` | Discord sunucu linki |
| `?ping` | Ping kontrol |
| `?w` | War help (Discord @everyone, 5 dk cooldown) |
| `?pl [id]` | Oyuncu istatistiği (packetloss/FPS/ping) |
| `?gst` | Bulunulan zone + aktif oyuncu sayısı |
| `?gstats [id]` | Grup istatistikleri (Discord'a gönderir) |
| `?astats <id>` | Account istatistikleri |
| `?alog <id>` | Admin log özeti |
| `?az` | Serbest zone bilgisi |
| `?zlist` / `?zwho` | Bulunulan zone bilgisi (/gzinfo) |
| `?zscan on/off` | Otomatik zone tarama aç/kapa |
| `?zgo <#N\|tag\|isim>` | Zone'a ışınlan, 40 sn bekle, geri dön |
| `?za` / `?zattack` | Zone saldırısı (/gz) |
| `?zsnip [zone]` | Zone sniper (savaş bitince /gz, Co-Leader yetki) |
| `?zf` | Flashing zone dağılımı + bilinmeyenleri tara |
| `?zgrp on/off` | Grup tag harvest (tek seferlik) |
| `?tagscan` | `?zgrp` alias |
| `?tag <isim> <tag>` | Grup tag'ini elle kaydet (yerel + DB) |
| `?gzt on/off/time` | Savaş geri sayım duyuruları |
| `?gz` / `?gz2` | Savaşa katıl / yedek çağır |
| `?greet on/off` | Welcome mesajları |
| `?welcomemsg <isim> <mesaj>` | Per-user welcome mesajı (kişi bazlı) |
| `?gm <mesaj>` | Global welcome mesajı alias |
| `?gname` | Grup adını yeniden öğren |
| `?tr` / `?en` | Bot dili Türkçe/İngilizce |
| `?discordchat on/off` | Discord grup chat köprüsü |
| `?guildid <id>` | HWID'i Discord sunucusuna bağla |
| `?post <neden>` | Rol değişikliği gerekçesi (Discord embed) |
| `?m` | Test mesajı |
| `!<mesaj>` | Discord'a grup chat mesajı gönder |

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
| `/backup` | Acil durum çağrısı (@here) |
| `/grole [name] [role]` | Grup üyesine rol ver (Member/Co-Leader) — interaktif üye/rol seçimi |
| `/grank [name] [rank]` | Grup üyesine rank ver — interaktif |
| `/setallowedrole <rol>` | Grole/Grank menülerini kullanabilecek rolü ayarla |
| `/stats` | Bot/oyuncu istatistikleri |
| `/zonewar` | Zone savaşları kanalı ayarı |

---

## ⚙️ Fake Client (RakSAMP Çekirdeği)

**Available runmodes:**
* 0 = RCON mode
* 1 = Bare mode (doesn't spawn)
* 2 = Still mode (stays still at spawn position)
* 3 = Normal mode (stays still at a position)
* 4 = Follows a player
* 5 = Follows a player with a vehicle

**Available commands:**
* `!exit` / `!quit`: exits client.
* `!reconnect`: reconnects the server.
* `!reload`: reloads settings.
* `!runmode`: sets current runmode.
* `!stats`: shows raknet statistics.
* `!players`: shows list of players.
* `!npcs`: show list of NPCs.
* `!login`: login to RCON.
* `!rcon`: send an RCON command.
* `!goto`: go to players position.
* `!gotocp`: go to the current checkpoint.
* `!autogotocp`: toggle automatic checkpoint teleporter.
* `!imitate`: change imitate name.
* `!vlist`: shows list of vehicles.
* `!vdeath`: send vehicle death notification.
* `!fu`: send lost connection packet to server.
* `!spawn`: spawns fake player.
* `!pickup`: pick up a pickup by ID.
* `!class`: select a class.
* `!menusel`: selects an item from the GTA menu.
* `!kill`: toggle fake kill flooder.
* `!lag`: toggle server lagger.
* `!spam`: toggle reconnect spammer.
* `!joinflood`: toggle join flooder.
* `!chatflood`: toggle chat flooder.
* `!classflood`: toggle class selection flooder.
* `!bulletflood`: flood the server with bullet sync packets.
* `!weapon`: sets the current weapon in the fake player's hand.
* `!selplayer`: sets the followed player's name.
* `!selveh`: sets the fake player's vehicle.
* `!pos`: sets the fake player's position.
* `!follow`: sets the following offset.
* `!pulsator`: pulse health & armor.
* `!change_server`: connect to another server without restarting.
* `!change_name`: change the fake player's name and reconnect.
* `!dialogresponse`: send a dialog response.
* `!logstatus`: show log status.
* `!log`: toggle logging objects/pickups/textlabels/textdraws.
* `!teleport`: show the teleport menu.
* `!scmevent`: send SCM event.
* `!fakekick`: send vehicle enter notification with invalid vehicle id.
* `!seltd`: select a textdraw.
* `!sendrates`: show sendrates.

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

## 🧩 Server (LUA scripting)

You can create LUA scripts for the server.

**Limits:** Max players 1000, max vehicles 2000, max scripts 32.

**Events:** `onScriptStart`, `onPlayerJoin`, `onPlayerSpawn`, `onPlayerCommand`, `onDialogResponse`, `onPlayerMessage` ve daha fazlası.

**Functions:** `setPlayerPos`, `setPlayerPosFindZ`, `showPlayerDialog`, `sendPlayerChatMessage`, `giveWeapon`, `setPlayerHealth`, `sendDeathMessage`, `setGravity` ve daha fazlası.