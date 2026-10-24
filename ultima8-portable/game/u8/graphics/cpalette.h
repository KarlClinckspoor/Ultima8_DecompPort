#ifndef CPALETTE_H
#define CPALETTE_H

#include "u8port.h"

class MemHandle;
class RGB;

// A run of `count` palette entries starting at colour index `start`.
class PaletteData
{
public:
	int16_t count;
	uint8_t start;
	RGB *colors;

	PaletteData(void) { count = 0; colors = 0; }
	PaletteData(uint8_t first, int16_t n) { init(first, n); }
	PaletteData(uint8_t first, int16_t n, RGB *rgb) { init(first, n, rgb); }
	~PaletteData(void);
	void init(uint8_t, int16_t);
	void init(RGB *);
	void init(MemHandle &);
	void init(uint8_t, int16_t, RGB *);
	void grab(void);
	void activate(uint8_t, int16_t, int16_t);
	void activate(int16_t steps) { activate(start, count, steps); }
	void grab(uint8_t, int16_t);
	void clear(RGB &, uint8_t, int16_t);
	void copy_from(PaletteData &, uint8_t, int16_t);
	void copy_from(PaletteData &p) { copy_from(p, p.start, p.count); }
	void set(RGB &, uint8_t);
	uint8_t fade_step(PaletteData &);
};

class Palette : public PaletteData
{
public:
	void init(MemHandle &);
	void fade_to_color(RGB &, int16_t, uint8_t, int16_t);
	void fade_from_color(RGB &, int16_t, uint8_t, int16_t);
};

// The VGA DAC (GETRGB.ASM, SETRGB.ASM): count entries from first on, three bytes each.
void GETCOLORRGB(uint8_t, RGB *);
void GETCOLORBANKRGB(uint8_t, int16_t, RGB *);
void SETCOLORBANKRGB(uint8_t, int16_t, RGB *);
void SETCOLORRGB(uint8_t, RGB *);

#endif
