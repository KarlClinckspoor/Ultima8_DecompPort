/* The game's DOS files, found in the data directory whatever their letter case.
 *
 * "STATIC\\U8SHAPES.FLX" becomes <data>/static/u8shapes.flx when those are the names on disk.
 * Handles are numbered from 5, as DOS's were after its five standard handles.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define strcasecmp _stricmp
#define mkdir(path, mode) _mkdir(path)
#define rmdir _rmdir
#else
#include <dirent.h>
#include <strings.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

#include "backend.h"

#define PATH_SIZE 1024
#define NAME_SIZE 256
#define MAX_HANDLES 64
/* The first handle given out; DOS kept 0-4 for its standard handles. */
#define FIRST_HANDLE 5

enum { LAST_NONE, LAST_READ, LAST_WRITE };

typedef struct {
	FILE *file;
	int last;
} handle;

typedef struct {
	char name[13];
	uint8_t attrib;
	uint32_t size;
	uint32_t time;
} found_file;

typedef struct {
	found_file *files;
	uint32_t count;
	uint32_t next;
} find_list;

static char root[PATH_SIZE] = ".";
static handle handles[MAX_HANDLES + 1];

const char *files_root(void)
{
	return root;
}

void files_set_root(const char *dir)
{
	size_t n;

	snprintf(root, sizeof root, "%s", dir);
	n = strlen(root);
	while (n > 1 && (root[n - 1] == '/' || root[n - 1] == '\\'))
		root[--n] = '\0';
}

static bool is_separator(char c)
{
	return c == '/' || c == '\\';
}

static bool stat_path(const char *path, struct stat *st)
{
	return stat(path, st) == 0;
}

static bool is_file(const char *path)
{
	struct stat st;

	return stat_path(path, &st) && (st.st_mode & S_IFMT) == S_IFREG;
}

static bool is_dir(const char *path)
{
	struct stat st;

	return stat_path(path, &st) && (st.st_mode & S_IFMT) == S_IFDIR;
}

static bool join(char *path, size_t size, const char *dir, const char *name)
{
	int n = snprintf(path, size, "%s/%s", dir, name);

	return n > 0 && (size_t) n < size;
}

/* Finds the name on disk of one entry of dir, ignoring case. */
static bool real_name(const char *dir, const char *name, char *real)
{
	char path[PATH_SIZE];
	bool found = false;

	if (join(path, sizeof path, dir, name) && stat_path(path, &(struct stat) {0})) {
		snprintf(real, NAME_SIZE, "%s", name);
		return true;
	}
#ifndef _WIN32
	DIR *d = opendir(dir);
	struct dirent *entry;

	if (d == NULL)
		return false;
	while (!found && (entry = readdir(d)) != NULL) {
		if (strcasecmp(entry->d_name, name) == 0) {
			snprintf(real, NAME_SIZE, "%s", entry->d_name);
			found = true;
		}
	}
	closedir(d);
#endif
	return found;
}

/* Turns a DOS name into a host path. With create set, a missing last part is allowed and
 * spelt in upper case. */
static bool resolve(const char *dos, bool create, char *path)
{
	const char *p = dos;
	char part[NAME_SIZE];
	char real[NAME_SIZE];

	snprintf(path, PATH_SIZE, "%s", root);
	if (isalpha((unsigned char) p[0]) && p[1] == ':')
		p += 2;
	for (;;) {
		size_t n;
		bool last;

		while (is_separator(*p))
			p++;
		if (*p == '\0')
			return true;
		for (n = 0; p[n] != '\0' && !is_separator(p[n]); n++)
			;
		if (n >= sizeof part)
			return false;
		memcpy(part, p, n);
		part[n] = '\0';
		p += n;
		last = true;
		for (const char *q = p; *q != '\0'; q++) {
			if (!is_separator(*q))
				last = false;
		}

		if (strcmp(part, ".") == 0)
			continue;
		if (strcmp(part, "..") != 0) {
			/* "NAME." is NAME with no extension. */
			while (n > 0 && part[n - 1] == '.')
				part[--n] = '\0';
			if (n == 0)
				continue;
		}
		if (!real_name(path, part, real)) {
			if (!create || !last)
				return false;
			for (size_t i = 0; i <= n; i++)
				real[i] = (char) toupper((unsigned char) part[i]);
		}
		if (strlen(path) + 1 + strlen(real) >= PATH_SIZE)
			return false;
		strcat(path, "/");
		strcat(path, real);
	}
}

