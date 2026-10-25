/* Portable base for the game: fixed-width types, the C library, and Borland helpers.
 *
 * Every game source includes this first. The game was written for Borland C++ 3.1 in the
 * large model: int and unsigned are 16 bits, long 32, pointers 32-bit far pointers, char
 * signed, structs packed to one byte. The sources spell the integer types out (int16_t,
 * uint16_t, int32_t, ...); this header supplies the Borland library calls they use, and
 * packs the game's structs as Borland laid them out. System headers come before the
 * packing pragma so only game structs are packed.
 */
#ifndef U8PORT_H
#define U8PORT_H

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
#include <new>
extern "C" {
#endif

/* ---- Borland string and number helpers with no standard equivalent ---- */
int16_t u8_stricmp(const char *a, const char *b);
int16_t u8_strnicmp(const char *a, const char *b, size_t n);
char *u8_itoa(int32_t value, char *out, int16_t radix);
char *u8_ltoa(int32_t value, char *out, int16_t radix);
char *u8_ultoa(uint32_t value, char *out, int16_t radix);
char *u8_strupr(char *s);
char *u8_strlwr(char *s);
/* Borland's random(n) and randomize(): 0 .. n - 1 from its own generator. */
int16_t u8_random(int16_t range);
void u8_randomize(void);

/* ---- printf family ----
 * Format strings are the game's, written for 16-bit int: %d takes an int (an int16_t argument
 * is promoted to one, as before), %ld a 32-bit long. These rewrite the length modifiers for
 * the host before calling its library, so %ld reads an int32_t on every platform, and %Fs,
 * %Np (Borland's far and near modifiers) read plain pointers. */
int u8_vsnprintf(char *out, size_t size, const char *format, va_list args);
int u8_vsprintf(char *out, const char *format, va_list args);
int u8_sprintf(char *out, const char *format, ...);
int u8_printf(const char *format, ...);
int u8_vprintf(const char *format, va_list args);
int u8_fprintf(FILE *f, const char *format, ...);
int u8_vfprintf(FILE *f, const char *format, va_list args);
/* scanf reads: %d stores 16 bits, %ld 32 bits, as Borland did. */
int u8_sscanf(const char *in, const char *format, ...);

/* ---- Console ----
 * The game printed to the text screen (and, in graphics mode, BIOS text drew on the game
 * screen). printf and the conio calls go to the platform's console. */
int u8_cprintf(const char *format, ...);
int u8_cputs(const char *s);
void u8_gotoxy(int16_t x, int16_t y);
int16_t u8_wherex(void);
int16_t u8_wherey(void);
void u8_clrscr(void);
void u8_textmode(int16_t mode);
void u8_window(int16_t left, int16_t top, int16_t right, int16_t bottom);
int16_t u8_getch(void);
int16_t u8_kbhit(void);
/* Reads a line from the keyboard with echo, as DOS console input did: the line keeps its
 * ending CR. */
char *u8_gets(char *buffer);
void u8_delay(uint16_t milliseconds);

/* ---- Stack ----
 * Some objects behaved differently when built on the stack, which DOS told from their
 * selector (that of SS). The game thread records the top of its stack first thing. */
void u8_set_stack_top(void *top);
int u8_on_stack(const void *p);

/* ---- Memory ----
 * farmalloc and friends are malloc now; coreleft reports plenty. */
uint32_t u8_coreleft(void);

#ifdef __cplusplus
}
#endif

#define stricmp u8_stricmp
#define strcmpi u8_stricmp
#define strnicmp u8_strnicmp
#define strncmpi u8_strnicmp
#define itoa(v, s, r) u8_itoa((v), (s), (r))
#define ltoa(v, s, r) u8_ltoa((v), (s), (r))
#define ultoa(v, s, r) u8_ultoa((v), (s), (r))
#define strupr u8_strupr
#define strlwr u8_strlwr
#define random(n) u8_random(n)
#define randomize u8_randomize

#define sprintf u8_sprintf
#define vsprintf u8_vsprintf
#define printf u8_printf
#define vprintf u8_vprintf
#define fprintf u8_fprintf
#define vfprintf u8_vfprintf
#define sscanf u8_sscanf
#define cprintf u8_cprintf
#define cputs u8_cputs
#define gotoxy u8_gotoxy
#define wherex u8_wherex
#define wherey u8_wherey
#define clrscr u8_clrscr
#define textmode u8_textmode
#define getch u8_getch
#define kbhit u8_kbhit
#define gets u8_gets
#define delay u8_delay

#define farmalloc(n) ::malloc(n)
#define farcalloc(n, s) ::calloc((n), (s))
#define farrealloc(p, n) ::realloc((p), (n))
#define farfree(p) ::free(p)
#define coreleft u8_coreleft
#define farcoreleft u8_coreleft

/* Far pointers are ordinary pointers now. */
#define _fmemcpy memcpy
#define _fmemmove memmove
#define _fmemset memset
#define _fmemcmp memcmp
#define _fstrlen strlen
#define _fstrcpy strcpy
#define _fstrncpy strncpy
#define _fstrcat strcat
#define _fstrcmp strcmp
#define _fstrchr strchr

#ifdef __cplusplus
/* Borland's stdlib.h made these templates in C++. */
template <class T> inline T min(T a, T b) { return a < b ? a : b; }
template <class T> inline T max(T a, T b) { return a > b ? a : b; }
#endif

#include "u8dos.h"

/* The game's structs are byte-packed, as Borland C++ laid them out without -a. */
// The decompiled files define the inline functions they use themselves. Borland emitted each
// one a file did not expand as a "virtual segment", of which the linker kept one copy, and
// files without a definition called that copy. U8_INLINE has every file that defines one
// emit it, so those calls link at any optimisation level.
#if defined(__GNUC__) || defined(__clang__)
#define U8_INLINE inline __attribute__((used))
#else
#define U8_INLINE inline
#endif

#pragma pack(1)

#endif
