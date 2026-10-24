#ifndef FWRITSKP_H
#define FWRITSKP_H

#include "u8port.h"

class Vport;

uint16_t EstimateFastWriteSkip(Vport &, int16_t, int16_t, int16_t, int16_t, int16_t);
int16_t FastWriteSkip(Vport &, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint8_t *, int16_t);

#endif
