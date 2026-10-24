#ifndef MSGPROC_H
#define MSGPROC_H

#include "u8port.h"

#include "cafont.h"
#include "cdlist.h"

class PaganProcess;

// One message of the credits or quotes, scrolling up the screen.
class Msg : public Link
{
public:
	int16_t indent;
	int16_t highlight;
	int16_t numLines;
	int16_t y;
	char *text;
	int16_t offScreen;
	int16_t allShown;
	char *lines[20];
	PaganProcess *process;
	CacheFont font;
	CacheFont titleFont;
	uint8_t scroll;

	Msg(PaganProcess *, char *);
	void run(void);
	uint8_t scrolling(void);
	static uint16_t fonttype;
	static uint8_t credits;
};

#endif
