/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "Security.h"
#include <iostream>

int RPC_ServerJoin = 137;
int RPC_ServerQuit = 138;
int RPC_InitGame = 139;
int RPC_ClientJoin = 25;
int RPC_NPCJoin = 54;
int RPC_Death = 53;
int RPC_RequestClass = 128;
int RPC_RequestSpawn = 129;
int RPC_SetInteriorId = 118;
int RPC_Spawn = 52;
int RPC_Chat = 101;
int RPC_EnterVehicle = 26;
int RPC_ExitVehicle = 154;
int RPC_DamageVehicle = 106;
int RPC_MenuSelect = 132;
int RPC_MenuQuit = 140;
int RPC_ScmEvent = 96;
int RPC_AdminMapTeleport = 255;
int RPC_WorldPlayerAdd = 32;
int RPC_WorldPlayerDeath = 166;
int RPC_WorldPlayerRemove = 163;
int RPC_WorldVehicleAdd = 164;
int RPC_WorldVehicleRemove = 165;
int RPC_SetCheckpoint = 107;
int RPC_DisableCheckpoint = 37;
int RPC_SetRaceCheckpoint = 38;
int RPC_DisableRaceCheckpoint = 39;
int RPC_UpdateScoresPingsIPs = 155;
int RPC_SvrStats = 102;
int RPC_GameModeRestart = 40;
int RPC_ConnectionRejected = 130;
int RPC_ClientMessage = 93;
int RPC_WorldTime = 94;
int RPC_Pickup = 95;
int RPC_DestroyPickup = 63;
int RPC_DestroyWeaponPickup = 97;
int RPC_Weather = 152;
int RPC_SetTimeEx = 255;
int RPC_ToggleClock = 30;
int RPC_ServerCommand = 50;
int RPC_PickedUpPickup = 131;
int RPC_PickedUpWeapon = 255;
int RPC_VehicleDestroyed = 136;
int RPC_DialogResponse = 62;
int RPC_PlayAudioStream = 41;
int RPC_StopAudioStream = 42;
int RPC_ClickPlayer = 23;
int RPC_PlayerUpdate = 60;
int RPC_ClickTextDraw = 83;
int RPC_MapMarker = 119;
int RPC_PlayerGiveTakeDamage = 115;
int RPC_EnterEditObject = 27;
int RPC_EditObject = 117;

