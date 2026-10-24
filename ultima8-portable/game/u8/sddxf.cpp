// name: SDDXF
// Ported from SDDXF.ASM.
// Blends a run-length shape frame with the screen through a table, clipped.
//
// The screen is addressed as the assembly addressed es: by 16-bit offsets from
// vp->mem (Vport::alloc gives every buffer 64K, so a wrapped offset stays in
// it). Frame bytes are read through a plain pointer.

#include "u8port.h"

#include "cvport.h"
#include "skipdraw.h"

// A shape pixel whose mix entry is this is drawn as is.
#define KEEP_PIXEL	0xff

// One pixel of the blend: shape pixel s over screen pixel p draws
// mix[mix[s] * 256 + p], or s itself when mix[s] is KEEP_PIXEL. The index is a
// 16-bit sum, as bx was.
static inline uint8_t blend(const uint8_t *mix, uint8_t s, uint8_t p)
{
	uint8_t m = mix[s];
	if (m == KEEP_PIXEL)
		return s;
	return mix[(uint16_t)((m << 8) + p)];
}

// The blend loops (dec cx / jne) over n pixels, left to right. As in the
// assembly, n = 0 goes round 65536 times. A run of pixels of its own reads
// one frame byte per pixel; a run of one colour reads its fill byte once.
static inline void blendCopy(uint8_t *mem, uint16_t &di, const uint8_t *&si, const uint8_t *mix, uint16_t n)
{
	do
	{
		uint8_t s = *si++;
		mem[di] = blend(mix, s, mem[di]);
		di++;
	}
	while (--n);
}

static inline void blendFill(uint8_t *mem, uint16_t &di, const uint8_t *&si, const uint8_t *mix, uint16_t n)
{
	uint8_t s = *si++;
	do
	{
		mem[di] = blend(mix, s, mem[di]);
		di++;
	}
	while (--n);
}

