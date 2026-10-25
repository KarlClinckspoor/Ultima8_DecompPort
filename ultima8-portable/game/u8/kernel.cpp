// flags: -P -2 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: KERNEL.C

#include "u8port.h"

// #include <phapi.h>  (DOS only)
// #include <dos.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include <stdarg.h>
// #include <alloc.h>  (DOS only)
#include "u8process.h"
#include "kernel.h"
#include "chargen.h"
#include "error.h"
#include "u8stdint.h"
#include "systimer.h"
#include "biostime.h"
#include "filespec.h"
#include "flex.h"
#include "cglobvp.h"
#include "interupt.h"
#include "procsave.h"
#include "plat.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Inline in the shared headers when this file was built.
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }
U8_INLINE SharedFile::SharedFile(void) {}
U8_INLINE FlexFile::FlexFile(void) {}
U8_INLINE Index::Index(void) { size = 0; offset = 0; }

void PutPixel(Vport *, int16_t, int16_t, int16_t);
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
// DOS's InDOS flag, which kept real-time processes out of DOS calls. The host has no DOS
// to interrupt.
static uint8_t notInDos = 0;
uint8_t *Kernel::inDosPtr = &notInDos;
void *Kernel::origProt = 0;
uint32_t Kernel::origReal = 0;
void *Kernel::borlandInterruptRegsPtr = 0;
void *Kernel::borlandExceptionRegsPtr = 0;
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

