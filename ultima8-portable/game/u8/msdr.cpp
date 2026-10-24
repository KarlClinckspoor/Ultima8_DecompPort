// name: MSDR
// Ported from MSDR.ASM.
// Scrolls a viewport's picture down and to the right.

#include "u8port.h"

#include "cvport.h"
#include "mvscrn.h"
#include "segmem.h"

// void MoveScreenDownRight(Vport *vp, int dx, int dy)
// Moves the picture dx pixels right and dy pixels down, working from the bottom row up
// and copying each row backwards.
// Rows are taken to be one viewport width apart, starting at the first row offset.
void MoveScreenDownRight(Vport *vp, int16_t xShift, int16_t yShift)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t height = vp->rect.f_06 - vp->rect.f_02 + 1;

	// di = last pixel, si = yShift rows above it, xShift pixels back.
	uint16_t end = vp->graphicYtable.index[0] + Mul16(width, height);
	uint16_t rowDi = end - 1;
	uint16_t rowSi = end - Mul16(yShift, width) - xShift - 1;
	uint16_t rows = height - yShift;
	uint16_t bytes = width - xShift;	// per row

	// Odd byte first, then back up one so the words copy whole. The row count is 16 bits:
	// 0 counts down through 65536.
	do
	{
		uint16_t di = rowDi;
		uint16_t si = rowSi;
		SegMovs(vp->mem, di, vp->mem, si, bytes & 1, 1, true);
		si--;
		di--;
		SegMovs(vp->mem, di, vp->mem, si, bytes >> 1, 2, true);
		rowSi -= bytes + xShift;
		rowDi -= bytes + xShift;
	} while (--rows != 0);
}
