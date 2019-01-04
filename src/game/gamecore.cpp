/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecore.h"

const char *CTuningParams::m_apNames[] =
{
	#define MACRO_TUNING_PARAM(Name,ScriptName,Value) #ScriptName,
	#include "tuning.h"
	#undef MACRO_TUNING_PARAM
};


bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue) const
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue) const
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Get(i, pValue);

	return false;
}

float HermiteBasis1(float v)
{
	return 2*v*v*v - 3*v*v+1;
}

float VelocityRamp(float Value, float Start, float Range, float Curvature)
{
	if(Value < Start)
		return 1.0f;
	return 1.0f/powf(Curvature, (Value-Start)/Range);
}

void CCharacterCore::Init(CWorldCore *pWorld, CCollision *pCollision)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
}

void CCharacterCore::Reset()
{
	m_Pos = vec2(0,0);
	m_Vel = vec2(0,0);
	m_HookPos = vec2(0,0);
	m_HookDir = vec2(0,0);
	m_HookTick = 0;
	m_HookState = HOOK_IDLE;
	m_HookedPlayer = -1;
	m_Endless = false;
	m_Jumped = 0;
	m_FreezeTick = 0;
	m_UnfreezeOnNextTick = false;
	m_TriggeredEvents = 0;
	m_Solo = 0;
}

