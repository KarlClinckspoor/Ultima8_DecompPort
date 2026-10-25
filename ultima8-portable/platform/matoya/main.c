/* Process entry, window and threads.
 *
 * The process main thread runs the window (libmatoya wants it there). The game, U8Main, runs
 * on a game thread of its own, and sound renders on another.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"

#define WINDOW_TITLE "Ultima VIII: Pagan"

static MTY_App *app;
static MTY_Window window = -1;
static MTY_Time start_time;
static _Thread_local bool on_app_thread;
static _Thread_local bool on_game_thread;

static MTY_Atomic32 exit_claimed;
static MTY_Atomic32 exit_requested;
static MTY_Atomic32 exit_code;
static MTY_Atomic32 fatal_claimed;
static MTY_Atomic32 fatal_pending;
static char fatal_text[1024];

static int game_argc;
static char **game_argv;

/* The window's client size, for mapping scripted mouse positions on the game thread. */
static MTY_Mutex *view_lock;
static MTY_Size view_size = {3 * 320, 3 * 240};

MTY_Size backend_view_size(void)
{
	MTY_Size size;

	MTY_MutexLock(view_lock);
	size = view_size;
	MTY_MutexUnlock(view_lock);
	return size;
}

int64_t backend_elapsed_us(void)
{
	if (test_clock_fixed())
		return test_clock_us();
	return (int64_t) (MTY_TimeDiff(start_time, MTY_GetTime()) * 1000.0);
}

void backend_request_exit(int code)
{
	if (MTY_Atomic32CAS(&exit_claimed, 0, 1)) {
		MTY_Atomic32Set(&exit_code, code);
		MTY_Atomic32Set(&exit_requested, 1);
	}
}

/* A thread that asked the app thread to end the process waits here for it. */
static void wait_for_exit(void)
{
	for (;;)
		MTY_Sleep(1000);
}

/* Ends at once, as a DOS program did. The system frees the window, the sound device and the
 * memory; tearing them down first only adds delay and risk (an X11 display closed under a live
 * GL context can corrupt the driver's heap). Static destructors are skipped too: the game thread
 * may still be running. */
static void end_process(int code)
{
	fflush(NULL);
	_Exit(code);
}

void plat_exit(int16_t code)
{
	if (on_app_thread)
		end_process(code);
	if (on_game_thread)
		/* As a DOS exit: the program's stack is left as it is, never unwound. */
		audio_drop_lock();
	backend_request_exit(code);
	wait_for_exit();
}

void plat_fatal(const char *message)
{
	fprintf(stderr, "u8: %s\n", message);
	/* Before main has opened the window (a static constructor), this is the app thread too. */
	if (on_app_thread || (app == NULL && !on_game_thread)) {
		/* A scripted run has no one to close the box. */
		if (MTY_HasDialogs() && !test_input_active())
			MTY_ShowMessageBox(WINDOW_TITLE, "%s", message);
		end_process(1);
	}
	/* Message boxes must be shown from the app thread. */
	if (MTY_Atomic32CAS(&fatal_claimed, 0, 1)) {
		snprintf(fatal_text, sizeof fatal_text, "%s", message);
		MTY_Atomic32Set(&fatal_pending, 1);
	}
	wait_for_exit();
}

void plat_log(const char *text)
{
	fputs(text, stderr);
	fflush(stderr);
}

static bool point_over_image(int32_t x, int32_t y)
{
	int32_t vx, vy, vw, vh;

	video_view_rect(MTY_WindowGetSize(app, window), &vx, &vy, &vw, &vh);
	return x >= vx && y >= vy && x < vx + vw && y < vy + vh;
}

/* A fully transparent 1x1 PNG. Over the game image the OS pointer is this, since the game
 * draws its own; outside the window macOS shows its normal arrow by itself. */
