/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "Security.h"

#include <cmath>   // json.hpp'nin std::isfinite gereksinimi
#include <math.h>
#undef isfinite    // MSVC math.h makrosu std::isfinite'i kırar (Security.cpp'de sorun yok; burada main.h getiriyor)
#include <stdio.h>
#undef snprintf    // MSVC stdio.h makrosu std::snprintf'i kırar
#undef vsnprintf
#include "httplib.h"
#include "json.hpp"
using json = nlohmann::json;

int iNetModeNormalOnfootSendRate, iNetModeNormalIncarSendRate, iNetModeFiringSendRate, iNetModeSendMultiplier;

char g_szHostName[256];
BYTE m_bLagCompensation;

PLAYERID imitateID = -1;
bool iGettingNewName=false;

int iMoney, iDrunkLevel, iLocalPlayerSkin;

struct stGTAMenu GTAMenu;

struct stSAMPDialog sampDialog;
HFONT hSAMPDlgFont = NULL;
HANDLE hDlgThread = NULL;
HWND hwndSAMPDlg = NULL;

PLAYER_SPAWN_INFO SpawnInfo;

BOOL bIsSpectating = 0;

// Grup komutu cooldown'lari ve durum (GetTickCount ms)
static DWORD g_lastWarHelpTime = 0;
static DWORD g_lastPLTime = 0;
static DWORD g_lastGStatsTime = 0;
static DWORD g_lastBackupTime = 0;
static DWORD g_lastZoneWarTime = 0;
static DWORD g_lastPingTime = 0;
static bool g_zwLearningGroup = false;
static DWORD g_zwLearnTick = 0;
static DWORD g_lastGztTime = 0;
static int g_knownGroupMembers[MAX_PLAYERS];

// Zone war süresi: ilk savaştan ölçülür (join -> wins/loses arası). Varsayılan 10 dk,
// ?gzt ölçülmüş süreyi kullanir (chat'e düşen "Type /gz2 to join" bildirimi savaşın başlangıç anıdır).
#define ZONE_WAR_TOTAL_MS 600000
static DWORD g_zwWarDurationMs = ZONE_WAR_TOTAL_MS; // son ölçülen savaş süresi
#define KILL_LIST_CAPTURE_WINDOW_MS 15000 // sonuç textdraw'ları için toplama penceresi
static DWORD g_zoneWarStartTick = 0;

// Kill list yakalayıcı durumu (GZ sonucu textdraw'ları -> dosya + Discord)
static bool g_zwCapturingKillList = false;  // textdraw toplama aktif mi
static bool g_zwKillListSent = false;       // sonuç zaten gönderildi mi
static DWORD g_zwCaptureStart = 0;          // toplama başlangıcı
static std::string g_zwKillListText;        // birikmiş sonuç metni
static char g_zwWarZone[16] = {0};          // aktif savaşın zone no'su ("146"); 60 sn'lik tekrar duyurusu bununla elenir
static bool g_zwWelcomed[MAX_PLAYERS];      // bu savaşta oyuncu başına bir kez welcome (savaş başında sıfırlanır)
// Esir-alma ile grup adi ogrenimi: "ZONE WAR: X takes over zone '# N'" -> zone N'nin
// rengi (RPC 108 ile) X'in rengine guncellenir; o guncellemede renk<->X esle (pending).
static int g_zwPendingZone = -1;            // hangi zone'un sahibini bekliyoruz
static char g_zwPendingName[64] = {0};      // o zone'un yeni sahibi (grup adi)
static DWORD g_zwPendingTick = 0;           // pending zaman asimi (30 sn)

// Savaş zamanlayıcı (UIF textdraw ID 60): "~r~~h~4~w~-~b~~h~3 ~n~~w~08:27" -> skor + kalan süre.
// Bot GZ'ye GIRMEDEN yakalanabildigi anda skor kullanilir; oto-giris YOK (desync riski).
static DWORD g_warTimerTick = 0;            // son yakalama zamanı
static int  g_warRemainingSec = -1;         // yakalanan kalan süre (sn)
// GZ'ye giris kaldirildi: bot savasa katilmaz (desync/kick riski).
#define MAX_PLAYER_NAME_CACHE

// Gang zone yakalayıcı: UIF zone numaraları haritanın üst solundan (Tierra Robada)
// başlayarak #0'dan itibaren sıralı oluşturulur -> gang zone ID == zone # kabul edilir.
// RPC_ScrAddGangZone (108) geldikçe koordinatlar buraya kaydedilir (?zgo için).
#define MAX_GANG_ZONES 2048
static float g_zoneMinX[MAX_GANG_ZONES];
static float g_zoneMinY[MAX_GANG_ZONES];
static float g_zoneMaxX[MAX_GANG_ZONES];
static float g_zoneMaxY[MAX_GANG_ZONES];
static unsigned int g_zoneColor[MAX_GANG_ZONES];
static bool g_zoneFlash[MAX_GANG_ZONES];
static int g_zoneCount = 0;

// ?zgo: bot zone'a isinlanip 40 sn bekler, sonra eski pozisyonuna doner.
#define ZGO_STAY_MS 40000
#define ZGO_TELEPORT_Z 25.0f  // yer seviyesine yakin: cok yuksek Z'de sunucu "zonede degilsin" der
static bool g_zgoActive = false;            // isinlama + bekleme aktif mi
static DWORD g_zgoReturnTick = 0;           // geri donus zamani
static float g_zgoReturnPos[3] = {0, 0, 0}; // donulecek (spawn) pozisyon
static int g_zgoZoneId = -1;                // hedef zone (geri donus bildirimi icin)

// ?zf: /gzinfo asenkron oldugu icin flash paketleri (ScrFlashGangZone -> g_zoneFlash)
// birkaç yuz ms sonra gelir. Aninda BuildFlashZoneSummary cagirmak BOŞ/ESKI veri okur.
// 3 sn sonra ana donguden cagrilir (ProcessZfFlashSummary).
static DWORD g_zfFlashAt = 0;               // 0: bekleyen istek yok
static bool  g_zfFlashPendingFlag = false;  // BuildFlashZoneSummary bir kez calistirilir

void ProcessZfFlashSummary()
{
	if(!g_zfFlashPendingFlag) return;
	if(GetTickCount() < g_zfFlashAt) return;
	g_zfFlashPendingFlag = false;
	g_zfFlashAt = 0;
	Log("[ZF] /gzinfo sonrasi 3 sn gecti, flashing zone dagilimi gonderiliyor.");
	BuildFlashZoneSummary();
}

// ?zf: flashing zone'lari grup tag'larina gore sayip gruba bildirir (YS bot gibi).

// Oto renk taramasi: her renkten bir ornek zone'a isinlanip /gzinfo'dan grup adini
// ogrenir (zgo mantigi). Sonuc group_colors.txt'ye yazilir; ?zf etiketleri ve
// "serbest zone" tespiti bu haritadan gelir. YS bot mesajlari gelmese de calisir.
#define ZONE_SCAN_PER_SAMPLE_MS 7000
#define ZONE_SCAN_PERIOD_MS      900000 // oto tarama periyodu: yeni grup adlari icin 15 dk'da bir
static int   g_zoneScanQueue[MAX_GANG_ZONES] = {0};
static int   g_zoneScanQ = 0;          // kuyruktaki ornek zone sayisi
static int   g_zoneScanIdx = -1;       // -1: tarama yok; 0..: siradaki ornek
static DWORD g_zoneScanDeadline = 0;   // ornek icin zaman asimi
static float g_zoneScanReturnPos[3] = {0, 0, 0};
static bool  g_zoneScanDone = false;   // oturumda bir kez
static DWORD g_zoneScanWaitStart = 0;  // oto-baslatma sayaci (spawn'dan 30 sn sonra)
static DWORD g_zoneScanLastRun = 0;    // son oto tarama zamani (periyod kontrolu)
static bool  g_zoneSweepDone = false;  // stale renk temizligi oturumda bir kez

// Freeroam'a gecis: /fr'yi oturum basina EN FAZLA bir kez gonder (cmd mute riski yok).
// Birincil tetik: spawn menusu "Freeroam" cevabi (ScrDialogBox). Yedek tetik: spawn'dan
// 10 sn sonra tek seferlik zaman tabanli gonderim (menu hic gelmezse / RPC_Spawn gelmezse).
// SpawnConfirmed (RPC_Spawn) de /fr gonderebilir; hepsi g_frSentOnce ile korunur.
static DWORD g_dwFrFallbackStart = 0;

void StartFrRetry()
{
	// Gerek yok: tetikler olay tabanli. (Yedek tetik ProcessFrRetry'de.)
}

void ProcessFrRetry()
{
	if(g_frSentOnce) return;
	if(!iSpawned) { g_dwFrFallbackStart = 0; return; } // yeniden spawn'da timer'i kur
	if(g_dwFrFallbackStart == 0)
		g_dwFrFallbackStart = GetTickCount();
	else if(GetTickCount() - g_dwFrFallbackStart >= 10000)
	{
		g_frSentOnce = true;
		Log("[SPAWN] Spawn menusu gelmedi/zaman asimi, tek seferlik /fr gonderimi.");
		sendServerCommand((char *)"/fr");
	}
}

// Spawn menusu ("Where would you like to go?") son otomatik secilme zamani.
static DWORD g_dwSpawnMenuHandled = 0;

// -------------------------------------------------------------
// ?zf GRUP TAG HARITASI (renk -> grup tag'i)
// YS botun "Flashing Zones: Tag(n) | ..." mesajlarindan tag'ler toplanir;
// her tag icin tek tek /gstats sorgulanir, sonuctaki zone listesinin renklerinden
// grup rengi ogrenilir ve group_colors.txt'ye kalici kaydedilir. ?zf bu haritayi
// kullanarak flashing zone'lari grup taglarina gore gruplar ve sayar.
// -------------------------------------------------------------
#define MAX_GROUP_COLORS 128
#define MAX_SEEN_TAGS 256
static struct { unsigned int uiColor; char szTag[64]; char szName[64]; } g_groupColors[MAX_GROUP_COLORS];
static int g_groupColorCount = 0;
static char g_seenTags[MAX_SEEN_TAGS][64];
static int g_seenTagCount = 0;
static int g_harvestIdx = 0;
static DWORD g_lastHarvestTime = 0;
static bool g_harvestInFlight = false;   // /gstats sorgusu sonucu bekleniyor
static char g_harvestTag[64] = {0};      // sorgulanan tag
static const char *g_szColorsFile = "group_colors.txt";
static const char *g_szLangFile    = "bot_lang.txt";
static char g_ysFlashMsg[192] = {0};   // son YS "Flashing Zones: Ad(n) | ..." mesaji ("!" on-eki yok)
static DWORD g_ysFlashMsgTick = 0;     // 0: henuz gelmedi; 40 sn gecerlilik
static void SyncPlayerCacheToDb();
static void SyncGroupColorsToDb();

static int FindGroupColorByTag(const char *szTag)
{
	if(!szTag) return -1;
	for(int i = 0; i < g_groupColorCount; i++)
		if(!_stricmp(g_groupColors[i].szTag, szTag)) return i;
	return -1;
}

static int FindGroupColorByColor(unsigned int uiColor)
{
	for(int i = 0; i < g_groupColorCount; i++)
		if(g_groupColors[i].uiColor == uiColor) return i;
	return -1;
}

// ?zgo <gruptag>: grubun rengini bulup o renkteki 1 boş (flash etmeyen) zone'u doner.
// Grubun hic zonu yoksa ya da hepsi savastaysa -1. "Free" -> sahipsiz (renk 0) ilk zon.
static int FindZoneByGroupTag(const char *szTag)
{
	if(!szTag || !szTag[0]) return -1;
	if(!_stricmp(szTag, "Free"))
	{
		for(int zid = 0; zid < g_zoneCount; zid++)
			if(g_zoneColor[zid] == 0) return zid;
		return -1;
	}
	int gi = FindGroupColorByTag(szTag);
	if(gi < 0) return -1;
	unsigned int uiC = g_groupColors[gi].uiColor;
	for(int zid = 0; zid < g_zoneCount; zid++)
		if(g_zoneColor[zid] == uiC)
			return zid;
	return -1;
}

// {RRGGBB} renk kodlarini adlardan temizle (UIF grup adlarinda tasiyabiliyor).
static void StripColorCodes(char *sz)
{
	if(!sz) return;
	char *pW = sz;
	for(const char *pR = sz; *pR; )
	{
		if(*pR == '{')
		{
			const char *pE = strchr(pR, '}');
			if(pE && pE - pR == 7) { pR = pE + 1; continue; } // {RRGGBB} atla
		}
		*pW++ = *pR++;
	}
	*pW = 0;
}

static void LoadGroupColors()
{
	g_groupColorCount = 0;
	FILE *f = fopen(g_szColorsFile, "r");
	if(f)
	{
		char szLine[200];
		while(fgets(szLine, sizeof(szLine), f) && g_groupColorCount < MAX_GROUP_COLORS)
		{
			// Format: "RRGGBBAA kisa_tag|Tam Grup Adi" (isim opsiyonel, "|" ile ayrilir)
			unsigned int uiC = 0; char szTag[96] = {0}; char szName[96] = {0};
			char szRest[200] = {0};
			if(sscanf(szLine, "%x %95[^\r\n]", &uiC, szRest) == 2 && uiC != 0 && szRest[0])
			{
				char *pSep = strchr(szRest, '|');
				if(pSep)
				{
					int nT = (int)(pSep - szRest);
					if(nT > 95) nT = 95;
					memcpy(szTag, szRest, nT); szTag[nT] = 0;
					strncpy_s(szName, sizeof(szName), pSep + 1, sizeof(szName) - 1);
				}
				else
					strncpy_s(szTag, sizeof(szTag), szRest, sizeof(szTag) - 1);
				StripColorCodes(szTag);
				StripColorCodes(szName);
				g_groupColors[g_groupColorCount].uiColor = uiC;
				strncpy_s(g_groupColors[g_groupColorCount].szTag, sizeof(g_groupColors[0].szTag),
					szTag, sizeof(g_groupColors[0].szTag) - 1);
				strncpy_s(g_groupColors[g_groupColorCount].szName, sizeof(g_groupColors[0].szName),
					szName, sizeof(g_groupColors[0].szName) - 1);
				g_groupColorCount++;
			}
		}
		fclose(f);
	}
	Log("[ZF] %d grup rengi dosyadan yuklendi.", g_groupColorCount);
}

static void SaveGroupColors()
{
	FILE *f = fopen(g_szColorsFile, "w");
	if(f)
	{
		for(int i = 0; i < g_groupColorCount; i++)
			fprintf(f, "%08X %s%s%s\n", g_groupColors[i].uiColor, g_groupColors[i].szTag,
				g_groupColors[i].szName[0] ? "|" : "", g_groupColors[i].szName);
		fclose(f);
	}
	Log("[ZF] %d grup rengi dosyaya kaydedildi.", g_groupColorCount);
}

// --- DB PUSH (MySQL backend) ---
// player_cache.txt / group_colors.txt'nin kopyasini backend'e de yazar (?pl ve zone
// etiketleri DB'de de dursun). Dosya tek dogru kaynak olmaya devam eder; DB kopyadir.
// Push, main loop'taki ProcessDbSync (60 sn'de bir) tarafindan tetiklenir; buradaki
// iki cagri periyodik akistan, Save*tarafindan degil.
static const char *g_szBackendBase = "http://127.0.0.1:3000";

static void SyncPlayerCacheToDb()
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;

	json arr = json::array();
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(g_playerCache[i].dwTime == 0) continue;
		arr.push_back({
			{"id", i},
			{"name", g_playerCache[i].szName[0] ? g_playerCache[i].szName : "?"},
			{"packetloss", g_playerCache[i].dLoss},
			{"fps", g_playerCache[i].iFps},
			{"ping", g_playerCache[i].iPing}
		});
	}
	if(arr.empty()) return;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {{"players", arr}};
		auto res = cli.Post("/api/player/cache", body.dump(), "application/json");
		if(res && res->status == 200) Log("[PL-CACHE] %d oyuncu DB'ye push edildi.", (int)arr.size());
	}
	catch(...) {}
}

static void SyncGroupColorsToDb()
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;

	json arr = json::array();
	for(int i = 0; i < g_groupColorCount; i++)
	{
		arr.push_back({
			{"color", (unsigned int)g_groupColors[i].uiColor},
			{"tag", g_groupColors[i].szTag}
		});
	}
	if(arr.empty()) return;
	try
	{
		httplib::Client cli(g_szBackendBase);
		cli.set_connection_timeout(3);
		json body = {{"colors", arr}};
		auto res = cli.Post("/api/group/colors", body.dump(), "application/json");
		if(res && res->status == 200) Log("[ZF] %d renk DB'ye push edildi.", (int)arr.size());
	}
	catch(...) {}
}

// Periyodik (main loop, 60 sn'de bir): cache + renkleri DB'ye yazar.
static DWORD g_lastDbSyncTick = 0;
void ProcessDbSync()
{
	if(!g_isEngineActive || g_sessionToken.empty()) return;
	DWORD dwNow = GetTickCount();
	if(dwNow - g_lastDbSyncTick < 60000) return;
	g_lastDbSyncTick = dwNow;
	SyncPlayerCacheToDb();
	SyncGroupColorsToDb();
}

// Grup dili: ?tr / ?en ile degistirilir; varsayilan INGILIZCE. Tek dogru kaynak
// MongoDB'dir (Guild.lang); Security.cpp polling ile senkronlar. bot_lang.txt
// sadece boot anindaki ilk deger (poll gelene kadar) icindir.
static const char *LM(const char *tr, const char *en) { return g_langTurkish ? tr : en; }

static void LoadLang()
{
	FILE *f = fopen(g_szLangFile, "r");
	if(f)
	{
		char szBuf[16] = {0};
		if(fgets(szBuf, sizeof(szBuf), f) && szBuf[0] == 't')
			g_langTurkish = true;
		fclose(f);
	}
	Log("[LANG] Grup dili: %s", g_langTurkish ? "Turkce" : "English");
}

void SaveLang()
{
	FILE *f = fopen(g_szLangFile, "w");
	if(f) { fputs(g_langTurkish ? "tr\n" : "en\n", f); fclose(f); }
	Log("[LANG] Grup dili kaydedildi: %s", g_langTurkish ? "tr" : "en");
}

