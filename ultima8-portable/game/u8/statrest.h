#ifndef STATREST_H
#define STATREST_H

#include "u8port.h"

#include "process.h"
#include "randint.h"
#include "urandom.h"

// Restores the avatar's mana and hit points over time.
class StatRestorer : public ProcessRealTime
{
public:
	uint16_t ticks;

	StatRestorer(void);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

void FeedAvatar(int16_t);

#endif
