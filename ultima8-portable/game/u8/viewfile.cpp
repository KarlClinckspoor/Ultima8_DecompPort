// name: VIEWFILE
// Ported from VIEWFILE.ASM.
// Hand-written line splitter of ViewFile.

#include "u8port.h"

#include "debugump.h"

#define MAX_LINES 2000

// int ViewFile::parse(unsigned long *offsets, char *text, unsigned long length, unsigned long base)
// Ends each line of text with a NUL in place of the CR before its LF and stores base plus the
// line's start for every line. Returns the number of lines. The DOS build passed the text's
// linear address as base and scanned from the start of text's segment (always text itself
// under Phar Lap); the host passes 0 and keeps offsets into text.
int16_t ViewFile::parse(uint32_t *offsets, char *text, uint32_t len, uint32_t base)
{
	uint32_t edi = 0;
	uint32_t ecx = len;
	uint16_t lines = 0;

	do
	{
		*offsets++ = base + edi;
		// repne scasb for LF; stops after the LF, or when ecx runs out
		while (ecx != 0)
		{
			ecx--;
			if (text[edi++] == 0x0a)
				break;
		}
		text[edi - 2] = 0;
		lines++;
		if (lines == MAX_LINES)
			break;
	} while (ecx != 0);
	return (int16_t)lines;
}
