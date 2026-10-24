#ifndef PRISM_H
#define PRISM_H

#include "u8port.h"

// A box in world space from corner (x, y, z) to corner (x2, y2, z2).
class Prism
{
public:
	int16_t x;
	int16_t y;
	uint8_t z;
	int16_t x2;
	int16_t y2;
	uint8_t z2;

	void set(int16_t, int16_t, uint8_t, int16_t, int16_t, uint8_t);
	uint8_t intersects(Prism *);
	void moveRel(int16_t, int16_t, int16_t);
};

#endif
