// The VGA BIOS ROM fonts (int 10h function 1130h), for the debug text the game draws
// itself. The host has the 8x8 font; the 8x14 and 8x16 ones are made from it by repeating
// rows.

#include "u8port.h"

#include "vgafont8.h"

static uint8_t font14[256][14];
static uint8_t font16[256][16];

static void build(void)
{
	static bool built;

	if (built)
		return;
	built = true;
	for (int c = 0; c < 256; c++) {
		for (int r = 0; r < 14; r++)
			font14[c][r] = VgaFont8[c][r * 8 / 14];
		for (int r = 0; r < 16; r++)
			font16[c][r] = VgaFont8[c][r / 2];
	}
}

// font: 2 the 8x14 font, 3 the 8x8 one, 6 the 8x16 one, as BH chose them.
uint8_t *getRomFontPtr(int16_t font)
{
	build();
	switch (font) {
	case 3:
		return (uint8_t *)VgaFont8;
	case 6:
		return &font16[0][0];
	default:
		return &font14[0][0];
	}
}
