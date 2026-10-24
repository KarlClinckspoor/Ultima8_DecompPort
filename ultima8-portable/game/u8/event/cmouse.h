#ifndef CMOUSE_H
#define CMOUSE_H

#include "u8port.h"

// The mouse driver's event hook (NEWHOOK.ASM): events, buttons, x, y as int 33h gave them.
void mouseHook(uint16_t, uint16_t, int16_t, int16_t);
extern int16_t fakeStackOverflowFlag;

class MouseDevice
{
public:
	MouseDevice(void);
	~MouseDevice(void);
	static void init(void);
	static int16_t reset(void);
	static void install_mouse_hook(void);
	static void remove_mouse_hook(void);
	static void move_to(int16_t, int16_t);
	static void set_x_bounds(int16_t, int16_t);
	static void set_y_bounds(int16_t, int16_t);
	static void set_speed(int16_t, int16_t);
	static int16_t get_last_x(void);
	static int16_t get_last_y(void);
	static void freeze(void);
	static void release(void);
	static char is_installed(void);
	static int16_t get_number_of_buttons(void);
	static void set_bounds(int16_t, int16_t, int16_t, int16_t);
	static uint16_t original_handler_mask;
	static uint16_t original_handler[2];
	static uint8_t installed_flag;
	static uint16_t number_of_buttons;
	static uint16_t mouseLastX;
	static uint16_t mouseLastY;
};

class MouseHandler
{
public:
	uint16_t slot;
	uint16_t mask;

	MouseHandler(uint16_t);
	~MouseHandler(void);
	virtual void handle(int16_t, int16_t, int16_t, int16_t) = 0;
	void suspend(void);
	void resume(void);
	static MouseHandler *handlers[4];
};

#endif