/* ---- Handles ---- */

/* Closes whatever the last program left open. */
void files_reset(void)
{
	for (int i = 1; i <= MAX_HANDLES; i++) {
		if (handles[i].file != NULL)
			fclose(handles[i].file);
		handles[i].file = NULL;
		handles[i].last = LAST_NONE;
	}
}

static int16_t add_handle(FILE *file)
{
	if (file == NULL)
		return -1;
	for (int16_t i = 1; i <= MAX_HANDLES; i++) {
		if (handles[i].file == NULL) {
			handles[i].file = file;
			handles[i].last = LAST_NONE;
			return (int16_t) (i + FIRST_HANDLE - 1);
		}
	}
	fclose(file);
	return -1;
}

/* The handle's stream, ready for a read or a write. C needs a seek between the two. */
static FILE *use_handle(int16_t file, int op)
{
	handle *h;

	file = (int16_t) (file - FIRST_HANDLE + 1);
	if (file < 1 || file > MAX_HANDLES || handles[file].file == NULL)
		return NULL;
	h = &handles[file];
	if (op != LAST_NONE && h->last != LAST_NONE && h->last != op)
		fseek(h->file, 0, SEEK_CUR);
	h->last = op;
	return h->file;
}

int16_t plat_file_open(const char *name, int16_t mode)
{
	char path[PATH_SIZE];

	if (!resolve(name, false, path) || !is_file(path))
		return -1;
	return add_handle(fopen(path, (mode & PLAT_FILE_WRITE) ? "r+b" : "rb"));
}

int16_t plat_file_create(const char *name)
{
	char path[PATH_SIZE];

	if (!resolve(name, true, path) || is_dir(path))
		return -1;
	return add_handle(fopen(path, "w+b"));
}

void plat_file_close(int16_t file)
{
	FILE *f = use_handle(file, LAST_NONE);

	if (f != NULL) {
		fclose(f);
		handles[file - FIRST_HANDLE + 1].file = NULL;
	}
}

int32_t plat_file_read(int16_t file, void *buffer, int32_t count)
{
	FILE *f = use_handle(file, LAST_READ);
	size_t n;

	if (f == NULL || count < 0)
		return -1;
	n = fread(buffer, 1, (size_t) count, f);
	return n == 0 && ferror(f) ? -1 : (int32_t) n;
}

int32_t plat_file_write(int16_t file, const void *buffer, int32_t count)
{
	FILE *f = use_handle(file, LAST_WRITE);
	size_t n;

	if (f == NULL || count < 0)
		return -1;
	n = fwrite(buffer, 1, (size_t) count, f);
	return n == 0 && count > 0 ? -1 : (int32_t) n;
}

int32_t plat_file_seek(int16_t file, int32_t offset, int16_t origin)
{
	FILE *f = use_handle(file, LAST_NONE);
	int whence = origin == PLAT_SEEK_CUR ? SEEK_CUR : origin == PLAT_SEEK_END ? SEEK_END : SEEK_SET;

	if (f == NULL || fseek(f, offset, whence) != 0)
		return -1;
	return (int32_t) ftell(f);
}

int32_t plat_file_length(int16_t file)
{
	FILE *f = use_handle(file, LAST_NONE);
	long position, length;

	if (f == NULL)
		return -1;
	position = ftell(f);
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fseek(f, position, SEEK_SET);
	return (int32_t) length;
}

int16_t plat_file_truncate(int16_t file)
{
	FILE *f = use_handle(file, LAST_NONE);
	long position;

	if (f == NULL || fflush(f) != 0 || (position = ftell(f)) < 0)
		return 0;
#ifdef _WIN32
	return _chsize_s(_fileno(f), position) == 0;
#else
	return ftruncate(fileno(f), position) == 0;
#endif
}

