// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: DEXT286\SELMNGR.C

#include "u8port.h"

#include "cdesc.h"
#include "spanker.h"
#include "selmngr.h"

uint16_t SelectorManager::selectorStart = 0;
uint16_t SelectorManager::selectorCount = 0;
uint16_t SelectorManager::currentSelectorCount = 0;
uint16_t SelectorManager::globalSel = 0;

// Free descriptors form a ring: limit holds the previous entry, baseLow the next.
void SelectorManager::init(void)
{
	uint16_t unused = 0;
	uint16_t entries = (LocalDescriptorTable::limit + 1) >> 3;
	uint16_t i;
	uint16_t prev;
	Desc *desc;
	uint16_t last;

	for (i = 0; i < entries; i++) {
		if (LocalDescriptorTable::table[i].access == 0)
			unused++;
		if (unused > 200) {
			selectorStart = i;
			last = i;
			prev = i;
			for (; i < entries; i++) {
				if (LocalDescriptorTable::table[i].access == 0) {
					selectorCount++;
					last = i;
					desc = &LocalDescriptorTable::table[i];
					desc->access = 0x80;
					desc->limit = prev;
					LocalDescriptorTable::table[prev].baseLow = i;
					prev = i;
				}
			}
			LocalDescriptorTable::table[selectorStart].limit = last;
			LocalDescriptorTable::table[last].baseLow = selectorStart;
		}
	}
	currentSelectorCount = selectorCount;
	globalSel = allocateSelector();
	setupDescriptor(globalSel, 0, 0xFFFFFFFFL);
}

void SelectorManager::uninit(void)
{
	uint8_t done = 0;
	uint16_t i = selectorStart;
	uint16_t next;

	while (!done) {
		next = LocalDescriptorTable::table[i].baseLow;
		if (next == selectorStart)
			done = 1;
		LocalDescriptorTable::table[i].access = 0;
		i = next;
	}
}

uint16_t SelectorManager::allocateSelector(void)
{
	Desc *head = &LocalDescriptorTable::table[selectorStart];
	uint16_t i = head->baseLow;

	if (i == selectorStart)
		return 0;
	else {
		Desc *desc = &LocalDescriptorTable::table[i];
		Desc *prev = &LocalDescriptorTable::table[desc->limit];
		Desc *after = &LocalDescriptorTable::table[desc->baseLow];

		prev->baseLow = desc->baseLow;
		after->limit = desc->limit;
		desc->access = 0xF3;
		currentSelectorCount--;
		return (i << 3) | 7;
	}
}

void SelectorManager::deallocateSelector(uint16_t sel)
{
	uint16_t i = sel >> 3;
	Desc *desc = &LocalDescriptorTable::table[i];
	Desc *head = &LocalDescriptorTable::table[selectorStart];
	Desc *next = &LocalDescriptorTable::table[head->baseLow];

	desc->access = 0x80;
	desc->limit = selectorStart;
	desc->baseLow = head->baseLow;
	head->baseLow = i;
	next->limit = i;
	currentSelectorCount++;
}

void SelectorManager::setupDescriptor(uint16_t sel, uint32_t base)
{
	Desc *desc = LocalDescriptorTable::getDesc(sel);

	desc->setBase(base);
}

void SelectorManager::setupDescriptor(uint16_t sel, uint32_t base, uint32_t size)
{
	Desc *desc = LocalDescriptorTable::getDesc(sel);
	uint32_t pages;

	desc->setBase(base);
	if (size > 0x100000L) {
		pages = size >> 12;
		if ((size & 0xFFF) && size <= 0xFFFFEFFFL)
			pages++;
		desc->setLimit(pages);
		desc->limitHigh |= 0x80;
	} else {
		desc->setLimit(size - 1);
		desc->limitHigh &= 0x7F;
	}
}

// Moves every selector based at oldBase to newBase.
void SelectorManager::remapBase(uint32_t oldBase, uint32_t newBase)
{
	Desc *desc;
	uint16_t end = selectorStart + selectorCount;
	uint16_t i;

	for (i = selectorStart; i < end; i++) {
		desc = &LocalDescriptorTable::table[i];
		if (desc->access == 0xF3 && desc->getBase() == oldBase) {
			asm pushf
			asm cli
			if (oldBase == Spanky::originalIdtAddress)
				Spanky::setOriginalIdtAddress(newBase);
			desc->setBase(newBase);
			asm popf
		}
	}
}
