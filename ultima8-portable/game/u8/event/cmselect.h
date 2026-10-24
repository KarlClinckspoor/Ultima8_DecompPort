#ifndef CMSELECT_H
#define CMSELECT_H

#include "u8port.h"

#include "cmevent.h"
#include "cmouse.h"

class MouseSelectEvent;

class MouseSelectChronicler : public MouseEventChronicler
{
public:
	MouseSelectChronicler(MouseSelectEvent *, int16_t);
	MouseSelectChronicler(void *, int16_t, int16_t);
	virtual void handle(int16_t, int16_t, int16_t, int16_t);
	static void check_button_swap(void *);
	static uint8_t read_event(void *, int16_t);
	static uint8_t handedness;
};

class MouseSelectInspector : public MouseDevice
{
public:
	MouseSelectInspector(int16_t = 12);
	static MouseSelectEvent *poll(void);
	static MouseSelectEvent &poll(MouseSelectEvent &);
	static uint8_t check_was_moved(MouseSelectEvent *);
	static int16_t threshold;
	static MouseSelectEvent last_event;
};

#endif