/* A host time as DOS kept it: date in the high word (year - 1980, month, day), time in the
 * low (hours, minutes, seconds / 2). */
static uint32_t dos_time(time_t t)
{
	struct tm tm;

#ifdef _WIN32
	if (localtime_s(&tm, &t) != 0)
		return 0;
#else
	if (localtime_r(&t, &tm) == NULL)
		return 0;
#endif
	if (tm.tm_year < 80)
		return 0x00210000u;	/* 1980-01-01 */
	return (uint32_t) ((tm.tm_year - 80) << 9 | (tm.tm_mon + 1) << 5 | tm.tm_mday) << 16
		| (uint32_t) (tm.tm_hour << 11 | tm.tm_min << 5 | tm.tm_sec / 2);
}

uint32_t plat_file_time(const char *name)
{
	char path[PATH_SIZE];
	struct stat st;

	if (!resolve(name, false, path) || !stat_path(path, &st))
		return 0;
	return dos_time(st.st_mtime);
}

uint32_t plat_file_handle_time(int16_t file)
{
	FILE *f = use_handle(file, LAST_NONE);
	struct stat st;

	if (f == NULL || fflush(f) != 0 || fstat(fileno(f), &st) != 0)
		return 0;
	return dos_time(st.st_mtime);
}

/* ---- Names ---- */

int16_t plat_file_exists(const char *name)
{
	char path[PATH_SIZE];

	return resolve(name, false, path) && is_file(path);
}

int16_t plat_dir_exists(const char *name)
{
	char path[PATH_SIZE];

	return resolve(name, false, path) && is_dir(path);
}

int16_t plat_dir_create(const char *name)
{
	char path[PATH_SIZE];

	if (!resolve(name, true, path) || stat_path(path, &(struct stat) {0}))
		return 0;
	return mkdir(path, 0777) == 0;
}

int16_t plat_dir_remove(const char *name)
{
	char path[PATH_SIZE];

	return resolve(name, false, path) && is_dir(path) && rmdir(path) == 0;
}

int16_t plat_file_remove(const char *name)
{
	char path[PATH_SIZE];

	return resolve(name, false, path) && is_file(path) && remove(path) == 0;
}

int16_t plat_file_rename(const char *from, const char *to)
{
	char from_path[PATH_SIZE];
	char to_path[PATH_SIZE];

	/* DOS refused to rename over an existing file. */
	if (!resolve(from, false, from_path) || !resolve(to, true, to_path)
		|| stat_path(to_path, &(struct stat) {0}))
		return 0;
	return rename(from_path, to_path) == 0;
}

uint32_t plat_disk_free(void)
{
	uint64_t bytes = 0;

#ifdef _WIN32
	ULARGE_INTEGER available;

	if (GetDiskFreeSpaceExA(root, &available, NULL, NULL))
		bytes = available.QuadPart;
#else
	struct statvfs fs;

	if (statvfs(root, &fs) == 0)
		bytes = (uint64_t) fs.f_bavail * fs.f_frsize;
#endif
	/* Callers may hold this in a signed long. */
	return bytes > INT32_MAX ? INT32_MAX : (uint32_t) bytes;
}

/* ---- Listings ---- */

/* Splits a name into DOS's blank-padded 8 and 3 character fields; false if it has no
 * 8.3 form. In a pattern, '*' fills the rest of its field with '?'. */
static bool dos_fields(const char *name, bool pattern, char fields[11])
{
	const char *dot = strchr(name, '.');
	size_t base = dot ? (size_t) (dot - name) : strlen(name);
	size_t ext = dot ? strlen(dot + 1) : 0;

	if (base > 8 || ext > 3 || (dot && strchr(dot + 1, '.')))
		return false;
	if (base == 0 && !pattern)
		return false;
	memset(fields, ' ', 11);
	for (int field = 0; field < 2; field++) {
		const char *src = field == 0 ? name : dot ? dot + 1 : "";
		size_t length = field == 0 ? base : ext;
		char *dst = fields + (field == 0 ? 0 : 8);
		size_t width = field == 0 ? 8 : 3;

		for (size_t i = 0; i < length; i++) {
			if (pattern && src[i] == '*') {
				memset(dst + i, '?', width - i);
				break;
			}
			dst[i] = (char) toupper((unsigned char) src[i]);
		}
	}
	return true;
}

