// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: EVENT\KTIMER.C

#include "u8port.h"

#include "ktimer.h"

// Inline in the shared headers when this file was built.
U8_INLINE void nameProcess(uint16_t pid, char *name) { ((char **)Kernel::idStringList)[pid] = name; }

AilTimer *ailTimer = 0;

// 54920 microseconds: the PC's default 18.2 Hz tick.
AilTimer::AilTimer(void)
{
	setPeriod(54920L);
	intFlags = 2;
	setProcessType(0x100);
	nameProcess(pid, "AilTimer");
	setSafeInterrupt();
	flags |= 1;
	setDaemon();
}

void AilTimer::interruptHandler(uint32_t, uint32_t)
{
	API_timer();
}

void hook_timer_process(void)
{
	ailTimer->connectToRealTime(1);
}

void unhook_timer_process(void)
{
	ailTimer->disconnectFromRealTime();
}

// Called from assembly (AIL16), so the DOS build kept every register.
void set_PIT_period(int16_t period)
{
	ailTimer->setPeriod((uint16_t)period);
}

void AilTimer::process(void)
{
}
