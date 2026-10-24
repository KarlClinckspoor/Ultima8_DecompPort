// name: MSDOWN
// Ported from MSDOWN.ASM.
// Scrolls a viewport's picture down.

#include "u8port.h"

#include "cvport.h"
#include "mvscrn.h"
#include "segmem.h"

// void MoveScreenDown(Vport *vp, int dy)
// Moves the picture dy pixels down as one backward block copy.
// Rows are taken to be one viewport width apart, starting at the first row offset.
void MoveScreenDown(Vport *vp, int16_t yShift)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t height = vp->rect.f_06 - vp->rect.f_02 + 1;

	// di = last pixel, si = yShift rows above it.
	uint16_t di = vp->graphicYtable.index[0] + Mul16(width, height) - 1;
	uint16_t si = di - Mul16(yShift, width);
	uint16_t bytes = Mul16(width, height - yShift);

	// Odd byte first, then back up one so the words copy whole.
	SegMovs(vp->mem, di, vp->mem, si, bytes & 1, 1, true);
	di--;
	si--;
	SegMovs(vp->mem, di, vp->mem, si, bytes >> 1, 2, true);
}
