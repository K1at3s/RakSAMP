# v1.5.1

## Spawn / Konum
- **Virtual world spawn fix**: spawn menüsü "Freeroam" cevabı GTA-client formatına (BYTE uzunluk) çevrildi — bot artık gerçek freeroam'da spawn oluyor (SF kulesi/virtual world'de kalmıyor).
- **`?zgo` yerin dibi fix**: teleport Z 25 → 100 (yüksekten düşüp zemin Z'sine oturur).

## Tag / Grup
- **`?tag <isim> <tag>`**: grup tag'ini elle kaydet — yerel dosya + DB'ye isim eşleşmeli push (sunucu komutu göndermez). `?zgo GS` gibi tag ile zone'a gitme çalışır.
- Tag boş/placeholder (tag==isim) gruplar `?zgrp`'ta yeniden taranır.

## Karakter / Diğer
- Welcome mesajı İngilizce şubeye `!` eklendi (grup chat'e düşer, normal chate değil).
- Backend kapalıysa client kendini kapatır (60 sn kontrol) + temiz Türkçe uyarı.
- README güncellendi: tüm bot komutları, Discord slash komutları, DB şeması.

# v1.5.0

## UIF Login Fix (tmp hesap çözümü)
- **Login dialog submit GTA-client formatina cevrildi**: BYTE uzunluk + listitem 0xFFFF. Önceden WORD uzunluk gönderiliyordu; sunucu şifreyi yanlış parse edip botu tmp hesaba atıyordu. Artık SA-MP client ile birebir aynı şekilde hesaba giriliyor.
- **Login bekleme mekanizmasi**: baglantida spawn isteği login tamamlanana kadar ertelenir; "You logged in" mesajı sonrası 3 sn beklenip sunucunun oturumu işlemesi sağlanır (tmp hesaba düşme engellenir).
- **Grup adı öğrenme düzeltildi**: `/stats` → "Group" butonuna bas → ikinci diyalogdan doğru grup adı okunur. Harvest ile çakışma, spawn öncesi gönderim, sonsuz kilitlenme giderildi. Doğru öğrenildi: `uL - watchin' u gaze`.

## /grole / /grank (Discord interaktif)
- **Discord `/grole`**: paginated üye listesi butonlu sayfalama (◀ ▶) → üye seç → rol seç (Member/Co-Leader) → oyunda paginated listede hedef aranıp uygulanır. Direkt `/grole name role` da çalışır.
- **Discord `/grank`**: üye seç → rank seç (Member/Co-Leader/Leader/Elder/Veteran/Recruit).
- **DB izinli rol kontrolü**: `/setallowedrole` ile menüleri kullanabilecek rol ayarlanır (DB `allowed_role_id`); boşsa herkes.
- Sayfalama rate-limit dostu tek `editReply` ile; menüler 5 dk sonra otomatik kapanır.

## Welcome (per-user)
- **Per-user welcome**: `?welcomemsg <isim> <mesaj>` — giren kullanıcıya özel mesaj (DB `welcome_per_user`). `%s` desteği, `?welcomemsg list`, `clear`. Global `?welcomemsg <mesaj>` fallback olarak durur.
- Hoş geldin mesajları `!` ile grup chat'e (SAMP'ta `(GROUP)` olarak görünür).

## Zone / War
- **Harvest sonrasi otomatik zone taramasi**: grup harvesti bitince yeni veri varsa zscan off olsa bile tarama başlar.
- **`?zscan on`** savaş biteli uzun süre geçtiyse takılı kalmaz: `takes over` de grup adına bağlı işlenir + 15 dk stuck-savaş temizliği.
- **wins/loses her grup için değil**: sadece botun grubunun adı mesajda geçiyorsa işlenir.
- **?gzt** saniyesi saniyesine: tam eşikte duyurur (09:10/09:03/09:01/00:10/00:03/00:01), geç açılımda geçmiş eşiği tekrar duyurmaz.
- **Serbest zone listesi buffer fix** (scan sonrası crash 0xc0000409 giderildi), dialog satır buffer'ları 512'ye büyütüldü.

## Karakter / Kodlama
- **`/utf-8` derleme bayragi + UTF-8 BOM** tüm C++ kaynaklarında — emoji/karakterler doğru derlenir.
- Backend ve client'ta Türkçe özel karakterler ASCII'ye çevrildi (konsol kod sayfasından bağımsız); emojiler korundu.
- Backend açılış logları düzgün görünür (chcp 65001).
- `?zf`/`?pl` isimlerinde renk kodları temizlenir; chat çıkış kuyruğu 5 sn → 1 sn (çok parçalı mesajlar hızlı gider).