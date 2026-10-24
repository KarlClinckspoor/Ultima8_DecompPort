// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: ..\SOUND\SONARC.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include "cexit.h"
#include "dsfxman.h"
#include "sonarc.h"

// Moves a selector's base forward by offset bytes. The DOS build did that so DCMP8, which
// reached its buffers through 16-bit offsets, could be handed a fresh 64K window into a
// buffer longer than 64K with the same pointer; the pointer itself moves now.
void incPtr(char *&ptr, int16_t offset)
{
	ptr += offset;
}

// A word of a Sonarc block header, which need not be aligned.
static inline int16_t blockWord(const char *p, int16_t index)
{
	const uint8_t *b = (const uint8_t *)p + index * 2;
	return (int16_t)(b[0] | (b[1] << 8));
}

// Decompresses Sonarc blocks from src to dest; returns the bytes written.
// (src and dest are walked through the buffers here; the DOS build moved their selectors'
// bases and put them back at the end, which the caller's copies never saw either.)
int32_t decompress(char *src, int32_t srcLength, char *dest, int16_t toUnsigned)
{
	int32_t total = 0;

	while (srcLength != 0)
	{
		int16_t packed = blockWord(src, 0);
		int16_t length = blockWord(src, 1);
		int16_t headerLength = src[7];

		// memset and memcpy took a 16-bit size_t.
		memset(decompressionBuffer, 0, (uint16_t)headerLength);
		if (DCMP8(src, (char *)decompressionBuffer))
		{
			halt(__FILE__, 155);	// __LINE__
		}
		if (toUnsigned)
		{
			char *p = (char *)decompressionBuffer + headerLength;
			for (int16_t i = 0; i < length; i++, p++)
			{
				*p += 0x80;
			}
		}
		memcpy(dest, (char *)decompressionBuffer + headerLength, (uint16_t)length);
		total += length;
		incPtr(dest, length);
		incPtr(src, packed);
		srcLength -= packed;
	}
	return total;
}