void CCharacterCore::Tick(bool UseInput)
{
	float PhysSize = 28.0f;
	int StartHit = 0;
	m_TriggeredEvents = 0;

	// get ground state
	bool Grounded = false;
	if(m_pCollision->TestHLineMove(m_Pos + vec2(0, PhysSize / 2 + 5), m_Pos + vec2(0, PhysSize / 2),PhysSize))
		Grounded = true;

	vec2 TargetDirection = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	m_Vel.y += m_pWorld->m_Tuning.m_Gravity;

	float MaxSpeed = Grounded ? m_pWorld->m_Tuning.m_GroundControlSpeed : m_pWorld->m_Tuning.m_AirControlSpeed;
	float Accel = Grounded ? m_pWorld->m_Tuning.m_GroundControlAccel : m_pWorld->m_Tuning.m_AirControlAccel;
	float Friction = Grounded ? m_pWorld->m_Tuning.m_GroundFriction : m_pWorld->m_Tuning.m_AirFriction;

	// to guarantee that the tee is unfrozen in this tick
	if(m_UnfreezeOnNextTick)
		Unfreeze();
	m_UnfreezeOnNextTick = false;

	// handle input
	if(UseInput)
	{
		m_Direction = m_Input.m_Direction;
		m_Angle = (int)(angle(vec2(m_Input.m_TargetX, m_Input.m_TargetY))*256.0f);

		// handle jump
		if(m_Input.m_Jump)
		{
			if(!(m_Jumped&1) && m_FreezeTick == 0)
			{
				if(Grounded)
				{
					m_TriggeredEvents |= COREEVENTFLAG_GROUND_JUMP;
					m_Vel.y = -m_pWorld->m_Tuning.m_GroundJumpImpulse;
					m_Jumped |= 1;
				}
				else if(!(m_Jumped&2))
				{
					m_TriggeredEvents |= COREEVENTFLAG_AIR_JUMP;
					m_Vel.y = -m_pWorld->m_Tuning.m_AirJumpImpulse;
					m_Jumped |= 3;
				}
			}
		}
		else
			m_Jumped &= ~1;

		// handle hook
		if(m_Input.m_Hook)
		{
			if(m_HookState == HOOK_IDLE && m_FreezeTick == 0)
			{
				m_HookState = HOOK_FLYING;
				m_HookPos = m_Pos+TargetDirection*PhysSize*1.5f;
				// dirty fix
				if(m_pCollision->GetCollisionAt(m_HookPos)&CCollision::COLFLAG_SOLID_HOOK)
					StartHit = m_pCollision->IntersectLine(m_Pos, m_HookPos, 0, 0, CCollision::COLFLAG_SOLID_HOOK);
				m_HookDir = TargetDirection;
				m_HookedPlayer = -1;
				m_HookTick = 0;
				//m_TriggeredEvents |= COREEVENTFLAG_HOOK_LAUNCH;
			}
		}
		else
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_IDLE;
			m_HookPos = m_Pos;
		}
	}

	// add the speed modification according to players wanted direction
	if(m_Direction == 0 || m_FreezeTick != 0)
		m_Vel.x *= Friction;
	else if(m_Direction < 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -Accel);
	else if(m_Direction > 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, Accel);

	// handle jumping
	// 1 bit = to keep track if a jump has been made on this input
	// 2 bit = to keep track if a air-jump has been made
	if(Grounded)
		m_Jumped &= ~2;

	// do hook
	if(m_HookState == HOOK_IDLE)
	{
		m_HookedPlayer = -1;
		m_HookState = HOOK_IDLE;
		m_HookPos = m_Pos;
	}
	else if(m_HookState >= HOOK_RETRACT_START && m_HookState < HOOK_RETRACT_END)
	{
		m_HookState++;
	}
	else if(m_HookState == HOOK_RETRACT_END)
	{
		m_HookState = HOOK_RETRACTED;
		//m_TriggeredEvents |= COREEVENTFLAG_HOOK_RETRACT;
		m_HookState = HOOK_RETRACTED;
	}
	else if(m_HookState == HOOK_FLYING)
	{
		vec2 NewPos = m_HookPos+m_HookDir*m_pWorld->m_Tuning.m_HookFireSpeed;
		if(distance(m_Pos, NewPos) > m_pWorld->m_Tuning.m_HookLength)
		{
			m_HookState = HOOK_RETRACT_START;
			NewPos = m_Pos + normalize(NewPos-m_Pos) * m_pWorld->m_Tuning.m_HookLength;
		}

		// make sure that the hook doesn't go though the ground
		bool GoingToHitGround = false;
		bool GoingToRetract = false;
		// dirty fix part two
		int Hit = m_pCollision->IntersectLine(m_HookPos, NewPos, &NewPos, 0, CCollision::COLFLAG_SOLID_HOOK);
		if(StartHit)
		{
			NewPos = m_HookPos;
			if(StartHit&CCollision::COLFLAG_NOHOOK)
				GoingToRetract = true;
			else
				GoingToHitGround = true;
		}
		else if(Hit)
		{
			if(Hit&CCollision::COLFLAG_NOHOOK)
				GoingToRetract = true;
			else
				GoingToHitGround = true;
		}

		// Check against other players first
		if(m_pWorld && m_pWorld->m_Tuning.m_PlayerHooking && !m_Solo)
		{
			float Distance = 0.0f;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
				if(!pCharCore || pCharCore == this || pCharCore->m_Solo)
					continue;

				vec2 ClosestPoint = closest_point_on_line(m_HookPos, NewPos, pCharCore->m_Pos);
				if(distance(pCharCore->m_Pos, ClosestPoint) < PhysSize+2.0f)
				{
					if (m_HookedPlayer == -1 || distance(m_HookPos, pCharCore->m_Pos) < Distance)
					{
						m_TriggeredEvents |= COREEVENTFLAG_HOOK_ATTACH_PLAYER;
						m_HookState = HOOK_GRABBED;
						m_HookedPlayer = i;
						Distance = distance(m_HookPos, pCharCore->m_Pos);
					}
				}
			}
		}

		if(m_HookState == HOOK_FLYING)
		{
			// check against ground
			if(GoingToHitGround)
			{
				m_TriggeredEvents |= COREEVENTFLAG_HOOK_ATTACH_GROUND;
				m_HookState = HOOK_GRABBED;
			}
			else if(GoingToRetract)
			{
				m_TriggeredEvents |= COREEVENTFLAG_HOOK_HIT_NOHOOK;
				m_HookState = HOOK_RETRACT_START;
			}

			m_HookPos = NewPos;
		}
	}

	if(m_HookState == HOOK_GRABBED)
	{
		if(m_FreezeTick == 0)
		{
			if(m_HookedPlayer != -1)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
				if(pCharCore)
					m_HookPos = pCharCore->m_Pos;
				else
				{
					// release hook
					m_HookedPlayer = -1;
					m_HookState = HOOK_RETRACTED;
					m_HookPos = m_Pos;
				}

				// keep players hooked for a max of 1.5sec
				//if(Server()->Tick() > hook_tick+(Server()->TickSpeed()*3)/2)
					//release_hooked();
			}

			// don't do this hook rutine when we are hook to a player
			if(m_HookedPlayer == -1 && distance(m_HookPos, m_Pos) > 46.0f)
			{
				vec2 HookVel = normalize(m_HookPos-m_Pos)*m_pWorld->m_Tuning.m_HookDragAccel;
				// the hook as more power to drag you up then down.
				// this makes it easier to get on top of an platform
				if(HookVel.y > 0)
					HookVel.y *= 0.3f;

				// the hook will boost it's power if the player wants to move
				// in that direction. otherwise it will dampen everything abit
				if((HookVel.x < 0 && m_Direction < 0) || (HookVel.x > 0 && m_Direction > 0))
					HookVel.x *= 0.95f;
				else
					HookVel.x *= 0.75f;

				vec2 NewVel = m_Vel+HookVel;

				// check if we are under the legal limit for the hook
				if(length(NewVel) < m_pWorld->m_Tuning.m_HookDragSpeed || length(NewVel) < length(m_Vel))
					m_Vel = NewVel; // no problem. apply

			}
		}

		// release hook (max hook time is 1.2)
		if(m_Endless)
			m_HookTick = 0;
		else
			m_HookTick++;

		if(m_FreezeTick != 0 || (m_HookedPlayer != -1 && (m_HookTick > SERVER_TICK_SPEED+SERVER_TICK_SPEED/5 || !m_pWorld->m_apCharacters[m_HookedPlayer]
									 || m_Solo || m_pWorld->m_apCharacters[m_HookedPlayer]->m_Solo)))
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}

		// for disappearing walls
		if(m_HookedPlayer == -1 && !(m_pCollision->GetCollisionAt(m_HookPos)&CCollision::COLFLAG_SOLID_HOOK))
		{
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
	}

	if(m_pWorld && !m_Solo)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
			if(!pCharCore)
				continue;

			//player *p = (player*)ent;
			if(pCharCore == this || pCharCore->m_Solo) // || !(p->flags&FLAG_ALIVE)
				continue; // make sure that we don't nudge our self

			// handle player <-> player collision
			float Distance = distance(m_Pos, pCharCore->m_Pos);
			vec2 Dir = normalize(m_Pos - pCharCore->m_Pos);
			if(m_pWorld->m_Tuning.m_PlayerCollision && Distance < PhysSize*1.25f && Distance > 0.0f)
			{
				float a = (PhysSize*1.45f - Distance);
				float Velocity = 0.5f;

				// make sure that we don't add excess force by checking the
				// direction against the current velocity. if not zero.
				if (length(m_Vel) > 0.0001)
					Velocity = 1-(dot(normalize(m_Vel), Dir)+1)/2;

				m_Vel += Dir*a*(Velocity*0.75f);
				m_Vel *= 0.85f;
			}

			// handle hook influence
			if(m_HookedPlayer == i && m_pWorld->m_Tuning.m_PlayerHooking)
			{
				if(Distance > PhysSize*1.50f) // TODO: fix tweakable variable
				{
					float Accel = m_pWorld->m_Tuning.m_HookDragAccel * (Distance/m_pWorld->m_Tuning.m_HookLength);
					float DragSpeed = m_pWorld->m_Tuning.m_HookDragSpeed;

					// add force to the hooked player
					pCharCore->m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.x, Accel*Dir.x*1.5f);
					pCharCore->m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.y, Accel*Dir.y*1.5f);

					// add a little bit force to the guy who has the grip
					m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, -Accel*Dir.x*0.25f);
					m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, -Accel*Dir.y*0.25f);
				}
			}
		}
	}

	// clamp the velocity to something sane
	if(length(m_Vel) > 6000)
		m_Vel = normalize(m_Vel) * 6000;

	if(m_FreezeTick > 0)
		m_FreezeTick--;
}

