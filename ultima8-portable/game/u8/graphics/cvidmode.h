#ifndef CVIDMODE_H
#define CVIDMODE_H

#include "u8port.h"

class VideoMode
{
public:
	int16_t mode;		// BIOS video mode

	VideoMode(void) { mode = 3; }
	void grab(void);
	void activate(void);
};

extern uint16_t params[17];

#endif