static bool fields_match(const char pattern[11], const char name[11])
{
	for (int i = 0; i < 11; i++) {
		if (pattern[i] != '?' && pattern[i] != name[i])
			return false;
	}
	return true;
}

static void dos_name(const char fields[11], char out[13])
{
	int n = 0;

	for (int i = 0; i < 8 && fields[i] != ' '; i++)
		out[n++] = fields[i];
	if (fields[8] != ' ') {
		out[n++] = '.';
		for (int i = 8; i < 11 && fields[i] != ' '; i++)
			out[n++] = fields[i];
	}
	out[n] = '\0';
}

static void add_found(find_list *list, const char fields[11], uint64_t size, bool dir, time_t t)
{
	found_file *f;

	list->files = realloc(list->files, (list->count + 1) * sizeof *list->files);
	f = &list->files[list->count++];
	dos_name(fields, f->name);
	f->size = size > UINT32_MAX ? UINT32_MAX : (uint32_t) size;
	f->attrib = dir ? 0x10 : 0x20;
	f->time = dos_time(t);
}

#ifdef _WIN32
static time_t file_time_to_time_t(FILETIME ft)
{
	uint64_t t = (uint64_t) ft.dwHighDateTime << 32 | ft.dwLowDateTime;

	return (time_t) (t / 10000000u - 11644473600u);
}
#endif

/* Every file in the pattern's directory whose 8.3 name matches, listed up front so
 * deleting files during the listing is safe. */
static find_list *list_matches(const char *dos_pattern, bool dirs)
{
	char dir_dos[PATH_SIZE];
	char dir[PATH_SIZE];
	char want[11], have[11];
	const char *wild = dos_pattern;
	find_list *list;

	for (const char *p = dos_pattern; *p != '\0'; p++) {
		if (is_separator(*p))
			wild = p + 1;
	}
	snprintf(dir_dos, sizeof dir_dos, "%.*s", (int) (wild - dos_pattern), dos_pattern);
	if (!resolve(dir_dos, false, dir) || !is_dir(dir) || !dos_fields(wild, true, want))
		return NULL;

	list = calloc(1, sizeof *list);
#ifdef _WIN32
	char glob[PATH_SIZE];
	WIN32_FIND_DATAA data;
	HANDLE h;

	join(glob, sizeof glob, dir, "*");
	h = FindFirstFileA(glob, &data);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			bool dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

			if ((!dir || dirs) && strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0
				&& dos_fields(data.cFileName, false, have) && fields_match(want, have))
				add_found(list, have, (uint64_t) data.nFileSizeHigh << 32 | data.nFileSizeLow, dir,
					file_time_to_time_t(data.ftLastWriteTime));
		} while (FindNextFileA(h, &data));
		FindClose(h);
	}
#else
	DIR *d = opendir(dir);
	struct dirent *entry;

	while (d != NULL && (entry = readdir(d)) != NULL) {
		char path[PATH_SIZE];
		struct stat st;

		if (!dos_fields(entry->d_name, false, have) || !fields_match(want, have))
			continue;
		if (entry->d_name[0] == '.')
			continue;
		if (join(path, sizeof path, dir, entry->d_name) && stat_path(path, &st)
			&& ((st.st_mode & S_IFMT) == S_IFREG || (dirs && (st.st_mode & S_IFMT) == S_IFDIR)))
			add_found(list, have, (uint64_t) st.st_size, (st.st_mode & S_IFMT) == S_IFDIR, st.st_mtime);
	}
	if (d != NULL)
		closedir(d);
#endif
	return list;
}

void plat_find_close(plat_find *find)
{
	find_list *list = find->state;

	if (list != NULL) {
		free(list->files);
		free(list);
		find->state = NULL;
	}
}

