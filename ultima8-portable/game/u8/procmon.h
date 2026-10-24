#ifndef PROCMON_H
#define PROCMON_H

#include "u8port.h"

#include "u8process.h"

// Counts frames and prints process, memory and timing figures once a frame.
class ProcessMonitor : public ProcessRealTime
{
public:
	uint16_t frames;
	uint16_t fps;
	uint16_t ticks;

	ProcessMonitor(void);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	void turnOn(void);
	void turnOff(void);
};

#endif
