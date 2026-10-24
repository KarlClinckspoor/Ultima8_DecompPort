#ifndef LSYSTEM_H
#define LSYSTEM_H

#include "u8port.h"

#include "cexit.h"
#include "cglobvp.h"
#include "cvport.h"

class RawSystem;

class RawSystem : public ExitSystem
{
public:
	virtual void exit_code(void);
	virtual void init(void) = 0;
	virtual void free(void) = 0;
	virtual void initMouse(void) = 0;
	virtual void initTimer(void) = 0;
	virtual void initMemory(void) = 0;
	virtual void initRemember(void) = 0;
	virtual void initVideoMode(void) = 0;
	virtual void initCrtc(void) = 0;
	virtual void initMainScreen(void) = 0;
	virtual void initVirtualScreen(void) = 0;
	static uint8_t active;
};

class LargeGameSystem : public RawSystem
{
public:
	GlobalVport mainScreen;
	VideoRestore videoRestore;

	~LargeGameSystem(void) { free(); }
	virtual void exit_code(void);
	virtual void init(void);
	virtual void free(void);
	virtual void initMouse(void);
	virtual void initTimer(void);
	virtual void initMemory(void);
	virtual void initRemember(void);
	virtual void initVideoMode(void);
	virtual void initCrtc(void);
	virtual void initMainScreen(void);
	virtual void initVirtualScreen(void);
	virtual void remove(void);
};

extern Vport Vscreen;

#endif
