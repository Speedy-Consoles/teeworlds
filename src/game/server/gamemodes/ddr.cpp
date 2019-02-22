/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ddr.h"

#include <game/server/gameworld.h>
#include <game/server/player.h>

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

int CGameControllerDDR::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	CRaceTimer *pRaceTimer = pVictim->RaceTimer();
	CGameWorld *pWorld = pVictim->GameWorld();
	CRaceTimer *pWorldRaceTimer = pWorld->RaceTimer();

	pRaceTimer->Reset();

	if(pWorld->IsDefault())
		return 0;

	switch(pWorldRaceTimer->State())
	{
	case CRaceTimer::RACESTATE_STARTING:
		break;
	case CRaceTimer::RACESTATE_STARTED:
		{
			pWorldRaceTimer->Cancel();
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "A Team's race was canceled!");
			GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
		}
		// fall through
	case CRaceTimer::RACESTATE_FINISHED:
	case CRaceTimer::RACESTATE_CANCELED:
		{
			bool AllStarting = true;
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if(!pPlayer || GameServer()->GetWorld(pPlayer->WorldID()) != pWorld)
					continue;

				CCharacter *pChr = pPlayer->GetCharacter();
				if(pChr && pChr->RaceTimer()->State() != CRaceTimer::RACESTATE_STARTING)
				{
					AllStarting = false;
					break;
				}
			}

			if(AllStarting)
			{
				pWorld->m_SoftResetRequested = true;
				pWorldRaceTimer->Reset();

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "A Team's race was reset!");
				GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
			}
		}
		break;
	}
	return 0;
}

void CGameControllerDDR::OnRaceStart(CCharacter *pChr)
{
	CRaceTimer *pRaceTimer = pChr->RaceTimer();
	CGameWorld *pWorld = pChr->GameWorld();
	CRaceTimer *pWorldRaceTimer = pWorld->RaceTimer();

	pRaceTimer->Reset();
	pRaceTimer->Start();

	if(!pWorld->IsDefault() && pWorldRaceTimer->State() == CRaceTimer::RACESTATE_STARTING)
	{
		pWorldRaceTimer->Start();

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "A Team has started a race!");
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
	}
}

void CGameControllerDDR::OnRaceFinish(CCharacter *pChr)
{
	CRaceTimer *pRaceTimer = pChr->RaceTimer();
	CGameWorld *pWorld = pChr->GameWorld();
	CRaceTimer *pWorldRaceTimer = pWorld->RaceTimer();
	CPlayer *pPlayer = pChr->GetPlayer();

	pRaceTimer->Stop();

	int Seconds = ceil(pRaceTimer->Time());
	if(-Seconds > pPlayer->m_Score || pPlayer->m_Score == 0)
		pPlayer->m_Score = -Seconds;

	if(pWorld->IsDefault())
	{
		char aBuf[256];
		str_format(
			aBuf,
			sizeof(aBuf), "'%s' finished in %.2f seconds!",
			Server()->ClientName(pPlayer->GetCID()),
			pRaceTimer->Time()
		);
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);

		return;
	}

	if (pWorldRaceTimer->State() != CRaceTimer::RACESTATE_STARTED)
		return;

	bool AllFinished = true;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || GameServer()->GetWorld(pPlayer->WorldID()) != pWorld)
			continue;

		CCharacter *pChr = pPlayer->GetCharacter();
		if(!pChr || pChr->RaceTimer()->State() != CRaceTimer::RACESTATE_FINISHED)
		{
			AllFinished = false;
			break;
		}
	}

	if(AllFinished)
	{
		pWorldRaceTimer->Stop();

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "A Team finished in %.2f seconds!", pWorldRaceTimer->Time());
		GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
	}
	// TODO DDRace store finish time
}

void CGameControllerDDR::TryChangePlayerWorld(CPlayer *pPlayer, int TargetWorldID)
{
	int WorldID = pPlayer->WorldID();
	CGameWorld *pWorld = GameServer()->GetWorld(WorldID);
	CGameWorld *pTargetWorld = GameServer()->GetWorld(TargetWorldID);
	if(pTargetWorld == pWorld || pTargetWorld->RaceTimer()->State() != CRaceTimer::RACESTATE_STARTING)
		return;
	bool WorldRunning = pWorld->RaceTimer()->State() != CRaceTimer::RACESTATE_STARTING;
	bool PlayerRunning = pPlayer->GetCharacter() && pPlayer->GetCharacter()->RaceTimer()->State() != CRaceTimer::RACESTATE_STARTING;
	if(WorldRunning || PlayerRunning)
		pPlayer->KillCharacter();
	pPlayer->ChangeWorld(TargetWorldID);
}

