/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <math.h>
#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

CCollision::CCollision()
{
	for(int t = 0; t < NUM_GAMELAYERTYPES; t++)
	{
		m_apTiles[t] = 0;
		m_aWidth[t] = 0;
		m_aHeight[t] = 0;
	}
	m_pVanillaTiles = 0;
	m_VanillaWidth = 0;
	m_VanillaHeight = 0;
	m_pLayers = 0;
	m_NumCheckpoints = 0;
}

void CCollision::Init(class CLayers *pLayers, bool *pSwitchStates)
{
	m_pSwitchStates = pSwitchStates;
	m_pLayers = pLayers;

	for(int t = 0; t < NUM_GAMELAYERTYPES; t++)
	{
		CMapItemLayerTilemap *pTileMap = m_pLayers->GameLayer(t);
		if (pTileMap)
		{
			m_apTiles[t] = static_cast<CTile *>(m_pLayers->Map()->GetData(pTileMap->m_Data));
			m_aWidth[t] = pTileMap->m_Width;
			m_aHeight[t] = pTileMap->m_Height;
		}
		else
		{
			m_apTiles[t] = 0;
			m_aWidth[t] = 0;
			m_aHeight[t] = 0;
		}
	}

	CMapItemLayerTilemap *pVanillaTileMap = m_pLayers->VanillaLayer();
	if (pVanillaTileMap)
	{
		m_pVanillaTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(pVanillaTileMap->m_Data));
		m_VanillaWidth = pVanillaTileMap->m_Width;
		m_VanillaHeight = pVanillaTileMap->m_Height;

		for(int i = 0; i < m_VanillaWidth*m_VanillaHeight; i++)
		{
			int Index = m_pVanillaTiles[i].m_Index;

			if(Index > 128)
				continue;

			switch(Index)
			{
			case TILE_DEATH:
				m_pVanillaTiles[i].m_Index = COLFLAG_DEATH;
				break;
			case TILE_SOLID:
				m_pVanillaTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_SOLID_HOOK|COLFLAG_SOLID_PROJ;
				break;
			case TILE_NOHOOK:
				m_pVanillaTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_SOLID_HOOK|COLFLAG_SOLID_PROJ|COLFLAG_NOHOOK;
				break;
			default:
				m_pVanillaTiles[i].m_Index = 0;
			}
		}
	}

	if(m_apTiles[GAMELAYERTYPE_COLLISION])
	{
		for(int i = 0; i < m_aWidth[GAMELAYERTYPE_COLLISION]*m_aHeight[GAMELAYERTYPE_COLLISION]; i++)
		{
			int Index = m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index;

			if(Index > 128)
				continue;		

			switch(Index%16)
			{
			case TILE_DEATH:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = COLFLAG_DEATH;
				break;
			case TILE_SOLID:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = COLFLAG_SOLID|COLFLAG_SOLID_HOOK|COLFLAG_SOLID_PROJ;
				break;
			case TILE_NOHOOK:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = COLFLAG_SOLID|COLFLAG_SOLID_HOOK|COLFLAG_SOLID_PROJ|COLFLAG_NOHOOK;
				break;
			case TILE_SEMISOLID_HOOK:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = COLFLAG_SOLID|COLFLAG_SOLID_PROJ;
				break;
			case TILE_SEMISOLID_PROJ:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = COLFLAG_SOLID|COLFLAG_SOLID_HOOK;
				break;
			case TILE_SEMISOLID_PROJ_NOHOOK:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = COLFLAG_SOLID|COLFLAG_SOLID_HOOK|COLFLAG_NOHOOK;
				break;
			case TILE_SEMISOLID_BOTH:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = COLFLAG_SOLID;
				break;
			default:
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index = 0;
			}

			if(m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Index&COLFLAG_SOLID)
			{
				int Flags = m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags;
				m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags = Flags & TILEFLAG_INVERT_SWITCH;

				switch(Index/16)
				{
					// TODO DDRace this is not right
					case ROW_ONE_OPEN:
						if(Flags&TILEFLAG_ROTATE)
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= Flags&TILEFLAG_HFLIP ? DIRFLAG_LEFT : DIRFLAG_RIGHT;
						else
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= Flags&TILEFLAG_VFLIP ? DIRFLAG_DOWN : DIRFLAG_UP;
						break;
					case ROW_TWO_OPEN:
						if(Flags&TILEFLAG_ROTATE)
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= DIRFLAG_LEFT|DIRFLAG_RIGHT;
						else
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= DIRFLAG_DOWN|DIRFLAG_UP;
						break;
					case ROW_TWO_CORNER_OPEN:
						m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= Flags&TILEFLAG_HFLIP ? DIRFLAG_LEFT : DIRFLAG_RIGHT;
						if(Flags&TILEFLAG_ROTATE)
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= Flags&TILEFLAG_VFLIP ? DIRFLAG_UP : DIRFLAG_DOWN;
						else
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= Flags&TILEFLAG_VFLIP ? DIRFLAG_DOWN : DIRFLAG_UP;
						break;
					case ROW_THREE_OPEN:
						if(Flags&TILEFLAG_ROTATE)
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= DIRFLAG_UP|DIRFLAG_DOWN|(Flags&TILEFLAG_VFLIP ? DIRFLAG_LEFT : DIRFLAG_RIGHT);
						else
							m_apTiles[GAMELAYERTYPE_COLLISION][i].m_Flags |= DIRFLAG_RIGHT|DIRFLAG_LEFT|(Flags&TILEFLAG_VFLIP ? DIRFLAG_DOWN : DIRFLAG_UP);
						break;
				}
			}
		}
	}
	else
	{
		m_apTiles[GAMELAYERTYPE_COLLISION] = m_pVanillaTiles;
		m_aWidth[GAMELAYERTYPE_COLLISION] = m_VanillaWidth;
		m_aHeight[GAMELAYERTYPE_COLLISION] = m_VanillaHeight;
	}

	for(int i = 0; i < m_aWidth[GAMELAYERTYPE_FREEZE]*m_aHeight[GAMELAYERTYPE_FREEZE]; i++)
	{
		int Index = m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index;

		switch(Index)
		{
		case TILE_FREEZE:
			m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index = FREEZEFLAG_FREEZE;
			break;
		case TILE_UNFREEZE:
			m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index = FREEZEFLAG_UNFREEZE;
			break;
		case TILE_DEEP_FREEZE:
			m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index = FREEZEFLAG_DEEP_FREEZE;
			break;
		case TILE_DEEP_UNFREEZE:
			m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index = FREEZEFLAG_DEEP_UNFREEZE;
			break;
		case TILE_FREEZE_DEEP_UNFREEZE:
			m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index = FREEZEFLAG_DEEP_UNFREEZE|FREEZEFLAG_FREEZE;
			break;
		case TILE_UNFREEZE_DEEP_UNFREEZE:
			m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index = FREEZEFLAG_DEEP_UNFREEZE|FREEZEFLAG_UNFREEZE;
			break;
		default:
			m_apTiles[GAMELAYERTYPE_FREEZE][i].m_Index = 0;
		}
	}

	for(int i = 0; i < m_aWidth[GAMELAYERTYPE_TELE]*m_aHeight[GAMELAYERTYPE_TELE]; i++)
		if(m_apTiles[GAMELAYERTYPE_TELE][i].m_Index > 0)
			if(!(m_apTiles[GAMELAYERTYPE_TELE][i].m_Flags&TELEFLAG_IN))
				m_aTeleTargets[m_apTiles[GAMELAYERTYPE_TELE][i].m_Index] = vec2((i%m_aWidth[GAMELAYERTYPE_TELE]+0.5f)*32, (i/m_aWidth[GAMELAYERTYPE_TELE]+0.5f)*32);

	for(int i = 0; i < m_aWidth[GAMELAYERTYPE_RACE]*m_aHeight[GAMELAYERTYPE_RACE]; i++)
		m_NumCheckpoints = max(m_apTiles[GAMELAYERTYPE_RACE][i].m_Index - 2, m_NumCheckpoints);
}

