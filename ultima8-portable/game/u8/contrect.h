#ifndef CONTRECT_H
#define CONTRECT_H

#include "u8port.h"

#include "crect.h"

// Where a container gump draws its contents: top left and size.
class ContGumpRect
{
public:
	int16_t x;
	int16_t y;
	Point size;

	ContGumpRect(void) {}
	ContGumpRect(int16_t x, int16_t y, int16_t w, int16_t h)
		{ this->x = x; this->y = y; size.f_00 = w; size.f_02 = h; }
};

int32_t getCurrentTimerTick(void);

extern ContGumpRect contGumpRect[20];

#endif
