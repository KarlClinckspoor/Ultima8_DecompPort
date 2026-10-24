#ifndef FLICPLAY_H
#define FLICPLAY_H

#include "u8port.h"

#include "process.h"
#include "cpalette.h"
#include "crect.h"
#include "flex.h"

// Ends after a number of ticks.
class QuickWait : public ProcessRealTime
{
public:
	int16_t ticks;

	QuickWait(int16_t, int16_t);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

class CacheFont;
class FastFlex;

// One scripted event of a flic: at a frame, do `type` with `value`.
class FlicEvent
{
public:
	int16_t frame;
	int16_t type;
	int16_t value;
};

// Plays a flic from a flex file (record 0 holds its event script) on a timer.
class FlicPlayer : public ProcessRealTime
{
public:
	FlicEvent events[256];
	uint8_t pad_638;
	int16_t textY;
	uint8_t pad_63b[2];
	Rect textRect;
	uint8_t pad_645;
	CacheFont *font;
	FastFlex *flex;
	char filename[30];
	uint8_t loop;
	int16_t state;
	int16_t record;
	int16_t numRecords;
	Index index;
	char *underText;
	Palette *palette;
	char *speech;
	uint8_t f_687;
	int16_t speed;
	PaletteData paletteData;
	char *paletteChunk;
	char *frameData;
	int16_t value;
	uint8_t pad_69b[2];
	uint8_t shown;
	uint8_t fading;
	PaletteData *fadePalette;
	int16_t loopCount;
	int16_t frameCount;
	int16_t nextEvent;

	FlicPlayer(char *, uint8_t, uint8_t);
	virtual void process(void);
	virtual ~FlicPlayer(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

void PauseGame(void);
int16_t playIntro(uint8_t, uint8_t, uint8_t);
int16_t playEndgame(void);

#endif