// Bir zone'un rengini verilen grup adiyla esle ve kalici kaydet. YS botu ya da
// /gzinfo dialoguna bagimli DEGILDIR: esir-alma mesajlarindan gelen (isim, zone)
// cifti + o zone'un RPC 108 renk guncellemesiyle calisir. Guncelenen sahiplik en
// guncel dogrudur -> onceki eslesme ezilir.
static void BindZoneGroup(int iZone, const char *szName)
{
	if(!szName || !szName[0] || iZone < 0 || iZone >= MAX_GANG_ZONES) return;
	unsigned int c = g_zoneColor[iZone];
	if(c == 0 || g_groupColorCount >= MAX_GROUP_COLORS) return; // renk henuz bilinmiyorsa atla

	char szBuf[80];
	strncpy_s(szBuf, sizeof(szBuf), szName, _TRUNCATE);
	StripColorCodes(szBuf);
	while(szBuf[0] && szBuf[strlen(szBuf)-1] == ' ') szBuf[strlen(szBuf)-1] = 0;
	if(!szBuf[0]) return;

	int idx = FindGroupColorByColor(c);
	if(idx >= 0)
	{
		if(strcmp(g_groupColors[idx].szName, szBuf) != 0)
		{
			Log("[ZFU:LEARN] Renk 0x%08X '%s' -> '%s' (guncel sahip).", c, g_groupColors[idx].szName, szBuf);
			strncpy_s(g_groupColors[idx].szName, sizeof(g_groupColors[0].szName),
				szBuf, sizeof(g_groupColors[0].szName) - 1);
			SaveGroupColors();
		}
	}
	else
	{
		g_groupColors[g_groupColorCount].uiColor = c;
		strncpy_s(g_groupColors[g_groupColorCount].szName, sizeof(g_groupColors[0].szName),
			szBuf, sizeof(g_groupColors[0].szName) - 1);
		g_groupColorCount++;
		SaveGroupColors();
		Log("[ZFU:LEARN] '%s' rengi esir-alma mesajindan ogrenildi: 0x%08X (zone #%d)", szBuf, c, iZone);
	}
}

// -------------------------------------------------------------
// ?pl ISTATISTIK CACHE'I
// UIF'in /pnetstats cevabi FPS icermez (FPS: --); FPS dahil tam istatistik YS.Hold'un
// "Player [X] Stats -> Packetloss: y | FPS: z | Ping: w" grup mesajlarinda gelir. Bu cache
// ?pl cevabina FPS'i (ve eksik loss/ping'i) koyar; player_cache.txt ile restart'a dayanir.
// Isimler de buraya yazilir: grup basligindan ogrenilen ad, ?pl cevabinda "hakixld"
// yerine gercek kullanici adinin gorunmesini saglar.
// -------------------------------------------------------------
static const char *g_szPlayerCacheFile = "player_cache.txt";

void LoadPlayerCache()
{
	memset(g_playerCache, 0, sizeof(g_playerCache));
	int nLoaded = 0;
	FILE *f = fopen(g_szPlayerCacheFile, "r");
	if(f)
	{
		char szLine[256];
		while(fgets(szLine, sizeof(szLine), f))
		{
			int iId = -1; char szName[64] = {0}; double dLoss = -1.0; int iFps = -1, iPing = -1;
			if(sscanf(szLine, "%d %63s %lf %d %d", &iId, szName, &dLoss, &iFps, &iPing) == 5
				&& iId >= 0 && iId < MAX_PLAYERS)
			{
				g_playerCache[iId].dwTime = 1; // "biliniyor" isareti (0 = yok)
				strncpy_s(g_playerCache[iId].szName, sizeof(g_playerCache[0].szName), szName, _TRUNCATE);
				g_playerCache[iId].dLoss = dLoss;
				g_playerCache[iId].iFps = iFps;
				g_playerCache[iId].iPing = iPing;
				nLoaded++;
			}
		}
		fclose(f);
	}
	Log("[PL-CACHE] %d oyuncu istatistigi dosyadan yuklendi.", nLoaded);
}

static void SavePlayerCache()
{
	FILE *f = fopen(g_szPlayerCacheFile, "w");
	if(f)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(g_playerCache[i].dwTime == 0) continue;
			fprintf(f, "%d %s %.2f %d %d\n", i, g_playerCache[i].szName,
				g_playerCache[i].dLoss, g_playerCache[i].iFps, g_playerCache[i].iPing);
		}
		fclose(f);
	}
}

