#ifndef MVSCRN_H
#define MVSCRN_H

#include "u8port.h"

class Vport;

// Scroll a viewport's contents by the given number of pixels.
extern "C" void MoveScreenUp(Vport *, int16_t);
extern "C" void MoveScreenDown(Vport *, int16_t);
extern "C" void MoveScreenLeft(Vport *, int16_t);
extern "C" void MoveScreenRight(Vport *, int16_t);
extern "C" void MoveScreenUpLeft(Vport *, int16_t, int16_t);
extern "C" void MoveScreenUpRight(Vport *, int16_t, int16_t);
extern "C" void MoveScreenDownLeft(Vport *, int16_t, int16_t);
extern "C" void MoveScreenDownRight(Vport *, int16_t, int16_t);

void MoveScreen(Vport *, int16_t, int16_t);

#endif
