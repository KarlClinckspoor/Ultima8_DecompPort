#ifndef U8GMPSHP_H
#define U8GMPSHP_H

#include "u8port.h"

class GumpShapeDrawer;
class Rect;
class FlexShapeHandler;

class GumpShapeDrawer
{
public:
	virtual void draw(int16_t, int16_t, int16_t, int16_t) = 0;
	virtual Rect get_rect(int16_t, int16_t) = 0;
	virtual void *get(int16_t) = 0;
	virtual uint8_t collide(int16_t, int16_t, int16_t, int16_t) = 0;
};

class U8GumpShapeDrawer : public GumpShapeDrawer
{
public:
	U8GumpShapeDrawer(void);
	~U8GumpShapeDrawer(void);
	virtual void draw(int16_t, int16_t, int16_t, int16_t);
	virtual Rect get_rect(int16_t, int16_t);
	virtual void *get(int16_t);
	virtual uint8_t collide(int16_t, int16_t, int16_t, int16_t);
	static FlexShapeHandler *theGumpShapeHandler;
	static Rect *rects;
};

#endif
