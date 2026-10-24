#ifndef TBUTTON_H
#define TBUTTON_H

#include "u8port.h"

#include "button.h"

class NewGump;

class TextPushButton : public PushButton
{
public:
	uint16_t _frame;
	uint8_t color;
	char *text;

	TextPushButton(uint8_t, NewGump *, char *, uint8_t);
	virtual void draw(int16_t, int16_t);
	virtual void select(void);
	virtual void unselect(void);
	inline int16_t set_frame(int16_t);
};

class TextActivatorButton : public ActivatorButton
{
public:
	uint16_t _frame;
	uint8_t color;
	char *text;

	TextActivatorButton(uint8_t, NewGump *, char *, uint8_t);
	virtual void draw(int16_t, int16_t);
	virtual void select(void);
	virtual void unselect(void);
	inline int16_t set_frame(int16_t);
};

#endif
