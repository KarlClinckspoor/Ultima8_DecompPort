#ifndef CMPOINT_H
#define CMPOINT_H

#include "u8port.h"

#include "cmouse.h"

// Whether the mouse interrupt may draw the pointer.
enum CatState
{
	CAT_BUSY,
	CAT_FREE
};

enum MouseButton
{
	NO_BUTTON,
	LEFT_BUTTON,
	RIGHT_BUTTON,
	BOTH_BUTTONS
};

enum MouseSelectCommand
{
	NO_SELECT,
	SELECT_CLICK,
	SELECT_DOUBLE_CLICK,
	SELECT_RELEASE,
	SELECT_MOVE
};

class MousePointer : public MouseHandler
{
public:
	MousePointer(void);
	static void pushCat(CatState);
	static void popCat(void);
	static uint16_t old_x;
	static uint16_t old_y;
	static uint8_t visible;
	static uint16_t catSP;
	static CatState cat[5];
};

#endif
