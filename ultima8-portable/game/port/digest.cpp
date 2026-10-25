// The game's state as one number, for the platform's recordings: a replay checks it once a
// second of game time against the recorded one (plat_set_state_digest). It covers the two
// random number generators, every item's place, shape, frame, flags and quality, the
// usecode's global flags and the system timer, which between them move with nearly
// everything the game does. Host pointers are left out: they differ from run to run.
#include "u8port.h"
#include "plat.h"

#include "itemdata.h"
#include "randint.h"
#include "flag.h"
#include "systimer.h"

static uint64_t mix(uint64_t h, const void *p, size_t n)
{
	const uint8_t *b = (const uint8_t *)p;

	for (size_t i = 0; i < n; i++)
		h = (h ^ b[i]) * 0x100000001B3ull;
	return h;
}

template <typename T> static uint64_t mixArray(uint64_t h, const T *a, size_t n)
{
	uint8_t present = a != 0;

	h = mix(h, &present, 1);
	return a ? mix(h, a, n * sizeof *a) : h;
}

static uint64_t stateDigest(void)
{
	uint64_t h = 0xCBF29CE484222325ull;
	uint32_t v;

	v = (uint32_t)GetRandomSeed();
	h = mix(h, &v, sizeof v);
	v = u8_rand_state();
	h = mix(h, &v, sizeof v);
	v = systemTimer ? systemTimer->ticks : 0;
	h = mix(h, &v, sizeof v);
	h = mixArray(h, ItemData::xArray, MAX_ITEMS);
	h = mixArray(h, ItemData::yArray, MAX_ITEMS);
	h = mixArray(h, ItemData::zArray, MAX_ITEMS);
	h = mixArray(h, ItemData::typeArray, MAX_ITEMS);
	h = mixArray(h, ItemData::frameArray, MAX_ITEMS);
	h = mixArray(h, ItemData::statusArray, MAX_ITEMS);
	h = mixArray(h, ItemData::qArray, MAX_ITEMS);
	h = mixArray(h, ItemData::npcNumArray, MAX_ITEMS);
	h = mixArray(h, ItemData::mapNumArray, MAX_ITEMS);
	h = mixArray(h, ItemData::nextArray, MAX_ITEMS);
	h = mixArray(h, (const uint8_t *)GlobalFlag::bits, 0x400);
	return h;
}

// Registered before main, so the first checkpoint has it.
static struct RegisterDigest
{
	RegisterDigest(void) { plat_set_state_digest(stateDigest); }
} registerDigest;
