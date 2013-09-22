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
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
}

void CCollision::Init(class CLayers *pLayers)
{
	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index = m_pTiles[i].m_Index;

		if(Index > 128)
			continue;

		switch(Index)
		{
		case TILE_DEATH:
			m_pTiles[i].m_Index = COLFLAG_DEATH;
			break;
		case TILE_SOLID:
			m_pTiles[i].m_Index = COLFLAG_SOLID;
			break;
		case TILE_NOHOOK:
			m_pTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_NOHOOK;
			break;
		default:
			m_pTiles[i].m_Index = 0;
		}
	}
}

int CCollision::GetTile(int x, int y)
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
}

bool CCollision::IsTileSolid(int x, int y)
{
	return GetTile(x, y)&COLFLAG_SOLID;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision)
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;

	for(int i = 0; i < End; i++)
	{
		float a = i/Distance;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(CheckPoint(Pos.x, Pos.y))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(Pos.x, Pos.y);
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
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces)
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size)
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y))
		return true;
	return false;
}

// assumes that the box is smaller than one tile
void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity)
{
	ivec2 Dirs[4] = {ivec2(1, 0), ivec2(0, 1), ivec2(-1, 0), ivec2(0, -1)};
	int DirValue[4] = {1,1,-1,-1};
	int DirDim[4] = {0, 1, 0, 1};

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	vec2 NewVel = Vel;

	while(length(Vel) > 0.00001)
	{
		int x = Pos.x / 32;
		int y = Pos.y / 32;

		if(x == Pos.x / 32 && Vel.x < 0)
			x--;
		if(y == Pos.y / 32 && Vel.y < 0)
			y--;

		ivec2 TilePos = ivec2(x, y);
		vec2 TileMid = vec2(TilePos.x * 32 + 16, TilePos.y * 32 + 16);
		vec2 PosOffset = Pos - TileMid;
		float MinCrossingTime = 1.0f;
		int FirstDir = -1;
		int FirstBorder = 0;
		bool Outer = false;

		for(int i = 0; i < 4; i++)
		{
			int Sign = DirValue[i];
			int Dim = DirDim[i];

			if(Vel[Dim] * Sign <= 0)
				continue;

			if(PosOffset[Dim] * Sign <= 16 - Size[Dim] / 2)
			{
				int Border = TileMid[Dim] + (16 - Size[Dim] / 2) * Sign;
				float CrossingTime = (Border - Pos[Dim]) / Vel[Dim];
				if(CrossingTime >= 0 && CrossingTime < 1 && (i == 0 || CrossingTime < MinCrossingTime))
				{
					ivec2 Neighbor = TilePos + Dirs[i];
					bool Collision = IsTileSolid(32 * Neighbor.x, 32 * Neighbor.y);
					float OtherDimPosOffset = Pos[1 - Dim] + Vel[1 - Dim] * CrossingTime - TileMid[1 - Dim];
					if(fabs(OtherDimPosOffset) > 16 - Size[1 - Dim] / 2)
					{
						ivec2 CornerNeighbor = Neighbor;
						CornerNeighbor[1 - Dim] += (OtherDimPosOffset > 0) - (OtherDimPosOffset < 0);
						Collision |= IsTileSolid(32 * CornerNeighbor.x, 32 * CornerNeighbor.y);
					}
					if(Collision)
					{
						FirstBorder = Border;
						MinCrossingTime = CrossingTime;
						FirstDir = i;
						Outer = false;
					}
				}
			}
			else
			{
				int Border = TileMid[Dim] + 16 * Sign;
				float CrossingTime = (Border - Pos[Dim]) / Vel[Dim];
				if(CrossingTime > 0 && CrossingTime <= 1 && (i == 0 || CrossingTime < MinCrossingTime))
				{
					FirstBorder = Border;
					MinCrossingTime = CrossingTime;
					FirstDir = i;
					Outer = true;
				}
			}
		}

		if(FirstDir != -1)
		{
			int Sign = DirValue[FirstDir];
			int Dim = DirDim[FirstDir];

			Pos[1 - Dim] += Vel[1 - Dim] * MinCrossingTime;
			Pos[Dim] = FirstBorder;
			Vel *= 1 - MinCrossingTime;

			if(!Outer)
			{
				dbg_msg("dbg", "Collision on %d, %d in Direction %d", x, y, FirstDir);
				Vel[Dim] *= Elasticity * Sign;
				NewVel[Dim] *= Elasticity * Sign;
			}
		}
		else
		{
			Pos += Vel;
			Vel = vec2(0.0f, 0.0f);
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = NewVel;
}























