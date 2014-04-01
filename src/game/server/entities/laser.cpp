/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>

#include "character.h"
#include "laser.h"
#include <game/server/player.h>

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, bool Pull, bool OnlySelf)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, 0, false)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_Pull = Pull;
	m_OnlySelf = OnlySelf;
	GameWorld()->InsertEntity(this);
	DoBounce();
}


bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pHit;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(m_OnlySelf && m_Bounces == 0)
		return false;
	else if(m_OnlySelf)
		pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pOwnerChar, true);
	else if(m_Bounces == 0)
		pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pOwnerChar, false);
	else
		pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, 0, false);

	if(!pHit)
		return false;

	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	if(m_Pull)
		pHit->TakeDamage(normalize(From-To)*10, GameServer()->Tuning()->m_LaserDamage/2, m_Owner, WEAPON_SHOTGUN);
	else
		pHit->TakeDamage(vec2(0.f, 0.f), GameServer()->Tuning()->m_LaserDamage, m_Owner, WEAPON_LASER);
	return true;
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

	if(Collision()->IntersectLine(m_Pos, To, 0x0, &To, CCollision::COLFLAG_SOLID_PROJ))
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0, CCollision::COLFLAG_SOLID_PROJ);
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			m_Energy -= distance(m_From, m_Pos) + GameServer()->Tuning()->m_LaserBounceCost;
			m_Bounces++;

			if(m_Bounces > GameServer()->Tuning()->m_LaserBounceNum)
				m_Energy = -1;

			CGameContext::CreateSound(Events(), m_Pos, SOUND_LASER_BOUNCE);
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

void CLaser::Snap(int SnappingClient, int World)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	pObj->m_StartTick = m_EvalTick;
	pObj->m_World = World;
}
