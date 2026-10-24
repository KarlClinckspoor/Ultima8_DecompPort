#ifndef INTERUPT_H
#define INTERUPT_H

#include "u8port.h"

// The 8259 PIC's mask, as INTERUPT.ASM drove it: an interrupt the game masked is not
// delivered. Only IRQ 0 (the timer, which runs the kernel's real-time processes from
// plat_pump) is still delivered on the host.
void disableInterrupt(char);
void enableInterrupt(char);
// The end-of-interrupt the handlers sent; nothing to acknowledge on the host.
void sendEoi(char);
// Nonzero while the game has this IRQ masked.
uint8_t isInterruptMasked(char);

#endif
