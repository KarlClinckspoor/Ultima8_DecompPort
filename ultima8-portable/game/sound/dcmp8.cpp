// name: DCMP8
// Ported from DCMP8.ASM.
// Speech Compression's 1993 DCMP8 speech decoder, linked into U8.EXE as a
// finished object. Disassembled from the shipped code.
//
// int16_t DCMP8(char *src, char *dest)
//   Decodes one Sonarc block. src is the block:
//     +0 word   bytes in the block (the checksum covers its words)
//     +2 word   samples in the block
//     +4 word   (not read by the decoder; it balances the checksum)
//     +6 byte   coding mode, 8 to 21: picks a row of width_table
//     +7 byte   order of the prediction filter (0 for none)
//     +8        order words of filter coefficients, then the packed samples
//   The words of the block XOR to 0ACEDh, or DCMP8 returns -1 (0FFFFh) and writes
//   nothing. Otherwise the samples go to dest + order and 0 is returned. The filter reads
//   the order bytes before them as history, so the caller clears dest[0 .. order - 1].
//   Samples come out as signed bytes with the top bit flipped (80h is silence); SONARC's
//   decompress makes them unsigned.
//
// The assembly reached its buffers through 16-bit offsets (the blocks are well under 64K)
// and read its tables from the code segment; here they are plain pointers and arrays.

#include "u8port.h"

#include "sonarc.h"

// Trailing zero bits of each byte value
static const uint8_t trailing_zeros[256] =
{
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
	0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 8,
};

