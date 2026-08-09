#include "Security.h"
#include <windows.h>
#include <intrin.h>
#include <iostream>
#include <sstream>
#include <string.h>

#include "httplib.h"
#include "json.hpp"

// misc_funcs.cpp icerisinde tanimli (main.h bagimliligini onlemek icin local decl)
void sendChat(char *szMessage);

using json = nlohmann::json;

// VDS Backend taban adresi (main.cpp handshake ile ayni kalmali)
static const std::string g_backendBase = "http://127.0.0.1:3000";

std::string g_myHWID = "";
std::string g_sessionToken = "";
std::string g_commandPrefix = "";
std::string g_myGroupId = "";
std::string g_myGroupName = "";
std::string g_enemyGroupName = "";
bool g_isEngineActive = false;
bool g_waitingForGStats = false;
unsigned long g_gstatsReqTime = 0;
// true = /gstats,/stats (ID dialogunda otomatik bos submit -> kendi grubu)
// false = /astats (kullanici account ID girer)
bool g_gstatsAutoSubmit = true;
// Discord slash komutundan gelen requesterId: sonuc o kullaniciya reply olarak gonderilir
std::string g_gstatsRequesterId = "";
// Komut parametresi (/gstats 5, /stats 5, /astats 5 gibi) — ID dialogu submit'inde kullanilir
std::string g_gstatsInput = "";
bool g_waitingForPNetStats = false;
unsigned long g_pnetstatsReqTime = 0;
bool g_inZoneWar = false;
int g_plTargetId = -1;
bool g_waitingForGst = false;
unsigned long g_gstReqTime = 0;
bool g_waitingForAdmins = false;
unsigned long g_adminsReqTime = 0;
std::string g_adminsRequesterId = "";

bool g_groupChatEnabled = false;
int g_bridgeSuppressEcho = 0;
std::string g_welcomeMessage = "";

static DWORD s_lastGroupChatPoll = 0;

// 1. Panoya HWID Kopyalama
void CopyToClipboard(const std::string& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

// 2. HWID Üretimi (CPU ID + Vol Serial)
std::string GenerateSecureHWID() {
    int cpuinfo[4] = { 0 };
    __cpuid(cpuinfo, 0);
    std::stringstream ss;
    ss << std::hex << cpuinfo[0] << cpuinfo[1] << cpuinfo[2] << cpuinfo[3];

    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);
    ss << std::hex << volSerial;

    std::string raw = ss.str();
    std::string hwid = "BURHAN-";
    for (size_t i = 0; i < raw.length() && i < 16; ++i) {
        if (i > 0 && i % 4 == 0) hwid += "-";
        hwid += toupper(raw[i]);
    }
    return hwid;
}

// 3. Sunucu Handshake
bool PerformSecurityHandshake(const std::string& serverUrl) {
    if (IsDebuggerPresent()) {
        ExitProcess(0);
    }

    g_myHWID = GenerateSecureHWID();

    httplib::Client cli(serverUrl);
    cli.set_connection_timeout(5);

    json requestBody = { {"hwid", g_myHWID} };
    auto res = cli.Post("/api/handshake", requestBody.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            if (data["success"] == true) {
                g_sessionToken = data["sessionToken"].get<std::string>();
                g_commandPrefix = data["config"]["commandPrefix"].get<std::string>();
                g_isEngineActive = true;

                std::cout << "-------------------------------------------\n";
                std::cout << "✅ [LISANS ONAYLANDI] Sunucuya baglaniliyor...\n";
                std::cout << "-------------------------------------------\n";
                return true;
            }
        }
        catch (...) {}
    }

    std::cout << "-------------------------------------------\n";
    std::cout << "❌ [LISANSSIZ CIHAZ] Erisim Yetkiniz Yok!\n";
    CopyToClipboard(g_myHWID);
    std::cout << "📋 HWID Pano'ya Kopyalandi: " << g_myHWID << "\n";
    std::cout << "💬 Discord uzerinden yoneticiye bu kodu iletin.\n";
    std::cout << "-------------------------------------------\n";

    Sleep(5000);
    ExitProcess(0);
    return false;
}

