#ifndef UPROCESS_H
#define UPROCESS_H

#include "u8port.h"

#include "u8process.h"
#include "inter.h"

class BaseFile;
class FastFlex;
class Item;

// A usecode process: runs one event routine of a unit (usecode class).
class UnkProcess : public Process
{
public:
	uint16_t unit;
	Routine routine;
	uint32_t *resultDest;	// receives the result when the routine returns
	uint16_t thisOffset;		// stack offset of the pushed this pointer
	uint8_t thisSize;
	uint8_t finished;
	uint8_t running;
	char *killed;			// set while running when the process is freed

	UnkProcess(uint32_t *, void *, uint16_t, uint16_t, uint16_t, uint8_t, char *, int16_t);
	virtual void process(void);
	virtual void freeMemory(void);
	virtual void load(BaseFile *);
	virtual void save(BaseFile *);
};

// Record 1 of the usecode file: one entry per unit.
class Overload
{
public:
	uint32_t events;	// bit n set: the unit handles event n
	char name[9];
};

// A unit's code, loaded on demand and cached.
class UnitIndex
{
public:
	uint16_t unit;
	char *code;
	uint16_t useCount;
	uint16_t size;
};

#define MAX_UNITS	30

class RoutineIndex
{
public:
	RoutineIndex(void);
	~RoutineIndex(void);
	static UnitIndex *findUnit(uint16_t, UnitIndex *&);
	static int16_t getNumFree(void);
	static char *getCode(uint16_t, uint16_t, uint8_t, int32_t);
	static void markUnused(uint16_t);
	static char *codeBase(uint16_t);
	static uint8_t cacheOut(int16_t);
	static Overload *overloadTable;
	static int32_t unkMemoryUsed;
	static int32_t maxUnkMemory;
	static UnitIndex unitIndex[MAX_UNITS];
};

uint8_t mangleName(char *, char *);
void enumMangle(char *, const char *);
void initCFunctions(void);
void initUnk(void);
void reloadUnk(void);
uint8_t spawnUnk(Item *, uint32_t, uint16_t, uint16_t &, uint32_t);
void resetRef(uint16_t, uint16_t);
void setRef(uint16_t, uint16_t, uint16_t);

extern FastFlex *usecodeFile;
extern uint8_t failedInit;
extern uint8_t unkInitted;
extern void **cTable;
extern uint16_t numCFunctions;

#endif
