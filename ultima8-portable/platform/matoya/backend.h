/* Shared state between the parts of the libmatoya backend.
 *
 *   app thread (process main)   game thread (U8Main)        audio thread
 *   window, events, drawing     plat_* calls, PIT handler   Munt, OPL3, digitised sound
 *        |  events_push ------------> plat_pump                   ^
 *        |  video_draw <------------ plat_video_present           |
 *                                    plat_midi_send / plat_pcm ---+
 */
#ifndef BACKEND_H
#define BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#include <matoya.h>

#include "plat.h"

#define PATH_SIZE 1024

/* options.c: the port's command line, read the first time anything asks. */
typedef struct {
	const char *data;          /* --data or U8_DATA */
	bool data_switch;          /* given as --data, which is remembered */
	const char *home;          /* --home or U8_HOME */
	const char *record;        /* --record DIR */
	const char *replay;        /* --replay DIR */
	const char *shots;         /* --shots DIR */
	int shot_every;            /* --shot-every SECS (game seconds) */
	bool replay_view;
	bool no_recording;
	bool saves_in_data;
	bool no_aspect;
	bool no_integer;
	bool help;
	char music;                /* 'a', 'r' or 0 */
	int scale;                 /* 0 when not given */
	int game_argc;
	char **game_argv;          /* argv[0] and the switches that are the game's */
} port_options;

const port_options *options(void);
void options_show_help(void);

/* home.c */
bool path_is_dir(const char *path);
bool path_is_file(const char *path);
bool path_join(char *out, size_t size, const char *dir, const char *name);
bool path_make_dir(const char *path);
bool path_make_dirs(const char *path);
char **path_list(const char *dir, int *count);
void path_free_list(char **names, int count);
bool path_copy_file(const char *from, const char *to);
bool path_copy_tree(const char *from, const char *to);
bool path_remove_tree(const char *path);
const char *home_dir(void);
const char *setting_get(const char *key);
int setting_int(const char *key, int fallback, int low, int high);
bool setting_bool(const char *key, bool fallback);
void setting_set(const char *key, const char *value);
void setting_set_int(const char *key, int value);
void setting_set_bool(const char *key, bool value);
/* The folder holding GAMEDAT and SAVEGAME, or NULL for the game folder. */
const char *saves_root(void);
void saves_set_root(const char *dir);
void saves_prepare(const char *game_root);
bool saves_copy(const char *from, const char *to);

/* main.c */
int64_t backend_elapsed_us(void);
/* Asks the app thread to close the window and end the process with this code. */
void backend_request_exit(int code);
/* Ends the program with this code from any thread; a game thread stops here. */
void backend_stop(int code);

/* An input event as the game thread takes it from the app thread. */
enum { EVENT_KEY, EVENT_MOVE, EVENT_BUTTON, EVENT_FOCUS_LOST };

typedef struct {
	uint8_t kind;
	bool pressed;
	uint16_t key;
	uint16_t mod;
	uint16_t button;
	int16_t x;
	int16_t y;
} port_event;

/* record.c: recording each session's inputs and playing them back. */

/* The game's clock moves in steps of this many microseconds. */
#define CLOCK_QUANTUM_US 1000

/* Values from outside the game that it reads between pumps. */
enum { VALUE_FILE_TIME = 1, VALUE_DISK_FREE, VALUE_DAY_OF_WEEK, VALUE_TIME_NOW };

/* Opens the recording or the replay, once the folders are chosen. App thread, or the first
 * file access before main. */
void record_start(void);
bool record_replaying(void);
/* Replaying with no window, as fast as the game runs. */
bool record_replay_fast(void);
/* The music device the recording was made with, or 0 when not replaying. */
char record_music_device(void);
/* The game's arguments the recording was made with; NULL when not replaying. */
char **record_game_args(int *argc);
/* The folder of the session being recorded, or NULL. */
const char *record_session_dir(void);
/* Recording: notes the value and returns it. Replaying: returns the recorded one. */
uint32_t record_value(int tag, uint32_t live);
void record_pump_write(uint32_t quanta, const port_event *events, uint32_t count);
/* The clock step of the next recorded pump, its events in events. */
uint32_t record_pump_read(port_event *events, uint32_t max, uint32_t *count);
void record_checkpoint(int64_t game_us);
/* The game thread is ending the program by itself (plat_exit). */
void record_game_exit(void);
/* Ends the recording with the program's exit code, or ends a replay; returns the code the
 * process should end with. Any thread. */
