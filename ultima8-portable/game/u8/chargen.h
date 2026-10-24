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
	void setScreen(uint8_t *);	// setSeg(selector) in DOS
	void writeChar(int16_t, int16_t, char);
	void writeString(int16_t, int16_t, char *);
	void makeString(int16_t, int16_t, char *, ...);
};

extern uint8_t *textBuffer;	// textBufferSeg, B800:0000, in DOS
extern CharacterGenerator charGen;

#endif