// 4. Oyun içi ?w (War Help) — Discord'a @everyone ile yardım çağrısı gönder
void TriggerWarHelp() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "")},
        {"ownGroup", g_myGroupName},
        {"enemyGroup", g_enemyGroupName}
    };

    auto res = cli.Post("/api/warhelp", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "⚔️ [WARHELP] Discord'a yardim cagrisi gonderildi!\n";
    } else {
        std::cout << "⚠️ [WARHELP] Discord'a gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 5. Oyun içi ?discordchat — MongoDB'deki köprü durumunu aç/kapat
void ToggleDiscordChat() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = { {"hwid", g_myHWID} };

    auto res = cli.Post("/api/groupchat/toggle", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            g_groupChatEnabled = data.value("enabled", false);
            if (g_groupChatEnabled) {
                std::cout << "💬 [GROUPCHAT] Discord grup chat köprüsü AÇILDI\n";
            } else {
                std::cout << "💬 [GROUPCHAT] Discord grup chat köprüsü KAPANDI\n";
            }
        } catch (...) {}
    } else {
        std::cout << "⚠️ [GROUPCHAT] Durum degistirilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 6. Oyun -> Discord: grup chat'inde ! ile baslayan kendi mesajimizi ilet
void SendGroupChatToDiscord(const char *text) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (text == NULL || text[0] == '\0') return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"author", std::string(g_szNickName ? g_szNickName : "Oyuncu")},
        {"text", text}
    };

    auto res = cli.Post("/api/groupchat", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            (void)data;
        } catch (...) {}
    }
}

// 7. Discord -> Oyun: bekleyen Discord mesajlarini cekip oyuna ilet (2 sn'de bir)
void PollDiscordChat() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = GetTickCount();
    if (s_lastGroupChatPoll != 0 && (dwNow - s_lastGroupChatPoll) < 2000) return;
    s_lastGroupChatPoll = dwNow;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(3);

    std::string url = "/api/groupchat/poll?hwid=" + g_myHWID;
    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        g_groupChatEnabled = data.value("enabled", g_groupChatEnabled);

        if (data.contains("messages") && data["messages"].is_array()) {
            for (auto &m : data["messages"]) {
                std::string text = m.value("text", "");
                if (text.empty()) continue;

                g_bridgeSuppressEcho++;
                std::string snd = "!" + text;
                char szBuf[300] = {0};
                strncpy_s(szBuf, sizeof(szBuf) - 1, snd.c_str(), sizeof(szBuf) - 1);
                sendChat(szBuf);
            }
        }
    } catch (...) {}
}

// 8. /gstats dialog verisini Discord gstats kanalina yolla (SAMPRPC.cpp'den cagrilir)
void SendGStatsToDiscord(const char *szInfo) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (szInfo == NULL) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"groupId", g_myGroupId},
        {"groupName", g_myGroupName},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"statsText", std::string(szInfo)},
        {"requesterId", g_gstatsRequesterId}
    };

    auto res = cli.Post("/api/gstats", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "📊 [GSTATS] Discord'a gonderildi!\n";
    } else {
        std::cout << "⚠️ [GSTATS] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }

    g_gstatsRequesterId = "";
}

// /admins dialog sonucunu Discord VDS'e aktar (requesterId'ye reply olarak)
void SendAdminsToDiscord(const char *szInfo) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (szInfo == NULL) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"adminsText", std::string(szInfo)},
        {"requesterId", g_adminsRequesterId}
    };

    auto res = cli.Post("/api/admins", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "🛡️ [ADMINS] Discord'a gonderildi!\n";
    } else {
        std::cout << "⚠️ [ADMINS] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }

    g_adminsRequesterId = "";
}

