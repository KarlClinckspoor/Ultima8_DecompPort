#ifndef STDINT_H
#define STDINT_H

#include "u8port.h"

#include "process.h"
#include "mlist.h"
#include "interupt.h"
#include "auxfuncs.h"

class BaseFile;
class IntList;
class StdIntHandler;

extern StdIntHandler *stdIntHandler;

// The pids polled for interrupts, with the position of a poll in progress.
class IntList : public List
{
public:
	uint8_t changed;	// a pid was removed during the poll
	uint16_t current;
	uint16_t last;

	void removeFromList(uint16_t);
};

// Each game loop, offers every pending interrupt bit to the processes that want them.
class StdIntHandler : public Process
{
public:
	IntList intList;
	IntList f_39;

	StdIntHandler(void);
	virtual void process(void);
	virtual void freeMemory(void);
	virtual void load(BaseFile *);
	virtual void save(BaseFile *);
	virtual void postLoad(void) { stdIntHandler = this; }
};

class ArmorData
{
public:
	static char shieldBonus[3];
	static char leggingBonus[3];
	static char helmetBonus[6];
	static char armourBonus[4];
	static char armGuardsBonus[5];
	static char magArmorBonus[2];
	static char magArmorTwoBonus[2];
	static char magShieldBonus[2];
	static char magArmsBonus[2];
	static char magLegsBonus[2];
	static char magHelmBonus[2];
	static char clothesBonus[14];
};


extern StdIntHandler *stdIntHandler;

#endif
