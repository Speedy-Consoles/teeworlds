/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_EFFECTS_H
#include <game/client/component.h>

class CEffects : public CComponent
{
	bool m_Add50hz;
	bool m_Add100hz;

	int m_DamageTaken;
	float m_DamageTakenTick;
public:
	CEffects();

	virtual void OnRender();

	void BulletTrail(vec2 Pos, int WorldID, bool Solo);
	void SmokeTrail(vec2 Pos, vec2 Vel, int WorldID, bool Solo);
	void SkidTrail(vec2 Pos, vec2 Vel, int WorldID, bool Solo);
	void Explosion(vec2 Pos, int WorldID, bool Solo);
	void HammerHit(vec2 Pos, int WorldID);
	void AirJump(vec2 Pos, int WorldID, bool Solo);
	void Speedup(vec2 Pos, int WorldID, bool Solo);
	void DamageIndicator(vec2 Pos, int Amount, int WorldID);
	void PlayerSpawn(vec2 Pos, int WorldID);
	void PlayerTeleport(vec2 Pos, int WorldID);
	void PlayerDeath(vec2 Pos, int ClientID, int WorldID);
	void PowerupShine(vec2 Pos, vec2 Size);

	void Update();
};
#endif
