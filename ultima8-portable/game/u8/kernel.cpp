// flags: -P -2 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: KERNEL.C

#include "u8port.h"

// #include <phapi.h>  (DOS only)
// #include <dos.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include <stdarg.h>
// #include <alloc.h>  (DOS only)
#include "process.h"
#include "kernel.h"
#include "chargen.h"
#include "error.h"
#include "stdint.h"
#include "systimer.h"
#include "biostime.h"
#include "filespec.h"
#include "flex.h"
#include "cglobvp.h"
#include "spanker.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Inline in the shared headers when this file was built.
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline SharedFile::SharedFile(void) {}
inline FlexFile::FlexFile(void) {}
inline Index::Index(void) { size = 0; offset = 0; }

extern "C" void PutPixel(Vport *, int16_t, int16_t, int16_t);
extern CharacterGenerator charGen;

Process **Kernel::processList = 0;
uint16_t *Kernel::processSize = 0;
uint16_t Kernel::reconfigureCounter = 0xffff;
uint16_t Kernel::currentScanSlot = 0;
uint16_t Kernel::currentLinearScanPid = 0;
uint16_t Kernel::maxProcesses = 100;
uint16_t Kernel::totalProcesses = 0;
uint16_t Kernel::currentProcess = 0;
uint16_t Kernel::currentProcessSize = 0;
uint8_t Kernel::expandOnDemand = 0;
uint8_t Kernel::processing = 0;
uint8_t Kernel::initialized = 0;
uint8_t Kernel::keyboardVectorChanged[1] = {0};
uint8_t Kernel::keyboardBufferOverflow[1] = {0};
uint8_t Kernel::restarting = 0;
uint16_t Kernel::numPhantoms = 0;
uint16_t Kernel::searchingRef = 0;
uint16_t Kernel::searchingProcessType = 6;
uint8_t Kernel::keyboardBufferHead[2] = {0, 0};
uint8_t Kernel::keyboardBufferTail[2] = {0, 0};
uint16_t *Kernel::priorityCount = 0;
uint32_t Kernel::theCurrentTick = 0;
uint8_t Kernel::currentPriority = 1;
uint8_t Kernel::abortDoProcesses = 0;
uint16_t Kernel::currentPolledIntPid = 0;
uint8_t Kernel::origKeyboardProt[4] = {0, 0, 0, 0};
uint8_t Kernel::origKeyboardReal[4] = {0, 0, 0, 0};
char **Kernel::idStringList = 0;
uint16_t Kernel::pitDivisor = 0;
uint32_t Kernel::pitPeriod = 0;
IntList Kernel::realTimeList = IntList();
uint8_t Kernel::interruptDebug = 0;
uint16_t Kernel::hertz = 0;
uint16_t Kernel::totalSafe = 0;
uint16_t Kernel::totalNonSafe = 0;
uint16_t Kernel::interruptStackSegment = 0;
uint16_t Kernel::interruptStackOffset = 0;
uint16_t Kernel::interruptStackSize = 0x960;
uint16_t Kernel::exceptionStackSegment = 0;
uint16_t Kernel::exceptionStackOffset = 0;
uint16_t Kernel::exceptionStackSize = 0x960;
uint16_t Kernel::oldIntStackSeg = 0;
uint16_t Kernel::oldIntStackOff = 0;
uint16_t Kernel::oldExceptionStackSeg = 0;
uint16_t Kernel::oldExceptionStackOff = 0;
uint16_t Kernel::currentInterruptPid = 0;
uint16_t Kernel::currentExceptionPid = 0;
uint8_t Kernel::inInterrupt = 0;
uint8_t Kernel::interruptPending = 0;
uint8_t Kernel::easyAtomicFlag = 0;
uint8_t Kernel::intStatusChanged = 0;
uint8_t Kernel::realTimeEnabled = 0;
uint8_t Kernel::timerVectorChanged = 0;
uint8_t Kernel::wantRealTime = 0;
uint8_t Kernel::reentrancyFlag = 0;
uint8_t Kernel::reentrantException = 0;
uint8_t Kernel::processBlank = 0;
uint8_t *Kernel::inDosPtr = 0;
PIHANDLER Kernel::origProt = 0;
uint32_t Kernel::origReal = 0;
REGS_BINT *Kernel::borlandInterruptRegsPtr = 0;
REGS_BINT *Kernel::borlandExceptionRegsPtr = 0;
uint8_t *Kernel::quickRefProcessArray = 0;
uint8_t *Kernel::quickProcessTypeArray = 0;
uint32_t *Kernel::accumulatorTable = 0;
uint8_t Kernel::magicNumberEnd = 0xec;
uint8_t Kernel::magicNumberTerminator = 0xee;

void initKernel(void)
{
	Kernel::init();
}

void uninitKernel(void)
{
	Kernel::uninit();
}

void Kernel::init(void)
{
	uint16_t es;
	uint16_t bx;
	uint16_t para;
	uint16_t sel;
	uint32_t linear;
	uint16_t seg;
	uint16_t offset;
	BiosTimer *timer;
	REGS16 regs;

	if (initialized)
		halt(__FILE__, 261);	// __LINE__
	initialized = 1;
	initMemory();
	hertz = 0;
	if (DosAllocRealSeg(interruptStackSize, &para, &sel))
		halt(__FILE__, 283);	// __LINE__
	interruptStackSegment = sel;
	if (DosAllocRealSeg(exceptionStackSize, &para, &sel))
		halt(__FILE__, 288);	// __LINE__
	exceptionStackSegment = sel;

	// Find DOS's InDOS flag.
	memset(&regs, 0, sizeof(regs));
	regs.ax = 0x3400;
	DosRealIntr(0x21, &regs, 0L, 0);
	asm sti
	es = regs.es;
	bx = regs.bx;
	linear = (es << 4) + bx;
	offset = linear & 0xf;
	seg = linear >> 4;
	DosMapRealSeg(seg, 16L, &sel);
	inDosPtr = (uint8_t *)MK_FP(sel, offset);

	disableInterrupt(0);
	seizeRealTimeVector();
	stdIntHandler = new StdIntHandler;
	timer = new BiosTimer;
	timer->setTimerVector(origReal, origProt);
	timer->connectToRealTime(1);
	new SystemTimer;
}

