// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: DOSFIX32\SPANKER.C

#include "u8port.h"

#pragma inline

#include <stdio.h>
#include <string.h>
// #include <alloc.h>  (DOS only)
// #include <dos.h>  (DOS only)
// #include <phapi.h>  (DOS only)
#include "cexit.h"
#include "cfile.h"
#include "selmngr.h"
#include "descript.h"
#include "spanker.h"

#define INTERRUPTS 256
#define IDT_SIZE (INTERRUPTS * sizeof(Descriptor))
#define CSIP_BUFFER_SIZE 60000U
#define PROFILE_SLOTS 16000

#define INTERRUPT_GATE 6
#define CALL_GATE 4

uint32_t *Spanky::csIpPtrArray = 0;
uint32_t *Spanky::csIpPtrRetrievalArray = 0;
uint16_t Spanky::numCsIpPtrs = 0;
uint16_t Spanky::numCsIpRetrievalPtrs = 0;
uint16_t Spanky::currentCsIpRetrievalPtr = 0;
uint8_t Spanky::csIpPtrOverflow = 0;
uint8_t Spanky::csIpPtrRetrievalOverflow = 0;
uint8_t Spanky::profiling = 0;
uint32_t *Spanky::profilePtr = 0;
int32_t Spanky::nextProfileSlot = 0;
uint8_t Spanky::profilerInitialized = 0;
uint8_t Spanky::intercepting = 0;
uint8_t Spanky::initialized = 0;
Descriptor *Spanky::interceptorBuffer;
Descriptor *Spanky::origBuffer;
void *Spanky::intJumpTable;
void *Spanky::patchPointer;
uint16_t Spanky::setRegsCodeSeg;
uint16_t Spanky::setRegsOrigCodeOff;
uint16_t Spanky::setRegsNewCodeOff;
uint32_t Spanky::originalIdtAddress;
uint32_t Spanky::newIdtAddress;
void (*setOrigIdtrCallGate)(void);
void (*setNewIdtrCallGate)(void);

inline uint8_t BaseFile::is_valid(void)
{
	return handle != -1;
}

inline BaseFile::~BaseFile(void)
{
	if (is_valid())
		close();
}

void FREESPANKER(uint16_t)
{
	Spanky::uninit();
	DosExitList(EXLST_EXIT, 0);
}

void initSpanky(void)
{
	Spanky::init();
}

void uninitSpanky(void)
{
	Spanky::uninit();
}

void Spanky::init(void)
{
	Descriptor gate;
	int16_t i;
	Descriptor *newIdt;
	uint16_t newIdtPara;
	uint16_t origPara;
	uint16_t interceptorPara;
	uint16_t newIdtSel;
	uint16_t origSel;
	uint16_t interceptorSel;
	FullSelector patchSel;
	FullSelector patchAlias;

	if (initialized)
		halt(__FILE__, 121);	// __LINE__
	DescMngr::init();
	if (DosAllocRealSeg(IDT_SIZE, &newIdtPara, &newIdtSel))
		halt(__FILE__, 126);	// __LINE__
	newIdt = (Descriptor *)MK_FP(newIdtSel, 0);
	if (DosAllocRealSeg(IDT_SIZE, &origPara, &origSel))
		halt(__FILE__, 131);	// __LINE__
	origBuffer = (Descriptor *)MK_FP(origSel, 0);
	if (newIdt == 0 || origBuffer == 0)
		halt(__FILE__, 139);	// __LINE__
	if (DosAllocRealSeg(IDT_SIZE, &interceptorPara, &interceptorSel))
		halt(__FILE__, 144);	// __LINE__
	interceptorBuffer = (Descriptor *)MK_FP(interceptorSel, 0);
	if (interceptorBuffer == 0)
		halt(__FILE__, 151);	// __LINE__
	csIpPtrArray = (uint32_t *)new char[CSIP_BUFFER_SIZE];
	csIpPtrRetrievalArray = (uint32_t *)new char[CSIP_BUFFER_SIZE];
	if (csIpPtrArray == 0 || csIpPtrRetrievalArray == 0)
		halt(__FILE__, 161);	// __LINE__
	getIntJumpTable();
	getPatchPosition();

	// The jump table is code; patch it through a data alias.
	patchSel.sel = FP_SEG(patchPointer);
	if (!DescMngr::dosCreateDSAlias(patchSel, patchAlias))
		halt(__FILE__, 178);	// __LINE__
	patchPointer = MK_FP(patchAlias.sel, FP_OFF(patchPointer));

	// Each hooked vector enters the jump table at its own 3-byte stub.
	gate.setGate(FP_SEG(intJumpTable), INTERRUPT_GATE);
	for (i = 0; i < INTERRUPTS; i++) {
		origBuffer[i] = DescMngr::getIntDescriptor(i);
		if (i == 5 || !(i < 8 || i == 0x0D || (i >= 0x29 && i <= 0x2E)
				|| (i >= 0x30 && i <= 0x32) || (i >= 0x34 && i <= 0x40)
				|| (i >= 0x42 && i <= 0x45) || (i >= 0x47 && i <= 0x49)
				|| (i >= 0x4B && i <= 0x5B) || (i >= 0x5D && i <= 0x66)
				|| (i >= 0x68 && i <= 0x6F) || i >= 0x78)) {
			gate.setOffset(FP_OFF(intJumpTable) + i * 3);
			newIdt[i] = gate;
		} else
			newIdt[i] = DescMngr::getIntDescriptor(i);
	}
	initIdtGate();

	FullSelector origSeg;
	origSeg.sel = FP_SEG(origBuffer);
	gate = DescMngr::getLocalDescriptor(origSeg.index());
	originalIdtAddress = gate.getBase();
	originalIdtAddress += FP_OFF(origBuffer);
	newIdtAddress = DescMngr::idtr.base;
	for (i = 0; i < INTERRUPTS; i++)
		interceptorBuffer[i] = newIdt[i];
	asm pushf
	asm cli
	intercepting = 1;
	for (i = 0; i < INTERRUPTS; i++)
		DescMngr::setIntDescriptor(i, newIdt[i]);
	asm popf
	DosFreeSeg(FP_SEG(newIdt));
	initialized = 1;
	DosExitList(EXLST_ADD, FREESPANKER);
}

