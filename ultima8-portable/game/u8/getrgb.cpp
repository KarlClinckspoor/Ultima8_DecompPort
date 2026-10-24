// name: GETRGB
// Ported from GETRGB.ASM.
// Reads VGA palette entries back from the DAC.
//
// The DAC is the platform's palette: plat_video_get_palette gives the same 6-bit red, green,
// blue triples the DAC's data port did. RGB (PAL.H) is those three bytes.

#include "u8port.h"

#include "plat.h"
#include "cpalette.h"

// void pascal GETCOLORRGB(int index, unsigned char far *rgb)
// The DAC index is the low byte of index. A null rgb reads nothing (the assembly tested only
// the pointer's offset for 0).
void GETCOLORRGB(uint8_t index, RGB *rgb)
{
	if (rgb == 0)
		return;
	plat_video_get_palette(index, 1, (uint8_t *)rgb);
}

// void pascal GETCOLORBANKRGB(int first, int count, unsigned char far *rgb)
// Reads count entries from first on, three bytes each.
// The count is cx for loop (16 bits, 0 meaning 65536) and the DAC index a byte that wraps from
// 255 to 0.
void GETCOLORBANKRGB(uint8_t first, int16_t count, RGB *rgb)
{
	if (rgb == 0)
		return;
	uint8_t *to = (uint8_t *)rgb;
	uint32_t left = (uint16_t)count ? (uint16_t)count : 0x10000;
	uint16_t index = first;

	while (left)
	{
		uint16_t run = 256 - index;
		if (run > left)
			run = (uint16_t)left;
		plat_video_get_palette(index, run, to);
		to += run * 3;
		left -= run;
		index = 0;
	}
}
