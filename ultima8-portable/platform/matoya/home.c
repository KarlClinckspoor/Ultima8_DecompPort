/* The port's home folder, its settings file and the game's saves.
 *
 * The home folder is ~/.u8port (on Windows %APPDATA%\u8port), or the folder named by --home or
 * U8_HOME. It holds:
 *
 *   u8port.cfg    the settings, as key=value lines (settings.c shows them)
 *   GAMEDAT/      the game in progress and
 *   SAVEGAME/     the saved games, which the game wrote into its own folder on DOS; the first
 *                 start copies both from the game folder, which is not written to after that
 *   recordings/   the black box's sessions (record.c)
 *
 * Folder helpers for the recordings are here too.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <shlobj.h>
#define strcasecmp _stricmp
#else
#include <dirent.h>
#include <strings.h>
#include <unistd.h>
#endif

#include "backend.h"

#define MAX_SETTINGS 64

static char home[PATH_SIZE];
static bool home_ready;

typedef struct {
	char key[32];
	char value[PATH_SIZE];
} setting;

static setting settings[MAX_SETTINGS];
static int setting_count;
static bool settings_loaded;

/* ---- Folders ---- */

bool path_is_dir(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

bool path_is_file(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
}

bool path_join(char *out, size_t size, const char *dir, const char *name)
{
	int n = snprintf(out, size, "%s/%s", dir, name);

	return n > 0 && (size_t) n < size;
}

bool path_make_dir(const char *path)
{
	if (path_is_dir(path))
		return true;
#ifdef _WIN32
	return _mkdir(path) == 0;
#else
	return mkdir(path, 0777) == 0;
#endif
}

/* Makes the folder and any missing parents. */
bool path_make_dirs(const char *path)
{
	char partial[PATH_SIZE];

	snprintf(partial, sizeof partial, "%s", path);
	for (char *p = partial + 1; *p; p++) {
		if (*p == '/' || *p == '\\') {
			char c = *p;

			*p = '\0';
			/* "C:" on its own is a drive, not a folder to make. */
			if (!(strlen(partial) == 2 && partial[1] == ':'))
				path_make_dir(partial);
			*p = c;
		}
	}
	return path_make_dir(partial);
}

/* The names in a folder, sorted, without . and ..; free with path_free_list. */
char **path_list(const char *dir, int *count)
{
	char **names = NULL;
	int n = 0;

#ifdef _WIN32
	char pattern[PATH_SIZE];
	WIN32_FIND_DATAA data;
	HANDLE h;

	path_join(pattern, sizeof pattern, dir, "*");
	h = FindFirstFileA(pattern, &data);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0)
				continue;
			names = realloc(names, (size_t) (n + 1) * sizeof *names);
			names[n++] = strdup(data.cFileName);
		} while (FindNextFileA(h, &data));
		FindClose(h);
	}
#else
	DIR *d = opendir(dir);
	struct dirent *entry;

	while (d != NULL && (entry = readdir(d)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		names = realloc(names, (size_t) (n + 1) * sizeof *names);
		names[n++] = strdup(entry->d_name);
	}
	if (d != NULL)
		closedir(d);
#endif
	for (int i = 1; i < n; i++) {
		for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
			char *t = names[j];

			names[j] = names[j - 1];
			names[j - 1] = t;
		}
	}
	*count = n;
	return names;
}

void path_free_list(char **names, int count)
{
	for (int i = 0; i < count; i++)
		free(names[i]);
	free(names);
}

bool path_copy_file(const char *from, const char *to)
{
	FILE *in = fopen(from, "rb");
	FILE *out;
	char buffer[65536];
	size_t n;
	bool ok = true;

	if (in == NULL)
		return false;
	out = fopen(to, "wb");
	if (out == NULL) {
		fclose(in);
		return false;
	}
	while ((n = fread(buffer, 1, sizeof buffer, in)) > 0) {
		if (fwrite(buffer, 1, n, out) != n) {
			ok = false;
			break;
		}
	}
	fclose(in);
	if (fclose(out) != 0)
		ok = false;
	return ok;
}

/* Copies a folder and everything in it. Links are copied as what they point to. */
bool path_copy_tree(const char *from, const char *to)
{
	int count;
	char **names;
	bool ok;

	if (!path_make_dirs(to))
		return false;
	names = path_list(from, &count);
	ok = true;
	for (int i = 0; i < count && ok; i++) {
		char a[PATH_SIZE], b[PATH_SIZE];

		path_join(a, sizeof a, from, names[i]);
		path_join(b, sizeof b, to, names[i]);
		if (path_is_dir(a))
			ok = path_copy_tree(a, b);
		else if (path_is_file(a))
			ok = path_copy_file(a, b);
	}
	path_free_list(names, count);
	return ok;
}

/* Removes a folder and everything in it. */
bool path_remove_tree(const char *path)
{
	int count;
	char **names;

	if (!path_is_dir(path))
		return remove(path) == 0;
	names = path_list(path, &count);
	for (int i = 0; i < count; i++) {
		char child[PATH_SIZE];

		path_join(child, sizeof child, path, names[i]);
		if (path_is_dir(child))
			path_remove_tree(child);
		else
			remove(child);
	}
	path_free_list(names, count);
#ifdef _WIN32
	return _rmdir(path) == 0;
#else
	return rmdir(path) == 0;
#endif
}

/* ---- The home folder ---- */

static bool default_home(char *out, size_t size)
{
#ifdef _WIN32
	char appdata[MAX_PATH];

	if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK)
		return false;
	return snprintf(out, size, "%s\\u8port", appdata) < (int) size;
#else
	const char *user = getenv("HOME");

	if (user == NULL || user[0] == '\0')
		return false;
	return snprintf(out, size, "%s/.u8port", user) < (int) size;
