/* Recording each session's inputs, and playing a recording back.
 *
 * Everything from outside that the game sees reaches it through the platform layer: the clock
 * and the input at each plat_pump (events.c), and a few values read between pumps (file
 * times, free disk space, the day of the week). A recording holds those, in the order the game
 * took them, and a copy of the saved games as they were when it started. Replaying feeds the
 * same values back in the same order to a copy of those saves, so the game runs the same path
 * and ends in the same state. Once a second of game time a digest of the game's state (the
 * random seeds, every item, the global flags; plat_set_state_digest) goes in as a checkpoint,
 * and the replay checks each one; the first that differs, or a read the recording does not
 * have in that place, stops the replay and says where.
 *
 * A session folder holds:
 *
 *   SESSION.U8R   a text header ("U8REC 1", key=value lines, "end"), then the operations
 *   stage/        GAMEDAT and SAVEGAME as they were at the start
 *
 * Each operation is a byte, the number of empty pumps (no clock step, no events) before it as
 * an unsigned LEB128 number, then its own numbers:
 *
 *   1 PUMP    step                        the clock moved on by step quanta
 *   2 EVENTS  step, count, events         and these events came in
 *   3 VALUE   tag, value                  a value the game read (VALUE_* in backend.h)
 *   4 CHECK   game milliseconds, digest   a checkpoint; the digest as 8 bytes, low first
 *   5 END     exit code (zigzag)          the session ended here
 *
 * An event is a byte (kind, and 0x80 when pressed) and then: a key, its key code and modifiers;
 * a move, x and y; a button, its number; focus lost, nothing.
 *
 * The black box: unless turned off, every session that is not a replay or a scripted test is
 * recorded to recordings/<date>-<time>/ in the home folder, and the newest five are kept.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "backend.h"

#ifndef U8_PORT_VERSION
#define U8_PORT_VERSION "dev"
#endif

#define MAGIC "U8REC 1"
#define SESSION_FILE "SESSION.U8R"
#define BLACK_BOX_KEEP 5
#define FLUSH_BYTES (256 * 1024)
#define MAX_ARGS 64

enum { OP_PUMP = 1, OP_EVENTS, OP_VALUE, OP_CHECK, OP_END, OP_EOF };

enum { MODE_OFF, MODE_RECORD, MODE_REPLAY };

static int mode;
static bool started;
static MTY_Mutex *lock;
static plat_digest_fn digest_fn;
static uint32_t checks;
static uint64_t pumps;

/* ---- Recording ---- */

static char session[PATH_SIZE];
static FILE *out;
static uint8_t *buffer;
static size_t length, capacity;
static uint32_t idle;
static bool header_written, closed;
static char data_print[17];

/* ---- Replaying ---- */

typedef struct {
	int op;
	uint32_t idle;
	uint32_t a, b;          /* PUMP/EVENTS: step, count; VALUE: tag, value; CHECK: ms */
	uint64_t digest;
	int32_t code;
	port_event events[256];
} operation;

static uint8_t *data;
static size_t data_size, data_at;
static operation next;
static char music;
static int replay_argc;
static char *replay_argv[MAX_ARGS + 1];
static char work[PATH_SIZE];
static bool reported;
static bool game_exited;

void plat_set_state_digest(plat_digest_fn fn)
{
	digest_fn = fn;
}

bool record_replaying(void)
{
	return mode == MODE_REPLAY;
}

bool record_replay_fast(void)
{
	return mode == MODE_REPLAY && !options()->replay_view;
}

char record_music_device(void)
{
	return mode == MODE_REPLAY ? music : 0;
}

char **record_game_args(int *argc)
{
	if (mode != MODE_REPLAY)
		return NULL;
	*argc = replay_argc;
	return replay_argv;
}

const char *record_session_dir(void)
{
	return mode == MODE_RECORD ? session : NULL;
}

/* ---- Numbers ---- */

