/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_ITEMS_H
#define GAME_CLIENT_COMPONENTS_ITEMS_H
#include <game/client/component.h>

class CItems : public CComponent
{
	void RenderProjectile(const CNetObj_DDRaceProjectile *pCurrent, int ItemID);
	void RenderPickup(const CNetObj_DDRacePickup *pPrev, const CNetObj_DDRacePickup *pCurrent);
	void RenderFlag(const CNetObj_DDRaceFlag *pPrev, const CNetObj_DDRaceFlag *pCurrent, const CNetObj_GameDataFlag *pPrevGameDataFlag, const CNetObj_GameDataFlag *pCurGameDataFlag);
	void RenderLaser(const struct CNetObj_DDRaceLaser *pCurrent);

public:
	virtual void OnRender();
};

#endif
