#ifndef CVPORT_H
#define CVPORT_H

#include "u8port.h"

#include "cdesc.h"
#include "crect.h"
#include "cvidmode.h"
#include "cexit.h"

class Vport
{
public:
	uint16_t seg;
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
	int16_t getSeg(void);
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

extern "C" void CopyViewport(Vport *, Vport *);
extern "C" void ClearViewport(Vport *, uint8_t);
extern "C" void DrawBox(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
extern "C" void DrawRectangle(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
extern "C" void DrawLine(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
extern "C" void SlamScreen(uint16_t, void *);

extern uint16_t VgaScreenDescriptor;
extern uint8_t SegmentInterval[2];

#endif
