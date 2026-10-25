/* The settings screen: F11 (Cmd+, on macOS) opens it over the game, F11 or Esc closes it.
 *
 * It has three tabs, Sound, Display and Game, of rows the arrows move between and Enter or
 * Left/Right change; the mouse works too. Every change is written to u8port.cfg in the home
 * folder at once (home.c); a row marked "next start" is used from the next start, the others
 * at once. While the screen is open the game waits in plat_pump and its clock stands still, so
 * a recording does not see the time either.
 *
 * The screen is drawn on the app thread, over a dimmed copy of the game's last frame, at the
 * window's own pixel size with the VGA 8x8 font scaled by whole numbers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"
#include "vgafont8.h"

#define PANEL_COLUMNS 56
#define PANEL_ROWS 23
/* Pixels from one text row to the next at scale 1: the 8-pixel font and a gap. */
#define LINE 10
#define VALUE_COLUMN 26
#define TAB_WIDTH 12
#define FIRST_ROW 6

/* 0xRRGGBB */
#define C_BACK 0x2A2318u
#define C_BORDER 0x6B5636u
#define C_TEXT 0xE8D9B5u
#define C_SELECT 0xF2C66Du
#define C_DIM 0xB9A77Fu

enum { R_MUSIC, R_ROMS, R_VOLUME, R_FULLSCREEN, R_SCALE, R_ASPECT, R_INTEGER, R_DATA, R_HOME,
	R_RECORD, R_AT_START, R_COUNT };

enum { K_SWITCH, K_LIST, K_SLIDER, K_INFO };

typedef struct {
	int tab;
	int kind;
	const char *label;
	bool next_start;
	const char *help;
} row_def;

static const row_def rows[R_COUNT] = {
	[R_MUSIC] = {0, K_LIST, "Music", true,
		"Automatic plays the Roland MT-32 when its ROMs are found, else the AdLib. "
		"--roland and --adlib choose for one run."},
	[R_ROMS] = {0, K_INFO, "MT-32 ROMs", true,
		"The folder with the MT-32 control and PCM ROMs: U8_MT32_ROMS, else mt32-roms= in "
		"u8port.cfg, else the game folder."},
	[R_VOLUME] = {0, K_SLIDER, "Volume", false, "Everything the game plays, 0 to 100."},
	[R_FULLSCREEN] = {1, K_SWITCH, "Fullscreen", false, "Alt-Enter switches it too."},
	[R_SCALE] = {1, K_LIST, "Window scale", false,
		"The window's size, as a multiple of the game's 320x200 screen. --scale n for one run."},
	[R_ASPECT] = {1, K_SWITCH, "4:3 aspect", false,
		"Shows the 200 lines as 240, as on a 4:3 monitor; off gives square pixels. "
		"--no-aspect for one run."},
	[R_INTEGER] = {1, K_SWITCH, "Whole-number scaling", false,
		"Scales by whole multiples only, so every pixel is the same size; off fills the "
		"window. --no-integer for one run."},
	[R_DATA] = {2, K_INFO, "Game folder", true,
		"The folder with STATIC and USECODE: --data, else U8_DATA, else data= in u8port.cfg, "
		"else the folder the program is in."},
	[R_HOME] = {2, K_INFO, "Home folder", true,
		"Settings, saved games and recordings. --home or U8_HOME moves it."},
	[R_RECORD] = {2, K_SWITCH, "Record sessions", true,
		"Each session's input goes to recordings/ in the home folder (the newest five are "
		"kept), so it can be replayed with --replay. --no-recording for one run."},
	[R_AT_START] = {2, K_SWITCH, "Show this at start", true,
		"Opens this screen before the game starts. F11 (Cmd+, on a Mac) opens it any time."},
};

static const char *const tab_names[3] = {"Sound", "Display", "Game"};

static MTY_Atomic32 is_open;
/* App thread. */
static int tab;
static int selected = -1;       /* a row, or -1 for the tab bar */
static bool dragging;
static int volume = 100;
static int scale = 3;
static bool aspect = true;
static bool integer = true;
static int cell;                /* the font's scale, as last drawn */
static int origin_x, origin_y;  /* the panel's corner, in canvas pixels */
static MTY_App *last_app;
static MTY_Window last_window = -1;

static const char *setting_music(void)
{
	const char *m = setting_get("music");

	return m != NULL && (strcmp(m, "roland") == 0 || strcmp(m, "adlib") == 0) ? m : "auto";
}

