#ifndef CVPORT_H
#define CVPORT_H

#include "u8port.h"

#include "cytable.h"
#include "crect.h"
#include "cvidmode.h"
#include "cexit.h"

// A rectangle of 8-bit pixels: the screen, or a buffer of its own.
// mem was the selector of the pixel memory; row y starts at mem + graphicYtable.index[y].
class Vport
{
public:
	uint8_t *mem;
	GraphicYtable graphicYtable;
	Rect rect;
	uint8_t f_0f;
	uint8_t f_10;

	Vport(void);
	~Vport(void);
	uint8_t alloc(int16_t);
	void map_to_screen(uint8_t);
	uint8_t init_ytable(uint16_t);
	void free(void);
	Vport &operator=(Vport &);
	void copy_to(Vport *);
	void copy_from(Vport *);
	void clear(uint8_t);
	void fill(Rect &, uint8_t);
	void fill(int16_t, int16_t, int16_t, int16_t, uint8_t);
	void copy_to(Vport *, int16_t, int16_t, Rect);
	void slam(Vport *);
	uint8_t *getMem(void) { return mem; }
	uint8_t *row(int16_t y) { return mem + graphicYtable.index[y]; }
	GraphicYtable *getYTable(void);
	Rect *getRect(void);
};

// Puts back the video mode that was current when it was made.
class VideoRestore : public VideoMode, public ExitSystem
{
public:
	VideoRestore(void) { grab(); }
	virtual void exit_code(void);
};

void CopyViewport(Vport *, Vport *);
void ClearViewport(Vport *, uint8_t);
void DrawBox(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
void DrawRectangle(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
void DrawLine(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
// CPYVD.ASM: whole-screen (320x200) fills and copies; screen was a selector. SlamScreen,
// SlamWordScreen, ClearScreenOnly and SlamClearScreenOnly show VgaScreen when done.
void SlamScreen(uint8_t *, void *);
void SlamWordScreen(uint8_t *, void *);
void ClearScreenOnly(uint8_t *, int16_t);
void SlamClearScreenOnly(void);
void TransformScreen(uint8_t *, uint8_t *);
void ClearAnyScreenOnly(uint8_t *, uint8_t);

// The VGA screen: 320x200 bytes the platform shows (was the selector of A000:0000). The block
// is 64K long, as the segment was, for drawing code whose 16-bit offsets wrap.
extern uint8_t *VgaScreen;
extern uint8_t SegmentInterval[2];

#endif
