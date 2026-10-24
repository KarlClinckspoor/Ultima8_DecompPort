/* Usecode's 32-bit far pointers.
 *
 * Usecode keeps pointers on its 200-byte stacks as 4-byte values: the "this" data of a
 * process, return addresses into a unit's code, strings in the YAMM heap, local variables
 * passed to intrinsics. On DOS they were far pointers, a selector and an offset. On the host
 * they stay 4 bytes, built the same way: the high word names a registered block of memory,
 * the low word is the offset into it (every such block is under 64K).
 *
 * The selectors of the blocks usecode keeps pointers into are fixed, so the pointers in a
 * saved process's stack still work after loading:
 *
 *   FAR_YAMM                 the YAMM string and list heap
 *   FAR_STACK + pid          a usecode process's stack (its Routine)
 *   FAR_CODE + unit          a unit's code, while it is cached
 *   FAR_DYNAMIC and up       anything else, given a selector the first time it is seen
 */
#ifndef FARPTR_H
#define FARPTR_H

#include "u8port.h"

#define FAR_YAMM 0x0008
#define FAR_STACK 0x1000
#define FAR_CODE 0x4000
#define FAR_DYNAMIC 0x8000

/* Names a block; replaces whatever the selector named before. */
void u8_far_register(uint16_t selector, void *base, uint32_t size);
void u8_far_unregister(uint16_t selector);
/* The far pointer for p; 0 for NULL. A pointer outside every registered block gets a dynamic
 * selector of its own, based at p. */
uint32_t u8_far_from_ptr(const void *p);
/* The host pointer for a far pointer; NULL for 0 or an unknown selector. */
void *u8_ptr_from_far(uint32_t far);

/* Reading and writing them in memory (usecode stacks are byte-packed). */
static inline uint32_t u8_far_load(const void *at)
{
	uint32_t v;
	memcpy(&v, at, 4);
	return v;
}

static inline void u8_far_store(void *at, uint32_t v)
{
	memcpy(at, &v, 4);
}

#endif
