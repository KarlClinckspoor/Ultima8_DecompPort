// flags: -P -3 -O -Ob -Oe -Og -Oi -Om -Ov -Z -G -k- -d
// name: DEXT286\CPROTMEM.C

#include "u8port.h"

#include "cprotmem.h"

// See cprotmem.h. The DOS build also replaced malloc, free, farmalloc and farfree with these;
// the host's are used as they are.

uint16_t FRAGMENT_0_PERCENTAGE = 45;
uint8_t ProtMemoryManager::initialized = 1;
uint8_t ProtMemoryManager::fromLargest = 0;
uint8_t ProtMemoryManager::failedLastRequest = 0;
Fragment ProtMemoryManager::fragments[MAX_FRAGMENTS] = {{0, PORT_MEMORY_SIZE, 0, 1}};
uint16_t ProtMemoryManager::numFragments = 1;

void *ProtMemoryManager::allocate(uint32_t size, uint16_t, uint8_t)
{
	void *p = malloc(size ? size : 1);

	failedLastRequest = p == 0;
	return p;
}

void ProtMemoryManager::free(void *block, uint16_t)
{
	::free(block);
}

void ProtMemoryManager::getMemStatus(int16_t, uint32_t &total, uint32_t &largest)
{
	total = largest = PORT_MEMORY_SIZE;
}

void ProtMemoryManager::getMemStatus(uint32_t &total, uint32_t &largest, uint16_t &selectors)
{
	total = largest = PORT_MEMORY_SIZE;
	selectors = 0x1000;
}
