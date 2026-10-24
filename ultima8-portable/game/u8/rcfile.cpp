// name: RCFILE
// Ported from RCFILE.ASM.
// Creates a file, or truncates an existing one.

#include "u8port.h"
#include "plat.h"

#include "cfile.h"

// int SafeCreateFile(const char far *path, int attribs)
// The DOS attributes (read-only, hidden, ...) have no meaning on the host. Returns the handle,
// open for reading and writing, or -1.
int16_t SafeCreateFile(const char *path, int16_t)
{
	int16_t handle;

	fileerr = 0;
	handle = plat_file_create(path);
	if (handle == -1)
		fileerr = DosOpenError(path);
	return handle;
}