static void put_byte(uint8_t b)
{
	if (length == capacity) {
		capacity = capacity ? capacity * 2 : 65536;
		buffer = realloc(buffer, capacity);
	}
	buffer[length++] = b;
}

static void put_number(uint64_t v)
{
	while (v >= 0x80) {
		put_byte((uint8_t) (v | 0x80));
		v >>= 7;
	}
	put_byte((uint8_t) v);
}

static bool get_number(uint64_t *v)
{
	int shift = 0;

	*v = 0;
	while (data_at < data_size && shift < 64) {
		uint8_t b = data[data_at++];

		*v |= (uint64_t) (b & 0x7F) << shift;
		if (!(b & 0x80))
			return true;
		shift += 7;
	}
	return false;
}

static uint32_t get32(bool *ok)
{
	uint64_t v;

	if (!get_number(&v))
		*ok = false;
	return (uint32_t) v;
}

/* ---- The game data's fingerprint: the names and sizes of STATIC's and USECODE's files ---- */

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
	const uint8_t *b = p;

	for (size_t i = 0; i < n; i++)
		h = (h ^ b[i]) * 0x100000001B3ull;
	return h;
}

static uint64_t fingerprint_dir(uint64_t h, const char *root, const char *want)
{
	int count, files;
	char **names = path_list(root, &count);

	for (int i = 0; i < count; i++) {
		char dir[PATH_SIZE];
		char **inside;

		if (strlen(names[i]) != strlen(want))
			continue;
		for (size_t c = 0; want[c]; c++) {
			if ((names[i][c] & ~0x20) != want[c])
				goto next;
		}
		path_join(dir, sizeof dir, root, names[i]);
		inside = path_list(dir, &files);
		for (int f = 0; f < files; f++) {
			char path[PATH_SIZE], upper[256];
			FILE *file;
			long size = -1;
			size_t n;

			for (n = 0; inside[f][n] && n < sizeof upper - 1; n++)
				upper[n] = (char) (inside[f][n] >= 'a' && inside[f][n] <= 'z' ? inside[f][n] - 32 : inside[f][n]);
			upper[n] = '\0';
			path_join(path, sizeof path, dir, inside[f]);
			file = fopen(path, "rb");
			if (file != NULL) {
				fseek(file, 0, SEEK_END);
				size = ftell(file);
				fclose(file);
			}
			h = fnv(h, upper, n + 1);
			h = fnv(h, &size, sizeof size);
		}
		path_free_list(inside, files);
		break;
	next:;
	}
	path_free_list(names, count);
	return h;
}

static void data_fingerprint(char out[17])
{
	uint64_t h = 0xCBF29CE484222325ull;

	h = fingerprint_dir(h, files_root(), "STATIC");
	h = fingerprint_dir(h, files_root(), "USECODE");
	snprintf(out, 17, "%016llx", (unsigned long long) h);
}

/* ---- Writing ---- */

static void write_header(void)
{
	const port_options *o = options();
	time_t now = time(NULL);
	char when[32];

	strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", localtime(&now));
	fprintf(out, MAGIC "\n");
	fprintf(out, "version=%s\n", U8_PORT_VERSION);
	fprintf(out, "started=%s\n", when);
	fprintf(out, "music=%c\n", plat_music_device() ? plat_music_device() : '-');
	fprintf(out, "quantum=%d\n", CLOCK_QUANTUM_US);
	fprintf(out, "data=%s\n", data_print);
	for (int i = 1; i < o->game_argc; i++)
		fprintf(out, "arg=%s\n", o->game_argv[i]);
	fprintf(out, "end\n");
	header_written = true;
}

/* Under the lock. */
static void flush(void)
{
	if (out == NULL)
		return;
	if (!header_written)
		write_header();
	if (length > 0 && fwrite(buffer, 1, length, out) != length) {
		fprintf(stderr, "u8: could not write the recording; it stops here\n");
		fclose(out);
		out = NULL;
	}
	length = 0;
	if (out != NULL)
		fflush(out);
}