#endif
}

/* The folder named by --home or U8_HOME (options.c), else the default. Made if missing. */
const char *home_dir(void)
{
	if (!home_ready) {
		const char *chosen = options()->home;

		home_ready = true;
		if (chosen != NULL)
			snprintf(home, sizeof home, "%s", chosen);
		else if (!default_home(home, sizeof home))
			snprintf(home, sizeof home, "u8port");
		path_make_dirs(home);
	}
	return home;
}

/* ---- Settings ---- */

static void settings_path(char *out, size_t size)
{
	path_join(out, size, home_dir(), "u8port.cfg");
}

static void load_settings(void)
{
	char path[PATH_SIZE], line[PATH_SIZE + 64];
	FILE *f;

	settings_loaded = true;
	settings_path(path, sizeof path);
	f = fopen(path, "r");
	if (f == NULL)
		return;
	while (fgets(line, sizeof line, f) != NULL && setting_count < MAX_SETTINGS) {
		char *eq = strchr(line, '=');
		size_t n;

		if (line[0] == '#' || eq == NULL)
			continue;
		*eq = '\0';
		n = strlen(eq + 1);
		while (n > 0 && (eq[n] == '\n' || eq[n] == '\r'))
			eq[n--] = '\0';
		snprintf(settings[setting_count].key, sizeof settings[0].key, "%s", line);
		snprintf(settings[setting_count].value, sizeof settings[0].value, "%s", eq + 1);
		setting_count++;
	}
	fclose(f);
}

static void save_settings(void)
{
	char path[PATH_SIZE];
	FILE *f;

	settings_path(path, sizeof path);
	f = fopen(path, "w");
	if (f == NULL)
		return;
	fprintf(f, "# Ultima VIII portable settings; the settings screen (F11) writes this file.\n");
	for (int i = 0; i < setting_count; i++)
		fprintf(f, "%s=%s\n", settings[i].key, settings[i].value);
	fclose(f);
}

const char *setting_get(const char *key)
{
	if (!settings_loaded)
		load_settings();
	for (int i = 0; i < setting_count; i++) {
		if (strcmp(settings[i].key, key) == 0)
			return settings[i].value;
	}
	return NULL;
}

int setting_int(const char *key, int fallback, int low, int high)
{
	const char *v = setting_get(key);
	char *end;
	long n;

	if (v == NULL || *v == '\0')
		return fallback;
	n = strtol(v, &end, 10);
	if (*end != '\0' || n < low || n > high)
		return fallback;
	return (int) n;
}

bool setting_bool(const char *key, bool fallback)
{
	const char *v = setting_get(key);

	if (v == NULL)
		return fallback;
	if (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 || strcasecmp(v, "on") == 0)
		return true;
	if (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0 || strcasecmp(v, "off") == 0)
		return false;
	return fallback;
}

/* Sets the value and writes the file at once; NULL removes the key. */
void setting_set(const char *key, const char *value)
{
	int i;

	if (!settings_loaded)
		load_settings();
	for (i = 0; i < setting_count; i++) {
		if (strcmp(settings[i].key, key) == 0)
			break;
	}
	if (value == NULL) {
		if (i < setting_count) {
			memmove(&settings[i], &settings[i + 1], (size_t) (setting_count - i - 1) * sizeof *settings);
			setting_count--;
		}
	} else {
		if (i == setting_count) {
			if (setting_count == MAX_SETTINGS)
				return;
			setting_count++;
			snprintf(settings[i].key, sizeof settings[i].key, "%s", key);
		}
		snprintf(settings[i].value, sizeof settings[i].value, "%s", value);
	}
	save_settings();
}

void setting_set_int(const char *key, int value)
{
	char text[16];

	snprintf(text, sizeof text, "%d", value);
	setting_set(key, text);
}

void setting_set_bool(const char *key, bool value)
{
	setting_set(key, value ? "1" : "0");
}

/* ---- The game's saves ---- */

/* The folder that holds GAMEDAT and SAVEGAME: the home folder, a replay's working copy, or
 * (with --saves-in-data, and for scripted tests) the game folder itself. */
static char save_root[PATH_SIZE];

const char *saves_root(void)
{
	return save_root[0] ? save_root : NULL;
}

void saves_set_root(const char *dir)
{
	snprintf(save_root, sizeof save_root, "%s", dir ? dir : "");
}

/* Copies GAMEDAT and SAVEGAME (spelt in any case in from) into the folder to, as GAMEDAT and
 * SAVEGAME; either may be missing. */
bool saves_copy(const char *from, const char *to)
{
	const char *dirs[2] = {"GAMEDAT", "SAVEGAME"};
	int count;
	char **names;
	bool ok = path_make_dirs(to);

	names = path_list(from, &count);
	for (int i = 0; i < 2 && ok; i++) {
		for (int j = 0; j < count; j++) {
			char a[PATH_SIZE], b[PATH_SIZE];

			if (strcasecmp(names[j], dirs[i]) != 0)
				continue;
			path_join(a, sizeof a, from, names[j]);
			path_join(b, sizeof b, to, dirs[i]);
			if (path_is_dir(a) && !path_copy_tree(a, b))
				ok = false;
			break;
		}
	}
	path_free_list(names, count);
	return ok;
}

/* The first start copies the game folder's GAMEDAT and SAVEGAME (the game in progress and the
 * saved games, the shipped starting save among them) to the save root. */
void saves_prepare(const char *game_root)
{
	char probe[PATH_SIZE];

	if (save_root[0] == '\0' || strcmp(save_root, game_root) == 0)
		return;
	path_make_dirs(save_root);
	path_join(probe, sizeof probe, save_root, "SAVEGAME");
	if (path_is_dir(probe))
		return;
	saves_copy(game_root, save_root);
}
