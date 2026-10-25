// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: SYSTEM\LSYSTEM.C

#include "u8port.h"

#include "ail.h"
#include "cdesc.h"
#include "cicrtc.h"
#include "ktimer.h"
#include "init.h"
#include "lsystem.h"

// Inline in the shared headers when this file was built.
U8_INLINE GraphicYtable::GraphicYtable(void) { f_04 = 2; index = 0; }
U8_INLINE Vport::Vport(void) { mem = 0; f_0f = 2; f_10 = 0; }
U8_INLINE Vport::~Vport(void) { free(); }
U8_INLINE Point::Point(void) {}
U8_INLINE Rect::Rect(void) {}
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }

// A blank message for the sound drivers to show on shutdown.
static char signoff[2] = "";
uint8_t RawSystem::active = 0;
Vport Vscreen;

void LargeGameSystem::init(void)
{
	if (!active)
	{
		initMemory();
		initRemember();
		initCrtc();
		initVideoMode();
		initMainScreen();
		initVirtualScreen();
		initMouse();
		initTimer();
		active = 1;
	}
}

void LargeGameSystem::initMouse(void)
{
}

void LargeGameSystem::remove(void)
{
	free();
}

void LargeGameSystem::initTimer(void)
{
	AIL_startup();
	ailTimer = new AilTimer;
}

void LargeGameSystem::initMemory(void)
{
}

void LargeGameSystem::initRemember(void)
{
}

// Saves the text mode for exit, then switches to 320x200x256.
void LargeGameSystem::initVideoMode(void)
{
	VideoMode mode;

	videoRestore.grab();
	mode.mode = 0x13;
	mode.activate();
}

void LargeGameSystem::initCrtc(void)
{
	GetBiosSegment();
	InitializeCrtc();
}

void LargeGameSystem::initMainScreen(void)
{
	SetVgaScreenDescriptor();
	mainScreen.create_main_screen();
}

// An off-screen buffer the size of the screen, made current.
void LargeGameSystem::initVirtualScreen(void)
{
	Vscreen.rect.set(0, 0, 319, 199);
	Vscreen.alloc(0);
	Vscreen.clear(0);
	GlobalVport::set(&Vscreen);
}

void LargeGameSystem::free(void)
{
	if (active)
	{
		active = 0;
		AIL_shutdown(signoff);
	}
}

void LargeGameSystem::exit_code(void)
{
	free();
}
