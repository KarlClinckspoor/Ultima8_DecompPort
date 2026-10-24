// name: MSUL
// Ported from MSUL.ASM.
// Scrolls a viewport's picture up and to the left.

#include "u8port.h"

#include "cvport.h"
#include "mvscrn.h"
#include "segmem.h"

// void MoveScreenUpLeft(Vport *vp, int dx, int dy)
// Moves the picture dx pixels left and dy pixels up, working from the top row down.
// Rows are taken to be one viewport width apart, starting at the first row offset.
void MoveScreenUpLeft(Vport *vp, int16_t xShift, int16_t yShift)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t height = vp->rect.f_06 - vp->rect.f_02 + 1;

	// di = top row, si = yShift rows below it, xShift pixels in.
	uint16_t di = vp->graphicYtable.index[0];
	uint16_t si = di + Mul16(yShift, width) + xShift;
	uint16_t rows = height - yShift;
	uint16_t bytes = width - xShift;	// per row

	// After each row both offsets are xShift short of the next one. The row count is 16
	// bits: 0 counts down through 65536.
	do
	{
		SegMovs(vp->mem, di, vp->mem, si, bytes & 1, 1, false);
		SegMovs(vp->mem, di, vp->mem, si, bytes >> 1, 2, false);
		si += xShift;
		di += xShift;
	} while (--rows != 0);
}
