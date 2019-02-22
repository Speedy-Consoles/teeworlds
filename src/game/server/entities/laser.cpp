/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>

#include "character.h"
#include "laser.h"
#include <game/server/player.h>

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, bool Pull, bool OnlySelf)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 0, 0, false)
{
	m_Owner = Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_Pull = Pull;
	m_OnlySelf = OnlySelf;
	m_Persistent = true;
	GameWorld()->InsertEntity(this);
	DoBounce();
}


bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pHit;
	if(m_OnlySelf && m_Bounces == 0)
		return false;

	pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, HitCharacter, this);
	if(!pHit)
		return false;

	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	vec2 Force(0.f, 0.f);
	if(m_Pull)
		Force = normalize(From-To)*10;
	pHit->TakeDamage(Force, normalize(To-From), g_pData->m_Weapons.m_aId[WEAPON_LASER].m_Damage, m_Owner, WEAPON_LASER);
	return true;
}

bool CLaser::HitCharacter(CCharacter *pCharacter, void *pUserData) {
	CLaser *pLaser = (CLaser *)pUserData;
	if(pCharacter->GetPlayer()->GetCID() == pLaser->m_Owner)
		return pLaser->m_Bounces > 0;
	else
		return !pLaser->m_OnlySelf && !pCharacter->Solo();
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	if(Collision()->IntersectLine(m_Pos, To, 0x0, &To, CCollision::COLFLAG_SOLID_PROJ, !GameServer()->IsDDRace()))
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0, CCollision::COLFLAG_SOLID_PROJ, !GameServer()->IsDDRace());
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			m_Energy -= distance(m_From, m_Pos) + GameServer()->Tuning()->m_LaserBounceCost;
			m_Bounces++;

			if(m_Bounces > GameServer()->Tuning()->m_LaserBounceNum)
				m_Energy = -1;

			GameServer()->CreateSound(Events(), m_Pos, SOUND_LASER_BOUNCE, m_OnlySelf ? m_Owner : -1);
		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}
}

void CLaser::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CLaser::Tick()
{
	if(Server()->Tick() > m_EvalTick+(Server()->TickSpeed()*GameServer()->Tuning()->m_LaserBounceDelay)/1000.0f)
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient, int WorldID)
{
	if(!WorldVisible(SnappingClient, WorldID))
		return;

	if(NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_From))
		return;

	CNetObj_Laser *pVanillaObj = nullptr;
	CNetObj_DDRaceLaser *pObj;
	CNetObj_DDRaceLaser DummyObj;
	if(GameServer()->DoesPlayerHaveDDRaceClient(SnappingClient))
	{
		pObj = static_cast<CNetObj_DDRaceLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDRACELASER, GetID(), sizeof(CNetObj_DDRaceLaser)));
		if(!pObj)
			return;
	}
	else if(SoloVisible(SnappingClient, m_OnlySelf, m_Owner))
	{
		pVanillaObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pVanillaObj)
			return;
		pObj = &DummyObj;
	}
	else
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	pObj->m_StartTick = m_EvalTick;
	pObj->m_WorldID = WorldID;
	pObj->m_SoloClientID = m_OnlySelf ? m_Owner : -1;

	if(pVanillaObj)
		*pVanillaObj = pObj->ToVanilla();
}