static const uint8_t blank_cursor[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
	0x0b, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x60, 0x00, 0x02, 0x00,
	0x00, 0x05, 0x00, 0x01, 0x7a, 0x5e, 0xab, 0x3f, 0x00, 0x00, 0x00, 0x00,
	0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static bool pointer_over_image;

static void on_event(const MTY_Event *event, void *opaque)
{
	(void) opaque;

	switch (event->type) {
	case MTY_EVENT_CLOSE:
	case MTY_EVENT_QUIT:
	case MTY_EVENT_SHUTDOWN:
		backend_request_exit(0);
		return;
	case MTY_EVENT_KEY:
		/* Alt-Enter toggles fullscreen and never reaches the game. */
		if ((event->key.key == MTY_KEY_ENTER || event->key.key == MTY_KEY_NP_ENTER)
			&& (event->key.mod & MTY_MOD_ALT)) {
			if (event->key.pressed)
				MTY_WindowSetFullscreen(app, window, !MTY_WindowIsFullscreen(app, window));
			return;
		}
		break;
	case MTY_EVENT_MOTION:
		/* The game draws its own pointer over the image. */
		if (!event->motion.relative) {
			bool over = point_over_image(event->motion.x, event->motion.y);

			if (over != pointer_over_image) {
				pointer_over_image = over;
				MTY_AppSetCursor(app, over ? MTY_CURSOR_NONE : MTY_CURSOR_ARROW);
			}
		}
		break;
	default:
		break;
	}
	events_push(event, MTY_WindowGetSize(app, window));
}

/* Scripted runs open no window, so nothing takes focus or switches Spaces and no display is
 * needed. Frames still reach video's shared buffer, where "shot" reads them. */
static void run_without_window(void)
{
	while (!MTY_Atomic32Get(&exit_requested)) {
		if (MTY_Atomic32Get(&fatal_pending)) {
			MTY_Atomic32Set(&exit_code, 1);
			break;
		}
		MTY_Sleep(16);
	}
	end_process(MTY_Atomic32Get(&exit_code));
}

static bool on_frame(void *opaque)
{
	uint32_t x, y;

	(void) opaque;
	if (MTY_Atomic32Get(&fatal_pending)) {
		if (MTY_HasDialogs() && !test_input_active())
			MTY_ShowMessageBox(WINDOW_TITLE, "%s", fatal_text);
		MTY_Atomic32Set(&exit_code, 1);
		return false;
	}
	if (MTY_Atomic32Get(&exit_requested))
		return false;
	MTY_MutexLock(view_lock);
	view_size = MTY_WindowGetSize(app, window);
	MTY_MutexUnlock(view_lock);
	if (events_take_warp(MTY_WindowGetSize(app, window), &x, &y))
		MTY_WindowWarpCursor(app, window, x, y);
	video_draw(app, window);
	/* Presenting does not wait for a display refresh while the window is hidden. */
	if (!MTY_WindowIsVisible(app, window))
		MTY_Sleep(16);
	return true;
}

static void *game_thread(void *opaque)
{
	(void) opaque;
	on_game_thread = true;
	plat_exit(U8Main((int16_t) game_argc, game_argv));
	return NULL;
}

/* Takes the port's own switches out of the arguments the game sees. */
static void show_help(void)
{
	printf("Usage: Ultima8 [--data <dir>] [switches]\n"
		"\n"
		"  --data <dir>   the game folder, with STATIC, USECODE and U8.INI (default: U8_DATA,\n"
		"                 else the current folder if it has STATIC, else the folder Ultima8 is\n"
		"                 in, else Documents/Ultima8)\n"
		"  --adlib        AdLib music, even when MT-32 ROMs are found\n"
		"  --roland       MT-32 music (the default when its ROMs are found)\n"
		"  --help         this list\n"
		"\n"
		"Other switches go to the game, as U8.EXE's did.\n");
	exit(0);
}

static const char *take_data_dir(int argc, char **argv)
{
	const char *dir = getenv("U8_DATA");

	game_argv = calloc((size_t) argc + 1, sizeof *game_argv);
	for (int i = 0; i < argc; i++) {
		if (i > 0 && strcmp(argv[i], "--data") == 0 && i + 1 < argc)
			dir = argv[++i];
		else if (i > 0 && strcmp(argv[i], "--help") == 0)
			show_help();
		else if (i > 0 && strcmp(argv[i], "--adlib") == 0)
			audio_choose_music('a');
		else if (i > 0 && strcmp(argv[i], "--roland") == 0)
			audio_choose_music('r');
		else
			game_argv[game_argc++] = argv[i];
	}
	return dir != NULL && dir[0] != '\0' ? dir : NULL;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

/* The folder the program itself is in; for Ultima8.app, the folder holding the app. */
static bool exe_directory(char *out, size_t size)
{
	char *slash;

#ifdef _WIN32
	DWORD n = GetModuleFileNameA(NULL, out, (DWORD) size);

	if (n == 0 || n >= size)
		return false;
	slash = strrchr(out, '\\');
#elif defined(__APPLE__)
	uint32_t n = (uint32_t) size;

	char *bundle;

	if (_NSGetExecutablePath(out, &n) != 0)
		return false;
	/* ".../Ultima8.app/Contents/MacOS/Ultima8" becomes ".../Ultima8.app". */
	bundle = strstr(out, ".app/Contents/MacOS/");
	if (bundle != NULL)
		bundle[4] = '\0';
	slash = strrchr(out, '/');
#else
	ssize_t n = readlink("/proc/self/exe", out, size - 1);

	if (n <= 0)
		return false;
	out[n] = '\0';
	slash = strrchr(out, '/');
#endif
	if (slash == NULL)
		return false;
	*slash = '\0';
	return true;
}

/* Ultima8 in the user's Documents folder. */
static bool documents_directory(char *out, size_t size)
{
#ifdef _WIN32
	char docs[MAX_PATH];

	if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs) != S_OK)
		return false;
	return snprintf(out, size, "%s\\Ultima8", docs) < (int) size;
#else
	const char *home = getenv("HOME");

	if (home == NULL || home[0] == '\0')
		return false;
	return snprintf(out, size, "%s/Documents/Ultima8", home) < (int) size;
#endif
}

