// flags: -P -2 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: SYSTIMER.C

#include "u8port.h"

// #include <phapi.h>  (DOS only)
#include "cfile.h"
#include "kernel.h"
#include "systimer.h"
#include "error.h"

SystemTimer *systemTimer = 0;

// Counts 60 ticks a second.
SystemTimer::SystemTimer(void)
{
	Kernel::setIdString(pid, "SystemTimer");
	ticks = 0;
	if (systemTimer)
		halt(__FILE__, 29);	// __LINE__
	else
		systemTimer = this;
	setPriority(31);
	setHertz(60);
	intFlags |= 2;
	setProcessType((ProcessType)4);
	setSafeInterrupt();
	flags |= PROC_SUSPENDED;
	setDaemon();
	connectToRealTime(1);
}

void SystemTimer::interruptHandler(uint32_t, uint32_t)
{
	ticks++;
}

void SystemTimer::freeMemory(void)
{
	systemTimer = 0;
}

void SystemTimer::load(BaseFile *file)
{
	Process::load(file);
	systemTimer = this;
}

void SystemTimer::wait(uint32_t count)
{
	uint32_t start = ticks;

	while (start + count > ticks)
		;
}

void SystemTimer::process(void)
{
}
