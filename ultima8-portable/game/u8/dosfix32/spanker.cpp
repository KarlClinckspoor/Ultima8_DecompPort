// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: DOSFIX32\SPANKER.C

#include "u8port.h"

#include "spanker.h"

// See spanker.h: the interrupt interceptor and its profiler have nothing to do on the host.

uint8_t Spanky::profiling = 0;

void Spanky::init(void)
{
}

void Spanky::uninit(void)
{
}

void Spanky::checkProfiler(void)
{
}

void Spanky::toggleProfiler(void)
{
	profiling = !profiling;
}
