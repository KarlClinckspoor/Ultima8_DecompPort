// flags: -P -2 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: INTLIST.C

#include "u8port.h"

#include "u8process.h"
#include "kernel.h"
#include "cfile.h"
#include "u8stdint.h"

// Keeps a poll in progress pointing at the right entry when a pid leaves the list.
void IntList::removeFromList(uint16_t pid)
{
	int16_t index = List::removeFromList(pid);

	if (isAllocated())
	{
		if (current == index)
		{
			current--;
			changed = 1;
		}
		else if (current > index)
			current--;
		last--;
		return;
	}
	last = 0;
	current = 0;
	changed = 1;
}
