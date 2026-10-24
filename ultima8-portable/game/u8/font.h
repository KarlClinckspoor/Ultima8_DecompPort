#ifndef FONT_H
#define FONT_H

#include "u8port.h"

class Font;
class Rect;

class Font
{
public:
	int16_t cursorX;
	int16_t cursorY;
	int16_t xSpacing;
	int16_t ySpacing;

	Font(void);
	virtual void drawChar(int16_t, int16_t, uint8_t) = 0;
	virtual ~Font(void);
	virtual int16_t height(char *);
	virtual int16_t width(char *);
	virtual void dim(char *, int16_t &, int16_t &);
	virtual int16_t height(char) = 0;
	virtual int16_t width(char) = 0;
	virtual void dim(char, int16_t &, int16_t &) = 0;
	virtual int16_t getBaseLine(void) = 0;
	void print(uint8_t);
	void print(char *);
	int16_t print(int16_t, char *);
	void printf(int16_t, int16_t, char, char *, ...);
	int16_t printf(int16_t, int16_t, char *, ...);
	void printf(int16_t, int16_t, int16_t, char *, ...);
	void printf(char *, ...);
	void getRect(Rect &, int16_t, int16_t, char *);
	void gotoXY(int16_t, int16_t);
	void setXSpacing(int16_t);
	void setYSpacing(int16_t);
};

class GlobalFont
{
public:
	static void setMainFont(Font *);
	static Font *main_font;
	static Font *global_ptr;
};

// Makes a font current for the life of a scope.
class CurrentFont
{
public:
	Font *oldFont;

	CurrentFont(Font *);
	~CurrentFont(void);
};

extern "C" void *getRomFontPtr(void);

#endif
