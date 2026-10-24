#ifndef SELMNGR_H
#define SELMNGR_H

#include "u8port.h"

// Hands out LDT selectors from a ring of descriptors free at startup.
class SelectorManager
{
public:
	static void init(void);
	static void uninit(void);
	static uint16_t allocateSelector(void);
	static void deallocateSelector(uint16_t);
	static void setupDescriptor(uint16_t, uint32_t);
	static void setupDescriptor(uint16_t, uint32_t, uint32_t);
	static void remapBase(uint32_t, uint32_t);
	static uint16_t selectorStart;
	static uint16_t selectorCount;
	static uint16_t currentSelectorCount;
	static uint16_t globalSel;
};

#endif
