#ifndef HARDWARE_H
#define HARDWARE_H

#include "u8port.h"

#include "u8process.h"
#include "procmon.h"

enum CPU : int16_t
{
	CPU_0,
	CPU_8086 = 0,
	CPU_286 = 1,
	CPU_386 = 2
};

// The machine's minimum requirements, checked at startup.
class HardwareChecker
{
public:
	int32_t minDiskSpace;
	int32_t minMemory;
	int32_t minLargestFragment;
	uint8_t pad_0c[2];
	CPU minCpu;

	HardwareChecker(int32_t, int32_t, int32_t, CPU);
	int32_t getHardDriveSpace(int16_t);
	int32_t getTotalMemory(void);
	int32_t getLargestMemoryFragment(void);
	int16_t getCpu(void);
	uint8_t mouseInstalled(void);
	uint8_t verifyAll(uint8_t);
};

#endif
