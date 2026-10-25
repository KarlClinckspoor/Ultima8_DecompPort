/* The port's command line.
 *
 * The game reads files from its static constructors, before main runs, and which folders it
 * reads from depends on the command line (--data, --home, --replay). So the arguments are read
 * from the system the first time they are needed, wherever that is, and main uses the same
 * reading. The port's own switches are taken out; the rest go to the game.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <crt_externs.h>
#endif

#include "backend.h"

#define MAX_ARGS 256

static port_options parsed;
static bool parsed_ready;
static char *args[MAX_ARGS];
static int arg_count;

static void system_args(void)
{
#ifdef _WIN32
	arg_count = __argc < MAX_ARGS ? __argc : MAX_ARGS;
	for (int i = 0; i < arg_count; i++)
		args[i] = __argv[i];
#elif defined(__APPLE__)
	arg_count = *_NSGetArgc() < MAX_ARGS ? *_NSGetArgc() : MAX_ARGS;
	for (int i = 0; i < arg_count; i++)
		args[i] = (*_NSGetArgv())[i];
#else
	static char buffer[16384];
	FILE *f = fopen("/proc/self/cmdline", "rb");
	size_t n = 0;

	if (f != NULL) {
		n = fread(buffer, 1, sizeof buffer - 1, f);
		fclose(f);
	}
	buffer[n] = '\0';
	for (size_t at = 0; at < n && arg_count < MAX_ARGS; at += strlen(buffer + at) + 1)
		args[arg_count++] = buffer + at;
#endif
}

static const char *nonempty(const char *s)
{
	return s != NULL && s[0] != '\0' ? s : NULL;
}

static void parse(void)
{
	port_options *o = &parsed;

	parsed_ready = true;
	system_args();
	o->data = nonempty(getenv("U8_DATA"));
	o->home = nonempty(getenv("U8_HOME"));
	o->game_argv = calloc((size_t) arg_count + 1, sizeof *o->game_argv);
	if (arg_count > 0)
		o->game_argv[o->game_argc++] = args[0];
	for (int i = 1; i < arg_count; i++) {
		const char *a = args[i];
		const char *next = i + 1 < arg_count ? args[i + 1] : NULL;

		if (strcmp(a, "--data") == 0 && next) {
			o->data = args[++i];
			o->data_switch = true;
		} else if (strcmp(a, "--home") == 0 && next) {
			o->home = args[++i];
		} else if (strcmp(a, "--record") == 0 && next) {
			o->record = args[++i];
		} else if (strcmp(a, "--replay") == 0 && next) {
			o->replay = args[++i];
		} else if (strcmp(a, "--replay-view") == 0) {
			o->replay_view = true;
		} else if (strcmp(a, "--shots") == 0 && next) {
			o->shots = args[++i];
		} else if (strcmp(a, "--shot-every") == 0 && next) {
			o->shot_every = atoi(args[++i]);
		} else if (strcmp(a, "--no-recording") == 0) {
			o->no_recording = true;
		} else if (strcmp(a, "--saves-in-data") == 0) {
			o->saves_in_data = true;
		} else if (strcmp(a, "--adlib") == 0) {
			o->music = 'a';
		} else if (strcmp(a, "--roland") == 0) {
			o->music = 'r';
		} else if (strcmp(a, "--scale") == 0 && next) {
			o->scale = atoi(args[++i]);
		} else if (strcmp(a, "--no-aspect") == 0) {
			o->no_aspect = true;
		} else if (strcmp(a, "--no-integer") == 0) {
			o->no_integer = true;
		} else if (strcmp(a, "--help") == 0) {
			o->help = true;
		} else {
			o->game_argv[o->game_argc++] = args[i];
		}
	}
	if (o->shot_every <= 0)
		o->shot_every = 60;
}

const port_options *options(void)
{
	if (!parsed_ready)
		parse();
	return &parsed;
}

void options_show_help(void)
{
	printf("Usage: Ultima8 [--data <dir>] [switches]\n"
		"\n"
		"  --data <dir>     the game folder, with STATIC and USECODE (default: U8_DATA, else the\n"
		"                   settings file's, else the current folder if it has STATIC, else the\n"
		"                   folder Ultima8 is in, else Documents/Ultima8)\n"
		"  --home <dir>     the port's home folder: settings, saved games, recordings (default:\n"
		"                   U8_HOME, else ~/.u8port; on Windows %%APPDATA%%\\u8port)\n"
		"  --saves-in-data  keep the game's GAMEDAT and SAVEGAME in the game folder, as DOS did\n"
		"  --adlib          AdLib music, even when MT-32 ROMs are found\n"
		"  --roland         MT-32 music (the default when its ROMs are found)\n"
		"  --scale <n>      window size, 1 to 8 times the game's screen\n"
		"  --no-aspect      square pixels, not 4:3\n"
		"  --no-integer     scale to fit the window, not by whole multiples\n"
		"\n"
		"Recording and replaying (see README.md):\n"
		"  --no-recording   do not record this session to the home folder's recordings/\n"
		"  --record <dir>   record this session to <dir> (made; must not exist)\n"
		"  --replay <dir>   replay a recorded session: no window, as fast as it goes; the exit\n"
		"                   code is 0 when every checkpoint matched, 2 when the game diverged\n"
		"  --replay-view    with --replay: in a window at the recorded speed, with sound\n"
		"  --shots <dir>    save the game's screen to <dir> every so often, named by the game\n"
		"                   second, so a recording's and its replay's can be compared\n"
		"  --shot-every <n> game seconds between them (default 60)\n"
		"\n"
		"  --help           this list\n"
		"\n"
		"Other switches go to the game, as U8.EXE's did.\n");
}
