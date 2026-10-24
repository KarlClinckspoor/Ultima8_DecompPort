// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: MEM\CMEMHNDP.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "cmemhndp.h"
#include "ferror.h"

void MemHandle::alloc(uint32_t size, uint8_t fatal)
{
	f_00 = (uint32_t)new char[size];
	if (!f_00 && fatal)
		Fatal(0x203);
	f_04 = 1;
	f_05 = size;
}

void MemHandle::alloc(uint32_t size, uint8_t fatal, int16_t error)
{
	f_00 = (uint32_t)new char[size];
	if (!f_00 && fatal)
		Fatal(error);
	f_04 = 1;
	f_05 = size;
}

void MemHandle::alloc(uint32_t size, uint8_t fatal, char *message)
{
	f_00 = (uint32_t)new char[size];
	if (!f_00 && fatal)
		Fatal(message);
	f_04 = 1;
	f_05 = size;
}

void MemHandle::free(int16_t)
{
	if (f_04 && f_00)
		delete (char *)f_00;
	f_00 = 0;
	f_04 = 0;
	f_05 = 0;
}

// Copies this block into dest, allocating dest if it has none.
void MemHandle::copy(MemHandle &dest)
{
	if (f_00)
	{
		if (dest.f_00)
		{
			if (dest.f_05 >= f_05)
				_fmemcpy((void *)dest.f_00, (void *)f_00, f_05);
		}
		else
		{
			dest.alloc(f_05, 1);
			_fmemcpy((void *)dest.f_00, (void *)f_00, f_05);
		}
	}
}