void CCollision::Init(class CCollision *pOther, bool *pSwitchStates)
{
	m_pSwitchStates = pSwitchStates;
	m_pLayers = pOther->m_pLayers;

	for(int t = 0; t < NUM_GAMELAYERTYPES; t++)
	{
		m_aWidth[t] = pOther->m_aWidth[t];
		m_aHeight[t] = pOther->m_aHeight[t];
		m_apTiles[t] = pOther->m_apTiles[t];
	}

	m_pVanillaTiles = pOther->m_pVanillaTiles;
	m_VanillaWidth = pOther->m_VanillaWidth;
	m_VanillaHeight = pOther->m_VanillaHeight;

	for(int i = 0; i < m_aWidth[GAMELAYERTYPE_TELE]*m_aHeight[GAMELAYERTYPE_TELE]; i++)
		if(m_apTiles[GAMELAYERTYPE_TELE][i].m_Index > 0)
			if(!(m_apTiles[GAMELAYERTYPE_TELE][i].m_Flags&TELEFLAG_IN))
				m_aTeleTargets[m_apTiles[GAMELAYERTYPE_TELE][i].m_Index] = vec2((i%m_aWidth[GAMELAYERTYPE_TELE]+0.5f)*32, (i/m_aWidth[GAMELAYERTYPE_TELE]+0.5f)*32);

	for(int i = 0; i < m_aWidth[GAMELAYERTYPE_RACE]*m_aHeight[GAMELAYERTYPE_RACE]; i++)
		m_NumCheckpoints = max(m_apTiles[GAMELAYERTYPE_RACE][i].m_Index - 2, m_NumCheckpoints);
}

