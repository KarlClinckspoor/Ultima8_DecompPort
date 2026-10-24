#ifndef CHARGEN_H
#define CHARGEN_H

#include "u8port.h"

#include "flag.h"
#include "priority.h"

// Writes text with the ROM font straight into the 320x200 screen, for debug output.
class CharacterGenerator
{
public:
	uint8_t *font;
	uint8_t *screen;
	uint8_t smallFont;
	uint8_t fgColor;
	uint8_t bgColor;

	CharacterGenerator(void);
	void setSeg(uint16_t);
	void writeChar(int16_t, int16_t, char);
	void writeString(int16_t, int16_t, char *);
	void makeString(int16_t, int16_t, char *, ...);
};

extern uint16_t textBufferSeg;
extern CharacterGenerator charGen;

#endif
