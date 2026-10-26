/* Borland library calls the game used, on the host's C library and the platform layer. */
#include "u8port.h"
#include "plat.h"

int16_t u8_stricmp(const char *a, const char *b)
{
	for (;; a++, b++) {
		int ca = toupper((unsigned char) *a), cb = toupper((unsigned char) *b);

		if (ca != cb)
			return (int16_t) (ca - cb);
		if (ca == 0)
			return 0;
	}
}

int16_t u8_strnicmp(const char *a, const char *b, size_t n)
{
	for (; n > 0; n--, a++, b++) {
		int ca = toupper((unsigned char) *a), cb = toupper((unsigned char) *b);

		if (ca != cb)
			return (int16_t) (ca - cb);
		if (ca == 0)
			return 0;
	}
	return 0;
}

static char *unsigned_to_text(uint32_t value, char *out, int16_t radix)
{
	char digits[40];
	int n = 0;

	if (radix < 2 || radix > 36)
		radix = 10;
	do {
		int d = (int) (value % (uint32_t) radix);

		digits[n++] = (char) (d < 10 ? '0' + d : 'a' + d - 10);
		value /= (uint32_t) radix;
	} while (value != 0);
	for (int i = 0; i < n; i++)
		out[i] = digits[n - 1 - i];
	out[n] = 0;
	return out;
}

/* itoa took an int: a minus sign only for radix 10, else the 16-bit pattern. */
char *u8_itoa(int32_t value, char *out, int16_t radix)
{
	int16_t v = (int16_t) value;

	if (radix == 10 && v < 0) {
		out[0] = '-';
		unsigned_to_text((uint32_t) -(int32_t) v, out + 1, radix);
		return out;
	}
	return unsigned_to_text((uint16_t) v, out, radix);
}

char *u8_ltoa(int32_t value, char *out, int16_t radix)
{
	if (radix == 10 && value < 0) {
		out[0] = '-';
		unsigned_to_text((uint32_t) 0 - (uint32_t) value, out + 1, radix);
		return out;
	}
	return unsigned_to_text((uint32_t) value, out, radix);
}

char *u8_ultoa(uint32_t value, char *out, int16_t radix)
{
	return unsigned_to_text(value, out, radix);
}

char *u8_strupr(char *s)
{
	for (char *p = s; *p; p++)
		*p = (char) toupper((unsigned char) *p);
	return s;
}

char *u8_strlwr(char *s)
{
	for (char *p = s; *p; p++)
		*p = (char) tolower((unsigned char) *p);
	return s;
}

/* Borland's rand(): seed = seed * 0x015A4E35 + 1, 15 bits from the high word. random(n) is
 * (rand() * n) / 0x8000, and randomize() seeds from the clock. */
static uint32_t borland_seed = 1;

static int16_t borland_rand(void)
{
	borland_seed = borland_seed * 0x015A4E35u + 1u;
	return (int16_t) ((borland_seed >> 16) & 0x7FFF);
}

uint32_t u8_rand_state(void)
{
	return borland_seed;
}

int16_t u8_rand(void)
{
	return borland_rand();
}

int16_t u8_random(int16_t range)
{
	return (int16_t) (((int32_t) borland_rand() * range) / 0x8000);
}

void u8_randomize(void)
{
	borland_seed = plat_time_now();
	borland_rand();
}

/* ---- printf family ---- */

/* Rewrites one conversion's length modifiers for the host: Borland's l (32-bit) becomes none
 * for integers (int is 32 bits on every host this builds for) and stays for %lf; h stays; F and
 * N (far and near) are dropped. */
static void host_format(const char *format, char *out, size_t size)
{
	size_t o = 0;

	while (*format && o + 4 < size) {
		if (*format != '%') {
			out[o++] = *format++;
			continue;
		}
		out[o++] = *format++;
		if (*format == '%') {
			out[o++] = *format++;
			continue;
		}
		/* flags, width, precision */
		while (*format && strchr("-+ #0123456789.*", *format) && o + 4 < size)
			out[o++] = *format++;
		bool longer = false, shorter = false;
		while (*format == 'l' || *format == 'F' || *format == 'N' || *format == 'h' || *format == 'L') {
			if (*format == 'l')
				longer = true;
			else if (*format == 'h' || *format == 'L') {
				shorter |= *format == 'h';
				out[o++] = *format;
			}
			format++;
		}
		if (longer && (*format == 'f' || *format == 'e' || *format == 'g' || *format == 'E' || *format == 'G'))
			out[o++] = 'l';
		/* An int was 16 bits: unsigned conversions of one print 16 bits (-1 as FFFF). */
		if (!longer && !shorter && *format && strchr("ouxX", *format))
			out[o++] = 'h';
		if (*format)
			out[o++] = *format++;
	}
	out[o] = 0;
}

int u8_vsnprintf(char *out, size_t size, const char *format, va_list args)
{
	char fixed[1024];

	host_format(format, fixed, sizeof fixed);
	return vsnprintf(out, size, fixed, args);
}

int u8_vsprintf(char *out, const char *format, va_list args)
{
	/* The game's buffers are all short of this; it never measured them either. */
	return u8_vsnprintf(out, 4096, format, args);
}

int u8_sprintf(char *out, const char *format, ...)
{
	va_list args;
	int n;

	va_start(args, format);
	n = u8_vsprintf(out, format, args);
	va_end(args);
	return n;
}

int u8_vprintf(const char *format, va_list args)
{
	char text[4096];
	int n = u8_vsnprintf(text, sizeof text, format, args);

	plat_log(text);
	return n;
}

int u8_printf(const char *format, ...)
{
	va_list args;
	int n;

	va_start(args, format);
	n = u8_vprintf(format, args);
	va_end(args);
	return n;
}

