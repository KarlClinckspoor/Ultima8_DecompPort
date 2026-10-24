// name: CPYVPT
// Ported from CPYVPT.ASM.
// Copies one viewport's pixels into another.

#include "u8port.h"

#include "cvport.h"
#include "segmem.h"

// void CopyViewport(Vport far *dest, Vport far *src)
// Copies the overlap of both clip sizes, from each one's top left corner. Both sides
// start at src's top row.
void CopyViewport(Vport *dest, Vport *src)
{
	// Size: the smaller of the two clip rectangles (signed compares). Only a zero source
	// width and a zero final height give up early.
	int16_t wide = src->rect.f_04 - src->rect.f_00 + 1;
	int16_t srcX = src->rect.f_00;
	if (wide == 0)
		return;
	int16_t high = src->rect.f_06 - src->rect.f_02 + 1;
	int16_t destX = dest->rect.f_00;
	int16_t n = dest->rect.f_04 - dest->rect.f_00 + 1;
	if (n < wide)
		wide = n;
	n = dest->rect.f_06 - dest->rect.f_02 + 1;
	if (n < high)
		high = n;
	if (high == 0)
		return;

	// Both row tables are entered at src's top clip line.
	uint16_t *destRow = dest->graphicYtable.index + src->rect.f_02;
	uint16_t *srcRow = src->graphicYtable.index + src->rect.f_02;
	uint16_t rows = high;
	uint16_t count = wide;
	do
	{
		uint16_t di = *destRow + destX;
		uint16_t si = *srcRow + srcX;
		SegMovs(dest->mem, di, src->mem, si, count >> 1, 2, false);
		SegMovs(dest->mem, di, src->mem, si, count & 1, 1, false);
		destRow++;
		srcRow++;
	} while (--rows != 0);
}