int16_t plat_find_next(plat_find *find)
{
	find_list *list = find->state;

	if (list == NULL)
		return 0;
	if (list->next >= list->count) {
		plat_find_close(find);
		return 0;
	}
	/* As DOS findnext: a shorter name leaves the end of the last one after its NUL. */
	strcpy(find->name, list->files[list->next].name);
	find->size = list->files[list->next].size;
	find->attrib = list->files[list->next].attrib;
	find->time = list->files[list->next].time;
	list->next++;
	return 1;
}

int16_t plat_find_first(const char *pattern, int16_t dirs, plat_find *find)
{
	find->state = list_matches(pattern, dirs != 0);
	return plat_find_next(find);
}

/* ---- Start-up check ---- */

/* Files every game needs. The game stops with its own message when one is missing, but a
 * wrong folder then reads as an unrelated error. */
static const char *const required_files[] = {
	"STATIC\\U8SHAPES.FLX", "STATIC\\U8GUMPS.FLX", "STATIC\\U8FONTS.FLX", "STATIC\\U8PAL.PAL",
	"STATIC\\U8MOUSE.SHP", "STATIC\\TYPEFLAG.DAT", "STATIC\\FIXED.DAT", "STATIC\\GLOB.FLX",
	"STATIC\\ANIM.DAT", "STATIC\\XFORMPAL.DAT", "STATIC\\WPNOVLAY.DAT", "STATIC\\GUMPAGE.DAT",
	"SAVEGAME\\U8SAVE.000",
};

static void append(char *message, size_t size, const char *text)
{
	size_t n = strlen(message);

	if (n < size)
		snprintf(message + n, size - n, "%s", text);
}

#ifdef __APPLE__
#define WHERE_TO_PUT_GAME "Copy your Ultima VIII folder (the one with STATIC and USECODE) into " \
	"Documents and name it Ultima8, or point the game at it with --data <folder> or the " \
	"U8_DATA variable."
#else
#define WHERE_TO_PUT_GAME "Put this program in your Ultima VIII folder (the one with STATIC and " \
	"USECODE), or in Documents/Ultima8, or point the game at it with --data <folder> or the " \
	"U8_DATA variable."
#endif

bool files_check_data(char *message, size_t size)
{
	char missing[512] = "", empty[512] = "";
	int missing_count = 0, empty_count = 0;

	message[0] = '\0';
	if (!is_dir(root)) {
		snprintf(message, size, "The game data folder \"%s\" does not exist.\n\n%s", root,
			WHERE_TO_PUT_GAME);
		return false;
	}
	if (!plat_dir_exists("STATIC")) {
		snprintf(message, size, "\"%s\" is not an Ultima VIII folder: it has no STATIC folder.\n\n%s",
			root, WHERE_TO_PUT_GAME);
		return false;
	}
	for (size_t i = 0; i < sizeof required_files / sizeof *required_files; i++) {
		char name[64];
		int16_t file;
		int32_t length;

		snprintf(name, sizeof name, "%s", required_files[i]);
		file = plat_file_open(name, PLAT_FILE_READ);
		if (file < 0) {
			append(missing, sizeof missing, missing_count++ ? ", " : "");
			append(missing, sizeof missing, required_files[i]);
			continue;
		}
		length = plat_file_length(file);
		plat_file_close(file);
		if (length <= 0) {
			append(empty, sizeof empty, empty_count++ ? ", " : "");
			append(empty, sizeof empty, required_files[i]);
		}
	}
	if (missing_count == 0 && empty_count == 0)
		return true;

	snprintf(message, size, "The Ultima VIII installation in \"%s\" is incomplete.\n", root);
	if (missing_count) {
		append(message, size, "\nMissing: ");
		append(message, size, missing);
		append(message, size, "\n");
	}
	if (empty_count) {
		append(message, size, "\nEmpty: ");
		append(message, size, empty);
		append(message, size, "\n");
	}
	append(message, size, "\nCopy these files from a complete installation.");
	return false;
}
