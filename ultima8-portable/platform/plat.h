/* The platform layer: everything the game needs from the host.
 *
 *   game/
 *     |  plat_* calls only; no OS headers, interrupts, ports, DOS extender or BIOS
 *   plat.h
 *     |
 *   backend (one per target, chosen when linking): matoya, ...
 *
 * Threads: the game runs on one thread (called the game thread here) except the sound tick,
 * which the audio backend calls on its own clock. Code shared with the sound tick is
 * bracketed by plat_sound_lock()/plat_sound_unlock(), where the DOS game disabled interrupts.
 *
 * Interrupts: what the DOS game did in interrupt handlers (the kernel's real-time processes on
 * the PIT, the mouse driver's event hook) runs on the game thread from plat_pump, which the
 * game calls from its main loop and from every busy-wait.
 */
#ifndef PLAT_H
#define PLAT_H

#include <stdbool.h>
#include <stdint.h>

/* The game packs its own structs; these keep the host's layout on both sides. */
#pragma pack(push)
#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif

/* ---- System ---- */

/* The backend owns the process's main(). It opens the window and audio, then calls U8Main,
 * the game's own main(), on the game thread. */
int16_t U8Main(int16_t argc, char **argv);
/* Shows the message and ends the process. */
void plat_fatal(const char *message);
/* Ends the program with this exit code. The game's stack is not unwound. */
void plat_exit(int16_t code);
/* Text the DOS game printed to the console. */
void plat_log(const char *text);
/* Text on the game screen, as BIOS text output drew it in graphics mode: 8x8 characters on a
 * 40 by 25 grid. goto counts from 1, as gotoxy did; write handles \r, \n (down a row only)
 * and \b like the BIOS. */
void plat_console_goto(int16_t x, int16_t y);
void plat_console_write(const char *text);
int16_t plat_console_x(void);
int16_t plat_console_y(void);

/* ---- Time ---- */

#define PLAT_TICKS_PER_SECOND 60

/* 60 Hz ticks since start. */
uint32_t plat_ticks(void);
uint32_t plat_milliseconds(void);
/* Microseconds since start. */
uint64_t plat_microseconds(void);
void plat_sleep(uint32_t milliseconds);
/* Handles window and input events and runs due timers and the PIT handler. */
void plat_pump(void);
/* For busy-waits: pump, show the screen again (the game may have drawn into it), sleep
 * briefly. */
void plat_yield(void);

typedef void (*plat_timer_fn)(void);
/* The PC's interval timer: fn runs on the game thread from plat_pump once every period_us
 * microseconds, catching up on missed periods (a few at most). 0 stops it. */
void plat_pit_set(plat_timer_fn fn, uint32_t period_us);

/* The sound tick, run by the audio backend 60 times a second in step with the sound it
 * renders, under the sound lock. (The game's own sound runs from kernel processes on the
 * game thread and does not use it.) */
void plat_sound_tick_set(plat_timer_fn fn);
/* Recursive; also held by the backend around every sound tick. */
void plat_sound_lock(void);
void plat_sound_unlock(void);

/* ---- Video: 320x200, one byte a pixel ---- */

#define PLAT_SCREEN_WIDTH 320
#define PLAT_SCREEN_HEIGHT 200

/* Shows a whole frame. pixels is 320x200, 320 bytes a row. The backend keeps showing this
 * buffer (as VGA memory was always on screen) until another is presented. */
void plat_video_present(const uint8_t *pixels);
/* Palette entries as 6-bit VGA red, green, blue triples; applied with the next present. */
void plat_video_set_palette(int16_t first, int16_t count, const uint8_t *rgb);
/* Reads palette entries back, as 6-bit triples. */
void plat_video_get_palette(int16_t first, int16_t count, uint8_t *rgb);
/* Waits for the next 70 Hz display refresh, as the game's retrace wait did. */
void plat_video_wait_retrace(void);

/* ---- Keyboard ---- */

/* Nonzero when a key is waiting. */
int16_t plat_key_available(void);
/* The next key as getch returned it: ASCII, or 0 followed by the scan code on the next call. */
int16_t plat_key_get(void);
/* The next key as int 16h function 0 gave it: scan code high, ASCII low. Waits for one. */
uint16_t plat_key_bios(void);
/* BIOS shift flags: 1 right shift, 2 left shift, 4 ctrl, 8 alt. */
uint16_t plat_key_modifiers(void);

/* ---- Mouse: x in 0..639, y in 0..199, as the DOS mouse driver reported ---- */

