/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"

//input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}


MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize, -1, false)
{
	m_Health = 0;
	m_Armor = 0;
	m_TriggeredEvents = 0;
	m_TriggeredDDRaceEvents = 0;
	m_Persistent = true;
	m_RaceTimer.SetServer(Server());
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastNinjaSound = -1;
	m_LastFreezeCrySound = -1;
	m_ActiveWeapon = WEAPON_GUN;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_LastCheckpoint = -1;
	m_LastCorrectCheckpoint = -1;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameWorld()->m_Core, Collision());
	m_Core.m_Pos = m_Pos;
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameWorld()->InsertEntity(this);
	m_Alive = true;

	GameServer()->m_pController->OnCharacterSpawn(this);

	m_Nohit = false;

	return true;
}

void CCharacter::Destroy()
{
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::MoveToWorld(CGameWorld *pWorld)
{
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	CEntity::MoveToWorld(pWorld);
	m_Core.Init(&pWorld->m_Core, pWorld->Collision());
	pWorld->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_ActiveWeapon)
		return;

	m_LastWeapon = m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_ActiveWeapon = W;
	GameServer()->CreateSound(Events(), m_Pos, SOUND_WEAPON_SWITCH, -1, m_Core.m_Solo ? m_pPlayer->GetCID() : -1);

	if(m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
		m_ActiveWeapon = 0;
	m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
}

bool CCharacter::IsGrounded()
{
	// TODO DDRace why?
	if(Collision()->TestHLineMove(m_Pos + vec2(0, GetProximityRadius() / 2 + 5), m_Pos + vec2(0, GetProximityRadius() / 2), GetProximityRadius(), !GameServer()->IsDDRace()))
		return true;
	return false;
}


void CCharacter::HandleNinja()
{
	if(m_ActiveWeapon != WEAPON_NINJA)
		return;

	if ((Server()->Tick() - m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
	{
		// time's up, return
		m_aWeapons[WEAPON_NINJA].m_Got = false;
		m_ActiveWeapon = m_LastWeapon;
		
		// reset velocity
		if(m_Ninja.m_CurrentMoveTime > 0)
			m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;

		SetWeapon(m_ActiveWeapon);
		return;
	}

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;

		CCollision::CTriggers aTriggers[4 * (int)((MAX_SPEED + 15) / 16) + 2];
		int Size = Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, aTriggers, vec2(GetProximityRadius(), GetProximityRadius()), 0.f, !GameServer()->IsDDRace());
		for(int i = 0; i < Size; i++)
		{
			m_Core.HandleTriggers(aTriggers[i]);
			HandleTriggers(aTriggers[i]);
		}
		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = GetProximityRadius() * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameWorld()->FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) > (GetProximityRadius() * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameServer()->CreateSound(Events(), aEnts[i]->m_Pos, SOUND_NINJA_HIT, SoloClientID());
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

				aEnts[i]->TakeDamage(vec2(0, -10.0f), m_Ninja.m_ActivationDir*-1, g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}

	return;
}


void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_ActiveWeapon == WEAPON_GRENADE || m_ActiveWeapon == WEAPON_SHOTGUN || m_ActiveWeapon == WEAPON_LASER)
		FullAuto = true;

	// check if we gonna fire
	bool WillFire = false;
	if(m_Core.m_FreezeTick == 0)
	{
		if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
			WillFire = true;

		if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_ActiveWeapon].m_Ammo)
			WillFire = true;

		if(m_UnfreezeFire)
		{
			WillFire = true;
			m_UnfreezeFire = false;
		}
		
	}
	else if((m_LatestInput.m_Fire&1) && CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
	{
		m_UnfreezeFire = true;
		if(m_LastFreezeCrySound+Server()->TickSpeed() <= Server()->Tick())
		{
			GameServer()->CreateSound(Events(), m_Pos, SOUND_PLAYER_PAIN_LONG, SoloClientID());
			m_LastFreezeCrySound = Server()->Tick();
		}
			
	}
	else if(!(m_LatestInput.m_Fire&1))
		m_UnfreezeFire = false;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_ActiveWeapon].m_Ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		if(m_LastNoAmmoSound+Server()->TickSpeed() <= Server()->Tick())
		{
			GameServer()->CreateSound(Events(), m_Pos, SOUND_WEAPON_NOAMMO, SoloClientID());
			m_LastNoAmmoSound = Server()->Tick();
		}
		return;
	}

	vec2 HammerStartPos = m_Pos+Direction*GetProximityRadius()*0.75f;
	vec2 ProjStartPos = HammerStartPos;
	// dirty fix
	Collision()->IntersectLine(m_Pos, ProjStartPos, 0, &ProjStartPos, CCollision::COLFLAG_SOLID_PROJ, !GameServer()->IsDDRace());

	switch(m_ActiveWeapon)
	{
		case WEAPON_HAMMER:
		{
			// reset objects Hit
			m_NumObjectsHit = 0;
			GameServer()->CreateSound(Events(), m_Pos, SOUND_HAMMER_FIRE, -1, m_Core.m_Solo ? m_pPlayer->GetCID() : -1);

			CCharacter *apEnts[MAX_CLIENTS];
			int Hits = 0;
			int Num = GameWorld()->FindEntities(HammerStartPos, GetProximityRadius()*0.5f, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			if(!m_Nohit && !m_Core.m_Solo)
			{
				for (int i = 0; i < Num; ++i)
				{
					CCharacter *pTarget = apEnts[i];

					if (pTarget == this || pTarget->m_Core.m_Solo)
						continue;

					// set his velocity to fast upward (for now)
					if(length(pTarget->m_Pos-ProjStartPos) > 0.0f)
						GameServer()->CreateHammerHit(Events(), pTarget->m_Pos-normalize(pTarget->m_Pos-ProjStartPos)*GetProximityRadius()*0.5f, SoloClientID());
					else
						GameServer()->CreateHammerHit(Events(), ProjStartPos, SoloClientID());

					vec2 Dir;
					if (length(pTarget->m_Pos - m_Pos) > 0.0f)
						Dir = normalize(pTarget->m_Pos - m_Pos);
					else
						Dir = vec2(0.f, -1.f);

					pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, Dir*-1, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
						m_pPlayer->GetCID(), m_ActiveWeapon);
					Hits++;
				}
			}

			// if we Hit anything, we have to wait for the reload
			if(Hits)
				m_ReloadTimer = Server()->TickSpeed()/3;

		} break;

		case WEAPON_GUN:
		{
			new CProjectile(GameWorld(), WEAPON_GUN,
				m_pPlayer->GetCID(),
				ProjStartPos,
				Direction,
				(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime),
				g_pData->m_Weapons.m_Gun.m_pBase->m_Damage, false, 0, -1, WEAPON_GUN, 0, false, m_Nohit || m_Core.m_Solo);

			GameServer()->CreateSound(Events(), m_Pos, SOUND_GUN_FIRE, -1, m_Core.m_Solo ? m_pPlayer->GetCID() : -1);
		} break;

		case WEAPON_SHOTGUN:
		{
			new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(), true, m_Nohit || m_Core.m_Solo);
			GameServer()->CreateSound(Events(), m_Pos, SOUND_SHOTGUN_FIRE, -1, m_Core.m_Solo ? m_pPlayer->GetCID() : -1);
		} break;

		case WEAPON_GRENADE:
		{
			new CProjectile(GameWorld(), WEAPON_GRENADE,
				m_pPlayer->GetCID(),
				ProjStartPos,
				Direction,
				(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime),
				g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE, 0, false, m_Nohit || m_Core.m_Solo);

			GameServer()->CreateSound(Events(), m_Pos, SOUND_GRENADE_FIRE, -1, m_Core.m_Solo ? m_pPlayer->GetCID() : -1);
		} break;

		case WEAPON_LASER:
		{
			new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(), false, m_Nohit || m_Core.m_Solo);
			GameServer()->CreateSound(Events(), m_Pos, SOUND_LASER_FIRE, -1, m_Core.m_Solo ? m_pPlayer->GetCID() : -1);
		} break;

		case WEAPON_NINJA:
		{
			// reset Hit objects
			m_NumObjectsHit = 0;

			m_Ninja.m_ActivationDir = Direction;
			m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
			m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

			GameServer()->CreateSound(Events(), m_Pos, SOUND_NINJA_FIRE, -1, m_Core.m_Solo ? m_pPlayer->GetCID() : -1);
		} break;

	}

	m_AttackTick = Server()->Tick();

	if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0) // -1 == unlimited
		m_aWeapons[m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
		m_ReloadTimer = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / 1000;
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();

	return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	// Ammo: -1 = infinite
	int CurrentAmmo = m_aWeapons[Weapon].m_Ammo;
	if((CurrentAmmo >= 0 && CurrentAmmo < g_pData->m_Weapons.m_aId[Weapon].m_Maxammo) || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		m_aWeapons[Weapon].m_Ammo = min(g_pData->m_Weapons.m_aId[Weapon].m_Maxammo, Ammo);
		return true;
	}
	return false;
}

