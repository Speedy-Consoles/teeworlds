/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include "eventhandler.h"
#include "gamecontext.h"
#include "player.h"

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
CEventHandler::CEventHandler()
{
	m_pGameServer = 0;
	Clear();
}

void CEventHandler::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void *CEventHandler::Create(int Type, int Size, int DDRaceType, int DDRaceSize, void ** pDDRaceData, int64 Mask)
{
	if(m_NumEvents == MAX_EVENTS)
		return 0;
	if(m_CurrentOffset+Size >= MAX_DATASIZE)
		return 0;
	if(m_CurrentDDRaceOffset+DDRaceSize >= MAX_DATASIZE)
		return 0;

	void *p = &m_aData[m_CurrentOffset];
	m_aOffsets[m_NumEvents] = m_CurrentOffset;
	m_aTypes[m_NumEvents] = Type;
	m_aSizes[m_NumEvents] = Size;
	m_aClientMasks[m_NumEvents] = Mask;
	m_CurrentOffset += Size;

	*pDDRaceData = &m_aDDRaceData[m_CurrentDDRaceOffset];
	m_aDDRaceOffsets[m_NumEvents] = m_CurrentDDRaceOffset;
	m_aDDRaceTypes[m_NumEvents] = DDRaceType;
	m_aDDRaceSizes[m_NumEvents] = DDRaceSize;
	m_CurrentDDRaceOffset += DDRaceSize;

	m_NumEvents++;
	return p;
}

void CEventHandler::Clear()
{
	m_NumEvents = 0;
	m_CurrentOffset = 0;
	m_CurrentDDRaceOffset = 0;
}

void CEventHandler::Snap(int SnappingClient, int WorldID)
{
	bool HasDDRace = GameServer()->DoesPlayerHaveDDRaceClient(SnappingClient);
	bool SameWorld = GameServer()->GetPlayerWorldID(SnappingClient) == WorldID;

	for(int i = 0; i < m_NumEvents; i++)
	{
		if(SnappingClient == -1 || CmaskIsSet(m_aClientMasks[i], SnappingClient))
		{
			if(!HasDDRace || m_aDDRaceSizes[i] == 0)
			{
				bool Opaque = true;
				if(SnappingClient != -1 && m_aDDRaceSizes[i] > 0)
				{
					CNetEvent_DDRaceCommon *ddrev = (CNetEvent_DDRaceCommon *)&m_aDDRaceData[m_aDDRaceOffsets[i]];
					Opaque = SameWorld && (ddrev->m_SoloClientID == -1 || ddrev->m_SoloClientID == SnappingClient);
				}

				if (Opaque && m_aSizes[i] > 0)
				{
					CNetEvent_Common *ev = (CNetEvent_Common *)&m_aData[m_aOffsets[i]];
					if(SnappingClient == -1 || distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, vec2(ev->m_X, ev->m_Y)) < 1500.0f)
					{
						void *d = GameServer()->Server()->SnapNewItem(m_aTypes[i], i, m_aSizes[i]);
						if(d)
							mem_copy(d, &m_aData[m_aOffsets[i]], m_aSizes[i]);
					}
				}
			}
			else
			{
				CNetEvent_DDRaceCommon *ddrev = (CNetEvent_DDRaceCommon *)&m_aDDRaceData[m_aDDRaceOffsets[i]];
				ddrev->m_WorldID = WorldID;
				if(SnappingClient == -1 || distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, vec2(ddrev->m_X, ddrev->m_Y)) < 1500.0f)
				{
					void *d = GameServer()->Server()->SnapNewItem(m_aDDRaceTypes[i], i, m_aDDRaceSizes[i]);
					if(d)
						mem_copy(d, &m_aDDRaceData[m_aDDRaceOffsets[i]], m_aDDRaceSizes[i]);
				}
			}
		}
	}
}