void settings_init(void)
{
	const port_options *o = options();

	volume = setting_int("volume", 100, 0, 100);
	scale = o->scale >= 1 && o->scale <= 8 ? o->scale : setting_int("scale", 3, 1, 8);
	aspect = o->no_aspect ? false : setting_bool("aspect", true);
	integer = o->no_integer ? false : setting_bool("integer", true);
}

bool settings_open(void)
{
	return MTY_Atomic32Get(&is_open) != 0;
}

void settings_show(bool show)
{
	bool was = settings_open();

	MTY_Atomic32Set(&is_open, show);
	if (was && !show)
		events_release_all();
	if (show) {
		selected = -1;
		dragging = false;
	}
}

int settings_scale(void)
{
	return scale;
}

bool settings_aspect(void)
{
	return aspect;
}

bool settings_integer(void)
{
	return integer;
}

int settings_volume(void)
{
	return volume;
}

int64_t settings_hold(void)
{
	int64_t start;

	if (!settings_open())
		return 0;
	start = backend_elapsed_us();
	while (settings_open())
		MTY_Sleep(10);
	return backend_elapsed_us() - start;
}

/* ---- Rows ---- */

static int tab_rows(int t, int *list)
{
	int n = 0;

	for (int r = 0; r < R_COUNT; r++) {
		if (rows[r].tab == t)
			list[n++] = r;
	}
	return n;
}

/* The largest scale whose window fits the screen. */
static int largest_scale(void)
{
	MTY_Size screen;
	float dpi;
	int fit = 8;

	if (last_app == NULL || last_window < 0)
		return 8;
	screen = MTY_WindowGetScreenSize(last_app, last_window);
	dpi = MTY_WindowGetScreenScale(last_app, last_window);
	if (dpi <= 0.0f)
		dpi = 1.0f;
	while (fit > 1 && (fit * 320 * dpi > screen.w * 0.95f
		|| fit * (aspect ? 240 : 200) * dpi > screen.h * 0.9f))
		fit--;
	return fit;
}

void settings_apply_window(MTY_App *app, MTY_Window window)
{
	MTY_Frame frame;
	int s;

	last_app = app;
	last_window = window;
	if (MTY_WindowIsFullscreen(app, window))
		return;
	s = scale < largest_scale() ? scale : largest_scale();
	frame = MTY_WindowGetFrame(app, window);
	frame.size.w = (uint32_t) (s * 320);
	frame.size.h = (uint32_t) (s * (aspect ? 240 : 200));
	MTY_WindowSetFrame(app, window, &frame);
}

static void value_text(int r, char *out, size_t size)
{
	const char *m;

	switch (r) {
	case R_MUSIC:
		m = setting_music();
		snprintf(out, size, "%s (playing %s)", strcmp(m, "roland") == 0 ? "Roland MT-32"
			: strcmp(m, "adlib") == 0 ? "AdLib" : "Automatic",
			plat_music_device() == 'r' ? "MT-32" : "AdLib");
		break;
	case R_ROMS:
		snprintf(out, size, "%s: %s", audio_mt32_found() ? "found" : "not found", audio_rom_dir());
		break;
	case R_VOLUME:
		snprintf(out, size, "%d", volume);
		break;
	case R_FULLSCREEN:
		snprintf(out, size, "%s", last_app != NULL && MTY_WindowIsFullscreen(last_app, last_window) ? "On" : "Off");
		break;
	case R_SCALE:
		snprintf(out, size, "%dx", scale < largest_scale() ? scale : largest_scale());
		break;
	case R_ASPECT:
		snprintf(out, size, "%s", aspect ? "On" : "Off");
		break;
	case R_INTEGER:
		snprintf(out, size, "%s", integer ? "On" : "Off");
		break;
	case R_DATA:
		snprintf(out, size, "%s", files_root());
		break;
	case R_HOME:
		snprintf(out, size, "%s", home_dir());
		break;
	case R_RECORD:
		snprintf(out, size, "%s", setting_bool("recording", true) ? "On" : "Off");
		break;
	case R_AT_START:
		snprintf(out, size, "%s", setting_bool("settings-at-start", true) ? "On" : "Off");
		break;
	default:
		out[0] = '\0';
	}
}

