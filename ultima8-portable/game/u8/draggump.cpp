// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: DRAGGUMP.C

#include "u8port.h"

#include "newgump.h"
#include "basecam.h"
#include "cdesc.h"
#include "cvport.h"
#include "ccurrvp.h"
#include "cshape.h"
#include "fwritskp.h"
#include "cexit.h"
#include "dispatch.h"
#include "menu.h"
#include "draggump.h"
#include "cmemhndp.h"

// Inline in the shared headers when this file was built.
U8_INLINE Rect::Rect(void) {}
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE int16_t Rect::width(void) { return f_04 - f_00 + 1; }
U8_INLINE int16_t Rect::height(void) { return f_06 - f_02 + 1; }
U8_INLINE void NewGump::popVisibility(void) { visibilityStack >>= 2; }
U8_INLINE GraphicYtable::GraphicYtable(void) { f_04 = 2; index = 0; }
U8_INLINE Vport::Vport(void) { mem = 0; f_0f = 2; f_10 = 0; }
U8_INLINE Vport::~Vport(void) { free(); }
U8_INLINE Rect *Vport::getRect(void) { return &rect; }
U8_INLINE void MemHandle::init(char *p, uint8_t owned) { f_00 = p; f_04 = owned; f_05 = 0; }
U8_INLINE MemHandle::MemHandle(void) { init(0, 0); }
U8_INLINE MemHandle::~MemHandle(void) { free(0); }
U8_INLINE char *MemHandle::get_ptr(void) { return (char *)f_00; }
U8_INLINE void BaseCamera::setNeedSlam(void) { needSlam = 1; }

// A one-frame shape built in memory: the header, then the frame table.
struct DragShapeHeader
{
	int16_t width;
	int16_t height;
	int16_t frames;
	int32_t offset;
	int16_t size;
};

struct DragFrameHeader
{
	int16_t f_00;
	int16_t f_02;
	int32_t f_04;
};

DrgNewGump::DrgNewGump(NewGumpId id, NewGump *parent) :
	NewGump(id, parent, 2)
{
	dragShape = new Shape;
	if (!dragShape)
		outOfMemory(__FILE__, 42);	// __LINE__ in the original file
	dragging = 0;
}

DrgNewGump::~DrgNewGump(void)
{
	if (dragShape)
	{
		delete dragShape;
		dragShape = 0;
	}
}

// A drag draws the gump once into a shape that then follows the mouse.
void DrgNewGump::commandMouseLeft(Event &event)
{
	if (dragging == 0 && event.isSingle())
	{
		dragging = 1;
		lastX = event.x;
		lastY = event.y;
		f_1d |= 8;
		setExclusive(this);
		pushVisibility(3);
		restore(rect);
		popVisibility();
		Rect saved = rect;
		rect.moveto(0, 0);
		Vport vport;
		*vport.getRect() = rect;
		if (!vport.alloc(0))
			halt(__FILE__, 105);	// __LINE__ in the original file
		vport.clear(GumpColorMap[7]);
		MemHandle mem;
		{
			CurrentVport current(&vport);
			refresh(0, 0, *vport.getRect());
			pushVisibility(3);
			uint16_t size = EstimateFastWriteSkip(vport, rect.f_00, rect.f_02, rect.f_04, rect.f_06, GumpColorMap[7]);
			if (size == 0xFFFF)	// -1, as Borland compared it
				halt(__FILE__, 128);	// __LINE__ in the original file
			int16_t headerSize = 20;
			mem.alloc(size + headerSize, 2);
			int16_t written = FastWriteSkip(vport, rect.f_00, rect.f_02, rect.f_04, rect.f_06, 0, 0, (uint8_t *)mem.get_ptr() + headerSize, GumpColorMap[7]);
			if (size < (uint16_t)written)
				halt(__FILE__, 141);	// __LINE__ in the original file
			DragShapeHeader *header = (DragShapeHeader *)mem.get_ptr();
			header->width = rect.width();
			header->height = rect.height();
			header->frames = 1;
			header->offset = 12;
			header->size = written;
			DragFrameHeader *frame = (DragFrameHeader *)(mem.get_ptr() + 12);
			frame->f_00 = -1;
			frame->f_02 = -1;
			frame->f_04 = -1;
		}
		rect = saved;
		dragShape->set_shape(mem);
		((Point *)&dragShape->f_02)->set(get_dx(), get_dy());
		dragShape->hide();
		commandMouseMovement(event);
		return;
	}
	if (dragging && event.isRelease())
	{
		dragging = 0;
		clrExclusive();
		f_1d &= ~8;
		dragShape->free();
		if (parent)
			parent->newGumpList.moveToTail(this);
		popVisibility();
		refresh();
	}
}

void DrgNewGump::commandMouseMovement(Event &event)
{
	if (dragging)
	{
		int16_t dx = event.x - lastX;
		int16_t dy = event.y - lastY;
		dragShape->move(dx, dy);
		rect.moverel(dx, dy);
		BaseCamera::setNeedSlam();
		lastX = event.x;
		lastY = event.y;
	}
}
