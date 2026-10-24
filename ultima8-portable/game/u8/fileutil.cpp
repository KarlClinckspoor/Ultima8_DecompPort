// name: FILEUTIL
// Ported from FILEUTIL.ASM.
// Reads a file's DOS attributes; owns fileerr, the last DOS error of the file routines.

#include "u8port.h"
#include "plat.h"

#include "cfile.h"

int16_t fileerr = 0;

// The DOS error a failed open or create of path would have given: 2 when the file is not
// there, 3 when its folder is not, else 5 (access denied).
int16_t DosOpenError(const char *path)
{
	char dir[MAXPATH];
	const char *slash;

	if (plat_file_exists(path))
		return 5;
	slash = strrchr(path, '\\');
	if (slash == NULL)
		slash = strrchr(path, '/');
	if (slash != NULL && slash != path) {
		size_t n = (size_t)(slash - path);
		if (n >= sizeof dir)
			n = sizeof dir - 1;
		memcpy(dir, path, n);
		dir[n] = 0;
		if (!plat_dir_exists(dir))
			return 3;
	}
	return 2;
}

// int file_attribs(const char far *path)
// Returns the attributes, or the DOS error with bit 15 set.
int16_t file_attribs(const char *path)
{
	fileerr = 0;
	if (plat_dir_exists(path))
		return FA_DIREC;
	if (plat_file_exists(path))
		return FA_ARCH;
	fileerr = DosOpenError(path);
	return (int16_t)(fileerr | 0x8000);
}

// unsigned char file_exists(const char far *path)
// True for a file or a folder, as DOS's get-attributes call found both.
uint8_t file_exists(const char *path)
{
	fileerr = 0;
	if (plat_file_exists(path) || plat_dir_exists(path))
		return 1;
	fileerr = DosOpenError(path);
	return 0;
}
