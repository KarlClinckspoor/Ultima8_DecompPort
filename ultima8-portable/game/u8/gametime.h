#ifndef GAMETIME_H
#define GAMETIME_H

#include "u8port.h"

#include "process.h"

// Counts game loops; starts the npc schedules when the game hour changes.
class GameTime : public Process
{
public:
	uint32_t loops;
	int16_t pause;		// loops to skip counting, -1 to stop
	uint8_t hourChanged;

	GameTime(void);
	virtual void process(void);
	virtual void postLoad(void);
	uint8_t isPaused(void) { return pause != 0; }
};

// The 30 Hz game clock. A game hour is 27000 ticks.
class Animation : public ProcessRealTime
{
public:
	uint32_t f_38;	// animation frames
	int32_t f_3c;		// frame count as of the last unlock
	int32_t f_40;		// ticks
	int32_t f_44;		// game hours
	int32_t f_48;		// tick count to jump to, -1 for none
	uint8_t f_4c;	// frame count locked
	uint8_t f_4d;	// animation stopped

	Animation(void);
	virtual void process(void) {}
	virtual void postLoad(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	void lockTime(void);
	void unlockTime(void);
	void setTicks(int32_t t) { f_48 = t; }
	int32_t getHours(void) { return f_44; }
};

void initGameTime(void);
int32_t TimeInGameLoops(void);
int32_t TimeInSeconds(void);
int32_t TimeInMinutes(void);
int32_t TimeInGameHours(void);
void SetTimeInSeconds(int32_t);
void SetTimeInMinutes(int32_t);
void SetTimeInGameHours(int16_t);

extern GameTime *theGameTime;
extern Animation *theAnimation;

#endif
