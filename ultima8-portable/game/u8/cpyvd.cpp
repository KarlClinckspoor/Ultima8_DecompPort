// name: CPYVD
// Ported from CPYVD.ASM.
// Whole-screen fills and copies for a 320x200 byte-per-pixel screen.
//
// The screen argument was a selector (the screen's, or a buffer's); it is the buffer's
// address now. The routines that write the whole screen show it afterwards with
// plat_video_present, as VGA memory showed every write at once.

#include "u8port.h"

#include "plat.h"
#include "cvport.h"

#define SCREEN_BYTES (320 * 200)

// void ClearScreenOnly(unsigned screen, int color)
// Fills the screen segment with the low byte of color.
void ClearScreenOnly(uint8_t *screen, int16_t color)
{
	memset(screen, (uint8_t)color, SCREEN_BYTES);
	plat_video_present(VgaScreen);
}

// void SlamScreen(unsigned screen, void far *image)
// Copies a full screen image into the screen segment.
void SlamScreen(uint8_t *screen, void *image)
{
	// memmove: a viewport mapped to the screen may be slammed onto it.
	memmove(screen, image, SCREEN_BYTES);
	plat_video_present(VgaScreen);
}

// void SlamWordScreen(unsigned screen, void far *image)
// SlamScreen a word at a time.
void SlamWordScreen(uint8_t *screen, void *image)
{
	memmove(screen, image, SCREEN_BYTES);
	plat_video_present(VgaScreen);
}

// void SlamClearScreenOnly(void)
// Does nothing. Without args the frame has to be spelled out.
void SlamClearScreenOnly(void)
{
	plat_video_present(VgaScreen);
}

// void TransformScreen(unsigned screen, unsigned char far *table)
// Maps every pixel through a 256-byte colour table, in place.
// The assembly never set si, so its reads started at whatever offset the caller left in si
// and the result was shifted by that much. Nothing calls it; the reads start at 0 here.
void TransformScreen(uint8_t *screen, uint8_t *table)
{
	uint16_t si = 0;
	uint16_t di = 0;
	uint16_t cx = SCREEN_BYTES;

	do
	{
		uint8_t pixel = screen[si++];
		screen[di++] = table[pixel];
	} while (--cx != 0);
}

// void ClearAnyScreenOnly(unsigned screen, unsigned char color)
void ClearAnyScreenOnly(uint8_t *screen, uint8_t color)
{
	memset(screen, color, SCREEN_BYTES);
}
