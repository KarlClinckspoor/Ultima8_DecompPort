// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: PRISM.C

#include "u8port.h"

#include "prism.h"

void Prism::set(int16_t x, int16_t y, uint8_t z, int16_t x2, int16_t y2, uint8_t z2)
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->x2 = x2;
	this->y2 = y2;
	this->z2 = z2;
}

uint8_t Prism::intersects(Prism *p)
{
	if (((x >= p->x && p->x2 >= x) || (p->x >= x && x2 >= p->x)) &&
	    ((y >= p->y && y <= p->y2) || (p->y >= y && p->y <= y2)) &&
	    ((z >= p->z && z <= p->z2) || (p->z >= z && p->z <= z2)))
		return 1;
	if (((x >= p->x && p->x2 >= x) || (p->x >= x && p->x2 <= x2)) &&
	    ((y >= p->y && y <= p->y2) || (p->y >= y && p->y2 <= y2)) &&
	    ((z >= p->z && z <= p->z2) || (p->z >= z && p->z2 <= z2)))
		return 1;
	return 0;
}

void Prism::moveRel(int16_t dx, int16_t dy, int16_t dz)
{
	x += dx;
	y += dy;
	z += dz;
	x2 += dx;
	y2 += dy;
	z2 += dz;
}
