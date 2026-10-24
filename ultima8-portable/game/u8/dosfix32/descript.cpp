// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: DOSFIX32\DESCRIPT.C

#include "u8port.h"

#include <string.h>
// #include <dos.h>  (DOS only)
// #include <phapi.h>  (DOS only)
#include "cexit.h"
#include "selmngr.h"
#include "descript.h"

Descriptor *DescMngr::globalDescriptorTable = 0;
Descriptor *DescMngr::localDescriptorTable = 0;
Descriptor *DescMngr::interruptDescriptorTable = 0;
TableRegister DescMngr::gdtr;
TableRegister DescMngr::idtr;
uint16_t DescMngr::ldtr;
TableRegister DescMngr::simLdtr;
uint16_t DescMngr::gdtSel;
uint16_t DescMngr::ldtSel;
uint16_t DescMngr::idtSel;

Descriptor::Descriptor(void)
{
	memset(this, 0, sizeof(Descriptor));
}

uint8_t Descriptor::operator==(Descriptor &d)
{
	char *a = (char *)this;
	char *b = (char *)&d;
	int16_t i;

	for (i = 0; i < sizeof(Descriptor); i++)
		if (a[i] != b[i])
			return 0;
	return 1;
}

void Descriptor::setBase(uint32_t base)
{
	seg.baseLow = base;
	seg.baseMid = (base >> 16) & 0xFF;
	seg.baseHigh = base >> 24;
}

void Descriptor::setLimit(uint32_t limit)
{
	seg.limitLow = limit & 0xFFFF;
	seg.limitHigh = limit >> 16;
}

uint32_t Descriptor::getBase(void)
{
	uint32_t base;

	base = seg.baseHigh;
	base <<= 8;
	base |= seg.baseMid;
	base <<= 16;
	base |= seg.baseLow;
	return base;
}

uint32_t Descriptor::getLimit(void)
{
	uint32_t limit;

	limit = seg.limitHigh;
	limit <<= 16;
	limit |= seg.limitLow;
	return limit;
}

void Descriptor::setOffset(uint32_t offset)
{
	gate.offsetLow = offset & 0xFFFF;
	gate.offsetHigh = offset >> 16;
}

uint32_t Descriptor::getOffset(void)
{
	uint32_t offset;

	offset = gate.offsetHigh;
	offset <<= 16;
	offset |= gate.offsetLow;
	return offset;
}

void DescMngr::init(void)
{
	Descriptor d;

	getRegisters(&gdtr, &ldtr, &idtr);
	if (DosMapLinSeg(gdtr.base, gdtr.limit + 1, &gdtSel))
		halt(__FILE__, 230);	// __LINE__
	if (DosMapLinSeg(idtr.base, idtr.limit + 1, &idtSel))
		halt(__FILE__, 233);	// __LINE__
	globalDescriptorTable = (Descriptor *)MK_FP(gdtSel, 0);
	interruptDescriptorTable = (Descriptor *)MK_FP(idtSel, 0);
	d = getGlobalDescriptor(ldtr >> 3);
	simLdtr.base = d.getBase();
	simLdtr.limit = d.getLimit();
	if (DosMapLinSeg(simLdtr.base, simLdtr.limit + 1, &ldtSel))
		halt(__FILE__, 244);	// __LINE__
	localDescriptorTable = (Descriptor *)MK_FP(ldtSel, 0);
}

Descriptor DescMngr::getDescriptor(FullSelector sel)
{
	if (sel.isGlobal())
		return getGlobalDescriptor(sel.index());
	return getLocalDescriptor(sel.index());
}

void DescMngr::setDescriptor(FullSelector sel, Descriptor d)
{
	if (sel.isGlobal())
		setGlobalDescriptor(sel.index(), d);
	else
		setLocalDescriptor(sel.index(), d);
}

Descriptor DescMngr::getGlobalDescriptor(uint16_t index)
{
	if (index >= (uint16_t)(gdtr.limit + 1) >> 3)
		halt(__FILE__, 268);	// __LINE__
	return globalDescriptorTable[index];
}

Descriptor DescMngr::getLocalDescriptor(uint16_t index)
{
	if (index >= (uint16_t)(simLdtr.limit + 1) >> 3)
		halt(__FILE__, 276);	// __LINE__
	return localDescriptorTable[index];
}

Descriptor DescMngr::getIntDescriptor(uint16_t index)
{
	if (index >= (uint16_t)(idtr.limit + 1) >> 3)
		halt(__FILE__, 284);	// __LINE__
	return interruptDescriptorTable[index];
}

void DescMngr::setGlobalDescriptor(uint16_t index, Descriptor &d)
{
	if (index >= (uint16_t)(gdtr.limit + 1) >> 3)
		halt(__FILE__, 292);	// __LINE__
	asm pushf
	asm cli
	globalDescriptorTable[index] = d;
	asm popf
}

void DescMngr::setLocalDescriptor(uint16_t index, Descriptor &d)
{
	if (index >= (uint16_t)(simLdtr.limit + 1) >> 3)
		halt(__FILE__, 303);	// __LINE__
	asm pushf
	asm cli
	localDescriptorTable[index] = d;
	asm popf
}

void DescMngr::setIntDescriptor(uint16_t index, Descriptor &d)
{
	if (index >= (uint16_t)(idtr.limit + 1) >> 3)
		halt(__FILE__, 314);	// __LINE__
	asm pushf
	asm cli
	interruptDescriptorTable[index] = d;
	asm popf
}

// Makes alias a data selector for the same memory as code selector src.
uint8_t DescMngr::dosCreateDSAlias(FullSelector src, FullSelector &alias)
{
	Descriptor d;

	if ((alias.sel = SelectorManager::allocateSelector()) == 0)
		halt(__FILE__, 331);	// __LINE__
	d = getLocalDescriptor(src.index());
	d.seg.type = 3;
	setLocalDescriptor(alias.index(), d);
	return 1;
}