// u8port.h sends the game's vfprintf here; this one is the C library's.
#undef vfprintf

int u8_vfprintf(FILE *f, const char *format, va_list args)
{
	char fixed[1024];

	host_format(format, fixed, sizeof fixed);
	return vfprintf(f, fixed, args);
}

int u8_fprintf(FILE *f, const char *format, ...)
{
	va_list args;
	int n;

	va_start(args, format);
	n = u8_vfprintf(f, format, args);
	va_end(args);
	return n;
}

/* sscanf: no modifier stores a 16-bit int, l a 32-bit one. */
int u8_sscanf(const char *in, const char *format, ...)
{
	char fixed[512];
	size_t o = 0;
	const char *f = format;

	while (*f && o + 4 < sizeof fixed) {
		if (*f != '%') {
			fixed[o++] = *f++;
			continue;
		}
		fixed[o++] = *f++;
		if (*f == '%' || *f == '*') {
			fixed[o++] = *f++;
			continue;
		}
		while (*f >= '0' && *f <= '9')
			fixed[o++] = *f++;
		bool longer = false, shorter = false;
		while (*f == 'l' || *f == 'h' || *f == 'F' || *f == 'N') {
			if (*f == 'l')
				longer = true;
			if (*f == 'h')
				shorter = true;
			f++;
		}
		if (strchr("diouxX", *f) && *f) {
			if (!longer || shorter)
				fixed[o++] = 'h';
		} else if (longer && strchr("feg", *f) && *f)
			fixed[o++] = 'l';
		if (*f)
			fixed[o++] = *f++;
	}
	fixed[o] = 0;

	va_list args;
	int n;

	va_start(args, format);
	n = vsscanf(in, fixed, args);
	va_end(args);
	return n;
}

/* ---- Console ---- */

int u8_cputs(const char *s)
{
	plat_console_write(s);
	return 0;
}

int u8_cprintf(const char *format, ...)
{
	char text[2048];
	va_list args;
	int n;

	va_start(args, format);
	n = u8_vsnprintf(text, sizeof text, format, args);
	va_end(args);
	plat_console_write(text);
	return n;
}

void u8_gotoxy(int16_t x, int16_t y)
{
	plat_console_goto(x, y);
}

int16_t u8_wherex(void)
{
	return plat_console_x();
}

int16_t u8_wherey(void)
{
	return plat_console_y();
}

void u8_clrscr(void)
{
	plat_console_goto(1, 1);
}

void u8_textmode(int16_t mode)
{
	(void) mode;
}

void u8_window(int16_t left, int16_t top, int16_t right, int16_t bottom)
{
	(void) left;
	(void) top;
	(void) right;
	(void) bottom;
}

int16_t u8_getch(void)
{
	return plat_key_get();
}

int16_t u8_kbhit(void)
{
	return plat_key_available();
}

char *u8_gets(char *buffer)
{
	int n = 0;
	char echo[2] = {0, 0};

	for (;;) {
		int16_t c = plat_key_get();

		if (c == 0) {
			plat_key_get();	/* a key without ASCII: its scan code */
			continue;
		}
		if (c == '\r') {
			buffer[n++] = '\r';
			break;
		}
		if (c == '\b') {
			if (n > 0) {
				n--;
				plat_console_write("\b \b");
			}
			continue;
		}
		if (n < 250) {
			buffer[n++] = (char) c;
			echo[0] = (char) c;
			plat_console_write(echo);
		}
	}
	buffer[n] = 0;
	plat_console_write("\r\n");
	return buffer;
}

void u8_delay(uint16_t milliseconds)
{
	uint32_t until = plat_milliseconds() + milliseconds;

	while ((int32_t) (until - plat_milliseconds()) > 0)
		plat_yield();
}

uint32_t u8_coreleft(void)
{
	return 0x2000000u;
}

/* ---- Stack ---- */

static char *stack_top;

void u8_set_stack_top(void *top)
{
	stack_top = (char *)top;
}

int u8_on_stack(const void *p)
{
	char here;

	if (stack_top == NULL)
		return 0;
	/* The stack grows down from stack_top to here (and on, as far as 64 MB). */
	return (const char *)p >= &here - 0x10000 && (const char *)p <= stack_top + 0x1000
		&& stack_top - (const char *)p < 0x4000000;
}

// ---- allocation ----
// Borland's new, new[], malloc and farmalloc all came from the one heap, and the game frees
// with whichever it likes: new[] blocks with delete or farfree, farmalloc blocks with delete.
// So all of them are malloc here, and every pairing of them stays good for the plain data
// the game allocates. (Arrays of objects with destructors still need delete[].) Every block
// starts zeroed, so whatever the game reads before it writes is the same on every run, which
// recordings rely on to replay (the host's heap is shared with other threads).
#include <new>

void *operator new(size_t size)
{
	// Borland's new gave 0 when memory ran out, and the game checks for it (built with
	// -fcheck-new so the compiler keeps those checks).
	return ::calloc(1, size ? size : 1);
}

void *u8_malloc(size_t size)
{
	return ::calloc(1, size ? size : 1);
}

void *operator new[](size_t size)
{
	return operator new(size);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept
{
	return ::calloc(1, size ? size : 1);
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
	return ::calloc(1, size ? size : 1);
}

void operator delete(void *p) noexcept { ::free(p); }
void operator delete[](void *p) noexcept { ::free(p); }
void operator delete(void *p, size_t) noexcept { ::free(p); }
void operator delete[](void *p, size_t) noexcept { ::free(p); }

// ---- exit ----
#undef exit

void u8_exit(int code)
{
	plat_exit((int16_t)code);
}