static void put_op(int op)
{
	put_byte((uint8_t) op);
	put_number(idle);
	idle = 0;
}

static void put_event(const port_event *e)
{
	put_byte((uint8_t) (e->kind | (e->pressed ? 0x80 : 0)));
	switch (e->kind) {
	case EVENT_KEY:
		put_number(e->key);
		put_number(e->mod);
		break;
	case EVENT_MOVE:
		put_number((uint16_t) e->x);
		put_number((uint16_t) e->y);
		break;
	case EVENT_BUTTON:
		put_number(e->button);
		break;
	default:
		break;
	}
}

void record_pump_write(uint32_t quanta, const port_event *events, uint32_t count)
{
	if (mode != MODE_RECORD)
		return;
	MTY_MutexLock(lock);
	if (!closed) {
		pumps++;
		if (quanta == 0 && count == 0) {
			idle++;
		} else if (count == 0) {
			put_op(OP_PUMP);
			put_number(quanta);
		} else {
			put_op(OP_EVENTS);
			put_number(quanta);
			put_number(count);
			for (uint32_t i = 0; i < count; i++)
				put_event(&events[i]);
		}
		if (length >= FLUSH_BYTES)
			flush();
	}
	MTY_MutexUnlock(lock);
}

/* ---- Reading ---- */

static void load_next(void)
{
	uint64_t v;
	bool ok = true;

	memset(&next, 0, offsetof(operation, events));
	if (data_at >= data_size) {
		next.op = OP_EOF;
		return;
	}
	next.op = data[data_at++];
	next.idle = get32(&ok);
	switch (next.op) {
	case OP_PUMP:
		next.a = get32(&ok);
		break;
	case OP_EVENTS:
		next.a = get32(&ok);
		next.b = get32(&ok);
		if (next.b > sizeof next.events / sizeof *next.events)
			ok = false;
		for (uint32_t i = 0; ok && i < next.b; i++) {
			port_event *e = &next.events[i];
			uint8_t b = data_at < data_size ? data[data_at++] : 0xFF;

			memset(e, 0, sizeof *e);
			e->kind = b & 0x7F;
			e->pressed = (b & 0x80) != 0;
			switch (e->kind) {
			case EVENT_KEY:
				e->key = (uint16_t) get32(&ok);
				e->mod = (uint16_t) get32(&ok);
				break;
			case EVENT_MOVE:
				e->x = (int16_t) get32(&ok);
				e->y = (int16_t) get32(&ok);
				break;
			case EVENT_BUTTON:
				e->button = (uint16_t) get32(&ok);
				break;
			case EVENT_FOCUS_LOST:
				break;
			default:
				ok = false;
			}
		}
		break;
	case OP_VALUE:
		next.a = get32(&ok);
		next.b = get32(&ok);
		break;
	case OP_CHECK:
		next.a = get32(&ok);
		if (data_at + 8 > data_size) {
			ok = false;
			break;
		}
		for (int i = 0; i < 8; i++)
			next.digest |= (uint64_t) data[data_at++] << (8 * i);
		break;
	case OP_END:
		ok = get_number(&v);
		next.code = (int32_t) ((v >> 1) ^ (~(v & 1) + 1));
		break;
	default:
		ok = false;
	}
	if (!ok) {
		fprintf(stderr, "u8: the recording is damaged at byte %zu; the replay ends there\n", data_at);
		data_at = data_size;
		next.op = OP_EOF;
		next.idle = 0;
	}
}

static const char *op_name(int op)
{
	switch (op) {
	case OP_PUMP: case OP_EVENTS: return "an input step";
	case OP_VALUE: return "a value read";
	case OP_CHECK: return "a checkpoint";
	case OP_END: return "the end";
	default: return "the end of the file";
	}
}

static void stop_replay(int code)
{
	reported = true;
	fflush(stderr);
	backend_stop(code);
}

