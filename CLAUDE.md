<!-- rtk-instructions v2 -->
# RTK (Rust Token Killer) - Token Optimize Edilmiş Komutlar

## Altın Kural

**Komutlara her zaman `rtk` öneki ekleyin**. RTK'nın ilgili bir filtresi varsa onu kullanır, yoksa komutu değiştirmeden geçirir. Yani RTK'yı kullanmak her zaman güvenlidir.

**Önemli**: `&&` ile bağlanan komut zincirlerinde bile `rtk` kullanın:
```bash
# ❌ Yanlış
git add . && git commit -m "msg" && git push

# ✅ Doğru
rtk git add . && rtk git commit -m "msg" && rtk git push
```

## RTK İş Akışına Göre Komutlar

### Derleme (80-90% tasarruf)
```bash
rtk cargo build         # Cargo derleme çıktısı
rtk cargo check         # Cargo check çıktısı
rtk cargo clippy        # Clippy uyarıları dosyaya göre gruplu (80%)
rtk tsc                 # TypeScript hataları dosya/koda göre gruplu (83%)
rtk lint                # ESLint/Biome ihlalleri gruplu (84%)
rtk prettier --check    # Sadece formatlanması gereken dosyalar (70%)
rtk next build          # Next.js derleme, rota metrikleriyle (87%)
```

### Test (60-99% tasarruf)
```bash
rtk cargo test          # Sadece Cargo test hataları (90%)
rtk go test             # Sadece Go test hataları (90%)
rtk jest                # Sadece Jest hataları (99.5%)
rtk vitest              # Sadece Vitest hataları (99.5%)
rtk playwright test     # Sadece Playwright hataları (94%)
rtk pytest              # Sadece Python test hataları (90%)
rtk rake test           # Sadece Ruby test hataları (90%)
rtk rspec               # Sadece RSpec hataları (60%)
rtk test <komut>        # Genel test sarıcı - sadece hatalar
```

### Git (59-80% tasarruf)
```bash
rtk git status          # Kompakt durum
rtk git log             # Kompakt log (tüm git bayraklarıyla çalışır)
rtk git diff            # Kompakt diff (80%)
rtk git show            # Kompakt show (80%)
rtk git add             # Ultra-kompakt onaylar (59%)
rtk git commit          # Ultra-kompakt onaylar (59%)
rtk git push            # Ultra-kompakt onaylar
rtk git pull            # Ultra-kompakt onaylar
rtk git branch          # Kompakt dal listesi
rtk git fetch           # Kompakt fetch
rtk git stash           # Kompakt stash
rtk git worktree        # Kompakt worktree
```

Not: Git geçişi, açıkça listelenmeyen TÜM alt komutlarda çalışır.

### GitHub (26-87% tasarruf)
```bash
rtk gh pr view <num>    # Kompakt PR görünümü (87%)
rtk gh pr checks        # Kompakt PR kontrolleri (79%)
rtk gh run list         # Kompakt iş akışı çalıştırmaları (82%)
rtk gh issue list       # Kompakt sorun listesi (80%)
rtk gh api              # Kompakt API yanıtları (26%)
```

### JavaScript/TypeScript Araçları (70-90% tasarruf)
```bash
rtk pnpm list           # Kompakt bağımlılık ağacı (70%)
rtk pnpm outdated       # Kompakt eski paketler (80%)
rtk pnpm install        # Kompakt kurulum çıktısı (90%)
rtk npm run <script>    # Kompakt npm script çıktısı
rtk npx <komut>         # Kompakt npx komut çıktısı
rtk prisma              # ASCII sanatı olmayan Prisma (88%)
rtk uv run <komut>      # Kompakt uv proje komut çıktısı
```

### Dosyalar & Arama (60-75% tasarruf)
```bash
rtk ls <yol>            # Ağaç formatı, kompakt (65%)
rtk read <dosya>        # Filtrelemeli kod okuma (60%)
rtk grep <desen>        # Dosyaya göre gruplu arama (75%). Format bayrakları (-c, -l, -L, -o, -Z) ham çalışır.
rtk find <desen>        # Dizine göre gruplu bulma (70%)
```

### Analiz & Hata Ayıklama (70-90% tasarruf)
```bash
rtk err <komut>         # Herhangi bir komuttan yalnızca hataları filtreler
rtk log <dosya>         # Sayılarıyla birlikte tekrarsız loglar
rtk json <dosya>        # Değersiz JSON yapısı
rtk deps                # Bağımlılık özeti
rtk env                 # Kompakt ortam değişkenleri
rtk summary <komut>     # Komut çıktısının akıllı özeti
rtk diff                # Ultra-kompakt diff'ler
```

### Altyapı (85% tasarruf)
```bash
rtk docker ps           # Kompakt kapsayıcı listesi
rtk docker images       # Kompakt imaj listesi
rtk docker logs <k>     # Tekrarsız loglar
rtk kubectl get         # Kompakt kaynak listesi
rtk kubectl logs        # Tekrarsız pod logları
```

### Ağ (65-70% tasarruf)
```bash
rtk curl <url>          # Kompakt HTTP yanıtları (70%)
rtk wget <url>          # Kompakt indirme çıktısı (65%)
```

### Meta Komutlar
```bash
rtk gain                # Token tasarruf istatistiklerini görüntüle
rtk gain --history      # Tasarruflarla komut geçmişini görüntüle
rtk discover            # RTK kullanımını kaçıran Claude Code oturumlarını analiz et
rtk proxy <komut>       # Komutu filtrelemeden çalıştır (hata ayıklama için)
rtk init                # RTK talimatlarını CLAUDE.md'ye ekle
rtk init --global       # RTK'yı ~/.claude/CLAUDE.md'ye ekle
```

## Token Tasarruf Genel Bakış

| Kategori | Komutlar | Tipik Tasarruf |
|----------|----------|-----------------|
| Testler | vitest, playwright, cargo test | 90-99% |
| Derleme | next, tsc, lint, prettier | 70-87% |
| Git | status, log, diff, add, commit | 59-80% |
| GitHub | gh pr, gh run, gh issue | 26-87% |
| Paket Yöneticileri | pnpm, npm, npx | 70-90% |
| Dosyalar | ls, read, grep, find | 60-75% |
| Altyapı | docker, kubectl | 85% |
| Ağ | curl, wget | 65-70% |

Genel ortalama: sık geliştirme işlemlerinde **%60-90 token azaltımı**.
<!-- /rtk-instructions -->