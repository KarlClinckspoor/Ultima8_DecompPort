#ifndef SPANKER_H
#define SPANKER_H

#include "u8port.h"

// The DOS build hooked every interrupt through a jump table here, so 32-bit registers
// survived the extender's handlers, and sampled cs:ip on each timer tick for a profiler. The
// host has no interrupts to hook; the profiler switch stays and samples nothing.
class Spanky
{
public:
	static void init(void);
	static void uninit(void);
	static void checkProfiler(void);
	static void toggleProfiler(void);
	static uint8_t profiling;
};

#endif
