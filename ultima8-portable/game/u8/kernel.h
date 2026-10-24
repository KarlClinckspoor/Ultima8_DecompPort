#ifndef KERNEL_H
#define KERNEL_H

#include "u8port.h"

// #include <phapi.h>  (DOS only)

class IntList;
class PriorityClass;
class Process;

enum Message
{
	Message_0
};

enum MessageAddress
{
	MessageAddress_0
};

enum MessageGroup
{
	MessageGroup_0
};

enum ProcessType
{
	ProcessType_0,
	PT_ANY = 6,	// matches every type in a search
	PT_END = 7,	// ends a list of types
	PT_FIRST = 0xf0	// the first type tracked per item
};

class Kernel
{
public:
	static void init(void);
	static void uninit(void);
	static void error(void);
	static void doProcesses(void);
	static void calculateProcessPriority(void);
	static void reconfigureCheck(void);
	static void reconfigureShrink(uint16_t);
	static uint8_t reconfigureExpand(void);
	static int16_t getNumProcesses(uint16_t, ProcessType);
	static int16_t getNumLinearProcesses(uint16_t, ProcessType);
	static int16_t getOpenSlot(void);
	static uint16_t spawn(Process *);
	static Process *getProcess(uint16_t);
	static void resetRef(uint16_t, ProcessType);
	static void killProcess(uint16_t, ProcessType, PriorityClass);
	static void killProcessSpecifiedPTypes(uint16_t, PriorityClass, ...);
	static void killProcessNonSpecifiedPTypes(uint16_t, PriorityClass, ...);
	static void restart(void);
	static void addDepElement(uint16_t, uint16_t);
	static Process *findValidProcess(uint16_t, ProcessType);
	static Process *findNextValidProcess(void);
	static Process *getLinearExecutor(uint16_t, ProcessType);
	static Process *getNextLinear(void);
	static void setCurrentLinearScanPid(uint16_t);
	static uint16_t getCurrentProcess(void);
	static void appendLinearProcess(Process *);
	static void makeInvalid(uint16_t);
	static void remove(uint16_t);
	static void setCurrentPriority(uint8_t);
	static void checkSynchronization(Process *);
	static uint8_t shouldSynchronize(Process *);
	static void sendMessage(Message, MessageAddress, MessageGroup, uint16_t, int32_t);
	static void setCurrentProcessSize(uint16_t);
	static void initMemory(void);
	static void freeProcesses(void);
	static void freeDaemons(void);
	static void freePhantoms(void);
	static void freeMemory(void);
	static void load(char *);
	static void save(char *);
	static void setPitDivisor(uint16_t);
	static void setPitPeriod(uint32_t);
	static void realTimeExecutor(void);
	static uint8_t isRealInterrupt(void);
	static REGS_BINT *getRegsStructure(void);
	static void addToRealTime(uint16_t, uint8_t);
	static void removeFromRealTime(uint16_t);
	static void seizeRealTimeVector(void);
	static void releaseRealTimeVector(void);
	static void enableRealTimeInterrupts(void);
	static void disableRealTimeInterrupts(void);
	static void calculateTimerRate(void);
	static void execute(uint32_t, void (*)(void));
	static void realTimeOn(void);
	static void realTimeOff(void);
	static void interrupt realTimeInterruptHandler(REGS_BINT);
	static void interruptHandlerStage2(void);
	static void checkIntegrity(void);
	static void setProcessRefActive(uint16_t);
	static void clrProcessRefActive(uint16_t);
	static void checkProcessRefActive(uint16_t);
	static uint8_t isProcessRefActive(uint16_t);
	static void setProcessTypeActive(uint16_t, ProcessType);
	static void clrProcessTypeActive(uint16_t, ProcessType);
	static void checkProcessTypeActive(uint16_t, ProcessType);
	static uint8_t isProcessTypeActive(uint16_t, ProcessType);
	static void clrAccumulator(void);
	static void checkAccumulator(void);
	static Process **processList;
	static uint16_t *processSize;
	static uint16_t reconfigureCounter;
	static uint16_t currentScanSlot;
	static uint16_t currentLinearScanPid;
	static uint16_t maxProcesses;
	static uint16_t totalProcesses;
	static uint16_t currentProcess;
	static uint16_t currentProcessSize;
	static uint8_t expandOnDemand;
	static uint8_t processing;
	static uint8_t initialized;
	static uint8_t keyboardVectorChanged[1];
	static uint8_t keyboardBufferOverflow[1];
	static uint8_t restarting;
	static uint16_t numPhantoms;
	static uint16_t searchingRef;
	static uint16_t searchingProcessType;
	static uint8_t keyboardBufferHead[2];
	static uint8_t keyboardBufferTail[2];
	static uint16_t *priorityCount;
	static uint32_t theCurrentTick;
	static uint8_t currentPriority;
	static uint8_t abortDoProcesses;
	static uint16_t currentPolledIntPid;
	static uint8_t origKeyboardProt[4];
	static uint8_t origKeyboardReal[4];
	static char **idStringList;
	static void decPriorityCount(uint8_t p) { priorityCount[p]--; }
	static void incPriorityCount(uint8_t p) { priorityCount[p]++; }
	static uint8_t isProcess(Process *p) { return p != 0; }
	static void setIdString(uint16_t id, char *name) { idStringList[id] = name; }
	static void setProcessSize(uint16_t id, uint16_t size) { processSize[id] = size; }
	static uint16_t pitDivisor;
	static uint32_t pitPeriod;
	static IntList realTimeList;
	static uint8_t interruptDebug;
	static uint16_t hertz;
	static uint16_t totalSafe;
	static uint16_t totalNonSafe;
	static uint16_t interruptStackSegment;
	static uint16_t interruptStackOffset;
	static uint16_t interruptStackSize;
	static uint16_t exceptionStackSegment;
	static uint16_t exceptionStackOffset;
	static uint16_t exceptionStackSize;
	static uint16_t oldIntStackSeg;
	static uint16_t oldIntStackOff;
	static uint16_t oldExceptionStackSeg;
	static uint16_t oldExceptionStackOff;
	static uint16_t currentInterruptPid;
	static uint16_t currentExceptionPid;
	static uint8_t inInterrupt;
	static uint8_t interruptPending;
	static uint8_t easyAtomicFlag;
	static uint8_t intStatusChanged;
	static uint8_t realTimeEnabled;
	static uint8_t timerVectorChanged;
	static uint8_t wantRealTime;
	static uint8_t reentrancyFlag;
	static uint8_t reentrantException;
	static uint8_t processBlank;
	static uint8_t *inDosPtr;
	static PIHANDLER origProt;
	static uint32_t origReal;
	static REGS_BINT *borlandInterruptRegsPtr;
	static REGS_BINT *borlandExceptionRegsPtr;
	static uint8_t *quickRefProcessArray;
	static uint8_t *quickProcessTypeArray;
	static uint32_t *accumulatorTable;
	static uint8_t magicNumberEnd;
	static uint8_t magicNumberTerminator;
};

void initKernel(void);
void uninitKernel(void);

#endif
