// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: U8GMPSHP.C

#include "u8port.h"

#include "crect.h"
#include "cglobvp.h"
#include "fvinfo.h"
#include "flexhand.h"
#include "cfile.h"
#include "filespec.h"
#include "scratchm.h"
#include "cexit.h"
#include "u8gmpshp.h"

// Inline in the shared headers when this file was built.
U8_INLINE Point::Point(void) {}
U8_INLINE Point::Point(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE Rect::Rect(void) {}
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }

#include "skipdraw.h"

FlexShapeHandler *U8GumpShapeDrawer::theGumpShapeHandler = 0;
Rect *U8GumpShapeDrawer::rects;

U8GumpShapeDrawer::U8GumpShapeDrawer(void)
{
	theGumpShapeHandler = new FlexShapeHandler(75, (CacheNodeType)3, fileSpec(0, StaticDir, gumpsFileName, 0));
	if (rects == 0)
	{
		rects = new Rect[75];
		if (rects == 0)
			outOfMemory(__FILE__, 77);	// __LINE__ in the original file
		BaseFile file(fileSpec(0, StaticDir, "gumpage.dat", 0), (OpenMode)0);
		file.read((char *)rects, 75 * sizeof(Rect), -1);
		file.close();
	}
	if (theGumpShapeHandler == 0)
		outOfMemory(__FILE__, 86);	// __LINE__ in the original file
}

void U8GumpShapeDrawer::draw(int16_t x, int16_t y, int16_t shape, int16_t frame)
{
	SkipDraw(GlobalVport::global_ptr, x, y, GetShapeAddress(get(shape), frame));
}

uint8_t U8GumpShapeDrawer::collide(int16_t x, int16_t y, int16_t shape, int16_t frame)
{
	return Collision(GetShapeAddress(get(shape), frame), Point(0, 0), Point(x, y));
}

Rect U8GumpShapeDrawer::get_rect(int16_t shape, int16_t frame)
{
	Rect rect;
	int16_t xOffset, yOffset;
	rect.f_00 = 1;
	rect.f_02 = 1;
	GetShapeInfo((char *)get(shape), frame, (int16_t &)rect.f_04, (int16_t &)rect.f_06, xOffset, yOffset);
	rect.moveto(0, 0);
	return rect;
}
