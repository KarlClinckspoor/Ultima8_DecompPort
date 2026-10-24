// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: HANDLES.C

#include "u8port.h"

// #include <io.h>  (DOS only)
// #include <fcntl.h>  (DOS only)
#include "appdebug.h"

// Counts free DOS file handles by opening one file until it fails.
int16_t howManyFreeHandles(void)
{
	int16_t i, count = 0;
	int16_t handles[100];

	for (i = 0; i < 100; i++)
	{
		handles[i] = open("\\autoexec.bat", O_BINARY);
		if (handles[i] == -1)
		{
			count = i;
			break;
		}
	}
	for (i--; i >= 0; i--)
		close(handles[i]);
	return count;
}