static void diverged(const char *what)
{
	fprintf(stderr, "u8: replay diverged at game time %.3f s (pump %llu, after %u matching "
		"checkpoints): %s\n", (double) game_clock_us() / 1e6, (unsigned long long) pumps, checks, what);
	stop_replay(2);
}

static void finished(void)
{
	if (next.op == OP_END)
		fprintf(stderr, "u8: replay finished: the recording ends at game time %.3f s; %u "
			"checkpoints matched\n", (double) game_clock_us() / 1e6, checks);
	else
		fprintf(stderr, "u8: replay finished: the recording stops at game time %.3f s without an "
			"end (that session crashed or was killed); %u checkpoints matched\n",
			(double) game_clock_us() / 1e6, checks);
	stop_replay(0);
}

uint32_t record_pump_read(port_event *events, uint32_t max, uint32_t *count)
{
	uint32_t step;

	*count = 0;
	if (mode != MODE_REPLAY)
		return 0;
	pumps++;
	if (next.idle > 0) {
		next.idle--;
		return 0;
	}
	if (next.op == OP_END || next.op == OP_EOF)
		finished();
	if (next.op != OP_PUMP && next.op != OP_EVENTS) {
		char what[128];

		snprintf(what, sizeof what, "the game took input where the recording has %s", op_name(next.op));
		diverged(what);
	}
	step = next.a;
	if (next.op == OP_EVENTS) {
		*count = next.b < max ? next.b : max;
		memcpy(events, next.events, *count * sizeof *events);
	}
	load_next();
	return step;
}

uint32_t record_value(int tag, uint32_t live)
{
	uint32_t v;

	backend_ensure_root();
	if (mode == MODE_RECORD) {
		MTY_MutexLock(lock);
		if (!closed) {
			put_op(OP_VALUE);
			put_number((uint32_t) tag);
			put_number(live);
		}
		MTY_MutexUnlock(lock);
		return live;
	}
	if (mode != MODE_REPLAY)
		return live;
	if (next.idle > 0 || next.op != OP_VALUE || next.a != (uint32_t) tag) {
		char what[160];

		if (next.idle > 0)
			snprintf(what, sizeof what, "the game read value %d where the recording has an input step", tag);
		else
			snprintf(what, sizeof what, "the game read value %d where the recording has %s%s", tag,
				next.op == OP_VALUE ? "another value" : op_name(next.op), "");
		diverged(what);
	}
	v = next.b;
	load_next();
	return v;
}

void record_checkpoint(int64_t game_us)
{
	uint64_t digest = digest_fn != NULL ? digest_fn() : 0;

	if (mode == MODE_RECORD) {
		MTY_MutexLock(lock);
		if (!closed) {
			put_op(OP_CHECK);
			put_number((uint64_t) (game_us / 1000));
			for (int i = 0; i < 8; i++)
				put_byte((uint8_t) (digest >> (8 * i)));
			flush();
		}
		MTY_MutexUnlock(lock);
		checks++;
		return;
	}
	if (mode != MODE_REPLAY)
		return;
	if (next.idle > 0 || next.op != OP_CHECK) {
		char what[128];

		snprintf(what, sizeof what, "a checkpoint is due where the recording has %s",
			next.idle > 0 ? "an input step" : op_name(next.op));
		diverged(what);
	}
	if (next.digest != digest) {
		char what[128];

		snprintf(what, sizeof what, "the game's state differs at the checkpoint (digest %016llx, "
			"recorded %016llx)", (unsigned long long) digest, (unsigned long long) next.digest);
		diverged(what);
	}
	checks++;
	load_next();
}

void record_game_exit(void)
{
	game_exited = true;
}

