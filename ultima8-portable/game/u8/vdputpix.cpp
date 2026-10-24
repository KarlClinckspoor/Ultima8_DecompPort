// name: VDPUTPIX
// Ported from VDPUTPIX.ASM.
// Plots one pixel in a viewport.

#include "u8port.h"

#include "cvport.h"

// int PutPixel(Vport far *vp, int x, int y, unsigned char color)
// Points outside the clip rectangle are skipped and return -1.
// A plotted point returns ax as the assembly left it: -1 with its low byte replaced by the
// colour.
int16_t PutPixel(Vport *vp, int16_t x, int16_t y, uint8_t color)
{
	if (y > vp->rect.f_06 || y < vp->rect.f_02 || x < vp->rect.f_00 || x > vp->rect.f_04)
		return -1;
	uint16_t di = vp->graphicYtable.index[y] + x;
	vp->mem[di] = color;
	return (int16_t)(0xFF00 | color);
}

// KERNEL.C declared it as void PutPixel(Vport *, int, int, int); both callers reached the one
// routine, which takes the colour's low byte.
void PutPixel(Vport *vp, int16_t x, int16_t y, int16_t color)
{
	PutPixel(vp, x, y, (uint8_t)color);
}
