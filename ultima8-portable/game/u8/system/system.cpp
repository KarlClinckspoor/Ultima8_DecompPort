// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: SYSTEM\SYSTEM.C

#include "u8port.h"

#include "lsystem.h"

void RawSystem::exit_code(void)
{
	free();
}
