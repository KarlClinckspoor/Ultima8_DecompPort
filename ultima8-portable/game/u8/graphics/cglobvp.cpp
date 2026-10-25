// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CGLOBVP.C

#include "u8port.h"

#include "cglobvp.h"
#include "cvport.h"

U8_INLINE Point::Point(void) {}
U8_INLINE Rect::Rect(void) {}
U8_INLINE GraphicYtable::GraphicYtable(void) { f_04 = 2; index = 0; }
U8_INLINE Vport::Vport(void) { mem = 0; f_0f = 2; f_10 = 0; }

Vport *GlobalVport::main_screen = 0;

void GlobalVport::create_main_screen(void)
{
	if (!main_screen)
	{
		main_screen = new Vport;
		main_screen->map_to_screen(0);
		global_ptr = main_screen;
	}
}
