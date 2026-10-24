#ifndef CTRACKER_H
#define CTRACKER_H

#include "u8port.h"

#include "cmouse.h"

// Hands mouse clicks to the debug tracker, and remembers the last few
// source places reported through SetArea.
class Tracker : public MouseHandler
{
public:
	static int16_t head;
	static char *area_file[10];
	static int16_t area_line[10];

	Tracker(void);
	static void set_area(char *, int16_t);
	void report_area(void);
	virtual void handle(int16_t, int16_t, int16_t, int16_t);
};

void SetArea(char *, int16_t);

#endif
