// name: NEWHOOK
// Ported from NEWHOOK.ASM.
// Mouse driver event hook: passes each event to the registered mouse handlers.
//
// The driver called mouseHook with AX = events, BX = buttons, CX = x, DX = y; the platform
// calls it from plat_pump with the same values. The DOS hook ran the handlers on a private
// 1K stack with a guard word at each end and set fakeStackOverflowFlag when the bottom guard
// was overwritten; the host's stack needs no guard, so the flag stays 0.

#include "u8port.h"

#include "cmouse.h"

#define MAX_HANDLERS 4

// Set while an event is being handled, so a nested call returns at once.
uint8_t mouseActive = 0;
int16_t fakeStackOverflowFlag = 0;

// Calls handle(events, buttons, x, y) on every handler whose mask has one of the events,
// the last handler first.
void mouseHook(uint16_t events, uint16_t buttons, int16_t x, int16_t y)
{
	if (mouseActive)
		return;
	mouseActive = 1;
	MouseDevice::mouseLastX = (uint16_t)x;
	MouseDevice::mouseLastY = (uint16_t)y;
	for (int16_t i = MAX_HANDLERS - 1; i >= 0; i--)
	{
		MouseHandler *handler = MouseHandler::handlers[i];

		if (handler != 0 && (events & handler->mask))
			handler->handle((int16_t)events, (int16_t)buttons, x, y);
	}
	mouseActive = 0;
}
