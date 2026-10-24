#ifndef SPRITE_H
#define SPRITE_H

#include "u8port.h"

#include "process.h"

// A short-lived item that plays its frames from first to last, then from last to end.
class Sprite : public Process
{
public:
	int16_t firstFrame;
	int16_t lastFrame;
	int16_t endFrame;
	int16_t frame;
	uint16_t repeat;
	uint16_t delay;
	uint8_t forward;
	uint8_t turned;

	Sprite(uint16_t, uint16_t, uint16_t, uint16_t, int16_t, uint16_t, uint16_t, uint16_t, uint8_t);
	virtual void process(void);
};

int16_t createSprite(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t, uint16_t, uint8_t);
int16_t createSprite(int16_t, int16_t, int16_t, int16_t, uint16_t, uint16_t, uint8_t);

#endif
