#ifndef RANDINT_H
#define RANDINT_H

#include "u8port.h"

extern "C" int16_t RandInt(void);
extern "C" int16_t Ultima5Random(int16_t, int16_t);
extern "C" void SetRandomSeed(int32_t);
extern "C" int32_t GetRandomSeed(void);
extern "C" int16_t PseudoRandom(int16_t);

#endif