void CCharacter::GiveNinja()
{
	if(!m_aWeapons[WEAPON_NINJA].m_Got)
		m_Ninja.m_CurrentMoveTime = -1;
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if (m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_ActiveWeapon;
	m_ActiveWeapon = WEAPON_NINJA;

	if(m_Ninja.m_ActivationTick < Server()->Tick()-1 && m_LastNinjaSound < Server()->Tick() - Server()->TickSpeed() * 0.7f)
	{
		GameServer()->CreateSound(Events(), m_Pos, SOUND_PICKUP_NINJA, SoloClientID());
		m_LastNinjaSound = Server()->Tick();
	}
	m_Ninja.m_ActivationTick = Server()->Tick();
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

int CCharacter::SoloClientID()
{
	return m_Core.m_Solo ? GetPlayer()->GetCID() : -1;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	m_Core.m_Input = m_Input;
	m_Core.Tick(true, !GameServer()->IsDDRace());

	// handle death-tiles and leaving gamelayer
	if(Collision()->GetCollisionAt(m_Pos.x+GetProximityRadius()/3.f, m_Pos.y-GetProximityRadius()/3.f, !GameServer()->IsDDRace())&CCollision::COLFLAG_DEATH ||
		Collision()->GetCollisionAt(m_Pos.x+GetProximityRadius()/3.f, m_Pos.y+GetProximityRadius()/3.f, !GameServer()->IsDDRace())&CCollision::COLFLAG_DEATH ||
		Collision()->GetCollisionAt(m_Pos.x-GetProximityRadius()/3.f, m_Pos.y-GetProximityRadius()/3.f, !GameServer()->IsDDRace())&CCollision::COLFLAG_DEATH ||
		Collision()->GetCollisionAt(m_Pos.x-GetProximityRadius()/3.f, m_Pos.y+GetProximityRadius()/3.f, !GameServer()->IsDDRace())&CCollision::COLFLAG_DEATH ||
		GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	// handle Weapons
	HandleWeapons();
}

void CCharacter::TickDefered()
{
	// TODO DDRace we should have a separate reckoning core for ddrace clients
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, Collision());
		m_ReckoningCore.Tick(false, true);
		m_ReckoningCore.Move(true);
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f), !GameServer()->IsDDRace());

	CCollision::CTriggers aTriggers[4 * (int)((MAX_SPEED + 15) / 16) + 2];
	int Size = m_Core.Move(aTriggers, !GameServer()->IsDDRace());
	for(int i = 0; i < Size; i++)
		HandleTriggers(aTriggers[i]);

	bool StuckAfterMove = Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f), !GameServer()->IsDDRace());
	m_Core.Quantize();
	bool StuckAfterQuant = Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f), !GameServer()->IsDDRace());
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}
	
	m_TriggeredEvents |= m_Core.m_TriggeredEvents;
	m_TriggeredDDRaceEvents |= m_Core.m_TriggeredDDRaceEvents;

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_DDRaceCharacter Predicted;
		CNetObj_DDRaceCharacter Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		// TODO DDRace is m_SwitchStateChanged really necessary
		if(m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_DDRaceCharacter)) != 0 || GameWorld()->m_SwitchStateChanged)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

