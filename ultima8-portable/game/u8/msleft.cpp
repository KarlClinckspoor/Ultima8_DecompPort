// name: MSLEFT
// Ported from MSLEFT.ASM.
// Scrolls a viewport's picture to the left.

#include "u8port.h"

#include "cvport.h"
#include "mvscrn.h"
#include "segmem.h"

// void MoveScreenLeft(Vport *vp, int dx)
// Moves the picture dx pixels left, row by row from the top.
// Rows are taken to be one viewport width apart, starting at the first row offset.
void MoveScreenLeft(Vport *vp, int16_t xShift)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t rows = vp->rect.f_06 - vp->rect.f_02 + 1;

	// di = top row, si = xShift pixels into it.
	uint16_t di = vp->graphicYtable.index[0];
	uint16_t si = di + xShift;
	uint16_t bytes = width - xShift;	// per row

	// After each row both offsets are xShift short of the next one. The row count is 16
	// bits: a height of 0 counts down through 65536.
	do
	{
		SegMovs(vp->mem, di, vp->mem, si, bytes & 1, 1, false);
		SegMovs(vp->mem, di, vp->mem, si, bytes >> 1, 2, false);
		si += xShift;
		di += xShift;
	} while (--rows != 0);
}