// 27 bytes for each mode from 8 up, indexed by the trailing zeros of the next bits (the
// length of the code's prefix of zeros, 0 to 8): [n] the bits the prefix takes, [9 + n]
// the bits of the value after it, [18 + n] what the sign extension of the value is
// XORed with.
static const uint8_t width_table[14 * 27] =
{
	// mode 8: prefix bits, value bits, sign xor
	1, 2, 3, 4, 5, 6, 7, 7, 7,
	1, 1, 2, 3, 4, 5, 6, 7, 7,
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 9: prefix bits, value bits, sign xor
	1, 2, 3, 4, 5, 6, 6, 6, 6,
	2, 2, 3, 4, 5, 6, 7, 7, 7,
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 10: prefix bits, value bits, sign xor
	1, 2, 3, 4, 5, 5, 5, 5, 5,
	3, 3, 4, 5, 6, 7, 7, 7, 7,
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 11: prefix bits, value bits, sign xor
	1, 2, 3, 4, 4, 4, 4, 4, 4,
	4, 4, 5, 6, 7, 7, 7, 7, 7,
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 12: prefix bits, value bits, sign xor
	1, 2, 3, 3, 3, 3, 3, 3, 3,
	5, 5, 6, 7, 7, 7, 7, 7, 7,
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 13: prefix bits, value bits, sign xor
	1, 2, 2, 2, 2, 2, 2, 2, 2,
	6, 6, 7, 7, 7, 7, 7, 7, 7,
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 14: prefix bits, value bits, sign xor
	1, 1, 1, 1, 1, 1, 1, 1, 1,
	7, 7, 7, 7, 7, 7, 7, 7, 7,
	0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 15: prefix bits, value bits, sign xor
	0, 2, 3, 4, 5, 6, 7, 8, 8,
	1, 1, 1, 2, 3, 4, 5, 6, 7,
	0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 16: prefix bits, value bits, sign xor
	0, 2, 3, 4, 5, 6, 7, 7, 7,
	1, 2, 2, 3, 4, 5, 6, 7, 7,
	0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 17: prefix bits, value bits, sign xor
	0, 2, 3, 4, 5, 6, 6, 6, 6,
	1, 3, 3, 4, 5, 6, 7, 7, 7,
	0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 18: prefix bits, value bits, sign xor
	0, 2, 3, 4, 5, 5, 5, 5, 5,
	1, 4, 4, 5, 6, 7, 7, 7, 7,
	0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 19: prefix bits, value bits, sign xor
	0, 2, 3, 4, 4, 4, 4, 4, 4,
	1, 5, 5, 6, 7, 7, 7, 7, 7,
	0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 20: prefix bits, value bits, sign xor
	0, 2, 3, 3, 3, 3, 3, 3, 3,
	1, 6, 6, 7, 7, 7, 7, 7, 7,
	0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	// mode 21: prefix bits, value bits, sign xor
	0, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 7, 7, 7, 7, 7, 7, 7, 7,
	0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

static inline uint16_t read_word(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

// The bit reader's refill: dx holds the bits, the low byte the current ones, and ch counts
// the bits of dh still unread. Shifting out cl bits, when more than ch are wanted, shifts
// out the ch first, loads the next byte into dh and shifts out the rest.
static inline void shift_bits(uint16_t &dx, uint8_t &ch, uint8_t cl, const uint8_t *&in)
{
	if (ch < cl)
	{
		uint8_t t = ch;
		ch = cl;
		cl = t;
		dx >>= cl;
		ch -= cl;
		dx = (uint16_t)((dx & 0x00FF) | (*in++ << 8));
		cl = ch;
		ch = 8;
	}
	dx >>= cl;
	ch -= cl;
}

// Unpacks count samples (0 meaning 65536, as the loop's dec/jne did) of mode's variable
// length codes from in to out. Each code is a run of zero bits ended by a one (its length
// read from the trailing zeros of the low byte), then a value whose top bit gives its sign.
static void unpack_samples(uint16_t mode, uint16_t count, const uint8_t *in, uint8_t *out)
{
	const uint8_t *row = width_table + 27 * (mode - 8);
	uint8_t ch = 8;
	uint16_t dx = read_word(in);
	in += 2;
	do
	{
		uint8_t bx = trailing_zeros[dx & 0xFF];
		shift_bits(dx, ch, row[bx], in);
		uint8_t cl = row[bx + 9];
		// ror al, cl; cbw: the value's low cl bits go to the top of al, its top bit
		// spreads into ah. A rotate of a byte by 8 leaves it as it was.
		uint8_t al = (uint8_t)dx;
		uint8_t r = cl & 7;
		al = (uint8_t)((al >> r) | (al << ((8 - r) & 7)));
		uint16_t ax = (uint16_t)(int16_t)(int8_t)al;
		ax ^= (uint16_t)(row[bx + 18] << 8);
		ax = (uint16_t)(ax << cl);
		ax ^= 0x8000;
		*out++ = (uint8_t)(ax >> 8);
		shift_bits(dx, ch, cl, in);
	} while (--count != 0);
}

// Runs the prediction filter over count samples (0 meaning 65536) at dest + order: from
// each it subtracts the sum of the order bytes before it (signed, top bit flipped back)
// times the coefficients, last coefficient first, as a 32-bit sum scaled down by 4096 and
// rounded. coefs is the block + 6, so the coefficients are at coefs + 2.
static void apply_filter(uint16_t order, uint16_t count, uint8_t *dest, const uint8_t *coefs)
{
	const uint8_t *last = coefs + 2 * order;
	do
	{
		uint8_t *si = dest++;
		const uint8_t *di = last;
		uint16_t cx = order;
		uint32_t sum = 0;	// bp:bx
		do
		{
			int16_t sample = (int8_t)(*si++ ^ 0x80);
			int16_t coef = (int16_t)read_word(di);
			di -= 2;
			sum += (uint32_t)((int32_t)sample * coef);
		} while (--cx != 0);
		// dx = bits 12 to 27 of the sum, plus bit 11 (add al, 80h; adc dx, 0).
		uint16_t dx = (uint16_t)(sum >> 12);
		if (sum & 0x800)
			dx++;
		*si -= (uint8_t)dx;
	} while (--count != 0);
}

int16_t DCMP8(char *source, char *destination)
{
	const uint8_t *src = (const uint8_t *)source;
	uint8_t *dest = (uint8_t *)destination;

	// The words of the block (its length halved, 0 meaning 65536) XOR to 0ACEDh.
	uint16_t cx = read_word(src) >> 1;
	uint16_t bx = 0;
	const uint8_t *p = src;
	do
	{
		bx ^= read_word(p);
		p += 2;
	} while (--cx != 0);
	if (bx != 0xACED)
		return -1;

	uint16_t order = src[7];
	uint16_t header = order * 2 + 8;
	uint16_t mode = src[6];
	// The assembly indexed width_table with any mode, reading the bytes of the code around
	// the table for modes outside 8 to 21. The game's sound files use none of those; a
	// block with one is refused as if its checksum had failed.
	if (mode < 8 || mode > 21)
		return -1;
	unpack_samples(mode, read_word(src + 2), src + header, dest + order);
	if (order != 0)
		apply_filter(order, read_word(src + 2), dest, src + 6);
	return 0;
}
