// RANDINT.ASM: the game's random numbers.
//
// A 32-bit generator, seed = seed * 10003h + 1, of which each call gives the new high word.
// Its seed is saved with the game (the avatar's randomSeed), so the sequence has to be this
// one exactly.
#include "u8port.h"
#include "randint.h"

static uint32_t seed = 0x0BAD0BADUL;

int16_t RandInt(void)
{
	seed = seed * 0x10003UL + 1;
	return (int16_t)(seed >> 16);
}

// A number from low to high, either way round (unsigned). Over the whole 16-bit range the
// assembly skipped the division and so always gave the lower bound; kept.
int16_t Ultima5Random(int16_t a, int16_t b)
{
	uint16_t r = (uint16_t)RandInt();
	uint16_t low = (uint16_t)a;
	uint16_t high = (uint16_t)b;
	uint16_t range;

	if (high < low)
	{
		uint16_t t = high;
		high = low;
		low = t;
	}
	range = (uint16_t)(high - low + 1);
	return (int16_t)(low + (range ? r % range : 0));
}

// The low bit is always set.
void SetRandomSeed(int32_t s)
{
	seed = (uint32_t)s | 1;
}

int32_t GetRandomSeed(void)
{
	return (int32_t)seed;
}

// 0 to n - 1 (unsigned); 0 for 0.
int16_t PseudoRandom(int16_t n)
{
	uint16_t r = (uint16_t)RandInt();

	return (int16_t)(n ? r % (uint16_t)n : 0);
}
