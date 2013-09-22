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

	while(length(Vel) > 0)
	{
		int x = Pos.x / 32;
		int y = Pos.y / 32;

		if(x == Pos.x / 32 && Vel.x < 0)
			x--;
		if(y == Pos.y / 32 && Vel.y < 0)
			y--;

		ivec2 TilePos = ivec2(x, y);
		vec2 TileMid = vec2(TilePos.x * 32 + 16, TilePos.y * 32 + 16);
		int Sign[2] = {(Vel.x > 0) - (Vel.x < 0), (Vel.y > 0) - (Vel.y < 0)};
		bool NeighborSolid[2] = {IsTileSolid((x + Sign[0]) * 32, y * 32), IsTileSolid(x * 32, (y + Sign[1]) * 32)};

		float MinCrossingTime = 1.0f;
		int FirstDim = -1;
		int FirstSign = 0;
		int FirstBorder = 0;
		bool Outer = false;

		for(int i = 0; i < 2; i++)
		{
			if(Vel[i] == 0)
				continue;

			int InnerBorder = floor(TileMid[i] + (15.5f - Size[i] / 2) * Sign[i]);
			int OuterBorder = TileMid[i] + 16 * Sign[i];
			if(Pos[i] * Sign[i] <= InnerBorder * Sign[i])
			{
				float CrossingTime = (InnerBorder - Pos[i]) / Vel[i];
				if(CrossingTime >= 0 && CrossingTime < 1 && CrossingTime <= MinCrossingTime)
				{
					bool Collision = NeighborSolid[i];
					if(!Collision)
					{
						int LowOtherBorder = TileMid[1 - i] - (16 - Size[1 - i] / 2);
						int HighOtherBorder = TileMid[1 - i] + (15 - Size[1 - i] / 2);
						float OtherDimNewPos = Pos[1 - i] + Vel[1 - i] * CrossingTime;
						ivec2 CornerNeighbor = TilePos;
						CornerNeighbor[i] += Sign[i];
						if(OtherDimNewPos >= HighOtherBorder)
						{
							CornerNeighbor[1 - i]++;
							if(OtherDimNewPos > HighOtherBorder)
								Collision |= IsTileSolid(32 * CornerNeighbor.x, 32 * CornerNeighbor.y);
							else if(OtherDimNewPos == HighOtherBorder && Vel[1 - i] > 0)
							{
								ivec2 OtherNeighbor = TilePos;
								OtherNeighbor[1 - i]++;
								Collision |= IsTileSolid(32 * CornerNeighbor.x, 32 * CornerNeighbor.y)
									&& !IsTileSolid(32 * OtherNeighbor.x, 32 * OtherNeighbor.y);
							}
						}
						else if(OtherDimNewPos <= LowOtherBorder)
						{
							CornerNeighbor[1 - i]--;
							if(OtherDimNewPos < LowOtherBorder)
								Collision |= IsTileSolid(32 * CornerNeighbor.x, 32 * CornerNeighbor.y);
							else if(OtherDimNewPos == LowOtherBorder && Vel[1 - i] < 0)
							{
								ivec2 OtherNeighbor = TilePos;
								OtherNeighbor[1 - i]--;
								Collision |= IsTileSolid(32 * CornerNeighbor.x, 32 * CornerNeighbor.y)
									&& !IsTileSolid(32 * OtherNeighbor.x, 32 * OtherNeighbor.y);
							}
						}
					}
					if(Collision)
					{
						FirstBorder = InnerBorder;
						MinCrossingTime = CrossingTime;
						FirstDim = i;
						Outer = false;
					}
				}
			}
			else if(Pos[i] * Sign[i] <= OuterBorder * Sign[i])
			{
				float CrossingTime = (OuterBorder - Pos[i]) / Vel[i];
				if(CrossingTime >= 0 && CrossingTime < 1 && CrossingTime < MinCrossingTime)
				{
					FirstBorder = OuterBorder;
					MinCrossingTime = CrossingTime;
					FirstDim = i;
					Outer = true;
				}
			}
		}

		if(FirstDim != -1)
		{
			Pos[1 - FirstDim] += Vel[1 - FirstDim] * MinCrossingTime;
			Pos[FirstDim] = FirstBorder;
			Vel *= 1 - MinCrossingTime;

			if(!Outer)
			{
				Vel[FirstDim] *= Elasticity * FirstSign;
				NewVel[FirstDim] *= Elasticity * FirstSign;
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
