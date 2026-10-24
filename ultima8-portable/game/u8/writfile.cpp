// name: WRITFILE
// Ported from WRITFILE.ASM.
// Writes a block of any size to a file (DOS did it a segment's worth at a time).

#include "u8port.h"
#include "plat.h"

#include "cfile.h"

// fileerr when fewer bytes were written than asked: the disk is full.
#define SHORT_WRITE 0x7F

// char WriteFile(int handle, long pos, long count, void far *buffer)
// A pos of -1 writes at the current position. Always returns 1; only fileerr tells of an
// error. A NULL buffer writes nothing and leaves fileerr 0.
char WriteFile(int16_t handle, int32_t pos, int32_t count, void *buffer)
{
	int32_t put;

	fileerr = 0;
	if (buffer == NULL)
		return 1;
	if (pos != -1 && plat_file_seek(handle, pos, PLAT_SEEK_SET) == -1) {
		fileerr = 6;
		return 1;
	}
	put = plat_file_write(handle, buffer, count);
	if (put < 0)
		fileerr = 5;
	else if (put < count)
		fileerr = SHORT_WRITE;
	return 1;
}