int CCharacterCore::Move(CCollision::CTriggers *pOutTriggers)
{
	if(!m_pWorld)
		return 0;

	if(length(m_Vel) > MAX_SPEED)
		m_Vel = normalize(m_Vel) * MAX_SPEED;

	float RampValue = VelocityRamp(length(m_Vel)*50, m_pWorld->m_Tuning.m_VelrampStart, m_pWorld->m_Tuning.m_VelrampRange, m_pWorld->m_Tuning.m_VelrampCurvature);

	m_Vel.x = m_Vel.x*RampValue;

	vec2 NewPos = m_Pos;

	int Size = m_pCollision->MoveBox(&NewPos, &m_Vel, pOutTriggers, vec2(28.0f, 28.0f), 0);
	bool Teleport = false;
	for(int i = 0; i < Size; i++)
	{
		HandleTriggers(pOutTriggers[i]);
		// dirty fix for dirty code
		if(pOutTriggers[i].m_TeleFlags == CCollision::TRIGGERFLAG_TELEPORT)
			Teleport = true;
	}

	m_Vel.x = m_Vel.x*(1.0f/RampValue);

	if(m_pWorld && m_pWorld->m_Tuning.m_PlayerCollision && !Teleport && !m_Solo)
	{
		// check player collision
		float Distance = distance(m_Pos, NewPos);
		int End = Distance+1;
		vec2 LastPos = m_Pos;
		for(int i = 0; i < End; i++)
		{
			float a = i/Distance;
			vec2 Pos = mix(m_Pos, NewPos, a);
			for(int p = 0; p < MAX_CLIENTS; p++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
				if(!pCharCore || pCharCore == this || pCharCore->m_Solo)
					continue;
				float D = distance(Pos, pCharCore->m_Pos);
				if(D < 28.0f && D > 0.0f)
				{
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(distance(NewPos, pCharCore->m_Pos) > D)
						m_Pos = NewPos;
					// this might cause problems in rare cases
					return Size;
				}
			}
			LastPos = Pos;
		}
	}

	m_Pos = NewPos;

	return Size;
}

