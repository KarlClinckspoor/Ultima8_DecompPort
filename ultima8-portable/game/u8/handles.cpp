// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: HANDLES.C

#include "u8port.h"

// #include <io.h>  (DOS only)
// #include <fcntl.h>  (DOS only)
#include "appdebug.h"

// Counts free DOS file handles by opening one file (\AUTOEXEC.BAT, from the root of the
// current drive) until it fails. The host has neither the file nor the limit; this gives what
// DOS gave when the file was missing.
int16_t howManyFreeHandles(void)
{
	return 0;
}