void CCharacter::HandleTriggers(CCollision::CTriggers Triggers)
{
	if(Triggers.m_SwitchFlags&CCollision::TRIGGERFLAG_SWITCH)
		GameWorld()->SetSwitchState(Triggers.m_SwitchState, Triggers.m_SwitchGroup, Triggers.m_SwitchDuration);

	if(Triggers.m_TeleFlags&CCollision::TRIGGERFLAG_TELEPORT)
	{
		GameServer()->CreatePlayerTeleport(Events(), Triggers.m_TeleInPos, SoloClientID());
		GameServer()->CreatePlayerTeleport(Events(), Triggers.m_TeleOutPos, SoloClientID());
	}
	if(Triggers.m_TeleFlags&CCollision::TRIGGERFLAG_STOP_NINJA)
		m_Ninja.m_CurrentMoveTime = -1;

	int Checkpoint = Triggers.m_Checkpoint;
	if(Checkpoint == 0)
	{
		m_LastCheckpoint = -1;
		m_LastCorrectCheckpoint = -1;
		GameServer()->m_pController->OnRaceStart(this);
	}
	else if(Checkpoint >= 0 && Checkpoint - 1 != m_LastCheckpoint && m_RaceTimer.State() == CRaceTimer::RACESTATE_STARTED)
	{
		m_LastCheckpoint = Checkpoint - 1;
		if(Checkpoint - 2 == m_LastCorrectCheckpoint)
		{
			if(Checkpoint == Collision()->GetNumCheckpoints() + 1)
				GameServer()->m_pController->OnRaceFinish(this);
			else
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "You reached Checkpoint %d in %.2f seconds.", m_LastCheckpoint, m_RaceTimer.Time());
				GameServer()->SendChat(-1, CHAT_WHISPER, m_pPlayer->GetCID(), aBuf);
				// TODO DDRace store checkpoint time
				m_LastCorrectCheckpoint++;
			}
		}
		else if(Checkpoint - 2 > m_LastCorrectCheckpoint)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You missed checkpoint %d", m_LastCorrectCheckpoint + 1);
			GameServer()->SendChat(-1, CHAT_WHISPER, m_pPlayer->GetCID(), aBuf);
		}
		else if(Checkpoint - 1 < m_LastCorrectCheckpoint)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Wrong direction!");
			GameServer()->SendChat(-1, CHAT_WHISPER, m_pPlayer->GetCID(), aBuf);
		}
	}

	if(Triggers.m_Nohit == CCollision::PROPERTEE_ON)
		m_Nohit = true;
	else if(Triggers.m_Nohit == CCollision::PROPERTEE_OFF)
		m_Nohit = false;
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	m_RaceTimer.TickPaused();
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart > -1)
		++m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;
	m_Health = clamp(m_Health+Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	// we got to wait 0.5 secs before respawning
	m_Alive = false;
	m_pPlayer->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = ModeSpecial;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	// a nice sound
	GameServer()->CreateSound(Events(), m_Pos, SOUND_PLAYER_DIE, SoloClientID());

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();

	GameWorld()->RemoveEntity(this);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(Events(), m_Pos, m_pPlayer->GetCID(), SoloClientID());
}