void CCharacterCore::Move()
{
	CCollision::CTriggers aTriggers[4 * (int)((MAX_SPEED + 15) / 16) + 2];
	Move(aTriggers);
}

void CCharacterCore::HandleTriggers(CCollision::CTriggers Triggers)
{
	// handle freeze-tiles
	if(Triggers.m_FreezeFlags&CCollision::TRIGGERFLAG_DEEP_FREEZE)
		DeepFreeze();
	else if(Triggers.m_FreezeFlags&CCollision::TRIGGERFLAG_DEEP_UNFREEZE)
		DeepUnfreeze();

	if(Triggers.m_FreezeFlags&CCollision::TRIGGERFLAG_FREEZE)
		Freeze();
	else if(Triggers.m_FreezeFlags&CCollision::TRIGGERFLAG_UNFREEZE)
		Unfreeze();

	// handle teleporters
	if(Triggers.m_TeleFlags&CCollision::TRIGGERFLAG_CUT_OTHER)
	{
		// this part is very dirty
		int MyId = -1;
		for(int i = 0; i != MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];

			if(pCharCore == this)
			{
				// find out my own id
				MyId = i;
				break;
			}
		}

		for(int i = 0; i != MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
			if(!pCharCore)
				continue;

			if(pCharCore == this)
				continue;

			if(pCharCore->m_HookedPlayer == MyId)
			{
				// reject player's hook that hooks me
				pCharCore->m_HookedPlayer = -1;
				pCharCore->m_HookState = HOOK_RETRACTED;
				pCharCore->m_HookPos = pCharCore->m_Pos;
			}
		}
	}

	if(Triggers.m_TeleFlags&CCollision::TRIGGERFLAG_CUT_OWN && m_HookState != HOOK_IDLE)
	{
		m_HookedPlayer = -1;
		m_HookState = HOOK_RETRACT_START;
	}

	if(Triggers.m_SpeedupFlags&CCollision::TRIGGERFLAG_SPEEDUP)
		m_TriggeredEvents |= COREEVENTFLAG_SPEEDUP;

	if(Triggers.m_Endless == CCollision::PROPERTEE_ON)
		m_Endless = true;
	else if(Triggers.m_Endless == CCollision::PROPERTEE_OFF)
		m_Endless = false;

	if(Triggers.m_Solo == CCollision::PROPERTEE_ON)
		m_Solo = true;
	else if(Triggers.m_Solo == CCollision::PROPERTEE_OFF)
		m_Solo = false;
}

