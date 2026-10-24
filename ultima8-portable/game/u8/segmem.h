#ifndef SEGMEM_H
#define SEGMEM_H

#include "u8port.h"

// The x86 string instructions the drawing modules were written with (rep movsb, rep movsw,
// rep stosb, rep stosw), on a block addressed the way a selector's segment was: by 16-bit
// offsets from its start. Offsets wrap at 64K, the count is cx (16 bits, unsigned; 0 moves
// nothing), and a backward move (std) steps both offsets down. A word is read whole and then
// written, so overlapping moves come out exactly as the instructions left them, including the
// smear of a forward move onto itself.
//
// Blocks the assembly could reach with a wrapped offset are allocated 64K long (Vport::alloc,
// VgaScreen), so a wrap stays inside them as it stayed inside the segment.

#define SEGMENT_SIZE 0x10000

// The low word of a 16-bit multiply (imul ax, cx or mul); signed and unsigned agree on it.
inline uint16_t Mul16(uint16_t a, uint16_t b)
{
	return (uint16_t)((uint32_t)a * b);
}

// Moves count units of size bytes (1 for movsb, 2 for movsw) from src:si to dst:di. di and si
// end past the last unit, as the instruction leaves them.
inline void SegMovs(uint8_t *dst, uint16_t &di, const uint8_t *src, uint16_t &si, uint16_t count,
	uint16_t size, bool backward)
{
	if (count == 0)
		return;
	uint32_t bytes = (uint32_t)count * size;

	// When nothing wraps and the copy runs away from the overlap, memmove gives the same bytes.
	if (!backward)
	{
		if (di + bytes <= SEGMENT_SIZE && si + bytes <= SEGMENT_SIZE)
		{
			uint8_t *d = dst + di;
			const uint8_t *s = src + si;
			if ((uintptr_t)s >= (uintptr_t)d || (uintptr_t)(s + bytes) <= (uintptr_t)d)
			{
				memmove(d, s, bytes);
				di = (uint16_t)(di + bytes);
				si = (uint16_t)(si + bytes);
				return;
			}
		}
	}
	else
	{
		uint32_t span = bytes - size;	// from the lowest unit to the first one
		if (di >= span && si >= span && di + size <= SEGMENT_SIZE && si + size <= SEGMENT_SIZE)
		{
			uint8_t *d = dst + di - span;
			const uint8_t *s = src + si - span;
			if ((uintptr_t)s <= (uintptr_t)d || (uintptr_t)(d + bytes) <= (uintptr_t)s)
			{
				memmove(d, s, bytes);
				di = (uint16_t)(di - bytes);
				si = (uint16_t)(si - bytes);
				return;
			}
		}
	}

	uint16_t step = backward ? (uint16_t)-size : size;
	while (count--)
	{
		uint8_t lo = src[si];
		if (size == 2)
		{
			uint8_t hi = src[(uint16_t)(si + 1)];
			dst[di] = lo;
			dst[(uint16_t)(di + 1)] = hi;
		}
		else
			dst[di] = lo;
		di += step;
		si += step;
	}
}

// Stores count units of size bytes (1 for stosb: the low byte of value, 2 for stosw: low byte
// then high byte) at dst:di, forward. di ends past the last unit.
inline void SegStos(uint8_t *dst, uint16_t &di, uint16_t value, uint16_t count, uint16_t size)
{
	uint8_t lo = (uint8_t)value;
	uint8_t hi = size == 2 ? (uint8_t)(value >> 8) : lo;
	uint32_t bytes = (uint32_t)count * size;

	if (lo == hi && di + bytes <= SEGMENT_SIZE)
	{
		memset(dst + di, lo, bytes);
		di = (uint16_t)(di + bytes);
		return;
	}
	while (count--)
	{
		dst[di++] = lo;
		if (size == 2)
			dst[di++] = hi;
	}
}

#endif