void Kernel::uninit(void)
{
	if (initialized) {
		initialized = 0;
		disableRealTimeInterrupts();
		releaseRealTimeVector();
		setPitPeriod(54920L);
		enableInterrupt(0);
		freeMemory();
	}
}

void Kernel::error(void)
{
	if (initialized) {
		disableRealTimeInterrupts();
		releaseRealTimeVector();
		setPitPeriod(54920L);
		enableInterrupt(0);
	}
}

void Kernel::doProcesses(void)
{
	Process *p;
	uint8_t priority;
	int16_t size;
	REGS_BINT regs;

	processing = 1;
	memset(&regs, 0, sizeof(regs));
	size = sizeof(regs);
	for (currentProcess = 0; currentProcess < maxProcesses; currentProcess++) {
		p = processList[currentProcess];
		if (isProcess(p)) {
			if (!p->isPhantom()) {
				if (p->isSkipped())
					p->flags &= ~PROC_SKIP;
				else if (!p->dependList.isAllocated()) {
					if (!p->isSuspended()) {
						if (p->isLinearProcess()) {
							if (!p->isLinearExecutor())
								halt(__FILE__, 536);	// __LINE__
						}
						priority = p->getPriority();
						if (p->isDaemon() || priority == currentPriority || priority == 31)
							p->process();
					}
				}
			}
		}
		if (interruptPending && realTimeEnabled) {
			asm pushf
			asm cli
			asm xor ax, ax
			asm mov es, ax
			processBlank = 1;
			realTimeInterruptHandler(regs);
			asm add sp, size
			processBlank = 0;
			asm popf
		}
		freePhantoms();
		if (abortDoProcesses) {
			abortDoProcesses = 0;
			break;
		}
	}
	if (priorityCount[currentPriority] == 0)
		calculateProcessPriority();
	reconfigureCheck();
	processing = 0;
}

void Kernel::calculateProcessPriority(void)
{
	uint8_t highest = 1;
	uint16_t i;
	Process *p;

	for (i = 0; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p)) {
			if (p->getPriority() > highest && p->getPriority() != 31)
				highest = p->getPriority();
		}
	}
	setCurrentPriority(highest);
	abortDoProcesses = 0;
}

void Kernel::reconfigureCheck(void)
{
	uint16_t i;

	if (reconfigureCounter != 0xffff) {
		reconfigureCounter--;
		if (reconfigureCounter == 0) {
			for (i = maxProcesses - 1; i >= 100; i--)
				if (isProcess(processList[i]))
					break;
			if (i == 99 && totalProcesses + 30 < 100) {
				reconfigureShrink(100);
				return;
			}
			if (maxProcesses - 20 > i && totalProcesses + 30 < maxProcesses - 20) {
				reconfigureShrink(maxProcesses - 20);
				return;
			}
			reconfigureCounter = 150;
		}
	}
}

void Kernel::reconfigureShrink(uint16_t newMax)
{
	uint16_t i;
	Process **newList;
	Process **oldList;
	uint16_t *newSize;
	uint16_t *oldSize;
	char **newIds;
	char **oldIds;

	newSize = new uint16_t[newMax];
	newList = new Process *[newMax];
	newIds = new char *[newMax];
	if (!newList || !newSize || !newIds)
		halt(__FILE__, 727);	// __LINE__
	for (i = 0; i < newMax; i++) {
		newList[i] = processList[i];
		newSize[i] = processSize[i];
		newIds[i] = idStringList[i];
	}
	oldList = processList;
	oldSize = processSize;
	oldIds = idStringList;
	asm pushf
	asm cli
	processList = newList;
	processSize = newSize;
	idStringList = newIds;
	maxProcesses = newMax;
	if (newMax != 100)
		reconfigureCounter = 150;
	else
		reconfigureCounter = 0xffff;
	asm popf
	delete oldList;
	delete oldSize;
	delete oldIds;
}

uint8_t Kernel::reconfigureExpand(void)
{
	uint16_t i;
	uint16_t newMax = maxProcesses + 30;
	Process **newList;
	Process **oldList;
	uint16_t *newSize;
	uint16_t *oldSize;
	char **newIds;
	char **oldIds;

	if (!expandOnDemand)
		halt(__FILE__, 786);	// __LINE__
	if (newMax > 1023)
		halt(__FILE__, 790);	// __LINE__
	newList = new Process *[newMax];
	newSize = new uint16_t[newMax];
	newIds = new char *[newMax];
	if (!newList || !newSize || !newIds)
		halt(__FILE__, 803);	// __LINE__
	memset(newIds, 0, newMax * sizeof(char *));
	for (i = 0; i < maxProcesses; i++) {
		newList[i] = processList[i];
		newSize[i] = processSize[i];
		newIds[i] = idStringList[i];
	}
	for (i = maxProcesses; i < newMax; i++) {
		newList[i] = 0;
		newSize[i] = 0;
		newIds[i] = 0;
	}
	oldList = processList;
	oldSize = processSize;
	oldIds = idStringList;
	asm pushf
	asm cli
	processList = newList;
	processSize = newSize;
	idStringList = newIds;
	maxProcesses += 30;
	reconfigureCounter = 150;
	asm popf
	delete oldList;
	delete oldSize;
	delete oldIds;
	return 1;
}

