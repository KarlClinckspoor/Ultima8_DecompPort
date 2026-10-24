// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: PROCMON.C

#include "u8port.h"

#include "kernel.h"
#include "uprocess.h"
#include "cprotmem.h"
#include "sndmgr.h"
#include "chargen.h"
#include "wait.h"
#include "hardware.h"

ProcessMonitor::ProcessMonitor(void)
{
	Kernel::setIdString(pid, "Monitor");
	setProcessType((ProcessType)0x225);
	setSafeInterrupt();
	flags |= PROC_SUSPENDED;
	setDaemon();
	new Timer;
	setHertz(60);
	setIntFlags(intFlags | 2);
}

void ProcessMonitor::process(void)
{
	uint32_t total, largest, shapeMemory;
	uint16_t selectors;
	Timer *timer = (Timer *)Kernel::findValidProcess(0, (ProcessType)0x201);

	ProtMemoryManager::getMemStatus(total, largest, selectors);
	shapeMemory = ProtMemoryManager::fragments[0].length;
	charGen.makeString(250, 0,
		"P=%-2d\nFPS=%-2d\nT=%-2d.%-2d\nUnkMem=%ld\nSndMem=%ld\nFree sels=%u\nTot=%lu\nLar=%lu\nShape=%lu",
		Kernel::totalProcesses, fps <= 60 ? fps : 60, timer->seconds, timer->tenths,
		RoutineIndex::unkMemoryUsed, SoundManager->memoryUsed, selectors, total, largest, shapeMemory);
	if (ticks >= 60)
	{
		ticks = 0;
		fps = frames;
		frames = 0;
	}
	frames++;
}

void ProcessMonitor::interruptHandler(uint32_t event, uint32_t)
{
	switch (event)
	{
	case 2:
		ticks++;
		break;
	}
}

void ProcessMonitor::turnOn(void)
{
	ticks = 0;
	fps = 0;
	flags &= ~PROC_SUSPENDED;
	connectToRealTime(1);
}

void ProcessMonitor::turnOff(void)
{
	flags |= PROC_SUSPENDED;
	disconnectFromRealTime();
}
