#ifndef CACHE_H
#define CACHE_H

#include "u8port.h"

// Each cache node starts with a 29-byte header; handles point past it.
#define NODE_HEADER	29
// Handle of a free node, and of a node no handle points at.
#define FREE_HANDLE	0xfffe
#define NO_HANDLE	0xfffd
#define FREE_TYPE	((CacheNodeType)0xff)

class CacheHandler;

// 16 bits, as Borland laid enums out: CacheNode::type is a word in the node header.
enum CacheNodeType : int16_t
{
	CacheNodeType_0
};

// The header in front of every cache node. Its layout is CACHENOD's CN_* offsets (packed,
// 29 bytes); nodes are copied whole and saved by CacheNodePtr::save, so it must not change.
// next/prev are the block thread (address order), less/more the MRU or free thread; all
// links are 32-bit offsets into CacheNodePtr::memBase.
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

static_assert(sizeof(CacheNode) == NODE_HEADER, "CacheNode is the 29-byte node header");
static_assert(offsetof(CacheNode, handle) == 2 && offsetof(CacheNode, size) == 4 &&
	offsetof(CacheNode, sizeDiff) == 8 && offsetof(CacheNode, sum) == 9 &&
	offsetof(CacheNode, next) == 0x0D && offsetof(CacheNode, prev) == 0x11 &&
	offsetof(CacheNode, less) == 0x15 && offsetof(CacheNode, more) == 0x19,
	"CacheNode fields at CACHENOD's CN_* offsets");

// A node of the cache, named by the 32-bit offset of its header from memBase (the DOS
// build reached it through a selector mapped at the cache's linear base; now it is plain
// memory). Handles hold offsets of node data, NODE_HEADER past the header.
class CacheNodePtr
{
public:
	uint32_t offset;

	CacheNodePtr(void) {}
	CacheNodePtr(uint32_t o) { offset = o; }
	static void *getPtr(uint32_t o) { return (char *)memBase + o; }
	void free(void) { deallocate(*this); }
	// getHandle() is an int; Borland compared it unsigned with these unsigned constants.
	uint8_t isFree(void) { return (uint16_t)getHandle() == FREE_HANDLE; }
	uint8_t isAllocated(void) { return (uint16_t)getHandle() < FREE_HANDLE; }
	uint8_t isDummy(void) { return (uint16_t)getHandle() == 0xffff; }

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
	// The cache block (CACHE_BYTES, malloc'd by allocateCache). Defined in cachenod.cpp.
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