int16_t Kernel::getNumProcesses(uint16_t ref, ProcessType type)
{
	uint16_t i;
	int16_t count;
	Process *p;

	i = 0;
	count = 0;
	for (; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && p->ref == ref && (type == PT_ANY || p->processType == type) && !p->isPhantom())
			count++;
	}
	return count;
}

int16_t Kernel::getNumLinearProcesses(uint16_t ref, ProcessType type)
{
	uint16_t i;
	int16_t count;
	Process *p;

	i = 0;
	count = 0;
	for (; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && p->ref == ref && (type == PT_ANY || p->processType == type) && !p->isPhantom())
			count++;
	}
	return count;
}

int16_t Kernel::getOpenSlot(void)
{
	uint16_t i;
	Process **slot;

	do {
		slot = processList;
		for (i = 0; i < maxProcesses; i++)
			if (!isProcess(*slot++))
				return i;
	} while (reconfigureExpand());
	return -1;
}

uint16_t Kernel::spawn(Process *p)
{
	uint16_t slot;

	if (!initialized)
		halt(__FILE__, 920);	// __LINE__
	if (!isProcess(p))
		halt(__FILE__, 923);	// __LINE__
	slot = getOpenSlot();
	processList[slot] = p;
	p->setPid(slot);
	setProcessSize(slot, currentProcessSize);
	currentProcessSize = 0;
	totalProcesses++;
	idStringList[slot] = "Default";
	return slot;
}

Process *Kernel::getProcess(uint16_t pid)
{
	Process *p;

	if (pid == 0)
		return 0;
	if (pid >= maxProcesses)
		return 0;
	p = processList[pid];
	if (!isProcess(p))
		return 0;
	if (p->isPhantom())
		;
	return p;
}

void Kernel::resetRef(uint16_t ref, ProcessType type)
{
	killProcess(ref, type, PriorityClass(0x21));
}

void Kernel::killProcess(uint16_t ref, ProcessType type, PriorityClass priorities)
{
	Process *p;
	uint16_t i;

	if (!priorities.isValid())
		halt(__FILE__, 1005);	// __LINE__
	if (isProcessRefActive(ref)) {
		for (i = 3; i < maxProcesses; i++) {
			p = processList[i];
			if (isProcess(p) && (ref == 0 || p->ref == ref) && (type == PT_ANY || p->processType == type)
					&& !p->isDaemon() && !p->isTerminated() && priorities.contains(p->getPriority()))
				p->fail(0);
		}
	}
}

// The process types to kill follow priorities, ended by PT_END.
void Kernel::killProcessSpecifiedPTypes(uint16_t ref, PriorityClass priorities, ...)
{
	ProcessType type;
	Process *p;
	uint16_t i;
	uint16_t j;
	uint16_t count;
	va_list ap;
	uint16_t arg;
	uint16_t types[10];

	if (!priorities.isValid())
		halt(__FILE__, 1045);	// __LINE__
	va_start(ap, priorities);
	for (count = 0; count < 10; count++) {
		arg = va_arg(ap, uint16_t);
		types[count] = arg;
		if (arg == PT_END)
			break;
	}
	if (count == 10)
		halt(__FILE__, 1062);	// __LINE__
	if (isProcessRefActive(ref)) {
		for (i = 3; i < maxProcesses; i++) {
			p = processList[i];
			if (isProcess(p) && p->ref == ref && !p->isDaemon() && !p->isTerminated()
					&& priorities.contains(p->getPriority())) {
				type = p->processType;
				for (j = 0; j < count; j++)
					if (types[j] == type) {
						p->fail(0);
						break;
					}
			}
		}
	}
}

void Kernel::killProcessNonSpecifiedPTypes(uint16_t ref, PriorityClass priorities, ...)
{
	ProcessType type;
	Process *p;
	uint16_t i;
	uint16_t j;
	uint16_t count;
	va_list ap;
	uint16_t arg;
	uint16_t types[10];

	if (!priorities.isValid())
		halt(__FILE__, 1112);	// __LINE__
	va_start(ap, priorities);
	for (count = 0; count < 10; count++) {
		arg = va_arg(ap, uint16_t);
		types[count] = arg;
		if (arg == PT_END)
			break;
	}
	if (count == 10)
		halt(__FILE__, 1129);	// __LINE__
	if (isProcessRefActive(ref)) {
		for (i = 3; i < maxProcesses; i++) {
			p = processList[i];
			if (isProcess(p) && p->ref == ref && !p->isDaemon() && !p->isTerminated()
					&& priorities.contains(p->getPriority())) {
				type = p->processType;
				for (j = 0; j < count; j++)
					if (types[j] == type)
						break;
				if (j == count)
					p->fail(0);
			}
		}
	}
}

void Kernel::restart(void)
{
	Process *p;
	ProcessInterrupt *pi;
	uint16_t i;

	restarting = 1;
	for (i = 3; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p)) {
			if (p->isTerminated() && !p->isPhantom())
				halt(__FILE__, 1183);	// __LINE__
			if (p->isInterrupt()) {
				pi = (ProcessInterrupt *)p;
				if (pi->intFlags & 8)
					pi->interruptHandler(8, 0);
			}
			if (!p->isDaemon() && !p->isTerminated())
				p->fail(0);
		}
	}
	restarting = 0;
}