/* Changes the row's value one step (way +1 or -1). */
static void change(int r, int way)
{
	static const char *const musics[3] = {"auto", "roland", "adlib"};

	switch (r) {
	case R_MUSIC: {
		int i = 0;

		while (i < 2 && strcmp(musics[i], setting_music()) != 0)
			i++;
		setting_set("music", musics[(i + 3 + way) % 3]);
		break;
	}
	case R_VOLUME:
		volume += 10 * way;
		volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
		setting_set_int("volume", volume);
		break;
	case R_FULLSCREEN:
		if (last_app != NULL) {
			bool full = !MTY_WindowIsFullscreen(last_app, last_window);

			MTY_WindowSetFullscreen(last_app, last_window, full);
			setting_set_bool("fullscreen", full);
			if (!full)
				settings_apply_window(last_app, last_window);
		}
		break;
	case R_SCALE: {
		int top = largest_scale();

		scale = (scale > top ? top : scale) + way;
		scale = scale < 1 ? top : scale > top ? 1 : scale;
		setting_set_int("scale", scale);
		if (last_app != NULL)
			settings_apply_window(last_app, last_window);
		break;
	}
	case R_ASPECT:
		aspect = !aspect;
		setting_set_bool("aspect", aspect);
		if (last_app != NULL)
			settings_apply_window(last_app, last_window);
		break;
	case R_INTEGER:
		integer = !integer;
		setting_set_bool("integer", integer);
		break;
	case R_RECORD:
		setting_set_bool("recording", !setting_bool("recording", true));
		break;
	case R_AT_START:
		setting_set_bool("settings-at-start", !setting_bool("settings-at-start", true));
		break;
	default:
		break;
	}
}

/* ---- Input ---- */

static void move_row(int way)
{
	int list[R_COUNT], n = tab_rows(tab, list), at = -1;

	for (int i = 0; i < n; i++) {
		if (list[i] == selected)
			at = i;
	}
	at += way;
	if (at < -1)
		at = n - 1;
	if (at >= n)
		at = -1;
	selected = at < 0 ? -1 : list[at];
}

static void key(MTY_Key k)
{
	switch (k) {
	case MTY_KEY_ESCAPE:
	case MTY_KEY_F11:
		settings_show(false);
		break;
	case MTY_KEY_TAB:
		tab = (tab + 1) % 3;
		selected = -1;
		break;
	case MTY_KEY_UP:
		move_row(-1);
		break;
	case MTY_KEY_DOWN:
		move_row(1);
		break;
	case MTY_KEY_LEFT:
	case MTY_KEY_RIGHT:
		if (selected < 0) {
			tab = (tab + (k == MTY_KEY_LEFT ? 2 : 1)) % 3;
		} else if (rows[selected].kind != K_INFO) {
			change(selected, k == MTY_KEY_LEFT ? -1 : 1);
		}
		break;
	case MTY_KEY_ENTER:
	case MTY_KEY_NP_ENTER:
	case MTY_KEY_SPACE:
		if (selected >= 0 && rows[selected].kind != K_INFO)
			change(selected, 1);
		break;
	default:
		break;
	}
}

/* Canvas pixels to panel cells. */
static bool cell_at(int32_t wx, int32_t wy, MTY_Size view, int *cx, int *cy, int *px)
{
	int32_t vx, vy, vw, vh;

	if (cell <= 0)
		return false;
	video_view_rect(view, &vx, &vy, &vw, &vh);
	*px = (int) (wx - vx - origin_x);
	*cx = *px / (8 * cell);
	*cy = (int) (wy - vy - origin_y) / (LINE * cell);
	return *px >= 0 && wy - vy - origin_y >= 0 && *cx < PANEL_COLUMNS && *cy < PANEL_ROWS;
}

static void slide_to(int px)
{
	int left = VALUE_COLUMN * 8 * cell, width = 20 * 8 * cell;
	int v = (px - left) * 100 / width;

	v = (v + 5) / 10 * 10;
	volume = v < 0 ? 0 : v > 100 ? 100 : v;
	setting_set_int("volume", volume);
}

static void click(int cx, int cy, int px)
{
	int list[R_COUNT], n = tab_rows(tab, list);

	if (cy == 3 && cx >= 2 && cx < 2 + 3 * TAB_WIDTH) {
		tab = (cx - 2) / TAB_WIDTH;
		selected = -1;
		return;
	}
	for (int i = 0; i < n; i++) {
		if (cy != FIRST_ROW + 2 * i)
			continue;
		selected = list[i];
		if (rows[selected].kind == K_SLIDER) {
			dragging = true;
			slide_to(px);
		} else if (rows[selected].kind != K_INFO) {
			change(selected, 1);
		}
	}
}

