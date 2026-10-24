// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CVIDMODE.C

#include "u8port.h"

#include "cvidmode.h"

// CRTC index/value pairs for the tweaked 256-colour modes.
uint16_t params[17] =
{
	0x6b00, 0x5901, 0x5a02, 0x8e03, 0x5e04, 0x8a05, 0x4009, 0x0014,
	0xe317, 0x0d06, 0x3e07, 0xea10, 0xac11, 0xe715, 0x0616, 0x4109,
	0x0000
};

// The host screen is always 320x200x8, so setting a mode only records it. biosMode is what
// int 10h function 0Fh would report: the tweaked modes 60h-64h were mode 13h with CRTC and
// sequencer registers reprogrammed (from params), and 7Fh was text mode 3 on page 0. The game
// starts from DOS in text mode 3.
static int16_t biosMode = 3;

void VideoMode::grab(void)
{
	mode = biosMode;
}

void VideoMode::activate(void)
{
	int16_t wanted = mode;

	grab();
	if (mode == wanted)
		return;
	mode = wanted;

	switch (mode)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 0x13:
		biosMode = mode;
		break;

	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
		biosMode = 0x13;
		break;

	case 0x7f:
		biosMode = 3;
		break;
	}
}