void Kernel::addDepElement(uint16_t pid, uint16_t element)
{
	PidEntry entry(element);
	Process *p;

	if (pid && element) {
		p = getProcess(pid);
		p->dependList.appendToList(element);
		if (entry.isPid()) {
			getProcess(element)->setActiveDep(pid);
			p->setPassiveDep(element);
		}
	}
}

Process *Kernel::findValidProcess(uint16_t ref, ProcessType type)
{
	Process *p;

	searchingRef = ref;
	searchingProcessType = type;
	for (currentScanSlot = 3; currentScanSlot < maxProcesses; currentScanSlot++) {
		p = processList[currentScanSlot];
		if (isProcess(p) && (ref == 0 || p->ref == ref) && (type == PT_ANY || p->processType == type)
				&& !p->isPhantom()) {
			currentScanSlot++;
			return p;
		}
	}
	return 0;
}

Process *Kernel::findNextValidProcess(void)
{
	Process *p;
	uint16_t ref = searchingRef;
	ProcessType type = (ProcessType)searchingProcessType;

	if (currentScanSlot >= maxProcesses)
		return 0;
	for (; currentScanSlot < maxProcesses; currentScanSlot++) {
		p = processList[currentScanSlot];
		if (isProcess(p) && (ref == 0 || p->ref == ref) && (type == PT_ANY || p->processType == type)
				&& !p->isPhantom()) {
			currentScanSlot++;
			return p;
		}
	}
	return 0;
}

Process *Kernel::getLinearExecutor(uint16_t ref, ProcessType type)
{
	uint16_t i;
	Process *p;

	if (type < 0xf0 || type > 0xf7 || isProcessTypeActive(ref, type)) {
		for (i = 3; i < maxProcesses; i++) {
			p = processList[i];
			if (isProcess(p) && p->ref == ref && p->processType == type && p->isLinearExecutor()
					&& !p->isPhantom()) {
				currentLinearScanPid = i;
				return p;
			}
		}
	}
	currentLinearScanPid = maxProcesses;
	return 0;
}

// The next process in the chain waits on the current one.
Process *Kernel::getNextLinear(void)
{
	Process *p;
	uint16_t i;
	uint16_t ref;
	ProcessType type;
	uint16_t j;
	uint16_t last;

	if (currentLinearScanPid == maxProcesses)
		return 0;
	p = processList[currentLinearScanPid];
	ref = p->ref;
	type = p->processType;
	for (i = 3; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && p->isLinearProcess() && p->ref == ref && p->processType == type
				&& !p->isPhantom() && p->dependList.isAllocated()) {
			last = p->dependList.count - 1;
			for (j = 0; j < last; j++)
				if (p->dependList.getMember(j) == currentLinearScanPid) {
					currentLinearScanPid = i;
					return p;
				}
		}
	}
	currentLinearScanPid = maxProcesses;
	return 0;
}

void Kernel::setCurrentLinearScanPid(uint16_t pid)
{
	Process *p = getProcess(pid);

	if (!p->isLinearProcess())
		halt(__FILE__, 1369);	// __LINE__
	currentLinearScanPid = pid;
}

uint16_t Kernel::getCurrentProcess(void)
{
	Process *p;

	if (currentProcess >= maxProcesses)
		return 0;
	p = processList[currentProcess];
	if (!isProcess(p) || p->isPhantom())
		return 0;
	return currentProcess;
}

void Kernel::appendLinearProcess(Process *p)
{
	Process *last;
	Process *next;

	if (!p->isLinearProcess())
		halt(__FILE__, 1395);	// __LINE__
	last = getLinearExecutor(p->ref, p->processType);
	if (!last) {
		p->setLinearExecutor();
		return;
	}
	for (next = getNextLinear(); next; next = getNextLinear())
		last = next;
	last->setActiveDep(p->pid);
	p->setPassiveDep(last->pid);
	p->dependList.addToList(last->pid);
}

void Kernel::makeInvalid(uint16_t pid)
{
	Process *p = getProcess(pid);
	uint16_t ref = processList[pid]->ref;
	ProcessType type = processList[pid]->processType;

	priorityCount[p->getPriority()]--;
	if (p->isF10_4()) {
		remove(pid);
		free(p);
	} else
		p->setPhantom();
	totalProcesses--;
	if (ref)
		checkProcessRefActive(ref);
	if (ref && type >= 0xf0 && type <= 0xf7)
		checkProcessTypeActive(ref, type);
}

void Kernel::remove(uint16_t pid)
{
	processList[pid] = 0;
	processSize[pid] = 0;
	idStringList[pid] = 0;
}

void Kernel::setCurrentPriority(uint8_t priority)
{
	if (currentPriority != priority) {
		currentPriority = priority;
		abortDoProcesses = 1;
	}
}

void Kernel::checkSynchronization(Process *p)
{
	if (shouldSynchronize(p))
		p->flags |= PROC_SKIP;
}

// A process spawned ahead of the scan this cycle waits for the next one.
uint8_t Kernel::shouldSynchronize(Process *p)
{
	if (processing && p->pid > currentProcess)
		return 1;
	return 0;
}

void Kernel::sendMessage(Message message, MessageAddress address, MessageGroup group, uint16_t from, int32_t data)
{
	uint16_t i;
	Process *p;

	for (i = 0; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && (address == 0 || (p->messageGroups & group)))
			p->receiveMessage(message, from, data);
	}
}