#define PLAT_MOUSE_MOVED 0x01
#define PLAT_MOUSE_LEFT_DOWN 0x02
#define PLAT_MOUSE_LEFT_UP 0x04
#define PLAT_MOUSE_RIGHT_DOWN 0x08
#define PLAT_MOUSE_RIGHT_UP 0x10

/* buttons: 1 left, 2 right. */
void plat_mouse_state(int16_t *x, int16_t *y, uint16_t *buttons);
/* Called from plat_pump for each mouse event, with a PLAT_MOUSE_* mask, as the driver called
 * the game's event hook. */
typedef void (*plat_mouse_fn)(uint16_t events, uint16_t buttons, int16_t x, int16_t y);
void plat_mouse_set_handler(plat_mouse_fn fn);
void plat_mouse_move_to(int16_t x, int16_t y);
/* The range the pointer moves in, as int 33h functions 7 and 8 set it; -1 leaves a side as
 * it is. */
void plat_mouse_set_bounds(int16_t left, int16_t top, int16_t right, int16_t bottom);

/* ---- Files ----
 * Names are the game's DOS names ("STATIC\\U8SHAPES.FLX"), resolved against the data directory
 * with either separator and any letter case.
 */

#define PLAT_FILE_READ 1
#define PLAT_FILE_WRITE 2

#define PLAT_SEEK_SET 0
#define PLAT_SEEK_CUR 1
#define PLAT_SEEK_END 2

/* Handles are 5 and up (0-4 were DOS's standard handles); -1 on failure. */
int16_t plat_file_open(const char *name, int16_t mode);
/* Creates the file, or empties it. */
int16_t plat_file_create(const char *name);
void plat_file_close(int16_t file);
/* Bytes moved, or -1. */
int32_t plat_file_read(int16_t file, void *buffer, int32_t count);
int32_t plat_file_write(int16_t file, const void *buffer, int32_t count);
/* The new position, or -1. */
int32_t plat_file_seek(int16_t file, int32_t offset, int16_t origin);
int32_t plat_file_length(int16_t file);
/* Cuts the file at the current position. Nonzero on success. */
int16_t plat_file_truncate(int16_t file);
/* DOS date and time of the last change, as findfirst gave them: date high, time low. */
uint32_t plat_file_time(const char *name);
/* The same through an open handle. */
uint32_t plat_file_handle_time(int16_t file);

/* These return nonzero on success. */
int16_t plat_file_exists(const char *name);
int16_t plat_dir_exists(const char *name);
int16_t plat_dir_create(const char *name);
int16_t plat_dir_remove(const char *name);
int16_t plat_file_remove(const char *name);
int16_t plat_file_rename(const char *from, const char *to);
uint32_t plat_disk_free(void);

/* Directory listing with a DOS wildcard ("GAMEDAT\\*.*", "STATIC\\*.FLX"). */
typedef struct plat_find {
	char name[13];
	uint8_t attrib;     /* 0x10 for a directory */
	uint32_t size;
	uint32_t time;      /* DOS date high, time low */
	void *state;
} plat_find;
/* Nonzero when an entry was found. plat_find_close ends a listing early. Directories are
 * listed only when dirs is nonzero. */
int16_t plat_find_first(const char *pattern, int16_t dirs, plat_find *find);
int16_t plat_find_next(plat_find *find);
void plat_find_close(plat_find *find);

/* ---- Sound ---- */

/* MIDI to the MT-32: a channel message (1-3 bytes) or a whole SysEx message. Safe from the
 * sound tick. */
int16_t plat_midi_available(void);
void plat_midi_send(const uint8_t *message, int16_t length);
/* Which music synth the player chose: 'r' the MT-32, 'a' the AdLib, 0 neither. */
char plat_music_device(void);

/* AdLib: an OPL3 at any base port, written and read as the DOS drivers did with out and in.
 * port & 3: 0 the address, 1 the data, 2 the second bank's address, 3 its data. Reads give
 * the status register, with timers 1 and 2 running on the platform clock. Safe from any
 * thread. */
bool plat_opl_available(void);
void plat_opl_write(uint16_t port, uint8_t value);
uint8_t plat_opl_read(uint16_t port);

/* Digitised sound: unsigned 8-bit mono at the given rate. Never blocks. */
void plat_pcm_start(uint32_t rate);
/* Queues samples; returns how many were taken. */
int32_t plat_pcm_queue(const uint8_t *samples, int32_t count);
/* Samples queued and not yet played. */
int32_t plat_pcm_pending(void);
void plat_pcm_stop(void);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
