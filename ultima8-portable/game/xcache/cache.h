#ifndef CACHE_H
#define CACHE_H

#include "u8port.h"

#include "cdesc.h"

// Each cache node starts with a 29-byte header; handles point past it.
#define NODE_HEADER	29
// Handle of a free node, and of a node no handle points at.
#define FREE_HANDLE	0xfffe
#define NO_HANDLE	0xfffd
#define FREE_TYPE	((CacheNodeType)0xff)

class CacheHandler;

enum CacheNodeType
{
	CacheNodeType_0
};

// The header in front of every cache node.
struct CacheNode
{
	CacheNodeType type;
	uint16_t handle;
	int32_t size;
	uint8_t sizeDiff;
	int32_t sum;
	int32_t next;
	int32_t prev;
	int32_t less;
	int32_t more;
};

class CacheNodePtr
{
public:
	uint32_t offset;

	CacheNodePtr(void) {}
	CacheNodePtr(uint32_t o) { offset = o; }
	static void *getPtr(uint32_t o) { return makePtr(base + o); }
	void free(void) { deallocate(*this); }
	uint8_t isFree(void) { return getHandle() == FREE_HANDLE; }
	uint8_t isAllocated(void) { return getHandle() < FREE_HANDLE; }
	uint8_t isDummy(void) { return getHandle() == 0xffff; }

	void freeSpaceToBlockMru(void);
	void makeMru(void);
	int16_t unlinkFromMruThread(void);
	void unlinkFromBlockThread(void);
	void blockThreadInsertAfter(CacheNodePtr);
	void mruThreadReplace(CacheNodePtr);
	void addToFreeChain(void);
	void setMergeLink(CacheNodePtr);
	void reset(CacheNodePtr);
	void shiftUpSet(CacheNodePtr);
	static void setDummyNodes(void);
	int32_t getSum(void);
	int32_t getSize(void);
	int32_t getLess(void);
	int32_t getMore(void);
	int32_t getNext(void);
	int32_t getPrev(void);
	int16_t getHandle(void);
	uint16_t getSizeDiff(void);
	int16_t getType(void);
	void setSize(int32_t);
	void setSum(int32_t);
	void setNext(CacheNodePtr);
	void setPrev(CacheNodePtr);
	void setType(CacheNodeType);
	void setSizeDiff(uint8_t);
	void setHandle(uint16_t);
	void *shiftUpAsm(int32_t);
	void shiftFreeBlock(int32_t);
	static void initCache(int32_t);
	static void uninitCache(void);
	static void allocateCache(void);
	static void attach(CacheHandler *, CacheNodeType);
	static void detach(CacheNodeType);
	static void createDummyNodes(void);
	static CacheNodePtr findSpace(int32_t);
	static int32_t allocate(int32_t, uint16_t, CacheNodeType);
	static void findLargestFreeNode(void);
	static int32_t deallocate(CacheNodePtr);
	static void externalDeallocate(uint32_t);
	static void release(int32_t);
	static void garbageCollect(int32_t);
	void merge(void);
	void shiftUp(int32_t);
	void shiftUpToFreeBlock(void);
	static int16_t deallocateLru(CacheNodeType);
	static void verifyFailed(uint16_t, char *, uint16_t);
	static void verify(char *, uint16_t);
	static void dumpCache(uint8_t);
	void dump(void);
	static void profileCache(void);
	static int16_t getPercentFull(void);
	static void save(char *);
	static void load(char *);
	static void *memBase;
	static CacheNodePtr cache;
	static CacheNodePtr blockRoot;
	static CacheNodePtr freeRoot;
	static CacheNodePtr largestFreeNode;
	static int32_t totalBytes;
	static int32_t freeBytes;
	static uint16_t totalBlocks;
	static uint16_t totalAllocatedBlocks;
	static uint16_t totalFreeBlocks;
	static int32_t largestFreeBytes;
	static int32_t allocatedBytes;
	static int32_t slop;
	static uint16_t garbageCount;
	static uint32_t base;
	static uint16_t numHandlers;
	static uint8_t hasUninit;
	static uint16_t initialized;
	static CacheHandler *handlers[7];
};

void initCache(int32_t);
void UNINITCACHE(uint16_t);
void uninitCache(void);
void verifyCache(char *, int16_t);
void profileCache(void);
void dump(void);

extern uint16_t haltOnFail;

#endif