int CCollision::GetNumCheckpoints() const
{
	return m_NumCheckpoints;
}

int CCollision::GetSwitchGroup(int PosIndex, int Layer) const
{
	return m_apTiles[Layer][PosIndex].m_Reserved - 1;
}

ivec2 CCollision::PosToTilePos(float x, float y) const
{
	return ivec2(floor(round_to_int(x)/32.0f), floor(round_to_int(y)/32.0f));
}

int CCollision::TilePosToIndex(int x, int y, int Layer) const
{
	if(!m_apTiles[Layer])
		return -1;

	int Nx = clamp(x, 0, m_aWidth[Layer]-1);
	int Ny = clamp(y, 0, m_aHeight[Layer]-1);

	return Ny * m_aWidth[Layer] + Nx;
}

int CCollision::VanillaTilePosToIndex(int x, int y) const
{
	if(!m_pVanillaTiles)
		return -1;

	int Nx = clamp(x, 0, m_VanillaWidth-1);
	int Ny = clamp(y, 0, m_VanillaHeight-1);

	return Ny * m_VanillaWidth + Nx;
}

int CCollision::PosToIndex(float x, float y, int Layer) const
{
	ivec2 TilePos = PosToTilePos(x, y);
	return TilePosToIndex(TilePos.x, TilePos.y, Layer);
}

int CCollision::VanillaPosToIndex(float x, float y) const
{
	ivec2 TilePos = PosToTilePos(x, y);
	return VanillaTilePosToIndex(TilePos.x, TilePos.y);
}

int CCollision::GetDirFlags(ivec2 Dir) const
{
	int Flags = 0;
	if(Dir.x > 0)
		Flags |= DIRFLAG_RIGHT;
	else if(Dir.x < 0)
		Flags |= DIRFLAG_LEFT;

	if(Dir.y > 0)
		Flags |= DIRFLAG_DOWN;
	else if(Dir.y < 0)
		Flags |= DIRFLAG_UP;

	return Flags;
}

int CCollision::GetCollisionAt(float x, float y, bool Vanilla) const
{
	if (Vanilla)
	{
		int Index = VanillaPosToIndex(x, y);
		if(m_pVanillaTiles[Index].m_Index <= 128)
			return m_pVanillaTiles[Index].m_Index;
		else
			return 0;
	}
	else
	{
		int Index = PosToIndex(x, y, GAMELAYERTYPE_COLLISION);
		int Flags = m_apTiles[GAMELAYERTYPE_COLLISION][Index].m_Flags;
		bool Invert = Flags&TILEFLAG_INVERT_SWITCH;
		int SwitchGroup = GetSwitchGroup(Index, GAMELAYERTYPE_COLLISION);
		bool Switch;
		if(SwitchGroup != -1)
			Switch = m_pSwitchStates[SwitchGroup];
		else
			Switch = Invert;
		
		if(Switch == Invert && m_apTiles[GAMELAYERTYPE_COLLISION][Index].m_Index <= 128)
			return m_apTiles[GAMELAYERTYPE_COLLISION][Index].m_Index;
		else
			return 0;
	}
}

