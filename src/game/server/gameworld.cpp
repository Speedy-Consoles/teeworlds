/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entities/character.h"
#include "entity.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "eventhandler.h"


//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld()
{
	m_pGameServer = 0x0;
	m_pServer = 0x0;

	m_Paused = false;
	m_ResetRequested = false;
	for(int i = 0; i < NUM_ENTTYPES; i++)
		m_apFirstEntityTypes[i] = 0;
	for(int i = 0; i < 255; i++)
	{
		m_aDDRaceSwitchStates[i] = false;
		m_aDDRaceSwitchTicks[i] = -1;
	}
	
	m_RaceState = RACESTATE_STARTING;
	m_RaceStartTick = -1;
	m_Default = false;
}

CGameWorld::~CGameWorld()
{
	// delete all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		while(m_apFirstEntityTypes[i])
			delete m_apFirstEntityTypes[i];
}

void CGameWorld::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_Events.SetGameServer(pGameServer);
	m_pServer = m_pGameServer->Server();
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? 0 : m_apFirstEntityTypes[Type];
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type];	pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(distance(pEnt->m_Pos, Pos) < Radius+pEnt->m_ProximityRadius)
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

void CGameWorld::InsertEntity(CEntity *pEnt)
{
#ifdef CONF_DEBUG
	for(CEntity *pCur = m_apFirstEntityTypes[pEnt->m_ObjType]; pCur; pCur = pCur->m_pNextTypeEntity)
		dbg_assert(pCur != pEnt, "err");
#endif

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = 0x0;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::DestroyEntity(CEntity *pEnt)
{
	pEnt->MarkForDestroy();
}

void CGameWorld::RemoveEntity(CEntity *pEnt)
{
	// not in the list
	if(!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity && m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
		return;

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	if(m_pNextTraverseEntity == pEnt)
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

	pEnt->m_pNextTypeEntity = 0;
	pEnt->m_pPrevTypeEntity = 0;
}

void CGameWorld::SetSwitchState(bool State, int GroupID, int Duration)
{
	m_aNextSwitchStates[GroupID] = State;
	if(Duration == -1)
		m_aDDRaceSwitchTicks[GroupID] = -1;
	else
		m_aDDRaceSwitchTicks[GroupID] = Duration * Server()->TickSpeed() + 1;
}

void CGameWorld::StartRace()
{
	if(!m_Default && m_RaceState == RACESTATE_STARTING)
	{
		m_RaceState = RACESTATE_STARTED;
		m_RaceStartTick = Server()->Tick();
		GameServer()->m_pController->OnRaceStart(this);
	}
}

void CGameWorld::OnPlayerDeath()
{
	if(!m_Default)
	{
		if(m_RaceState == RACESTATE_STARTED)
		{
			m_RaceState = RACESTATE_CANCELED;
			GameServer()->m_pController->OnRaceCancel(this);
		}

		bool AllStarting = true;
		for(CEntity *pEnt = m_apFirstEntityTypes[ENTTYPE_CHARACTER]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(((CCharacter *) pEnt)->IsAlive() && ((CCharacter *) pEnt)->RaceState() != RACESTATE_STARTING)
			{
				AllStarting = false;
				break;
			}
			pEnt = m_pNextTraverseEntity;
		}

		if(AllStarting)
			m_RaceState = RACESTATE_STARTING;
	}
}

void CGameWorld::OnFinish()
{
	if(m_Default || m_RaceState != RACESTATE_STARTED)
		return;

	bool AllFinished = true;
	for(CEntity *pEnt = m_apFirstEntityTypes[ENTTYPE_CHARACTER]; pEnt; )
	{
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
		if(((CCharacter *) pEnt)->RaceState() != RACESTATE_FINISHED)
		{
			AllFinished = false;
			break;
		}
		pEnt = m_pNextTraverseEntity;
	}
	
	int ms = (Server()->Tick() - m_RaceStartTick) * 1000 / (float) Server()->TickSpeed();
	
	if(AllFinished)
	{
		GameServer()->m_pController->OnRaceFinish(this, ms);
		m_RaceState = RACESTATE_FINISHED;
	}
	// TODO store finish time
}

//
void CGameWorld::Snap(int SnappingClient, int WorldID)
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Snap(SnappingClient, WorldID);
			pEnt = m_pNextTraverseEntity;
		}

	// snap switch states
	if (GameServer()->DoesPlayerHaveDDRaceClient(SnappingClient))
	{
		CNetObj_DDRaceSwitchStates *pSwitchStates = static_cast<CNetObj_DDRaceSwitchStates *>(Server()->SnapNewItem(NETOBJTYPE_DDRACESWITCHSTATES, WorldID, sizeof(CNetObj_DDRaceSwitchStates)));
		if(pSwitchStates)
		{
			for(int i = 0; i < 255; i++)
				pSwitchStates->m_aStates[i/8] |= m_aDDRaceSwitchStates[i] << (i % 8);
		}
	}

	m_Events.Snap(SnappingClient, WorldID);
}

void CGameWorld::PostSnap()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->PostSnap();
			pEnt = m_pNextTraverseEntity;
		}
	m_Events.Clear();
}

void CGameWorld::Reset(bool Soft)
{
	// reset all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(!Soft || !pEnt->m_Persistent)
				pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	GameServer()->ResetPlayers(this);

	for(int i = 0; i < 255; i++)
	{
		m_aDDRaceSwitchStates[i] = false;
		m_aDDRaceSwitchTicks[i] = -1;
	}

	if(!Soft)
		m_RaceState = RACESTATE_STARTING;
	m_ResetRequested = false;
	m_SoftResetRequested = false;
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(pEnt->IsMarkedForDestroy())
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
			}
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset(false);
	else if(m_SoftResetRequested)
		Reset(true);

	if(!m_Paused)
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}

		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDefered();
				pEnt = m_pNextTraverseEntity;
			}
			
		// update switch states
		m_SwitchStateChanged = false;
		for(int i = 0; i < 255; i++)
		{
			int Old = m_aDDRaceSwitchStates[i];
			m_aDDRaceSwitchStates[i] = m_aNextSwitchStates[i];
			if(m_aDDRaceSwitchTicks[i] == 0)
			{
				m_aDDRaceSwitchTicks[i] = -1;
				m_aNextSwitchStates[i] = !m_aDDRaceSwitchStates[i];
			}
			else if(m_aDDRaceSwitchTicks[i] > 0)
				m_aDDRaceSwitchTicks[i]--;
			if(Old != m_aDDRaceSwitchStates[i])
				m_SwitchStateChanged = true;
		}
	}
	else
	{
		++m_RaceStartTick;

		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();
}


// TODO: should be more general
CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, FnCountCharacter pfnCountCharacterCB, void *pUserData)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = 0;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
 	{
		if(!pfnCountCharacterCB(p, pUserData))
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		float Len = distance(p->m_Pos, IntersectPos);
		if(Len < p->m_ProximityRadius+Radius)
		{
			Len = distance(Pos0, IntersectPos);
			if(Len < ClosestLen)
			{
				NewPos = IntersectPos;
				ClosestLen = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}


CEntity *CGameWorld::ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis)
{
	// Find other players
	float ClosestRange = Radius*2;
	CEntity *pClosest = 0;

	CEntity *p = FindFirst(Type);
	for(; p; p = p->TypeNext())
 	{
		if(p == pNotThis)
			continue;

		float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius+Radius)
		{
			if(Len < ClosestRange)
			{
				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}
