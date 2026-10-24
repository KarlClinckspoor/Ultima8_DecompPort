#ifndef CTMPOINT_H
#define CTMPOINT_H

#include "u8port.h"

#include "cmpoint.h"
#include "crect.h"

class MemHandle;
class Shape;
class Vport;

// A mouse pointer drawn in software as a shape on a viewport.
class TMMousePointer : public MousePointer
{
public:
	TMMousePointer(void);
	~TMMousePointer(void);
	virtual void handle(int16_t, int16_t, int16_t, int16_t);
	virtual int16_t getFrame(int16_t, int16_t);
	static void map_to(Vport &, uint8_t);
	static void set_shape(MemHandle &);
	static void show(void);
	static void hide(void);
	static void draw(int16_t, int16_t);
	static void undraw(void);
	static void draw(Vport &);
	static void undraw(Vport &);
	static void before_slam(Vport *);
	static void after_slam(Vport *);
	static Vport *mapped_vport;
	static Shape *pointer;
	static Point dimensions;
};

extern TMMousePointer *theMouse;

#endif
