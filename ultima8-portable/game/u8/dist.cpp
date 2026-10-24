// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: DIST.C

#include "u8port.h"

#include <stdlib.h>
#include "dist.h"
#include "scratchm.h"

uint8_t signToDirTable[9] = {7, 6, 5, 0, 8, 4, 1, 2, 3};
char x8add[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
char y8add[10] = {-1, -1, 0, 1, 1, 1, 0, -1, 0, 0};

// Chessboard distance between two map points.
int16_t dist(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
	int16_t dx, dy;

	dx = abs(x1 - x2);
	dy = abs(y1 - y2);
	return dx > dy ? dx : dy;
}

int16_t dist(int16_t x1, int16_t y1, uint8_t z1, int16_t x2, int16_t y2, uint8_t z2)
{
	int16_t dx, dy, dz, d;

	dx = abs(x1 - x2);
	dy = abs(y1 - y2);
	dz = abs(z1 - z2);
	d = dx > dy ? dx : dy;
	d = d > dz ? d : dz;
	return d;
}

// One of eight directions, 0 being north and counting clockwise.
char getDirCoordToCoord(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	int16_t dx = x1 - x2;
	int16_t dy = y1 - y2;
	int16_t adx = abs(dx);
	int16_t ady = abs(dy);

	if (adx / 2 <= ady && adx * 2 >= ady)
	{
		if (dx > 0 && dy > 0)
			return 7;
		if (dx < 0 && dy < 0)
			return 3;
		if (dx > 0 && dy < 0)
			return 5;
		return 1;
	}
	if (adx * 2 <= ady)
	{
		if (dy >= 0)
			return 0;
		return 4;
	}
	if (dx >= 0)
		return 6;
	return 2;
}
