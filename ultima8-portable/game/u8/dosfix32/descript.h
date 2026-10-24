#ifndef DESCRIPT_H
#define DESCRIPT_H

#include "u8port.h"

// A selector: table index, table bit (4: local) and privilege level.
class FullSelector
{
public:
	uint16_t sel;

	int16_t isGlobal(void) { return !(sel & 4); }
	uint16_t index(void) { return sel >> 3; }
};

struct SegmentFields
{
	uint16_t limitLow : 16;
	uint16_t baseLow : 16;
	uint16_t baseMid : 8;
	uint16_t type : 4;
	uint16_t system : 1;
	uint16_t dpl : 2;
	uint16_t present : 1;
	uint16_t limitHigh : 4;
	uint16_t available : 1;
	uint16_t reserved : 1;
	uint16_t big : 1;
	uint16_t granular : 1;
	uint16_t baseHigh : 8;
};

struct GateFields
{
	uint16_t offsetLow;
	uint16_t selector;
	uint16_t count : 5;
	uint16_t reserved : 3;
	uint16_t type : 4;
	uint16_t system : 1;
	uint16_t dpl : 2;
	uint16_t present : 1;
	uint16_t offsetHigh;
};

// An 8-byte segment or gate descriptor.
class Descriptor
{
public:
	union {
		SegmentFields seg;
		GateFields gate;
	};

	Descriptor(void);
	uint8_t operator==(Descriptor &);
	uint8_t operator!=(Descriptor &d) { return !(*this == d); }
	void setBase(uint32_t);
	void setLimit(uint32_t);
	uint32_t getBase(void);
	uint32_t getLimit(void);
	void setOffset(uint32_t);
	uint32_t getOffset(void);
	// A present, ring 3 gate into code segment sel.
	void setGate(uint16_t sel, uint16_t gateType)
	{
		gate.selector = sel;
		gate.count = 0;
		gate.reserved = 0;
		gate.type = gateType;
		gate.system = 0;
		gate.dpl = 3;
		gate.present = 1;
	}
};

// What sgdt and sidt store.
struct TableRegister
{
	uint16_t limit;
	uint32_t base;
};

class DescMngr
{
public:
	static void init(void);
	static Descriptor getDescriptor(FullSelector);
	static void setDescriptor(FullSelector, Descriptor);
	static Descriptor getGlobalDescriptor(uint16_t);
	static Descriptor getLocalDescriptor(uint16_t);
	static Descriptor getIntDescriptor(uint16_t);
	static void setGlobalDescriptor(uint16_t, Descriptor &);
	static void setLocalDescriptor(uint16_t, Descriptor &);
	static void setIntDescriptor(uint16_t, Descriptor &);
	static uint8_t dosCreateDSAlias(FullSelector, FullSelector &);
	static Descriptor *globalDescriptorTable;
	static Descriptor *localDescriptorTable;
	static Descriptor *interruptDescriptorTable;
	static TableRegister gdtr;
	static TableRegister idtr;
	static uint16_t ldtr;
	static TableRegister simLdtr;
	static uint16_t gdtSel;
	static uint16_t ldtSel;
	static uint16_t idtSel;
};

extern "C" void getRegisters(TableRegister *gdtr, uint16_t *ldtr, TableRegister *idtr);

#endif
