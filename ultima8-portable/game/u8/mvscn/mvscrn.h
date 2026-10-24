#ifndef MVSCRN_H
#define MVSCRN_H

#include "u8port.h"

class Vport;

// Scroll a viewport's contents by the given number of pixels.
void MoveScreenUp(Vport *, int16_t);
void MoveScreenDown(Vport *, int16_t);
void MoveScreenLeft(Vport *, int16_t);
void MoveScreenRight(Vport *, int16_t);
void MoveScreenUpLeft(Vport *, int16_t, int16_t);
void MoveScreenUpRight(Vport *, int16_t, int16_t);
void MoveScreenDownLeft(Vport *, int16_t, int16_t);
void MoveScreenDownRight(Vport *, int16_t, int16_t);

void MoveScreen(Vport *, int16_t, int16_t);

#endif
