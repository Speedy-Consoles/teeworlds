/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEWORLD_H
#define GAME_SERVER_GAMEWORLD_H

#include <game/gamecore.h>
#include "eventhandler.h"

class CEntity;
class CCharacter;

typedef bool (*FnCountCharacter)(CCharacter *character, void *UserData);

/*
	Class: Game World
		Tracks all entities in the game. Propagates tick and
		snap calls to all entities.
*/
class CGameWorld
{
public:
	enum
	{
		ENTTYPE_PROJECTILE = 0,
		ENTTYPE_LASER,
		ENTTYPE_PICKUP,
		ENTTYPE_CHARACTER,
		ENTTYPE_FLAG,
		NUM_ENTTYPES,

		RACESTATE_STARTING = 0,
		RACESTATE_STARTED,
		RACESTATE_FINISHED,
		RACESTATE_CANCELED,
		
		TEAMMODE_OPEN = 0,
		TEAMMODE_PRIVATE,
	};

private:
	void Reset(bool Soft);
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	class CGameContext *m_pGameServer;
	class CCollision m_Collision;
	class CEventHandler m_Events;

	class IServer *m_pServer;

	int m_RaceState;
	int m_RaceStartTick;
public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class CCollision *Collision() { return &m_Collision; }
	class CEventHandler *Events() { return &m_Events; }
	class IServer *Server() { return m_pServer; }
	void SetDefault() { m_Default = true; }

	bool m_ResetRequested;
	bool m_SoftResetRequested;
	bool m_Paused;
	CWorldCore m_Core;
	bool m_aNextSwitchStates[255];
	bool m_aDDRaceSwitchStates[255];
	int m_aDDRaceSwitchTicks[255];
	bool m_SwitchStateChanged;
	bool m_Default;

	void SetSwitchState(bool State, int GroupID, int Duration);

	int RaceState() { return m_RaceState; }
	void StartRace();
	void OnPlayerDeath();
	void OnFinish();

	CGameWorld();
	~CGameWorld();

	void InitCollision(class CLayers *pLayers) { m_Collision.Init(pLayers, m_aDDRaceSwitchStates); }
	void InitCollision(class CCollision *pOther) { m_Collision.Init(pOther, m_aDDRaceSwitchStates); }

	void SetGameServer(CGameContext *pGameServer);

	CEntity *FindFirst(int Type);

	/*
		Function: find_entities
			Finds entities close to a position and returns them in a list.

		Arguments:
			pos - Position.
			radius - How close the entities have to be.
			ents - Pointer to a list that should be filled with the pointers
				to the entities.
			max - Number of entities that fits into the ents array.
			type - Type of the entities to find.

		Returns:
			Number of entities found and added to the ents array.
	*/
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type);

	/*
		Function: closest_CEntity
			Finds the closest CEntity of a type to a specific point.

		Arguments:
			pos - The center position.
			radius - How far off the CEntity is allowed to be
			type - Type of the entities to find.
			notthis - Entity to ignore

		Returns:
			Returns a pointer to the closest CEntity or NULL if no CEntity is close enough.
	*/
	CEntity *ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis);

	/*
		Function: interserct_CCharacter
			Finds the closest CCharacter that intersects the line.

		Arguments:
			pos0 - Start position
			pos2 - End position
			radius - How for from the line the CCharacter is allowed to be.
			new_pos - Intersection position
			pfnHitCharacterCB - Callback that decides if the character will count

		Returns:
			Returns a pointer to the closest hit or NULL of there is no intersection.
	*/
	class CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, FnCountCharacter pfnCountCharacterCB, void *pUserData);

	/*
		Function: insert_entity
			Adds an entity to the world.

		Arguments:
			entity - Entity to add
	*/
	void InsertEntity(CEntity *pEntity);

	/*
		Function: remove_entity
			Removes an entity from the world.

		Arguments:
			entity - Entity to remove
	*/
	void RemoveEntity(CEntity *pEntity);

	/*
		Function: destroy_entity
			Destroys an entity in the world.

		Arguments:
			entity - Entity to destroy
	*/
	void DestroyEntity(CEntity *pEntity);

	/*
		Function: snap
			Calls snap on all the entities in the world to create
			the snapshot.

		Arguments:
			snapping_client - ID of the client which snapshot
			is being created.
			world_id - ID that this world should be identified with
	*/
	void Snap(int SnappingClient, int WorldID);
	
	void PostSnap();

	/*
		Function: tick
			Calls tick on all the entities in the world to progress
			the world to the next tick.

	*/
	void Tick();
};

#endif
