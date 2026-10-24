#ifndef WAIT_H
#define WAIT_H

#include "u8port.h"

#include "u8process.h"

// Pops after a number of game loops.
class RoundTimer : public Process
{
public:
	uint16_t count;

	RoundTimer(uint16_t, uint16_t);
	virtual void process(void) { if (--count == 0) pop(0); }
};

// Pops after a number of 30 Hz ticks.
class Wait : public ProcessRealTime
{
public:
	uint16_t count;
	uint16_t ticks;

	Wait(uint16_t, uint16_t);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	virtual void restart(void);
	virtual void timerDone(void) { pop(0); }
	void start(void);
};

// A Wait that fails instead of popping.
class FailWait : public Wait
{
public:
	FailWait(uint16_t t, uint16_t r) : Wait(t, r) {}
	virtual void interruptHandler(uint32_t, uint32_t);
};

// Counts seconds and tenths while running.
class Timer : public ProcessRealTime
{
public:
	uint8_t running;
	uint16_t seconds;
	uint16_t tenths;

	Timer(void);
	virtual void process(void) {}
	virtual void interruptHandler(uint32_t, uint32_t);
	void stop(void);
	void start(void);
};

class AccurateTimer : public ProcessRealTime
{
public:
	uint8_t timing;
	uint32_t ticks;

	AccurateTimer(int16_t);
	virtual void process(void) {}
	virtual void interruptHandler(uint32_t, uint32_t);
	void stop(void);
	void start(void);
	uint8_t isTiming(void);
	uint16_t getTicks(void);
};

#endif
