// name: TMSVRECT
// Ported from TMSVRECT.ASM.
// Saves a block of viewport pixels to a buffer.

#include "u8port.h"

#include "crect.h"
#include "cvport.h"
#include "cshape.h"
#include "segmem.h"

// void SaveRect(Vport far *vp, unsigned char far *buffer, Rect far *rect)
// The buffer is laid out as the whole rect, one row after another; only the part inside
// the viewport's clip rectangle is read. RestoreRect is the reverse.
//
// The buffer is addressed by 16-bit offsets from its start, as from offset 0 of its own
// selector. Compares are signed; widths and the row count are 16-bit and unchecked, so a
// clipped size of 0 rows counts down through 65536, as the assembly did.
void SaveRect(Vport *vp, void *buffer, Rect *rect)
{
	uint16_t bufPitch = rect->f_04 - rect->f_00 + 1;
	uint16_t wide = bufPitch;
	uint16_t rows = rect->f_06 - rect->f_02 + 1;
	int16_t rectLeft, left, rectTop, top;

	// Clip the rect to the viewport.
	if (rect->f_00 > vp->rect.f_04)
		return;
	rectLeft = left = rect->f_00;
	if (vp->rect.f_00 >= left)
	{
		wide -= vp->rect.f_00 - left;
		left = vp->rect.f_00;
	}
	if (rect->f_02 > vp->rect.f_06)
		return;
	rectTop = top = rect->f_02;
	if (vp->rect.f_02 >= top)
	{
		rows -= vp->rect.f_02 - top;
		top = vp->rect.f_02;
	}
	if (rect->f_04 < vp->rect.f_00)
		return;
	if (rect->f_04 > vp->rect.f_04)
		wide -= rect->f_04 - vp->rect.f_04;
	if (rect->f_06 < vp->rect.f_02)
		return;
	if (rect->f_06 > vp->rect.f_06)
		rows -= rect->f_06 - vp->rect.f_06;

	// di at the matching buffer byte, si at the first clipped screen pixel. The screen
	// pitch is the distance between the first two row offsets.
	uint8_t *buf = (uint8_t *)buffer;
	uint16_t rowDi = Mul16(top - rectTop, bufPitch) + (left - rectLeft);
	uint16_t screenPitch = vp->graphicYtable.index[1] - vp->graphicYtable.index[0];
	uint16_t rowSi = vp->graphicYtable.index[top] + left;
	do
	{
		uint16_t di = rowDi;
		uint16_t si = rowSi;
		SegMovs(buf, di, vp->mem, si, wide >> 1, 2, false);
		SegMovs(buf, di, vp->mem, si, wide & 1, 1, false);
		rowDi += bufPitch;
		rowSi += screenPitch;
	} while (--rows != 0);
}
