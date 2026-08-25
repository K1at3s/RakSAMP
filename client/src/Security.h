#ifndef SECURITY_H
#define SECURITY_H

#include <string>

// Küresel Değişkenler
extern std::string g_myHWID;
extern std::string g_sessionToken;
extern std::string g_commandPrefix;
extern std::string g_myGroupId;
extern std::string g_myGroupName;
extern std::string g_enemyGroupName;
extern bool g_isEngineActive;
extern bool g_waitingForGStats;
extern unsigned long g_gstatsReqTime;
extern bool g_gstatsAutoSubmit;
extern bool g_gstatsSentFirst;
extern std::string g_gstatsRequesterId;
extern std::string g_gstatsInput;
extern bool g_waitingForPNetStats;
extern unsigned long g_pnetstatsReqTime;
extern bool g_inZoneWar;
extern int g_plTargetId;
// ?pl istatistik cache'i: YS.Hold'un "Player [X] Stats -> ..." mesajlarindan doldurulur
// (/pnetstats FPS icermez). player_cache.txt'ye kalici yazilir (restart sonrasi isim+FPS
// bilinir). Boyut: SA-MP max 1000 oyuncu (MAX_PLAYERS ile ayni; Security.cpp main.h
// bagimliligi olmasin diye burada sabit).
#define PLAYER_CACHE_SIZE 1000
struct stPlayerCache { char szName[64]; double dLoss; int iFps; int iPing; unsigned long dwTime; };
extern stPlayerCache g_playerCache[PLAYER_CACHE_SIZE];
void LoadPlayerCache();
// ?gst : /gstats dialogundan zone + aktif oyuncu sayisi (Discord'a gitmez)
extern bool g_waitingForGst;
extern unsigned long g_gstReqTime;
extern char g_szNickName[32];
// /admins köprüsü (Discord /admins slash komutu)
extern bool g_waitingForAdmins;
extern unsigned long g_adminsReqTime;
extern std::string g_adminsRequesterId;
// /groups sonuc dialogu bekleyen flag (?zgrp harvest)
extern bool g_waitingForGroups;
// /gmembers sonuc dialogu bekleyen flag (?zsnip yetki kontrolu)
extern bool g_waitingForGmembers;
extern unsigned long g_gmembersReqTime;
extern unsigned long g_groupsReqTime;

// Grup chat <-> Discord kopusu
extern bool g_groupChatEnabled;
extern int g_bridgeSuppressEcho;
// Welcome back ayarlari (MongoDB ile senkron)
extern std::string g_welcomeMessage;
// Bot mesaj dili: false=English (varsayilan), true=Turkce. MongoDB'de (Guild.lang) saklanir,
// polling ile senkron; ?tr/?en oyunda degistirir.
extern bool g_langTurkish;
void SaveLang(); // bot_lang.txt'ye yazar (netrpc.cpp; Security.cpp poll senkronu kullanir)

// Fonksiyonlar
void HandleGroupCommand(const char *pGroupMsg, int iSenderId);
std::string GenerateSecureHWID();
void CopyToClipboard(const std::string& text);
bool PerformSecurityHandshake(const std::string& serverUrl);
void TriggerWarHelp(const char *playerName);
int HandleCapturedDialog(int wDialogID, const char *szTitle, const char *szInfo);
void ToggleDiscordChat();
void SendGroupChatToDiscord(const char *text);
void PollDiscordChat();
// ?tr/?en: dili MongoDB'ye (Guild.lang) yazar; polling bu degeri geri okur.
void ServerSetBotLang(bool turkish);
// ?guildid: bu HWID'i Discord sunucusuna (guildId) baglar; basariliysa true.
bool ServerSetGuildId(const char *guildId);
void SendGStatsToDiscord(const char *szInfo);
void SendAdminsToDiscord(const char *szInfo);
void SendBackupCall(const char *playerName);
void SendWelcomeBack(const char *playerName);
void PollCommands();
void SendKillList(const std::string& killListText);
void ProcessZoneWarAutoJoin();
void ProcessZgoReturn();
void ProcessZtkTick();
// ?zgrp grup ogrenme harvester'i: /groups + /gstats <ID> zinciri
void OnGroupsListCaptured(const char *szInfo);
int  OnGmembersPage(const char *szInfo, int *outButton, int *outItem); // 0=done, 1=next
void OnGmembersFinalize();
void GmembersReset();
void ProcessGhpHarvestStep();
bool OnGStatsResultForHarvest(const char *szInfo);
// Cache + grup renklerini backend'e (MySQL) yazar; main loop 60 sn'de bir çağırır
void ProcessDbSync();
void StartFrRetry();
void ProcessFrRetry();
void ToggleWelcomeBackEx(int bForceOnOff); // -1=toggle, 0=off, 1=on; sonucu gruba bildirir
void SetWelcomeMessage(const char *message);
void RefreshWelcomeState();
// ?zf grup tag haritasi
void ProcessZoneScan();
void BuildFlashZoneSummary();
void ProcessZfFlashSummary();
// /gstats sonucundan "- Tag: [XX]" cekip grup etiketini gunceller
bool LearnGroupTagFromGStats(const char *szInfo);

// main.h icerisinde tanimli yardimcilar (Security.cpp'nin bunlari gormesi icin)
void sendServerCommand(char *szCommand);
int RunCommand(char *szCMD, int iFromConsole);

// misc_funcs.cpp: cikis chat kuyrugu (5 sn arayla gonderim) — main loop her tick cagirir
void ProcessChatOutQueue();

#endif