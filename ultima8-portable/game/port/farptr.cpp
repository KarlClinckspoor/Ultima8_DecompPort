/* Usecode's 32-bit far pointers; see farptr.h. */
#include "u8port.h"

#include "farptr.h"

typedef struct {
	char *base;
	uint32_t size;
	int32_t active;	/* index in active[], or -1 */
} block;

static block blocks[0x10000];
/* The selectors in use, for finding the block that holds a pointer. */
static uint16_t active[0x10000];
static uint32_t activeCount;
static uint16_t lastHit;
static uint16_t nextDynamic = FAR_DYNAMIC;
static bool started;

static void start(void)
{
	if (started)
		return;
	started = true;
	for (uint32_t i = 0; i < 0x10000; i++)
		blocks[i].active = -1;
}

void u8_far_unregister(uint16_t selector)
{
	block *b = &blocks[selector];
	int32_t at;

	start();
	at = b->active;
	if (at < 0)
		return;
	/* the last active selector takes its place in the list */
	active[at] = active[--activeCount];
	blocks[active[at]].active = at;
	b->active = -1;
	b->base = NULL;
	b->size = 0;
	if (lastHit == selector)
		lastHit = 0;
}

void u8_far_register(uint16_t selector, void *base, uint32_t size)
{
	block *b;

	start();
	if (selector == 0)
		return;
	u8_far_unregister(selector);
	b = &blocks[selector];
	b->base = (char *)base;
	b->size = size > 0x10000 ? 0x10000 : size;
	b->active = (int32_t)activeCount;
	active[activeCount++] = selector;
}

static bool holds(uint16_t selector, const char *p)
{
	const block *b = &blocks[selector];

	return b->base != NULL && p >= b->base && (uintptr_t)(p - b->base) < b->size;
}

uint32_t u8_far_from_ptr(const void *pointer)
{
	const char *p = (const char *)pointer;
	uint16_t selector;

	start();
	if (p == NULL)
		return 0;
	if (lastHit && holds(lastHit, p))
		return (uint32_t)lastHit << 16 | (uint32_t)(p - blocks[lastHit].base);
	/* The fixed blocks first, so a pointer into a stack is never given a dynamic name. */
	for (int pass = 0; pass < 2; pass++) {
		for (uint32_t i = 0; i < activeCount; i++) {
			selector = active[i];
			if ((pass == 0) == (selector < FAR_DYNAMIC) && holds(selector, p)) {
				lastHit = selector;
				return (uint32_t)selector << 16 | (uint32_t)(p - blocks[selector].base);
			}
		}
	}
	/* Something else (an intrinsic's own data): a 64K window of its own, oldest reused. */
	selector = nextDynamic;
	nextDynamic = nextDynamic == 0xFFFF ? FAR_DYNAMIC : (uint16_t)(nextDynamic + 1);
	u8_far_register(selector, (void *)p, 0x10000);
	lastHit = selector;
	return (uint32_t)selector << 16;
}

void *u8_ptr_from_far(uint32_t far)
{
	uint16_t selector = (uint16_t)(far >> 16);
	const block *b;

	start();
	b = &blocks[selector];
	if (far == 0 || b->base == NULL)
		return NULL;
	return b->base + (far & 0xFFFF);
}