// int SkipDoubleTransformDraw(Vport far *vp, int x, int y, SKIPFRAME far *frame,
//                             unsigned char far *mix)
// A shape pixel s over screen pixel p draws mix[mix[s] * 256 + p], or s
// itself when mix[s] is KEEP_PIXEL. Returns 1 when the frame misses the
// viewport, else 0.
int16_t SkipDoubleTransformDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame, const uint8_t *mix)
{
	int16_t x1 = vp->rect.f_00;
	int16_t y1 = vp->rect.f_02;
	int16_t x2 = vp->rect.f_04;
	int16_t y2 = vp->rect.f_06;
	uint8_t *mem = vp->mem;
	uint16_t *ytable = vp->graphicYtable.index;
	uint16_t pitch, skipRows, rows, cols, skipLeft, clip;
	int16_t ax, old, hidden;
	uint16_t cx, n, di, d, run;
	const uint8_t *bx;
	const uint8_t *si;

	ax = x - frame->sfXOff;
	if (ax > x2)
		return 1;
	ax = ax + frame->sfWidth - 1;
	if (ax < x1)
		return 1;
	ax = y - frame->sfYOff;
	if (ax > y2)
		return 1;
	ax = ax + frame->sfHeight - 1;
	if (ax < y1)
		return 1;

	// Trim the frame to the clip rectangle.
	rows = frame->sfHeight;
	cols = frame->sfWidth;
	skipRows = 0;
	skipLeft = 0;
	clip = 0;
	ax = y - frame->sfYOff;
	if (ax < y1)
	{
		skipRows = y1 - ax;
		rows -= skipRows;
	}
	ax = ax + frame->sfHeight - 1;
	if (ax > y2)
		rows -= (uint16_t)(ax - y2);
	ax = x - frame->sfXOff;
	if (ax < x1)
	{
		skipLeft = x1 - ax;
		cols -= skipLeft;
		clip |= CLIP_HEAD;
	}
	ax = ax + frame->sfWidth - 1;
	if (ax > x2)
	{
		cols -= (uint16_t)(ax - x2);
		clip |= CLIP_TAIL;
	}

	// di = screen offset of the first drawn row's left edge; rows below it
	// are pitch apart, pitch being the distance of the first two row offsets.
	pitch = ytable[1] - ytable[0];
	di = ytable[(uint16_t)(y - frame->sfYOff + skipRows)];
	di = di - frame->sfXOff + x;
	bx = SkipFrameRowEntry(frame, skipRows);

	if (clip == 0)
	{
		// Whole rows.
		if (!(frame->sfFlags & SF_COMPRESSED))
		{
			do
			{
				ax = cols;
				si = SkipFrameRowData(bx);
				bx += 2;
				d = di;
				for (;;)
				{
					cx = *si++;
					ax -= cx;
					if (ax == 0)
						break;
					d += cx;
					n = *si++;
					blendCopy(mem, d, si, mix, n);
					ax -= n;
					if (ax == 0)
						break;
				}
				di += pitch;
			}
			while (--rows);
			return 0;
		}
		do
		{
			ax = cols;
			si = SkipFrameRowData(bx);
			bx += 2;
			d = di;
			for (;;)
			{
				cx = *si++;
				ax -= cx;
				if (ax == 0)
					break;
				d += cx;
				cx = *si++;
				n = cx >> 1;
				if (cx & 1)
					blendFill(mem, d, si, mix, n);
				else
					blendCopy(mem, d, si, mix, n);
				ax -= n;
				if (ax == 0)
					break;
			}
			di += pitch;
		}
		while (--rows);
		return 0;
	}

	// Clipped rows: runs are trimmed to cols pixels, starting skipLeft in.
	// A row cut at its start (and maybe its end too) is walked past its
	// hidden part first; one cut at the end only is drawn until cols run out.
	if (!(frame->sfFlags & SF_COMPRESSED))
	{
		if (clip != CLIP_TAIL)
		{
			// Raw rows cut at the start (and maybe the end).
			do
			{
				ax = cols;
				si = SkipFrameRowData(bx);
				bx += 2;
				hidden = skipLeft;
				d = di + skipLeft;
				// Walk the hidden part; hidden counts the pixels still to pass.
				for (;;)
				{
					cx = *si++;
					old = hidden;
					hidden -= cx;
					if (hidden == 0)
						goto rawHeadRun;
					if (old < (int16_t)cx)
					{
						// The edge is inside this skip.
						cx = -hidden;
						goto rawHeadGap;
					}
					cx = *si++;
					si += cx;
					old = hidden;
					hidden -= cx;
					if (hidden == 0)
						goto rawHeadSkip;
					if ((uint16_t)old < cx)
					{
						// The edge is inside this run: back up to its first
						// visible pixel.
						si -= cx;
						hidden += cx;
						cx -= hidden;
						si += hidden;
						goto rawHeadCopy;
					}
				}
			rawHeadSkip:
				cx = *si++;
			rawHeadGap:
				if (!(ax > (int16_t)cx))
					cx = ax;
				ax -= cx;
				if (ax == 0)
					goto rawHeadRowDone;
				d += cx;
			rawHeadRun:
				cx = *si++;
			rawHeadCopy:
				if (!(ax > (int16_t)cx))
					cx = ax;
				n = cx;
				blendCopy(mem, d, si, mix, n);
				ax -= n;
				if (ax != 0)
					goto rawHeadSkip;
			rawHeadRowDone:
				di += pitch;
			}
			while (--rows);
			return 0;
		}

		// Raw rows cut at the end only.
		do
		{
			ax = cols;
			si = SkipFrameRowData(bx);
			bx += 2;
			d = di;
			for (;;)
			{
				cx = *si++;
				old = ax;
				ax -= cx;
				if (old <= (int16_t)cx)
					break;
				d += cx;
				cx = *si++;
				if (!(ax > (int16_t)cx))
					cx = ax;
				n = cx;
				blendCopy(mem, d, si, mix, n);
				ax -= n;
				if (ax == 0)
					break;
			}
			di += pitch;
		}
		while (--rows);
		return 0;
	}

	if (clip != CLIP_TAIL)
	{
		// Compressed rows cut at the start (and maybe the end).
		do
		{
			ax = cols;
			si = SkipFrameRowData(bx);
			bx += 2;
			hidden = skipLeft;
			d = di + skipLeft;
			// Walk the hidden part; run keeps the run byte for its fill bit.
			for (;;)
			{
				cx = *si++;
				old = hidden;
				hidden -= cx;
				if (hidden == 0)
					goto packHeadRun;
				if (old < (int16_t)cx)
				{
					cx = -hidden;
					goto packHeadGap;
				}
				cx = *si;
				run = cx;
				si += 2;	// past the run byte and, for a fill, its colour
				cx >>= 1;
				if (!(run & 1))
				{
					si--;
					si += cx;
				}
				old = hidden;
				hidden -= cx;
				if (hidden == 0)
					goto packHeadSkip;
				if ((uint16_t)old < cx)
				{
					// The edge is inside this run.
					si--;
					if (run & 1)
					{
						// si is on the fill byte.
						hidden += cx;
						cx -= hidden;
						goto packHeadFill;
					}
					si++;
					si -= cx;
					hidden += cx;
					cx -= hidden;
					si += hidden;
					goto packHeadCopy;
				}
			}
		packHeadSkip:
			cx = *si++;
		packHeadGap:
			if (!(ax > (int16_t)cx))
				cx = ax;
			ax -= cx;
			if (ax == 0)
				goto packHeadRowDone;
			d += cx;
		packHeadRun:
			cx = *si++;
			run = cx;
			cx >>= 1;
			if (!(run & 1))
				goto packHeadCopy;
		packHeadFill:
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			blendFill(mem, d, si, mix, n);
			goto packHeadNext;
		packHeadCopy:
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			blendCopy(mem, d, si, mix, n);
		packHeadNext:
			ax -= n;
			if (ax != 0)
				goto packHeadSkip;
		packHeadRowDone:
			di += pitch;
		}
		while (--rows);
		return 0;
	}

	// Compressed rows cut at the end only.
	do
	{
		ax = cols;
		si = SkipFrameRowData(bx);
		bx += 2;
		d = di;
		for (;;)
		{
			cx = *si++;
			old = ax;
			ax -= cx;
			if (old <= (int16_t)cx)
				break;
			d += cx;
			cx = *si++;
			run = cx;
			cx >>= 1;
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			if (run & 1)
				blendFill(mem, d, si, mix, n);
			else
				blendCopy(mem, d, si, mix, n);
			ax -= n;
			if (ax == 0)
				break;
		}
		di += pitch;
	}
	while (--rows);
	return 0;
}
