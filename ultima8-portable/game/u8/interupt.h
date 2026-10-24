#ifndef INTERUPT_H
#define INTERUPT_H

#include "u8port.h"

// The 8259 PIC and DOS interrupt vector helpers.
extern "C" void setInterrupt(char, int16_t, int16_t);
extern "C" void *getInterrupt(char);
extern "C" void disableInterrupt(char);
extern "C" void enableInterrupt(char);
extern "C" void sendEoi(char);

#endif
