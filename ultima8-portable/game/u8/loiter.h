#ifndef LOITER_H
#define LOITER_H

#include "u8port.h"

#include "process.h"
#include "mlist.h"

// Wanders an npc about its spot, fidgeting now and then.
class Loiter : public ProcessInterrupt
{
public:
	int16_t count;

	Loiter(uint16_t, int16_t);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

#endif
