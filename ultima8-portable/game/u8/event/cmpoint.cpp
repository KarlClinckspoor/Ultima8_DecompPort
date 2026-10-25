// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: EVENT\CMPOINT.C

#include "u8port.h"

#include "cexit.h"
#include "systimer.h"
#include "cmpoint.h"
#include "dispatch.h"

// Inline in the shared headers when this file was built.
U8_INLINE MouseButtonStatus::MouseButtonStatus(void) {}
U8_INLINE MouseEvent::MouseEvent(void) { _action = 0; }

uint16_t MousePointer::old_x = 0;
uint16_t MousePointer::old_y = 0;
uint8_t MousePointer::visible = 0;
uint16_t MousePointer::catSP = 0;
CatState MousePointer::cat[5];

MousePointer::MousePointer(void) :
	MouseHandler(1)
{
	visible = 0;
	old_x = 0;
	old_y = 0;
	for (int16_t i = 0; i < 5; i++)
		cat[i] = CAT_FREE;
}

void MousePointer::pushCat(CatState state)
{
	if (catSP < 4)
		cat[++catSP] = state;
	else
		halt(__FILE__, 51);
}

void MousePointer::popCat(void)
{
	if (catSP > 0)
		catSP--;
}
