#ifndef BASECAM_H
#define BASECAM_H

#include "u8port.h"

#include "draggump.h"

class Event;
class Font;
class NewGump;
class NewGumpId;
class Rect;

class Panel
{
public:
	void drawup(Rect, uint8_t);
	void drawdown(Rect, uint8_t);
};

class PanelGump : public DrgNewGump
{
public:
	uint8_t color;
	Font *font;

	PanelGump(NewGumpId, NewGump *, Rect, uint8_t);
	virtual void commandMouseLeft(Event &);
	virtual void draw(int16_t, int16_t);
	void printf(int16_t, int16_t, char *, ...);
};

class BaseCamera
{
public:
	BaseCamera(Rect, Rect);
	virtual void show(void);
	virtual void show(int16_t, int16_t, int16_t, int16_t, uint8_t);
	virtual void slam(void);
	virtual void initPalette(void);
	void textVideoMode(void);
	void graphicVideoMode(void);
	void flipVideoMode(void);
	static void setNeedSlam(void);
	static uint8_t needSlam;
	static uint8_t inTextMode;
	static uint8_t okToShow;
	static uint8_t alternateFrame;
	static Rect graphics;
	static Rect text;
	static Rect screen;
};

extern BaseCamera *theBaseCamera;

#endif
