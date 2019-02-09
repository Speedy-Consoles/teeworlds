/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H

class CProjectile : public CEntity
{
public:
	CProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, bool Explosive, float Force, int SoundImpact, int Weapon, int SwitchGroup, bool InvertSwitch, bool OnlySelf);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_DDRaceProjectile *pProj, int World);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient, int World);

	static bool HitCharacter(CCharacter *pCharacter, void *pUserData);

private:
	vec2 m_Direction;
	vec2 m_StartPos;
	vec2 m_StartDir;
	int m_LifeSpan;
	int m_Owner;
	int m_Type;
	int m_Damage;
	int m_SoundImpact;
	int m_Weapon;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;
	bool m_OnlySelf;
};

#endif
