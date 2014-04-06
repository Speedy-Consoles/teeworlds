/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/mapitems.h>

#include "entities/character.h"
#include "entities/pickup.h"
#include "entities/projectile.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"


CGameController::CGameController(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pServer = m_pGameServer->Server();

	m_NumRealPlayers = 0;

	// game
	m_GameState = IGS_GAME_RUNNING;
	m_PrevGameState = IGS_GAME_RUNNING;
	m_GameStateTimer = TIMER_INFINITE;

	// info
	m_GameFlags = 0;

	// map
	m_aMapWish[0] = 0;

	// spawn
	m_NumSpawnPoints = 0;
}

//activity
void CGameController::DoActivityCheck()
{
	if(g_Config.m_SvInactiveKickTime == 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->IsDummy() && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS &&
			!Server()->IsAuthed(i) && (GameServer()->m_apPlayers[i]->m_InactivityTickCounter > g_Config.m_SvInactiveKickTime*Server()->TickSpeed()*60))
		{
			switch(g_Config.m_SvInactiveKick)
			{
			case 0:
				{
					// move player to spectator
					DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
			case 1:
				{
					// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
					int Spectators = 0;
					for(int j = 0; j < MAX_CLIENTS; ++j)
						if(GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->GetTeam() == TEAM_SPECTATORS)
							++Spectators;
					if(Spectators >= g_Config.m_SvSpectatorSlots)
						Server()->Kick(i, "Kicked for inactivity");
					else
						DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
			case 2:
				{
					// kick the player
					Server()->Kick(i, "Kicked for inactivity");
				}
			}
		}
	}
}

// event
void CGameController::OnCharacterSpawn(CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER);
	pChr->GiveWeapon(WEAPON_GUN);
}

int CGameController::OnCharacterDeath(CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	return 0;
}

bool CGameController::OnEntity(int Index, int Flags, vec2 Pos, int SwitchGroup, bool InvertSwitch)
{
	int Type = -1;

	switch(Index)
	{
	case ENTITY_SPAWN:
		m_aSpawnPoints[m_NumSpawnPoints++] = Pos;
		break;
	case ENTITY_SPAWN_RED:
		break;
	case ENTITY_SPAWN_BLUE:
		break;
	case ENTITY_ARMOR_1:
		Type = PICKUP_ARMOR;
		break;
	case ENTITY_HEALTH_1:
		Type = PICKUP_HEALTH;
		break;
	case ENTITY_WEAPON_SHOTGUN:
		Type = PICKUP_SHOTGUN;
		break;
	case ENTITY_WEAPON_GRENADE:
		Type = PICKUP_GRENADE;
		break;
	case ENTITY_WEAPON_LASER:
		Type = PICKUP_LASER;
		break;
	case ENTITY_POWERUP_NINJA:
		if(g_Config.m_SvPowerups)
			Type = PICKUP_NINJA;
		break;
	case ENTITY_CRAZY_BULLET:
		vec2 Dir = vec2(0.0, 0.0);
		if(Flags&TILEFLAG_ROTATE)
			Dir.x = Flags&TILEFLAG_HFLIP ? -1.0 : 1.0;
		else
			Dir.y = Flags&TILEFLAG_VFLIP ? 1.0 : -1.0;
		for(int i = 0; i < NUM_WORLDS; i++)
			new CProjectile(GameServer()->GetWorld(i), WEAPON_SHOTGUN, -1, Pos, Dir, 0,0, false, 0, SOUND_GUN_FIRE, WEAPON_SHOTGUN, SwitchGroup, InvertSwitch, false);
		break;
	}

	if(Type != -1)
	{
		for(int i = 0; i < NUM_WORLDS; i++)
		{
			CPickup *pPickup = new CPickup(GameServer()->GetWorld(i), Type, SwitchGroup, InvertSwitch);
			pPickup->m_Pos = Pos;
		}
		return true;
	}
	return false;
}

void CGameController::OnPlayerConnect(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();
	pPlayer->Respawn();

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam());
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
		m_NumRealPlayers++;

	// update game info
	UpdateGameInfo(ClientID);
}

void CGameController::OnPlayerDisconnect(CPlayer *pPlayer)
{
	pPlayer->OnDisconnect();

	int ClientID = pPlayer->GetCID();
	if(Server()->ClientIngame(ClientID))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
		m_NumRealPlayers--;
}

