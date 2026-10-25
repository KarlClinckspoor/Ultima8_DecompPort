// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: SBUTTON.C

#include "u8port.h"

#include "crect.h"
#include "dispatch.h"
#include "fvinfo.h"
#include "sbutton.h"
#include "u8gmpshp.h"

// Inline in the shared headers when this file was built.
U8_INLINE GumpShape::GumpShape(int16_t shape) { _frame = 0; this->shape = shape; }
U8_INLINE Rect GumpShape::get_rect(void) { return drawer->get_rect(shape, _frame); }
U8_INLINE void GumpShape::draw(int16_t x, int16_t y) { drawer->draw(x, y, shape, _frame); }
U8_INLINE void *GumpShape::get(void) { return drawer->get(shape); }
U8_INLINE void GumpShape::set_frame(int16_t frame) { _frame = frame; }
U8_INLINE int16_t ShapePushButton::set_frame(int16_t frame) { GumpShape::set_frame(frame); refresh(); return 0; }
U8_INLINE int16_t ShapeCycleButton::set_frame(int16_t frame) { GumpShape::set_frame(frame); refresh(); CycleButton::_frame = frame; return 0; }
U8_INLINE int16_t ShapeActivatorButton::set_frame(int16_t frame) { GumpShape::set_frame(frame); refresh(); return 0; }
// Out of line: a virtual function, which the class's vtable needs in every build.
void ShapeCycleButton::unselect(void) { set_frame(CycleButton::_frame); }

ShapePushButton::ShapePushButton(uint8_t id, NewGump *parent, int16_t shape) :
	PushButton(id, parent), GumpShape(shape)
{
	rect = get_rect();
}

void ShapePushButton::draw(int16_t x, int16_t y)
{
	GumpShape::draw(x, y);
}

void ShapePushButton::select(void)
{
	set_frame(1);
}

void ShapePushButton::unselect(void)
{
	set_frame(0);
}

ShapeCycleButton::ShapeCycleButton(uint8_t id, NewGump *parent, int16_t shape) :
	CycleButton(id, parent), GumpShape(shape)
{
	f_37 = GetNumShapes(get());
	rect = get_rect();
}

void ShapeCycleButton::draw(int16_t x, int16_t y)
{
	GumpShape::draw(x, y);
}

// Each state has a lit and an unlit frame.
void ShapeCycleButton::select(void)
{
	set_frame((CycleButton::_frame + 3) % (f_37 * 2));
}

ShapeActivatorButton::ShapeActivatorButton(uint8_t id, NewGump *parent, int16_t shape) :
	ActivatorButton(id, parent), GumpShape(shape)
{
	rect = get_rect();
}

void ShapeActivatorButton::draw(int16_t x, int16_t y)
{
	GumpShape::draw(x, y);
}

void ShapeActivatorButton::select(void)
{
	set_frame(1);
}

void ShapeActivatorButton::unselect(void)
{
	set_frame(0);
}
