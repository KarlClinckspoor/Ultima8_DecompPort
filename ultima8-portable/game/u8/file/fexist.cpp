// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FILE\FEXIST.C

#include "u8port.h"

// #include <dir.h>  (DOS only)
#include "fexist.h"

uint8_t FileExists(const char *name)
{
	struct ffblk ff;
	return !findfirst(name, &ff, 0);
}
