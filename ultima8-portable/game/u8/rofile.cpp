// name: ROFILE
// Ported from ROFILE.ASM.
// Opens a file in a given DOS access mode.

#include "u8port.h"
#include "plat.h"

#include "cfile.h"

// int SafeOpenFile(const char far *path, int mode)
// mode is the DOS open mode: access in the low bits (0 read, 1 write, 2 both), sharing in
// the high nibble (no meaning on the host). Returns the handle, or -1.
int16_t SafeOpenFile(const char *path, int16_t mode)
{
	int16_t handle;
	int16_t access;

	fileerr = 0;
	switch (mode & 7) {
	case 0:
		access = PLAT_FILE_READ;
		break;
	case 1:
		access = PLAT_FILE_WRITE;
		break;
	case 2:
		access = PLAT_FILE_READ | PLAT_FILE_WRITE;
		break;
	default:
		fileerr = 12;	// invalid access code
		return -1;
	}
	handle = plat_file_open(path, access);
	if (handle == -1)
		fileerr = DosOpenError(path);
	return handle;
}