// The DOS build also took real-mode stacks for its interrupt handlers here and found DOS's
// InDOS flag; the host runs real-time processes on the game thread (see pitTick).
void Kernel::init(void)
{
	BiosTimer *timer;

	if (initialized)
		halt(__FILE__, 261);	// __LINE__
	initialized = 1;
	initMemory();
	hertz = 0;
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

	processing = 1;
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
		// The timer may be due; on DOS it interrupted wherever the program was.
		plat_pump();
		if (interruptPending && realTimeEnabled) {
			processBlank = 1;
			realTimeInterruptHandler();
			processBlank = 0;
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
	processList = newList;
	processSize = newSize;
	idStringList = newIds;
	maxProcesses = newMax;
	if (newMax != 100)
		reconfigureCounter = 150;
	else
		reconfigureCounter = 0xffff;
	delete[] oldList;
	delete[] oldSize;
	delete[] oldIds;
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
	processList = newList;
	processSize = newSize;
	idStringList = newIds;
	maxProcesses += 30;
	reconfigureCounter = 150;
	delete[] oldList;
	delete[] oldSize;
	delete[] oldIds;
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
		arg = (uint16_t)va_arg(ap, int);
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
		arg = (uint16_t)va_arg(ap, int);
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
	delete[] processList;
	delete[] processSize;
	delete[] priorityCount;
	processList = 0;
	processSize = 0;
	priorityCount = 0;
	if (realTimeList.isAllocated())
		realTimeList.clear();
	delete[] quickRefProcessArray;
	quickRefProcessArray = 0;
	delete[] quickProcessTypeArray;
	quickProcessTypeArray = 0;
	delete[] accumulatorTable;
	accumulatorTable = 0;
	delete[] idStringList;
	idStringList = 0;
}

// The kernel's statics as the DOS build saved them: the 157 bytes of its data segment from
// processList to magicNumberEnd, at their DOS offsets. Pointers (stale on loading anyway,
// and reallocated by initMemory) are zero. realTimeList lay among them (0x44-0x52): its pids
// go in a record of their own, but its count and whether it had data came back with the
// statics, and List::load reads the pids only then, so those are kept here too (with the
// data pointer as a 0/1 flag).
#define STATICS_SIZE 157

static void putWord(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void putLong(uint8_t *p, uint32_t v) { putWord(p, (uint16_t)v); putWord(p + 2, (uint16_t)(v >> 16)); }
static uint16_t getWord(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t getLong(const uint8_t *p) { return getWord(p) | (uint32_t)getWord(p + 2) << 16; }

void Kernel::packStatics(uint8_t *b)
{
	memset(b, 0, STATICS_SIZE);
	putWord(b + 0x08, reconfigureCounter);
	putWord(b + 0x0a, currentScanSlot);
	putWord(b + 0x0c, currentLinearScanPid);
	putWord(b + 0x0e, maxProcesses);
	putWord(b + 0x10, totalProcesses);
	putWord(b + 0x12, currentProcess);
	putWord(b + 0x14, currentProcessSize);
	b[0x16] = expandOnDemand;
	b[0x17] = processing;
	b[0x18] = initialized;
	b[0x19] = keyboardVectorChanged[0];
	b[0x1a] = keyboardBufferOverflow[0];
	b[0x1b] = restarting;
	putWord(b + 0x1c, numPhantoms);
	putWord(b + 0x1e, searchingRef);
	putWord(b + 0x20, searchingProcessType);
	memcpy(b + 0x22, keyboardBufferHead, 2);
	memcpy(b + 0x24, keyboardBufferTail, 2);
	putLong(b + 0x2a, theCurrentTick);
	b[0x2e] = currentPriority;
	b[0x2f] = abortDoProcesses;
	putWord(b + 0x30, currentPolledIntPid);
	memcpy(b + 0x32, origKeyboardProt, 4);
	memcpy(b + 0x36, origKeyboardReal, 4);
	putWord(b + 0x3e, pitDivisor);
	putLong(b + 0x40, pitPeriod);
	putWord(b + 0x46, realTimeList.count);
	putWord(b + 0x48, realTimeList.linearType);
	putLong(b + 0x4a, realTimeList.isAllocated() ? 1 : 0);
	b[0x4e] = realTimeList.changed;
	putWord(b + 0x4f, realTimeList.current);
	putWord(b + 0x51, realTimeList.last);
	b[0x53] = interruptDebug;
	putWord(b + 0x54, hertz);
	putWord(b + 0x56, totalSafe);
	putWord(b + 0x58, totalNonSafe);
	putWord(b + 0x5e, interruptStackSize);
	putWord(b + 0x64, exceptionStackSize);
	putWord(b + 0x6e, currentInterruptPid);
	putWord(b + 0x70, currentExceptionPid);
	b[0x72] = inInterrupt;
	b[0x73] = interruptPending;
	b[0x74] = easyAtomicFlag;
	b[0x75] = intStatusChanged;
	b[0x76] = realTimeEnabled;
	b[0x77] = timerVectorChanged;
	b[0x78] = wantRealTime;
	b[0x79] = reentrancyFlag;
	b[0x7a] = reentrantException;
	b[0x7b] = processBlank;
	putLong(b + 0x84, origReal);
	b[0x9c] = magicNumberEnd;
}

void Kernel::unpackStatics(const uint8_t *b)
{
	reconfigureCounter = getWord(b + 0x08);
	currentScanSlot = getWord(b + 0x0a);
	currentLinearScanPid = getWord(b + 0x0c);
	maxProcesses = getWord(b + 0x0e);
	totalProcesses = getWord(b + 0x10);
	currentProcess = getWord(b + 0x12);
	currentProcessSize = getWord(b + 0x14);
	expandOnDemand = b[0x16];
	processing = b[0x17];
	initialized = b[0x18];
	keyboardVectorChanged[0] = b[0x19];
	keyboardBufferOverflow[0] = b[0x1a];
	restarting = b[0x1b];
	numPhantoms = getWord(b + 0x1c);
	searchingRef = getWord(b + 0x1e);
	searchingProcessType = getWord(b + 0x20);
	memcpy(keyboardBufferHead, b + 0x22, 2);
	memcpy(keyboardBufferTail, b + 0x24, 2);
	theCurrentTick = getLong(b + 0x2a);
	currentPriority = b[0x2e];
	abortDoProcesses = b[0x2f];
	currentPolledIntPid = getWord(b + 0x30);
	memcpy(origKeyboardProt, b + 0x32, 4);
	memcpy(origKeyboardReal, b + 0x36, 4);
	pitDivisor = getWord(b + 0x3e);
	pitPeriod = getLong(b + 0x40);
	{
		// A stand-in for the stale pointer: List::load replaces it without reading it.
		static uint16_t staleData;

		realTimeList.count = getWord(b + 0x46);
		realTimeList.linearType = getWord(b + 0x48);
		realTimeList.data = getLong(b + 0x4a) ? &staleData : 0;
		realTimeList.changed = b[0x4e];
		realTimeList.current = getWord(b + 0x4f);
		realTimeList.last = getWord(b + 0x51);
	}
	interruptDebug = b[0x53];
	hertz = getWord(b + 0x54);
	totalSafe = getWord(b + 0x56);
	totalNonSafe = getWord(b + 0x58);
	interruptStackSize = getWord(b + 0x5e);
	exceptionStackSize = getWord(b + 0x64);
	currentInterruptPid = getWord(b + 0x6e);
	currentExceptionPid = getWord(b + 0x70);
	inInterrupt = b[0x72];
	interruptPending = b[0x73];
	easyAtomicFlag = b[0x74];
	intStatusChanged = b[0x75];
	realTimeEnabled = b[0x76];
	timerVectorChanged = b[0x77];
	wantRealTime = b[0x78];
	reentrancyFlag = b[0x79];
	reentrantException = b[0x7a];
	processBlank = b[0x7b];
	magicNumberEnd = b[0x9c];
}

// kernel.dat, a flex file:
//   record 0      "Defines=... Id=..." text
//   record 1      the kernel's statics (see packStatics)
//   record 2      realTimeList
//   records 3-7   processSize, quickRefProcessArray, quickProcessTypeArray, priorityCount,
//                 idStringList
//   from 10       two per process slot: the process object, then what its save() wrote
// The DOS build saved each process object as its bytes, vtable pointer and all; a near
// vtable pointer was the same in every run of U8.EXE. On the host each object record starts
// with its class's name, and loading rebuilds the object through that class's copy
// constructor (game/u8/procreg.cpp), which sets its vtable pointers and its members'.
// idStringList held DGROUP offsets; the names themselves are saved now, one after another,
// each ended by a 0 (an empty one for a slot without a name).
void Kernel::load(char *name)
{
	uint16_t i;
	int16_t record = 0;
	Process *p;
	FileSpec spec("", name, "kernel", "dat");

	if (inInterrupt || !initialized)
		halt(__FILE__, 1770);	// __LINE__
	if (spec.exists()) {
		Process *timer;
		uint8_t found;
		uint8_t statics[STATICS_SIZE];
		Index index;

		disableRealTimeInterrupts();
		setPitDivisor(0);
		releaseRealTimeVector();
		enableInterrupt(0);
		freeMemory();
		FlexFile file(spec, ReadOnly, -1);
		record++;

		// The kernel's statics come back in one record; the stacks, InDOS flag and timer
		// vectors of this session (gone on the host) are not among them.
		file.readRecord(record++, statics);
		unpackStatics(statics);

		initMemory();
		file.seekRecord(record++);
		realTimeList.load(&file);
		file.readRecord(record++, processSize);
		file.readRecord(record++, quickRefProcessArray);
		file.readRecord(record++, quickProcessTypeArray);
		file.readRecord(record++, priorityCount);
		{
			// The names stay in this block until the next load replaces them all.
			static char *names = 0;
			Index namesIndex;
			uint32_t at = 0;

			delete[] names;
			file.getIndex(record, namesIndex);
			names = new char[namesIndex.size + 1];
			file.readRecord(record++, names);
			names[namesIndex.size] = 0;
			for (i = 0; i < maxProcesses; i++) {
				idStringList[i] = at < namesIndex.size && names[at] ? names + at : 0;
				while (at < namesIndex.size && names[at])
					at++;
				at++;
			}
		}

		record = 10;
		for (i = 0; i < maxProcesses; i++) {
			if (processSize[i] == 0) {
				record += 2;
				continue;
			}
			file.getIndex(record, index);
			char *saved = new char[index.size];
			file.readRecord(record++, saved);
			p = RestoreProcess(saved, index.size);
			delete[] saved;
			if (!p)
				halt(__FILE__, 1930);	// __LINE__
			processList[i] = p;
			processSize[i] = ProcessClassSize(p);
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

	{
		uint8_t statics[STATICS_SIZE];
		packStatics(statics);
		file.writeRecord(record++, statics, STATICS_SIZE);
	}
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
	{
		uint32_t size = 0;
		char *names;

		for (i = 0; i < maxProcesses; i++)
			size += (idStringList[i] ? strlen(idStringList[i]) : 0) + 1;
		names = new char[size];
		size = 0;
		for (i = 0; i < maxProcesses; i++) {
			const char *name = idStringList[i] ? idStringList[i] : "";

			strcpy(names + size, name);
			size += strlen(name) + 1;
		}
		file.writeRecord(record++, names, size);
		delete[] names;
	}

	record = 10;
	for (i = 0; i < maxProcesses; i++) {
		p = processList[i];
		if (isProcess(p) && p->isSaved()) {
			uint16_t size;
			char *saved = SaveProcess(p, size);
			file.writeRecord(record++, saved, size);
			delete[] saved;
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

// The PIT's channel 0 divisor (1193182 Hz / divisor). The host's timer takes the period in
// microseconds instead (setPitPeriod); 0, the BIOS rate, is 54925 us.
void Kernel::setPitDivisor(uint16_t divisor)
{
	uint32_t period = divisor ? (uint32_t)((uint64_t)divisor * 1000000u / 1193182u) : 54925u;

	plat_pit_set(timerVectorChanged ? pitTick : 0, period);
}

void Kernel::setPitPeriod(uint32_t microseconds)
{
	hertz = 1000000L / microseconds;
	pitPeriod = microseconds;
	if (microseconds <= 54920L)
		microseconds = microseconds * 10000 / 8380;
	else
		halt(__FILE__, 2293);	// __LINE__
	pitDivisor = microseconds;
	// The divisor is what the PIT got; on the host the period itself sets the rate.
	plat_pit_set(timerVectorChanged ? pitTick : 0, pitPeriod);
}

// IRQ 0 on the host: plat_pump calls this every PIT period. On DOS the handler ran whenever
// the timer interrupted, unless the game had masked IRQ 0.
void Kernel::pitTick(void)
{
	if (timerVectorChanged && !isInterruptMasked(0))
		realTimeInterruptHandler();
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
					currentInterruptPid = pid;
					p->realTimeCycle();
					currentInterruptPid = 0;
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

// Whether the interrupt came from real mode (Phar Lap could tell); never, on the host.
uint8_t Kernel::isRealInterrupt(void)
{
	if (!inInterrupt)
		halt(__FILE__, 2526);	// __LINE__
	return 0;
}

void *Kernel::getRegsStructure(void)
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

// Takes the timer interrupt (IRQ 0, vector 8) for the kernel; the BIOS handler it replaced
// went on running through BiosTimer.
void Kernel::seizeRealTimeVector(void)
{
	if (timerVectorChanged) {
		halt(__FILE__, 2671);	// __LINE__
		return;
	}
	timerVectorChanged = 1;
	plat_pit_set(pitTick, pitPeriod ? pitPeriod : 54925u);
}

void Kernel::releaseRealTimeVector(void)
{
	if (timerVectorChanged) {
		plat_pit_set(0, 0);
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
		realTimeEnabled = 1;
		enableInterrupt(0);
	}
}

void Kernel::disableRealTimeInterrupts(void)
{
	if (inInterrupt) {
		wantRealTime = 0;
		intStatusChanged = 1;
		return;
	}
	disableInterrupt(0);
	realTimeEnabled = 0;
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
}

void Kernel::execute(uint32_t ticks, void (*idle)(void))
{
	for (;;) {
		theCurrentTick = systemTimer->ticks;
		doProcesses();
		if (idle)
			idle();
		while (theCurrentTick + ticks > systemTimer->ticks)
			plat_yield();
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

// The timer interrupt's handler. On DOS it moved onto the kernel's own stack (the exception
// stack when it interrupted itself) and acknowledged the PIC. On the host it is called from
// plat_pump, which never runs inside itself, so it does not interrupt itself either; the
// reentrancy flags are kept as they were.
void Kernel::realTimeInterruptHandler(void)
{
	if (reentrancyFlag)
		reentrantException = 1;
	else
		reentrancyFlag = 1;
	interruptHandlerStage2();
	if (reentrantException) {
		reentrantException = 0;
		return;
	}
	reentrancyFlag = 0;
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
	realTimeExecutor();
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
