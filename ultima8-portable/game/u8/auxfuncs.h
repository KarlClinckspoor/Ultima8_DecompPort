#ifndef AUXFUNCS_H
#define AUXFUNCS_H

#include "u8port.h"

#include "process.h"

// Now and then, while the avatar stands idle, plays a look-around animation.
class HeadShaker : public ProcessRealTime
{
public:
	uint16_t seconds;	// since the avatar last animated
	uint16_t wait;		// seconds until the next look

	HeadShaker(void);
	virtual void process(void) {}
	virtual void postLoad(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	virtual void freeMemory(void);
	void animationUsed(void);
};

extern HeadShaker *headShaker;

#endif
