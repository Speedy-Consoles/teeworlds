/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>
#include <game/mapitems.h>

class CCollision
{
	class CTile *m_apTiles[NUM_GAMELAYERTYPES];
	int m_aWidth[NUM_GAMELAYERTYPES];
	int m_aHeight[NUM_GAMELAYERTYPES];
	class CTile *m_pVanillaTiles;
	int m_VanillaWidth;
	int m_VanillaHeight;
	int m_pOpen;
	class CLayers *m_pLayers;
	bool *m_pSwitchStates;

	vec2 m_aTeleTargets[255];

	int m_NumCheckpoints;

	int GetSwitchGroup(int PosIndex, int Layer) const;
	ivec2 PosToTilePos(float x, float y) const;
	int TilePosToIndex(int x, int y, int Layer) const;
	int VanillaTilePosToIndex(int x, int y) const;
	int PosToIndex(float x, float y, int Layer) const;
	int VanillaPosToIndex(float x, float y) const;
	int GetDirFlags(ivec2 Dir) const;

public:
	enum
	{
		COLFLAG_SOLID=1,
		COLFLAG_DEATH=2,
		COLFLAG_NOHOOK=4,
		COLFLAG_SOLID_HOOK=8,
		COLFLAG_SOLID_PROJ=16,

		DIRFLAG_RIGHT=1,
		DIRFLAG_LEFT=2,
		DIRFLAG_UP=4,
		DIRFLAG_DOWN=8,

		FREEZEFLAG_FREEZE=1,
		FREEZEFLAG_UNFREEZE=2,
		FREEZEFLAG_DEEP_FREEZE=4,
		FREEZEFLAG_DEEP_UNFREEZE=8,

		TRIGGERFLAG_FREEZE=1,
		TRIGGERFLAG_UNFREEZE=2,
		TRIGGERFLAG_DEEP_FREEZE=4,
		TRIGGERFLAG_DEEP_UNFREEZE=8,

		TRIGGERFLAG_SWITCH=1,

		TRIGGERFLAG_TELEPORT=1,
		TRIGGERFLAG_CUT_OTHER=2,
		TRIGGERFLAG_CUT_OWN=4,
		TRIGGERFLAG_STOP_NINJA=8,

		TRIGGERFLAG_SPEEDUP=1,

		PROPERTEE_ON=1,
		PROPERTEE_OFF=-1,
	};

	struct CTriggers
	{
		int m_FreezeFlags;

		int m_SwitchFlags;
		bool m_SwitchState;
		int m_SwitchGroup;
		int m_SwitchDuration;

		int m_TeleFlags;
		vec2 m_TeleInPos;

		vec2 m_TeleOutPos;
		int m_SpeedupFlags;

		int m_Checkpoint;

		int m_Endless;
		int m_Solo;
		int m_Nohit;

		CTriggers() : m_FreezeFlags(), m_SwitchFlags(), m_SwitchState(), m_SwitchGroup(), m_SwitchDuration(), m_TeleFlags(), m_TeleInPos(vec2(0.0f, 0.0f)),
			m_TeleOutPos(vec2(0.0f, 0.0f)), m_SpeedupFlags(), m_Checkpoint(), m_Endless(), m_Solo(), m_Nohit() {}
	};

	CCollision();
	void Init(class CLayers *pLayers, bool *pSwitchStates);
	void Init(class CCollision *pOther, bool *pSwitchStates);
	int GetNumCheckpoints() const;
	int GetDirFlagsAt(float x, float y) const;
	int GetCollisionAt(float x, float y, bool Vanilla) const;
	int GetCollisionAt(vec2 Pos, bool Vanilla) const { return GetCollisionAt(Pos.x, Pos.y, Vanilla); }
	int GetCollisionMove(float x, float y, float OldX, float OldY, bool Vanilla, int DirFlagsMask = ~0) const;
	int GetCollisionMove(vec2 Pos, vec2 OldPos, bool Vanilla) const { return GetCollisionMove(Pos.x, Pos.y, OldPos.x, OldPos.y, Vanilla); }
	int GetCollisionMove(vec2 Pos, float OldX, float OldY, bool Vanilla) const { return GetCollisionMove(Pos.x, Pos.y, OldX, OldY, Vanilla); }
	int GetCollisionMove(float x, float y, vec2 OldPos, bool Vanilla) const { return GetCollisionMove(x, y, OldPos.x, OldPos.y, Vanilla); }
	int GetWidth() const { return m_aWidth[GAMELAYERTYPE_COLLISION]; };
	int GetHeight() const { return m_aHeight[GAMELAYERTYPE_COLLISION]; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int ColFlag, bool Vanilla) const;
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces, int ColFlag, bool Vanilla) const;
	int MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, CTriggers *pOutTriggers, vec2 Size, float Elasticity, bool Vanilla) const;
	void HandleTriggerTiles(int x, int y, CTriggers *pOutTriggers) const;
	bool TestBox(vec2 Pos, vec2 Size, bool Vanilla) const;
	bool TestBoxMove(vec2 Pos, vec2 OldPos, vec2 Size, bool Vanilla) const;
	bool TestHLineMove(vec2 Pos, vec2 OldPos, float Length, bool Vanilla) const;
};

#endif
