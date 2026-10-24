#ifndef IWRITSKP_H
#define IWRITSKP_H

#include "u8port.h"

class Vport;

// Builds a skip table: a header, one offset per row (from the entry
// itself), then for each row
// alternating skip and run counts, each followed by the run's pixels.
class FastSkipMaker
{
public:
	uint8_t *start;
	int16_t *rowOffset;
	uint8_t *out;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;

	FastSkipMaker(uint8_t *, int16_t, int16_t, int16_t, int16_t);
	int16_t skip_to(int16_t, int16_t);
	int16_t add_run(int16_t, uint8_t *);
	uint16_t measure_run(int16_t);
	int16_t add_skip(int16_t);
	uint16_t measure_skip(int16_t);
	uint16_t measure_skip_to(int16_t, int16_t);
};

// Walks a rectangle of a Vport, finding runs of pixels that are not the
// skip colour.
class ShapeScanner
{
public:
	int16_t left;
	int16_t top;
	int16_t right;
	int16_t bottom;
	uint8_t *pos;
	int16_t lineSkip;
	int16_t x;
	int16_t y;
	int16_t runLength;
	uint8_t skipColor;

	ShapeScanner(Vport &, int16_t, int16_t, int16_t, int16_t, int16_t);
	uint8_t find_segment(void);
};

#endif
