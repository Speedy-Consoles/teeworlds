/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H

#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/entities/character.h>

class CGameControllerDDR : public IGameController
{
public:
	CGameControllerDDR(class CGameContext *pGameServer);
	virtual void Tick();
	virtual bool IsDDRace() const { return true; }
	virtual void OnCharacterSpawn(CCharacter *pChr);

	virtual void OnRaceStart(CGameWorld *pWorld);
	virtual void OnRaceFinish(CGameWorld *pWorld, int MilliSecs);
	virtual void OnRaceCancel(CGameWorld *pWorld);
};
#endif
