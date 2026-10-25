// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: CAFONT.C

#include "u8port.h"

#include "cvport.h"
#include "cglobvp.h"
#include "flexhand.h"
#include "fvinfo.h"
#include "scratchm.h"
#include "filespec.h"
#include "cexit.h"
#include "cafont.h"

FlexShapeHandler *CacheFont::theFontHandler = 0;
uint16_t CacheFont::xSpacingArray[16] = {0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint16_t CacheFont::ySpacingArray[16] = {0xffff, 1, 1, 0, 0, 0xffff, 0xffff, 0xffff, 0xffff, 0, 0, 0, 0, 0, 0, 0};

int16_t CacheFont::getBaseLine(void)
{
	int16_t width, height, xOffset, yOffset;
	char *shape = theFontHandler->get(shapeNum);
	GetShapeInfo(shape, 0, width, height, xOffset, yOffset);
	return yOffset;
}

void CacheFont::drawChar(int16_t x, int16_t y, uint8_t c)
{
	SkipDraw(GlobalVport::global_ptr, x, y, GetShapeAddress(theFontHandler->get(shapeNum), c));
}

int16_t CacheFont::height(char c)
{
	int16_t width, height, xOffset, yOffset;
	char *shape = theFontHandler->get(shapeNum);
	GetShapeInfo(shape, (uint8_t)c, width, height, xOffset, yOffset);
	return height + ySpacing;
}

int16_t CacheFont::width(char c)
{
	int16_t width, height, xOffset, yOffset;
	char *shape = theFontHandler->get(shapeNum);
	GetShapeInfo(shape, (uint8_t)c, width, height, xOffset, yOffset);
	return width + xSpacing;
}

void CacheFont::dim(char c, int16_t &w, int16_t &h)
{
	int16_t width, height, xOffset, yOffset;
	char *shape = theFontHandler->get(shapeNum);
	GetShapeInfo(shape, (uint8_t)c, width, height, xOffset, yOffset);
	w = width + xSpacing;
	h = height + ySpacing;
}

// The outOfMemory line numbers are those of the shipped build.
CacheFont::CacheFont(FontType type)
{
	if (!theFontHandler)
	{
		theFontHandler = new FlexShapeHandler(16, (CacheNodeType)1, fileSpec(0, StaticDir, fontsFileName, 0));
		if (!theFontHandler)
			outOfMemory(__FILE__, 121);
	}
	setFont(type);
}

CacheFont::CacheFont(void)
{
	if (!theFontHandler)
	{
		theFontHandler = new FlexShapeHandler(16, (CacheNodeType)1, fileSpec(0, StaticDir, fontsFileName, 0));
		if (!theFontHandler)
			outOfMemory(__FILE__, 139);
	}
}

void CacheFont::setFont(FontType type)
{
	type < 16 ? type : (type = (FontType)0);
	shapeNum = theFontHandler->startHandle + type;
	xSpacing = xSpacingArray[type];
	ySpacing = ySpacingArray[type];
}

U8_INLINE CacheFont::~CacheFont(void)
{
}

// Out of line: a virtual function, which the class's vtable needs in every build.
int16_t CacheFont::height(char *text)
{
	return Font::height(text);
}

// Out of line: a virtual function, which the class's vtable needs in every build.
int16_t CacheFont::width(char *text)
{
	return Font::width(text);
}

// Out of line: a virtual function, which the class's vtable needs in every build.
void CacheFont::dim(char *text, int16_t &width, int16_t &height)
{
	Font::dim(text, width, height);
}