#ifdef _WIN32

/* The exe is a windowed program, so a double-click opens no console. Started from a terminal,
 * its messages go to that terminal; output already redirected to a file or pipe stays there. */
static void use_parent_console(void)
{
	if (!AttachConsole(ATTACH_PARENT_PROCESS))
		return;
	if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN)
		freopen("CONOUT$", "w", stdout);
	if (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_UNKNOWN)
		freopen("CONOUT$", "w", stderr);
}
#else
static void use_parent_console(void)
{
}
#endif

/* The data folder: the one named (--data, else U8_DATA), else the current folder if it has
 * STATIC, else the program's own (a double-click does not always start in the game's folder),
 * else Documents/Ultima8. Documents comes last because macOS asks the player before letting an
 * app look there. Chosen once, by the first file access: the game reads files from its static
 * constructors, before main runs. */
static bool root_chosen;
static char own_dir[1024];

static void find_root(const char *data_dir)
{
	char docs_dir[1024];
	const char *fallback;

	if (data_dir != NULL) {
		files_set_root(data_dir);
		return;
	}
	files_set_root(".");
	if (plat_dir_exists("STATIC"))
		return;
	fallback = exe_directory(own_dir, sizeof own_dir) ? own_dir : ".";
	files_set_root(fallback);
	if (!plat_dir_exists("STATIC") && documents_directory(docs_dir, sizeof docs_dir)) {
		files_set_root(docs_dir);
#ifndef __APPLE__
		/* Found nowhere: report the program's folder, where the game is meant to go. A Mac
		 * install keeps the app in Applications and the game in Documents/Ultima8. */
		if (!plat_dir_exists("STATIC"))
			files_set_root(fallback);
#endif
	}
}

