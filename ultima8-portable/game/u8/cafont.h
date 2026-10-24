#ifndef CAFONT_H
#define CAFONT_H

#include "u8port.h"

#include "font.h"

class FlexShapeHandler;

enum FontType : int16_t
{
	FontType_0
};

// A font drawn from the shapes of the fonts flex.
class CacheFont : public Font
{
public:
	uint16_t shapeNum;

	CacheFont(FontType);
	CacheFont(void);
	virtual void drawChar(int16_t, int16_t, uint8_t);
	virtual ~CacheFont(void);
	virtual int16_t height(char *);
	virtual int16_t width(char *);
	virtual void dim(char *, int16_t &, int16_t &);
	virtual int16_t height(char);
	virtual int16_t width(char);
	virtual void dim(char, int16_t &, int16_t &);
	virtual int16_t getBaseLine(void);
	void setFont(FontType);
	static FlexShapeHandler *theFontHandler;
	static uint16_t xSpacingArray[16];
	static uint16_t ySpacingArray[16];
};

#endif
