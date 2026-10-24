#ifndef PREAMBLE_H
#define PREAMBLE_H

#include "u8port.h"

#include "process.h"
#include "itemfind.h"

// Holds the camera process while it waits, then restores the music.
class RestoreMusicProcess : public Process
{
public:
	RestoreMusicProcess(void);
	virtual void process(void);
	virtual ~RestoreMusicProcess(void);
	virtual void fail(int32_t);
};

// Starts a game: the intro for a new avatar, then the world.
class PreambleProcess : public Process
{
public:
	uint16_t f_2a;
	AreaItemFinder startEgg;

	PreambleProcess(void);
	virtual void process(void);
};

class RefreshWorld : public Process
{
public:
	uint16_t f_2a;

	RefreshWorld(void) { f_2a = 0; }
	virtual void process(void);
};

class MainMenuProcess : public Process
{
public:
	MainMenuProcess(void) {}
	virtual void process(void);
};

void preamble(void);

#endif