/* Stops with a message saying what is wrong when the folder does not hold the game. */
static void choose_root(const char *data_dir)
{
	char problem[1024];

	if (root_chosen)
		return;
	root_chosen = true;
	find_root(data_dir);
	if (!files_check_data(problem, sizeof problem)) {
		/* macOS runs a downloaded app from a hidden copy until it is moved in Finder. */
		if (strstr(own_dir, "/AppTranslocation/") != NULL)
			snprintf(problem, sizeof problem, "macOS started Ultima8 from a temporary copy, so "
				"it can't see the game files. In Finder, drag Ultima8.app out of the game "
				"folder and back in, then open it again.");
		plat_fatal(problem);
	}
}

#if defined(__APPLE__)
#include <crt_externs.h>
#endif

/* --data or U8_DATA, read before main has its arguments. */
static const char *early_data_dir(void)
{
	static char buffer[8192];
	int argc = 0;
	char *argv[256];
	const char *dir = getenv("U8_DATA");

#ifdef _WIN32
	argc = __argc < 256 ? __argc : 256;
	for (int i = 0; i < argc; i++)
		argv[i] = __argv[i];
#elif defined(__APPLE__)
	argc = *_NSGetArgc() < 256 ? *_NSGetArgc() : 256;
	for (int i = 0; i < argc; i++)
		argv[i] = (*_NSGetArgv())[i];
#else
	FILE *f = fopen("/proc/self/cmdline", "rb");
	size_t n = 0;

	if (f != NULL) {
		n = fread(buffer, 1, sizeof buffer - 1, f);
		fclose(f);
	}
	buffer[n] = '\0';
	for (size_t at = 0; at < n && argc < 256; at += strlen(buffer + at) + 1)
		argv[argc++] = buffer + at;
#endif
	for (int i = 1; i + 1 < argc; i++) {
		if (argv[i] != NULL && strcmp(argv[i], "--data") == 0)
			dir = argv[i + 1];
	}
	return dir != NULL && dir[0] != '\0' ? dir : NULL;
}

void backend_ensure_root(void)
{
	if (!root_chosen)
		choose_root(early_data_dir());
}

int main(int argc, char **argv)
{
	MTY_Frame frame;
	const char *data_dir;

	use_parent_console();
	on_app_thread = true;
	/* 1 ms sleeps on Windows (its default is 15.6 ms), so the game's timers and waits keep
	 * their rates; the process ends without reverting it, which ends it too. */
	MTY_SetTimerResolution(1);
	start_time = MTY_GetTime();
	data_dir = take_data_dir(argc, argv);
	choose_root(data_dir);
	events_init();
	video_init();
	view_lock = MTY_MutexCreate();

	if (test_input_active()) {
		audio_start();
		MTY_ThreadDetach(game_thread, NULL);
		run_without_window();
	}
	app = MTY_AppCreate(0, on_frame, on_event, NULL);
	if (app == NULL)
		plat_fatal("Could not start the window system.");
	frame = MTY_MakeDefaultFrame(0, 0, 3 * 320, 3 * 240, 0.9f);
	window = MTY_WindowCreate(app, WINDOW_TITLE, &frame, 0);
	MTY_AppSetPNGCursor(app, blank_cursor, sizeof blank_cursor, 0, 0);
	MTY_AppSetCursor(app, MTY_CURSOR_ARROW);
	if (window < 0 || !MTY_WindowSetGFX(app, window, MTY_GetDefaultGFX(), true))
		plat_fatal("Could not open the game window.");
	MTY_WindowSetMinSize(app, window, 320, 240);

	audio_start();
	MTY_ThreadDetach(game_thread, NULL);
	MTY_AppRun(app);
	end_process(MTY_Atomic32Get(&exit_code));
	return 0;
}