bool CCharacter::TakeDamage(vec2 Force, vec2 Source, int Dmg, int From, int Weapon)
{
	m_Core.m_Vel += Force;

	if(GameServer()->m_pController->IsFriendlyFire(m_pPlayer->GetCID(), From))
		return false;

	if(Weapon == WEAPON_LASER || Weapon == WEAPON_HAMMER)
		Unfreeze();

	// m_pPlayer only inflicts half damage on self
	if(From == m_pPlayer->GetCID() && Dmg)
		Dmg = max(1, Dmg/2);

	int OldHealth = m_Health, OldArmor = m_Armor;
	if(Dmg && !GameServer()->IsDDRace())
	{
		if(m_Armor)
		{
			if(Dmg > 1)
			{
				m_Health--;
				Dmg--;
			}

			if(Dmg > m_Armor)
			{
				Dmg -= m_Armor;
				m_Armor = 0;
			}
			else
			{
				m_Armor -= Dmg;
				Dmg = 0;
			}
		}

		m_Health -= Dmg;
	}

	// create healthmod indicator
	if (!GameServer()->IsDDRace())
		GameServer()->CreateDamage(Events(), m_Pos, m_pPlayer->GetCID(), Source, OldHealth-m_Health, OldArmor-m_Armor, From == m_pPlayer->GetCID(), SoloClientID());

	// do damage Hit sound
	if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
	{
		int64 Mask = CmaskOne(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||  GameServer()->m_apPlayers[i]->m_DeadSpecMode) &&
				GameServer()->m_apPlayers[i]->GetSpectatorID() == From)
				Mask |= CmaskOne(i);
		}
		if(Dmg > 0 && !GameServer()->IsDDRace())
			GameServer()->CreateSound(Events(), GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, SoloClientID(), Mask);
	}

	// check for death
	if(m_Health <= 0)
	{
		Die(From, Weapon);

		// set attacker's face to happy (taunt!)
		if (From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
		{
			CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
			if (pChr)
			{
				pChr->m_EmoteType = EMOTE_HAPPY;
				pChr->m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
			}
		}

		return false;
	}

	if (Dmg > 2 && !GameServer()->IsDDRace())
		GameServer()->CreateSound(Events(), m_Pos, SOUND_PLAYER_PAIN_LONG, SoloClientID());
	else if (Dmg > 0 && !GameServer()->IsDDRace())
		GameServer()->CreateSound(Events(), m_Pos, SOUND_PLAYER_PAIN_SHORT, SoloClientID());

	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

	return true;
}