bool settings_event(const MTY_Event *event, MTY_App *app, MTY_Window window)
{
	MTY_Size view = MTY_WindowGetSize(app, window);
	int cx, cy, px;
	bool opener;

	last_app = app;
	last_window = window;
	if (event->type == MTY_EVENT_KEY) {
#ifdef __APPLE__
		opener = event->key.key == MTY_KEY_F11
			|| (event->key.key == MTY_KEY_COMMA && (event->key.mod & MTY_MOD_WIN));
#else
		opener = event->key.key == MTY_KEY_F11;
#endif
		if (!settings_open()) {
			if (opener && event->key.pressed) {
				settings_show(true);
				return true;
			}
			/* The release of the key that opened or closed the screen. */
			return opener;
		}
		if (event->key.pressed) {
			if (opener)
				settings_show(false);
			else
				key(event->key.key);
		}
		return true;
	}
	if (!settings_open())
		return false;
	switch (event->type) {
	case MTY_EVENT_BUTTON:
		if (event->button.button != MTY_BUTTON_LEFT)
			break;
		if (!event->button.pressed) {
			dragging = false;
			break;
		}
		if (cell_at(event->button.x, event->button.y, view, &cx, &cy, &px))
			click(cx, cy, px);
		break;
	case MTY_EVENT_MOTION:
		if (dragging && selected == R_VOLUME && !event->motion.relative) {
			cell_at(event->motion.x, event->motion.y, view, &cx, &cy, &px);
			slide_to(px);
		}
		break;
	case MTY_EVENT_FOCUS:
		dragging = false;
		break;
	default:
		break;
	}
	return true;
}

/* ---- Drawing ---- */

static uint32_t *canvas;
static int canvas_w, canvas_h;

static uint32_t rgba(uint32_t c)
{
	return 0xFF000000u | (c & 0xFFu) << 16 | (c & 0xFF00u) | (c >> 16 & 0xFFu);
}

static void fill(int x, int y, int w, int h, uint32_t c)
{
	uint32_t v = rgba(c);

	x += origin_x;
	y += origin_y;
	for (int j = y < 0 ? 0 : y; j < y + h && j < canvas_h; j++) {
		for (int i = x < 0 ? 0 : x; i < x + w && i < canvas_w; i++)
			canvas[j * canvas_w + i] = v;
	}
}

static void text(int column, int row, const char *s, int max, uint32_t c)
{
	uint32_t v = rgba(c);
	int g = cell;

	for (int n = 0; s[n] != '\0' && n < max; n++) {
		const uint8_t *glyph = VgaFont8[(uint8_t) s[n]];
		int x0 = origin_x + (column + n) * 8 * g, y0 = origin_y + row * LINE * g + g;

		/* A path too long for its space ends in "..". */
		if (n == max - 1 && s[n + 1] != '\0')
			glyph = VgaFont8[(uint8_t) '.'];
		for (int y = 0; y < 8 * g; y++) {
			int py = y0 + y;

			if (py < 0 || py >= canvas_h)
				continue;
			for (int x = 0; x < 8 * g; x++) {
				int pxl = x0 + x;

				if (pxl >= 0 && pxl < canvas_w && (glyph[y / g] & (0x80 >> (x / g))))
					canvas[py * canvas_w + pxl] = v;
			}
		}
	}
}

/* Wraps s into lines of width columns, at most lines of them. */
static void wrapped(int column, int row, const char *s, int width, int lines, uint32_t c)
{
	char line[PANEL_COLUMNS + 1];

	while (*s != '\0' && lines-- > 0) {
		int n = (int) strlen(s), cut = n;

		if (n > width) {
			cut = width;
			while (cut > 0 && s[cut] != ' ')
				cut--;
			if (cut == 0)
				cut = width;
		}
		snprintf(line, sizeof line, "%.*s", cut, s);
		text(column, row++, line, width, c);
		s += cut;
		while (*s == ' ')
			s++;
	}
}

/* The screen over the game's frame (frame is 320x200 RGBA), at the window's pixel size. The
 * returned canvas is valid until the next call; NULL when the view is empty. */
