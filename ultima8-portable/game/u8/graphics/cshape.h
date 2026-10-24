#ifndef CSHAPE_H
#define CSHAPE_H

#include "u8port.h"

#include "cmemhndp.h"

class GraphicPoint;
class GraphicPrimitive;
class Rect;
class Vport;

class GraphicPrimitive
{
public:
	virtual void draw(void) = 0;
	virtual void hide(void) = 0;
	virtual void move(int16_t, int16_t) = 0;
};

class GraphicPoint : public GraphicPrimitive
{
public:
	uint16_t f_02;
	uint16_t f_04;

	virtual void draw(void);
	virtual void hide(void);
	virtual void move(int16_t, int16_t);
	virtual void draw(int16_t, int16_t);
	void set_position(int16_t, int16_t);
	void move_position(int16_t, int16_t);
};

class Shape : public GraphicPoint
{
public:
	MemHandle shape;
	MemHandle background;	// screen under the shape while it is drawn
	uint16_t f_18;			// frame
	uint16_t savedX;
	uint16_t savedY;

	Shape(void);
	Shape(MemHandle &);
	Shape(char *, char, uint8_t);
	Shape(MemHandle &, int16_t, int16_t);
	Shape(MemHandle &, int16_t, int16_t, int16_t);
	Shape(const char *, int16_t, int16_t, int16_t);
	~Shape(void);
	virtual void draw(void);
	virtual void hide(void);
	virtual void move(int16_t, int16_t);
	virtual void draw(int16_t, int16_t);
	virtual void move(int16_t, int16_t, int16_t);
	virtual void move_absolute(int16_t, int16_t);
	virtual void restore(int16_t, int16_t);
	virtual void clear_back(void);
	void free(void);
	void reset(void);
	void set_shape(MemHandle &);
	void set_shape(char *, char, uint8_t);
	void set_frame(int16_t);
	Rect get_rect(int16_t);
	void init_background(void);
	void draw(int16_t);
};

// TMSVRECT.ASM, TMRSRECT.ASM: the rect's pixels, row after row, to and from a buffer.
void SaveRect(Vport *, void *, Rect *);
void RestoreRect(Vport *, void *, Rect *);

#endif
