// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: ..\SOUND\SONARC.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include "cdesc.h"
#include "cexit.h"
#include "dsfxman.h"
#include "sonarc.h"

// Moves a selector's base forward by offset bytes.
void incPtr(char *ptr, int16_t offset)
{
	Desc *desc = &LocalDescriptorTable::table[FP_SEG(ptr) >> 3];
	uint32_t base = desc->getBase() + offset;
	desc->baseLow = base;
	desc->baseMid = base >> 16;
	desc->baseHigh = base >> 24;
}

// Decompresses Sonarc blocks from src to dest; returns the bytes written.
int32_t decompress(char *src, int32_t srcLength, char *dest, int16_t toUnsigned)
{
	uint32_t srcBase = LocalDescriptorTable::table[FP_SEG(src) >> 3].getBase();
	uint32_t destBase = LocalDescriptorTable::table[FP_SEG(dest) >> 3].getBase();
	int32_t total = 0;

	while (srcLength != 0)
	{
		int16_t packed = ((int16_t *)src)[0];
		int16_t length = ((int16_t *)src)[1];
		int16_t headerLength = src[7];

		memset(decompressionBuffer, 0, headerLength);
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
		memcpy(dest, (char *)decompressionBuffer + headerLength, length);
		total += length;
		incPtr(dest, length);
		incPtr(src, packed);
		srcLength -= packed;
	}
	LocalDescriptorTable::table[FP_SEG(src) >> 3].setBase(srcBase);
	LocalDescriptorTable::table[FP_SEG(dest) >> 3].setBase(destBase);
	return total;
}
