#ifndef CRECT_H
#define CRECT_H

#include "u8port.h"

class Point;

class Point
{
public:
	int16_t f_00;
	int16_t f_02;

	Point(void);
	Point(int16_t, int16_t);
	void set(int16_t, int16_t);
};

class Rect : public Point
{
public:
	int16_t f_04;
	int16_t f_06;

	Rect(void);
	Rect(int16_t, int16_t, int16_t, int16_t);
	void moveto(int16_t, int16_t);
	uint8_t moverel(int16_t, int16_t);
	int16_t contains(Point);
	uint8_t contains(int16_t, int16_t);
	void clip(Rect &);
	uint8_t intersects(Rect &);
	int16_t width(void);
	void set(int16_t, int16_t, int16_t, int16_t);
	int16_t height(void);
};

#endif