void Kernel::setCurrentProcessSize(uint16_t size)
{
	if (currentProcessSize)
		halt(__FILE__, 1524);	// __LINE__
	currentProcessSize = size;
}

void Kernel::initMemory(void)
{
	processList = new Process *[maxProcesses];
	processSize = new uint16_t[maxProcesses];
	priorityCount = new uint16_t[32];
	if (!processList || !processSize || !priorityCount)
		halt(__FILE__, 1543);	// __LINE__
	memset(processList, 0, maxProcesses * sizeof(Process *));
	memset(processSize, 0, maxProcesses * sizeof(uint16_t));
	memset(priorityCount, 0, 32 * sizeof(uint16_t));
	quickRefProcessArray = new uint8_t[1152];
	if (!quickRefProcessArray)
		halt(__FILE__, 1554);	// __LINE__
	memset(quickRefProcessArray, 0, 1152);
	quickProcessTypeArray = new uint8_t[9216];
	if (!quickProcessTypeArray)
		halt(__FILE__, 1565);	// __LINE__
	memset(quickProcessTypeArray, 0, 9216);
	accumulatorTable = new uint32_t[4096];
	if (!accumulatorTable)
		halt(__FILE__, 1576);	// __LINE__
	clrAccumulator();
	idStringList = new char *[maxProcesses];
	if (!idStringList)
		halt(__FILE__, 1587);	// __LINE__
	memset(idStringList, 0, maxProcesses * sizeof(char *));
}

void Kernel::freeProcesses(void)
{
	uint16_t i;
	Process *p;

	for (i = 0; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && !p->isPhantom() && !p->isDaemon()) {
			p->freeMemory();
			if (p->allocations)
				halt(__FILE__, 1610);	// __LINE__
			free(p);
			processList[i] = 0;
		}
	}
}

void Kernel::freeDaemons(void)
{
	uint16_t i;
	Process *p;

	for (i = 0; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && p->isDaemon() && !p->isPhantom()) {
			p->freeMemory();
			if (p->allocations)
				halt(__FILE__, 1638);	// __LINE__
			free(p);
			processList[i] = 0;
		}
	}
}

void Kernel::freePhantoms(void)
{
	uint16_t i;
	Process *p;

	if (numPhantoms) {
		for (i = 3; i < maxProcesses && numPhantoms; i++) {
			p = processList[i];
			if (isProcess(p) && p->isPhantom()) {
				if (p->allocations)
					halt(__FILE__, 1668);	// __LINE__
				remove(i);
				free(p);
				numPhantoms--;
			}
		}
	}
}

void Kernel::freeMemory(void)
{
	if (realTimeEnabled)
		halt(__FILE__, 1707);	// __LINE__
	freeProcesses();
	freeDaemons();
	freePhantoms();
	free(processList);
	free(processSize);
	free(priorityCount);
	processList = 0;
	processSize = 0;
	priorityCount = 0;
	if (realTimeList.isAllocated())
		realTimeList.clear();
	free(quickRefProcessArray);
	quickRefProcessArray = 0;
	free(quickProcessTypeArray);
	quickProcessTypeArray = 0;
	free(accumulatorTable);
	accumulatorTable = 0;
	free(idStringList);
	idStringList = 0;
}

void Kernel::load(char *name)
{
	uint16_t i;
	int16_t record = 0;
	uint16_t offset;
	Process *p;
	FileSpec spec("", name, "kernel", "dat");

	if (inInterrupt || !initialized)
		halt(__FILE__, 1770);	// __LINE__
	if (spec.exists()) {
		uint8_t *savedInDos;
		uint16_t savedIntSeg;
		uint16_t savedIntOff;
		uint16_t savedExcSeg;
		uint16_t savedExcOff;
		PIHANDLER savedProt;
		uint32_t savedReal;
		Process *timer;
		uint8_t found;

		disableRealTimeInterrupts();
		setPitDivisor(0);
		releaseRealTimeVector();
		enableInterrupt(0);
		freeMemory();
		FlexFile file(spec, ReadOnly, -1);
		record++;

		// The kernel's statics come back in one record; keep what belongs
		// to this session.
		savedInDos = inDosPtr;
		savedIntSeg = interruptStackSegment;
		savedIntOff = interruptStackOffset;
		savedExcSeg = exceptionStackSegment;
		savedExcOff = exceptionStackOffset;
		savedProt = origProt;
		savedReal = origReal;
		file.readRecord(record++, &processList);
		inDosPtr = savedInDos;
		interruptStackSegment = savedIntSeg;
		interruptStackOffset = savedIntOff;
		exceptionStackSegment = savedExcSeg;
		exceptionStackOffset = savedExcOff;
		origProt = savedProt;
		origReal = savedReal;

		initMemory();
		file.seekRecord(record++);
		realTimeList.load(&file);
		file.readRecord(record++, processSize);
		file.readRecord(record++, quickRefProcessArray);
		file.readRecord(record++, quickProcessTypeArray);
		file.readRecord(record++, priorityCount);
		file.readRecord(record++, idStringList);
		for (i = 0; i < maxProcesses; i++) {
			offset = FP_OFF(idStringList[i]);
			if (offset)
				idStringList[i] = (char *)MK_FP(_DS, offset);
		}

		record = 10;
		for (i = 0; i < maxProcesses; i++) {
			if (processSize[i] == 0) {
				record += 2;
				continue;
			}
			processList[i] = (Process *)operator new(processSize[i]);
			p = processList[i];
			if (!p)
				halt(__FILE__, 1930);	// __LINE__
			file.readRecord(record++, p, processSize[i]);
			file.seekRecord(record++);
			p->load(&file);
		}
		for (i = 0; i < maxProcesses; i++)
			if (processList[i])
				processList[i]->postLoad();

		found = 0;
		for (i = 0; i < maxProcesses; i++) {
			timer = processList[i];
			if (isProcess(timer) && timer->processType == 3) {
				if (found)
					halt(__FILE__, 1963);	// __LINE__
				found = 1;
				((BiosTimer *)timer)->setTimerVector(origReal, origProt);
			}
		}
		disableInterrupt(0);
		seizeRealTimeVector();
		setPitDivisor(pitDivisor);
		realTimeOn();
		if (magicNumberEnd != 0xed)
			halt(__FILE__, 1989);	// __LINE__
		magicNumberEnd = 0xec;
		if (magicNumberTerminator == 0xef)
			halt(__FILE__, 1994);	// __LINE__
		magicNumberTerminator = 0xee;
	} else
		restart();
}