int CCollision::GetCollisionMove(float x, float y, float OldX, float OldY, bool Vanilla, int DirFlagsMask) const
{
	ivec2 Pos = PosToTilePos(x, y);
	ivec2 OldPos = PosToTilePos(OldX, OldY);
	int DirFlags = GetDirFlags(Pos - OldPos)&DirFlagsMask;

	if (Vanilla)
	{
		int Index = VanillaTilePosToIndex(Pos.x, Pos.y);
		if(m_pVanillaTiles[Index].m_Index <= 128)
			return m_pVanillaTiles[Index].m_Index;
		else
			return 0;
	}
	else
	{
		int Index = TilePosToIndex(Pos.x, Pos.y, GAMELAYERTYPE_COLLISION);
		int Flags = m_apTiles[GAMELAYERTYPE_COLLISION][Index].m_Flags;
		bool Invert = Flags&TILEFLAG_INVERT_SWITCH;
		int SwitchGroup = GetSwitchGroup(Index, GAMELAYERTYPE_COLLISION);
		bool Switch;
		if(SwitchGroup != -1)
			Switch = m_pSwitchStates[SwitchGroup];
		else
			Switch = Invert;

		if(Switch == Invert
				&& m_apTiles[GAMELAYERTYPE_COLLISION][Index].m_Index <= 128
				&& (Flags&DirFlags) != DirFlags)
			return m_apTiles[GAMELAYERTYPE_COLLISION][Index].m_Index;
		else
			return 0;
	}
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int ColFlag, bool Vanilla) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;

	for(int i = 0; i <= End; i++)
	{
		float a = i/float(End);
		vec2 Pos = mix(Pos0, Pos1, a);
		int Col = GetCollisionMove(Pos, Last, Vanilla);
		if(Col&ColFlag)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return Col;
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces, int ColFlag, bool Vanilla) const
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(GetCollisionMove(Pos + Vel, Pos, Vanilla)&ColFlag)
	{
		int Affected = 0;
		if(GetCollisionMove(Pos.x + Vel.x, Pos.y, Pos, Vanilla)&ColFlag)
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(GetCollisionMove(Pos.x, Pos.y + Vel.y, Pos, Vanilla)&ColFlag)
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			if(GetCollisionMove(Pos + Vel, Pos.x, Pos.y + Vel.y, Vanilla)&ColFlag)
			{
				pInoutVel->x *= -Elasticity;
				if(pBounces)
					(*pBounces)++;
			}
			if(GetCollisionMove(Pos + Vel, Pos.x + Vel.x, Pos.y, Vanilla)&ColFlag)
			{
				pInoutVel->y *= -Elasticity;
				if(pBounces)
					(*pBounces)++;
			}
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size, bool Vanilla) const
{
	Size *= 0.5f;
	if(GetCollisionAt(Pos.x-Size.x, Pos.y-Size.y, Vanilla))
		return true;
	if(GetCollisionAt(Pos.x+Size.x, Pos.y-Size.y, Vanilla))
		return true;
	if(GetCollisionAt(Pos.x-Size.x, Pos.y+Size.y, Vanilla))
		return true;
	if(GetCollisionAt(Pos.x+Size.x, Pos.y+Size.y, Vanilla))
		return true;
	return false;
}

bool CCollision::TestBoxMove(vec2 Pos, vec2 OldPos, vec2 Size, bool Vanilla) const
{
	Size *= 0.5f;
	if(GetCollisionMove(Pos.x-Size.x, Pos.y-Size.y, OldPos.x-Size.x, OldPos.y-Size.y, Vanilla, DIRFLAG_UP|DIRFLAG_LEFT))
		return true;
	if(GetCollisionMove(Pos.x+Size.x, Pos.y-Size.y, OldPos.x+Size.x, OldPos.y-Size.y, Vanilla, DIRFLAG_UP|DIRFLAG_RIGHT))
		return true;
	if(GetCollisionMove(Pos.x-Size.x, Pos.y+Size.y, OldPos.x-Size.x, OldPos.y+Size.y, Vanilla, DIRFLAG_DOWN|DIRFLAG_LEFT))
		return true;
	if(GetCollisionMove(Pos.x+Size.x, Pos.y+Size.y, OldPos.x+Size.x, OldPos.y+Size.y, Vanilla, DIRFLAG_DOWN|DIRFLAG_RIGHT))
		return true;
	return false;
}

bool CCollision::TestHLineMove(vec2 Pos, vec2 OldPos, float Length, bool Vanilla) const
{
	Length *= 0.5f;
	if(GetCollisionMove(Pos.x-Length, Pos.y, OldPos.x-Length, OldPos.y, Vanilla, DIRFLAG_UP|DIRFLAG_DOWN))
		return true;
	if(GetCollisionMove(Pos.x+Length, Pos.y, OldPos.x+Length, OldPos.y, Vanilla, DIRFLAG_UP|DIRFLAG_DOWN))
		return true;
	return false;
}

int CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, CTriggers *pOutTriggers, vec2 Size, float Elasticity, bool Vanilla) const
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	vec2 SpeedupVel = vec2(0.0f, 0.0f);

	float Distance = length(Vel);
	int Max = (int)Distance;
	int NumTiles = 0;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		ivec2 OldPos = PosToTilePos(Pos.x, Pos.y);
		bool First = true;
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice

			if(TestBoxMove(NewPos, Pos, Size, Vanilla))
			{
				int Hits = 0;

				if(TestBoxMove(vec2(Pos.x, NewPos.y), Pos, Size, Vanilla))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBoxMove(vec2(NewPos.x, Pos.y), Pos, Size, Vanilla))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			Pos = NewPos;

			if (!Vanilla)
			{
				// speedups
				bool Speedup = false;
				ivec2 iPos = PosToTilePos(Pos.x, Pos.y);

				int PosIndex = TilePosToIndex(iPos.x, iPos.y, GAMELAYERTYPE_HSPEEDUP);
				if(PosIndex >= 0 && m_apTiles[GAMELAYERTYPE_HSPEEDUP][PosIndex].m_Index > 0)
				{
					float Accel = m_apTiles[GAMELAYERTYPE_HSPEEDUP][PosIndex].m_Index * Fraction;
					Speedup = true;
					if(m_apTiles[GAMELAYERTYPE_HSPEEDUP][PosIndex].m_Flags&SPEEDUPFLAG_FLIP)
						SpeedupVel.x -= Accel;
					else
						SpeedupVel.x += Accel;
				}

				PosIndex = TilePosToIndex(iPos.x, iPos.y, GAMELAYERTYPE_VSPEEDUP);
				if(PosIndex >= 0 && m_apTiles[GAMELAYERTYPE_VSPEEDUP][PosIndex].m_Index > 0)
				{
					float Accel = m_apTiles[GAMELAYERTYPE_VSPEEDUP][PosIndex].m_Index * Fraction;
					Speedup = true;
					if(m_apTiles[GAMELAYERTYPE_VSPEEDUP][PosIndex].m_Flags&SPEEDUPFLAG_FLIP)
						SpeedupVel.y -= Accel;
					else
						SpeedupVel.y += Accel;
				}

				if(pOutTriggers && (iPos != OldPos || First))
				{
					pOutTriggers[NumTiles] = CTriggers();

					if(Speedup && iPos != OldPos)
						pOutTriggers[NumTiles].m_SpeedupFlags |= TRIGGERFLAG_SPEEDUP;
					HandleTriggerTiles(iPos.x, iPos.y, pOutTriggers + NumTiles);

					// handle teleporters
					int PosIndex = TilePosToIndex(iPos.x, iPos.y, GAMELAYERTYPE_TELE);
					int TeleFlags = PosIndex >= 0 ? m_apTiles[GAMELAYERTYPE_TELE][PosIndex].m_Flags : 0;

					if(TeleFlags&TELEFLAG_IN)
					{
						pOutTriggers[NumTiles].m_TeleFlags |= TRIGGERFLAG_TELEPORT;
						pOutTriggers[NumTiles].m_TeleInPos = Pos;

						Pos = m_aTeleTargets[m_apTiles[GAMELAYERTYPE_TELE][PosIndex].m_Index];

						pOutTriggers[NumTiles].m_TeleOutPos = Pos;
						if(TeleFlags&TELEFLAG_RESET_VEL)
						{
							Vel = vec2(0.0f, 0.0f);
							pOutTriggers[NumTiles].m_TeleFlags |= TRIGGERFLAG_STOP_NINJA;
						}
						if(TeleFlags&TELEFLAG_CUT_OTHER)
							pOutTriggers[NumTiles].m_TeleFlags |= TRIGGERFLAG_CUT_OTHER;
						if(TeleFlags&TELEFLAG_CUT_OWN)
							pOutTriggers[NumTiles].m_TeleFlags |= TRIGGERFLAG_CUT_OWN;

						NumTiles++;

						ivec2 iPos = PosToTilePos(Pos.x, Pos.y);
						OldPos = iPos;

						pOutTriggers[NumTiles] = CTriggers();
						HandleTriggerTiles(iPos.x, iPos.y, pOutTriggers + NumTiles);
					}
					NumTiles++;

					OldPos = iPos;
				}

				First = false;
			}
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel + SpeedupVel;

	return NumTiles;
}

