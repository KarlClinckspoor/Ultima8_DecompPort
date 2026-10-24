#ifndef SPANKER_H
#define SPANKER_H

#include "u8port.h"

// #include <phapi.h>  (DOS only)

class Descriptor;

// Hooks every interrupt through a jump table so 32-bit registers survive the
// extender's handlers, and samples cs:ip for the profiler.
class Spanky
{
public:
	static void init(void);
	static void initIdtGate(void);
	static void uninit(void);
	static void dosSetPassToProtVec(int16_t, PIHANDLER, PIHANDLER *, REALPTR *);
	static void dosSetRealProtVec(int16_t, PIHANDLER, REALPTR, PIHANDLER *, REALPTR *);
	static void setIdtrToOriginal(void);
	static void setIdtrToIntercept(void);
	static uint8_t isIntact(void);
	static void bufferProfileData(void);
	static int32_t getNextCsIp(void);
	static void setOriginalIdtAddress(uint32_t);
	static void checkProfiler(void);
	static void toggleProfiler(void);
	static uint8_t haveCsIps(void) { return numCsIpPtrs != 0; }
	static uint32_t *csIpPtrArray;
	static uint32_t *csIpPtrRetrievalArray;
	static uint16_t numCsIpPtrs;
	static uint16_t numCsIpRetrievalPtrs;
	static uint16_t currentCsIpRetrievalPtr;
	static uint8_t csIpPtrOverflow;
	static uint8_t csIpPtrRetrievalOverflow;
	static uint8_t profiling;
	static uint32_t *profilePtr;
	static int32_t nextProfileSlot;
	static uint8_t profilerInitialized;
	static uint8_t intercepting;
	static uint8_t initialized;
	static Descriptor *interceptorBuffer;
	static Descriptor *origBuffer;
	static void *intJumpTable;
	static void *patchPointer;
	static uint16_t setRegsCodeSeg;
	static uint16_t setRegsOrigCodeOff;
	static uint16_t setRegsNewCodeOff;
	static uint32_t originalIdtAddress;
	static uint32_t newIdtAddress;
};

void FREESPANKER(uint16_t);
void initSpanky(void);
void uninitSpanky(void);

extern "C" void getIntJumpTable(void);
extern "C" void getPatchPosition(void);
extern "C" void getSetRegsCodeInfo(void);

extern void (*setOrigIdtrCallGate)(void);
extern void (*setNewIdtrCallGate)(void);

#endif