void Kernel::save(char *name)
{
	uint16_t i;
	int16_t record = 0;
	FlexFile file;
	FileSpec spec("", name, "kernel", "dat");
	char id[30] = "Kernel Ver. 1.00 - T. Zurovec";
	uint16_t defines = 0;
	Index index;
	Process *p;
	char buffer[100];

	if (inInterrupt || !initialized)
		halt(__FILE__, 2026);	// __LINE__
	for (i = 0; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && !p->isPhantom() && !p->isSaved())
			p->fail(0);
	}
	freePhantoms();
	realTimeOff();
	setPitDivisor(0);
	releaseRealTimeVector();
	enableInterrupt(0);
	magicNumberEnd = 0xed;
	magicNumberTerminator = 0xef;
	file.create(spec, maxProcesses * 2 + 10);

	// The build options the kernel was compiled with.
	defines |= 0x002;
	defines |= 0x004;
	defines |= 0x008;
	defines |= 0x010;
	defines |= 0x020;
	defines |= 0x040;
	defines |= 0x100;
	defines |= 0x200;
	defines |= 0x400;
	defines |= 0x800;
	sprintf(buffer, "Defines=%x Id=%s", defines, id);
	file.writeRecord(record++, buffer, strlen(buffer) + 1);

	file.writeRecord(record++, &processList, 157);
	file.getIndex(record, index);
	file.seek(0, FromEnd);
	index.offset = file.size();
	realTimeList.save(&file);
	index.size = file.size() - index.offset;
	file.setIndex(record++, index);
	file.writeRecord(record++, processSize, maxProcesses * 2);
	file.writeRecord(record++, quickRefProcessArray, 1152);
	file.writeRecord(record++, quickProcessTypeArray, 9216);
	file.writeRecord(record++, priorityCount, 64);
	file.writeRecord(record++, idStringList, maxProcesses * 4);

	record = 10;
	for (i = 0; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && p->isSaved()) {
			file.writeRecord(record++, p, processSize[i]);
			file.getIndex(record, index);
			file.seek(0, FromEnd);
			index.offset = file.size();
			p->save(&file);
			index.size = file.size() - index.offset;
			file.setIndex(record++, index);
		} else
			record += 2;
	}
	magicNumberEnd = 0xec;
	magicNumberTerminator = 0xee;
	disableInterrupt(0);
	seizeRealTimeVector();
	setPitDivisor(pitDivisor);
	realTimeOn();
}

void Kernel::setPitDivisor(uint16_t divisor)
{
	asm pushf
	asm cli
	asm mov al, 0x36
	asm out 0x43, al
	asm mov ax, divisor
	asm out 0x40, al
	asm mov al, ah
	asm nop
	asm out 0x40, al
	asm popf
}

void Kernel::setPitPeriod(uint32_t microseconds)
{
	asm pushf
	asm cli
	hertz = 1000000L / microseconds;
	pitPeriod = microseconds;
	if (microseconds <= 54920L)
		microseconds = microseconds * 10000 / 8380;
	else
		halt(__FILE__, 2293);	// __LINE__
	pitDivisor = microseconds;
	setPitDivisor(microseconds);
	asm popf
}