int RPC_ScrSetSpawnInfo = 68;
int RPC_ScrSetPlayerTeam = 69;
int RPC_ScrSetPlayerSkin = 153;
int RPC_ScrSetPlayerName = 11;
int RPC_ScrSetPlayerPos = 12;
int RPC_ScrSetPlayerPosFindZ = 13;
int RPC_ScrSetPlayerHealth = 14;
int RPC_ScrPutPlayerInVehicle = 70;
int RPC_ScrRemovePlayerFromVehicle = 71;
int RPC_ScrSetPlayerColor = 72;
int RPC_ScrDisplayGameText = 73;
int RPC_ScrSetInterior = 156;
int RPC_ScrSetCameraPos = 157;
int RPC_ScrSetCameraLookAt = 158;
int RPC_ScrSetVehiclePos = 159;
int RPC_ScrSetVehicleZAngle = 160;
int RPC_ScrVehicleParams = 161;
int RPC_ScrSetCameraBehindPlayer = 162;
int RPC_ScrTogglePlayerControllable = 15;
int RPC_ScrPlaySound = 16;
int RPC_ScrSetWorldBounds = 17;
int RPC_ScrHaveSomeMoney = 18;
int RPC_ScrSetPlayerFacingAngle = 19;
int RPC_ScrResetMoney = 20;
int RPC_ScrResetPlayerWeapons = 21;
int RPC_ScrGivePlayerWeapon = 22;
int RPC_ScrRespawnVehicle = 255;
int RPC_ScrLinkVehicle = 65;
int RPC_ScrSetPlayerArmour = 66;
int RPC_ScrDeathMessage = 55;
int RPC_ScrSetMapIcon = 56;
int RPC_ScrDisableMapIcon = 144;
int RPC_ScrSetWeaponAmmo = 145;
int RPC_ScrSetGravity = 146;
int RPC_ScrSetVehicleHealth = 147;
int RPC_ScrAttachTrailerToVehicle = 148;
int RPC_ScrDetachTrailerFromVehicle = 149;
int RPC_ScrCreateObject = 44;
int RPC_ScrSetObjectPos = 45;
int RPC_ScrSetObjectRotation = 46;
int RPC_ScrDestroyObject = 47;
int RPC_ScrCreateExplosion = 79;
int RPC_ScrShowNameTag = 80;
int RPC_ScrMoveObject = 99;
int RPC_ScrStopObject = 122;
int RPC_ScrNumberPlate = 123;
int RPC_ScrTogglePlayerSpectating = 124;
int RPC_ScrSetPlayerSpectating = 255;
int RPC_ScrPlayerSpectatePlayer = 126;
int RPC_ScrPlayerSpectateVehicle = 127;
int RPC_ScrRemoveComponent = 57;
int RPC_ScrForceSpawnSelection = 74;
int RPC_ScrAttachObjectToPlayer = 75;
int RPC_ScrInitMenu = 76;
int RPC_ScrShowMenu = 77;
int RPC_ScrHideMenu = 78;
int RPC_ScrSetPlayerWantedLevel = 133;
int RPC_ScrShowTextDraw = 134;
int RPC_ScrHideTextDraw = 135;
int RPC_ScrEditTextDraw = 105;
int RPC_ScrAddGangZone = 108;
int RPC_ScrRemoveGangZone = 120;
int RPC_ScrFlashGangZone = 121;
int RPC_ScrStopFlashGangZone = 85;
int RPC_ScrApplyAnimation = 86;
int RPC_ScrClearAnimations = 87;
int RPC_ScrSetSpecialAction = 88;
int RPC_ScrEnableStuntBonus = 104;
int RPC_ScrSetFightingStyle = 89;
int RPC_ScrSetPlayerVelocity = 90;
int RPC_ScrSetVehicleVelocity = 91;
int RPC_ScrToggleWidescreen = 255;
int RPC_ScrSetVehicleTireStatus = 255;
int RPC_ScrSetPlayerDrunkLevel = 35;
int RPC_ScrDialogBox = 61;
int RPC_ScrCreate3DTextLabel = 36;
int RPC_ScrSetWarGroup = 175;

// Anahtar kelimeden sonraki ilk tam sayiyi cikarir (ornek: "Zone: 5" -> 5)
static int ExtractNumberAfter(const char *pszText) {
    if (!pszText) return -1;
    for (const char *p = pszText; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            int val = 0;
            while (*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
                p++;
            }
            return val;
        }
    }
    return -1;
}

// /pnetstats dialog gövdesinden "Packetloss: X%" değerini çıkarır
static double ExtractPacketLoss(const char *szText) {
    if (!szText) return -1.0;
    int len = (int)strlen(szText);

    const char *keys[] = { "loss", "lost", "packet", "paket", "kay", "ploss" };
    int bestPos = -1;
    for (int k = 0; k < 6; k++) {
        const char *p = strstr(szText, keys[k]);
        if (p) {
            int pos = (int)(p - szText);
            if (bestPos == -1 || pos < bestPos) bestPos = pos;
        }
    }

    int start, end;
    if (bestPos == -1) { start = 0; end = len; }
    else { start = bestPos - 20; if (start < 0) start = 0; end = bestPos + 30; if (end > len) end = len; }

    for (int i = start; i < end; i++) {
        if (szText[i] == '%') {
            int j = i - 1;
            while (j >= start && ((szText[j] >= '0' && szText[j] <= '9') || szText[j] == '.' || szText[j] == ','))
                j--;
            if (j + 1 < i) {
                char num[32]; int n = 0;
                for (int q = j + 1; q < i && n < 31; q++) num[n++] = szText[q];
                num[n] = 0;
                return atof(num);
            }
        }
    }
    return -1.0;
}

