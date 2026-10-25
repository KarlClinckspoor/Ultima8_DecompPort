// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\INITSYS.C

#include "u8port.h"
#include "plat.h"

#include "cexit.h"
#include "cicrtc.h"
#include "cvport.h"
#include "cdesc.h"
#include "segmem.h"

// The DOS build asked Phar Lap for selectors onto real-mode memory: the BIOS data area, the
// VGA screen at A000:0000 (0xFA00 bytes) and the colour text screen at B800:0000 (8000 bytes).
// The screens are buffers here. VgaScreen is the frame plat_video_present shows; it is a full
// 64K, as the segment was, so 16-bit offsets that run past 320x200 stay inside it.
static uint8_t VgaScreenMemory[SEGMENT_SIZE];
static uint8_t TextScreenMemory[8000];

uint8_t *VgaScreen = VgaScreenMemory;
// 80x25 character and attribute pairs; the mapping was 8000 bytes long.
uint8_t *TextScreen = TextScreenMemory;

// DosGetBIOSSeg gave BiosSegment a selector onto 0040:0000 for InitializeCrtc, which no longer
// reads it.
void GetBiosSegment(void)
{
}

// VgaScreen and TextScreen point at their buffers from the start (constant initialised), since
// global constructors (chargen.cpp's charGen) draw before this runs. Here the platform is
// given the buffer, so what the game draws straight into screen memory shows from now on, as
// VGA memory did.
void SetVgaScreenDescriptor(void)
{
	plat_video_present(VgaScreen);
}
