// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CCURRVP.C

#include "u8port.h"

#include "ccurrvp.h"
#include "cvport.h"

Vport *GlobalVport::global_ptr = 0;

CurrentVport::CurrentVport(Vport *v)
{
	saved = global_ptr;
	set(v);
}

CurrentVport::~CurrentVport(void)
{
	set(saved);
}