void CCharacter::Freeze()
{
	m_Core.Freeze();
}

void CCharacter::Unfreeze()
{
	m_Core.m_UnfreezeOnNextTick = true;
}

void CCharacter::DeepFreeze()
{
	m_Core.DeepFreeze();
}

void CCharacter::DeepUnfreeze()
{
	m_Core.DeepUnfreeze();
}

void CCharacter::Snap(int SnappingClient, int WorldID)
{
	m_LastSnapWorld = WorldID;

	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character *pVanillaCharacter = nullptr;
	CNetObj_DDRaceCharacter TmpCharacter;
	CNetObj_DDRaceCharacter *pCharacter;
	if(GameServer()->DoesPlayerHaveDDRaceClient(SnappingClient))
	{
		pCharacter = static_cast<CNetObj_DDRaceCharacter *>(Server()->SnapNewItem(NETOBJTYPE_DDRACECHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_DDRaceCharacter)));
		if (!pCharacter)
			return;
	}
	else
	{
		if (!WorldVisible(SnappingClient, WorldID))
			return;

		pVanillaCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_Character)));
		if (!pVanillaCharacter)
			return;
		pCharacter = &TmpCharacter;
	}

	// write down the m_Core
	if(!m_ReckoningTick || GameWorld()->m_Paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	pCharacter->m_Emote = m_EmoteType;

	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_TriggeredEvents = m_TriggeredEvents;
	pCharacter->m_TriggeredDDRaceEvents = m_TriggeredDDRaceEvents;

	pCharacter->m_Weapon = m_ActiveWeapon;
	pCharacter->m_AttackTick = m_AttackTick;

	pCharacter->m_Direction = m_Input.m_Direction;

	pCharacter->m_WorldID = WorldID;

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID()))
	{
		pCharacter->m_Health = m_Health;
		pCharacter->m_Armor = m_Armor;
		if(m_ActiveWeapon == WEAPON_NINJA)
			pCharacter->m_AmmoCount = m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;
		else if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0)
			pCharacter->m_AmmoCount = m_aWeapons[m_ActiveWeapon].m_Ammo;
	}

	if(pCharacter->m_Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction)%(250)) < 5)
			pCharacter->m_Emote = EMOTE_BLINK;
	}

	if (pVanillaCharacter)
	{
		*pVanillaCharacter = pCharacter->ToVanilla();
		if (m_Core.m_FreezeTick != 0)
		{
			pVanillaCharacter->m_Emote = EMOTE_PAIN;
			pVanillaCharacter->m_Weapon = WEAPON_NINJA;
			float NinjaTime = g_pData->m_Weapons.m_Ninja.m_Duration / 1000.0;
			float FreezeTime = GameServer()->DDRaceTuning()->m_FreezeTime;
			pVanillaCharacter->m_AmmoCount = Server()->Tick() + int (round (m_Core.m_FreezeTick / FreezeTime * NinjaTime));
		}
	}
}

void CCharacter::PostSnap()
{
	m_TriggeredEvents = 0;
	m_TriggeredDDRaceEvents = 0;
}
