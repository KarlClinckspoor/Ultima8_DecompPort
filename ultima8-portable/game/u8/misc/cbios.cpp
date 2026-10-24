// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: MISC\CBIOS.C

#include "u8port.h"
#include "plat.h"

#include "cbios.h"

// BIOS teletype output (int 10h function 0Eh), a newline written as LF then CR. In graphics
// mode the BIOS drew the text on the screen; the platform's console does the same, and also
// logs it.
void conputs(const char *s)
{
	char text[2] = {0, 0};
	char c;

	plat_log(s);
	while ((c = *s++) != 0)
	{
		for (;;)
		{
			text[0] = c;
			plat_console_write(text);
			if (c != '\n')
				break;
			c = '\r';
		}
	}
}

// int 16h function 0: ASCII, or the scan code + 100h for a key without one.
int16_t congetkey(void)
{
	uint16_t word = plat_key_bios();
	int16_t key = word & 0xff;

	if (key == 0)
		key = (word >> 8) | 0x100;
	return key;
}

// int 10h functions 3 and 2 on page 0 (the only page).
void congetcursor(CursorPosition &pos, char)
{
	pos.row = (char)(plat_console_y() - 1);
	pos.column = (char)(plat_console_x() - 1);
}

void consetcursor(CursorPosition &pos, char)
{
	plat_console_goto(pos.column + 1, pos.row + 1);
}

// Found the text mode's height by trying to park the cursor on rows 49 and 39; the host's
// console is always 25 rows.
ScanCodes congetscans(void)
{
	return SCANS_400;
}

void consetscans(ScanCodes)
{
}

// DOS character output (int 21h function 2), a newline written as LF then CR.
void dosputs(const char *s)
{
	plat_log(s);
}
