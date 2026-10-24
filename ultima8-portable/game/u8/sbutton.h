#ifndef SBUTTON_H
#define SBUTTON_H

#include "u8port.h"

#include "button.h"

class GumpShape;
class GumpShapeDrawer;
class NewGump;
class Rect;

class GumpShape
{
public:
	uint16_t _frame;
	uint16_t shape;

	GumpShape(int16_t);
	Rect get_rect(void);
	void draw(int16_t, int16_t);
	void *get(void);
	void set_frame(int16_t);
	uint8_t collide(int16_t, int16_t);
	static void setDrawer(GumpShapeDrawer *d) { drawer = d; }
	static GumpShapeDrawer *drawer;
};

class ShapePushButton : public PushButton, public GumpShape
{
public:
	ShapePushButton(uint8_t, NewGump *, int16_t);
	virtual void draw(int16_t, int16_t);
	virtual void select(void);
	virtual void unselect(void);
	int16_t set_frame(int16_t);
};

class ShapeCycleButton : public CycleButton, public GumpShape
{
public:
	ShapeCycleButton(uint8_t, NewGump *, int16_t);
	virtual void draw(int16_t, int16_t);
	virtual void select(void);
	virtual void unselect(void);
	int16_t set_frame(int16_t);
};

class ShapeActivatorButton : public ActivatorButton, public GumpShape
{
public:
	ShapeActivatorButton(uint8_t, NewGump *, int16_t);
	virtual void draw(int16_t, int16_t);
	virtual void select(void);
	virtual void unselect(void);
	int16_t set_frame(int16_t);
};

#endif