int record_finish(int code)
{
	if (mode == MODE_RECORD) {
		MTY_MutexLock(lock);
		if (!closed) {
			uint32_t z = (uint32_t) ((code << 1) ^ (code >> 31));

			put_op(OP_END);
			put_number(z);
			flush();
			if (out != NULL)
				fclose(out);
			out = NULL;
			closed = true;
		}
		MTY_MutexUnlock(lock);
		return code;
	}
	if (mode != MODE_REPLAY)
		return code;
	if (!reported) {
		reported = true;
		if (game_exited && next.idle == 0 && next.op == OP_END && next.code == code) {
			fprintf(stderr, "u8: replay finished: the game ended as recorded, at game time %.3f s "
				"with exit code %d; %u checkpoints matched\n", (double) game_clock_us() / 1e6, code, checks);
			code = 0;
		} else if (game_exited) {
			fprintf(stderr, "u8: replay diverged at game time %.3f s (pump %llu, after %u matching "
				"checkpoints): the game ended (exit code %d) where the recording has %s\n",
				(double) game_clock_us() / 1e6, (unsigned long long) pumps, checks, code,
				next.idle > 0 ? "an input step" : op_name(next.op));
			code = 2;
		} else {
			fprintf(stderr, "u8: replay stopped at game time %.3f s before the recording's end; "
				"%u checkpoints matched\n", (double) game_clock_us() / 1e6, checks);
			code = 1;
		}
	}
	if (work[0] != '\0')
		path_remove_tree(work);
	return code;
}

/* ---- Starting ---- */

static bool is_session_name(const char *name)
{
	/* YYYYMMDD-HHMMSS, perhaps with -n after it. */
	for (int i = 0; i < 15; i++) {
		if (i == 8 ? name[i] != '-' : (name[i] < '0' || name[i] > '9'))
			return false;
	}
	return name[15] == '\0' || name[15] == '-';
}

static void prune_black_box(const char *dir)
{
	int count, sessions = 0;
	char **names = path_list(dir, &count);

	for (int i = 0; i < count; i++)
		sessions += is_session_name(names[i]);
	for (int i = 0; i < count && sessions > BLACK_BOX_KEEP; i++) {
		char path[PATH_SIZE];

		if (!is_session_name(names[i]))
			continue;
		path_join(path, sizeof path, dir, names[i]);
		path_remove_tree(path);
		sessions--;
	}
	path_free_list(names, count);
}

static bool begin_recording(const char *dir, bool black_box)
{
	char file[PATH_SIZE], stage[PATH_SIZE];
	const char *saves = saves_root() != NULL ? saves_root() : files_root();

	path_join(file, sizeof file, dir, SESSION_FILE);
	if (path_is_file(file)) {
		fprintf(stderr, "u8: %s already holds a recording; not recording\n", dir);
		return false;
	}
	path_join(stage, sizeof stage, dir, "stage");
	if (!path_make_dirs(dir) || !saves_copy(saves, stage) || (out = fopen(file, "wb")) == NULL) {
		fprintf(stderr, "u8: could not start the recording in %s; not recording\n", dir);
		return false;
	}
	snprintf(session, sizeof session, "%s", dir);
	data_fingerprint(data_print);
	mode = MODE_RECORD;
	if (black_box) {
		char parent[PATH_SIZE];

		path_join(parent, sizeof parent, home_dir(), "recordings");
		prune_black_box(parent);
	} else {
		fprintf(stderr, "u8: recording to %s\n", dir);
	}
	return true;
}

static void black_box_dir(char *out_dir, size_t size)
{
	char parent[PATH_SIZE], name[32];
	time_t now = time(NULL);

	path_join(parent, sizeof parent, home_dir(), "recordings");
	path_make_dirs(parent);
	strftime(name, sizeof name, "%Y%m%d-%H%M%S", localtime(&now));
	path_join(out_dir, size, parent, name);
	for (int n = 2; path_is_dir(out_dir) && n < 100; n++) {
		char numbered[40];

		snprintf(numbered, sizeof numbered, "%s-%d", name, n);
		path_join(out_dir, size, parent, numbered);
	}
}

static char *read_line(char **at, char *end)
{
	char *line = *at;
	char *nl = memchr(line, '\n', (size_t) (end - line));

	if (nl == NULL)
		return NULL;
	*nl = '\0';
	*at = nl + 1;
	if (nl > line && nl[-1] == '\r')
		nl[-1] = '\0';
	return line;
}

