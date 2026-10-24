#ifndef URANDOM_H
#define URANDOM_H

#include "u8port.h"

#include "randint.h"

int16_t urandom(int16_t);
void urandomize(void);
int16_t rndRange(int16_t, int16_t);
uint8_t OneIn(int16_t);

#endif
