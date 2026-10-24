#ifndef REFSTORE_H
#define REFSTORE_H

#include "u8port.h"

#include "process.h"
#include "kernel.h"

class RefStorage;
class StorageData;

// Delivers one hit or got-hit event to an item as a process.
class StorageData : public Process
{
public:
	RefStorage *storage;
	uint16_t other;
	int16_t force;
	uint16_t index;
	ProcessType event;
	uint8_t invalidated;

	StorageData(RefStorage *, ProcessType, uint16_t, uint16_t, int16_t);
	virtual void process(void);
	virtual ~StorageData(void);
	void setIndex(uint16_t i) { index = i; }
};

// The events of one movement, run once it is done.
class RefStorage
{
public:
	StorageData **list;
	int16_t size;
	uint16_t count;

	RefStorage(int16_t);
	~RefStorage(void);
	void set(StorageData *);
	void invalidate(uint16_t);
	void execute(void);
};

#endif
