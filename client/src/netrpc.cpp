/*
	Updated to 0.3.7 by P3ti
*/

#include "main.h"
#include "Security.h"

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

// Zone war süresi sabittir (UIF: 10 dakika). Chat'e düşen "Type /gz2 to join"
// bildirimi savaşın başlangıç anıdır; ?gzt kalan süreyi bundan hesaplar.
#define ZONE_WAR_TOTAL_MS 600000
#define ZONE_WAR_AUTOJOIN_MS 10000     // GZ bitmeden son 10 sn'de otomatik /gz2
#define KILL_LIST_CAPTURE_WINDOW_MS 15000 // sonuç textdraw'ları için toplama penceresi
static DWORD g_zoneWarStartTick = 0;

// Kill list yakalayıcı durumu (GZ sonucu textdraw'ları -> dosya + Discord)
static bool g_zwAutoJoinSent = false;       // son 10 sn'de /gz2 gönderildi mi
static bool g_zwCapturingKillList = false;  // textdraw toplama aktif mi
static bool g_zwKillListSent = false;       // sonuç zaten gönderildi mi
static DWORD g_zwCaptureStart = 0;          // toplama başlangıcı
static std::string g_zwKillListText;        // birikmiş sonuç metni

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

	// Grup uyesi bir oyuncu baglandi ve zone war aktifse gruba welcome mesaji at.
	// (Sadece bir kez: g_knownGroupMembers bu oyuncu icin zaten 1 ise tekrar atma.)
	if(g_inZoneWar && playerId != g_myPlayerID && g_knownGroupMembers[playerId])
	{
		Log("[WELCOME] Grup uyesi (%d) %s baglandi, %s ile savas aktif.", playerId, szPlayerName, g_enemyGroupName.c_str());
		Log("[WELCOME] Gruba savas bildirimi gonderiliyor...");
		// ! ön eki: bu mesaj gruba normal chat olarak gider (Discord koprusune de duser)
		static char szWarNotice[192];
		sprintf_s(szWarNotice, sizeof(szWarNotice),
			"!Grup uyemiz %s oyuna katildi. Zone war devam ediyor! /gz2 yazarak savasa katil.",
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
			pAction = "join";
			// Savas baslangic anini kaydet: kalan sure = ZONE_WAR_TOTAL_MS - gecen sure.
			g_zoneWarStartTick = GetTickCount();
			g_inZoneWar = true;
			// Yeni savas: onceki yakalayici/kill list durumunu sifirla
			g_zwAutoJoinSent = false;
			g_zwCapturingKillList = false;
			g_zwKillListSent = false;
			g_zwKillListText.clear();
			Log("[ZONEWAR] Savas basladi (Type /gz2 to join), sure sayaci baslatildi.");
			// Grubumuzun adini henuz bilmiyorsak ogrenme moduna gir.
			// Sonraki ilk "ZONE WAR: X keeps/takes..." mesajindaki X bizim grubumuzdur.
			if(g_myGroupName.empty() && !g_zwLearningGroup)
			{
				g_zwLearningGroup = true;
				g_zwLearnTick = GetTickCount();
				Log("[ZONEWAR] /gz2 bildirimi alindi, kendi grup adi ogrenilecek...");
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

				// Mesaj sonuna kadar ismi kopyala
				std::string strLearned;
				const char *p = pName;
				while(*p && *p != '\r' && *p != '\n')
					strLearned += *p++;

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
		if(pAction && !bJoinNotice)
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

				// Savas bitti (wins/loses): sure sayacini sifirla.
				if(!strcmp(pAction, "wins") || !strcmp(pAction, "loses"))
				{
					g_zoneWarStartTick = 0;
					g_inZoneWar = false;

					// Sonuc textdraw'lari gelmek uzere: henuz yakalayici acilmadiysa ac.
					// (Normalde son 10 sn'de /gz2 ile acilir; bot iceride kalmadiysa buradan yakala.)
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

			if(iSenderId != g_myPlayerID) return; // sadece kendi komutlarimiz

			// --- ?w : Discord war help, 120 sn cooldown (RakGravy ile ayni) ---
			if(!strncmp(pGroupMsg, "?w", 2) && (pGroupMsg[2] == '\0' || pGroupMsg[2] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastWarHelpTime != 0 && (dwNow - g_lastWarHelpTime) < 120000)
				{
					Log("[WARHELP] Cooldown aktif (%lu sn kaldi), ?w yoksayildi.", (120000 - (dwNow - g_lastWarHelpTime)) / 1000);
					return;
				}
				g_lastWarHelpTime = dwNow;
				Log("[WARHELP] Grup chat'inde ?w yakalandi. Discord'a gonderiliyor...");
				TriggerWarHelp();
			}
			// --- ?ping : yanit verir, 30 sn cooldown (RakGravy ile ayni) ---
			if(!strncmp(pGroupMsg, "?ping", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastPingTime != 0 && (dwNow - g_lastPingTime) < 30000)
				{
					Log("[PING] Cooldown aktif, ?ping yoksayildi.");
					return;
				}
				g_lastPingTime = dwNow;
				Log("[PING] ?ping yakalandi, Pong cevabi gonderiliyor.");
				sendChat((char *)"!Pong!");
			}
			// --- ?pl [id] : /pnetstats dialogundan packetloss, 30 sn cooldown ---
			else if(!strncmp(pGroupMsg, "?pl", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastPLTime != 0 && (dwNow - g_lastPLTime) < 30000)
				{
					Log("[PNETSTATS] Cooldown aktif, ?pl yoksayildi.");
					return;
				}
				g_lastPLTime = dwNow;

				int iTarget = g_myPlayerID;
				const char *pArg = pGroupMsg + 3;
				while(*pArg == ' ') pArg++;
				if(*pArg != '\0')
				{
					int i = atoi(pArg);
					if(i >= 0 && i < MAX_PLAYERS && playerInfo[i].iIsConnected)
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
			}
			// --- ?gst : /gstats dialogundan zone + aktif oyuncu sayisini oyunda goster (Discord'a gitmez) ---
			else if(!strncmp(pGroupMsg, "?gst", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGStatsTime != 0 && (dwNow - g_lastGStatsTime) < 30000)
				{
					Log("[GST] Cooldown aktif, ?gst yoksayildi.");
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGst = true;
				g_gstReqTime = GetTickCount();
				Log("[GST] /gstats gonderiliyor (zone + aktif oyuncu sayisi icin)...");
				sendServerCommand((char *)"/gstats");
			}
			// --- ?gstats : /gstats dialogunu Discord'a yolla, 30 sn cooldown ---
			else if(!strncmp(pGroupMsg, "?gstats", 7) && (pGroupMsg[7] == '\0' || pGroupMsg[7] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGStatsTime != 0 && (dwNow - g_lastGStatsTime) < 30000)
				{
					Log("[GSTATS] Cooldown aktif, ?gstats yoksayildi.");
					return;
				}
				g_lastGStatsTime = dwNow;

				g_waitingForGStats = true;
				g_gstatsReqTime = GetTickCount();
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
			// --- ?gzt : zone war timer — chat duyurusundan hesaplanan kalan sureyi gruba bildir ---
			else if(!strncmp(pGroupMsg, "?gzt", 4) && (pGroupMsg[4] == '\0' || pGroupMsg[4] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastGztTime != 0 && (dwNow - g_lastGztTime) < 30000)
				{
					Log("[GZT] Cooldown aktif, ?gzt yoksayildi.");
					return;
				}
				g_lastGztTime = dwNow;

				// Kalan sure = toplam sure - (simdi - baslangic). Savas baslamadiysa yok.
				DWORD dwElapsed = (g_zoneWarStartTick != 0) ? (dwNow - g_zoneWarStartTick) : 0;
				if(g_zoneWarStartTick != 0 && dwElapsed < ZONE_WAR_TOTAL_MS)
				{
					int iRemainingSec = (int)((ZONE_WAR_TOTAL_MS - dwElapsed) / 1000);
					int iMin = iRemainingSec / 60;
					int iSec = iRemainingSec % 60;
					char szWarTime[192];
					sprintf_s(szWarTime, sizeof(szWarTime), "!War time: %d:%02d", iMin, iSec);
					Log("[GZT] Kalan sure hesaplandi: %d:%02d", iMin, iSec);
					sendChat(szWarTime);
				}
				else
				{
					// Savas baslangici bilinmiyor ya da coktan bitti.
					Log("[GZT] Savas baslangici yok, War time bilgisi gonderilemiyor.");
					sendChat((char *)"!War time: yok");
				}
			}
			// --- ?discordchat : Discord grup chat kopusunu ac/kapat (MongoDB'de saklanir) ---
			else if(!strncmp(pGroupMsg, "?discordchat", 12) && (pGroupMsg[12] == '\0' || pGroupMsg[12] == ' '))
			{
				Log("[GROUPCHAT] ?discordchat komutu yakalandi, durum degistiriliyor...");
				ToggleDiscordChat();
			}
			// --- ?1414 : backup cagrisi → Discord backup kanalina @here, 60 sn cooldown ---
			else if(!strncmp(pGroupMsg, "?1414", 5) && (pGroupMsg[5] == '\0' || pGroupMsg[5] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastBackupTime != 0 && (dwNow - g_lastBackupTime) < 60000)
				{
					Log("[BACKUP] Cooldown aktif, ?1414 yoksayildi.");
					return;
				}
				g_lastBackupTime = dwNow;
				Log("[BACKUP] Grup chat'inde ?1414 yakalandi. Discord'a backup cagrisi gonderiliyor...");
				SendBackupCall();
			}
			// --- ?gz : zone war baslatma → Discord zonewar kanalina bildirim, 180 sn cooldown (RakGravy ile ayni) ---
			else if(!strncmp(pGroupMsg, "?gz", 3) && (pGroupMsg[3] == '\0' || pGroupMsg[3] == ' '))
			{
				DWORD dwNow = GetTickCount();
				if(g_lastZoneWarTime != 0 && (dwNow - g_lastZoneWarTime) < 180000)
				{
					Log("[ZONEWAR] Cooldown aktif, ?gz yoksayildi.");
					return;
				}
				g_lastZoneWarTime = dwNow;
				Log("[ZONEWAR] Grup chat'inde ?gz yakalandi. Zone war bildirimi gonderiliyor...");
				SendZoneWarEvent("keeps", "");
			}
			// --- Grup chat kopusu: ! ile baslayan kendi mesajlarimizi Discord'a ilet ---
			else if(pGroupMsg[0] == '!')
			{
				if(g_bridgeSuppressEcho > 0)
				{
					g_bridgeSuppressEcho--;
					return;
				}
				Log("[GROUPCHAT] Grup mesaji Discord'a iletilmek uzere: %s", pGroupMsg);
				SendGroupChatToDiscord(pGroupMsg);
			}
		}
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

	if(imitateID == playerId)
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

	// /pnetstats ve /gstats dialog yakalamasi: yakalandiysa ekrana cizdirme
	if(HandleCapturedDialog(sampDialog.wDialogID, sampDialog.szTitle, szDlgInfo))
	{
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
			"Kills", "DMG", "Total", "MVP", "Score", "Group Zone War", "Top Feeder", "Player"
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
// 1. GZ son 10 sn'de otomatik /gz2 giris yap (bot iceride kalip sonucu alsin)
// 2. Toplama penceresi dolunca sonucu finalize et (dosya + Discord)
void ProcessZoneWarAutoJoin()
{
	DWORD dwNow = GetTickCount();

	// GZ aktif ve sure sayaci baslamissa son 10 sn kontrolu
	if(g_inZoneWar && g_zoneWarStartTick != 0 && !g_zwAutoJoinSent)
	{
		DWORD dwElapsed = dwNow - g_zoneWarStartTick;
		if(dwElapsed >= (ZONE_WAR_TOTAL_MS - ZONE_WAR_AUTOJOIN_MS))
		{
			g_zwAutoJoinSent = true;
			g_zwCapturingKillList = true;
			g_zwCaptureStart = dwNow;
			g_zwKillListText.clear();
			Log("[KILLLIST] GZ son 10 sn'ye girildi, /gz2 gonderiliyor + yakalayici acildi.");
			sendServerCommand((char *)"/gz2");
		}
	}

	// Toplama penceresi doldu mu? -> finalize et
	if(g_zwCapturingKillList && !g_zwKillListSent)
	{
		if(dwNow - g_zwCaptureStart >= KILL_LIST_CAPTURE_WINDOW_MS)
			FinalizeKillList();
	}
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
	}
}
