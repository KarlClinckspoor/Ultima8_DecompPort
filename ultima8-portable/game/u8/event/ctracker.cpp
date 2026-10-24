// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: EVENT\CTRACKER.C

#include "u8port.h"

#include <stdlib.h>

#include "anim.h"
#include "ctracker.h"

// Driver event bits for the three button presses.
#define BUTTON_PRESSES	0x2A

int16_t Tracker::head = 0;
char *Tracker::area_file[10];
int16_t Tracker::area_line[10];

Tracker::Tracker(void) :
	MouseHandler(BUTTON_PRESSES)
{
	head = 0;
}

void Tracker::set_area(char *file, int16_t line)
{
	if (++head >= 10)
		head = 0;
	area_file[head] = file;
	area_line[head] = line;
}

// Lists the remembered places, newest first.
void Tracker::report_area(void)
{
	int16_t n;
	char number[10];
	int16_t i;

	charGen.writeString(0, 0, "- Most Recent -");
	for (i = 0; i < 10; i++)
	{
		n = (head - i + 10) % 10;
		ltoa(area_line[n], number, 10);
		charGen.writeString(0, i * 14 + 14, number);
		charGen.writeString(50, i * 14 + 14, area_file[n]);
	}
	charGen.writeString(0, i * 14 + 14, "- Least Recent -");
}

void Tracker::handle(int16_t, int16_t buttons, int16_t x, int16_t y)
{
	handleTracker(x, y, buttons);
}

void SetArea(char *file, int16_t line)
{
	Tracker::set_area(file, line);
}
