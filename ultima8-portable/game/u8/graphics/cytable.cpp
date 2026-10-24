// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CYTABLE.C

#include "u8port.h"

#include "cytable.h"

uint8_t GraphicYtable::verify_ytable(int16_t step, int16_t count)
{
	int16_t width = step;
	uint16_t n = count;
	uint16_t i;

	if (index == 0)
		return 0;
	for (i = 0; i < n; i++)
		if (index[i] != index[0] + i * width)
			return 0;
	return 1;
}

uint8_t GraphicYtable::init_ytable(int16_t step, int16_t count, uint16_t start)
{
	uint32_t n = (uint16_t)count;
	uint32_t width = (uint16_t)step;
	uint16_t i;

	if (index)
		free();
	if ((index = new uint16_t[n]) == 0)
		return 0;
	if (index) {
		for (i = 0; i < n; i++)
			setindex(i, start + i * width);
		return 1;
	}
	return 0;
}