int record_finish(int code);

/* settings.c: the settings screen (F11). */
void settings_init(void);
bool settings_open(void);
void settings_show(bool show);
/* App thread: a window event for the screen; true when it took it. */
bool settings_event(const MTY_Event *event, MTY_App *app, MTY_Window window);
/* App thread: the screen over the game's 320x200 RGBA frame, as a w x h RGBA image (valid
 * until the next call), or NULL for an empty view. */
const uint32_t *settings_render(const uint8_t (*frame)[4], int w, int h);
/* App thread: sizes the window by the scale and aspect settings. */
void settings_apply_window(MTY_App *app, MTY_Window window);
/* Game thread: waits while the screen is open; returns the microseconds it waited. */
int64_t settings_hold(void);
/* The window settings, as the screen and the command line set them. */
int settings_scale(void);
bool settings_aspect(void);
bool settings_integer(void);
int settings_volume(void);

/* events.c */
void events_init(void);
/* Queues the release of every button and key, as after the settings screen. */
void events_release_all(void);
int64_t game_clock_us(void);
void events_reset(void);
void events_push(const MTY_Event *event, MTY_Size view);
bool events_take_warp(MTY_Size view, uint32_t *x, uint32_t *y);
/* The getch word for a key press (scan code high, ASCII low), or 0 when BIOS gave none. */
uint16_t keys_bios_word(MTY_Key key, MTY_Mod mod);
/* The unshifted key that types c, or MTY_KEY_NONE. */
MTY_Key keys_for_char(char c);

/* The window's client size, as the app thread last saw it. */
MTY_Size backend_view_size(void);

/* testinput.c */
bool test_input_active(void);
/* Queues the U8_TEST_INPUT steps that are due, as if the user had made them. Game thread. */
void test_input_poll(void);
/* Called as each frame is presented. Game thread. */
void test_input_frame(void);
/* True when the script runs the game on its fixed clock, which test_clock_us reads. */
bool test_clock_fixed(void);
int64_t test_clock_us(void);
/* Sleeps, and moves the fixed clock on by the same time. Game thread. */
void backend_sleep(uint32_t milliseconds);

/* video.c */
void video_init(void);
void video_reset(void);
/* The game image inside a window of this size, in window pixels. */
void video_view_rect(MTY_Size view, int32_t *x, int32_t *y, int32_t *w, int32_t *h);
/* Integer scale matoya should use for this window, or 0 to fit. */
float video_scale(MTY_Size view);
void video_draw(MTY_App *app, MTY_Window window);
/* The game's screen memory: the buffer it last presented, or NULL before its first frame. */
uint8_t *video_screen(void);
/* Shows the last presented frame again when the palette changed or time has passed. */
void video_refresh(bool force);
/* Saves the last presented frame as a 320x200 PPM. */
bool video_write_shot(const char *path);
/* Saves the game's screen memory now, with the palette now. */
bool video_write_game_shot(const char *path);

/* The data folder, chosen on first use (main.c). */
void backend_ensure_root(void);

/* files.c */
void files_set_root(const char *dir);
/* The game's folder, as set. */
const char *files_root(void);
/* Whether the data folder holds the game's files; if not, says what is wrong in message. */
bool files_check_data(char *message, size_t size);
void files_reset(void);

/* console.c */
/* A new program's text cursor starts at the top left, as after a mode set. */
void console_reset(void);

/* audio.c */
/* 'a' plays AdLib, 'r' the MT-32; otherwise the MT-32 when its ROMs are found. */
void audio_choose_music(char device);
/* Whether the MT-32 is playing, and the folder its ROMs were looked for in. */
bool audio_mt32_found(void);
const char *audio_rom_dir(void);
void audio_start(void);
/* Stops the sound tick, flushes queued MIDI and speech, and restarts the synth as powered on. */
void audio_reset(void);
/* Releases the sound lock if this thread holds it. */
void audio_drop_lock(void);


#endif
