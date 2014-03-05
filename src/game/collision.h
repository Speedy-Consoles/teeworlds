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
	class CLayers *m_pLayers;

	int m_NumCheckpoints;

	bool IsTileSolid(int x, int y);
	int GetTile(int x, int y);
	ivec2 GetTilePos(float x, float y);
	int GetPosIndex(int x, int y, int Layer);

public:
	enum
	{
		COLFLAG_SOLID=1,
		COLFLAG_DEATH=2,
		COLFLAG_NOHOOK=4,
	};

	struct CTriggers
	{
		int m_Checkpoint;
		CTriggers() : m_Checkpoint() {}
	};

	CCollision();
	void Init(class CLayers *pLayers);
	int GetNumCheckpoints();
	bool CheckPoint(float x, float y) { return IsTileSolid(round_to_int(x), round_to_int(y)); }
	bool CheckPoint(vec2 Pos) { return CheckPoint(Pos.x, Pos.y); }
	int GetCollisionAt(float x, float y) { return GetTile(round_to_int(x), round_to_int(y)); }
	int GetWidth() { return m_aWidth[GAMELAYERTYPE_VANILLA]; };
	int GetHeight() { return m_aHeight[GAMELAYERTYPE_VANILLA]; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces);
	int MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, CTriggers *pOutTriggers, vec2 Size, float Elasticity);
	void HandleTriggerTiles(int x, int y, CTriggers *pOutTriggers);
	bool TestBox(vec2 Pos, vec2 Size);
};

#endif
