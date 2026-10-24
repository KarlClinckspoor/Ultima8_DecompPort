#ifndef PROCESS_H
#define PROCESS_H

#include "u8port.h"

#include "kernel.h"
#include "mlist.h"
#include "notlist.h"

class BaseFile;
class Process;
class ProcessInterrupt;

// Process::flags bits
#define PROC_SUSPENDED		0x0001
#define PROC_DAEMON		0x0002
#define PROC_INTHANDLER	0x0004
#define PROC_PHANTOM		0x0008
#define PROC_CONDITIONAL	0x0010
#define PROC_FAILED		0x0020
#define PROC_SKIP		0x0040
#define PROC_MESSAGES		0x0080
#define PROC_INTERRUPT		0x0100
#define PROC_REALTIME		0x0200
#define PROC_ONREALTIME	0x0400
#define PROC_LINEAR		0x0800
#define PROC_EXECUTOR		0x1000
#define PROC_SAFE		0x2000
#define PROC_TERMINATED	0x4000
#define PROC_DEPFAILED		0x8000	// a process this one waited on failed
#define PROC_DEPFAILED	0x8000	// a process this one was notified of failed

// Dependency list words: a pid, or one of these operators.
#define DEP_TRUE	0x401
#define DEP_AND		0x402
#define DEP_OR		0x404
#define DEP_END		0x408

class Process
{
public:
	uint16_t pid;
	uint16_t ref;
	ProcessType processType;
	uint8_t priority;
	uint8_t pad_09[1];
	int32_t result;
	uint16_t flags;
	uint16_t f_10;
	uint16_t allocations;
	uint16_t messageGroups;
	List dependList;
	NotifyList notifyList;

	Process(void);
	virtual void process(void) = 0;
	virtual ~Process(void) {}
	virtual void pop(int32_t);
	virtual void fail(int32_t);
	virtual void receiveMessage(Message, uint16_t, int32_t);
	virtual void freeMemory(void);
	virtual void load(BaseFile *);
	virtual void save(BaseFile *);
	virtual void postLoad(void);
	static void *operator new(size_t);
	static void operator delete(void *);
	void setRef(uint16_t);
	void setProcessType(ProcessType);
	// Borland turned a number into an enum by itself; callers pass numbers.
	void setProcessType(int type) { setProcessType((ProcessType)type); }
	void setPriority(uint8_t);
	void notifyDependents(void);
	void unlinkFromKernel(void);
	void kill(void);
	void disableCriticalInterrupts(void);
	void terminate(void);
	void checkDependencies(uint16_t);
	void makeDependencyList(uint16_t, ...);
	void clrPassiveInt(uint16_t);
	void clrActiveInt(uint16_t);
	void clrPassiveDep(uint16_t);
	void clrActiveDep(uint16_t);
	void setPassiveInt(uint16_t);
	void setActiveInt(uint16_t);
	void setPassiveDep(uint16_t);
	void setActiveDep(uint16_t);
	Process *push(Process *);
	void cpush(Process *);
	Process *then(Process *);
	void cthen(Process *);
	uint16_t push(uint16_t);
	void sendMessage(Message, MessageAddress, MessageGroup, int32_t);
	void sendMessage(Message, uint16_t, int32_t);
	void setLinearProcess(void);
	void clrLinearProcess(void);
	void setDaemon(void);
	void setLinearExecutor(void);
	void clrLinearExecutor(void);
	void setPhantom(void);
	int16_t allocate(uint32_t);
	void deallocate(void *);
	void callDestructor(void);

	uint8_t getPriority(void) { return priority & 0x1f; }
	void setPid(uint16_t p) { pid = p; }
	void setFlags(uint16_t f) { flags = f; }
	void setResult(int32_t r) { result = r; }
	int32_t getResult(void) { return result; }
	uint8_t isSuspended(void) { return (flags & PROC_SUSPENDED) ? 1 : 0; }
	uint8_t isDaemon(void) { return (flags & PROC_DAEMON) ? 1 : 0; }
	uint8_t isOnIntHandler(void) { return (flags & PROC_INTHANDLER) ? 1 : 0; }
	uint8_t isPhantom(void) { return (flags & PROC_PHANTOM) ? 1 : 0; }
	uint8_t isSkipped(void) { return (flags & PROC_SKIP) ? 1 : 0; }
	uint8_t acceptsMessages(void) { return (flags & PROC_MESSAGES) ? 1 : 0; }
	uint8_t isInterrupt(void) { return (flags & PROC_INTERRUPT) ? 1 : 0; }
	uint8_t isOnRealTime(void) { return (flags & PROC_ONREALTIME) ? 1 : 0; }
	uint8_t isLinearProcess(void) { return (flags & PROC_LINEAR) ? 1 : 0; }
	uint8_t isLinearExecutor(void) { return (flags & PROC_EXECUTOR) ? 1 : 0; }
	uint8_t isSafeInterrupt(void) { return (flags & PROC_SAFE) ? 1 : 0; }
	uint8_t isTerminated(void) { return (flags & PROC_TERMINATED) ? 1 : 0; }
	uint8_t dependencyFailed(void) { return (flags & PROC_DEPFAILED) ? 1 : 0; }
	uint8_t isFailed(void) { return (flags & PROC_FAILED) ? 1 : 0; }
	uint8_t isF10_2(void) { return (f_10 & 2) ? 1 : 0; }
	uint8_t isF10_4(void) { return (f_10 & 4) ? 1 : 0; }
	uint8_t isSaved(void) { return (f_10 & 1) ? 1 : 0; }
	uint8_t isRealTime(void) { return (flags & PROC_REALTIME) ? 1 : 0; }
};

class ProcessInterrupt : public Process
{
public:
	uint32_t intFlags;

	ProcessInterrupt(void) { flags |= PROC_INTERRUPT; intFlags = 0; }
	virtual uint8_t checkInterrupt(uint32_t, uint32_t &);
	virtual void interruptHandler(uint32_t, uint32_t);
	void doNotNotifyOfDeath(uint16_t);
	void notifyOfDeath(uint16_t);
	void connectToIntHandler(void);
	void disconnectFromIntHandler(void);
};

class ProcessRealTime : public ProcessInterrupt
{
public:
	uint16_t hertz;
	uint32_t period;
	uint32_t elapsed;

	ProcessRealTime(void);
	void setHertz(uint16_t);
	void setPeriod(uint32_t);
	void realTimeCycle(void);
	void connectToRealTime(uint8_t);
	void disconnectFromRealTime(void);
	void setSafeInterrupt(void);
	void clrSafeInterrupt(void);
	void setIntFlags(uint32_t f) { intFlags = f; }
};

#endif
