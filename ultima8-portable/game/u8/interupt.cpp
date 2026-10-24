// name: INTERUPT
// Ported from INTERUPT.ASM.
// Interrupt vector and 8259 PIC helpers. The vector calls (setInterrupt, getInterrupt) had
// no callers left in U8.EXE and are gone; the mask is kept as the PIC kept it.

#include "u8port.h"

#include "interupt.h"

static uint16_t picMask = 0;

void disableInterrupt(char irq)
{
	picMask |= (uint16_t)(1 << (irq & 15));
}

void enableInterrupt(char irq)
{
	picMask &= (uint16_t)~(1 << (irq & 15));
}

void sendEoi(char)
{
}

uint8_t isInterruptMasked(char irq)
{
	return (picMask >> (irq & 15)) & 1;
}
