#include "teamcore.h"
#include "gamecontext.h"
#include "player.h"

CTeamCore::CTeamCore()
{
}

void CTeamCore::InitCollision(class CLayers *pLayers)
{
	m_aTeams[0].m_Collision.Init(pLayers, m_aTeams[0].m_TeamWorld.m_aSwitchStates);
	for(int i = 1; i < MAX_CLIENTS; i++)
		m_aTeams[i].m_Collision.Init(&m_aTeams[0].m_Collision, m_aTeams[i].m_TeamWorld.m_aSwitchStates);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aTeams[i].m_TeamWorld.SetCollision(&m_aTeams[i].m_Collision);
		m_aTeams[i].m_TeamWorld.SetEvents(&m_aTeams[i].m_Events);
	}
}

void CTeamCore::Tick()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aTeams[i].m_TeamWorld.Tick();
}

void CTeamCore::SetTeamMode(int Team, int Mode)
{
	m_aTeams[Team].m_Mode = Mode;
}

void CTeamCore::SetTeamState(int Team, int State)
{
	m_aTeams[Team].m_State = State;
}

void CTeamCore::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aTeams[i].m_TeamWorld.SetGameServer(pGameServer);
		m_aTeams[i].m_Events.SetGameServer(pGameServer);
	}
}

void CTeamCore::Snap(int SnappingClient)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aTeams[i].m_TeamWorld.Snap(SnappingClient, i);
		m_aTeams[i].m_Events.Snap(SnappingClient, i);
	}
}

void CTeamCore::PostSnap()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aTeams[i].m_TeamWorld.PostSnap();
		m_aTeams[i].m_Events.Clear();
	}
}

void CTeamCore::Reset(int Team)
{
	if(Team >= 0 && Team < MAX_CLIENTS)
		m_aTeams[Team].m_TeamWorld.m_ResetRequested = true;
	else if(Team == -1) {
		for(int i = 0; i < MAX_CLIENTS; i++)
			m_aTeams[i].m_TeamWorld.m_ResetRequested = true;
	}
}
