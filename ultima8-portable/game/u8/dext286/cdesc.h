#ifndef CDESC_H
#define CDESC_H

#include "u8port.h"

#include "cytable.h"

// An 8-byte 286/386 segment descriptor.
class Desc
{
public:
	uint16_t limit;
	uint16_t baseLow;
	uint8_t baseMid;
	uint8_t access;
	uint8_t limitHigh;	// limit bits 16-19 in the low nibble
	uint8_t baseHigh;

	int32_t getBase(void);
	void setBase(uint32_t base)
	{
		baseLow = base;
		baseMid = base >> 16;
		baseHigh = base >> 24;
	}
	void setLimit(uint32_t);
	void display(uint16_t);
};

#define RESERVED_SELECTORS 32

// Saves the bases of the reserved selectors and puts them back.
class HoldPointers
{
public:
	uint32_t base[RESERVED_SELECTORS];

	HoldPointers(void);
	~HoldPointers(void);
};

class LocalDescriptorTable
{
public:
	static void initialize(void);
	static Desc *getDesc(uint16_t sel) { return &table[sel >> 3]; }
	static uint32_t limit;
	static uint16_t mappedSelector;
	static Desc *table;
};

class GlobalDescriptorTable
{
public:
	static void initialize(void);
	static uint32_t limit;
	static uint16_t mappedSelector;
	static Desc *table;
};

void GetBiosSegment(void);
void SetVgaScreenDescriptor(void);
uint16_t MapSegment(uint16_t, uint16_t);
int16_t MERGEBYTEARRAY(char *, char *, int16_t);
uint16_t getLDT(void);
void initReservedSelectors(void);
void *makePtr(uint32_t);

extern int16_t init;
extern uint16_t TextSelector;
extern char *error;
extern char *delta;
extern char *vector;
extern int16_t major;
extern int16_t iterations;

#endif
