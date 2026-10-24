#ifndef WRITSKP_H
#define WRITSKP_H

#include "u8port.h"

#include "iwritskp.h"

class Vport;

// Bit 0 of the flags word in the table header: runs may be RLE coded.
#define SKIP_RLE	1

// A skip table that can share identical rows and RLE code its runs.
class SkipMaker : public FastSkipMaker
{
public:
	int16_t *firstRow;
	uint8_t *blankRow;

	SkipMaker(uint8_t *, int16_t, int16_t, int16_t, int16_t, uint16_t);
	int16_t skip_to(int16_t, int16_t);
	uint8_t segment_can_rle(int16_t, uint8_t *);
	int16_t add_non_rle_run(int16_t, uint8_t *);
	uint16_t measure_non_rle_run(int16_t);
	int16_t add_rle_run(int16_t, uint8_t *);
	uint16_t measure_rle_run(int16_t, uint8_t *);
	void optimize(void);
};

uint16_t EstimateWriteSkip(Vport &, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t);
int16_t WriteSkip(Vport &, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint8_t *, int16_t, uint16_t);

#endif
