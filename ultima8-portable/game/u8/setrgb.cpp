// name: SETRGB
// Ported from SETRGB.ASM.
// Writes VGA palette entries to the DAC.
//
// The DAC is the platform's palette: plat_video_set_palette takes the same 6-bit red, green,
// blue triples the DAC's data port did. RGB (PAL.H) is those three bytes.

#include "u8port.h"

#include "plat.h"
#include "cpalette.h"

// void pascal SETCOLORRGB(int index, unsigned char far *rgb)
// The DAC index is the low byte of index.
void SETCOLORRGB(uint8_t index, RGB *rgb)
{
	plat_video_set_palette(index, 1, (const uint8_t *)rgb);
}

// void pascal SETCOLORBANKRGB(int first, int count, unsigned char far *rgb)
// Writes count entries from first on, three bytes each.
// The count is cx for loop (16 bits, 0 meaning 65536) and the DAC index a byte that wraps from
// 255 to 0, so later entries land on earlier ones as they did on the DAC.
void SETCOLORBANKRGB(uint8_t first, int16_t count, RGB *rgb)
{
	const uint8_t *from = (const uint8_t *)rgb;
	uint32_t left = (uint16_t)count ? (uint16_t)count : 0x10000;
	uint16_t index = first;

	while (left)
	{
		uint16_t run = 256 - index;
		if (run > left)
			run = (uint16_t)left;
		plat_video_set_palette(index, run, from);
		from += run * 3;
		left -= run;
		index = 0;
	}
}
