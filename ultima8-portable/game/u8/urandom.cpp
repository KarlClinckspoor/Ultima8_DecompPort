// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: URANDOM.C

#include "u8port.h"

#include "statrest.h"

// 0 to n - 1.
int16_t urandom(int16_t n)
{
	return PseudoRandom(n);
}

void urandomize(void)
{
	SetRandomSeed(0x12345678L);
}

// low to high inclusive, either way round.
int16_t rndRange(int16_t low, int16_t high)
{
	if (low <= high)
		return urandom(high - low + 1) + low;
	return urandom(low - high + 1) + high;
}

// True one time in n.
uint8_t OneIn(int16_t n)
{
	return urandom(n) == 0;
}
