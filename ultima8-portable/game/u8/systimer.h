#ifndef SYSTIMER_H
#define SYSTIMER_H

#include "u8port.h"

// #include <phapi.h>  (DOS only)
#include "u8process.h"

class BaseFile;

class SystemTimer : public ProcessRealTime
{
public:
	uint32_t ticks;

	SystemTimer(void);
	virtual void process(void);
	virtual void freeMemory(void);
	virtual void load(BaseFile *);
	virtual void interruptHandler(uint32_t, uint32_t);
	void wait(uint32_t);
};

extern SystemTimer *systemTimer;

#endif
