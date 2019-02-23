#ifndef GAME_SERVER_RACETIMER_H
#define GAME_SERVER_RACETIMER_H

class IServer;

class CRaceTimer
{
public:
	enum
	{
		RACESTATE_STARTING = 0,
		RACESTATE_STARTED,
		RACESTATE_FINISHED,
		RACESTATE_CANCELED,
	};

	CRaceTimer();
	void SetServer(IServer *pServer) { m_pServer = pServer; }
	void Start();
	void Stop();
	void Cancel();
	void Reset();
	void TickPaused ();

	int State() const { return m_State; }
	float Time() const;
private:
	IServer *m_pServer;
	int m_State;
	int m_StartTick;
	int m_EndTick;
};

#endif
