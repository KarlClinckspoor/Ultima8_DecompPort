#ifndef CYTABLE_H
#define CYTABLE_H

#include "u8port.h"

// Row start offsets of a screen buffer, `step` bytes apart.
class GraphicYtable
{
public:
	uint16_t *index;
	uint8_t f_04;

	GraphicYtable(void);
	void setType(uint8_t);
	uint8_t verify_ytable(int16_t, int16_t);
	uint8_t init_ytable(int16_t, int16_t, uint16_t);
	int16_t getindex(int16_t);
	void setindex(int16_t y, uint16_t offset) { index[y] = offset; }
	void free(void)
	{
		if (index) {
			delete index;
			index = 0;
		}
	}
};

#endif
