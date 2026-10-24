#ifndef APPDEBUG_H
#define APPDEBUG_H

#include "u8port.h"

int16_t howManyFreeHandles(void);
void standardDebug(char *, int16_t);
void initDebug(void);
void applicationGarbageCollect(uint8_t);
void applicationOutOfMemory(uint16_t);

#endif
