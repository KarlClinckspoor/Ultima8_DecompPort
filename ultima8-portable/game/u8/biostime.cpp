// flags: -P -2 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: BIOSTIME.C

#include "u8port.h"

// #include <phapi.h>  (DOS only)
#include "kernel.h"
#include "biostime.h"

// Passes the PC timer tick on to the BIOS handler it replaced.
BiosTimer::BiosTimer(void)
{
	realVector = 0;
	protVector = 0;
	Kernel::setIdString(pid, "BiosTimer");
	setPriority(31);
	setPeriod(54920L);
	intFlags |= 2;
	setProcessType((ProcessType)3);
	setSafeInterrupt();
	flags |= PROC_SUSPENDED;
	setDaemon();
}

void BiosTimer::setTimerVector(uint32_t real, PIHANDLER prot)
{
	realVector = real;
	protVector = prot;
}

void BiosTimer::interruptHandler(uint32_t, uint32_t)
{
	uint16_t flags;

	asm pushf
	asm cld
	asm pushf
	asm pop flags
	DosRealFarCall(realVector, 0, 0, -1, flags);
	asm popf
}

void BiosTimer::process(void)
{
}
