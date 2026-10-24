#ifndef SLIDER_H
#define SLIDER_H

#include "u8port.h"

#include "basecam.h"
#include "sbutton.h"

class Event;
class NewGump;

class SliderGump : public PanelGump, public GumpShape
{
public:
	int16_t value;
	int16_t min;
	int16_t max;
	int16_t delta;

	SliderGump(NewGump *, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t);
	virtual void commandMouseLeft(Event &) {}
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
};

int32_t getSliderInput(uint16_t, int16_t, int16_t, int16_t);

#endif
