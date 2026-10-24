// name: SEEKFILE
// Ported from SEEKFILE.ASM.
// Moves a file's read/write position.

#include "u8port.h"
#include "plat.h"

#include "cfile.h"

// long SeekFile(int handle, long pos, int from)
// from: 0 from the start, 1 from the current position, 2 from the end. Returns the new
// position, or -1.
int32_t SeekFile(int16_t handle, int32_t pos, int16_t from)
{
	int32_t at;

	fileerr = 0;
	at = plat_file_seek(handle, pos, from);
	if (at == -1)
		fileerr = from > 2 ? 1 : 6;
	return at;
}
