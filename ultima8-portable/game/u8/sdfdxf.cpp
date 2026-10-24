// name: SDFDXF
// Ported from SDFDXF.ASM.
// Blends a mirrored run-length shape frame with the screen through a table,
// clipped.
//
// The screen is addressed as the assembly addressed es: by 16-bit offsets from
// vp->mem (Vport::alloc gives every buffer 64K, so a wrapped offset stays in
// it). Frame bytes are read through a plain pointer.

#include "u8port.h"

#include "cvport.h"
#include "ferror.h"
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

// The blend loops (dec cx / jne) over n pixels, right to left. As in the
// assembly, n = 0 goes round 65536 times. A run of pixels of its own reads
// one frame byte per pixel; a run of one colour reads its fill byte once.
static inline void blendCopyLeft(uint8_t *mem, uint16_t &di, const uint8_t *&si, const uint8_t *mix, uint16_t n)
{
	do
	{
		uint8_t s = *si++;
		mem[di] = blend(mix, s, mem[di]);
		di--;
	}
	while (--n);
}

static inline void blendFillLeft(uint8_t *mem, uint16_t &di, const uint8_t *&si, const uint8_t *mix, uint16_t n)
{
	uint8_t s = *si++;
	do
	{
		mem[di] = blend(mix, s, mem[di]);
		di--;
	}
	while (--n);
}

// int FlipSkipDoubleTransformDraw(Vport far *vp, int x, int y,
//                                 SKIPFRAME far *frame, unsigned char far *mix)
// FlipSkipDraw's placement with SkipDoubleTransformDraw's blending: shape
// pixel s over screen pixel p draws mix[mix[s] * 256 + p], or s itself when
// mix[s] is KEEP_PIXEL. Returns 1 when the frame misses the viewport, else 0.
int16_t FlipSkipDoubleTransformDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame, const uint8_t *mix)
{
	int16_t x1 = vp->rect.f_00;
	int16_t y1 = vp->rect.f_02;
	int16_t x2 = vp->rect.f_04;
	int16_t y2 = vp->rect.f_06;
	uint8_t *mem = vp->mem;
	uint16_t *ytable = vp->graphicYtable.index;
	uint16_t pitch, skipRows, rows, cols, skipLeft, clip, skipRight;
	int16_t ax, old, hidden, flipOff;
	uint16_t cx, n, di, d, run, dx;
	const uint8_t *bx;
	const uint8_t *si;

	// The frame's left edge is width - 1 - sfXOff left of x.
	flipOff = frame->sfWidth - 1 - frame->sfXOff;
	ax = x - flipOff;
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

	// Trim the frame to the clip rectangle. A row starts at the right, so the
	// left edge cuts the tail of the stored row and the right edge its head.
	rows = frame->sfHeight;
	cols = frame->sfWidth;
	skipRows = 0;
	skipLeft = 0;
	clip = 0;
	skipRight = 0;	// left unset by the assembly; only read when CLIP_HEAD is set
	ax = y - frame->sfYOff;
	if (ax < y1)
	{
		skipRows = y1 - ax;
		rows -= skipRows;
	}
	ax = ax + frame->sfHeight - 1;
	if (ax > y2)
		rows -= (uint16_t)(ax - y2);
	ax = x - flipOff;
	if (ax < x1)
	{
		skipLeft = x1 - ax;	// kept as the assembly kept it; nothing reads it
		cols -= skipLeft;
		clip |= CLIP_TAIL;
	}
	ax = ax + frame->sfWidth - 1;
	if (ax > x2)
	{
		ax -= x2;
		cols -= ax;
		skipRight = ax;
		clip |= CLIP_HEAD;
	}

	// di = screen offset of the first drawn row's left edge; each path moves
	// it to the right edge, where the row starts.
	pitch = ytable[1] - ytable[0];
	di = ytable[(uint16_t)(y - frame->sfYOff + skipRows)];
	di = di - flipOff + x;
	bx = SkipFrameRowEntry(frame, skipRows);
	di = di + frame->sfWidth - 1;

	if (clip == 0)
	{
		// Whole rows.
		if (!(frame->sfFlags & SF_COMPRESSED))
		{
			// Raw whole rows. The assembly gets its bookkeeping wrong here (its
			// own comment: "this subtracts the last pixel drawn from ax, and the
			// saved run length is popped once per row however many runs pushed
			// it"). After a run it takes dx from ax, and dx then holds what the
			// blend loop last left in it: 0xff (the mix entry) when the last
			// pixel was kept, else the blended colour; not the run length. It
			// also pushes each run's length and pops it only when that check
			// ends the row.
			//
			// So a row keeps the stack balanced only when its first skip uses
			// up the width, or ax comes to 0 right after its first run. Any
			// other row ends with run lengths left on the stack, which the row's
			// end pops as the row table pointer, the row count and the screen
			// offset; the routine goes on drawing from and to wherever those
			// point and returns through the corrupt stack, which in DOS crashed
			// the game. Once a first run leaves ax nonzero there is no way back
			// to a balanced stack, so the port stops there with Fatal instead of
			// following the lost registers. (No uncompressed frame of a
			// translucent shape is flipped in the shipped map, FIXED.DAT.)
			do
			{
				ax = cols;
				si = SkipFrameRowData(bx);
				bx += 2;
				d = di;
				cx = *si++;
				ax -= cx;
				if (ax != 0)
				{
					d -= cx;
					cx = *si++;
					dx = 0;
					do
					{
						uint8_t s = *si++;
						uint8_t m = mix[s];
						uint8_t out = s;
						dx = m;
						if (m != KEEP_PIXEL)
						{
							out = mix[(uint16_t)((m << 8) + mem[d])];
							dx = out;
						}
						mem[d--] = out;
					}
					while (--cx);
					ax -= dx;
					if (ax != 0)
						Fatal("FlipSkipDoubleTransformDraw: a row of this frame breaks SDFDXF.ASM's stack (see sdfdxf.cpp)\r\n");
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
				d -= cx;
				cx = *si++;
				n = cx >> 1;
				if (cx & 1)
					blendFillLeft(mem, d, si, mix, n);
				else
					blendCopyLeft(mem, d, si, mix, n);
				ax -= n;
				if (ax == 0)
					break;
			}
			di += pitch;
		}
		while (--rows);
		return 0;
	}

	// Clipped rows: runs are trimmed to cols pixels, starting skipRight in.
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
				hidden = skipRight;
				d = di - skipRight;
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
				d -= cx;
			rawHeadRun:
				cx = *si++;
			rawHeadCopy:
				if (!(ax > (int16_t)cx))
					cx = ax;
				n = cx;
				blendCopyLeft(mem, d, si, mix, n);
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
				d -= cx;
				cx = *si++;
				if (!(ax > (int16_t)cx))
					cx = ax;
				n = cx;
				blendCopyLeft(mem, d, si, mix, n);
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
			hidden = skipRight;
			d = di - skipRight;
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
				cx = *si++;
				run = cx;
				si += 2;
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
			d -= cx;
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
			blendFillLeft(mem, d, si, mix, n);
			goto packHeadNext;
		packHeadCopy:
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			blendCopyLeft(mem, d, si, mix, n);
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
			d -= cx;
			cx = *si++;
			run = cx;
			cx >>= 1;
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			if (run & 1)
				blendFillLeft(mem, d, si, mix, n);
			else
				blendCopyLeft(mem, d, si, mix, n);
			ax -= n;
			if (ax == 0)
				break;
		}
		di += pitch;
	}
	while (--rows);
	return 0;
}