static void begin_replay(const char *where)
{
	char dir[PATH_SIZE], file[PATH_SIZE], stage[PATH_SIZE], print[17] = "";
	char *at, *end, *line;
	FILE *f;
	long size;

	snprintf(dir, sizeof dir, "%s", where);
	if (path_is_file(dir)) {
		char *slash = strrchr(dir, '/');
		char *back = strrchr(dir, '\\');

		if (back > slash)
			slash = back;
		snprintf(file, sizeof file, "%s", where);
		if (slash != NULL)
			*slash = '\0';
		else
			snprintf(dir, sizeof dir, ".");
	} else {
		path_join(file, sizeof file, dir, SESSION_FILE);
	}
	f = fopen(file, "rb");
	if (f == NULL) {
		fprintf(stderr, "u8: no recording at %s\n", file);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc((size_t) size + 1);
	data_size = fread(data, 1, (size_t) size, f);
	fclose(f);
	data[data_size] = '\0';

	at = (char *) data;
	end = (char *) data + data_size;
	line = read_line(&at, end);
	if (line == NULL || strcmp(line, MAGIC) != 0) {
		fprintf(stderr, "u8: %s is not a recording this version can play\n", file);
		exit(1);
	}
	replay_argv[replay_argc++] = options()->game_argc > 0 ? options()->game_argv[0] : "Ultima8";
	while ((line = read_line(&at, end)) != NULL && strcmp(line, "end") != 0) {
		if (strncmp(line, "music=", 6) == 0)
			music = line[6] == 'r' || line[6] == 'a' ? line[6] : 0;
		else if (strncmp(line, "data=", 5) == 0)
			snprintf(print, sizeof print, "%s", line + 5);
		else if (strncmp(line, "quantum=", 8) == 0 && atoi(line + 8) != CLOCK_QUANTUM_US) {
			fprintf(stderr, "u8: %s was made with a clock step of %s us; this version uses %d\n",
				file, line + 8, CLOCK_QUANTUM_US);
			exit(1);
		} else if (strncmp(line, "arg=", 4) == 0 && replay_argc < MAX_ARGS)
			replay_argv[replay_argc++] = line + 4;
	}
	if (line == NULL) {
		fprintf(stderr, "u8: %s has no complete header\n", file);
		exit(1);
	}
	data_at = (size_t) ((uint8_t *) at - data);
	data_fingerprint(data_print);
	if (print[0] != '\0' && strcmp(print, data_print) != 0)
		fprintf(stderr, "u8: warning: the game data differs from the recording's (fingerprint %s, "
			"recorded %s); the replay may diverge\n", data_print, print);

	/* A working copy of the saves as the session began, which the replay may write over. */
	path_join(stage, sizeof stage, dir, "stage");
	{
		char parent[PATH_SIZE], name[32];

		path_join(parent, sizeof parent, home_dir(), "replay-work");
		snprintf(name, sizeof name, "%d", (int) getpid());
		path_join(work, sizeof work, parent, name);
	}
	path_remove_tree(work);
	if (!saves_copy(stage, work)) {
		fprintf(stderr, "u8: could not copy the recording's saved games to %s\n", work);
		exit(1);
	}
	saves_set_root(work);
	mode = MODE_REPLAY;
	load_next();
	fprintf(stderr, "u8: replaying %s\n", file);
}

void record_start(void)
{
	const port_options *o = options();

	if (started)
		return;
	started = true;
	lock = MTY_MutexCreate();
	if (o->replay != NULL) {
		begin_replay(o->replay);
	} else if (o->record != NULL) {
		begin_recording(o->record, false);
	} else if (!o->no_recording && setting_bool("recording", true) && !test_input_active()) {
		char dir[PATH_SIZE];

		black_box_dir(dir, sizeof dir);
		begin_recording(dir, true);
	}
}
