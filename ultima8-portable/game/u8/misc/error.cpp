// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: MISC\ERROR.C

#include "u8port.h"

// #include <conio.h>  (DOS only)
#include <stdarg.h>
#include <stdio.h>
#include "error.h"
#include "ferror.h"
#include "cprotmem.h"
#include "appdebug.h"
#include "system.h"

// Inline in the shared header when this file was built.
U8_INLINE SimpleVirtualString::operator char *(void) { return string; }

int16_t traceLevel;

void halt(char *file, int16_t line)
{
	if (ProtMemoryManager::failedLastRequest)
		outOfMemory(file, line);
	else
		Fatal("Halting in file %s, line %d.\n", file, line);
}

void halt(char *file, int16_t line, char *format, ...)
{
	va_list args;

	if (format)
		FORMAT_WORKSTRING(format, args);
	else
		*(char *)WorkString = 0;
	textmode(3);
	trace(TRACE_ALWAYS, WorkString);
	trace(TRACE_ALWAYS, "\r\nHalting in file %s, line %d.\n", file, line);
	trace(TRACE_ALWAYS, "\r\n");
	Fatal(1);
}

void outOfMemory(char *file, int16_t line)
{
	textmode(3);
	trace(TRACE_ALWAYS, "Out of memory in file %s, line %d.\n\r", file, line);
	applicationOutOfMemory(100 - FRAGMENT_0_PERCENTAGE);
	Fatal(1);
}

void outOfMemory(char *file, int16_t line, char *format, ...)
{
	va_list args;

	if (format)
		FORMAT_WORKSTRING(format, args);
	else
		*(char *)WorkString = 0;
	textmode(3);
	trace(TRACE_ALWAYS, WorkString);
	trace(TRACE_ALWAYS, "\r\n");
	trace(TRACE_ALWAYS, "Out of memory in file %s, line %d.\n", file, line);
	applicationOutOfMemory(100 - FRAGMENT_0_PERCENTAGE);
	Fatal(1);
}

void unableToOpenFile(char *file, int16_t line, char *name)
{
	textmode(3);
	trace(TRACE_ALWAYS, "Unable to open file %s in %s, line %d\n", name, file, line);
	Fatal(1);
}

void trace(TraceLevel level, char *format, ...)
{
	va_list args;

	FORMAT_WORKSTRING(format, args);
	if (level >= traceLevel)
		printf(WorkString);
}

void trace(TraceLevel level, int16_t x, int16_t y, char *format, ...)
{
	va_list args;

	FORMAT_WORKSTRING(format, args);
	if (level >= traceLevel)
	{
		gotoxy(x, y);
		printf(WorkString);
	}
}

// Like trace, then waits for a key.
void traceGet(TraceLevel level, char *format, ...)
{
	va_list args;

	FORMAT_WORKSTRING(format, args);
	if (level >= traceLevel)
	{
		printf(WorkString);
		getch();
	}
}

void traceGet(TraceLevel level, int16_t x, int16_t y, char *format, ...)
{
	va_list args;

	FORMAT_WORKSTRING(format, args);
	if (level >= traceLevel)
	{
		gotoxy(x, y);
		printf(WorkString);
		getch();
	}
}
