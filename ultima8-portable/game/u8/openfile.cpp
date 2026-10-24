// name: OPENFILE
// Ported from OPENFILE.ASM.
// Opens a file for reading and writing, and closes a file handle.

#include "u8port.h"
#include "plat.h"

#include "cfile.h"

// int OpenFile(const char far *path)
// Returns the handle, or -1.
int16_t OpenFile(const char *path)
{
	int16_t handle;

	fileerr = 0;
	handle = plat_file_open(path, PLAT_FILE_READ | PLAT_FILE_WRITE);
	if (handle == -1)
		fileerr = DosOpenError(path);
	return handle;
}

// int CloseFile(int handle)
int16_t CloseFile(int16_t handle)
{
	fileerr = 0;
	if (handle < 5) {
		fileerr = 6;	// invalid handle
		return 0;
	}
	plat_file_close(handle);
	return 0;
}
