// name: MSRIGHT
// Ported from MSRIGHT.ASM.
// Scrolls a viewport's picture to the right.

#include "u8port.h"

#include "cvport.h"
#include "mvscrn.h"
#include "segmem.h"

// void MoveScreenRight(Vport *vp, int dx)
// Moves the picture dx pixels right, copying each row backwards from the top.
// Rows are taken to be one viewport width apart, starting at the first row offset.
void MoveScreenRight(Vport *vp, int16_t xShift)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t rows = vp->rect.f_06 - vp->rect.f_02 + 1;

	// di = last pixel of the top row, si = xShift pixels before it.
	uint16_t top = vp->graphicYtable.index[0];
	uint16_t rowDi = top + width - 1;
	uint16_t rowSi = top + width - xShift - 1;
	uint16_t bytes = width - xShift;	// per row

	// Odd byte first, then back up one so the words copy whole. The row count is 16 bits:
	// a height of 0 counts down through 65536.
	do
	{
		uint16_t di = rowDi;
		uint16_t si = rowSi;
		SegMovs(vp->mem, di, vp->mem, si, bytes & 1, 1, true);
		di--;
		si--;
		SegMovs(vp->mem, di, vp->mem, si, bytes >> 1, 2, true);
		rowSi += bytes + xShift;
		rowDi += bytes + xShift;
	} while (--rows != 0);
}
