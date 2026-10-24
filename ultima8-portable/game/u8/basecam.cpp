// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: BASECAM.C

#include "u8port.h"

// #include <conio.h>  (DOS only)
// #include <dos.h>  (DOS only)
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "newgump.h"
#include "basecam.h"
#include "cdesc.h"
#include "cglobvp.h"
#include "crect.h"
#include "ctmpoint.h"
#include "cvport.h"
#include "dispatch.h"
#include "font.h"
#include "system.h"

// Inline in the shared headers when this file was built.
inline Point::Point(void) {}
inline void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
inline Rect::Rect(void) {}
inline void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
inline Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }
inline GraphicYtable *Vport::getYTable(void) { return &graphicYtable; }
inline int16_t GraphicYtable::getindex(int16_t y) { return index[y]; }
inline void Vport::slam(Vport *from) { SlamScreen(getMem(), from->getMem() + from->getYTable()->getindex(0)); }

BaseCamera *theBaseCamera = 0;
uint8_t BaseCamera::needSlam = 0;
uint8_t BaseCamera::inTextMode = 0;
uint8_t BaseCamera::okToShow = 1;
uint8_t BaseCamera::alternateFrame = 0;
Rect BaseCamera::graphics;
Rect BaseCamera::text;
Rect BaseCamera::screen;

void BaseCamera::textVideoMode(void)
{
	if (!inTextMode)
	{
		inTextMode = 1;
		theMouse->suspend();
		// DOS: 80x50 text mode (BIOS mode 3 with the 8x8 font) and a text-mode mouse
		// pointer, for the debugger. The host keeps showing the game screen.
		screen = text;
	}
}

void BaseCamera::initPalette(void)
{
}

void BaseCamera::graphicVideoMode(void)
{
	if (inTextMode)
	{
		inTextMode = 0;
		// DOS: back to mode 13h without clearing the screen.
		theMouse->resume();
		u8_window(1, 1, 80, 25);
		gotoxy(1, 1);
		initPalette();
		screen = graphics;
	}
}

void BaseCamera::flipVideoMode(void)
{
	if (inTextMode)
		graphicVideoMode();
	else
		textVideoMode();
}

void BaseCamera::show(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t)
{
	dispatcher->refresh(Rect(x1, y1, x2, y2));
}

void BaseCamera::show(void)
{
	if (okToShow)
	{
		GlobalVport::global_ptr->clear(0);
		show(screen.f_00, screen.f_02, screen.f_04, screen.f_06, 0);
		needSlam = 1;
		slam();
	}
}

void BaseCamera::slam(void)
{
	if (!needSlam)
		return;
	TMMousePointer::before_slam(GlobalVport::global_ptr);
	GlobalVport::main_screen->slam(GlobalVport::global_ptr);
	TMMousePointer::after_slam(GlobalVport::global_ptr);
	needSlam = 0;
}
