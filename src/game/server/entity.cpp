/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
#include "player.h"

CEntity::CEntity(CGameWorld *pGameWorld, int ObjType, vec2 Pos, int ProximityRadius, int SwitchGroup, bool InvertSwitch)
{
	m_pGameWorld = pGameWorld;

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;

	m_ID = Server()->SnapNewID();
	m_ObjType = ObjType;

	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_Pos = Pos;

	m_DDRaceSwitchGroup = SwitchGroup;
	m_DDRaceInvertSwitch = InvertSwitch;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeID(m_ID);
}

void CEntity::MoveToWorld(CGameWorld *pWorld)
{
	m_pGameWorld->RemoveEntity(this);
	m_pGameWorld = pWorld;
	m_pGameWorld->InsertEntity(this);
}

int CEntity::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	if(SnappingClient == -1)
		return 0;

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x-CheckPos.x;
	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y-CheckPos.y;

	if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
		return 1;

	if(distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 1100.0f)
		return 1;
	return 0;
}

bool CEntity::WorldVisible(int SnappingClient, int WorldID)
{
	return SnappingClient == -1 || WorldID == GameServer()->m_apPlayers[SnappingClient]->WorldID();
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	int rx = round_to_int(CheckPos.x) / 32;
	int ry = round_to_int(CheckPos.y) / 32;
	return (rx < -200 || rx >= Collision()->GetWidth()+200)
			|| (ry < -200 || ry >= Collision()->GetHeight()+200);
}

bool CEntity::Active()
{
	return m_DDRaceSwitchGroup == -1 || (GameWorld()->m_aDDRaceSwitchStates[m_DDRaceSwitchGroup] && m_DDRaceInvertSwitch)
			|| (!GameWorld()->m_aDDRaceSwitchStates[m_DDRaceSwitchGroup] && !m_DDRaceInvertSwitch);
}