// Call gates into the code that switches the IDT register.
void Spanky::initIdtGate(void)
{
	Descriptor d;
	uint32_t gatePtr;
	FullSelector sel;

	getSetRegsCodeInfo();
	setRegsCodeSeg = setRegsCodeSeg & 0xFFFC;
	setRegsCodeSeg |= 0;	// ring 0
	d = DescMngr::getLocalDescriptor(setRegsCodeSeg >> 3);
	d.seg.dpl = 0;
	DescMngr::setLocalDescriptor(setRegsCodeSeg >> 3, d);

	sel.sel = SelectorManager::allocateSelector();
	if (sel.sel == 0)
		halt(__FILE__, 317);	// __LINE__
	d.setGate(setRegsCodeSeg, CALL_GATE);
	d.setOffset(setRegsOrigCodeOff);
	DescMngr::setLocalDescriptor(sel.index(), d);
	gatePtr = sel.sel;
	gatePtr <<= 16;
	setOrigIdtrCallGate = (void (*)(void))gatePtr;

	sel.sel = SelectorManager::allocateSelector();
	if (sel.sel == 0)
		halt(__FILE__, 339);	// __LINE__
	d.setOffset(setRegsNewCodeOff);
	DescMngr::setLocalDescriptor(sel.index(), d);
	gatePtr = sel.sel;
	gatePtr <<= 16;
	setNewIdtrCallGate = (void (*)(void))gatePtr;
}

void Spanky::uninit(void)
{
	int16_t i;

	if (initialized) {
		asm pushf
		asm cli
		initialized = 0;
		for (i = 0; i < INTERRUPTS; i++)
			DescMngr::setIntDescriptor(i, origBuffer[i]);
		asm popf
		setIdtrToIntercept();
		DosFreeSeg(FP_SEG(origBuffer));
		origBuffer = 0;
		DosFreeSeg(FP_SEG(interceptorBuffer));
		interceptorBuffer = 0;
	}
}

// Installs a handler while keeping our hook in the IDT.
void Spanky::dosSetPassToProtVec(int16_t intNo, PIHANDLER handler, PIHANDLER *oldHandler, REALPTR *oldReal)
{
	Descriptor hook;
	Descriptor installed;

	asm pushf
	asm cli
	hook = DescMngr::getIntDescriptor(intNo);
	DosSetPassToProtVec(intNo, handler, oldHandler, oldReal);
	installed = DescMngr::getIntDescriptor(intNo);
	DescMngr::setIntDescriptor(intNo, hook);
	origBuffer[intNo] = installed;
	asm popf
}

