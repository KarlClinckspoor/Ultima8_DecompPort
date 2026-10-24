#ifndef CPROTMEM_H
#define CPROTMEM_H

#include "u8port.h"

// #include <alloc.h>  (DOS only)

// A descriptor whose bytes 6 and 7 hold a list index.
class FreeDescriptor
{
public:
	uint8_t pad_00[6];
	uint8_t indexLow;
	uint8_t indexHigh;

	uint16_t getIndex(void);
	void setIndex(uint16_t);
};

#define BLOCK_HEADER 10UL

// Header of a block of linear memory, reached through makePtr().
// Blocks are laid end to end; a free block's size is its length - 1 (odd),
// a used block's is its length, with its owner tag after prevBlock.
class FreeBlock
{
public:
	uint32_t size;
	uint32_t prevBlock;	// block before this one in memory
	union {
		uint32_t prev;	// free list links, free blocks only
		uint16_t tag;	// owner, used blocks only
	};
	uint32_t next;

	uint32_t getNext(void);
	uint32_t getLength(void) { return size + 1; }
	uint8_t isFree(void) { return size & 1; }
	uint8_t isEnd(void) { return size == 0xFFFFFFFFL; }
	uint32_t getSize(void) { return size & 0x0FFFFFFFL; }
	void setSize(uint32_t s) { size = (size & 0xF0000000L) | s; }
	void setFreeLength(uint32_t len) { size = len - 1; }
	void setTag(uint16_t t) { tag = t; }
	void setPrevBlock(uint32_t p) { prevBlock = p; }
	void setPrev(uint32_t p) { prev = p; }
	void setNext(uint32_t n) { next = n; }
};

// One range of linear memory with its own free list.
class Fragment
{
public:
	uint32_t start;	// list head block
	uint32_t length;
	uint16_t used;
	uint16_t freeBlocks;

	uint32_t allocate(uint32_t, uint16_t);
	void free(void *, uint16_t);
	void garbageCollect(void);
	void verify(char *, int16_t);
	void getMemStatus(uint32_t &, uint32_t &);
	uint8_t contains(uint32_t a) { return start <= a && start + length >= a; }
};

#define MAX_FRAGMENTS 10

class ProtMemoryManager
{
public:
	static void verify(char *, int16_t);
	static void *allocate(uint32_t, uint16_t, uint8_t);
	static void garbageCollect(uint8_t);
	static void free(void *, uint16_t);
	static void allocateFragments(void);
	static void freeFragments(void);
	static void getMemStatus(int16_t, uint32_t &, uint32_t &);
	static void getMemStatus(uint32_t &, uint32_t &, uint16_t &);
	static void initialize(void);
	static uint8_t initialized;
	static uint8_t fromLargest;
	static uint8_t failedLastRequest;
	static Fragment fragments[MAX_FRAGMENTS];
	static uint16_t numFragments;
};

void CLEANUP(uint16_t);

extern uint16_t FRAGMENT_0_PERCENTAGE;
extern uint8_t newAddress1[4];
extern uint8_t newAddress2[4];
extern uint8_t reserved[1];
extern uint8_t addr_xhi[1];

#endif
