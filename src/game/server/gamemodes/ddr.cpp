/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ddr.h"

CGameControllerDDR::CGameControllerDDR(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	// Exchange this to a string that identifies your game mode.
	// DM, TDM and CTF are reserved for teeworlds original modes.
	m_pGameType = "DDRace";

	//m_GameFlags = GAMEFLAG_TEAMS; // GAMEFLAG_TEAMS makes it a two-team gamemode
}

void CGameControllerDDR::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	IGameController::Tick();
}

void CGameControllerDDR::OnCharacterSpawn(CCharacter *pChr)
{
		pChr->IncreaseHealth(10);
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		pChr->GiveWeapon(WEAPON_GUN, -1);
}

void CGameControllerDDR::OnRaceStart(CGameWorld *pWorld)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "A Team has started a race!");
	GameServer()->SendBroadcast(aBuf, -1);
}

void CGameControllerDDR::OnRaceCancel(CGameWorld *pWorld)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "A Team has canceled a race!");
	GameServer()->SendBroadcast(aBuf, -1);
}

void CGameControllerDDR::OnRaceFinish(CGameWorld *pWorld, int MilliSecs)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "A Team finished in %.2f seconds!", MilliSecs / 1000.0);
	GameServer()->SendBroadcast(aBuf, -1);
}