void Kernel::realTimeExecutor(void)
{
	ProcessRealTime *p;
	uint32_t flags;
	uint16_t pid;
	uint8_t inDos;
	uint8_t runAll;
	uint8_t savedFlag;
	uint16_t savedIndex;
	uint16_t savedLast;
	uint16_t savedCurrent;

	runAll = 1;
	if (interruptDebug) {
		Process *current;
		uint16_t currentPid;

		if (processBlank)
			PutPixel(GlobalVport::main_screen, 2, 0, ((int32_t)rand() << 8) / 32768L);
		else
			PutPixel(GlobalVport::main_screen, 0, 0, ((int32_t)rand() << 8) / 32768L);
		currentPid = getCurrentProcess();
		if (currentPid) {
			current = getProcess(currentPid);
			charGen.makeString(0, 2, "PT=%x - %s ", current->processType, idStringList[currentPid]);
		}
	}
	if (reentrantException)
		runAll = 0;
	else if (totalNonSafe) {
		inDos = *inDosPtr ? 1 : 0;
		if (interruptPending) {
			if (processBlank) {
				if (inDos)
					goto done;
				interruptPending = 0;
			} else if (inDos || processing) {
				if (!totalSafe)
					goto done;
				runAll = 0;
			} else
				interruptPending = 0;
		} else if (inDos || processing) {
			interruptPending = 1;
			if (!totalSafe)
				goto done;
			runAll = 0;
		}
	}
	if (reentrantException) {
		savedFlag = realTimeList.changed;
		savedIndex = realTimeList.current;
		savedLast = realTimeList.last;
	}
	realTimeList.last = realTimeList.count;
	if (realTimeList.last) {
		realTimeList.last--;
		for (realTimeList.current = 0; realTimeList.current < realTimeList.last; realTimeList.current++) {
			pid = realTimeList.getMember(realTimeList.current);
			p = (ProcessRealTime *)getProcess(pid);
			flags = p->intFlags;
			if (flags & 2) {
				if (reentrantException) {
					if (p->isSafeInterrupt()) {
						currentExceptionPid = pid;
						p->realTimeCycle();
						currentExceptionPid = 0;
					}
				} else if (processBlank) {
					if ((p->getPriority() == currentPriority || p->getPriority() == 31) && !p->isSafeInterrupt()) {
						savedCurrent = currentProcess;
						currentProcess = pid;
						currentInterruptPid = pid;
						p->realTimeCycle();
						currentInterruptPid = 0;
						currentProcess = savedCurrent;
					}
				} else if (p->isSafeInterrupt()) {
					asm pushf
					asm cli
					currentInterruptPid = pid;
					p->realTimeCycle();
					currentInterruptPid = 0;
					asm popf
				} else if (runAll && (p->getPriority() == currentPriority || p->getPriority() == 31)) {
					savedCurrent = currentProcess;
					currentProcess = pid;
					currentInterruptPid = pid;
					p->realTimeCycle();
					currentInterruptPid = 0;
					currentProcess = savedCurrent;
				}
			}
		}
	} else
		halt(__FILE__, 2507);	// __LINE__
	if (reentrantException) {
		realTimeList.changed = savedFlag;
		realTimeList.current = savedIndex;
		realTimeList.last = savedLast;
	}
done:
	;
}

uint8_t Kernel::isRealInterrupt(void)
{
	if (!inInterrupt)
		halt(__FILE__, 2526);	// __LINE__
	if (reentrantException) {
		if (DosIsRealIntr(borlandExceptionRegsPtr))
			return 1;
	} else if (DosIsRealIntr(borlandInterruptRegsPtr))
		return 1;
	return 0;
}

REGS_BINT *Kernel::getRegsStructure(void)
{
	if (reentrantException)
		return borlandExceptionRegsPtr;
	return borlandInterruptRegsPtr;
}

void Kernel::addToRealTime(uint16_t pid, uint8_t start)
{
	ProcessRealTime *p = (ProcessRealTime *)processList[pid];

	if (!p->isInterrupt() || !p->isRealTime())
		halt(__FILE__, 2593);	// __LINE__
	realTimeOff();
	p->flags |= PROC_ONREALTIME;
	if (p->isSafeInterrupt())
		totalSafe++;
	else
		totalNonSafe++;
	realTimeList.addToList(pid);
	calculateTimerRate();
	if (easyAtomicFlag) {
		realTimeOn();
		return;
	}
	if (start)
		enableRealTimeInterrupts();
}

void Kernel::removeFromRealTime(uint16_t pid)
{
	ProcessRealTime *p = (ProcessRealTime *)processList[pid];

	realTimeOff();
	if (p->isSafeInterrupt())
		totalSafe--;
	else {
		totalNonSafe--;
		if (totalNonSafe == 0)
			interruptPending = 0;
	}
	realTimeList.removeFromList(pid);
	if (realTimeList.isAllocated()) {
		calculateTimerRate();
		realTimeOn();
		return;
	}
	interruptPending = 0;
	disableRealTimeInterrupts();
}

void Kernel::seizeRealTimeVector(void)
{
	if (timerVectorChanged) {
		halt(__FILE__, 2671);	// __LINE__
		return;
	}
	Spanky::dosSetPassToProtVec(8, realTimeInterruptHandler, &origProt, &origReal);
	timerVectorChanged = 1;
}

void Kernel::releaseRealTimeVector(void)
{
	PIHANDLER prot;
	REALPTR real;

	if (timerVectorChanged) {
		Spanky::dosSetRealProtVec(8, origProt, origReal, &prot, &real);
		timerVectorChanged = 0;
	}
}

void Kernel::enableRealTimeInterrupts(void)
{
	if (inInterrupt) {
		wantRealTime = 1;
		intStatusChanged = 1;
		return;
	}
	if (realTimeList.isAllocated()) {
		asm pushf
		asm cli
		realTimeEnabled = 1;
		enableInterrupt(0);
		asm popf
	}
}

void Kernel::disableRealTimeInterrupts(void)
{
	if (inInterrupt) {
		wantRealTime = 0;
		intStatusChanged = 1;
		return;
	}
	asm pushf
	asm cli
	disableInterrupt(0);
	realTimeEnabled = 0;
	asm popf
}

// The PIT runs at the rate of the most frequent real-time process.
void Kernel::calculateTimerRate(void)
{
	uint16_t pid;
	ProcessRealTime *p;
	uint16_t i;
	uint16_t last;
	uint32_t period;
	uint32_t shortest;

	asm pushf
	asm cli
	if (realTimeList.isAllocated()) {
		last = realTimeList.count - 1;
		for (i = 0, shortest = 1000000L; i < last; i++) {
			pid = realTimeList.getMember(i);
			p = (ProcessRealTime *)getProcess(pid);
			if ((period = p->period) < shortest)
				shortest = period;
		}
		setPitPeriod(shortest);
	}
	asm popf
}