// Genel sayi cikarma: metinde key'e yakin ilk sayiyi dondurur (yoksa -1.0).
// Ornek: "FPS: 60" -> 60, "Ping 45 ms" -> 45. Once ileri, olmazsa geriye arar.
static double ExtractNumberNear(const char *szText, const char *key) {
    if (!szText || !key) return -1.0;
    int len = (int)strlen(szText);
    const char *p = strstr(szText, key);
    if (!p) return -1.0;

    const char *q = p - 30; if (q < szText) q = szText;
    const char *end = p + (int)strlen(key) + 30; if (end > szText + len) end = szText + len;

    for (const char *r = p + (int)strlen(key); r < end; r++) {
        if (*r >= '0' && *r <= '9') {
            const char *s = r;
            while (s > q && ((*(s - 1) >= '0' && *(s - 1) <= '9') || *(s - 1) == '.')) s--;
            double d = atof(s);
            if (d >= 0.0) return d;
            break;
        }
    }
    for (const char *r = p - 1; r >= q; r--) {
        if (*r >= '0' && *r <= '9') {
            const char *s = r;
            while (s > q && ((*(s - 1) >= '0' && *(s - 1) <= '9') || *(s - 1) == '.')) s--;
            double d = atof(s);
            if (d >= 0.0) return d;
            break;
        }
    }
    return -1.0;
}

