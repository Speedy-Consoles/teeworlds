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
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);

	virtual void OnRaceStart(CCharacter *pChr);
	virtual void OnRaceFinish(CCharacter *pChr);
	virtual void TryChangePlayerWorld(CPlayer *pPlayer, int TargetWorldID);
};
#endif
