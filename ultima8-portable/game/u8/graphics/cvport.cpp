// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CVPORT.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include "crect.h"
#include "cvport.h"
#include "segmem.h"

inline void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
inline void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
inline int16_t Rect::width(void) { return f_04 - f_00 + 1; }
inline int16_t Rect::height(void) { return f_06 - f_02 + 1; }
inline Rect *Vport::getRect(void) { return &rect; }
inline void GraphicYtable::setType(uint8_t type) { f_04 = type; }

uint8_t SegmentInterval[2];

// The pixels are a block of their own (they had a selector of their own), rows from offset 0.
// The DOS block was (uint16_t)(h * w + 2) bytes, the 2 for the dword adjustment init_ytable
// made; the block is a full 64K now, so the drawing code's 16-bit offsets, which wrapped inside
// the selector's segment, stay inside it.
uint8_t Vport::alloc(int16_t keepYtable)
{
	uint8_t *block = 0;

	if ((block = new (std::nothrow) uint8_t[SEGMENT_SIZE]) != 0)
	{
		mem = block;
		if (graphicYtable.index && keepYtable)
		{
			// As shipped: an owned viewport keeping its table frees the new block and
			// keeps pointing at it. Nothing calls alloc with keepYtable set.
			if (f_10 & 1)
				delete[] block;
		}
		else if (!init_ytable(0))
		{
			delete[] block;
			block = 0;
		}
		if (block)
		{
			mem = block;
			f_10 |= 1;
		}
	}
	return block != 0;
}

void Vport::map_to_screen(uint8_t color)
{
	f_0f = 2;
	rect.set(0, 0, 319, 199);
	mem = VgaScreen;
	if (init_ytable(0) == 0)
		Fatal("Failed creating Ytable!");
	clear(color);
}

// The DOS code moved start on by 2 when the selector's base was not dword aligned; host
// blocks are, and the table starts where it is told.
uint8_t Vport::init_ytable(uint16_t start)
{
	uint16_t *oldIndex = graphicYtable.index;

	if (graphicYtable.index && !(uint8_t)(f_10 & 2))
		graphicYtable.index = 0;
	graphicYtable.setType(f_0f);
	uint8_t ok = graphicYtable.init_ytable(rect.width(), rect.height(), start);
	if (ok)
		f_10 |= 2;
	else
		graphicYtable.index = oldIndex;
	return ok;
}

void Vport::free(void)
{
	if (mem)
	{
		if (f_10 & 1)
			;
		(f_10 & 2) && graphicYtable.index ? (delete[] graphicYtable.index, graphicYtable.index = 0) : 0;
		if (f_10 & 1)
			delete[] mem;
		f_10 = 0;
	}
}

Vport &Vport::operator=(Vport &v)
{
	free();
	memcpy(this, &v, sizeof(Vport));
	f_10 = 0;
	return *this;
}

void Vport::copy_to(Vport *dst)
{
	CopyViewport(dst, this);
}

void Vport::copy_from(Vport *src)
{
	CopyViewport(this, src);
}

void Vport::clear(uint8_t color)
{
	ClearViewport(this, color);
}

void Vport::fill(Rect &r, uint8_t color)
{
	DrawBox(this, r.f_00, r.f_02, r.f_04, r.f_06, color);
}

void Vport::fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
	DrawBox(this, x1, y1, x2, y2, color);
}

// Copies r (all of this viewport if r is empty) to dst at x, y, clipped to dst.
// The far pointers moved by 16-bit offset arithmetic within their segments and _fmemcpy
// copied with rep movs, so the offsets here are 16 bits into each block.
void Vport::copy_to(Vport *dst, int16_t x, int16_t y, Rect r)
{
	if (r.width() <= 0 && r.height() <= 0)
		r = rect;
	else
		r.clip(rect);

	Rect dr = *dst->getRect();
	int16_t rows = r.height();
	uint16_t src = *graphicYtable.index + (int16_t)(r.f_02 * rect.width()) + r.f_00;
	uint16_t to = *dst->graphicYtable.index + (int16_t)(y * dr.width()) + x;
	int16_t srcStride = rect.width();
	int16_t dstStride = dr.width();
	int16_t cols = r.width();
	int16_t skip;
	int16_t i;

	skip = dr.f_02 - y;
	if (skip > 0)
	{
		rows -= skip;
		src += (int16_t)(rect.width() * skip);
		to += (int16_t)(dr.width() * skip);
	}
	if ((int16_t)(y + r.height()) >= dr.f_06)
		rows -= r.f_06 - r.f_02 + y - dr.f_06;
	skip = dr.f_00 - x;
	if (skip > 0)
	{
		cols -= skip;
		src += skip;
		to += skip;
	}
	if ((int16_t)(x + r.width()) >= dr.f_04)
		cols -= r.f_04 - r.f_00 + x - dr.f_04;
	if (rows > 0 && cols > 0)
	{
		for (i = 0; i < rows; i++, src += srcStride, to += dstStride)
		{
			uint16_t di = to;
			uint16_t si = src;
			SegMovs(dst->mem, di, mem, si, cols, 1, false);
		}
	}
}

void VideoRestore::exit_code(void)
{
	activate();
}
