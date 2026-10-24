#ifndef KTIMER_H
#define KTIMER_H

#include "u8port.h"

#include "process.h"

// Runs the sound library's timer service from the kernel's real-time clock.
class AilTimer : public ProcessRealTime
{
public:
	AilTimer(void);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

extern "C" void hook_timer_process(void);
extern "C" void unhook_timer_process(void);
extern "C" void set_PIT_period(int16_t);
extern "C" void interrupt API_timer(...);

extern AilTimer *ailTimer;

#endif
