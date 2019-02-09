/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"
#include "projectile.h"

CProjectile::CProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, bool Explosive, float Force, int SoundImpact, int Weapon, int SwitchGroup, bool InvertSwitch, bool OnlySelf)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos, 0, SwitchGroup, InvertSwitch)
{
	m_Type = Type;
	m_Direction = Dir;
	m_StartPos = Pos;
	m_StartDir = Dir;
	m_LifeSpan = Span;
	m_Owner = Owner;
	m_Force = Force;
	m_Damage = Damage;
	m_SoundImpact = SoundImpact;
	m_Weapon = Weapon;
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;
	m_OnlySelf = OnlySelf;
	if(m_Type == WEAPON_SHOTGUN)
		m_Persistent = false;
	else
		m_Persistent = true;

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	if(m_Type == WEAPON_SHOTGUN)
	{
		m_Pos = m_StartPos;
		m_Direction = m_StartDir;
		m_StartTick = Server()->Tick();
	}	
	else
		GameWorld()->DestroyEntity(this);
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
			Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			// TODO DDRace remove gun curvature in server AND client without tuning
			Curvature = GameServer()->Tuning()->m_GunCurvature;
			Speed = GameServer()->Tuning()->m_GunSpeed;
			break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CProjectile::Tick()
{
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 ColPos;
	vec2 PreColPos;

	int Collide = Collision()->IntersectLine(PrevPos, CurPos, &ColPos, &PreColPos, CCollision::COLFLAG_SOLID_PROJ);

	if(m_Type == WEAPON_SHOTGUN)
	{
		if(Collide)
		{
			if(Collision()->IntersectLine(vec2(PrevPos.x, ColPos.y), ColPos, 0, 0, CCollision::COLFLAG_SOLID_PROJ))
				m_Direction.x *= -1;
			if(Collision()->IntersectLine(vec2(ColPos.x, PrevPos.y), ColPos, 0, 0, CCollision::COLFLAG_SOLID_PROJ))
				m_Direction.y *= -1;
			m_Pos = PreColPos;
			m_StartTick = Server()->Tick();
		}

		CCharacter *pChr = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
		if(Active()){
			for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
		 	{
				if(pChr && pChr->IsAlive() && distance(CurPos, pChr->GetPos()) < pChr->GetProximityRadius())
					pChr->Freeze();
			}
		}
	}
	else
	{
		CurPos = ColPos;
		CCharacter *pTargetChr;
		if(m_OnlySelf)
			pTargetChr = 0;
		else
			pTargetChr = GameWorld()->IntersectCharacter(PrevPos, CurPos, 6.0f, CurPos, HitCharacter, this);

		m_LifeSpan--;

		if(pTargetChr || Collide || m_LifeSpan < 0 || GameLayerClipped(CurPos))
		{
			if(m_LifeSpan >= 0 || m_Weapon == WEAPON_GRENADE)
				GameServer()->CreateSound(Events(), CurPos, m_SoundImpact, -1, m_OnlySelf ? m_Owner : -1);

			if(m_Explosive)
				GameServer()->CreateExplosion(Events(), GameWorld(), CurPos, m_Owner, m_Weapon, m_Damage, m_OnlySelf);

			else if(pTargetChr)
				pTargetChr->TakeDamage(m_Direction * max(0.001f, m_Force), m_Direction*-1, m_Damage, m_Owner, m_Weapon);

			GameWorld()->DestroyEntity(this);
		}
	}
}

bool CProjectile::HitCharacter(CCharacter *pCharacter, void *pUserData) {
	CProjectile *pProj = (CProjectile *)pUserData;
	return !(pCharacter->GetPlayer()->GetCID() == pProj->m_Owner || pCharacter->Solo() || pProj->m_OnlySelf);
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::FillInfo(CNetObj_DDRaceProjectile *pProj, int WorldID)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x*100.0f);
	pProj->m_VelY = (int)(m_Direction.y*100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
	pProj->m_WorldID = WorldID;
	pProj->m_SoloClientID = m_OnlySelf ? m_Owner : -1;
}

void CProjectile::Snap(int SnappingClient, int WorldID)
{
	if (!WorldVisible(SnappingClient, WorldID))
		return;

	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	if (GameServer()->DoesPlayerHaveDDRaceClient(SnappingClient))
	{
		CNetObj_DDRaceProjectile *pProj = static_cast<CNetObj_DDRaceProjectile *>(Server()->SnapNewItem(NETOBJTYPE_DDRACEPROJECTILE, GetID(), sizeof(CNetObj_DDRaceProjectile)));
		if(pProj)
			FillInfo(pProj, WorldID);
	}
	else if (!m_OnlySelf || m_Owner == SnappingClient)
	{
		CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
		if(pProj)
		{
			CNetObj_DDRaceProjectile pDDRaceProj;
			FillInfo(&pDDRaceProj, WorldID);
			*pProj = pDDRaceProj.ToVanilla();
		}
	}
}
