// name: MSDL
// Ported from MSDL.ASM.
// Scrolls a viewport's picture down and to the left.

#include "u8port.h"

#include "cvport.h"
#include "mvscrn.h"
#include "segmem.h"

// void MoveScreenDownLeft(Vport *vp, int dx, int dy)
// Moves the picture dx pixels left and dy pixels down, working from the bottom row up.
// Rows are taken to be one viewport width apart, starting at the first row offset.
void MoveScreenDownLeft(Vport *vp, int16_t xShift, int16_t yShift)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t height = vp->rect.f_06 - vp->rect.f_02 + 1;

	// di = start of the bottom row, si = yShift rows above it, xShift pixels in.
	uint16_t di = vp->graphicYtable.index[0] + Mul16(width, height) - width;
	uint16_t si = di - Mul16(yShift, width) + xShift;
	uint16_t rows = height - yShift;
	uint16_t bytes = width - xShift;	// per row

	// Copy each row, then step both offsets back to the start of the row above. The row
	// count is 16 bits: 0 counts down through 65536.
	do
	{
		SegMovs(vp->mem, di, vp->mem, si, bytes & 1, 1, false);
		SegMovs(vp->mem, di, vp->mem, si, bytes >> 1, 2, false);
		di -= bytes + bytes + xShift;
		si -= bytes + bytes + xShift;
	} while (--rows != 0);
}
