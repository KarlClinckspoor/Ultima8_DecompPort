#ifndef CPROTMEM_H
#define CPROTMEM_H

#include "u8port.h"

// The DOS build's memory manager: linear memory from the extender, in up to ten fragments
// with their own free lists, each block reached through a selector of its own, and garbage
// collection that slid blocks together by remapping their selectors. The host's allocator
// does all of that now; this keeps the calls the rest of the game makes.

// The memory the game was told it had, for its status displays.
#define PORT_MEMORY_SIZE 0x4000000UL

class Fragment
{
public:
	uint32_t start;
	uint32_t length;
	uint16_t used;
	uint16_t freeBlocks;

	void getMemStatus(uint32_t &total, uint32_t &largest) { total = largest = length; }
};

#define MAX_FRAGMENTS 10

class ProtMemoryManager
{
public:
	static void verify(char *, int16_t) {}
	// tag (the owning process, or 0) and anyFragment no longer matter.
	static void *allocate(uint32_t size, uint16_t tag, uint8_t anyFragment);
	static void garbageCollect(uint8_t) {}
	static void free(void *block, uint16_t tag);
	static void getMemStatus(int16_t, uint32_t &total, uint32_t &largest);
	static void getMemStatus(uint32_t &total, uint32_t &largest, uint16_t &selectors);
	static void initialize(void) { initialized = 1; }
	static uint8_t initialized;
	static uint8_t fromLargest;
	static uint8_t failedLastRequest;
	static Fragment fragments[MAX_FRAGMENTS];
	static uint16_t numFragments;
};

extern uint16_t FRAGMENT_0_PERCENTAGE;

#endif
