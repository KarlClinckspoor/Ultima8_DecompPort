// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: AHEADER.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "aheader.h"

void AnimHeader::clear(void)
{
	memset(this, 0, sizeof(AnimHeader));
}

Boolean AnimHeader::isData(void)
{
	for (int16_t i = 0; i < 64; i++)
		if (data[i])
			return TRUE;
	return FALSE;
}