void Spanky::dosSetRealProtVec(int16_t intNo, PIHANDLER handler, REALPTR realHandler, PIHANDLER *oldHandler, REALPTR *oldReal)
{
	Descriptor hook;
	Descriptor installed;

	asm pushf
	asm cli
	hook = DescMngr::getIntDescriptor(intNo);
	DosSetRealProtVec(intNo, handler, realHandler, oldHandler, oldReal);
	installed = DescMngr::getIntDescriptor(intNo);
	DescMngr::setIntDescriptor(intNo, hook);
	origBuffer[intNo] = installed;
	asm popf
}

void Spanky::setIdtrToOriginal(void)
{
	(*setOrigIdtrCallGate)();
}

void Spanky::setIdtrToIntercept(void)
{
	(*setNewIdtrCallGate)();
}

// Lists the interrupts whose vectors changed since init.
uint8_t Spanky::isIntact(void)
{
	FILE *f;
	uint8_t intact = 1;
	int16_t i;

	f = fopen("interupt.dat", "w+b");
	if (f == 0)
		halt(__FILE__, 457);	// __LINE__
	fprintf(f, "The following is a list of interrupts that have been altered since Spanky\r\n");
	fprintf(f, "was initialized:\r\n\n");
	for (i = 0; i < INTERRUPTS; i++) {
		Descriptor d = DescMngr::getIntDescriptor(i);

		if (d != interceptorBuffer[i]) {
			intact = 0;
			fprintf(f, "Interrupt #%2x.\n", i);
		}
	}
	if (intact)
		fprintf(f, "All interrupt vectors are intact.\n");
	fclose(f);
	return intact;
}

// Moves the cs:ip samples taken so far to the retrieval buffer.
void Spanky::bufferProfileData(void)
{
	uint8_t wasProfiling;

	if (numCsIpPtrs) {
		wasProfiling = profiling;
		profiling = 0;
		numCsIpRetrievalPtrs = numCsIpPtrs;
		csIpPtrRetrievalOverflow = csIpPtrOverflow;
		currentCsIpRetrievalPtr = 0;
		memcpy(csIpPtrRetrievalArray, csIpPtrArray, numCsIpPtrs * 4);
		numCsIpPtrs = 0;
		csIpPtrOverflow = 0;
		if (wasProfiling)
			profiling = 1;
	} else
		halt(__FILE__, 509);	// __LINE__
}

int32_t Spanky::getNextCsIp(void)
{
	if (numCsIpRetrievalPtrs) {
		numCsIpRetrievalPtrs--;
		return csIpPtrRetrievalArray[currentCsIpRetrievalPtr++];
	}
	halt(__FILE__, 521);	// __LINE__
}

void Spanky::setOriginalIdtAddress(uint32_t address)
{
	originalIdtAddress = address;
}

// Copies new samples into the profile until it is full, then saves it.
void Spanky::checkProfiler(void)
{
	uint8_t wasProfiling;
	void *stack;

	if (profiling && haveCsIps()) {
		wasProfiling = profiling;
		if (wasProfiling)
			profiling = 0;
		bufferProfileData();
		if (!profilerInitialized) {
			stack = MK_FP(_SS, 0);
			profilePtr[nextProfileSlot] = (uint32_t)stack;
			nextProfileSlot++;
			profilerInitialized = 1;
		}
		while (numCsIpRetrievalPtrs && nextProfileSlot < PROFILE_SLOTS) {
			profilePtr[nextProfileSlot] = getNextCsIp();
			nextProfileSlot++;
		}
		if (wasProfiling) {
			profiling = 1;
			if (nextProfileSlot == PROFILE_SLOTS)
				toggleProfiler();
		}
	}
}

// Starts profiling, or stops it and writes the samples to fprofile.dat.
void Spanky::toggleProfiler(void)
{
	if (profiling) {
		uint32_t csIp;
		int32_t slot;
		BaseFile file;
		char line[82];

		profiling = 0;
		bufferProfileData();
		file.create("fprofile.dat", ReadWrite);
		if (!file.is_valid())
			halt(__FILE__, 603);	// __LINE__
		for (slot = 0; slot < nextProfileSlot; slot++) {
			csIp = profilePtr[slot];
			sprintf(line, "%4x:%4x\r\n", FP_SEG((void *)csIp), FP_OFF((void *)csIp));
			file.write(line, strlen(line));
		}
	} else {
		nextProfileSlot = 0;
		profilerInitialized = 0;
		if (profilePtr == 0) {
			profilePtr = (uint32_t *)farmalloc(PROFILE_SLOTS * 4L);
			if (profilePtr == 0)
				halt(__FILE__, 624);	// __LINE__
		}
		if (haveCsIps()) {
			bufferProfileData();
			while (numCsIpRetrievalPtrs)
				getNextCsIp();
		}
		profiling = 1;
	}
}
