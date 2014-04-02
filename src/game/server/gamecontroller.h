/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>

#include <game/generated/protocol.h>

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class CGameController
{
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	// activity
	void DoActivityCheck();
	bool GetPlayersReadyState();
	void SetPlayersReadyState(bool ReadyState);

	// game
	enum EGameState
	{
		IGS_WARMUP,
		IGS_GAME_PAUSED,
		IGS_GAME_RUNNING,
 	};
	EGameState m_GameState;
	EGameState m_PrevGameState;
	int m_GameStateTimer;

	void ResetWorlds();
	void PauseWorlds();
	void UnpauseWorlds();

	// map
	char m_aMapWish[128];
	
	void CycleMap();

	// spawn
	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_Pos = vec2(100,100);
		}

		vec2 m_Pos;
		bool m_Got;
		float m_Score;
	};
	vec2 m_aSpawnPoints[64];
	int m_NumSpawnPoints;
	
	int m_NumRealPlayers;
	
	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos, int WorldID) const;
	void EvaluateSpawn(CSpawnEval *pEval, int WorldID) const;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return m_pServer; }

	// info
	int m_GameFlags;

	void UpdateGameInfo(int ClientID);

public:
	CGameController(class CGameContext *pGameServer);
	~CGameController() {};

	// event
	/*
		Function: on_CCharacter_death
			Called when a CCharacter in the world dies.

		Arguments:
			victim - The CCharacter that died.
			killer - The player that killed it.
			weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.
	*/
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	/*
		Function: on_CCharacter_spawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			chr - The CCharacter that was spawned.
	*/
	void OnCharacterSpawn(class CCharacter *pChr);

	/*
		Function: on_entity
			Called when the map is loaded to process an entity
			in the map.

		Arguments:
			index - Entity index.
			index - Tile flags.
			pos - Where the entity is located in the world.
			SwitchGroup - The switch group ID the entity shall have.
			InvertSwitch - Whether entity should react inverted to switches.

		Returns:
			bool?
	*/
	bool OnEntity(int Index, int Flags, vec2 Pos, int SwitchGroup, bool InvertSwitch);

	void OnPlayerConnect(class CPlayer *pPlayer);
	void OnPlayerDisconnect(class CPlayer *pPlayer);
	void OnPlayerInfoChange(class CPlayer *pPlayer);

	// game
	enum
	{
		TIMER_INFINITE = -1,
		TIMER_END = 10,
	};

	void DoPause(int Seconds);
	void DoWarmup(int Seconds);

	// general
	void Snap(int SnappingClient);
	void Tick();

	// info
	bool IsGamePaused() const { return m_GameState == IGS_GAME_PAUSED; }
	int GetRealPlayerNum() { return m_NumRealPlayers; }

	// map
	void ChangeMap(const char *pToMap);

	//spawn
	bool CanSpawn(int Team, vec2 *pPos, int WorldID) const;
	bool GetStartRespawnState() const;

	// team
	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg=true);
};

#endif
