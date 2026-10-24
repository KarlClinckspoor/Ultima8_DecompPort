/* The parts of Borland's <dir.h> and <io.h> the game uses, on the platform's files.
 *
 * Paths stay the game's DOS paths ("GAMEDAT\\NONFIXED.DAT"); the platform resolves them in
 * the data folder in any letter case.
 */
#ifndef U8DOS_H
#define U8DOS_H

#include "u8port.h"

#define MAXPATH 80
#define MAXDRIVE 3
#define MAXDIR 66
#define MAXFILE 9
#define MAXEXT 5

/* fnsplit's result flags. */
#define WILDCARDS 0x01
#define EXTENSION 0x02
#define FILENAME 0x04
#define DIRECTORY 0x08
#define DRIVE 0x10

/* File attributes. */
#define FA_RDONLY 0x01
#define FA_HIDDEN 0x02
#define FA_SYSTEM 0x04
#define FA_LABEL 0x08
#define FA_DIREC 0x10
#define FA_ARCH 0x20

/* Borland's search record. ff_reserved names the search's slot here. */
struct ffblk {
	char ff_reserved[21];
	char ff_attrib;
	uint16_t ff_ftime;
	uint16_t ff_fdate;
	int32_t ff_fsize;
	char ff_name[13];
};

#ifdef __cplusplus
extern "C" {
#endif

/* 0 when a file was found, -1 when none (the DOS error code is not kept). */
int16_t findfirst(const char *pattern, struct ffblk *found, int16_t attrib);
int16_t findnext(struct ffblk *found);
int16_t fnsplit(const char *path, char *drive, char *dir, char *name, char *ext);
void fnmerge(char *path, const char *drive, const char *dir, const char *name, const char *ext);
/* The current directory of a drive: always the data folder's root, so "". */
int16_t getcurdir(int16_t drive, char *directory);
int16_t getdisk(void);
/* Drives: the data folder is drive C:, the only one. 1 is A:. */
int16_t _getdrive(void);
void _dos_getdrive(uint16_t *drive);
int16_t _chdrive(int16_t drive);
char *_getdcwd(int16_t drive, char *buffer, int16_t size);
int16_t u8_unlink(const char *path);
int16_t u8_mkdir(const char *path);
int16_t u8_rmdir(const char *path);
int16_t u8_rename(const char *from, const char *to);

#ifdef __cplusplus
}
#endif

#define unlink u8_unlink
#define mkdir u8_mkdir
#define rmdir u8_rmdir
#define rename u8_rename

#endif
