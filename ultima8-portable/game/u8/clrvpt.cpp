// name: CLRVPT
// Ported from CLRVPT.ASM.
// Fills a viewport's whole clip rectangle with one colour.

#include "u8port.h"

#include "cvport.h"
#include "segmem.h"

// void ClearViewport(Vport far *vp, unsigned char color)
// Each row is filled from the left clip edge, words first and then the odd byte. The row
// count is 16 bits: a clip rectangle of height 0 does nothing, a negative one counts down
// through 65536.
void ClearViewport(Vport *vp, uint8_t color)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t left = vp->rect.f_00;
	uint16_t rows = vp->rect.f_06 - vp->rect.f_02 + 1;
	if (rows == 0)
		return;

	// The row table is walked from the top clip line.
	uint16_t *row = vp->graphicYtable.index + vp->rect.f_02;
	uint16_t value = color | color << 8;
	do
	{
		uint16_t di = *row + left;
		SegStos(vp->mem, di, value, width >> 1, 2);
		SegStos(vp->mem, di, value, width & 1, 1);
		row++;
	} while (--rows != 0);
}
