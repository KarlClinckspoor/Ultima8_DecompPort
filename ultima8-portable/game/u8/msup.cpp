// name: MSUP
// Ported from MSUP.ASM.
// Scrolls a viewport's picture up.

#include "u8port.h"

#include "cvport.h"
#include "mvscrn.h"
#include "segmem.h"

// void MoveScreenUp(Vport *vp, int dy)
// Moves the picture dy pixels up as one forward block copy.
// Rows are taken to be one viewport width apart, starting at the first row offset.
void MoveScreenUp(Vport *vp, int16_t yShift)
{
	uint16_t width = vp->rect.f_04 - vp->rect.f_00 + 1;
	uint16_t height = vp->rect.f_06 - vp->rect.f_02 + 1;

	// di = top row, si = yShift rows below it.
	uint16_t di = vp->graphicYtable.index[0];
	uint16_t si = di + Mul16(yShift, width);
	uint16_t bytes = Mul16(width, height - yShift);

	// The odd byte first, then words.
	SegMovs(vp->mem, di, vp->mem, si, bytes & 1, 1, false);
	SegMovs(vp->mem, di, vp->mem, si, bytes >> 1, 2, false);
}