const uint32_t *settings_render(const uint8_t (*frame)[4], int w, int h)
{
	int list[R_COUNT], n;
	int pw, ph;
	char value[PATH_SIZE];

	if (w <= 0 || h <= 0)
		return NULL;
	if (w != canvas_w || h != canvas_h) {
		free(canvas);
		canvas = malloc((size_t) w * (size_t) h * sizeof *canvas);
		canvas_w = w;
		canvas_h = h;
	}
	/* The game, dimmed. */
	for (int y = 0; y < h; y++) {
		int sy = y * 200 / h;

		for (int x = 0; x < w; x++) {
			const uint8_t *p = frame[sy * 320 + x * 320 / w];

			canvas[y * w + x] = 0xFF000000u | (uint32_t) (p[2] * 3 / 10) << 16
				| (uint32_t) (p[1] * 3 / 10) << 8 | (uint32_t) (p[0] * 3 / 10);
		}
	}
	cell = w / (8 * (PANEL_COLUMNS + 2)) < h / (LINE * (PANEL_ROWS + 2))
		? w / (8 * (PANEL_COLUMNS + 2)) : h / (LINE * (PANEL_ROWS + 2));
	if (cell < 1)
		cell = 1;
	pw = PANEL_COLUMNS * 8 * cell;
	ph = PANEL_ROWS * LINE * cell;
	origin_x = (w - pw) / 2;
	origin_y = (h - ph) / 2;

	fill(0, 0, pw, ph, C_BACK);
	fill(0, 0, pw, 2 * cell, C_BORDER);
	fill(0, ph - 2 * cell, pw, 2 * cell, C_BORDER);
	fill(0, 0, 2 * cell, ph, C_BORDER);
	fill(pw - 2 * cell, 0, 2 * cell, ph, C_BORDER);
	text(2, 1, "Ultima VIII portable: settings", PANEL_COLUMNS - 4, C_SELECT);

	for (int t = 0; t < 3; t++) {
		int x = (2 + t * TAB_WIDTH) * 8 * cell;
		bool here = t == tab;

		if (here && selected < 0)
			fill(x - 4 * cell, 3 * LINE * cell, (TAB_WIDTH - 1) * 8 * cell, LINE * cell, C_SELECT);
		else if (here)
			fill(x - 4 * cell, 4 * LINE * cell - cell, (TAB_WIDTH - 1) * 8 * cell, cell, C_SELECT);
		text(2 + t * TAB_WIDTH, 3, tab_names[t], TAB_WIDTH - 1, here && selected < 0 ? C_BACK : here ? C_SELECT : C_DIM);
	}

	n = tab_rows(tab, list);
	for (int i = 0; i < n; i++) {
		int r = list[i], y = FIRST_ROW + 2 * i;
		bool sel = r == selected;
		uint32_t fg = sel ? C_BACK : rows[r].kind == K_INFO ? C_DIM : C_TEXT;
		int room = PANEL_COLUMNS - VALUE_COLUMN - 2;

		if (sel)
			fill(8 * cell, y * LINE * cell, pw - 16 * cell, LINE * cell, C_SELECT);
		text(3, y, rows[r].label, VALUE_COLUMN - 4, fg);
		if (rows[r].kind == K_SLIDER) {
			int left = VALUE_COLUMN * 8 * cell, wide = 20 * 8 * cell;

			fill(left, y * LINE * cell + 3 * cell, wide, 4 * cell, sel ? C_BACK : C_DIM);
			fill(left + cell, y * LINE * cell + 4 * cell, (wide - 2 * cell) * volume / 100, 2 * cell, sel ? C_SELECT : C_TEXT);
			snprintf(value, sizeof value, "%d", volume);
			text(VALUE_COLUMN + 21, y, value, 4, fg);
		} else {
			value_text(r, value, sizeof value);
			text(VALUE_COLUMN, y, value, room, fg);
		}
	}

	if (selected >= 0) {
		char help[512];

		snprintf(help, sizeof help, "%s%s", rows[selected].help,
			rows[selected].next_start && rows[selected].kind != K_INFO ? " Used from the next start." : "");
		wrapped(3, PANEL_ROWS - 8, help, PANEL_COLUMNS - 6, 4, C_DIM);
	}
	text(3, PANEL_ROWS - 2, "Arrows move, Enter changes, Tab, Esc closes", PANEL_COLUMNS - 6, C_DIM);
	return canvas;
}