void CGameController::ResetWorlds()
{
	for(int i = 0; i < NUM_WORLDS; i++)
		GameServer()->GetWorld(i)->m_ResetRequested = true;
}

void CGameController::PauseWorlds()
{
	for(int i = 0; i < NUM_WORLDS; i++)
		GameServer()->GetWorld(i)->m_Paused = true;
}

void CGameController::UnpauseWorlds()
{
	for(int i = 0; i < NUM_WORLDS; i++)
		GameServer()->GetWorld(i)->m_Paused = false;
}

// general
void CGameController::Snap(int SnappingClient)
{
	CNetObj_GameData *pGameData = static_cast<CNetObj_GameData *>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData)));
	if(!pGameData)
		return;

	pGameData->m_GameStartTick = 0;
	pGameData->m_GameStateFlags = 0;
	pGameData->m_GameStateEndTick = 0; // no timer/infinite = 0, on end = GameEndTick, otherwise = GameStateEndTick
	switch(m_GameState)
	{
	case IGS_WARMUP:
		pGameData->m_GameStateFlags |= GAMESTATEFLAG_WARMUP;
		if(m_GameStateTimer != TIMER_INFINITE)
			pGameData->m_GameStateEndTick = Server()->Tick()+m_GameStateTimer;
		break;
	case IGS_GAME_PAUSED:
		pGameData->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;
		if(m_GameStateTimer != TIMER_INFINITE)
			pGameData->m_GameStateEndTick = Server()->Tick()+m_GameStateTimer;
		break;
	case IGS_GAME_RUNNING:
		break;
	}

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_GameInfo *pGameInfo = static_cast<CNetObj_De_GameInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_GAMEINFO, 0, sizeof(CNetObj_De_GameInfo)));
		if(!pGameInfo)
			return;

		pGameInfo->m_GameFlags = m_GameFlags;
		pGameInfo->m_ScoreLimit = 0;
		pGameInfo->m_TimeLimit = 0;
		pGameInfo->m_MatchNum = 0;
		pGameInfo->m_MatchCurrent = 0;
	}
}

void CGameController::Tick()
{
	// handle game states
	if(m_GameState != IGS_GAME_RUNNING)
	{
		if(m_GameStateTimer > 0)
			--m_GameStateTimer;

		if(m_GameStateTimer == 0)
		{
			// timer fires
			switch(m_GameState)
			{
			case IGS_WARMUP:
				ResetWorlds();
				m_GameState = IGS_GAME_RUNNING;
				break;
			case IGS_GAME_PAUSED:
				UnpauseWorlds();
				m_GameState = m_PrevGameState;
				break;
			case IGS_GAME_RUNNING:
				break;
			}
		}
	}

	// check for inactive players
	DoActivityCheck();
}

void CGameController::DoPause(int Seconds)
{
	m_GameState = IGS_GAME_PAUSED;
	m_GameStateTimer = Seconds * Server()->TickSpeed();
	PauseWorlds();
}

void CGameController::DoWarmup(int Seconds)
{
	m_GameState = IGS_WARMUP;
	m_GameStateTimer = Seconds * Server()->TickSpeed();
}

// info
void CGameController::UpdateGameInfo(int ClientID)
{
	CNetMsg_Sv_GameInfo GameInfoMsg;
	GameInfoMsg.m_GameFlags = m_GameFlags;
	GameInfoMsg.m_ScoreLimit = 0;
	GameInfoMsg.m_TimeLimit = 0;
	GameInfoMsg.m_MatchNum = 0;
	GameInfoMsg.m_MatchCurrent = 0;

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(!GameServer()->m_apPlayers[i] || !Server()->ClientIngame(i))
				continue;

			Server()->SendPackMsg(&GameInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}
	else
		Server()->SendPackMsg(&GameInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);
}

// map
static bool IsSeparator(char c) { return c == ';' || c == ' ' || c == ',' || c == '\t'; }

void CGameController::ChangeMap(const char *pToMap)
{
	str_copy(m_aMapWish, pToMap, sizeof(m_aMapWish));
}