void Kernel::execute(uint32_t ticks, void (*idle)(void))
{
	for (;;) {
		theCurrentTick = systemTimer->ticks;
		doProcesses();
		if (idle)
			idle();
		while (theCurrentTick + ticks > systemTimer->ticks)
			;
	}
}

void Kernel::realTimeOn(void)
{
	if (easyAtomicFlag)
		enableInterrupt(0);
}

void Kernel::realTimeOff(void)
{
	easyAtomicFlag = realTimeEnabled;
	if (realTimeEnabled)
		disableInterrupt(0);
}

// Moves onto the kernel's own stack: the exception stack when it interrupts
// itself.
void interrupt Kernel::realTimeInterruptHandler(REGS_BINT regs)
{
	if (reentrancyFlag) {
		reentrantException = 1;
		borlandExceptionRegsPtr = &regs;
		oldExceptionStackSeg = _SS;
		oldExceptionStackOff = _SP;
		_AX = exceptionStackSegment;
		_SS = _AX;
		_SP = exceptionStackSize + exceptionStackOffset;
	} else {
		reentrancyFlag = 1;
		borlandInterruptRegsPtr = &regs;
		oldIntStackSeg = _SS;
		oldIntStackOff = _SP;
		_AX = interruptStackSegment;
		_SS = _AX;
		_SP = interruptStackSize + interruptStackOffset;
	}
	if (processBlank) {
		if (reentrantException)
			sendEoi(0);
	} else
		sendEoi(0);
	interruptHandlerStage2();
	if (reentrantException) {
		reentrantException = 0;
		_SS = oldExceptionStackSeg;
		_SP = oldExceptionStackOff;
		return;
	}
	reentrancyFlag = 0;
	_SS = oldIntStackSeg;
	_SP = oldIntStackOff;
}

void Kernel::interruptHandlerStage2(void)
{
	uint16_t safe;

	if (reentrantException) {
		disableInterrupt(0);
		realTimeExecutor();
		enableInterrupt(0);
		borlandExceptionRegsPtr = 0;
		return;
	}
	safe = totalSafe;
	intStatusChanged = 0;
	if (!safe)
		disableInterrupt(0);
	inInterrupt = 1;
	asm sti
	asm cld
	realTimeExecutor();
	asm cli
	inInterrupt = 0;
	borlandInterruptRegsPtr = 0;
	if (intStatusChanged) {
		if (wantRealTime) {
			enableRealTimeInterrupts();
			return;
		}
		disableRealTimeInterrupts();
		return;
	}
	if (!safe)
		enableInterrupt(0);
}

void Kernel::checkIntegrity(void)
{
	uint16_t i;

	for (i = 0; i < maxProcesses; i++)
		if (isProcess(processList[i]) && processList[i]->pid != i)
			halt(__FILE__, 3401);	// __LINE__
}

// One bit per item ref: set while a process belongs to it.
void Kernel::setProcessRefActive(uint16_t ref)
{
	uint8_t bits;
	uint8_t bit;
	uint16_t index;

	index = ref >> 3;
	bits = quickRefProcessArray[index];
	bit = ref & 7;
	bits |= 1 << bit;
	quickRefProcessArray[index] = bits;
}

void Kernel::clrProcessRefActive(uint16_t ref)
{
	uint8_t bits;
	uint8_t bit;
	uint16_t index;

	index = ref >> 3;
	bits = quickRefProcessArray[index];
	bit = ref & 7;
	bits &= ~(1 << bit);
	quickRefProcessArray[index] = bits;
}

void Kernel::checkProcessRefActive(uint16_t ref)
{
	Process *p;

	p = findValidProcess(ref, PT_ANY);
	if (p)
		setProcessRefActive(ref);
	else
		clrProcessRefActive(ref);
}

uint8_t Kernel::isProcessRefActive(uint16_t ref)
{
	uint8_t bits;
	uint8_t bit;

	bits = quickRefProcessArray[ref >> 3];
	bit = ref & 7;
	if (bits & (1 << bit))
		return 1;
	return 0;
}

// One bit per special process type (PT_FIRST and up) for each item ref.
void Kernel::setProcessTypeActive(uint16_t ref, ProcessType type)
{
	uint8_t *p = quickProcessTypeArray + ref;
	int16_t bit = type - PT_FIRST;

	bit = 1 << bit;
	*p |= bit;
}

void Kernel::clrProcessTypeActive(uint16_t ref, ProcessType type)
{
	uint8_t *p = quickProcessTypeArray + ref;
	int16_t bit = type - PT_FIRST;

	bit = 1 << bit;
	*p &= ~bit;
}

void Kernel::checkProcessTypeActive(uint16_t ref, ProcessType type)
{
	Process *p;

	p = findValidProcess(ref, type);
	if (p)
		setProcessTypeActive(ref, type);
	else
		clrProcessTypeActive(ref, type);
}

uint8_t Kernel::isProcessTypeActive(uint16_t ref, ProcessType type)
{
	uint8_t *p = quickProcessTypeArray + ref;
	int16_t bit = type - PT_FIRST;

	bit = 1 << bit;
	if (*p & bit)
		return 1;
	return 0;
}

void Kernel::clrAccumulator(void)
{
	memset(accumulatorTable, 0, 16384);
}

// Counts how often each process type runs.
void Kernel::checkAccumulator(void)
{
	uint16_t pid = getCurrentProcess();

	if (pid) {
		Process *p = getProcess(pid);
		accumulatorTable[p->processType] = accumulatorTable[p->processType] + 1;
	}
}
