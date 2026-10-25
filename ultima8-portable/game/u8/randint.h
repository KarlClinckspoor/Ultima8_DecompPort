#ifndef RANDINT_H
#define RANDINT_H

#include "u8port.h"

int16_t RandInt(void);
int16_t Ultima5Random(int16_t, int16_t);
void SetRandomSeed(int32_t);
int32_t GetRandomSeed(void);
int16_t PseudoRandom(int16_t);

#endif
