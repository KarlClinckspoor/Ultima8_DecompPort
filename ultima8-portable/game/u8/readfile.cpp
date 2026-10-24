// name: READFILE
// Ported from READFILE.ASM.
// Reads a block of any size from a file. DOS read at most a segment's worth (0FFF0h bytes) at
// a time; the host reads it in one go.

#include "u8port.h"
#include "plat.h"

#include "cfile.h"

uint32_t FileBytesTransferred = 0;

// long ReadFile(int handle, long pos, long count, void far *buffer)
// A pos of -1 reads from the current position. Returns the bytes read, fewer at the end of
// the file, or 0 on an error (fileerr 1 for a NULL buffer).
int32_t ReadFile(int16_t handle, int32_t pos, int32_t count, void *buffer)
{
	int32_t got;

	fileerr = 0;
	if (count == 0)
		return 0;
	FileBytesTransferred = 0;
	if (buffer == NULL) {
		fileerr = 1;
		return 0;
	}
	if (pos != -1 && plat_file_seek(handle, pos, PLAT_SEEK_SET) == -1) {
		fileerr = 6;
		return 0;
	}
	got = plat_file_read(handle, buffer, count);
	if (got < 0) {
		fileerr = 5;
		return 0;
	}
	FileBytesTransferred = (uint32_t)got;
	return got;
}