// netrpc.cpp'deki ScrDialogBox'tan cagrilir: dialog dogru decode edildikten sonra
// /pnetstats ve /gstats yakalamasini yapar. Yakaladiysa 1, yakalamadiysa 0 dondurur.
int HandleCapturedDialog(int wDialogID, const char* szTitle, const char* szInfo) {
#ifdef RAKSAMP_CLIENT
    if (!g_isEngineActive || g_sessionToken.empty()) return 0;

    // -------------------------------------------------------------
    // 1. GİRİŞ (LOGIN) EKRANI KORUMASI
    // Giriş ekranı geldiğinde diyalogu KAPANMAKTAN korur (0 döndürür).
    // -------------------------------------------------------------
    if (szTitle && (strstr(szTitle, "Login") || strstr(szTitle, "Giris") || strstr(szTitle, "Giriş") || strstr(szTitle, "Authentication"))) {
        std::cout << "[INFO] Giriş ekranı algılandı, diyalog kapatılmadı.\n";
        return 0; // 0 dönerek diyalogu kapatmaz, RakSAMP'ın normal akışına bırakır
    }

    // -------------------------------------------------------------
    // 2. /pnetstats YAKALAMASI (?pl)
    // -------------------------------------------------------------
    if (g_waitingForPNetStats) {
        if (GetTickCount() - g_pnetstatsReqTime > 15000) {
            g_waitingForPNetStats = false;
            return 0;
        }

        double dLoss = ExtractPacketLoss(szInfo);
        double dFps = ExtractNumberNear(szInfo, "fps");
        double dPing = ExtractNumberNear(szInfo, "ping");
        // /stats <id> dialogu FPS/ping iceriyorsa cache'e yaz: ?pl cevabinda bu taze
        // veriler gosterilir (kullanici: "stats'tan fps+ping cek, cache'e save, ?pl'de yaz").
        if (g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS) {
            if (dFps >= 0.0) {
                g_playerCache[g_plTargetId].iFps = (int)dFps;
                g_playerCache[g_plTargetId].dwTime = GetTickCount();
                Log("[PNETSTATS] /stats'tan FPS cache'e yazildi: hedef %d, fps %d", g_plTargetId, (int)dFps);
            }
            if (dPing >= 0.0) {
                g_playerCache[g_plTargetId].iPing = (int)dPing;
                g_playerCache[g_plTargetId].dwTime = GetTickCount();
                Log("[PNETSTATS] /stats'tan Ping cache'e yazildi: hedef %d, ping %d", g_plTargetId, (int)dPing);
            }
        }
        // Erteleme kontrolu merge ONCESI yapilir: cache merge boslugu doldurup
        // (cache dLoss=0.0) ertelemeyi engellememeli. (Erteleme amaci: taze
        // Stats-post'tan gercek FPS almak; cache bayat/deger yok olabilir.)
        bool bDialogEmpty = (dLoss < 0.0);
        if (dPing < 0.0 && g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS)
            dPing = (double)playerInfo[g_plTargetId].dwPing; // scoreboard ping'i

        // UIF /pnetstats FPS icermez (FPS: --) ve kullaniciya ait loss/ping dialogda
        // olmayabilir. "Stats ->" grup mesajlarindan doldurulan cache'ten eksikleri
        // tamamla (netrpc.cpp doldurur, player_cache.txt'ye kalici yazar).
        if (g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS && g_playerCache[g_plTargetId].dwTime != 0) {
            if (dLoss < 0.0 && g_playerCache[g_plTargetId].dLoss >= 0.0)
                dLoss = g_playerCache[g_plTargetId].dLoss;
            if (dFps < 0.0 && g_playerCache[g_plTargetId].iFps >= 0)
                dFps = (double)g_playerCache[g_plTargetId].iFps;
            if (dPing < 0.0 && g_playerCache[g_plTargetId].iPing >= 0)
                dPing = (double)g_playerCache[g_plTargetId].iPing;
        }

        Log("[PNETSTATS-DATA] [%s] %.1500s", szTitle ? szTitle : "?", szInfo);

        // UIF /pnetstats "Network Active: 1" gibi BOS doner (loss/fps yok). Asil
        // veri /pnetstats'in tetikledigi "Player [X] Stats ->" grup mesajindadir.
        // Dialogda loss yoksa cevabi O yakalayiciya birak: flag ACIK kalir, dialogu
        // kapat; Stats-post gelince netrpc.cpp taze veriyle cevaplar (cache yoluyla
        // aninda eski FPS gostermek yerine). Zaman asimini ProcessZoneWarAutoJoin
        // toparlar (cache fallback'li).
        if (bDialogEmpty) {
            Log("[PNETSTATS] Dialog veri icermiyor, 'Stats ->' bekleniyor (hedef %d).", g_plTargetId);
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)0);        // Button 0 (İptal/Kapat)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint8_t)0);        // Input Text Length
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            std::cout << "[AUTOCLOSE] pnetstats (bos) diyalogu kapatildi, Stats-> bekleniyor\n";
            return 1; // g_waitingForPNetStats ACIK kalir
        }

        g_waitingForPNetStats = false;

        char szResult[192];
        // Hedef adi: canli grup basligindan ogrenilen ad -> cache'teki ad -> "Player <id>".
        // Botun kendi adi (g_szNickName) SADECE hedef botun kendisiyse kullanilir; ?pl <id>
        // cevabi "hakixld's PL" gibi yanlis isimle donemez.
        char szFallbackName[32];
        const char *pPlName = "Player ?";
        if (g_plTargetId >= 0 && g_plTargetId < MAX_PLAYERS) {
            if (playerInfo[g_plTargetId].szPlayerName[0]) {
                pPlName = playerInfo[g_plTargetId].szPlayerName;
            } else if (g_playerCache[g_plTargetId].szName[0]) {
                pPlName = g_playerCache[g_plTargetId].szName;
            } else if (g_plTargetId == (int)g_myPlayerID) {
                pPlName = g_szNickName;
            } else {
                sprintf_s(szFallbackName, sizeof(szFallbackName), "Player %d", g_plTargetId);
                pPlName = szFallbackName;
            }
        }
        if (dLoss >= 0.0) {
            unsigned int uiFps = (dFps >= 0.0 && dFps <= 1000.0) ? (unsigned int)dFps : 0;
            int iPing = (dPing >= 0.0 && dPing <= 30000.0) ? (int)dPing : -1;
            char szFps[16], szPing[16];
            sprintf_s(szFps, sizeof(szFps), "%u", uiFps);
            sprintf_s(szPing, sizeof(szPing), "%d", iPing);
            if (iPing < 0) strcpy(szPing, "--");
            if (dFps < 0.0) strcpy(szFps, "--");
            sprintf_s(szResult, sizeof(szResult), "!%s's PL: %g%% | FPS: %s | Ping: %s ms",
                pPlName, dLoss, szFps, szPing);
        }
        else {
            sprintf_s(szResult, sizeof(szResult), "!Packetloss bulunamadi.");
        }

        std::cout << "[pnetstats yakalandi] " << szResult << "\n";

        // Sonucu sadece OYUNDA goster; Discord koprusune yansitma
        // (! ile basladigi icin kopruye duserdi; echo bastirilarak onlenir).
        g_bridgeSuppressEcho++;
        sendChat(szResult);

        // pnetstats diyalogunu ekrana çizdirmeden kapat (RPC 62)
        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)0);        // Button 0 (İptal/Kapat)
        bsResponse.Write((uint16_t)0xFFFF);   // Item ID
        bsResponse.Write((uint8_t)0);        // Input Text Length
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

        std::cout << "[AUTOCLOSE] pnetstats diyalogu kapatildi!\n";
        return 1;
    }

    // -------------------------------------------------------------
    // 3. /gstats YAKALAMASI (?gst - zone + aktif oyuncu sayisi)
    // -------------------------------------------------------------
    if (g_waitingForGst) {
        if (g_gstReqTime != 0 && GetTickCount() - g_gstReqTime > 20000) {
            g_waitingForGst = false;
            Log("[GST] Bekleme zamanaşımı, flag sıfırlandı.");
            return 0;
        }

        if (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT || sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD) {
            const char *pszInput = "";
            size_t nInputLen = 0;
            Log("[GST] ID giris dialogu algilandi, bos submit gonderiliyor (kendi grubu)...");
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);        // Button 1 (OK)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint16_t)nInputLen); // Input Text Length
            if(nInputLen > 0)
                bsResponse.Write(pszInput, nInputLen);
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            return 1;
        }

        g_waitingForGst = false;
        Log("[GST] Sonuc dialogu yakalandi. Zone + oyuncu sayisi cikariliyor...");
        // Dialogu TAM logla: Log buffer'i 512 byte ile sinirli, bu yuzden parcala.
        {
            const char *pFull = szInfo ? szInfo : "";
            size_t nFull = strlen(pFull);
            for(size_t off = 0; off < nFull; off += 440)
            {
                char szChunk[512];
                size_t nChunk = (nFull - off) < 440 ? (nFull - off) : 440;
                memcpy(szChunk, pFull + off, nChunk);
                szChunk[nChunk] = 0;
                Log("[GST-DATA] %s", szChunk);
            }
        }

        // Dialog formati (logdan ornek):
        //   {00FF00}Zones:{FFFFFF}
        //   1. # 2
        //   2. # 13
        //   ...
        // Zone sayisi = "Zones:" basligi altindaki liste satirlari (# karakteri).
        // (Eski kod "Zone" sonrasi ilk sayiyi aliyordu -> satir numarasi 1 okunuyordu.)
        int nZones = 0, nPlayers = -1;
        const char *pszInfo = szInfo ? szInfo : "";
        const char *pZoneSec = strstr(pszInfo, "Zones:");
        if(!pZoneSec) pZoneSec = strstr(pszInfo, "Zone:");
        if(pZoneSec)
        {
            const char *p = pZoneSec;
            while((p = strchr(p, '#')) != NULL)
            {
                nZones++;
                p++;
            }
        }
        if(nZones == 0)
        {
            const char *pZone = strstr(pszInfo, "Zone");
            if(pZone) nZones = ExtractNumberAfter(pZone + 4);
        }

        // Aktif oyuncu sayisi: "Online Members:" basligi altindaki liste satirlari.
        // ornek:
        //   {00FF00}Online Members:{FFFFFF}
        //   1 - .Punisher(27) - King's Hand
        //   2 - washe(28) - Underboss
        //   ... -> her satir bir aktif uye
        const char *pAktif = strstr(pszInfo, "Online Members:");
        if(pAktif)
        {
            nPlayers = 0;
            const char *p = pAktif + 15; // "Online Members:" sonrasi
            while(*p)
            {
                const char *pNext = strchr(p, '\n');
                const char *pLineEnd = pNext ? pNext : (p + strlen(p));
                size_t nLen = (size_t)(pLineEnd - p);
                const char *pTrim = p;
                // Satir basini trimle: renk kodunu ({RRGGBB}) ve bosluk/satirsonu karakterlerini atla.
                // Djnat: '\n' bir satir sonudur, onu bosluk gibi atlamak bir sonraki satira akar ve
                // ilk uyeyi 2 kez saydirir. Bu yuzden satir sonunu (pLineEnd) sinir al.
                while(pTrim < pLineEnd)
                {
                    if(*pTrim == '{')
                    {
                        while(pTrim < pLineEnd && *pTrim != '}') pTrim++;
                        if(pTrim < pLineEnd && *pTrim == '}') pTrim++;
                    }
                    else if(*pTrim == ' ' || *pTrim == '\t' || *pTrim == '\r')
                        pTrim++;
                    else
                        break;
                }
                // Rakamla baslayan satirlar liste girdileridir ("1 - ...")
                if(pTrim < pLineEnd && *pTrim >= '0' && *pTrim <= '9')
                    nPlayers++;
                else if(pTrim >= pLineEnd)
                    { /* bos / sadece renk kodu olan satir, devam et */ }
                else
                    break; // rakamla baslamayan satir = sonraki bolum basligi
                if(!pNext) break;
                p = pNext + 1;
            }
        }
        else {
            // Fallback: "Online players", "Online", "Active", "Aktif", "Player", "Oyuncu"
            const char *pAktif2 = strstr(pszInfo, "Online players");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Active players");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Online");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Active");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Aktif");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Player");
            if(!pAktif2) pAktif2 = strstr(pszInfo, "Oyuncu");
            if(pAktif2)
            {
                const char *pNum = pAktif2;
                while(*pNum && !(*pNum >= '0' && *pNum <= '9')) pNum++;
                if(*pNum) nPlayers = atoi(pNum);
            }
        }

        // Sonucu oyunda grup sohbeti olarak goster (! ile), Discord'a gitmesin
        g_bridgeSuppressEcho++;
        if(nZones > 0) {
            char szResult[256];
            if(nPlayers >= 0)
                sprintf_s(szResult, "!Zone: %d | Aktif: %d", nZones, nPlayers);
            else
                sprintf_s(szResult, "!Zone: %d", nZones);
            Log("[GST] Sonuc: %s", szResult);
            sendChat(szResult);
        } else {
            Log("[GST] Zone bilgisi bulunamadi, ham veri gonderiliyor.");
            char szRaw[256];
            sprintf_s(szRaw, "!%s", pszInfo);
            sendChat(szRaw);
        }
        g_bridgeSuppressEcho--;

        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)1);
        bsResponse.Write((uint16_t)0xFFFF);
        bsResponse.Write((uint8_t)0);
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
        Log("[GST] Sonuc dialogu otomatik kapatildi (AUTOCLOSE).");
        return 1;
    }

    // -------------------------------------------------------------
    // 3b. /admins YAKALAMASI (Discord /admins slash komutu köprüsü)
    // -------------------------------------------------------------
    if (g_waitingForAdmins) {
        if (g_adminsReqTime != 0 && GetTickCount() - g_adminsReqTime > 20000) {
            g_waitingForAdmins = false;
            Log("[ADMINS] Bekleme zamanaşımı, flag sıfırlandı.");
            return 0;
        }

        g_waitingForAdmins = false;

        Log("[ADMINS] Sonuc dialogu yakalandi. Discord'a gonderiliyor...");
        Log("[ADMINS-DATA] %.250s", szInfo);

        // Security.cpp üzerinden Discord VDS'e aktarma
        SendAdminsToDiscord(szInfo);

        // Sonuc dialogunu ekrana cizdirmeden kapat
        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)1);        // Button 1 (OK)
        bsResponse.Write((uint16_t)0xFFFF);   // Item ID
        bsResponse.Write((uint8_t)0);        // Input Text Length
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

        Log("[ADMINS] Sonuc dialogu otomatik kapatildi (AUTOCLOSE).");
        return 1;
    }

    // -------------------------------------------------------------
    // 3.5 /groups YAKALAMASI (?zgrp grup ogrenme harvester'i)
    // Sunucu /groups cevabini LIST dialogu olarak acar; her satir "ID. isim".
    // OnGroupsListCaptured parse edip /gstats <ID> zincirini baslatir.
    // -------------------------------------------------------------
    if (g_waitingForGroups) {
        if (g_groupsReqTime != 0 && GetTickCount() - g_groupsReqTime > 15000) {
            g_waitingForGroups = false;
            Log("[GHP] /groups bekleme zamanaşımı, flag sıfırlandı.");
            return 0;
        }
        // /groups cevabi LIST dialog style'inda gelir (veya INFO). Ham olarak gecir:
        // OnGroupsListCaptured kendi parse kuraliyla calisir (format degisikligine dayanikli).
        // UIF iki dialog acar: once "UIF - Group Zone Info" (MSGBOX, kendi /gzinfo'ya benzer —
        // bu /groups cevabi DEGIL, beklemede kal), sonra gercek liste "Online Groups"
        // (TABLIST_HEADERS). Yalnizca liste stili yakalanir.
        if (sampDialog.bDialogStyle == DIALOG_STYLE_LIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST ||
            sampDialog.bDialogStyle == DIALOG_STYLE_TABLIST_HEADERS ||
            (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT && g_groupsReqTime != 0)) {
            Log("[GHP] /groups cevabi yakalandi (style %d, baslik: %s).", sampDialog.bDialogStyle, szTitle);
            g_waitingForGroups = false;
            OnGroupsListCaptured(szInfo);

            // Cevap dialogunu kapatiyor (ekrana cizdirme)
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);        // Button 1 (OK)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint8_t)0);        // Input Text Length
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            return 1;
        }
        // Farkli bir dialog geldi (ID giris vs.): bu /groups cevabi degil, beklemede kal
    }

    // -------------------------------------------------------------
    // 4. /gstats YAKALAMASI (Discord'a gonderilen)
    // -------------------------------------------------------------
    if (g_waitingForGStats) {
        // 20 sn içinde sonuç gelmezse bekleme flag'ini sıfırla (takılı kalmasın)
        if (g_gstatsReqTime != 0 && GetTickCount() - g_gstatsReqTime > 20000) {
            g_waitingForGStats = false;
            g_gstatsSentFirst = false;
            std::cout << "[gstats] Bekleme zamanaşımı, flag sıfırlandı.\n";
            return 0;
        }

        // Sunucu once ID giris dialogu (INPUT) gonderir.
        // /gstats ve /stats: dialogu anlik ac-kapat, g_gstatsInput doluysa o degeri yazip
        // submit et (belirli grup/oyuncu), bos ise bos submit -> kendi grubu (RakGravy davranisi).
        // /astats: parametre yoksa kullanici account ID girmeli, otomatik submit YAPILMAZ.
        if (sampDialog.bDialogStyle == DIALOG_STYLE_INPUT || sampDialog.bDialogStyle == DIALOG_STYLE_PASSWORD) {
            if(!g_gstatsAutoSubmit)
            {
                Log("[GSTATS] /astats ID giris dialogu atlaniyor (kullanici account ID girecek)...");
                return 0;
            }

            const char *pszInput = g_gstatsInput.c_str();
            size_t nInputLen = g_gstatsInput.length();
            if(nInputLen > 128) nInputLen = 128;

            Log("[GSTATS] ID giris dialogu algilandi, %s submit gonderiliyor...",
                nInputLen > 0 ? ("'" + g_gstatsInput + "' ile").c_str() : "bos");

            // Girilen deger + Button 1 (OK) ile DialogResponse gonder
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);        // Button 1 (OK)
            bsResponse.Write((uint16_t)0xFFFF);   // Item ID
            bsResponse.Write((uint16_t)nInputLen); // Input Text Length
            if(nInputLen > 0)
                bsResponse.Write(pszInput, nInputLen);
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

            std::cout << "[gstats] ID dialogu submit ile kapatildi, sonuc bekleniyor.\n";
            return 1;
        }

        // Sonuc dialogu yakalandi.
        // /stats sunucuda iki sonuc dialogu acar: once oyuncu stats, sonra grup stats.
        // Discord'a YALNIZCA komut Discord tarafindan istendiyse (g_gstatsRequesterId
        // dolu) gonderilir; oyun ici ?gstats/otomatik harvest istekleri Discord'a GITMEZ.
        std::cout << "[gstats yakalandi] ID: " << wDialogID << " - Baslik: " << szTitle << "\n";

        // HARVEST MODU (?zgrp): sonuc Discord'a gitmez, zone ogrenmesi icin kullanilir.
        if(g_waitingForGStats && OnGStatsResultForHarvest(szInfo))
        {
            // Harvester sonucu isledi ve zinciri surdurdu; dialogu kapat.
            RakNet::BitStream bsResponse;
            bsResponse.Write((uint16_t)wDialogID);
            bsResponse.Write((uint8_t)1);
            bsResponse.Write((uint16_t)0xFFFF);
            bsResponse.Write((uint8_t)0);
            pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
            Log("[GHP] gstats sonuc dialogu kapatildi (harvest).");
            return 1;
        }

        bool bDiscordRequested = (g_gstatsRequesterId[0] != '\0');
        if(!g_gstatsSentFirst && bDiscordRequested)
        {
            // Discord'dan istenen ilk sonuc: Discord'a gonder. /stats ikinci sonuc
            // dialogunu (grup stats) da acar; o sadece kapatilir (g_gstatsSentFirst=true).
            Log("[GSTATS] Sonuc dialogu yakalandi. Discord'a gonderiliyor...");
            Log("[GSTATS-DATA] %.250s", szInfo);

            // Security.cpp üzerinden Discord VDS'e aktarma
            SendGStatsToDiscord(szInfo);
            g_gstatsSentFirst = true;
        }
        else
        {
            Log(bDiscordRequested
                ? "[GSTATS] Ek sonuc dialogu (grup stats) yakalandi, sadece kapatiliyor."
                : "[GSTATS] Sonuc dialogu yakalandi (oyun ici istek), Discord'a gonderilmedi.");
            Log("[GSTATS-TAG] %.300s", szInfo);
            LearnGroupTagFromGStats(szInfo);
            g_waitingForGStats = false;
        }

        // Sonuc dialogunu ekrana cizdirmeden kapat (RakGravy davranisi: anlik ac-kapat)
        RakNet::BitStream bsResponse;
        bsResponse.Write((uint16_t)wDialogID);
        bsResponse.Write((uint8_t)1);        // Button 1 (OK)
        bsResponse.Write((uint16_t)0xFFFF);   // Item ID
        bsResponse.Write((uint8_t)0);        // Input Text Length
        pRakClient->RPC(&RPC_DialogResponse, &bsResponse, HIGH_PRIORITY, RELIABLE_ORDERED, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);

        Log("[GSTATS] Sonuc dialogu otomatik kapatildi (AUTOCLOSE).");
        return 1;
    }

    return 0;
#else
    (void)wDialogID; (void)szTitle; (void)szInfo;
    return 0;
#endif
}
