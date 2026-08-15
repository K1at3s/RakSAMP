# TODO

## Yarın (15.08.2026 sonrası)

- [ ] **?zf tag ile çekmiyor, tam grup isimleri basıyor** (netrpc.cpp)
      Kök neden: `GangZoneScanGzDialog` (/gzinfo taraması, ~satır 821) tam grup adını
      ("The Youngsters Family") `g_groupColors[].szTag`'e yazıyor ve harvest/gstats'ın
      öğrendiği kısa tag'i ("GeMs") eziyor. `BuildFlashZoneSummary` (?zf, ~satır 752)
      aynı alanı bastığı için çıktıda tag yerine tam isim görünüyor.
      Çözüm önerisi: `g_groupColors` yapısına ayrı `szName` (tam isim) alanı ekle;
      tarama tam ismi `szName`'e yazsın, ?zf etikette `szTag`'i bassın. group_colors.txt
      formatına da karar ver (tag + isim birlikte saklanabilir mi, ayrı mı).

- [ ] **?zgo tag ile çalışmıyor** (netrpc.cpp `FindZoneByGroupTag` ~184 + handler ~2188)
      Log kanıtı: `?zgo nM` haritada birebir "nM" varken "zone bulunamadi" (nl altında
      görülen : `AF414141 nM`). Tag eşleşiyor ama zone bulunamıyor → olası neden(ler):
      1) Renk formatı uyuşmazlığı: `group_colors.txt` AA formatında (`AF414141`), ama
      `FindZoneByGroupTag` içindeki `g_zoneColor[zid] == uiC` karşılaştırması g_zoneColor'un
      sakladığı formatta (FF?) — bu eşleşmeyi hep false yapar. LoadGroupColors/save
      formatını ve g_zoneColor'ın kayıt noktasını (RPC_ScrAddGangZone) karşılaştır.
      2) Harf duyarlılığı: `?zgo nm` (küçük) vs harita `nM` — FindGroupColorByTag duyarlıysa
      büyük/küçük duyarsız (_stricmp) hale getir.
      3) "free"/"Free" özel anlamı: renk 0'a karşılık geliyor olabilir, özel-case ver.
      Doğrulama: düzeltme sonrası `?zgo nM` → "!Zone #N de..." mesajı gelmeli.