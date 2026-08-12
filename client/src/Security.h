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
// ?zf grup tag haritasi: /gstats sonucu harvest modunda islenip Discord'a gonderilmez
extern bool g_gstatsHarvestMode;
extern bool g_waitingForPNetStats;
extern unsigned long g_pnetstatsReqTime;
extern bool g_inZoneWar;
extern int g_plTargetId;
// ?gst : /gstats dialogundan zone + aktif oyuncu sayisi (Discord'a gitmez)
extern bool g_waitingForGst;
extern unsigned long g_gstReqTime;
extern char g_szNickName[32];
// /admins köprüsü (Discord /admins slash komutu)
extern bool g_waitingForAdmins;
extern unsigned long g_adminsReqTime;
extern std::string g_adminsRequesterId;

// Grup chat <-> Discord kopusu
extern bool g_groupChatEnabled;
extern int g_bridgeSuppressEcho;
// Welcome back ayarlari (MongoDB ile senkron)
extern std::string g_welcomeMessage;

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
void SendGStatsToDiscord(const char *szInfo);
void SendAdminsToDiscord(const char *szInfo);
void SendBackupCall(const char *playerName);
void SendZoneWarEvent(const char *action, const char *zoneName);
void SendWelcomeBack(const char *playerName);
void PollCommands();
void SendKillList(const std::string& killListText);
void ProcessZoneWarAutoJoin();
void ProcessZgoReturn();
void StartFrRetry();
void ProcessFrRetry();
void ToggleWelcomeBack();
void SetWelcomeMessage(const char *message);
void RefreshWelcomeState();
// ?zf grup tag haritasi
void CreateGroupHarvest();
void ProcessGroupHarvest();
void HarvestParseGstatsResult(const char *szInfo);
void BuildFlashZoneSummary();

// main.h icerisinde tanimli yardimcilar (Security.cpp'nin bunlari gormesi icin)
void sendServerCommand(char *szCommand);
int RunCommand(char *szCMD, int iFromConsole);

#endif