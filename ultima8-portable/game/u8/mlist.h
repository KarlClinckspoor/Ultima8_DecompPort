#ifndef MLIST_H
#define MLIST_H

#include "u8port.h"

class BaseFile;
class PidEntry;

// A list word: a pid with flag bits above it, or an operator (0x400 set).
class PidEntry
{
public:
	uint16_t value;

	PidEntry(void) {}
	PidEntry(uint16_t v) { value = v; }
	uint16_t getPid(void) { return value & 0x3ff; }
	uint8_t isOperator(void) { return (value & 0x400) ? 1 : 0; }
	uint8_t isPid(void) { return !isOperator(); }
	uint8_t isPassiveInt(void) { return (value & 0x800) ? 1 : 0; }
	uint8_t isActiveInt(void) { return (value & 0x1000) ? 1 : 0; }
	uint8_t isPassiveDep(void) { return (value & 0x2000) ? 1 : 0; }
	uint8_t isActiveDep(void) { return (value & 0x4000) ? 1 : 0; }
	uint8_t isUsed(void) { return isPassiveInt() || isActiveInt() || isPassiveDep() || isActiveDep(); }
};

class List
{
public:
	uint16_t count;
	uint16_t linearType;
	uint16_t *data;

	List(void) { count = 0; data = 0; linearType = 0; }
	~List(void);
	virtual void load(BaseFile *);
	virtual void save(BaseFile *);
	int16_t constructList(int, ...);	// int: va_start needs a promoted type
	int16_t constructList(uint16_t, const uint16_t *, uint16_t);
	void clear(void);
	int16_t addToList(uint16_t);
	int16_t appendToList(uint16_t);
	int16_t removeFromList(uint16_t);
	uint16_t getMember(uint16_t);
	void setMember(uint16_t, uint16_t);
	int16_t isInList(uint16_t);
	PidEntry isPidInList(uint16_t);
	PidEntry isPidInList(uint16_t, uint16_t &);
	void setListSize(uint16_t);
	uint8_t isAllocated(void) { return data != 0; }
	void setLinearType(uint16_t type) { linearType = type; }
};

#endif
