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

void BiosTimer::setTimerVector(uint32_t real, void *prot)
{
	realVector = real;
	protVector = prot;
}

// DOS: called the BIOS's own timer handler, which kept the time of day and the floppy motor.
// The host keeps its own time.
void BiosTimer::interruptHandler(uint32_t, uint32_t)
{
}

void BiosTimer::process(void)
{
}
