// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: EVENT\CMSELECT.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
#include "systimer.h"
#include "cmselect.h"
#include "dispatch.h"

// Inline in the shared headers when this file was built.
inline MouseButtonStatus::MouseButtonStatus(void) {}
inline MouseButtonStatus::MouseButtonStatus(int16_t s) { status = s; }
inline MouseEvent::MouseEvent(void) { _action = MOUSE_MOVED; }
inline MouseSelectEvent::MouseSelectEvent(void) { _type = NO_SELECT; }

uint8_t MouseSelectChronicler::handedness = 0;
int16_t MouseSelectInspector::threshold;
MouseSelectEvent MouseSelectInspector::last_event;

MouseSelectEvent::MouseSelectEvent(MouseSelectCommand command, MouseButton button, int16_t x, int16_t y)
{
	_type = command;
	_button = button;
	_x = x;
	_y = y;
	_time = systemTimer->ticks;
}

MouseSelectChronicler::MouseSelectChronicler(MouseSelectEvent *buffer, int16_t count) :
	MouseEventChronicler(buffer, sizeof(MouseSelectEvent), count)
{
}

MouseSelectChronicler::MouseSelectChronicler(void *buffer, int16_t size, int16_t count) :
	MouseEventChronicler(buffer, size, count)
{
}

// Mouse driver callback: mask holds the int 33h event bits.
void MouseSelectChronicler::handle(int16_t mask, int16_t buttons, int16_t x, int16_t y)
{
	MouseSelectEvent event;

	event._x = x;
	event._y = y;
	event._time = systemTimer->ticks;
	event.mouseButtonStatus = MouseButtonStatus(buttons);
	event._button = NO_BUTTON;
	if (mask & 0x06)
		event._button = LEFT_BUTTON;
	if (mask & 0x18) {
		if (event._button)
			event._button = BOTH_BUTTONS;
		else
			event._button = RIGHT_BUTTON;
	}
	event._type = NO_SELECT;
	if (mask & 0x54)
		event._action = MOUSE_UP;
	else if (mask & 0x2A)
		event._action = MOUSE_DOWN;
	else
		event._action = MOUSE_MOVED;
	_AH = 2;
	geninterrupt(0x16);
	event.extraStatus = _AL;
	write_event(&event);
}

void MouseSelectChronicler::check_button_swap(void *event)
{
	int16_t button = ((MouseSelectEvent *)event)->_button;
	int16_t status = ((MouseSelectEvent *)event)->mouseButtonStatus.status;

	if (handedness) {
		if (button == LEFT_BUTTON)
			button = RIGHT_BUTTON;
		else if (button == RIGHT_BUTTON)
			button = LEFT_BUTTON;
		if (!(status & 3))
			status ^= 3;
	}
	((MouseSelectEvent *)event)->_button = button;
	((MouseSelectEvent *)event)->mouseButtonStatus.status = status;
}

// Turns the raw button events at the read position into one select event,
// waiting up to threshold ticks to tell a click from a double click.
uint8_t MouseSelectChronicler::read_event(void *p, int16_t threshold)
{
	uint32_t now = systemTimer->ticks;
	MouseSelectEvent *queued[4];

	queued[0] = (MouseSelectEvent *)read;
	queued[1] = (MouseSelectEvent *)get_next_read(queued[0]);
	queued[2] = (MouseSelectEvent *)get_next_read(queued[1]);

	if (!is_valid(queued[0]))
		return 0;
	if (queued[0]->_action == MOUSE_UP) {
		MouseEventChronicler::read_event(p);
		check_button_swap(p);
		((MouseSelectEvent *)p)->_type = SELECT_RELEASE;
		return 1;
	}
	if (queued[0]->_button == BOTH_BUTTONS) {
		MouseEventChronicler::read_event(p);
		check_button_swap(p);
		((MouseSelectEvent *)p)->_type = SELECT_CLICK;
		((MouseSelectEvent *)p)->_button = BOTH_BUTTONS;
		return 1;
	}
	if (queued[0]->_action != MOUSE_DOWN)
		return 0;
	if (!is_valid(queued[1])) {
		if (threshold >= now - queued[0]->_time)
			return 0;
	} else if (threshold >= queued[1]->_time - queued[0]->_time) {
		if (queued[1]->_button != queued[0]->_button) {
			if (queued[1]->_action != MOUSE_UP) {
				MouseEventChronicler::read_event(p);
				MouseEventChronicler::read_event(p);
				check_button_swap(p);
				((MouseSelectEvent *)p)->_type = SELECT_CLICK;
				((MouseSelectEvent *)p)->_button = BOTH_BUTTONS;
				return 1;
			}
		} else if (!is_valid(queued[2])) {
			if (threshold >= now - queued[1]->_time)
				return 0;
		} else if (threshold >= queued[2]->_time - queued[1]->_time && queued[2]->_button == queued[1]->_button) {
			MouseEventChronicler::read_event(p);
			MouseEventChronicler::read_event(p);
			MouseEventChronicler::read_event(p);
			check_button_swap(p);
			((MouseSelectEvent *)p)->_type = SELECT_DOUBLE_CLICK;
			return 1;
		}
	}
	MouseEventChronicler::read_event(p);
	check_button_swap(p);
	((MouseSelectEvent *)p)->_type = SELECT_CLICK;
	return 1;
}

MouseSelectEvent *MouseSelectInspector::poll(void)
{
	last_event._type = NO_SELECT;
	if (!MouseSelectChronicler::read_event(&last_event, threshold))
		check_was_moved(&last_event);
	return &last_event;
}

MouseSelectEvent &MouseSelectInspector::poll(MouseSelectEvent &event)
{
	poll();
	event = last_event;
	return event;
}

uint8_t MouseSelectInspector::check_was_moved(MouseSelectEvent *event)
{
	if (get_last_x() != event->_x || get_last_y() != event->_y) {
		event->_type = SELECT_MOVE;
		event->_x = get_last_x();
		event->_y = get_last_y();
		return 1;
	}
	event->_type = NO_SELECT;
	return 0;
}
