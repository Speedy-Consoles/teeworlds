/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>

#include "character.h"
#include "flag.h"

CFlag::CFlag(CGameWorld *pGameWorld, int Team, vec2 StandPos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG, StandPos, ms_PhysSize, 0, false)
{
	m_Team = Team;
	m_StandPos = StandPos;

	pGameWorld->InsertEntity(this);

	Reset();
}

void CFlag::Reset()
{
	m_pCarrier = 0;
	m_AtStand = true;
	m_Pos = m_StandPos;
	m_Vel = vec2(0, 0);
	m_GrabTick = 0;
}

void CFlag::Grab(CCharacter *pChar)
{
	m_pCarrier = pChar;
	if(m_AtStand)
		m_GrabTick = Server()->Tick();
	m_AtStand = false;
}

void CFlag::Drop()
{
	m_pCarrier = 0;
	m_Vel = vec2(0, 0);
	m_DropTick = Server()->Tick();
}

void CFlag::TickDefered()
{
	if(m_pCarrier)
	{
		// update flag position
		m_Pos = m_pCarrier->GetPos();
	}
	else
	{
		// flag hits death-tile or left the game layer, reset it
		if((Collision()->GetCollisionAt(m_Pos.x, m_Pos.y, !GameServer()->IsDDRace()) & CCollision::COLFLAG_DEATH)
			|| GameLayerClipped(m_Pos))
		{
			Reset();
			GameServer()->m_pController->OnFlagReturn(this);
		}

		if(!m_AtStand)
		{
			if(Server()->Tick() > m_DropTick + Server()->TickSpeed()*30)
			{
				Reset();
				GameServer()->m_pController->OnFlagReturn(this);
			}
			else
			{
				m_Vel.y += GameWorld()->m_Core.m_Tuning.m_Gravity;
				Collision()->MoveBox(&m_Pos, &m_Vel, 0, vec2(ms_PhysSize, ms_PhysSize), 0.5f, !GameServer()->IsDDRace());
			}
		}
	}
}

void CFlag::TickPaused()
{
	m_DropTick++;
	if(m_GrabTick)
		m_GrabTick++;
}

void CFlag::Snap(int SnappingClient, int WorldID)
{
	if (!WorldVisible(SnappingClient, WorldID))
		return;

	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Flag *pVanillaFlag = 0;
	CNetObj_DDRaceFlag *pFlag;
	CNetObj_DDRaceFlag DummyFlag;
	if(GameServer()->DoesPlayerHaveDDRaceClient(SnappingClient))
	{
		pFlag = static_cast<CNetObj_DDRaceFlag *>(Server()->SnapNewItem(NETOBJTYPE_DDRACEFLAG, GetID(), sizeof(CNetObj_DDRaceFlag)));
		if(!pFlag)
			return;
	}
	else
	{
		pVanillaFlag = static_cast<CNetObj_Flag *>(Server()->SnapNewItem(NETOBJTYPE_FLAG, GetID(), sizeof(CNetObj_Flag)));
		if(!pVanillaFlag)
			return;
		pFlag = &DummyFlag;
	}

	pFlag->m_X = (int)m_Pos.x;
	pFlag->m_Y = (int)m_Pos.y;
	pFlag->m_Team = m_Team;
	pFlag->m_WorldID = WorldID;

	if(pVanillaFlag)
		*pVanillaFlag = pFlag->ToVanilla();
}