void CGameController::CycleMap()
{
	if(m_aMapWish[0] != 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "rotating map to %s", m_aMapWish);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
		str_copy(g_Config.m_SvMap, m_aMapWish, sizeof(g_Config.m_SvMap));
		m_aMapWish[0] = 0;
		return;
	}
	if(!str_length(g_Config.m_SvMaprotation))
		return;

	// handle maprotation
	const char *pMapRotation = g_Config.m_SvMaprotation;
	const char *pCurrentMap = g_Config.m_SvMap;

	int CurrentMapLen = str_length(pCurrentMap);
	const char *pNextMap = pMapRotation;
	while(*pNextMap)
	{
		int WordLen = 0;
		while(pNextMap[WordLen] && !IsSeparator(pNextMap[WordLen]))
			WordLen++;

		if(WordLen == CurrentMapLen && str_comp_num(pNextMap, pCurrentMap, CurrentMapLen) == 0)
		{
			// map found
			pNextMap += CurrentMapLen;
			while(*pNextMap && IsSeparator(*pNextMap))
				pNextMap++;

			break;
		}

		pNextMap++;
	}

	// restart rotation
	if(pNextMap[0] == 0)
		pNextMap = pMapRotation;

	// cut out the next map
	char aBuf[512] = {0};
	for(int i = 0; i < 511; i++)
	{
		aBuf[i] = pNextMap[i];
		if(IsSeparator(pNextMap[i]) || pNextMap[i] == 0)
		{
			aBuf[i] = 0;
			break;
		}
	}

	// skip spaces
	int i = 0;
	while(IsSeparator(aBuf[i]))
		i++;

	char aBufMsg[256];
	str_format(aBufMsg, sizeof(aBufMsg), "rotating map to %s", &aBuf[i]);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	str_copy(g_Config.m_SvMap, &aBuf[i], sizeof(g_Config.m_SvMap));
}

// spawn
bool CGameController::CanSpawn(int Team, vec2 *pOutPos, int WorldID) const
{
	// spectators can't spawn
	if(Team == TEAM_SPECTATORS || GameServer()->GetWorld(WorldID)->m_Paused || GameServer()->GetWorld(WorldID)->m_ResetRequested)
		return false;

	CSpawnEval Eval;

	EvaluateSpawn(&Eval, WorldID);

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

float CGameController::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos, int WorldID) const
{
	float Score = 0.0f;
	CCharacter *pC = static_cast<CCharacter *>(GameServer()->GetWorld(WorldID)->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
	for(; pC; pC = (CCharacter *)pC->TypeNext())
	{
		float d = distance(Pos, pC->m_Pos);
		Score += d == 0 ? 1000000000.0f : 1.0f / d;
	}

	return Score;
}

void CGameController::EvaluateSpawn(CSpawnEval *pEval, int WorldID) const
{
	// get spawn point
	for(int i = 0; i < m_NumSpawnPoints; i++)
	{
		// check if the position is occupado
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameServer()->GetWorld(WorldID)->FindEntities(m_aSpawnPoints[i], 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) };	// start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			for(int c = 0; c < Num; ++c)
				if(GameServer()->GetWorld(WorldID)->Collision()->GetCollisionAt(m_aSpawnPoints[i]+Positions[Index]) ||
					distance(aEnts[c]->m_Pos, m_aSpawnPoints[i]+Positions[Index]) <= aEnts[c]->m_ProximityRadius)
				{
					Result = -1;
					break;
				}
		}
		if(Result == -1)
			continue;	// try next spawn point

		vec2 P = m_aSpawnPoints[i]+Positions[Result];
		float S = EvaluateSpawnPos(pEval, P, WorldID);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

// team
void CGameController::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	if(Team == pPlayer->GetTeam() || Team == TEAM_BLUE)
		return;

	if(Team == TEAM_SPECTATORS)
		m_NumRealPlayers--;
	else if(pPlayer->GetTeam() == TEAM_SPECTATORS)
		m_NumRealPlayers++;

	pPlayer->SetTeam(Team);

	int ClientID = pPlayer->GetCID();

	// notify clients
	CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Team = Team;
	Msg.m_Silent = DoChatMsg ? 0 : 1;
	Msg.m_CooldownTick = pPlayer->m_TeamChangeTick;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", ClientID, Server()->ClientName(ClientID), Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// update effected game settings
	GameServer()->OnClientTeamChange(ClientID);
}
