// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CSHAPE.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "crect.h"
#include "cshape.h"
#include "cglobvp.h"
#include "cfile.h"
#include "fvinfo.h"
#include "cmemhndp.h"

inline Point::Point(void) {}
inline Rect::Rect(void) {}
inline void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
inline Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }
inline void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
inline MemHandle::MemHandle(void) { f_00 = 0; f_04 = 0; f_05 = 0; }
inline MemHandle::~MemHandle(void) { free(0); }
inline char *MemHandle::get_ptr(void) { return (char *)f_00; }
inline void MemHandle::init(char *p, uint8_t owned) { f_00 = p; f_04 = owned; f_05 = 0; }
inline void MemHandle::init(MemHandle &from) { init(from.get_ptr(), 0); }
inline void MemHandle::take_ownership(MemHandle &from) { from.f_04 = 0; f_04 = 1; }
inline int16_t MemHandle::is_valid(void) { return f_00 != 0; }
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline void GraphicPoint::set_position(int16_t x, int16_t y) { f_02 = x; f_04 = y; }
inline void GraphicPoint::move_position(int16_t dx, int16_t dy) { f_02 += dx; f_04 += dy; }
inline void Shape::set_frame(int16_t frame) { f_18 = frame; }

Shape::Shape(void)
{
	f_18 = 0;
}

Shape::Shape(MemHandle &h)
{
	set_shape(h);
}

Shape::Shape(char *data, char unused, uint8_t owned)
{
	set_shape(data, unused, owned);
}

Shape::Shape(MemHandle &h, int16_t x, int16_t y)
{
	set_shape(h);
	set_position(x, y);
}

Shape::Shape(MemHandle &h, int16_t x, int16_t y, int16_t frame)
{
	set_shape(h);
	set_position(x, y);
	set_frame(frame);
}

Shape::Shape(const char *name, int16_t x, int16_t y, int16_t frame)
{
	BaseFile file;

	reset();
	file.open(name, (OpenMode)0);
	shape.alloc(file.size(), 1);
	file.read((char *)shape.f_00, file.size());
	file.close();
	set_position(x, y);
	set_frame(frame);
}

Shape::~Shape(void)
{
	free();
}

void Shape::free(void)
{
	if (shape.f_00)
		shape.free(0);
	if (background.f_00)
		background.free(0);
}

void Shape::reset(void)
{
	shape.free(0);
	background.free(0);
	f_02 = 0;
	f_04 = 0;
	f_18 = 0;
}

void Shape::set_shape(MemHandle &h)
{
	reset();
	shape.init(h);
	shape.take_ownership(h);
}

void Shape::set_shape(char *data, char, uint8_t owned)
{
	reset();
	shape.init(data, owned);
}

Rect Shape::get_rect(int16_t frame)
{
	Rect r;
	int16_t xoff, yoff;

	GetShapeInfo((char *)shape.f_00, frame, (int16_t &)r.f_04, (int16_t &)r.f_06, xoff, yoff);
	r.f_00 = r.f_02 = 0;
	r.f_04--;
	r.f_06--;
	return r;
}

void Shape::init_background(void)
{
	if (shape.is_valid())
	{
		int16_t width = GetMaxShapeWidth((void *)shape.f_00);
		int16_t depth = GetMaxShapeDepth((void *)shape.f_00);
		int16_t size = width * depth;
		background.alloc(size, 0);
		memset((void *)background.f_00, 0x0f, size);
	}
}

void Shape::draw(int16_t frame)
{
	f_18 = frame;
	draw();
}

void Shape::draw(int16_t x, int16_t y)
{
	SkipDraw(GlobalVport::global_ptr, x, y, GetShapeAddress((void *)shape.f_00, f_18));
}

void Shape::draw(void)
{
	Shape::draw(f_02, f_04);
}

// Saves the screen under the shape.
void Shape::hide(void)
{
	if (!background.f_00)
	{
		init_background();
		if (!background.f_00)
			return;
	}
	Rect r(1, 1, 0, 0);
	int16_t xoff, yoff;
	GetShapeInfo((char *)shape.f_00, f_18, (int16_t &)r.f_04, (int16_t &)r.f_06, xoff, yoff);
	r.moveto(f_02 - xoff, f_04 - yoff);
	SaveRect(GlobalVport::global_ptr, (void *)background.f_00, &r);
	savedX = f_02;
	savedY = f_04;
}

void Shape::move(int16_t dx, int16_t dy)
{
	restore(f_02, f_04);
	move_position(dx, dy);
	hide();
	draw();
}

void Shape::move(int16_t frame, int16_t dx, int16_t dy)
{
	restore(f_02, f_04);
	move_position(dx, dy);
	set_frame(frame);
	hide();
	draw();
}

void Shape::restore(int16_t, int16_t)
{
	if (background.f_00)
	{
		Rect r(1, 1, 0, 0);
		int16_t xoff, yoff;
		GetShapeInfo((char *)shape.f_00, f_18, (int16_t &)r.f_04, (int16_t &)r.f_06, xoff, yoff);
		r.moveto(f_02 - xoff, f_04 - yoff);
		RestoreRect(GlobalVport::global_ptr, (void *)background.f_00, &r);
	}
}

void Shape::move_absolute(int16_t x, int16_t y)
{
	restore(f_02, f_04);
	set_position(x, y);
	hide();
	draw();
}

void Shape::clear_back(void)
{
	background.free(0);
}

void GraphicPoint::draw(void)
{
}

void GraphicPoint::hide(void)
{
}

void GraphicPoint::move(int16_t dx, int16_t dy)
{
	hide();
	move_position(dx, dy);
	draw();
}

void GraphicPoint::draw(int16_t x, int16_t y)
{
	set_position(x, y);
	draw();
}