// "Anahtar: deger" deseninden ilk sayiyi ceker; sayi yoksa -1.0 (FPS: N/A gibi)
static double NumberAfter(const char *pText, const char *szKey)
{
	if(!pText || !szKey) return -1.0;
	const char *p = strstr(pText, szKey);
	if(!p) return -1.0;
	p += strlen(szKey);
	while(*p == ' ' || *p == '\t') p++;
	if(!((*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return -1.0;
	return atof(p);
}

static void AddSeenTag(const char *szTag)
{
	if(!szTag || !*szTag) return;
	for(int i = 0; i < g_seenTagCount; i++)
		if(!_stricmp(g_seenTags[i], szTag)) return;
	if(g_seenTagCount < MAX_SEEN_TAGS)
	{
		strncpy_s(g_seenTags[g_seenTagCount], sizeof(g_seenTags[0]), szTag, sizeof(g_seenTags[0]) - 1);
		g_seenTagCount++;
	}
}

static const char *TagForZone(int iZone)
{
	if(iZone < 0 || iZone >= MAX_GANG_ZONES) return NULL;
	unsigned int uiColor = g_zoneColor[iZone];
	if(uiColor == 0) return NULL;
	int idx = FindGroupColorByColor(uiColor);
	return idx >= 0 ? g_groupColors[idx].szTag : NULL;
}

// YS bot "Flashing Zones: ..." mesajindaki Tag(n) ciftlerini toparlar
static void AddFlashTags(const char *pMsg)
{
	const char *pStart = pMsg ? strstr(pMsg, "Flashing Zones:") : NULL;
	if(!pStart) return;
	const char *p = pStart + strlen("Flashing Zones:");
	while(p && *p)
	{
		while(*p && (*p == ' ' || *p == '|' || *p == '\t')) p++;
		if(!*p) break;
		char szTok[128]; int n = 0;
		while(*p && *p != '|' && n < 120)
		{
			if(*p == '{') { while(*p && *p != '}') p++; if(*p) p++; continue; }
			szTok[n++] = *p; p++;
		}
		szTok[n] = 0;
		while(n > 0 && (szTok[n-1] == ' ' || szTok[n-1] == '\t')) { szTok[--n] = 0; }
		char *paren = strchr(szTok, '(');
		if(paren) *paren = 0;
		// "Free" sahipsiz/nötr zone'lari temsil eder (grup degil) -> harita disi.
		if(strlen(szTok) > 1 && _stricmp(szTok, "Free") != 0)
			AddSeenTag(szTok);
	}
}

// YS botun "Flashing Zones: Ad(n) | ..." mesaji, anlik flashing grup adlarini ve
// zone sayilarini ("n") icerir. Bot ayni anda flashing zone'larin renklerini sayar;
// "Ad(n)" sirasindaki count, botun siradaki renginin count'una denk gelirse renk<->
// ad eslesmesi ANINDA ogrenilir (yavas /gstats harvest beklentisi olmadan). Guvenlik:
// count tekil degilse atlanir, cunku sira eslesmesi yanlis renk verebilir.
static void SyncFlashGroupColors(const char *pMsg)
{
	const char *pStart = pMsg ? strstr(pMsg, "Flashing Zones:") : NULL;
	if(!pStart) return;

	struct { unsigned int c; int n; } agg[256];
	int aggCount = 0;
	for(int zid = 0; zid < MAX_GANG_ZONES; zid++)
	{
		if(!g_zoneFlash[zid]) continue;
		unsigned int c = g_zoneColor[zid];
		int a = 0;
		for(; a < aggCount; a++) if(agg[a].c == c) break;
		if(a == aggCount && aggCount < 256) { agg[aggCount].c = c; agg[aggCount].n = 0; aggCount++; }
		if(a < aggCount) agg[a].n++;
	}
	if(aggCount == 0) return;
	for(int i = 0; i < aggCount; i++)
		for(int j = i + 1; j < aggCount; j++)
			if(agg[j].n > agg[i].n)
			{
				unsigned int tc = agg[i].c; int tn = agg[i].n;
				agg[i].c = agg[j].c; agg[i].n = agg[j].n;
				agg[j].c = tc; agg[j].n = tn;
			}

	const char *p = pStart + strlen("Flashing Zones:");
	int iAgg = 0;
	while(p && *p && iAgg < aggCount)
	{
		while(*p && (*p == ' ' || *p == '|' || *p == '\t')) p++;
		if(!*p) break;
		char szTok[128]; int n = 0;
		while(*p && *p != '|' && n < 120)
		{
			if(*p == '{') { while(*p && *p != '}') p++; if(*p) p++; continue; }
			szTok[n++] = *p; p++;
		}
		szTok[n] = 0;
		while(n > 0 && (szTok[n-1] == ' ' || szTok[n-1] == '\t')) { szTok[--n] = 0; }
		char *paren = strchr(szTok, '(');
		if(!paren) continue;
		int nCnt = atoi(paren + 1);
		*paren = 0;
		StripColorCodes(szTok);
		if(!szTok[0] || strstr(szTok, "Capturing") || !_stricmp(szTok, "Free")) continue;

		if(agg[iAgg].n == nCnt)
		{
			bool bUnique = true; // count tekil mi? tekilse esleme guvenli
			for(int k = 0; k < aggCount; k++)
				if(k != iAgg && agg[k].n == agg[iAgg].n) { bUnique = false; break; }
			if(bUnique && FindGroupColorByColor(agg[iAgg].c) < 0 && g_groupColorCount < MAX_GROUP_COLORS)
			{
				g_groupColors[g_groupColorCount].uiColor = agg[iAgg].c;
				strncpy_s(g_groupColors[g_groupColorCount].szTag, sizeof(g_groupColors[0].szTag),
					szTok, sizeof(g_groupColors[0].szTag) - 1);
				g_groupColorCount++;
				SaveGroupColors();
				Log("[ZF:SYNC] '%s' rengi YS flash mesajindan ogrenildi: 0x%08X (%d zone)", szTok, agg[iAgg].c, nCnt);
			}
			iAgg++;
		}
	}
}

// /gstats <tag> sorgusunun sonucu: zone listesini rengine cevirip grup rengini ogrenir
void HarvestParseGstatsResult(const char *szInfo)
{
	if(!szInfo || !g_harvestTag[0]) return;

	const char *pZone = strstr(szInfo, "Zones:");
	if(!pZone)
	{
		Log("[ZF:HARVEST] '%s' sonucunda 'Zones:' bolumu yok, sorgu kapatiliyor.", g_harvestTag);
		// Sonuc tek dialogda gelir; ikinci parca gelmez. Flag'i temizle, yoksa 25sn
		// zaman asimina kadar sorgu kilitlenir.
		g_harvestInFlight = false;
		g_gstatsHarvestMode = false;
		g_lastHarvestTime = GetTickCount();
		return;
	}

	// zone id + renk histogramini cikar
	struct { unsigned int c; int n; } hist[128];
	int histCount = 0, nZones = 0;
	const char *p = pZone;
	while((p = strstr(p, "# ")) != NULL)
	{
		int id = atoi(p + 1);
		if(id >= 0 && id < MAX_GANG_ZONES)
		{
			unsigned int c = g_zoneColor[id];
			if(c != 0)
			{
				int h = 0;
				for(; h < histCount; h++) if(hist[h].c == c) break;
				if(h == histCount && histCount < 128) { hist[histCount].c = c; hist[histCount].n = 0; histCount++; }
				if(h < histCount) hist[h].n++;
				nZones++;
			}
		}
		p += 2;
	}

	unsigned int uiBest = 0; int nBest = 0;
	for(int h = 0; h < histCount; h++)
		if(hist[h].n > nBest) { nBest = hist[h].n; uiBest = hist[h].c; }

	if(uiBest != 0 && FindGroupColorByColor(uiBest) < 0 && g_groupColorCount < MAX_GROUP_COLORS)
	{
		g_groupColors[g_groupColorCount].uiColor = uiBest;
		strncpy_s(g_groupColors[g_groupColorCount].szTag, sizeof(g_groupColors[0].szTag),
			g_harvestTag, sizeof(g_groupColors[0].szTag) - 1);
		g_groupColorCount++;
		SaveGroupColors();
		Log("[ZF:HARVEST] '%s' rengi ogrenildi: 0x%08X (%d zone)", g_harvestTag, uiBest, nZones);
	}
	else
	{
		Log("[ZF:HARVEST] '%s' icin renk cikarilamadi (zone rengi bulunamadi, %d zone islendi).",
			g_harvestTag, nZones);
	}

	g_harvestInFlight = false;
	g_gstatsHarvestMode = false;
}

// Sira ile bilinmeyen tag'leri /gstats ile sorgular (cmd mute korumasi: 90 sn aralik)
void CreateGroupHarvest()
{
	g_harvestIdx = 0;
	ProcessGroupHarvest();
}

void ProcessGroupHarvest()
{
	static bool bColorsLoaded = false;
	if(!bColorsLoaded)
	{
		bColorsLoaded = true;
		LoadGroupColors();
		LoadLang();
	}

	if(!iSpawned) return;
	if(g_inZoneWar) return;             // savas sirasinda sorgu yok
	if(g_groupColorCount >= MAX_GROUP_COLORS) return;

	DWORD dwNow = GetTickCount();

	// Devam eden sorgu zaman asimi -> flag temizle, sonraki pencereyi bekle
	if(g_harvestInFlight && dwNow - g_lastHarvestTime > 25000)
	{
		g_harvestInFlight = false;
		g_gstatsHarvestMode = false;
		Log("[ZF:HARVEST] Sorgu zaman asimi, flag temizlendi.");
	}
	if(g_harvestInFlight) return;
	if(g_lastHarvestTime != 0 && dwNow - g_lastHarvestTime < 90000)
		return;

	// Hic tag gormediysek (YS bot mesaji henuz gelmedi) sorgu baslatma
	if(g_seenTagCount == 0) return;

	// O siradaki henuz rengi bilinmeyen tag
	char szNext[64] = {0};
	for(int k = 0; k < g_seenTagCount && !szNext[0]; k++)
	{
		int idx = (g_harvestIdx + k) % g_seenTagCount;
		if(FindGroupColorByTag(g_seenTags[idx]) < 0)
		{
			strncpy_s(szNext, sizeof(szNext), g_seenTags[idx], sizeof(szNext) - 1);
			g_harvestIdx = idx + 1;
		}
	}
	if(!szNext[0])
	{
		Log("[ZF:HARVEST] Bilinmeyen tag kalmadi (%d tag haritalandi).", g_groupColorCount);
		g_lastHarvestTime = dwNow; // tamam, tekrar denemesin
		return;
	}

	g_harvestInFlight = true;
	g_lastHarvestTime = dwNow;
	strncpy_s(g_harvestTag, sizeof(g_harvestTag), szNext, sizeof(g_harvestTag) - 1);

	g_gstatsHarvestMode = true;
	g_gstatsAutoSubmit = true;
	g_gstatsInput = szNext;
	g_waitingForGStats = true;
	g_gstatsReqTime = GetTickCount();
	g_gstatsSentFirst = false;
	Log("[ZF:HARVEST] '%s' icin /gstats gonderiliyor (cmd mute icin 90 sn aralik).", szNext);
	sendServerCommand((char *)"/gstats");
}

// ?zf: flashing zone'lari grup tag'larina gore sayip gruba bildirir (YS bot gibi).
// Cikti TEK mesajdir (SA-MP 128 karakter siniri): en buyuk gruplar once, tasarsa
// kesilir. Rakamsal tag (123) veya bilinmeyen grup goruldugunde gercek isim icin
// harvest hemen tetiklenir — YS botu gibi yeni gruplari aninda cekmeye baslar.
static bool IsNumericTag(const char *t)
{
	if(!t || !t[0]) return false;
	for(const char *p = t; *p; p++)
		if(*p < '0' || *p > '9') return false;
	return true;
}

void BuildFlashZoneSummary()
{
	// YS bot son 40 sn icinde "Flashing Zones: Ad(n) | ..." attiysa isimler
	// dogrudan hazirdir; renk histogrami yerine onu yanitla ("?" yok).
	if(g_ysFlashMsg[0] && GetTickCount() - g_ysFlashMsgTick < 40000)
	{
		g_bridgeSuppressEcho++;
		Log("[ZF] YS dagilimi tekrarlandi: %s", g_ysFlashMsg);
		sendChat(g_ysFlashMsg);
		return;
	}

	struct { unsigned int c; int n; } agg[256];
	int aggCount = 0, total = 0;
	for(int zid = 0; zid < MAX_GANG_ZONES; zid++)
	{
		if(!g_zoneFlash[zid]) continue;
		unsigned int c = g_zoneColor[zid];
		int a = 0;
		for(; a < aggCount; a++) if(agg[a].c == c) break;
		if(a == aggCount && aggCount < 256) { agg[aggCount].c = c; agg[aggCount].n = 0; aggCount++; }
		if(a < aggCount) { agg[a].n++; total++; }
	}

	// count'a gore buyukten kucuge
	for(int i = 0; i < aggCount; i++)
		for(int j = i + 1; j < aggCount; j++)
			if(agg[j].n > agg[i].n)
			{
				unsigned int tc = agg[i].c; int tn = agg[i].n;
				agg[i].c = agg[j].c; agg[i].n = agg[j].n;
				agg[j].c = tc; agg[j].n = tn;
			}

	// Rakamsal ya da bos tag'li sahipli renk: gercek isim icin sorgu baslat (YS gibi aninda).
	for(int a = 0; a < aggCount; a++)
	{
		int idx = FindGroupColorByColor(agg[a].c);
		if(idx >= 0 && g_groupColors[idx].szTag[0]
			&& IsNumericTag(g_groupColors[idx].szTag))
			CreateGroupHarvest();
	}

	// Tek mesaj: "!Flashing Zones: GeMs(40) | The Youngsters Family(35) | Free(10)"
	char szOut[192]; int nOut = 0;
	nOut += sprintf_s(szOut + nOut, sizeof(szOut) - nOut, "!Flashing Zones: ");
	char szOne[128]; bool bFirst = true;
	for(int a = 0; a < aggCount; a++)
	{
		int idx = FindGroupColorByColor(agg[a].c);
		const char *pszLabel = "?";
		if(agg[a].c == 0 || (idx >= 0 && !g_groupColors[idx].szTag[0] && !g_groupColors[idx].szName[0]))
			pszLabel = "Free";
		else if(idx >= 0 && g_groupColors[idx].szTag[0])
			pszLabel = g_groupColors[idx].szTag;   // kisa etiket oncelikli
		else if(idx >= 0)
			pszLabel = g_groupColors[idx].szName;  // etiket yoksa tam ad
		int nL = sprintf_s(szOne, sizeof(szOne), "%s%s(%d)", bFirst ? "" : " | ", pszLabel, agg[a].n);
		if(nOut + nL >= (int)sizeof(szOut)) break; // 192 karakter siniri: kalanı atla
		bFirst = false;
		nOut += sprintf_s(szOut + nOut, sizeof(szOut) - nOut, "%s", szOne);
	}
	g_bridgeSuppressEcho++;
	Log("[ZF] Dagilim: %s", szOut);
	sendChat(szOut);
	Log("[ZF] Toplam %d flashing zone (gorunen bolge).", total);
}

// --- Oto renk taramasi: renk -> grup adi (gzinfo), zgo mantigiyla ---

static void GangZoneScanNext();
static void GangZoneScanGo(int iZone);

// /gzinfo dialogu geldi: "Zone Name: # N" -> zone id, "owned by group "X"" -> ad.
// Sahipsiz cikarsa renk "Free" (bos tag) olarak isaretlenir; tekrar kontrol edilmez.
static void GangZoneScanGzDialog(const char *szInfo)
{
	int iZone = -1;
	const char *pZone = strstr(szInfo, "Zone Name: #");
	if(pZone)
	{
		pZone += strlen("Zone Name: #");
		while(*pZone == ' ') pZone++;
		iZone = atoi(pZone);
	}

	// Ogrenme guard'i: dialog baska/eski bir zone icinse ogrenme (iZone -1 = parse hatasi
	// dahil). Stale dialog atlanir; siradaki ornege gecisi 7 sn'lik deadline yapar.
	if(g_zoneScanIdx >= 0 && iZone != g_zoneScanQueue[g_zoneScanIdx])
	{
		Log("[ZF:SCAN] Dialog zone #%d, beklenen ornek #%d degil; stale dialog atlaniyor.",
			iZone, g_zoneScanQueue[g_zoneScanIdx]);
		return;
	}

	unsigned int uiColor = 0;
	if(iZone >= 0 && iZone < MAX_GANG_ZONES) uiColor = g_zoneColor[iZone];

	char szOwner[80] = {0};
	const char *pOwn = strstr(szInfo, "owned by group");
	// Not: sahip adi renk bilinse de ayrismali — bilinen kisa etiket (GeMs gibi)
	// buradan tam isme (The Youngsters Family gibi) yukseltilir.
	if(pOwn)
	{
		const char *q = strchr(pOwn, '"');
		const char *r = q ? strchr(q + 1, '"') : NULL;
		if(q && r && r > q + 1)
		{
			int n = (int)(r - q - 1);
			if(n > 79) n = 79;
			memcpy(szOwner, q + 1, n);
			szOwner[n] = 0;
			StripColorCodes(szOwner);
		}
	}

	// Ogren: gercek sahip adi varsa (grup zone'lari savasla grup degistirebilir,
	// gelen dialog guncel dogrudur -> eski baglanti ezer). Sahipsiz ciktiysa ve
	// renk bilinmiyorsa "Free" (bos isim) olarak isaretle. Tam ad SADECE szName'e
	// yazilir; szTag (kisa etiket, ?zf/?zgo'da kullanilir) asla ezilmez.
	if(uiColor != 0 && g_groupColorCount < MAX_GROUP_COLORS)
	{
		int idx = FindGroupColorByColor(uiColor);
		if(idx >= 0 && szOwner[0] && strcmp(g_groupColors[idx].szName, szOwner) != 0)
		{
			Log("[ZF:SCAN] Renk 0x%08X '%s' -> '%s' (guncel).", uiColor, g_groupColors[idx].szName, szOwner);
			strncpy_s(g_groupColors[idx].szName, sizeof(g_groupColors[0].szName),
				szOwner, sizeof(g_groupColors[0].szName) - 1);
			SaveGroupColors();
		}
		else if(idx < 0)
		{
			g_groupColors[g_groupColorCount].uiColor = uiColor;
			strncpy_s(g_groupColors[g_groupColorCount].szName, sizeof(g_groupColors[0].szName),
				szOwner, sizeof(g_groupColors[0].szName) - 1); // bos isim = sahipsiz/free
			g_groupColorCount++;
			SaveGroupColors();
		}
	}
	Log("[ZF:SCAN] Ornek zone #%d (renk 0x%08X): %s", iZone, uiColor,
		szOwner[0] ? szOwner : "<sahipsiz/free>");

	GangZoneScanNext();
}

// Oturumdaki TUM zone renklerine karsi dogrula: hicbir zonda gorunmeyen grup rengi
// (grup ayrilmis ya da zoneleri bosalmis -> eski kayit) dosyadan silinir.
// Renk 0 = sahipsiz zone; grup rengi sifir olamaz, korumaya gerek yok.
static void SweepStaleGroupColors()
{
	if(g_zoneCount <= 0) return;
	int nRemoved = 0;
	for(int i = 0; i < g_groupColorCount; i++)
	{
		if(g_groupColors[i].uiColor == 0) continue;
		bool bFound = false;
		for(int zid = 0; zid < g_zoneCount && !bFound; zid++)
			if(g_zoneColor[zid] == g_groupColors[i].uiColor) bFound = true;
		if(bFound) continue;
		Log("[ZF:SWEEP] '%s' (0x%08X) hicbir zonda gorunmuyor, dosyadan siliniyor.",
			g_groupColors[i].szTag[0] ? g_groupColors[i].szTag : "<bos>", g_groupColors[i].uiColor);
		for(int j = i + 1; j < g_groupColorCount; j++)
			g_groupColors[j-1] = g_groupColors[j];
		g_groupColorCount--;
		nRemoved++;
		i--;
	}
	if(nRemoved > 0) SaveGroupColors();
	Log("[ZF:SWEEP] %d stale renk temizlendi (kalan: %d).", nRemoved, g_groupColorCount);
}

static void GangZoneScanNext()
{
	if(g_zoneScanIdx < 0) return;
	g_zoneScanIdx++;
	if(g_zoneScanIdx >= g_zoneScanQ)
	{
		g_zoneScanIdx = -1;
		g_zoneScanDone = true;
		Log("[ZF:SCAN] Tarama tamam (%d renk haritada).", g_groupColorCount);

		// Serbest (bilinen sahipsiz) zone id listesini logla
		char szFree[512]; int nF = 0;
		for(int zid = 0; zid < g_zoneCount; zid++)
		{
			unsigned int c = g_zoneColor[zid];
			if(c == 0) continue;
			int idx = FindGroupColorByColor(c);
			if(idx >= 0 && !g_groupColors[idx].szTag[0] && !g_groupColors[idx].szName[0])
				nF += sprintf_s(szFree + nF, sizeof(szFree) - nF, " #%d", zid);
		}
		Log("[ZF:SCAN] Serbest zone'lar:%s", nF > 0 ? szFree : " (yok)");

		char szCmd[128];
		sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f",
			g_zoneScanReturnPos[0], g_zoneScanReturnPos[1], g_zoneScanReturnPos[2]);
		sendServerCommand(szCmd);
		return;
	}
	GangZoneScanGo(g_zoneScanQueue[g_zoneScanIdx]);
}

static void GangZoneScanGo(int iZone)
{
	float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
	float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
	g_zoneScanDeadline = GetTickCount() + ZONE_SCAN_PER_SAMPLE_MS;
	Log("[ZF:SCAN] Ornek zone #%d taranıyor (renk 0x%08X) -> /gzinfo", iZone, g_zoneColor[iZone]);
	sendServerCommand(szCmd);
	sendServerCommand((char *)"/gzinfo");
}

void StartZoneScan(bool bAuto)
{
	if(g_zoneScanIdx >= 0 || !iSpawned || g_inZoneWar) return;

	// Her renkten bir ornek zone sec; renk 0 = sahipsiz, taramaya gerek yok.
	g_zoneScanQ = 0;
	for(int zid = 0; zid < g_zoneCount; zid++)
	{
		unsigned int c = g_zoneColor[zid];
		if(c == 0) continue;
		bool bDup = false;
		for(int q = 0; q < g_zoneScanQ; q++)
			if(g_zoneColor[g_zoneScanQueue[q]] == c) { bDup = true; break; }
		if(!bDup && g_zoneScanQ < MAX_GANG_ZONES)
			g_zoneScanQueue[g_zoneScanQ++] = zid;
	}
	if(g_zoneScanQ == 0) { Log("[ZF:SCAN] Tarama icin ornek zone yok."); return; }

	float fPos[3] = {0, 0, 0};
	if(!getPlayerPos(g_myPlayerID, fPos))
	{
		g_zoneScanWaitStart = 0; // oto-baslatma 30 sn sonra tekrar deneyecek
		Log("[ZF:SCAN] Pozisyon alinamadi, tarama baslatilmadi (oto-retry).");
		return;
	}
	g_zoneScanReturnPos[0] = fPos[0];
	g_zoneScanReturnPos[1] = fPos[1];
	g_zoneScanReturnPos[2] = fPos[2];

	g_zoneScanIdx = 0;
	Log("[ZF:SCAN] %d renk taraması basliyor (%s).", g_zoneScanQ, bAuto ? "oto" : "manuel ?zscan");
	GangZoneScanGo(g_zoneScanQueue[0]);
}

// Her ana dongu tick'inde: oto-tarama (spawn'dan 30 sn sonra, sonra her 15 dk'da bir) +
// stale renk temizligi. Zone war sirasinda tarama bekletilir (teleport yok).
void ProcessZoneScan()
{
	if(g_zoneScanIdx < 0)
	{
		// Stale temizlik: tum zone renkleri yuklendikten sonra oturumda bir kez.
		if(!g_zoneSweepDone && iSpawned && g_zoneCount > 0)
		{
			g_zoneSweepDone = true;
			SweepStaleGroupColors();
		}
		// Oto-tarama: spawn'dan 30 sn sonra ilk, sonra her periyotta tekrar.
		// Yeni grup adlarini /gzinfo'dan ogrenir (YS bot gibi); savas/zgo'da atlanir.
		DWORD dwNow = GetTickCount();
		if(iSpawned && !g_inZoneWar && !g_zgoActive
			&& g_zoneScanWaitStart != 0 && dwNow - g_zoneScanWaitStart >= 30000
			&& (g_zoneScanLastRun == 0 || dwNow - g_zoneScanLastRun >= ZONE_SCAN_PERIOD_MS))
		{
			StartZoneScan(true);
			if(g_zoneScanIdx >= 0) g_zoneScanLastRun = dwNow;
		}
		return;
	}
	if(g_inZoneWar || g_zgoActive) return; // savas/zgo surecinde tarama bekler
	if(GetTickCount() > g_zoneScanDeadline)
	{
		Log("[ZF:SCAN] Ornek zone #%d icin dialog gelmedi, atlaniyor.", g_zoneScanQueue[g_zoneScanIdx]);
		GangZoneScanNext();
	}
}

void ServerJoin(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	CHAR szPlayerName[256];
	PLAYERID playerId;
	BYTE byteNameLen=0;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	
	bsData.Read(playerId);
	int iUnk = 0;
	bsData.Read(iUnk);
	BYTE bIsNPC = 0;
	bsData.Read(bIsNPC);
	bsData.Read(byteNameLen);
	if(byteNameLen > 20) return;
	bsData.Read(szPlayerName,byteNameLen);
	szPlayerName[byteNameLen] = '\0';
	
	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsConnected = 1;
	playerInfo[playerId].byteIsNPC = bIsNPC;
	strcpy((char *)playerInfo[playerId].szPlayerName, szPlayerName);

	// Zone war aktifken GRUP UYESI bir oyuncu baglanirsa gruba gz2 bildirimi at.
	// (g_knownGroupMembers: kendi grubumuzun sohbetinde gorulen uyeler — botun grubu.
	//  Oyunun TUM oyuncularina degil, sadece kendi grubuna bildirilir.)
	if(g_inZoneWar && playerId != g_myPlayerID && g_knownGroupMembers[playerId] && !g_zwWelcomed[playerId])
	{
		g_zwWelcomed[playerId] = true;
		Log("[WELCOME] Grup uyesi (%d) %s baglandi, savas aktif (%s).", playerId, szPlayerName, g_enemyGroupName.c_str());
		static char szWarNotice[192];
		sprintf_s(szWarNotice, sizeof(szWarNotice),
			LM("!%s oyuna katildi. Zone war devam ediyor! /gz2 yazarak savasa katil.",
			   "!%s joined. Zone war is ongoing! Type /gz2 to join the fight."),
			szPlayerName);
		sendChat(szWarNotice);
	}

	//Log("***[JOIN] (%d) %s", playerId, szPlayerName);
}

void ServerQuit(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	PLAYERID playerId;
	BYTE byteReason;

	bsData.Read(playerId);
	bsData.Read(byteReason);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsConnected = 0;
	playerInfo[playerId].byteIsNPC = 0;
	//Log("***[QUIT:%d] (%d) %s", byteReason, playerId, playerInfo[playerId].szPlayerName);
	memset(playerInfo[playerId].szPlayerName, 0, 20);	
}

void InitGame(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsInitGame((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID MyPlayerID;
	bool bLanMode, bStuntBonus;
	BYTE byteVehicleModels[212];

	bool m_bZoneNames, m_bUseCJWalk, m_bAllowWeapons, m_bLimitGlobalChatRadius;
	float m_fGlobalChatRadius, m_fNameTagDrawDistance;
	bool m_bDisableEnterExits, m_bNameTagLOS, m_bManualVehicleEngineAndLight;
	bool m_bShowPlayerTags;
	int m_iShowPlayerMarkers;
	BYTE m_byteWorldTime, m_byteWeather;
	float m_fGravity;
	int m_iDeathDropMoney;
	bool m_bInstagib;

	bsInitGame.ReadCompressed(m_bZoneNames);
	bsInitGame.ReadCompressed(m_bUseCJWalk);
	bsInitGame.ReadCompressed(m_bAllowWeapons);
	bsInitGame.ReadCompressed(m_bLimitGlobalChatRadius);
	bsInitGame.Read(m_fGlobalChatRadius);
	bsInitGame.ReadCompressed(bStuntBonus);
	bsInitGame.Read(m_fNameTagDrawDistance);
	bsInitGame.ReadCompressed(m_bDisableEnterExits);
	bsInitGame.ReadCompressed(m_bNameTagLOS);
	bsInitGame.ReadCompressed(m_bManualVehicleEngineAndLight); // 
	bsInitGame.Read(iSpawnsAvailable);
	bsInitGame.Read(MyPlayerID);
	bsInitGame.ReadCompressed(m_bShowPlayerTags);
	bsInitGame.Read(m_iShowPlayerMarkers);
	bsInitGame.Read(m_byteWorldTime);
	bsInitGame.Read(m_byteWeather);
	bsInitGame.Read(m_fGravity);
	bsInitGame.ReadCompressed(bLanMode);
	bsInitGame.Read(m_iDeathDropMoney);
	bsInitGame.ReadCompressed(m_bInstagib);

	// Server's send rate restrictions
	if(!settings.uiForceCustomSendRates)
	{
		bsInitGame.Read(iNetModeNormalOnfootSendRate);
		bsInitGame.Read(iNetModeNormalIncarSendRate);
		bsInitGame.Read(iNetModeFiringSendRate);
		bsInitGame.Read(iNetModeSendMultiplier);
	}
	else
		bsInitGame.SetReadOffset(bsInitGame.GetReadOffset() + 4*32);

	bsInitGame.Read(m_bLagCompensation);

	BYTE unk;
	bsInitGame.Read(unk);
	bsInitGame.Read(unk);
	bsInitGame.Read(unk);

	BYTE byteStrLen;
	bsInitGame.Read(byteStrLen);
	if(byteStrLen)
	{
		memset(g_szHostName,0,sizeof(g_szHostName));
		bsInitGame.Read(g_szHostName, byteStrLen);
	}
	g_szHostName[byteStrLen] = '\0';

	bsInitGame.Read((char *)&byteVehicleModels[0],212);

	g_myPlayerID = MyPlayerID;

	char szTitle[64];
	if(settings.iConsole)
	{
		sprintf(szTitle, "%s (%d) - %.16s - RakSAMP %s", g_szNickName, g_myPlayerID, g_szHostName, RAKSAMP_VERSION);
		SetConsoleTitle(szTitle);
		Log("Connected to %.64s\n", g_szHostName);
	}
	else
	{
		sprintf(szTitle, "%s (%d) - RakSAMP %s", g_szNickName, g_myPlayerID, RAKSAMP_VERSION);
		SetWindowText(hwnd, szTitle);
		Log("Connected to %.64s", g_szHostName);
	}

	iGameInited = 1;
}

void WorldPlayerAdd(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	BYTE byteFightingStyle=4;
	BYTE byteTeam=0;
	int iSkin=0;
	float vecPos[3];
	float fRotation=0;
	DWORD dwColor=0;

	bsData.Read(playerId);
	bsData.Read(byteTeam);
	bsData.Read(iSkin);
	bsData.Read(vecPos[0]);
	bsData.Read(vecPos[1]);
	bsData.Read(vecPos[2]);
	bsData.Read(fRotation);
	bsData.Read(dwColor);
	bsData.Read(byteFightingStyle);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsStreamedIn = 1;
	playerInfo[playerId].onfootData.vecPos[0] = 
	playerInfo[playerId].incarData.vecPos[0] = vecPos[0];
	playerInfo[playerId].onfootData.vecPos[1] = 
	playerInfo[playerId].incarData.vecPos[1] = vecPos[1];
	playerInfo[playerId].onfootData.vecPos[2] =
	playerInfo[playerId].incarData.vecPos[2] = vecPos[2];

	//Log("[WORLD ADD] Player [%d]", playerId);
}

void WorldPlayerDeath(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	bsData.Read(playerId);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	//Log("[PLAYER_DEATH] %d", playerId);
}

void WorldPlayerRemove(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId=0;
	bsData.Read(playerId);

	if(playerId < 0 || playerId >= MAX_PLAYERS) return;

	playerInfo[playerId].iIsStreamedIn = 0;
	playerInfo[playerId].incarData.vecPos[0] = 0.0f;
	playerInfo[playerId].incarData.vecPos[1] = 0.0f;
	playerInfo[playerId].incarData.vecPos[2] = 0.0f;

	//Log("[PLAYER_REMOVE] %d", playerId);
}

void WorldVehicleAdd(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	NEW_VEHICLE NewVehicle;

	bsData.Read((char *)&NewVehicle,sizeof(NEW_VEHICLE));

	if(NewVehicle.VehicleId < 0 || NewVehicle.VehicleId >= MAX_VEHICLES) return;

	vehiclePool[NewVehicle.VehicleId].iDoesExist = 1;
	vehiclePool[NewVehicle.VehicleId].fPos[0] = NewVehicle.vecPos[0];
	vehiclePool[NewVehicle.VehicleId].fPos[1] = NewVehicle.vecPos[1];
	vehiclePool[NewVehicle.VehicleId].fPos[2] = NewVehicle.vecPos[2];
	vehiclePool[NewVehicle.VehicleId].iModelID = NewVehicle.iVehicleType;

	//Log("[VEHICLE_ADD:%d] ModelID: %d, Position: %0.2f, %0.2f, %0.2f",
	//	NewVehicle.VehicleId, NewVehicle.iVehicleType, NewVehicle.vecPos[0], NewVehicle.vecPos[1], NewVehicle.vecPos[2]);
}

void WorldVehicleRemove(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	VEHICLEID VehicleID;

	bsData.Read(VehicleID);

	if(VehicleID < 0 || VehicleID >= MAX_VEHICLES) return;

	vehiclePool[VehicleID].iDoesExist = 0;
	vehiclePool[VehicleID].fPos[0] = 0.0f;
	vehiclePool[VehicleID].fPos[1] = 0.0f;
	vehiclePool[VehicleID].fPos[2] = 0.0f;

	//Log("[VEHICLE_REMOVE] %d", VehicleID);
}

void ConnectionRejected(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	BYTE byteRejectReason;

	bsData.Read(byteRejectReason);

	if(byteRejectReason==REJECT_REASON_BAD_VERSION)
	{
		Log("[RAKSAMP] Bad SA-MP version.");
	}
	else if(byteRejectReason==REJECT_REASON_BAD_NICKNAME)
	{
		char szNewNick[32], randgen[4];

		iGettingNewName = true;

		gen_random(randgen, 4);
		sprintf(szNewNick, "%s_%s", g_szNickName, randgen);

		Log("[RAKSAMP] Bad nickname. Changing name to %s", szNewNick);

		strcpy(g_szNickName, szNewNick);
		resetPools(1, 0);
	}
	else if(byteRejectReason==REJECT_REASON_BAD_MOD)
	{
		Log("[RAKSAMP] Bad mod version.");
	}
	else if(byteRejectReason==REJECT_REASON_BAD_PLAYERID)
	{
		Log("[RAKSAMP] Bad player ID.");
	}
	else
		Log("ConnectionRejected: unknown");
}

void ClientMessage(RPCParameters *rpcParams)
{
	//if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	DWORD dwStrLen, dwColor;
	char szMsg[257];
	memset(szMsg, 0, 257);

	bsData.Read(dwColor);
	bsData.Read(dwStrLen);
	if(dwStrLen > 256) return;

	bsData.Read(szMsg, dwStrLen);
	szMsg[dwStrLen] = 0;

	if(settings.iFind)
	{
		for(int i = 0; i < MAX_FIND_ITEMS; i++)
		{
			if(!settings.findItems[i].iExists)
				continue;

			if(strstr(szMsg, settings.findItems[i].szFind))
				if(settings.findItems[i].szSay[0] != 0x00)
					sendChat(settings.findItems[i].szSay);
		}
	}

	char szNonColorEmbeddedMsg[257];
	int iNonColorEmbeddedMsgLen = 0;

	for (size_t pos = 0; pos < strlen(szMsg) && szMsg[pos] != '\0'; pos++)
	{
		if (!((*(unsigned char*)(&szMsg[pos]) - 32) >= 0 && (*(unsigned char*)(&szMsg[pos]) - 32) < 224))
			continue;

		if(pos+7 < strlen(szMsg))
		{
			if (szMsg[pos] == '{' && szMsg[pos+7] == '}')
			{
				pos += 7;
				continue;
			}
		}

		szNonColorEmbeddedMsg[iNonColorEmbeddedMsgLen] = szMsg[pos];
		iNonColorEmbeddedMsgLen++;
	}

	szNonColorEmbeddedMsg[iNonColorEmbeddedMsgLen] = 0;

	Log("[CMSG] %s", szNonColorEmbeddedMsg);

	// --- Zone War chat dedektoru (RakGravy desenleri) ---
	// "keeps zone" / "takes over" / "wins against" / "loses to" iceren mesajlar
	// Discord zonewar kanalina bildirilir. 60 sn cooldown, cift tetiklenmeyi onler.
	// Yalnizca botun kendi grubunun mesajlari yakalanir.
	// Kendi grup adi /gz2 sonrasi gelen ilk "ZONE WAR: X" mesajindan otomatik ogrenilir
	// (oyun RPC'sinden gelen isim guvenilmez; bot baskalari tarafindan da kullanilacak).
	{
		const char *pZoneMsg = szNonColorEmbeddedMsg;
		const char *pAction = NULL;
		const char *pTail = NULL;

		// "Type /gz2 to join" bildirimi: botun grubunun savasi basladi.
		// Bu mesaj kendi grubumuzun uyelerine gonderilir -> her zaman yakala.
		bool bJoinNotice = (strstr(pZoneMsg, "Type /gz2 to join") != NULL);
		if(bJoinNotice)
		{
			// Duyurunun zone'sunu cikar: "Zone: '# 146'." -> "146"
			char szZoneTag[16] = {0};
			const char *pZ = strstr(pZoneMsg, "Zone: '#");
			if(pZ)
			{
				pZ += strlen("Zone: '#");
				int nZ = 0;
				while(*pZ >= '0' && *pZ <= '9' && nZ < 15) szZoneTag[nZ++] = *pZ++;
				szZoneTag[nZ] = 0;
			}
			// Ayni savasin tekrar eden duyurusu: yoksay. (Yoksa g_zoneWarStartTick her
			// tekrarda sifirlanir ve ?gzt kalan sureyi yanlis hesaplar.) Savas zaten
			// aktifse gelen her yeni join duyurusu tekrar sayilir; zone tag'i uyuşmuyorsa
			// (bos tag dahil) da ayni savasin devami kabul edilir.
			if(g_inZoneWar && g_zoneWarStartTick != 0
				&& (!szZoneTag[0] || !strcmp(g_zwWarZone, szZoneTag)))
			{
				Log("[ZONEWAR] Savas duyurusu tekrari yoksayildi (zone #%s aktif).", g_zwWarZone);
			}
			else
			{
				pAction = "join";
				strncpy_s(g_zwWarZone, sizeof(g_zwWarZone), szZoneTag, sizeof(g_zwWarZone) - 1);
				// Savas baslangic anini kaydet: kalan sure = ZONE_WAR_TOTAL_MS - gecen sure.
				g_zoneWarStartTick = GetTickCount();
				g_inZoneWar = true;
				memset(g_zwWelcomed, 0, sizeof(g_zwWelcomed)); // yeni savas: welcome tekrari olmasin
				// Yeni savas: onceki yakalayici/kill list durumunu sifirla
				g_zwCapturingKillList = false;
				g_zwKillListSent = false;
				g_zwKillListText.clear();
				// Timer'i textdraw'dan yakalamak icin bot GZ'ye GIRILMEZ (desync/kick riski);
				// kalan sure bilgisi sadece log icindir, oto-giris yok.
				g_warRemainingSec = -1;
				g_warTimerTick = 0;
				Log("[ZONEWAR] Savas basladi (zone #%s), sure sayaci baslatildi.", g_zwWarZone);
				// Grubumuzun adini henuz bilmiyorsak ogrenme moduna gir.
				// Sonraki ilk "ZONE WAR: X keeps/takes..." mesajindaki X bizim grubumuzdur.
				if(g_myGroupName.empty() && !g_zwLearningGroup)
				{
					g_zwLearningGroup = true;
					g_zwLearnTick = GetTickCount();
					Log("[ZONEWAR] /gz2 bildirimi alindi, kendi grup adi ogrenilecek...");
				}
			}
		}

		// --- Esir-alma ile kisim grup adi ogrenimi (YS botu / gzinfo gerektirmez) ---
		// "ZONE WAR: X takes over zone '# N'"  -> X o zone'un YENI sahibi; zone rengi
		// RPC 108 ile X'in rengine guncellenir, o guncellemede esle (pending).
		// "ZONE WAR: X keeps zone '# N'"       -> X mevcut sahibi; renk simdi biliniyorsa direkt esle.
		{
			const char *pTakes = strstr(pZoneMsg, "takes over zone '#");
			const char *pKeeps = strstr(pZoneMsg, "keeps zone '#");
			const char *pCk = pTakes ? pTakes : pKeeps;
			if(pCk && strncmp(pZoneMsg, "ZONE WAR:", 9) == 0)
			{
				const char *pName = pZoneMsg + 9;
				while(*pName == ' ') pName++;
				int n = (int)(pCk - pName);
				if(n > 0 && n < 80)
				{
					char szName[80] = {0};
					memcpy(szName, pName, n);
					szName[n] = 0;
					StripColorCodes(szName);
					int zid = atoi(pCk + (pTakes ? strlen("takes over zone '#") : strlen("keeps zone '#")));
					if(zid >= 0 && zid < MAX_GANG_ZONES && szName[0])
					{
						if(pTakes)
						{
							g_zwPendingZone = zid;
							g_zwPendingTick = GetTickCount();
							strncpy_s(g_zwPendingName, sizeof(g_zwPendingName), szName, sizeof(g_zwPendingName) - 1);
							Log("[ZFU] '%s' zone #%d'yi aldi, renk guncellenmesi bekleniyor.", szName, zid);
						}
						else
						{
							BindZoneGroup(zid, szName);
						}
					}
				}
			}
		}

		if((pTail = strstr(pZoneMsg, "keeps zone")) != NULL) { pAction = "keeps"; }
		else if((pTail = strstr(pZoneMsg, "takes over")) != NULL) { pAction = "takes"; }
		else if((pTail = strstr(pZoneMsg, "wins against")) != NULL) { pAction = "wins"; }
		else if((pTail = strstr(pZoneMsg, "loses to")) != NULL) { pAction = "loses"; }

		// Ogrenme modundaysak ve "ZONE WAR: <Grup> ..." mesaji geldiyse, grup adini cek.
		// /gz2'den sonra gelen ilk zone mesaji bizim grubumuzun mesajidir.
		if(pAction && g_zwLearningGroup && (strncmp(pZoneMsg, "ZONE WAR:", 9) == 0))
		{
			if(GetTickCount() - g_zwLearnTick <= 60000)
			{
				const char *pName = pZoneMsg + 9;
				while(*pName == ' ') pName++;

				// Grup adi, action kelimesinden ("keeps zone", "takes over", ...)
				// ONCEKI kisimdir. Ornek: "ZONE WAR: swan of gours takes over zone '# 222'"
				// -> "swan of gours". pTail bu keyword'un basina isaret eder.
				std::string strLearned;
				if(pTail && pTail > pName)
					strLearned.assign(pName, (size_t)(pTail - pName));

				// Sondaki bosluklari temizle
				while(!strLearned.empty() && strLearned[strLearned.size()-1] == ' ')
					strLearned.erase(strLearned.size()-1);

				if(!strLearned.empty())
				{
					g_myGroupName = strLearned;
					g_zwLearningGroup = false;
					Log("[ZONEWAR] Kendi grup adi otomatik ogrenildi: %s", g_myGroupName.c_str());
				}
			}
			else
			{
				g_zwLearningGroup = false;
			}
		}

		// Kendi grubumuzun adini biliyorsak, mesajda o ad gecmiyorsa yoksay.
		// Grup adi henuz ogrenilmediyse (bos) hicbir zone mesaji yakalanmaz;
		// sadece /gz2 join bildirimi yakalanir (kendi grubumuzun uyelerine ozel).
		// /gz2 bildirimi kendi grubumuzun uyelerine ozel gonderildigi icin filtre uygulanmaz.
		// wins/loses da filtreden MUAFlardir: savas bitisi grup adindan bagimsiz
		// islenmeli (kill-list yakalayici + sure olcumu + ?gzt reset).
		if(pAction && !bJoinNotice
			&& strcmp(pAction, "wins") != 0 && strcmp(pAction, "loses") != 0)
		{
			if(g_myGroupName.empty() || strstr(pZoneMsg, g_myGroupName.c_str()) == NULL)
				pAction = NULL;
		}

		if(pAction)
		{
			DWORD dwNow = GetTickCount();
			if(g_lastZoneWarTime == 0 || (dwNow - g_lastZoneWarTime) >= 60000)
			{
				g_lastZoneWarTime = dwNow;

				// Savas bitti (wins/loses): sure sayacini sifirla ve gercek suresi olc.
				if(!strcmp(pAction, "wins") || !strcmp(pAction, "loses"))
				{
					if(g_zoneWarStartTick != 0)
					{
						g_zwWarDurationMs = dwNow - g_zoneWarStartTick;
						if(g_zwWarDurationMs < 60000) g_zwWarDurationMs = 60000; // mantikli alt sinir
						if(g_zwWarDurationMs > ZONE_WAR_TOTAL_MS) g_zwWarDurationMs = ZONE_WAR_TOTAL_MS;
						Log("[GZT] Savas suresi olculdu: %d sn", (int)(g_zwWarDurationMs / 1000));
					}
					g_zoneWarStartTick = 0;
					g_inZoneWar = false;
g_zwWarZone[0] = 0; // sonraki savas yeni duyuru sayilir
					g_warRemainingSec = -1;
					g_warTimerTick = 0;

					// Sonuc textdraw'lari gelmek uzere: henuz yakalayici acilmadiysa ac.
					if(!g_zwCapturingKillList && !g_zwKillListSent)
					{
						g_zwCapturingKillList = true;
						g_zwCaptureStart = GetTickCount();
						g_zwKillListText.clear();
						Log("[KILLLIST] Savas bitti (%s), sonuc textdraw yakalayici acildi.", pAction);
					}
					Log("[ZONEWAR] Savas bitti (%s), sure sayaci sifirlandi.", pAction);
				}

				// Zone adini ham mesajdan temiz cikar (embed'de duzgun gorunsun):
				// "ZONE WAR: X keeps zone '# 113'" -> "'# 113'"
				// "ZONE WAR: X takes over zone '# 108'" -> "'# 108'"
				// wins/loses/join icin zone adi yok (bos).
				const char *pZoneName = NULL;
				if(pTail)
				{
					const char *pZone = strstr(pTail, "zone");
					if(pZone)
					{
						pZone += 4;
						while(*pZone == ' ') pZone++;
						pZoneName = pZone;
					}
				}

				Log("[ZONEWAR] Chat mesaji yakalandi (%s): %s", pAction, pZoneMsg);
				SendZoneWarEvent(pAction, pZoneName ? pZoneName : "");
			}
		}
	}

	// --- Welcome back dedektoru: "GROUP: X logged in" ---
	// Oyun ici gruplarda "X logged in" mesaji gelince Discord welcome kanalina bildir.
	{
		const char *pLogged = strstr(szNonColorEmbeddedMsg, "logged in");
		if(pLogged && (pLogged == szNonColorEmbeddedMsg || pLogged[-1] == ' '))
		{
			// Isim cikar: "(GROUP) [12] isim(id): X logged in" -> "X"
			const char *pColon = NULL;
			const char *pOpenParen = NULL;
			for(const char *q = szNonColorEmbeddedMsg; *q; q++)
			{
				if(*q == ':') pColon = q;
				if(*q == '(') pOpenParen = q;
			}
			if(pColon && pOpenParen && pColon > pOpenParen)
			{
				const char *pName = pColon + 1;
				while(*pName == ' ') pName++;
				char szName[64] = {0};
				size_t n = 0;
				while(pName[n] && pName[n] != ' ' && n < 63) { szName[n] = pName[n]; n++; }
				szName[n] = 0;

				if(szName[0] && strcmp(szName, g_szNickName) != 0)
				{
					Log("[WELCOME] Welcome back tespit edildi: %s", szName);
					SendWelcomeBack(szName);
				}
			}
		}
	}

	// Grup chat tespiti: "(GROUP) [20] isim(oyuncuID): mesaj"
	// KENDI oyuncunun ?w / ?pl / ?gstats komutlarini yakalar (cooldown ile).
	// Normal chat (RakSAMP konsol inputu) buraya girmez, tetiklemez.
	if(!strncmp(szNonColorEmbeddedMsg, "(GROUP)", 7))
	{
		const char *pCloseBracket = strchr(szNonColorEmbeddedMsg, ']'); // "[20]" kapanisi
		const char *pOpenParen = pCloseBracket ? strchr(pCloseBracket, '(') : NULL; // oyuncu id parantezi
		if(pOpenParen)
		{
			int iSenderId = atoi(pOpenParen + 1);
			const char *pColon = strchr(pOpenParen, ':');
			const char *pGroupMsg = pColon ? pColon + 1 : "";
			while(*pGroupMsg == ' ') pGroupMsg++;

			// "(GROUP) [20] isim(oyuncuID): mesaj" basligindan gonderici adini ogren.
			// playerInfo[yeniKatilan].szPlayerName sadece ServerJoin ile dolar; botun
			// baglanmasindan ONCE oyunda olanlar bos kalir. Bu yuzden ?pl cevabinda
			// bakilan kullanicinin adi g_szNickName'e (bot adi) dusuyordu. Grup
			// basligindaki ad, kimligin dogru kaynagidir (rename edilmis kisilerde de
			// guncel kalir).
			if(pCloseBracket && pCloseBracket + 1 < pOpenParen && iSenderId >= 0 && iSenderId < MAX_PLAYERS)
			{
				const char *pNameStart = pCloseBracket + 1;
				while(*pNameStart == ' ') pNameStart++;
				size_t nLen = (size_t)(pOpenParen - pNameStart);
				if(nLen > 0 && nLen <= 20)
				{
					char szGrpName[64] = {0};
					memcpy(szGrpName, pNameStart, nLen);
					szGrpName[nLen] = 0;
					if(szGrpName[0])
					{
						playerInfo[iSenderId].iIsConnected = 1;
						strncpy_s(playerInfo[iSenderId].szPlayerName,
							sizeof(playerInfo[iSenderId].szPlayerName), szGrpName, _TRUNCATE);
						// Isim cache'ini de guncelle (restart sonrasi ?pl ismi bilinsin)
						if(g_playerCache[iSenderId].dwTime == 0
							|| strcmp(g_playerCache[iSenderId].szName, szGrpName) != 0)
						{
							strncpy_s(g_playerCache[iSenderId].szName,
								sizeof(g_playerCache[0].szName), szGrpName, _TRUNCATE);
							g_playerCache[iSenderId].dwTime = 1;
							SavePlayerCache();
						}
					}
				}
			}

			// Grup ID'si: "(GROUP) [20]" -> 20
			if(pCloseBracket && pCloseBracket > szNonColorEmbeddedMsg + 8)
			{
				char szGrpId[16] = {0};
				int n = 0;
				for(const char *q = szNonColorEmbeddedMsg + 8; q < pCloseBracket && n < 15; q++)
					szGrpId[n++] = *q;
				szGrpId[n] = 0;
				int iGrp = atoi(szGrpId);

				// Kendi grubumuzu ilk gordugumuzde kaydet
				if(iSenderId == g_myPlayerID && iGrp > 0)
					g_myGroupId = szGrpId;

				// Ayni grupta gordugumuz oyuncular -> grup uyesi
				if(g_myGroupId != "" && szGrpId == g_myGroupId)
					g_knownGroupMembers[iSenderId] = 1;
			}

			// Bot kendi sendChat ile gonderdigi mesaji (GROUP) olarak echo
			// edilirse tekrar islenmesin (cevap/otomatik mesajlar).
			// NOT: Oyun ici (GTA SA) sohbet kutusundan yazilan mesajlar sunucu
			// tarafindan gonderan oyuncunun ID'siyle yayinlanir. Kullanici
			// bot hesabi (hakixld) degil, oyun hesabiyla (orn. washe) yazdigi
			// icin iSenderId != g_myPlayerID olur. Bu yuzden sadece bot'un
			// KENDI echo'larini engelliyoruz, diger grup uyelerinin mesajlarini
			// HandleGroupCommand'e veriyoruz.
			if(iSenderId == g_myPlayerID) return;

			// Sunucu kendi mesajimizi (GROUP) olarak geri echo etmedigi icin komut
			// mantigi HandleGroupCommand'e tasindi (konsol inputu RunCommand'den de cagrilir).
			HandleGroupCommand(pGroupMsg, iSenderId);
		}
	}
}

// Grup chat komutlarini lokal isler (?help, ?w, ?ping, ?pl, ?gst, ?gstats,
// ?greet, ?welcomemsg, ?gzt, ?discordchat, ?1414, ?gz ve !<mesaj>).
// Sunucu kendi mesajini (GROUP) olarak geri echo etmedigi icin bu mantik hem
// ClientMessage yakalamasindan hem de RunCommand (konsol inputu) icin cagrilir.
// iSenderId < 0 ise konsol/gelen sender yok anlaminda g_myPlayerID varsayilir.
void HandleGroupCommand(const char *pGroupMsg, int iSenderId)
{
			// YS.Hold'un periyodik "Player [X] Stats -> Packetloss: y | FPS: z | Ping: w"
			// mesaji: FPS dahil tam istatistiktir. /pnetstats FPS icermediginden bunu
			// cache'e kaydet (?pl cevabinda kullanilir), player_cache.txt'ye yaz.
			if(!strncmp(pGroupMsg, "Player [", 8) && strstr(pGroupMsg, " Stats ->"))
			{
				int iPid = atoi(pGroupMsg + 8);
				if(iPid >= 0 && iPid < MAX_PLAYERS)
				{
					bool bNew = (g_playerCache[iPid].dwTime == 0);
					double dLoss = NumberAfter(pGroupMsg, "Packetloss:");
					double dFps  = NumberAfter(pGroupMsg, "FPS:");
					double dPing = NumberAfter(pGroupMsg, "Ping:");
					if(dLoss >= 0.0) g_playerCache[iPid].dLoss = dLoss;
					if(dFps  >= 0.0) g_playerCache[iPid].iFps = (int)dFps;
					if(dPing >= 0.0) g_playerCache[iPid].iPing = (int)dPing;
					g_playerCache[iPid].dwTime = GetTickCount();
					SavePlayerCache();
					if(bNew)
						Log("[PL-CACHE] %d (%s) stats cache: loss %.2f%% fps %d ping %d",
							iPid, g_playerCache[iPid].szName[0] ? g_playerCache[iPid].szName : "?",
							g_playerCache[iPid].dLoss, g_playerCache[iPid].iFps, g_playerCache[iPid].iPing);

					// ?pl bekliyorsa ve hedef bu oyuncuysa cevabi simdi gonder:
					// /pnetstats dialogu "Network Active: 1" gibi BOS doner; asil
					// loss/fps/ping bu "Stats ->" mesajindadir (SAMPRPC dialogda loss
					// bulamazsa cevabi buraya birakir, flag acik kalir).
					if(g_waitingForPNetStats && g_plTargetId == iPid
						&& GetTickCount() - g_pnetstatsReqTime <= 15000)
					{
						g_waitingForPNetStats = false;
						const char *pName = "Player ?";
						char szFallbackName[32];
						if(playerInfo[iPid].szPlayerName[0]) pName = playerInfo[iPid].szPlayerName;
						else if(g_playerCache[iPid].szName[0]) pName = g_playerCache[iPid].szName;
						else { sprintf_s(szFallbackName, sizeof(szFallbackName), "Player %d", iPid); pName = szFallbackName; }
						char szFps[16], szPing[16];
						if(g_playerCache[iPid].iFps >= 0) sprintf_s(szFps, sizeof(szFps), "%d", g_playerCache[iPid].iFps); else strcpy(szFps, "--");
						if(g_playerCache[iPid].iPing >= 0) sprintf_s(szPing, sizeof(szPing), "%d", g_playerCache[iPid].iPing); else strcpy(szPing, "--");
						char szOut[192];
						sprintf_s(szOut, sizeof(szOut), "!%s's PL: %.2f%% | FPS: %s | Ping: %s ms",
							pName, g_playerCache[iPid].dLoss, szFps, szPing);
						Log("[PNETSTATS] Stats-post ile cevap: %s", szOut);
						g_bridgeSuppressEcho++;
						sendChat(szOut);
					}
				}
				return;
			}

			// YS botun "Flashing Zones:" mesajlari: grup tag'lerini topla (harvest icin).
			// iSenderId == g_myPlayerID -> bu sendChat'in LOKAL cagrisidir (misc_funcs.cpp:
			// sendChat -> HandleGroupCommand(szMessage, g_myPlayerID)). Bot kendi
			// BuildFlashZoneSummary ozetini ("!Flashing Zones: Free(...)") yeniden toplar
			// ve harvest kendi echo'sundan tag uretirdi. Diger oyuncularin (YS bot vs.)
			// mesajlari ag uzerinden ClientMessage (GROUP) ile gelir, onlari isle.
			if(strstr(pGroupMsg, "Flashing Zones:"))
			{
				if(iSenderId != g_myPlayerID)
				{
					AddFlashTags(pGroupMsg);
					SyncFlashGroupColors(pGroupMsg); // renk<->isim eslesmesini aninda ogren
					// YS dagilimini sakla: ?zf bu isimleri dogrudan yanitlar ("?" gostermez).
					const char *pFz = strstr(pGroupMsg, "Flashing Zones:");
					if(pFz)
					{
						const char *pRest = pFz + strlen("Flashing Zones:");
						while(*pRest == ' ' || *pRest == '|') pRest++;
						char szBody[160] = {0};
						strncpy_s(szBody, sizeof(szBody) - 1, pRest, sizeof(szBody) - 1);
						// "|" ayiracli kuyruk bos ise at; body'yi sabitle
						while(szBody[0] && (szBody[strlen(szBody)-1] == ' ' || szBody[strlen(szBody)-1] == '|'))
							szBody[strlen(szBody)-1] = 0;
						if(szBody[0])
						{
							sprintf_s(g_ysFlashMsg, sizeof(g_ysFlashMsg), "!Flashing Zones: %s", szBody);
							g_ysFlashMsgTick = GetTickCount();
							Log("[ZF] YS dagilimi kaydedildi: %s", g_ysFlashMsg);
						}
					}
				}
				Log("[ZF] YS flash mesaji: %s", pGroupMsg);
				return; // normal komut islemine sokma
			}

			int iCmdSender = iSenderId;
			if(iCmdSender < 0 || iCmdSender >= MAX_PLAYERS)
				iCmdSender = g_myPlayerID;

			// Komutu kullananin oyun ici adi (?w, ?1414, ?pl icin Discord'a isim gider).
			const char *pCmdSenderName = "";
			if(playerInfo[iCmdSender].iIsConnected && playerInfo[iCmdSender].szPlayerName[0])
				pCmdSenderName = playerInfo[iCmdSender].szPlayerName;
			else
				pCmdSenderName = g_szNickName;

			// --- ?help : komut listesini oyunda goster (Discord'a gitmez) ---
			if(!strncmp(pGroupMsg, "?help", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[HELP] ?help yakalandi, komut listesi gonderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Komutlar: ?w ?ping ?pl [id] ?gst ?az ?gzt ?gz ?gz2 ?za ?zattack ?zlist ?zwho ?zscan ?zf ?zgo", "!Commands: ?w ?ping ?pl [id] ?gst ?az ?gzt ?gz ?gz2 ?za ?zattack ?zlist ?zwho ?zscan ?zf ?zgo"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Ayarlar: ?greet ?welcomemsg ?gm <mesaj> ?discordchat", "!Settings: ?greet ?welcomemsg ?gm <message> ?discordchat"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Bilgi: ?info ?disc ?test ?m ?astats <id> ?alogs <id>", "!Info: ?info ?disc ?test ?m ?astats <id> ?alogs <id>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Discord'a mesaj: !<mesaj> yaz", "!Send Discord message: !<message>"));
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Nasil: ?help", "!How: ?help"));
			}
			// --- ?test : baglanti kontrolu, bot cevap verir ---
			else if(!strncmp(pGroupMsg, "?test", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[TEST] ?test yakalandi, cevap gonderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!Bot calisiyor. ?help ile komutlari gor.", "!Bot is running. Use ?help to see commands."));
			}
			// --- ?info : bot surum bilgisi ---
			else if(!strncmp(pGroupMsg, "?info", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[INFO] ?info yakalandi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)LM("!RakSAMP bot v1.0 - ?help ile komut listesi", "!RakSAMP bot v1.0 - ?help for commands"));
			}
			// --- ?disc : Discord sunucu linki ---
			else if(!strncmp(pGroupMsg, "?disc", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				Log("[DISC] ?disc yakalandi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Discord: uifserver.net/discord");
			}
			// --- ?w : Discord war help, 5 dk cooldown (spam onlemek icin; 30 sn iken Discord'a yagmur gibi iniyordu) ---
			else if(!strncmp(pGroupMsg, "?w", 2) && (pGroupMsg[2] == '\0' || pGroupMsg[2] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastWarHelpTime != 0 && (dwNow - g_lastWarHelpTime) < 300000)
				{
					int iLeft = (int)((300000 - (dwNow - g_lastWarHelpTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?w)", "!Cooldown: %d secs left (?w)"), iLeft);
					Log("[WARHELP] Cooldown aktif (%d sn kaldi), ?w yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastWarHelpTime = dwNow;
				Log("[WARHELP] Grup chat'inde ?w yakalandi. %s Discord'a gonderiliyor...", pCmdSenderName);
				TriggerWarHelp(pCmdSenderName);
			}
			// --- ?ping : yanit verir, 30 sn cooldown (RakGravy ile ayni) ---
			if(!strncmp(pGroupMsg, "?ping", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastPingTime != 0 && (dwNow - g_lastPingTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastPingTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?ping)", "!Cooldown: %d secs left (?ping)"), iLeft);
					Log("[PING] Cooldown aktif (%d sn kaldi), ?ping yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastPingTime = dwNow;
				Log("[PING] ?ping yakalandi, Pong cevabi gonderiliyor.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Pong!");
			}
			// --- ?pl [id] : /pnetstats dialogundan packetloss, 30 sn cooldown ---
			else if(!strncmp(pGroupMsg, "?pl", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastPLTime != 0 && (dwNow - g_lastPLTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastPLTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?pl)", "!Cooldown: %d secs left (?pl)"), iLeft);
					Log("[PNETSTATS] Cooldown aktif (%d sn kaldi), ?pl yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastPLTime = dwNow;

				// id verilmezse: komutu KULLANAN KISININ istatistigi (grup basligindan
				// gelen iSenderId). Konsol/lokal cagrilarinda iSenderId zaten bot'tur.
				int iTarget = (iSenderId >= 0 && iSenderId < MAX_PLAYERS) ? iSenderId : g_myPlayerID;
				const char *pArg = pGroupMsg + 3;
				while(*pArg == ' ') pArg++;
				if(*pArg != '\0')
				{
					// Baglanti kontrolu YOK: istenen id her zaman sorgulanir (UIF tum
					// oyuncularin istatistiklerini bilir). Bagliyiz sandigimiz kisi yanlis
					// olabilir (ServerJoin'i kacirmisiz); sunucu cozer.
					int i = atoi(pArg);
					if(i >= 0 && i < MAX_PLAYERS)
						iTarget = i;
				}

				g_plTargetId = iTarget;
				g_waitingForPNetStats = true;
				g_pnetstatsReqTime = GetTickCount();

				char szCmd[64];
				if(iTarget == g_myPlayerID)
					sprintf_s(szCmd, sizeof(szCmd), "/pnetstats");
				else
					sprintf_s(szCmd, sizeof(szCmd), "/pnetstats %d", iTarget);

				Log("[PNETSTATS] /pnetstats gonderiliyor (hedef: %d)...", iTarget);
				sendServerCommand(szCmd);
				// DB'deki oyuncu cache'ini de hemen tazele (eski veri backend'de silinir)
				SyncPlayerCacheToDb();
			}
			// --- ?gst : /gstats dialogundan zone + aktif oyuncu sayisini oyunda goster (Discord'a gitmez) ---
			else if(!strncmp(pGroupMsg, "?gst", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGStatsTime != 0 && (dwNow - g_lastGStatsTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastGStatsTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?gst)", "!Cooldown: %d secs left (?gst)"), iLeft);
					Log("[GST] Cooldown aktif (%d sn kaldi), ?gst yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGst = true;
				g_gstReqTime = GetTickCount();
				Log("[GST] /gstats gonderiliyor (zone + aktif oyuncu sayisi icin)...");
				sendServerCommand((char *)"/gstats");
			}
			// --- ?az : attackable (free/sahipsiz) zone sayisini aninda goster (YS bot gibi) ---
			else if(!strncmp(pGroupMsg, "?az", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				int nFree = 0;
				for(int zid = 0; zid < g_zoneCount; zid++)
				{
					unsigned int c = g_zoneColor[zid];
					if(c == 0) { nFree++; continue; }
					int idx = FindGroupColorByColor(c);
					if(idx >= 0 && !g_groupColors[idx].szTag[0] && !g_groupColors[idx].szName[0]) nFree++;
				}
				char szOut[96];
				sprintf_s(szOut, sizeof(szOut), "!Attackable zones: %d", nFree);
				Log("[AZ] Attackable zones: %d (anlik renk haritasi).", nFree);
				g_bridgeSuppressEcho++;
				sendChat(szOut);
			}
			// --- ?gstats / ?act : /gstats dialogunu Discord'a yolla, 30 sn cooldown ---
			else if((!strncmp(pGroupMsg, "?gstats", 7) && (pGroupMsg[7] == '\0' || pGroupMsg[7] == ' '))
			     || (!strncmp(pGroupMsg, "?act", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' ')))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGStatsTime != 0 && (dwNow - g_lastGStatsTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastGStatsTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?gstats)", "!Cooldown: %d secs left (?gstats)"), iLeft);
					Log("[GSTATS] Cooldown aktif (%d sn kaldi), ?gstats yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGStats = true;
				g_gstatsReqTime = GetTickCount();
				g_gstatsSentFirst = false;
				Log("[GSTATS] /gstats gonderiliyor...");
				sendServerCommand((char *)"/gstats");
			}
			// --- ?greet : welcome back mesajlarini ac/kapat (MongoDB'de saklanir) ---
			else if(!strncmp(pGroupMsg, "?greet", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				Log("[WELCOME] ?greet komutu yakalandi, durum degistiriliyor...");
				ToggleWelcomeBack();
			}
			// --- ?welcomemsg <mesaj> : kisiye ozel welcome mesaji ayarla ---
			else if(!strncmp(pGroupMsg, "?welcomemsg", 11) && (pGroupMsg[11] == '\0' || pGroupMsg[11] == ' '))
			{
				const char *pArg = pGroupMsg + 11;
				while(*pArg == ' ') pArg++;
				if(*pArg == '\0')
				{
					Log("[WELCOME] ?welcomemsg icin mesaj bos, ayar siliniyor.");
					SetWelcomeMessage("");
				}
				else
				{
					char szMsg[256] = {0};
					strncpy_s(szMsg, sizeof(szMsg) - 1, pArg, sizeof(szMsg) - 1);
					Log("[WELCOME] ?welcomemsg ayarlaniyor: %s", szMsg);
					SetWelcomeMessage(szMsg);
				}
			}
			// --- ?gm <mesaj> : ?welcomemsg alias (E1 uyumlu) ---
			else if(!strncmp(pGroupMsg, "?gm", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				const char *pArg = pGroupMsg + 3;
				while(*pArg == ' ') pArg++;
				if(*pArg == '\0')
				{
					Log("[WELCOME] ?gm icin mesaj bos, ayar siliniyor.");
					SetWelcomeMessage("");
				}
				else
				{
					char szMsg[256] = {0};
					strncpy_s(szMsg, sizeof(szMsg) - 1, pArg, sizeof(szMsg) - 1);
					Log("[WELCOME] ?gm ayarlaniyor: %s", szMsg);
					SetWelcomeMessage(szMsg);
				}
			}
			// --- ?gzt : zone war timer — chat duyurusundan hesaplanan kalan sureyi gruba bildir ---
			else if(!strncmp(pGroupMsg, "?gzt", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGztTime != 0 && (dwNow - g_lastGztTime) < 30000)
				{
					int iLeft = (int)((30000 - (dwNow - g_lastGztTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?gzt)", "!Cooldown: %d secs left (?gzt)"), iLeft);
					Log("[GZT] Cooldown aktif (%d sn kaldi), ?gzt yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastGztTime = dwNow;

				// Kalan sure: oncelikle yakalanan GZ timer textdraw'ina dayanir
				// (gercek kalan sure; chat duyuru ani ile savas basi ayni degil).
				// Yoksa baslangic tick'inden hesaplanan deger kullanilir.
				int iRemainingSec = -1;
				if(!g_inZoneWar)
				{
					Log("[GZT] Savas aktif degil, War time bilgisi gonderilemiyor.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War time: yok", "!War time: none"));
					return;
				}
				if(g_warTimerTick != 0 && dwNow - g_warTimerTick < g_zwWarDurationMs)
					iRemainingSec = g_warRemainingSec - (int)((dwNow - g_warTimerTick) / 1000);
				else if(g_zoneWarStartTick != 0 && dwNow - g_zoneWarStartTick < g_zwWarDurationMs)
					iRemainingSec = (int)((g_zwWarDurationMs - (dwNow - g_zoneWarStartTick)) / 1000);

				if(iRemainingSec >= 0)
				{
					int iMin = iRemainingSec / 60;
					int iSec = iRemainingSec % 60;
					char szWarTime[192];
					sprintf_s(szWarTime, sizeof(szWarTime), "!War time: %d:%02d", iMin, iSec);
					Log("[GZT] Kalan sure hesaplandi: %d:%02d", iMin, iSec);
					g_bridgeSuppressEcho++;
					sendChat(szWarTime);
				}
				else
				{
					// Savas baslangici bilinmiyor ya da coktan bitti.
					Log("[GZT] Savas baslangici yok, War time bilgisi gonderilemiyor.");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!War time: yok", "!War time: none"));
				}
			}
			// --- ?tr / ?en : grup dili (varsayilan INGILIZCE; ?tr ile Turkce'ye gec) ---
			else if(!strncmp(pGroupMsg, "?tr", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				g_langTurkish = true;
				SaveLang();
				ServerSetBotLang(true); // MongoDB Guild.lang'a yaz
				Log("[LANG] ?tr ile Turkce secildi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Dil: Turkce. Mesajlar artik Turkce atilacak.");
			}
			else if(!strncmp(pGroupMsg, "?en", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				g_langTurkish = false;
				SaveLang();
				ServerSetBotLang(false); // MongoDB Guild.lang'a yaz
				Log("[LANG] ?en ile Ingilizce secildi.");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!Language: English. Messages will now be sent in English.");
			}
			// --- ?guildid <id> : bu botu kullanan Discord sunucusunu (guild) bagla ---
			else if(!strncmp(pGroupMsg, "?guildid", 8) && (pGroupMsg[8] == ' ' || pGroupMsg[8] == '\0'))
			{
				const char *pArg = pGroupMsg + 8;
				while(*pArg == ' ') pArg++;
				if(!pArg[0])
				{
					Log("[GUILDID] Arguman yok, kullanim: ?guildid <discord sunucu id>");
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Kullanim: ?guildid <discord sunucu id>",
						"!Usage: ?guildid <discord guild id>"));
				}
				else if(ServerSetGuildId(pArg))
				{
					Log("[GUILDID] Grup Discord sunucusuna baglandi: %s", pArg);
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Discord sunucusuna baglandi.", "!Linked to Discord guild."));
				}
				else
				{
					Log("[GUILDID] Baglanti basarisiz: %s", pArg);
					g_bridgeSuppressEcho++;
					sendChat((char *)LM("!Baglanti basarisiz: sunucu botu eklememis olabilir.",
						"!Link failed: the guild may not have added the bot."));
				}
			}
			// --- ?discordchat : Discord grup chat kopusunu ac/kapat (MongoDB'de saklanir) ---
			else if(!strncmp(pGroupMsg, "?discordchat", 12) && (pGroupMsg[12] == '\0' || pGroupMsg[12] == ' '))
			{
				Log("[GROUPCHAT] ?discordchat komutu yakalandi, durum degistiriliyor...");
				ToggleDiscordChat();
			}
			// --- ?m : game-modes listesi sunucuya sor ---
			else if(!strncmp(pGroupMsg, "?m", 2) && (pGroupMsg[2] == '\0' || pGroupMsg[2] == ' '))
			{
				Log("[MODES] ?m yakalandi, /gamemodes gonderiliyor.");
				sendServerCommand((char *)"/gamemodes");
			}
			// --- ?astats <id> : account stats sunucuya sor ---
			else if(!strncmp(pGroupMsg, "?astats", 7) && (pGroupMsg[7] == ' ' || pGroupMsg[7] != '\0'))
			{
				Log("[ASTATS] ?astats yakalandi.");
				char szCmd[64];
				sprintf_s(szCmd, sizeof(szCmd), "/astats %s", pGroupMsg + 7);
				sendServerCommand(szCmd);
			}
			// --- ?alogs <id> : admin loglari sunucuya sor ---
			else if(!strncmp(pGroupMsg, "?alogs", 6) && (pGroupMsg[6] == ' ' || pGroupMsg[6] != '\0'))
			{
				Log("[ALOGS] ?alogs yakalandi.");
				char szCmd[64];
				sprintf_s(szCmd, sizeof(szCmd), "/alogs %s", pGroupMsg + 6);
				sendServerCommand(szCmd);
			}
			// --- ?grole <id> <rol> : grup uye rolunu ayarla (sunucuya ilet) ---
			else if(!strncmp(pGroupMsg, "?grole", 6) && (pGroupMsg[6] == ' ' || pGroupMsg[6] != '\0'))
			{
				Log("[GROLE] ?grole yakalandi.");
				char szCmd[64];
				sprintf_s(szCmd, sizeof(szCmd), "/grouprole %s", pGroupMsg + 6);
				sendServerCommand(szCmd);
			}
			// --- ?post <neden> : rol degisikligine gerekce ekle (sunucuya ilet) ---
			else if(!strncmp(pGroupMsg, "?post", 5) && (pGroupMsg[5] == ' ' || pGroupMsg[5] != '\0'))
			{
				Log("[POST] ?post yakalandi.");
				char szCmd[128];
				sprintf_s(szCmd, sizeof(szCmd), "/rankreason %s", pGroupMsg + 5);
				sendServerCommand(szCmd);
			}
			// --- ?tag <tag> <yeni> : grup zone tagini duzelt (sunucuya ilet) ---
			else if(!strncmp(pGroupMsg, "?tag", 4) && (pGroupMsg[4] == ' ' || pGroupMsg[4] != '\0'))
			{
				Log("[TAG] ?tag yakalandi.");
				char szCmd[128];
				sprintf_s(szCmd, sizeof(szCmd), "/groupzonetag %s", pGroupMsg + 4);
				sendServerCommand(szCmd);
			}
			// --- ?zlist / ?zwho : bulundugu zone bilgisi → /gzinfo (hangi grup, ne zaman bosalir) ---
			else if((!strncmp(pGroupMsg, "?zlist", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			     || (!strncmp(pGroupMsg, "?zwho", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' ')))
			{
				Log("[ZONE] %s yakalandi, /gzinfo gonderiliyor.", pGroupMsg);
				sendServerCommand((char *)"/gzinfo");
			}
			// --- ?zscan : grup renk haritasini DB'ye taze yaz (eski veri silinip yeniden yazilir,
// route tarafinda DELETE+INSERT) ve bilinen grup tag'lerini goster + tarama baslat ---
			else if(!strncmp(pGroupMsg, "?zscan", 6) && (pGroupMsg[6] == '\0' || pGroupMsg[6] == ' '))
			{
				Log("[ZONE] ?zscan yakalandi, DB tazeleniyor + tarama baslatiliyor (%d grup).", g_groupColorCount);
				// 1) Tag'lari oyunda goster (YS gibi en buyuk okunurluk)
				char szOut[192];
				int nOut = sprintf_s(szOut, sizeof(szOut), "!Groups: ");
				for(int i = 0; i < g_groupColorCount && nOut < (int)sizeof(szOut) - 3; i++)
				{
					int nL = sprintf_s(szOut + nOut, sizeof(szOut) - nOut,
						"%s%s", i ? ", " : "", g_groupColors[i].szTag[0] ? g_groupColors[i].szTag : "Free");
					nOut += nL;
				}
				Log("[ZONE] %s", szOut);
				g_bridgeSuppressEcho++;
				sendChat(szOut);
				// 2) DB'yi hemen tazele (backend DELETE+INSERT ile eskiyi siler)
				SyncGroupColorsToDb();
				SyncPlayerCacheToDb();
				// 3) Taramayi baslat: bilinmeyen renkleri /gzinfo'dan ogren
				if(g_zoneScanIdx < 0 && iSpawned && !g_inZoneWar && !g_zgoActive)
					StartZoneScan(false);
			}
			// --- ?zgo <#N | gruptag> : zone'a isinlan, 40 sn bekle, sonra geri don (gruba bildirir) ---
			// Sayi verilirse o zone; tag verilirse o grubun rengine ait 1 zone'a gider.
			else if(!strncmp(pGroupMsg, "?zgo", 4) && (pGroupMsg[4] == ' ' || pGroupMsg[4] == '#' || (pGroupMsg[4] >= '0' && pGroupMsg[4] <= '9')))
			{
				const char *pArg = pGroupMsg + 4;
				while(*pArg == ' ' || *pArg == '#') pArg++;
				int iZone = -1;
				if(pArg[0] >= '0' && pArg[0] <= '9')
					iZone = atoi(pArg);
				else
					iZone = FindZoneByGroupTag(pArg);
				if(iZone >= 0 && iZone < g_zoneCount)
				{
					// Eski (spawn) pozisyonu kaydet
					float fPos[3] = {0, 0, 0};
					if(!getPlayerPos(g_myPlayerID, fPos))
					{
						Log("[ZGO] Mevcut pozisyon alinamadi, geri donus noktasi kullanilamayacak.");
					}
					g_zgoReturnPos[0] = fPos[0];
					g_zgoReturnPos[1] = fPos[1];
					g_zgoReturnPos[2] = fPos[2];

					float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
					float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
					char szCmd[128];
					sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
					Log("[ZGO] ?zgo %s -> zone #%d merkezine isinlaniliyor (%s), 40 sn beklenip donulecek.", pArg, iZone, szCmd);

					g_zgoActive = true;
					g_zgoZoneId = iZone;
					g_zgoReturnTick = GetTickCount() + ZGO_STAY_MS;
					sendServerCommand(szCmd);

					char szMsg[128];
					sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d de. /go yazarak yanima isinlanabilirsin, 40 sn sonra donuyorum.",
				"!At zone #%d. Type /go to teleport to me, returning in 40 sec."), iZone);
					g_bridgeSuppressEcho++;
					sendChat(szMsg);
				}
				else
				{
					// Tag bilinmiyorsa (renk ogrenilmemis): YS flash listesinden tanidik
					// bir tag ise HEMEN /gstats taramasi baslat (90 sn kuyruk beklemez).
					AddSeenTag(pArg);
					int fi = -1;
					for(int k = 0; k < g_seenTagCount; k++)
						if(!_stricmp(g_seenTags[k], pArg)) { fi = k; break; }
					if(fi >= 0)
					{
						g_harvestIdx = fi + 1; // siradaki harvest bu tag'dan baslayacak
						g_lastHarvestTime = 0; // 90 sn koselini atla
						Log("[ZONE] ?zgo %s icin renk bilinmiyor, harvest kuyruguna alindi (aninda).", pArg);
						g_bridgeSuppressEcho++;
						char szMsg[160];
						sprintf_s(szMsg, sizeof(szMsg),
							LM("!%s icin zon taranıyor, ~30 sn sonra ?zgo %s tekrar dene.",
							   "!Scanning zones for %s, retry ?zgo %s in ~30 sec."), pArg, pArg);
						sendChat(szMsg);
					}
					else
					{
						Log("[ZONE] ?zgo %s icin zone bulunamadi (tag YS listesinde yok).", pArg);
						g_bridgeSuppressEcho++;
						char szMsg[160];
						sprintf_s(szMsg, sizeof(szMsg), LM("!%s icin zone bulunamadi.", "!No zone found for %s."), pArg);
						sendChat(szMsg);
					}
				}
			}
			// --- ?zgo (argumansiz) : zone listesi bilgisi ---
			else if(!strncmp(pGroupMsg, "?zgo", 4) && (pGroupMsg[4] == '\0'))
			{
				Log("[ZONE] ?zgo (argumansiz), /gzinfo gonderiliyor.");
				sendServerCommand((char *)"/gzinfo");
			}
			// --- ?za / ?zattack [<gruptag>] : zone saldirisi — <gruptag> verilirse once o grubun
// zonunu bulup gider, sonra /gz gonderir (bot co-leader/leader olmali; yetki sunucuda). ---
			else if((!strncmp(pGroupMsg, "?za", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			     || (!strncmp(pGroupMsg, "?zattack", 8) && (pGroupMsg[8] == '\0' || pGroupMsg[8] == ' ')))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastZoneWarTime != 0 && (dwNow - g_lastZoneWarTime) < 180000)
				{
					int iLeft = (int)((180000 - (dwNow - g_lastZoneWarTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?za)", "!Cooldown: %d secs left (?za)"), iLeft);
					Log("[ZONEWAR] Cooldown aktif (%d sn kaldi), ?za yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastZoneWarTime = dwNow;
				Log("[ZONE] %s yakalandi. /gz sunucu komutu gonderiliyor...", pGroupMsg);
				// <gruptag> verildiyse once hedef zonu bul (taga gore), oraya isinla
				const char *pA = pGroupMsg + (strncmp(pGroupMsg, "?zattack", 8) == 0 ? 8 : 3);
				while(*pA == ' ') pA++;
				if(pA[0] && pA[0] >= 'A' && pA[0] <= 'z')
				{
					int iZone = FindZoneByGroupTag(pA);
					if(iZone >= 0 && iZone < g_zoneCount)
					{
						float fCenterX = (g_zoneMinX[iZone] + g_zoneMaxX[iZone]) / 2.0f;
						float fCenterY = (g_zoneMinY[iZone] + g_zoneMaxY[iZone]) / 2.0f;
						char szCmd[128];
						sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", fCenterX, fCenterY, ZGO_TELEPORT_Z);
						Log("[ZONE] %s -> grup zonu #%d (%s), /pos gonderildi + /gz.", pA, iZone, szCmd);
						sendServerCommand(szCmd);
					}
					else
					{
						Log("[ZONE] %s icin zon bulunamadi, yine de /gz deniyor.", pA);
					}
				}
				sendServerCommand((char *)"/gz");
			}
			// --- ?zf <#N> : zone durumu sorgula → /gzinfo + flashing zone dagilimi ---
			else if(!strncmp(pGroupMsg, "?zf", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				Log("[ZONE] ?zf yakalandi, /gzinfo gonderiliyor.");
				// /gzinfo asenkron: flash paketleri gelene kadar bekle, 3 sn sonra ozet gonder.
				g_zfFlashAt = GetTickCount() + 3000;
				g_zfFlashPendingFlag = true;
				sendServerCommand((char *)"/gzinfo");
			}
			// --- ?gz : zone war baslatma → Discord zonewar kanalina bildirim, 180 sn cooldown (RakGravy ile ayni) ---
			else if(!strncmp(pGroupMsg, "?gz", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastZoneWarTime != 0 && (dwNow - g_lastZoneWarTime) < 180000)
				{
					int iLeft = (int)((180000 - (dwNow - g_lastZoneWarTime)) / 1000);
					char szCd[128];
					sprintf_s(szCd, sizeof(szCd), LM("!Cooldown: %d sn kaldi (?gz)", "!Cooldown: %d secs left (?gz)"), iLeft);
					Log("[ZONEWAR] Cooldown aktif (%d sn kaldi), ?gz yoksayildi.", iLeft);
					g_bridgeSuppressEcho++;
					sendChat(szCd);
					return;
				}
				g_lastZoneWarTime = dwNow;
				Log("[ZONEWAR] Grup chat'inde ?gz yakalandi. Zone war bildirimi gonderiliyor...");
				SendZoneWarEvent("keeps", "");
			}
			// --- ?gz2 : GZ'ye girmeyip gruba belirgin "/gz2 yazin" duyurusu atar (bot savasa katilmaz) ---
			else if(!strncmp(pGroupMsg, "?gz2", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				Log("[ZONEWAR] ?gz2 yakalandi, grup chatine belirgin duyuru atiliyor (bot GZ'ye girmiyor).");
				g_bridgeSuppressEcho++;
				sendChat((char *)"!@@@@ /gz2 @@@@ GZ'ye girin! (savas devam ediyor) @@@@ /gz2 @@@@");
			}
			// --- Grup chat kopusu: ! ile baslayan mesajlar Discord'a iletilir.
			// ornek: oyunda "!test" yazilir -> bot sunucudan "!test" (GROUP) alir,
			// basta ! (Discord'a gonderim isareti), sonrasi ("test") Discord'a yollanir.
			// (Geri echo: sendChat ile gonderilen kendi mesajlarimizda da ! on-eki var;
			//  g_bridgeSuppressEcho ile Discord'a tekrar gitmeleri engellenir.)
			// Discord'dan oyuna iletilen mesajlar "![Discord] ..." on-ekiyle gelir;
			// bunlar tekrar Discord'a gonderilmez (sonsuz dongu engeli).
			else if(pGroupMsg[0] == '!')
			{
				if(g_bridgeSuppressEcho > 0)
				{
					g_bridgeSuppressEcho--;
					return;
				}
				// Konsol'dan (cmds.cpp ! yolu) yazilan mesajlar gruba gider ama
				// Discord'a KOPRULENMEZ: iSenderId == bot = bot/console gonderimi.
				// Oyuncularin oyun ici !<mesaj> koprusu iSenderId != bot oldugu
				// icin calismaya devam eder.
				if(iSenderId == g_myPlayerID)
				{
					Log("[GROUPCHAT] Bot/konsol mesaji Discord'a iletilmedi: %s", pGroupMsg + 1);
					return;
				}
				if(strncmp(pGroupMsg, "![Discord]", 10) == 0)
				{
					Log("[GROUPCHAT] Discord geri iletimi atlandi (%s).", pGroupMsg);
					return;
				}
				Log("[GROUPCHAT] Grup mesaji Discord'a iletilmek uzere: %s", pGroupMsg + 1);
				SendGroupChatToDiscord(pGroupMsg + 1);
			}
}

void Chat(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	PlayerID sender = rpcParams->sender;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	PLAYERID playerId;
	BYTE byteTextLen;

	unsigned char szText[256];
	memset(szText, 0, 256);

	bsData.Read(playerId);
	bsData.Read(byteTextLen);
	bsData.Read((char*)szText, byteTextLen);
	szText[byteTextLen] = 0;

	if(playerId < 0 || playerId >= MAX_PLAYERS)
		return;

	if(imitateID == playerId || playerId == g_myPlayerID)
		sendChat((char *)szText);

	Log("[CHAT] %s: %s", playerInfo[playerId].szPlayerName, szText);

	if(settings.iFind)
	{
		for(int i = 0; i < MAX_FIND_ITEMS; i++)
		{
			if(!settings.findItems[i].iExists)
				continue;

			if(strstr((const char *)szText, settings.findItems[i].szFind))
			{
				if(settings.findItems[i].szSay[0] != 0x00)
					sendChat(settings.findItems[i].szSay);
			}
		}
	}
}

void UpdateScoresPingsIPs(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYERID playerId;
	int iPlayerScore;
	DWORD dwPlayerPing;

	for(PLAYERID i=0; i<(iBitLength/8)/9; i++)
	{
		bsData.Read(playerId);
		bsData.Read(iPlayerScore);
		bsData.Read(dwPlayerPing);

		if(playerId < 0 || playerId >= MAX_PLAYERS)
			continue;

		playerInfo[playerId].iScore = iPlayerScore;
		playerInfo[playerId].dwPing = dwPlayerPing;
	}
}

void SetCheckpoint(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.CurrentCheckpoint.fPosition[0]);
	bsData.Read(settings.CurrentCheckpoint.fPosition[1]);
	bsData.Read(settings.CurrentCheckpoint.fPosition[2]);
	bsData.Read(settings.CurrentCheckpoint.fSize);

	settings.CurrentCheckpoint.bActive = true;

	char SetCheckpointAlert[256];
	sprintf_s(SetCheckpointAlert, 256, "[CP] Checkpoint set to %.2f %.2f %.2f position. (size: %.2f)", settings.CurrentCheckpoint.fPosition[0], settings.CurrentCheckpoint.fPosition[1], settings.CurrentCheckpoint.fPosition[2], settings.CurrentCheckpoint.fSize);
	Log(SetCheckpointAlert);
}

void DisableCheckpoint(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	settings.CurrentCheckpoint.bActive = false;

	Log("[CP] Current checkpoint disabled.");
}

void Pickup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int PickupID;
	PICKUP Pickup;

	bsData.Read(PickupID);
	bsData.Read((PCHAR)&Pickup, sizeof(PICKUP));

	if(settings.uiPickupsLogging != 0)
	{
		char szCreatePickupAlert[256];
		sprintf_s(szCreatePickupAlert, sizeof(szCreatePickupAlert), "[CREATEPICKUP] ID: %d | Model: %d | Type: %d | X: %.2f | Y: %.2f | Z: %.2f", PickupID, Pickup.iModel, Pickup.iType, Pickup.fX, Pickup.fY, Pickup.fZ);
		Log(szCreatePickupAlert);
	}
}

void DestroyPickup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int PickupID;

	bsData.Read(PickupID);

	if(settings.uiPickupsLogging != 0)
	{
		Log("[DESTROYPICKUP] %d", PickupID);
	}
}

void RequestClass(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BYTE byteRequestOutcome = 0;

	bsData.Read(byteRequestOutcome);

	if(byteRequestOutcome)
	{
		bsData.Read((PCHAR)&SpawnInfo,sizeof(PLAYER_SPAWN_INFO));

		iLocalPlayerSkin = SpawnInfo.iSkin;
	}
}

// RPC_Spawn (sunucu->istemci): sunucu oyuncuyu gercekten spawn ettigi an. Bu onay gelince
// /fr (freeroam) komutunu tam zamaninda gonderir. UIF'de oyuncu komutlari RPC_ServerCommand
// uzerinden islenir (RPC_Chat chat mesaji olarak echo edilir), bu yuzden sendServerCommand kullan.
// Not: /fr'yi erken (login/auth oncesi) gonderip g_frSentOnce'i yutma; buradaki tek gonderim,
// spawn menusu "Freeroam" secildikten sonraki GERCEK spawn onayina denk gelir.
void SpawnConfirmed(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	iSpawned = 1;
	bIsSpectating = 0;
	g_zoneScanWaitStart = GetTickCount(); // oto-tarama sayaci: 30 sn sonra ilk tarama

	if(!g_frSentOnce)
	{
		g_frSentOnce = true;
		Log("[SPAWN] Sunucu spawn onayi (RPC_Spawn) alindi, /fr (freeroam) gonderiliyor.");
		sendServerCommand((char *)"/fr");
	}
}

void ScrInitMenu(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	memset(&GTAMenu, 0, sizeof(struct stGTAMenu));

	BYTE byteMenuID;
	BOOL bColumns; // 0 = 1, 1 = 2
	CHAR cText[MAX_MENU_LINE];
	float fX;
	float fY;
	float fCol1;
	float fCol2 = 0.0;
	MENU_INT MenuInteraction;

	bsData.Read(byteMenuID);
	bsData.Read(bColumns);
	bsData.Read(cText, MAX_MENU_LINE);
	bsData.Read(fX);
	bsData.Read(fY);
	bsData.Read(fCol1);
	if (bColumns) bsData.Read(fCol2);
	bsData.Read(MenuInteraction.bMenu);
	for (BYTE i = 0; i < MAX_MENU_ITEMS; i++)
		bsData.Read(MenuInteraction.bRow[i]);

	Log("[MENU] %s", cText);
	strcpy(GTAMenu.szTitle, cText);

	BYTE byteColCount;
	bsData.Read(cText, MAX_MENU_LINE);
	Log("[MENU] %s", cText);
	strcpy(GTAMenu.szSeparator, cText);

	bsData.Read(byteColCount);
	GTAMenu.byteColCount = byteColCount;
	for (BYTE i = 0; i < byteColCount; i++)
	{
		bsData.Read(cText, MAX_MENU_LINE);
		Log("[MENU:%d] %s", i, cText);
		strcpy(GTAMenu.szColumnContent[i], cText);
	}

	if (bColumns)
	{
		bsData.Read(cText, MAX_MENU_LINE);
		//Log("4: %s", cText);

		bsData.Read(byteColCount);
		for (BYTE i = 0; i < byteColCount; i++)
		{
			bsData.Read(cText, MAX_MENU_LINE);
			//Log("5: %d %s", i, cText);
		}
	}
}

LRESULT CALLBACK SAMPDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndEditBox = GetDlgItem(hwnd, IDE_INPUTEDIT);
	HWND hwndListBox = GetDlgItem(hwnd, IDL_LISTBOX);
	WORD wSelection;
	char szResponse[257];

	switch(msg)
	{
	case WM_CREATE:
		{
			HINSTANCE hInst = GetModuleHandle(NULL);
			switch(sampDialog.bDialogStyle)
			{
				case DIALOG_STYLE_MSGBOX:
					if(sampDialog.bButton1Len == 0 && sampDialog.bButton2Len == 0)
					{
						// no butans, no badi cana cross it
					}
					if(sampDialog.bButton1Len != 0 && sampDialog.bButton2Len == 0) // a butan
					{
						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							150, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);
					}
					else if(sampDialog.bButton1Len != 0 && sampDialog.bButton2Len != 0) // tu butans
					{
						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							100, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							210, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON2, hInst, NULL);
					}

					break;

				case DIALOG_STYLE_INPUT:
				case DIALOG_STYLE_PASSWORD:
					{
						CreateWindowEx(NULL, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
							50, 200, 300, 24, hwnd, (HMENU)IDE_INPUTEDIT, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							100, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							210, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON2, hInst, NULL);
					}

					break;

				case DIALOG_STYLE_LIST:
					{
						hwndListBox = CreateWindowEx(NULL, "LISTBOX", "",
							WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER | LBS_HASSTRINGS,
							10, 10, 375, 225, hwnd, (HMENU)IDL_LISTBOX, hInst, NULL);

						char *szInfoTemp = strtok(sampDialog.szInfo, "\n");
						while(szInfoTemp != NULL)
						{
							int id = SendMessage(hwndListBox, LB_ADDSTRING, 0, (LPARAM)szInfoTemp);
							SendMessage(hwndListBox, LB_SETITEMDATA, id, (LPARAM)id);

							szInfoTemp = strtok(NULL, "\n");
						}
						
						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton1, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							100, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON1, hInst, NULL);

						CreateWindowEx(NULL, "BUTTON", sampDialog.szButton2, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
							210, 230, 100, 24, hwnd, (HMENU)IDB_BUTTON2, hInst, NULL);
					}

					break;
			}

		}
		break;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDB_BUTTON1:
					if(sampDialog.bDialogStyle == DIALOG_STYLE_LIST)
					{
						wSelection = (WORD)SendMessage(hwndListBox, LB_GETCURSEL, 0, 0);
						if(wSelection != (WORD)-1)
						{
							SendMessage(hwndListBox, LB_GETTEXT, wSelection, (LPARAM)szResponse);
							sendDialogResponse(sampDialog.wDialogID, 1, 0, szResponse);
							PostQuitMessage(0);
						}
						break;
					}

					GetWindowText(hwndEditBox, szResponse, 257);
					sendDialogResponse(sampDialog.wDialogID, 1, 0, szResponse);
					PostQuitMessage(0);
					break;

				case IDB_BUTTON2:
					GetWindowText(hwndEditBox, szResponse, 257);
					sendDialogResponse(sampDialog.wDialogID, 0, 0, szResponse);
					PostQuitMessage(0);
					break;
			}
		}

		break;

	case WM_PAINT:
		{
			if(sampDialog.bDialogStyle != DIALOG_STYLE_LIST)
			{
				RECT rect;
				GetClientRect(hwnd, &rect);
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				HDC hdcMem = CreateCompatibleDC(hdc);
				SelectObject(hdc, hSAMPDlgFont);
				DrawText(hdc, sampDialog.szInfo, strlen(sampDialog.szInfo), &rect, DT_WORDBREAK | DT_EXPANDTABS);
				DeleteDC(hdcMem);
				EndPaint(hwnd, &ps);
			}
			else
			{
				RECT rect;
				GetClientRect(hwnd, &rect);
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				HDC hdcMem = CreateCompatibleDC(hdc);
				DeleteDC(hdcMem);
				EndPaint(hwnd, &ps);
			}
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

DWORD WINAPI DialogBoxThread(PVOID)
{
	WNDCLASSEX wc;
	MSG Msg;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	RECT conRect;
	if(settings.iConsole)
		GetWindowRect(GetConsoleWindow(), &conRect);
	else
		GetWindowRect(hwnd, &conRect);

	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = SAMPDlgProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "dlgWndClass";
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	if(!RegisterClassEx(&wc))
		return 0;

	hSAMPDlgFont = CreateFont(18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma");

	hwndSAMPDlg = CreateWindowEx(NULL, "dlgWndClass", sampDialog.szTitle, NULL,
		conRect.right, conRect.top, 400, 300, NULL, NULL, hInstance, NULL);

	if(hwndSAMPDlg == NULL)
		return 0;

	ShowWindow(hwndSAMPDlg, 1);
	UpdateWindow(hwndSAMPDlg);
	SetForegroundWindow(hwndSAMPDlg);

	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	sampDialog.iIsActive = 0;
	SendMessage(hwndSAMPDlg, WM_DESTROY, 0, 0);
	DestroyWindow(hwndSAMPDlg);
	UnregisterClass("dlgWndClass", GetModuleHandle(NULL));
	hSAMPDlgFont = NULL;
	TerminateThread(hDlgThread, 0);

	return 0;
}

void ScrDialogBox(RPCParameters *rpcParams)
{
	if(!iGameInited) return;

	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(sampDialog.wDialogID);
	bsData.Read(sampDialog.bDialogStyle);

	bsData.Read(sampDialog.bTitleLength);
	bsData.Read(sampDialog.szTitle, sampDialog.bTitleLength);
	sampDialog.szTitle[sampDialog.bTitleLength] = 0;

	bsData.Read(sampDialog.bButton1Len);
	bsData.Read(sampDialog.szButton1, sampDialog.bButton1Len);
	sampDialog.szButton1[sampDialog.bButton1Len] = 0;

	bsData.Read(sampDialog.bButton2Len);
	bsData.Read(sampDialog.szButton2, sampDialog.bButton2Len);
	sampDialog.szButton2[sampDialog.bButton2Len] = 0;

	char szDlgInfo[2048] = {0};
	stringCompressor->DecodeString(szDlgInfo, sizeof(szDlgInfo), &bsData);

	// --- SPAWN MENUSU: "Where would you like to go?" otomatik Freeroam secimi ---
	// Spawn menusu (id=30, style 5 TABLIST_HEADERS) acildiginda "Freeroam" satirini
	// secip dialogu gondeririz. Bu sayede bot virtual map'e dusmez, gercek freeroam
	// bolgesine spawn olur. Bulunamazsa menu acik birakilir (asla kapatilmaz).
	bool bIsSpawnMenu = (strstr(sampDialog.szTitle, "Where would you like to go") != NULL);
	if(bIsSpawnMenu && (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
		sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST || sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS))
	{
		int iRow = 0, iItem = 0;
		int iHeaderRows = (sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS) ? 1 : 0;
		char *pLine = szDlgInfo;
		char *pNext = strchr(pLine, '\n');
		bool bSelected = false;
		while(pLine && *pLine)
		{
			int iLen = pNext ? (int)(pNext - pLine) : (int)strlen(pLine);
			if(iLen > 0)
			{
				char szRow[256];
				int iCopy = iLen < 255 ? iLen : 255;
				memcpy(szRow, pLine, iCopy);
				szRow[iCopy] = 0;
				while(iCopy > 0 && szRow[iCopy-1] == '\r') szRow[--iCopy] = 0;
				Log("[SPAWNMENU] row %d: %s", iRow + 1, szRow);
				if(iRow >= iHeaderRows && strstr(szRow, "Freeroam"))
				{
					iItem = iRow - iHeaderRows;
					Log("[SPAWNMENU] 'Freeroam' secildi (ListItem %d).", iItem);
					sendDialogResponse(sampDialog.wDialogID, 1, iItem, (char *)"Freeroam");
					sampDialog.iIsActive = 0;
					g_dwSpawnMenuHandled = GetTickCount();
					bSelected = true;

					// Freeroam secimi cevaplandigina gore /fr'yi tek sefer gonder.
					// UIF RPC_Spawn (52) gondermez (SpawnConfirmed tetiklenmez); guvenilir an
					// menunun cevaplandigi andir. g_frSentOnce bayragi toplam 1 gonderim
					// garantiler (cmd mute yok). RELIABLE_ORDERED sira garantisi sayesinde
					// sunucu once menu cevabini, sonra /fr'yi isler.
					if(!g_frSentOnce)
					{
						g_frSentOnce = true;
						Log("[SPAWN] Spawn menusu 'Freeroam' cevaplandi, /fr (freeroam) gonderiliyor.");
						sendServerCommand((char *)"/fr");
					}
					break;
				}
			}
			iRow++;
			if(!pNext) break;
			pLine = pNext + 1;
			pNext = strchr(pLine, '\n');
		}
		if(bSelected)
			return;
		Log("[SPAWNMENU] Freeroam satiri bulunamadi, menu acik birakildi.");
	}

	// /pnetstats ve /gstats dialog yakalamasi: yakalandiysa ekrana cizdirme
	if(HandleCapturedDialog(sampDialog.wDialogID, sampDialog.szTitle, szDlgInfo))
	{
		sampDialog.iIsActive = 0;
		return;
	}

	// --- GLOBAL AUTOCLOSE ---
	// Acik kalmasi zorunlu diyaloglar disindakileri otomatik kapat (ekranda birikmesin).
	// Acik kalanlar: Login/Giris/Authentication, spawn menusu (freeroam secilmediyse),
	// /astats icin account ID girisi (kullanici yazacak).
	bool bKeepOpen = false;
	if(strstr(sampDialog.szTitle, "Giris") || strstr(sampDialog.szTitle, "Login") ||
		strstr(sampDialog.szTitle, "Authentication"))
		bKeepOpen = true;
	else if(bIsSpawnMenu)
		bKeepOpen = true;
	else if(sampDialog.bDialogStyle == DIALOG_STYLE_INPUT && g_waitingForGStats && !g_gstatsAutoSubmit)
		bKeepOpen = true;

	if(!bKeepOpen)
	{
		if(strstr(sampDialog.szTitle, "Zone Info"))
		{
			Log("[GZINFO-DATA] '%s': %.330s", sampDialog.szTitle, szDlgInfo);
			// Her gzinfo dialogundan ogren (tarama aktif olmasa da): ?zf'nin kendi
			// /gzinfo'su su anki gercek sahibi getirir, eski/eski cache'i ezer.
			GangZoneScanGzDialog(szDlgInfo);
		}
		Log("[AUTOCLOSE] Dialog id=%d '%s' (style %d) kapatildi.", sampDialog.wDialogID, sampDialog.szTitle, sampDialog.bDialogStyle);
		sendDialogResponse(sampDialog.wDialogID, 0, 0xFFFF, (char *)"");
		sampDialog.iIsActive = 0;
		return;
	}

	strncpy(sampDialog.szInfo, szDlgInfo, 256);
	sampDialog.szInfo[256] = 0;

	switch(sampDialog.bDialogStyle)
	{
		case DIALOG_STYLE_MSGBOX:
		case DIALOG_STYLE_INPUT:
		case DIALOG_STYLE_LIST:
		case DIALOG_STYLE_PASSWORD:
			if(!sampDialog.iIsActive)
			{
				sampDialog.iIsActive = 1;
				hDlgThread = CreateThread(NULL, 0, DialogBoxThread, NULL, 0, NULL);
			}
		break;

		default:
			if(sampDialog.iIsActive)
			{
				sampDialog.iIsActive = 0;
				SendMessage(hwndSAMPDlg, WM_DESTROY, 0, 0);
				DestroyWindow(hwndSAMPDlg);
				UnregisterClass("dlgWndClass", GetModuleHandle(NULL));
				hSAMPDlgFont = NULL;
				TerminateThread(hDlgThread, 0);
			}
		break;
	}
}

void ScrGameText(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	char szMessage[400];
	int iType, iTime, iLength;

	bsData.Read(iType);
	bsData.Read(iTime);
	bsData.Read(iLength);

	if(iLength > 400) return; // tsk tsk, kye

	bsData.Read(szMessage, iLength);
	szMessage[iLength] = '\0';

	Log("[GAMETEXT] %s", szMessage);
}

void ScrPlayAudioStream(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned char bURLLen;
	char szURL[256];

	bsData.Read(bURLLen);
	bsData.Read(szURL, bURLLen);
	szURL[bURLLen] = 0;

	Log("[AUDIO_STREAM] %s", szURL);
}

void ScrSetDrunkLevel(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(iDrunkLevel);
}

void ScrHaveSomeMoney(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int iGivenMoney;
	bsData.Read(iGivenMoney);

	iMoney += iGivenMoney;
}

void ScrResetMoney(RPCParameters *rpcParams)
{
	iMoney = 0;
}

void ScrSetPlayerPos(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	if(settings.iNormalModePosForce == 0)
	{
		bsData.Read(settings.fNormalModePos[0]);
		bsData.Read(settings.fNormalModePos[1]);
		bsData.Read(settings.fNormalModePos[2]);
	}
}

void ScrSetPlayerFacingAngle(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	if(settings.iNormalModePosForce == 0)
	{
		bsData.Read(settings.fNormalModeRot);
	}
}

// RPC 108 (RPC_ScrAddGangZone): sunucu gang zone oluştururken minX,minY,maxX,maxY
// float'ları gönderir; zone ID client tarafında sıralı atanır. UIF zone numaraları
// (#0'dan itibaren, haritanın üst solundan) bu sırayla eşleşir -> ?zgo için kaydet.
void ScrAddGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	// Format (cozumlendi): WORD zoneId + 4 float (minX, minY, maxX, maxY) + 4 byte renk.
	unsigned short usZoneId;
	float fMinX, fMinY, fMaxX, fMaxY;
	unsigned int uiColor = 0;
	if(!bsData.Read(usZoneId) || !bsData.Read(fMinX) || !bsData.Read(fMinY) || !bsData.Read(fMaxX) || !bsData.Read(fMaxY))
		return;
	bsData.Read(uiColor); // renk (grup rengi, ?zf icin)

	if(usZoneId < MAX_GANG_ZONES)
	{
		g_zoneMinX[usZoneId] = fMinX;
		g_zoneMinY[usZoneId] = fMinY;
		g_zoneMaxX[usZoneId] = fMaxX;
		g_zoneMaxY[usZoneId] = fMaxY;
		g_zoneColor[usZoneId] = uiColor;
		if((int)usZoneId + 1 > g_zoneCount)
			g_zoneCount = (int)usZoneId + 1;

		// Esir-alma pending'i: bu zone rengi yeni sahibin rengine guncellendi mi?
		if(g_zwPendingZone == (int)usZoneId && uiColor != 0)
		{
			if(GetTickCount() - g_zwPendingTick < 30000)
				BindZoneGroup(usZoneId, g_zwPendingName);
			g_zwPendingZone = -1;
			g_zwPendingName[0] = 0;
		}
	}
	Log("[GANGZONE] id=%d x=[%.1f..%.1f] y=[%.1f..%.1f] color=0x%08X", usZoneId, fMinX, fMaxX, fMinY, fMaxY, uiColor);
}

// RPC 120 (RPC_ScrRemoveGangZone): zone kaldırılır. Listeyi sıfırlamayız çünkü
// zone numaraları (#N) sabittir; sadece gerekirse tekrar toplanır.
void ScrRemoveGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned int uiZoneID = 0;
	if(bsData.Read(uiZoneID))
		Log("[GANGZONE] zone kaldirildi: %u", uiZoneID);
}

// RPC 121 (RPC_ScrFlashGangZone): UIF `/gzinfo` ile zone'lar flaslanir. Paket
// zone ID + renk (grup rengi) tasir. ?zf icin hangi zone'larin flaslandigi
// (kimin elinde oldugu) buradan cikarilir. (4 float koordinat da gelebilir.)
void ScrFlashGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned short usZoneId = 0;
	unsigned int uiColor = 0;
	if(bsData.Read(usZoneId))
		bsData.Read(uiColor);
	g_zoneFlash[usZoneId] = true;
	Log("[GANGZONE:FLASH] id=%u color=0x%08X bytes=%d", usZoneId, uiColor, iBitLength/8);
}

// RPC 85 (RPC_ScrStopFlashGangZone): flaslama durdurulur.
void ScrStopFlashGangZone(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;
	if(iBitLength <= 0) return;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);
	unsigned short usZoneId = 0;
	if(bsData.Read(usZoneId))
	{
		g_zoneFlash[usZoneId] = false;
		Log("[GANGZONE:STOPFLASH] id=%u", usZoneId);
	}
}

void ScrSetSpawnInfo(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	PLAYER_SPAWN_INFO SpawnInfo;

	bsData.Read((PCHAR)&SpawnInfo, sizeof(PLAYER_SPAWN_INFO));

	if(settings.iNormalModePosForce == 0)
	{
		settings.fNormalModePos[0] = SpawnInfo.vecPos[0];
		settings.fNormalModePos[1] = SpawnInfo.vecPos[1];
		settings.fNormalModePos[2] = SpawnInfo.vecPos[2];
	}
}

void ScrSetPlayerHealth(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.fPlayerHealth);
}

void ScrSetPlayerArmour(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	bsData.Read(settings.fPlayerArmour);
}

void ScrSetPlayerSkin(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	int iPlayerID;
	unsigned int uiSkin;

	bsData.Read(iPlayerID);
	bsData.Read(uiSkin);

	if(iPlayerID < 0 || iPlayerID >= MAX_PLAYERS)
		return;

	if(iGameInited && g_myPlayerID == iPlayerID)
		iLocalPlayerSkin = uiSkin;
}

void ScrCreateObject(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	unsigned short ObjectID;
	bsData.Read(ObjectID);

	unsigned long ModelID;
	bsData.Read(ModelID);

	float vecPos[3];
	bsData.Read(vecPos[0]);
	bsData.Read(vecPos[1]);
	bsData.Read(vecPos[2]);

	float vecRot[3];
	bsData.Read(vecRot[0]);
	bsData.Read(vecRot[1]);
	bsData.Read(vecRot[2]);

	float fDrawDistance;
	bsData.Read(fDrawDistance);

	if(settings.uiObjectsLogging != 0)
	{
		char szCreateObjectAlert[256];
		sprintf_s(szCreateObjectAlert, sizeof(szCreateObjectAlert), "[OBJECT] %d, %d, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.2f", ObjectID, ModelID, vecPos[0], vecPos[1], vecPos[2], vecRot[0], vecRot[1], vecRot[2], fDrawDistance);
		Log(szCreateObjectAlert);
	}
}

void ScrCreate3DTextLabel(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD ID;
	CHAR Text[256];
	DWORD dwColor;
	FLOAT vecPos[3];
	FLOAT DrawDistance;
	BYTE UseLOS;
	WORD PlayerID;
	WORD VehicleID;

	bsData.Read((WORD)ID);
	bsData.Read((DWORD)dwColor);
	bsData.Read((FLOAT)vecPos[0]);
	bsData.Read((FLOAT)vecPos[1]);
	bsData.Read((FLOAT)vecPos[2]);
	bsData.Read((FLOAT)DrawDistance);
	bsData.Read((BYTE)UseLOS);
	bsData.Read((WORD)PlayerID);
	bsData.Read((WORD)VehicleID);

	stringCompressor->DecodeString(Text, 256, &bsData);

	if(settings.uiTextLabelsLogging != 0)
	{
		char szCreate3DTextLabelAlert[256];
		sprintf_s(szCreate3DTextLabelAlert, sizeof(szCreate3DTextLabelAlert), "[TEXTLABEL] %d - %s (%X, %.3f, %.3f, %.3f, %.2f, %i, %d, %d)", ID, Text, dwColor, vecPos[0], vecPos[1], vecPos[2], DrawDistance, UseLOS, PlayerID, VehicleID);
		Log(szCreate3DTextLabelAlert);
	}
}

// UIF savaş zamanlayıcı textdraw'ı: "~r~~h~4~w~-~b~~h~3 ~n~~w~08:27" -> "08:27" (dk:sn).
// Gerçek kalan süreyi yakalar; ?gzt bunu baz alir (chat duyuru anindan hesaplamak sagliksiz).
static void CaptureWarTimer(const char *pText, int iLen)
{
	if(!pText || !g_inZoneWar) return;
	const char *pColon = NULL;
	for(int i = 0; i < iLen - 1; i++)
	{
		if(pText[i] == ':' && i > 0 && pText[i+1] >= '0' && pText[i+1] <= '9')
		{ pColon = pText + i; break; }
	}
	if(!pColon) return;
	const char *p = pColon - 1;
	while(p >= pText && *p >= '0' && *p <= '9') p--;
	const char *pNum = p + 1;
	if(pNum >= pColon) return;
	int iM = atoi(pNum), iS = atoi(pColon + 1);
	if(iS < 0 || iS > 59 || iM < 0 || iM > 120) return;
	g_warRemainingSec = iM * 60 + iS;
	g_warTimerTick = GetTickCount();
	Log("[WAR:TIMER] Kalan sure yakalandi: %d:%02d (%d sn)", iM, iS, g_warRemainingSec);
}

void ScrShowTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	TEXT_DRAW_TRANSMIT TextDrawTransmit;

	CHAR cText[1024];
	unsigned short cTextLen = 0;

	bsData.Read(wTextID);
	bsData.Read((PCHAR)&TextDrawTransmit, sizeof(TEXT_DRAW_TRANSMIT));
	bsData.Read(cTextLen);
	bsData.Read(cText, cTextLen);
	cText[cTextLen] = '\0';

	CaptureWarTimer(cText, (int)cTextLen);

	if(settings.uiTextDrawsLogging != 0)
		SaveTextDrawData(wTextID, &TextDrawTransmit, cText);

	if(TextDrawTransmit.byteSelectable)
		Log("[SELECTABLE-TEXTDRAW] ID: %d, Text: %s.", wTextID, cText);

	// --- Kill list yakalayici: GZ bittikten sonra gelen sonuc textdraw'larini topla ---
	// GZ sonucu ekranda textdraw olarak gosterilir ("Group Zone War", "Score",
	// "Kills: X : Y", "Total", "MVP", "Top Feeder" vb.). Bu textdraw'lar biriktirilir,
	// KILL_LIST_CAPTURE_WINDOW_MS sonra dosyaya yazilip Discord'a gonderilir.
	if(g_zwCapturingKillList && !g_zwKillListSent)
	{
		// SAMP renk kodlarini temizle: {RRGGBB}
		std::string strClean;
		for(int i = 0; i < cTextLen; i++)
		{
			if(cText[i] == '{' && (i + 8) <= cTextLen && cText[i + 7] == '}')
			{
				i += 7;
				continue;
			}
			strClean += cText[i];
		}

		static const char *szMarkers[] = {
			"Kills", "Deaths", "Assist", "DMG", "Total", "MVP", "Score", "Group Zone War", "Top Feeder", "Player"
		};

		bool bIsKillList = false;
		for(int m = 0; m < (int)(sizeof(szMarkers) / sizeof(szMarkers[0])); m++)
		{
			if(strstr(strClean.c_str(), szMarkers[m]) != NULL)
			{
				bIsKillList = true;
				break;
			}
		}

		if(bIsKillList && !strClean.empty() && g_zwKillListText.find(strClean) == std::string::npos)
		{
			if(!g_zwKillListText.empty()) g_zwKillListText += "\n";
			g_zwKillListText += strClean;
			Log("[KILLLIST] Textdraw yakalandi (ID %d): %s", wTextID, strClean.c_str());
		}
	}
}

void ScrHideTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	bsData.Read(wTextID);

	if(settings.uiTextDrawsLogging != 0)
		Log("[TEXTDRAW:HIDE] ID: %d.", wTextID);
}

void ScrEditTextDraw(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	WORD wTextID;
	CHAR cText[1024];
	unsigned short cTextLen = 0;

	bsData.Read(wTextID);
	bsData.Read(cTextLen);
	bsData.Read(cText, cTextLen);
	cText[cTextLen] = '\0';

	CaptureWarTimer(cText, (int)cTextLen);

	if(settings.uiTextDrawsLogging != 0)
		Log("[TEXTDRAW:EDIT] ID: %d, Text: %s.", wTextID, cText);
}

void ScrTogglePlayerSpectating(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BOOL bToggle;

	bsData.Read(bToggle);

	if(bIsSpectating && !bToggle && !iSpawned)
	{
		sampSpawn();
		iSpawned = 1;
	}

	bIsSpectating = bToggle;
}

// Gamemode'dan gelen grup bilgisi: setPlayerWarGroup(playerID, ownGroup, enemyGroup)
void ScrSetWarGroup(RPCParameters *rpcParams)
{
	PCHAR Data = reinterpret_cast<PCHAR>(rpcParams->input);
	int iBitLength = rpcParams->numberOfBitsOfData;

	RakNet::BitStream bsData((unsigned char *)Data,(iBitLength/8)+1,false);

	BYTE byteOwnLen = 0;
	char szOwn[64] = {0};
	BYTE byteEnemyLen = 0;
	char szEnemy[64] = {0};

	bsData.Read(byteOwnLen);
	if(byteOwnLen > 63) return;
	bsData.Read(szOwn, byteOwnLen);
	szOwn[byteOwnLen] = '\0';

	bsData.Read(byteEnemyLen);
	if(byteEnemyLen > 63) return;
	bsData.Read(szEnemy, byteEnemyLen);
	szEnemy[byteEnemyLen] = '\0';

	// Oyun RPC'sinden gelen grup adi guvenilmez (kisa kod/ID olabiliyor, ornek: "NM").
	// Kendi grup adimiz chat mesajindan otomatik ogrenilir; burada ASLA ezilmez.
	// Dusman grup adini bosken al (welcome back icin); o da tam isim degilse sorun degil.
	if(g_enemyGroupName.empty())
		g_enemyGroupName = szEnemy;
	g_inZoneWar = (szEnemy[0] != '\0');

	Log("[WARGROUP] Kendi Grubu: %s | Dusman Grup: %s", szOwn, szEnemy);
}

// Sonucu "gz sonucl.txt" dosyasina yaz ve Discord'a gonder
void FinalizeKillList()
{
	if(g_zwKillListText.empty())
	{
		Log("[KILLLIST] Yakalanan sonuc metni bos, gonderim iptal.");
		g_zwCapturingKillList = false;
		g_zwKillListSent = true;
		return;
	}

	// Dosyaya yaz
	FILE *fl = fopen("gz sonucl.txt", "w");
	if(fl)
	{
		fprintf(fl, "%s\n", g_zwKillListText.c_str());
		fclose(fl);
		Log("[KILLLIST] Sonuc 'gz sonucl.txt' dosyasina yazildi.");
	}
	else
	{
		Log("[KILLLIST] 'gz sonucl.txt' dosyasi acilamadi.");
	}

	// Discord'a gonder
	SendKillList(g_zwKillListText);

	g_zwCapturingKillList = false;
	g_zwKillListSent = true;
}

// Ana donguden her iterasyonda cagrilir:
// 1. ?pl stats-post zaman asimi cevabi
// 2. Toplama penceresi dolunca sonucu finalize et (dosya + Discord)
// Not: Bot GZ'ye otomatik GIRMEZ (desync/kick riski); sonuc textdraw'lari savas
// bitisinde (wins/loses) disaridan yakalanir.
void ProcessZoneWarAutoJoin()
{
	DWORD dwNow = GetTickCount();

	// ?pl: Stats-post gecikmeli cevabi beklerken zaman asimi. Once taze Stats-post
	// dener (genelde ~1 sn icinde gelir); gelmezse cache'ten cevapla (fps yoksa "--").
	if(g_waitingForPNetStats && dwNow - g_pnetstatsReqTime > 3500)
	{
		g_waitingForPNetStats = false;
		if(g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS && g_playerCache[g_plTargetId].dLoss >= 0.0)
		{
			const char *pName = "Player ?";
			char szFallbackName[32];
			if(playerInfo[g_plTargetId].szPlayerName[0]) pName = playerInfo[g_plTargetId].szPlayerName;
			else if(g_playerCache[g_plTargetId].szName[0]) pName = g_playerCache[g_plTargetId].szName;
			else { sprintf_s(szFallbackName, sizeof(szFallbackName), "Player %d", g_plTargetId); pName = szFallbackName; }
			char szFps[16], szPing[16];
			if(g_playerCache[g_plTargetId].iFps >= 0) sprintf_s(szFps, sizeof(szFps), "%d", g_playerCache[g_plTargetId].iFps); else strcpy(szFps, "--");
			if(g_playerCache[g_plTargetId].iPing >= 0) sprintf_s(szPing, sizeof(szPing), "%d", g_playerCache[g_plTargetId].iPing); else strcpy(szPing, "--");
			char szOut[192];
			sprintf_s(szOut, sizeof(szOut), "!%s's PL: %.2f%% | FPS: %s | Ping: %s ms",
				pName, g_playerCache[g_plTargetId].dLoss, szFps, szPing);
			Log("[PNETSTATS] Zaman asimi, cache ile cevap: %s", szOut);
			g_bridgeSuppressEcho++;
			sendChat(szOut);
		}
		else
		{
			Log("[PNETSTATS] Stats-post ve cache yok, 'bulunamadi' cevabi.");
			g_bridgeSuppressEcho++;
			sendChat((char *)LM("!Packetloss bulunamadi.", "!Packetloss not found."));
		}
	}

	// Bot GZ'ye hic GIRMEZ (desync/kick riski; kullanici savas ortasinda kick yedi).
	// Sonuc textdraw'lari savas bitisinde (wins/loses) disaridan yakalanir (bkz. 1558).

	// Toplama penceresi doldu mu? -> finalize et
	if(g_zwCapturingKillList && !g_zwKillListSent)
	{
		if(dwNow - g_zwCaptureStart >= KILL_LIST_CAPTURE_WINDOW_MS)
			FinalizeKillList();
	}
}

// ?zgo: bekleme suresi doldu mu? -> eski (spawn) pozisyonuna don + gruba bildir.
void ProcessZgoReturn()
{
	if(!g_zgoActive) return;

	DWORD dwNow = GetTickCount();
	if(dwNow < g_zgoReturnTick) return;

	g_zgoActive = false;

	char szCmd[128];
	sprintf_s(szCmd, sizeof(szCmd), "/pos %.2f %.2f %.2f", g_zgoReturnPos[0], g_zgoReturnPos[1], g_zgoReturnPos[2]);
	Log("[ZGO] Bekleme suresi doldu, eski pozisyona donuluyor (%s).", szCmd);
	sendServerCommand(szCmd);

	char szMsg[128];
	if(g_zgoZoneId >= 0)
		sprintf_s(szMsg, sizeof(szMsg), LM("!Zone #%d beklemesi bitti, geri donuldu.", "!Zone #%d wait finished, returned back."), g_zgoZoneId);
	else
		sprintf_s(szMsg, sizeof(szMsg), LM("!Zone beklemesi bitti, geri donuldu.", "!Zone wait finished, returned back."));
	g_bridgeSuppressEcho++;
	sendChat(szMsg);
}

void RegisterRPCs(RakClientInterface *pRakClient)
{
	if (pRakClient == ::pRakClient)
	{
		// Core RPCs
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerJoin, ServerJoin);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ServerQuit, ServerQuit);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_InitGame, InitGame);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd, WorldPlayerAdd);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath, WorldPlayerDeath);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove, WorldPlayerRemove);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd, WorldVehicleAdd);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove, WorldVehicleRemove);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ConnectionRejected, ConnectionRejected);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ClientMessage, ClientMessage);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Chat, Chat);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs, UpdateScoresPingsIPs);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_SetCheckpoint, SetCheckpoint);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_DisableCheckpoint, DisableCheckpoint);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Pickup, Pickup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_DestroyPickup, DestroyPickup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_RequestClass, RequestClass);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_Spawn, SpawnConfirmed);

		// Scripting RPCs
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrInitMenu, ScrInitMenu);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDialogBox, ScrDialogBox);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText, ScrGameText);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_PlayAudioStream, ScrPlayAudioStream);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel, ScrSetDrunkLevel);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney, ScrHaveSomeMoney);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrResetMoney, ScrResetMoney);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos, ScrSetPlayerPos);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle, ScrSetPlayerFacingAngle);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo, ScrSetSpawnInfo);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth, ScrSetPlayerHealth);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour, ScrSetPlayerArmour);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin, ScrSetPlayerSkin);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreateObject, ScrCreateObject);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel, ScrCreate3DTextLabel);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw, ScrShowTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw, ScrHideTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw, ScrEditTextDraw);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating, ScrTogglePlayerSpectating);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrSetWarGroup, ScrSetWarGroup);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrAddGangZone, ScrAddGangZone);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone, ScrRemoveGangZone);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone, ScrFlashGangZone);
		pRakClient->RegisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone, ScrStopFlashGangZone);
	}
}

void UnRegisterRPCs(RakClientInterface * pRakClient)
{
	if (pRakClient == ::pRakClient)
	{
		// Core RPCs
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerJoin);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ServerQuit);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_InitGame);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerAdd);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerDeath);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldPlayerRemove);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleAdd);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_WorldVehicleRemove);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ConnectionRejected);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ClientMessage);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Chat);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_UpdateScoresPingsIPs);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_SetCheckpoint);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DisableCheckpoint);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Pickup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_DestroyPickup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_RequestClass);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_Spawn);

		// Scripting RPCs
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrInitMenu);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDialogBox);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrDisplayGameText);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_PlayAudioStream);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerDrunkLevel);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHaveSomeMoney);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrResetMoney);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerPos);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerFacingAngle);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetSpawnInfo);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerHealth);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerArmour);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetPlayerSkin);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreateObject);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrCreate3DTextLabel);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrShowTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrHideTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrEditTextDraw);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrTogglePlayerSpectating);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrSetWarGroup);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrAddGangZone);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrRemoveGangZone);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrFlashGangZone);
		pRakClient->UnregisterAsRemoteProcedureCall(&RPC_ScrStopFlashGangZone);
	}
}
