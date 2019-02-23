#include "racetimer.h"

#include <engine/server.h>

CRaceTimer::CRaceTimer()
{
	m_pServer = 0;
	m_State = RACESTATE_STARTING;
	m_StartTick = 0;
	m_EndTick = 0;
}

void CRaceTimer::Start()
{
	if(m_pServer && m_State == RACESTATE_STARTING)
	{
		m_State = RACESTATE_STARTED;
		m_StartTick = m_pServer->Tick();
	}
}


void CRaceTimer::Stop()
{
	if(m_pServer && m_State == RACESTATE_STARTED)
	{
		m_State = RACESTATE_FINISHED;
		m_EndTick = m_pServer->Tick();
	}
}


void CRaceTimer::Cancel()
{
	if(m_State == RACESTATE_STARTED)
		m_State = RACESTATE_CANCELED;
}


void CRaceTimer::Reset()
{
	m_State = RACESTATE_STARTING;
	m_StartTick = 0;
	m_EndTick = 0;
}

void CRaceTimer::TickPaused () {
	if(m_State == RACESTATE_STARTED)
		++m_StartTick;
};

float CRaceTimer::Time() const
{
	if(!m_pServer)
		return 0.0f;
	else if (m_State == RACESTATE_STARTED)
		return (m_pServer->Tick() - m_StartTick) / (float) m_pServer->TickSpeed();
	else if(m_State == RACESTATE_FINISHED)
		return (m_EndTick - m_StartTick) / (float) m_pServer->TickSpeed();

	return 0.0f;
}
