// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: EVENT\CMOUSE.C

#include "u8port.h"
#include "plat.h"

// #include <dos.h>  (DOS only)
#include "cexit.h"
#include "cmouse.h"

uint16_t MouseDevice::original_handler_mask = 0;
uint16_t MouseDevice::original_handler[2] = {0, 0};
uint8_t MouseDevice::installed_flag;
uint16_t MouseDevice::number_of_buttons;
uint16_t MouseDevice::mouseLastX;
uint16_t MouseDevice::mouseLastY;
MouseHandler *MouseHandler::handlers[4];

MouseDevice::MouseDevice(void)
{
	init();
}

MouseDevice::~MouseDevice(void)
{
	remove_mouse_hook();
}

// The mouse driver's calls (int 33h) are the platform's now.
void MouseDevice::init(void)
{
	installed_flag = 1;
	number_of_buttons = 2;
	move_to(0, 0);
	mouseLastX = mouseLastY = 0;
	// The DOS build set 3 mickeys per 8 pixels across and 16 down; the host's pointer moves
	// with the host's.
	install_mouse_hook();
}

int16_t MouseDevice::reset(void)
{
	return -1;	// installed
}

// Swaps mouseHook in for every driver event, keeping the old handler.
void MouseDevice::install_mouse_hook(void)
{
	original_handler_mask = 0;
	original_handler[0] = original_handler[1] = 0;
	plat_mouse_set_handler(mouseHook);
}

void MouseDevice::remove_mouse_hook(void)
{
	plat_mouse_set_handler(0);
}

void MouseDevice::move_to(int16_t x, int16_t y)
{
	plat_mouse_move_to(x, y);
}

void MouseDevice::set_x_bounds(int16_t min, int16_t max)
{
	plat_mouse_set_bounds(min, -1, max, -1);
}

void MouseDevice::set_y_bounds(int16_t min, int16_t max)
{
	plat_mouse_set_bounds(-1, min, -1, max);
}

void MouseDevice::set_speed(int16_t, int16_t)
{
}

int16_t MouseDevice::get_last_x(void)
{
	if (fakeStackOverflowFlag)
		Fatal("mouse stack overflow");
	return mouseLastX;
}

int16_t MouseDevice::get_last_y(void)
{
	if (fakeStackOverflowFlag)
		Fatal("mouse stack overflow");
	return mouseLastY;
}

// Pins the pointer where it is by shrinking both ranges to its position.
void MouseDevice::freeze(void)
{
	int16_t x, y;

	plat_mouse_state(&x, &y, 0);
	plat_mouse_set_bounds(x, y, x, y);
}

void MouseDevice::release(void)
{
	plat_mouse_set_bounds(0, 0, 639, 199);
}

char MouseDevice::is_installed(void)
{
	return installed_flag;
}

int16_t MouseDevice::get_number_of_buttons(void)
{
	return number_of_buttons;
}

void MouseDevice::set_bounds(int16_t left, int16_t top, int16_t right, int16_t bottom)
{
	set_x_bounds(left, right);
	set_y_bounds(top, bottom);
}

MouseHandler::MouseHandler(uint16_t m)
{
	for (int16_t i = 0; i < 4; i++) {
		if (handlers[i] == 0) {
			mask = m;
			slot = i;
			handlers[i] = this;
			return;
		}
	}
	Fatal("Too many handlers");
}

MouseHandler::~MouseHandler(void)
{
	handlers[slot] = 0;
}

void MouseHandler::suspend(void)
{
	handlers[slot] = 0;
}

void MouseHandler::resume(void)
{
	handlers[slot] = this;
}
