/* Borland's directory and file-name calls on the platform's files. */
#include "u8port.h"
#include "plat.h"

/* Searches in progress. A search record names its slot in ff_reserved; the slots are reused
 * oldest first, since the game often stops a search without saying so. */
#define SEARCH_SLOTS 16

static plat_find searches[SEARCH_SLOTS];
static bool search_open[SEARCH_SLOTS];
static uint32_t search_age[SEARCH_SLOTS];
static uint32_t search_clock;

static void fill(struct ffblk *found, const plat_find *f)
{
	memset(found->ff_name, 0, sizeof found->ff_name);
	strncpy(found->ff_name, f->name, 12);
	found->ff_attrib = (char) f->attrib;
	found->ff_fsize = (int32_t) f->size;
	found->ff_fdate = (uint16_t) (f->time >> 16);
	found->ff_ftime = (uint16_t) f->time;
}

int16_t findfirst(const char *pattern, struct ffblk *found, int16_t attrib)
{
	int slot = 0;

	for (int i = 0; i < SEARCH_SLOTS; i++) {
		if (!search_open[i]) {
			slot = i;
			break;
		}
		if (search_age[i] < search_age[slot])
			slot = i;
	}
	if (search_open[slot]) {
		plat_find_close(&searches[slot]);
		search_open[slot] = false;
	}
	memset(found, 0, sizeof *found);
	found->ff_reserved[0] = (char) (slot + 1);
	/* Plain files always; directories when asked for, as DOS did. */
	if (!plat_find_first(pattern, (attrib & FA_DIREC) != 0, &searches[slot]))
		return -1;
	search_open[slot] = true;
	search_age[slot] = ++search_clock;
	fill(found, &searches[slot]);
	return 0;
}

int16_t findnext(struct ffblk *found)
{
	int slot = found->ff_reserved[0] - 1;

	if (slot < 0 || slot >= SEARCH_SLOTS || !search_open[slot])
		return -1;
	search_age[slot] = ++search_clock;
	if (!plat_find_next(&searches[slot])) {
		search_open[slot] = false;
		return -1;
	}
	fill(found, &searches[slot]);
	return 0;
}

/* Splits "C:\\DIR\\NAME.EXT" into its parts, each with its separator as Borland kept it: the
 * drive with its colon, the directory with its trailing backslash, the extension with its dot. */
int16_t fnsplit(const char *path, char *drive, char *dir, char *name, char *ext)
{
	int16_t flags = 0;
	const char *p = path;
	const char *last_sep, *dot;
	char buffer[MAXPATH];

	if (drive)
		drive[0] = 0;
	if (dir)
		dir[0] = 0;
	if (name)
		name[0] = 0;
	if (ext)
		ext[0] = 0;
	while (*p == ' ')
		p++;
	snprintf(buffer, sizeof buffer, "%s", p);
	p = buffer;
	if (p[0] && p[1] == ':') {
		flags |= DRIVE;
		if (drive) {
			drive[0] = p[0];
			drive[1] = ':';
			drive[2] = 0;
		}
		p += 2;
	}
	last_sep = NULL;
	for (const char *q = p; *q; q++)
		if (*q == '\\' || *q == '/')
			last_sep = q;
	if (last_sep) {
		size_t n = (size_t) (last_sep - p + 1);

		flags |= DIRECTORY;
		if (n > MAXDIR - 1)
			n = MAXDIR - 1;
		if (dir) {
			memcpy(dir, p, n);
			dir[n] = 0;
		}
		p = last_sep + 1;
	}
	dot = strrchr(p, '.');
	if (dot && dot != p && strcmp(p, "..") != 0) {
		flags |= EXTENSION;
		if (ext)
			snprintf(ext, MAXEXT, "%s", dot);
	} else
		dot = NULL;
	{
		size_t n = dot ? (size_t) (dot - p) : strlen(p);

		if (n > 0)
			flags |= FILENAME;
		if (n > MAXFILE - 1)
			n = MAXFILE - 1;
		if (name) {
			memcpy(name, p, n);
			name[n] = 0;
		}
	}
	if (strpbrk(path, "*?"))
		flags |= WILDCARDS;
	return flags;
}

void fnmerge(char *path, const char *drive, const char *dir, const char *name, const char *ext)
{
	path[0] = 0;
	if (drive && *drive) {
		strncat(path, drive, 2);
		if (path[1] != ':')
			strcat(path, ":");
	}
	if (dir && *dir) {
		strcat(path, dir);
		size_t n = strlen(path);
		if (path[n - 1] != '\\' && path[n - 1] != '/')
			strcat(path, "\\");
	}
	if (name)
		strcat(path, name);
	if (ext && *ext) {
		if (*ext != '.')
			strcat(path, ".");
		strcat(path, ext);
	}
}

int16_t getcurdir(int16_t drive, char *directory)
{
	(void) drive;
	directory[0] = 0;
	return 0;
}

/* Drive C:. */
int16_t getdisk(void)
{
	return 2;
}

int16_t u8_unlink(const char *path)
{
	return plat_file_remove(path) ? 0 : -1;
}

int16_t u8_mkdir(const char *path)
{
	return plat_dir_create(path) ? 0 : -1;
}

int16_t u8_rmdir(const char *path)
{
	return plat_dir_remove(path) ? 0 : -1;
}

int16_t u8_rename(const char *from, const char *to)
{
	return plat_file_rename(from, to) ? 0 : -1;
}

int16_t _getdrive(void)
{
	return 3;
}

void _dos_getdrive(uint16_t *drive)
{
	*drive = 3;
}

int16_t _chdrive(int16_t drive)
{
	return drive == 3 ? 0 : -1;
}

char *_getdcwd(int16_t drive, char *buffer, int16_t size)
{
	(void) drive;
	snprintf(buffer, (size_t) size, "C:\\");
	return buffer;
}

static void to_find_t(const struct ffblk *f, struct find_t *found)
{
	memcpy(found->reserved, f->ff_reserved, sizeof found->reserved);
	found->attrib = f->ff_attrib;
	found->wr_time = f->ff_ftime;
	found->wr_date = f->ff_fdate;
	found->size = f->ff_fsize;
	memcpy(found->name, f->ff_name, sizeof found->name);
}

uint16_t _dos_findfirst(const char *pattern, uint16_t attrib, struct find_t *found)
{
	struct ffblk f;

	if (findfirst(pattern, &f, (int16_t) attrib) != 0)
		return 18;
	to_find_t(&f, found);
	return 0;
}

uint16_t _dos_findnext(struct find_t *found)
{
	struct ffblk f;

	memcpy(f.ff_reserved, found->reserved, sizeof f.ff_reserved);
	if (findnext(&f) != 0)
		return 18;
	to_find_t(&f, found);
	return 0;
}