// 9. Oyun içi ?1414 — backup çağrısı → backup kanalına @here
void SendBackupCall() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"ownGroup", g_myGroupName}
    };

    auto res = cli.Post("/api/backup", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "🆘 [BACKUP] Discord'a backup cagrisi gonderildi!\n";
    } else {
        std::cout << "⚠️ [BACKUP] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 10. Oyun içi zone war olayı — keeps/takes/wins/loses → zonewar kanalına embed
void SendZoneWarEvent(const char *action, const char *zoneName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (action == NULL) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"ownGroup", g_myGroupName},
        {"enemyGroup", g_enemyGroupName},
        {"action", std::string(action)},
        {"zoneName", zoneName ? std::string(zoneName) : ""}
    };

    auto res = cli.Post("/api/zonewar", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "⚔️ [ZONEWAR] Discord'a zone war olayi gonderildi!\n";
    } else {
        std::cout << "⚠️ [ZONEWAR] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 11. Oyun içi welcome back — "X logged in" → welcome kanalına embed
void SendWelcomeBack(const char *playerName) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (playerName == NULL) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(playerName)},
        {"groupName", g_myGroupName},
        {"welcomeMessage", g_welcomeMessage}
    };

    auto res = cli.Post("/api/welcome", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            if (data.contains("enabled") && !data.value("enabled", true)) {
                std::cout << "Welcome back mesajlari kapali, bildirim atlandi.\n";
            }
        } catch (...) {}
        std::cout << "👋 [WELCOME] Discord'a welcome back gonderildi!\n";
    } else {
        std::cout << "⚠️ [WELCOME] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 11a. ?greet — welcome back bildirimlerini aç/kapat (MongoDB'de saklanır)
void ToggleWelcomeBack() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = { {"hwid", g_myHWID} };

    auto res = cli.Post("/api/welcome/toggle", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            bool enabled = data.value("enabled", true);
            std::cout << (enabled ? "👋 [WELCOME] Welcome back mesajlari AÇILDI\n" : "👋 [WELCOME] Welcome back mesajlari KAPANDI\n");
        } catch (...) {}
    } else {
        std::cout << "⚠️ [WELCOME] Durum degistirilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 11b. ?welcomemsg <mesaj> — kişiye özel welcome mesajı ayarla (MongoDB'de saklanır)
void SetWelcomeMessage(const char *message) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"welcomeMessage", message ? std::string(message) : ""}
    };

    auto res = cli.Post("/api/welcome/message", body.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto data = json::parse(res->body);
            g_welcomeMessage = data.value("welcomeMessage", g_welcomeMessage);
            std::cout << "✉️ [WELCOME] Kisisele welcome mesaji ayarlandi: " << g_welcomeMessage << "\n";
        } catch (...) {}
    } else {
        std::cout << "⚠️ [WELCOME] Mesaj ayarlanamadi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}

// 11c. Backend'den welcome ayarlarini cek (PollCommands'a benzer, ana donguden cagrilir)
void RefreshWelcomeState() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = GetTickCount();
    static DWORD s_lastWelcomePoll = 0;
    if (s_lastWelcomePoll != 0 && (dwNow - s_lastWelcomePoll) < 10000) return;
    s_lastWelcomePoll = dwNow;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(3);

    std::string url = "/api/welcome/state?hwid=" + g_myHWID;
    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        g_welcomeMessage = data.value("welcomeMessage", g_welcomeMessage);
    } catch (...) {}
}

// 12. Discord -> Oyun: /send ve /gstats komutlarini kuyruktan cekip oyunda calistir (2 sn'de bir)
void PollCommands() {
    if (!g_isEngineActive || g_sessionToken.empty()) return;

    DWORD dwNow = GetTickCount();
    static DWORD s_lastCommandPoll = 0;
    if (s_lastCommandPoll != 0 && (dwNow - s_lastCommandPoll) < 2000) return;
    s_lastCommandPoll = dwNow;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(3);

    std::string url = "/api/command/poll?hwid=" + g_myHWID;
    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return;

    try {
        auto data = json::parse(res->body);
        if (data.contains("commands") && data["commands"].is_array()) {
            for (auto &c : data["commands"]) {
                std::string type = c.value("type", "command");
                std::string cmd = c.value("cmd", "");
                std::string requesterId = c.value("requesterId", "");

                if (type == "gstats") {
                    g_waitingForGStats = true;
                    g_gstatsReqTime = GetTickCount();
                    g_gstatsRequesterId = requesterId;
                    g_gstatsAutoSubmit = true;
                    std::cout << "📊 [COMMAND] Discord'dan /gstats istendi (" << cmd << ")\n";
                    sendServerCommand((char *)cmd.c_str());
                } else if (type == "admins") {
                    g_waitingForAdmins = true;
                    g_adminsReqTime = GetTickCount();
                    g_adminsRequesterId = requesterId;
                    std::cout << "🛡️ [COMMAND] Discord'dan /admins istendi (" << cmd << ")\n";
                    sendServerCommand((char *)cmd.c_str());
                } else if (!cmd.empty()) {
                    std::cout << "🎮 [COMMAND] Discord'dan komut geldi: " << cmd << "\n";
                    RunCommand((char *)cmd.c_str(), 1);
                }
            }
        }
    } catch (...) {}
}

// 13. GZ sonucu (kill list) — "gz sonucl.txt" dosyasindan okunan metni Discord zonewar kanalina embed olarak yolla
void SendKillList(const std::string& killListText) {
    if (!g_isEngineActive || g_sessionToken.empty()) return;
    if (killListText.empty()) return;

    httplib::Client cli(g_backendBase);
    cli.set_connection_timeout(5);

    json body = {
        {"hwid", g_myHWID},
        {"playerName", std::string(g_szNickName ? g_szNickName : "Bir oyuncu")},
        {"ownGroup", g_myGroupName},
        {"enemyGroup", g_enemyGroupName},
        {"killList", killListText}
    };

    auto res = cli.Post("/api/killlist", body.dump(), "application/json");
    if (res && res->status == 200) {
        std::cout << "⚔️ [KILLLIST] Discord'a GZ sonucu gonderildi!\n";
    } else {
        std::cout << "⚠️ [KILLLIST] Gonderilemedi (HTTP " << (res ? res->status : -1) << ")\n";
    }
}