void CCollision::HandleTriggerTiles(int x, int y, CTriggers *pOutTriggers) const
{
	pOutTriggers->m_FreezeFlags = 0;
	int Index = TilePosToIndex(x, y, GAMELAYERTYPE_FREEZE);
	if(Index >= 0)
	{
		if(m_apTiles[GAMELAYERTYPE_FREEZE][Index].m_Index&FREEZEFLAG_FREEZE)
			pOutTriggers->m_FreezeFlags |= TRIGGERFLAG_FREEZE;
		else if(m_apTiles[GAMELAYERTYPE_FREEZE][Index].m_Index&FREEZEFLAG_UNFREEZE)
			pOutTriggers->m_FreezeFlags |= TRIGGERFLAG_UNFREEZE;

		if(m_apTiles[GAMELAYERTYPE_FREEZE][Index].m_Index&FREEZEFLAG_DEEP_FREEZE)
			pOutTriggers->m_FreezeFlags |= TRIGGERFLAG_DEEP_FREEZE;
		else if(m_apTiles[GAMELAYERTYPE_FREEZE][Index].m_Index&FREEZEFLAG_DEEP_UNFREEZE)
			pOutTriggers->m_FreezeFlags |= TRIGGERFLAG_DEEP_UNFREEZE;
	}

	pOutTriggers->m_SwitchFlags = 0;
	Index = TilePosToIndex(x, y, GAMELAYERTYPE_SWITCH);
	if(Index >= 0)
	{
		if(m_apTiles[GAMELAYERTYPE_SWITCH][Index].m_Index > 0)
		{
			pOutTriggers->m_SwitchFlags |= TRIGGERFLAG_SWITCH;
			pOutTriggers->m_SwitchState = m_apTiles[GAMELAYERTYPE_SWITCH][Index].m_Flags&TILEFLAG_SWITCH_ON;
			pOutTriggers->m_SwitchGroup = m_apTiles[GAMELAYERTYPE_SWITCH][Index].m_Index - 1;
			pOutTriggers->m_SwitchDuration = m_apTiles[GAMELAYERTYPE_SWITCH][Index].m_Reserved - 1;
		}
	}

	pOutTriggers->m_Checkpoint = -1;
	Index = TilePosToIndex(x, y, GAMELAYERTYPE_RACE);
	if(Index >= 0)
	{
		if(m_apTiles[GAMELAYERTYPE_RACE][Index].m_Index > 0)
		{
			if(m_apTiles[GAMELAYERTYPE_RACE][Index].m_Index == TILE_RACE_START)
				pOutTriggers->m_Checkpoint = 0;
			else if(m_apTiles[GAMELAYERTYPE_RACE][Index].m_Index == TILE_RACE_FINISH)
				pOutTriggers->m_Checkpoint = m_NumCheckpoints + 1;
			else
				pOutTriggers->m_Checkpoint = m_apTiles[GAMELAYERTYPE_RACE][Index].m_Index - RACE_FIRST_CP_TILE + 1;
		}
	}


	pOutTriggers->m_Endless = 0;
	pOutTriggers->m_Solo = 0;
	pOutTriggers->m_Nohit = 0;
	Index = TilePosToIndex(x, y, GAMELAYERTYPE_PROPERTEE);
	if(Index >= 0)
	{
		switch(m_apTiles[GAMELAYERTYPE_PROPERTEE][Index].m_Index)
		{
			case TILE_PROPERTEE_ENDLESS_ON:
				pOutTriggers->m_Endless = PROPERTEE_ON;
				break;
			case TILE_PROPERTEE_ENDLESS_OFF:
				pOutTriggers->m_Endless = PROPERTEE_OFF;
				break;
			case TILE_PROPERTEE_SOLO_ON:
				pOutTriggers->m_Solo = PROPERTEE_ON;
				break;
			case TILE_PROPERTEE_SOLO_OFF:
				pOutTriggers->m_Solo = PROPERTEE_OFF;
				break;
			case TILE_PROPERTEE_NOHIT_ON:
				pOutTriggers->m_Nohit = PROPERTEE_ON;
				break;
			case TILE_PROPERTEE_NOHIT_OFF:
				pOutTriggers->m_Nohit = PROPERTEE_OFF;
				break;
		}
	}
}
