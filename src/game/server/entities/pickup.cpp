/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"
#include "pickup.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, vec2 Pos, int SwitchGroup, bool InvertSwitch)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, PickupPhysSize, SwitchGroup, InvertSwitch)
{
	m_Type = Type;
	m_Persistent = false;

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;
}

void CPickup::Tick()
{
	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			if(m_Type == PICKUP_GRENADE || m_Type == PICKUP_SHOTGUN || m_Type == PICKUP_LASER)
				CGameContext::CreateSound(Events(), m_Pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}

	if(Active())
	{
		// Check if a player intersected us
		CCharacter *pChr;
		if(m_Type == PICKUP_HEALTH || m_Type == PICKUP_ARMOR) // TODO DDRace or not ddrace
			pChr = (CCharacter *)GameWorld()->ClosestEntity(m_Pos, 20.0f, CGameWorld::ENTTYPE_CHARACTER, 0);
		else
			pChr = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);

		for(; pChr; pChr = (CCharacter *)pChr->TypeNext())
	 	{
			if(pChr && pChr->IsAlive() && distance(m_Pos, pChr->GetPos()) < pChr->GetProximityRadius()+20.0f)
			{
				// player picked us up, is someone was hooking us, let them go
				bool Picked = false;
				switch (m_Type)
				{
					case PICKUP_HEALTH:
						if(pChr->IncreaseHealth(1))
						{
							Picked = true;
							CGameContext::CreateSound(Events(), m_Pos, SOUND_PICKUP_HEALTH);
						}
						break;

					case PICKUP_ARMOR:
						if(pChr->IncreaseArmor(1))
						{
							Picked = true;
							CGameContext::CreateSound(Events(), m_Pos, SOUND_PICKUP_ARMOR);
						}
						break;

					case PICKUP_GRENADE:
						// TODO DDRace if ddrace give -1
						if(pChr->GiveWeapon(WEAPON_GRENADE, g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Maxammo))
						{
							Picked = true;
							CGameContext::CreateSound(Events(), m_Pos, SOUND_PICKUP_GRENADE);
							if(pChr->GetPlayer())
								GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_GRENADE);
						}
						break;
					case PICKUP_SHOTGUN:
						// TODO DDRace if ddrace give -1
						if(pChr->GiveWeapon(WEAPON_SHOTGUN, g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Maxammo))
						{
							Picked = true;
							CGameContext::CreateSound(Events(), m_Pos, SOUND_PICKUP_SHOTGUN);
							if(pChr->GetPlayer())
								GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_SHOTGUN);
						}
						break;
					case PICKUP_LASER:
						// TODO DDRace if ddrace give -1
						if(pChr->GiveWeapon(WEAPON_LASER, g_pData->m_Weapons.m_aId[WEAPON_LASER].m_Maxammo))
						{
							Picked = true;
							CGameContext::CreateSound(Events(), m_Pos, SOUND_PICKUP_SHOTGUN);
							if(pChr->GetPlayer())
								GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_LASER);
						}
						break;

					case PICKUP_NINJA:
						{
							Picked = true;
							// activate ninja on target player
							pChr->GiveNinja();

							// loop through all players, setting their emotes
							CCharacter *pC = static_cast<CCharacter *>(GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
							for(; pC; pC = (CCharacter *)pC->TypeNext())
							{
								if (pC != pChr)
									pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
							}

							pChr->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
							break;
						}

					default:
						break;
				};

				if(Picked)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d",
						pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type);
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					int RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
					if(RespawnTime >= 0)
						m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
				}
			}

			if(m_Type == PICKUP_HEALTH || m_Type == PICKUP_ARMOR) // TODO DDRace or not ddrace
				break;
		}
	}
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CPickup::Snap(int SnappingClient, int WorldID)
{
	if (!WorldVisible(SnappingClient, WorldID))
		return;

	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient) || !Active())
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
	pP->m_World = WorldID;
}