void CCharacterCore::Freeze()
{
	if(m_FreezeTick >= 0)
	{
		if(m_FreezeTick == 0)
			m_TriggeredEvents |= COREEVENTFLAG_FREEZE;
		m_FreezeTick = SERVER_TICK_SPEED * m_pWorld->m_Tuning.m_FreezeTime;
	}
}

void CCharacterCore::Unfreeze()
{
	if(m_FreezeTick > 0)
		m_FreezeTick = 0;
}

void CCharacterCore::DeepFreeze()
{
	if(m_FreezeTick == 0)
		m_TriggeredEvents |= COREEVENTFLAG_FREEZE;
	m_FreezeTick = -1;
}

void CCharacterCore::DeepUnfreeze()
{
	if(m_FreezeTick == -1)
		m_FreezeTick = SERVER_TICK_SPEED * m_pWorld->m_Tuning.m_FreezeTime;
}

void CCharacterCore::Write(CNetObj_CharacterCore *pObjCore)
{
	pObjCore->m_X = round_to_int(m_Pos.x);
	pObjCore->m_Y = round_to_int(m_Pos.y);

	pObjCore->m_VelX = round_to_int(m_Vel.x*256.0f);
	pObjCore->m_VelY = round_to_int(m_Vel.y*256.0f);
	pObjCore->m_HookState = m_HookState;
	pObjCore->m_HookTick = m_HookTick;
	pObjCore->m_HookX = round_to_int(m_HookPos.x);
	pObjCore->m_HookY = round_to_int(m_HookPos.y);
	pObjCore->m_HookDx = round_to_int(m_HookDir.x*256.0f);
	pObjCore->m_HookDy = round_to_int(m_HookDir.y*256.0f);
	pObjCore->m_HookedPlayer = m_HookedPlayer;
	pObjCore->m_Jumped = m_Jumped;
	pObjCore->m_FreezeTick = m_FreezeTick;
	pObjCore->m_Direction = m_Direction;
	pObjCore->m_Angle = m_Angle;
	pObjCore->m_Flags = m_Solo ? COREFLAG_SOLO : 0;
}

void CCharacterCore::Read(const CNetObj_CharacterCore *pObjCore)
{
	m_Pos.x = pObjCore->m_X;
	m_Pos.y = pObjCore->m_Y;
	m_Vel.x = pObjCore->m_VelX/256.0f;
	m_Vel.y = pObjCore->m_VelY/256.0f;
	m_HookState = pObjCore->m_HookState;
	m_HookTick = pObjCore->m_HookTick;
	m_HookPos.x = pObjCore->m_HookX;
	m_HookPos.y = pObjCore->m_HookY;
	m_HookDir.x = pObjCore->m_HookDx/256.0f;
	m_HookDir.y = pObjCore->m_HookDy/256.0f;
	m_HookedPlayer = pObjCore->m_HookedPlayer;
	m_Jumped = pObjCore->m_Jumped;
	m_FreezeTick = pObjCore->m_FreezeTick;
	m_Direction = pObjCore->m_Direction;
	m_Angle = pObjCore->m_Angle;
	m_Solo = pObjCore->m_Flags & COREFLAG_SOLO;
}

void CCharacterCore::Quantize()
{
	CNetObj_CharacterCore Core;
	Write(&Core);
	Read(&Core);
}

