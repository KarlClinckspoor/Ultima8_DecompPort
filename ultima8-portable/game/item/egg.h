#ifndef EGG_H
#define EGG_H

#include "u8port.h"

#include "process.h"
#include "item.h"

// Keeps the avatar in stasis until it runs.
class SuspendAvatar : public Process
{
public:
	uint8_t wasInStasis;

	SuspendAvatar(void);
	virtual void process(void);
};

// Moves the avatar to a teleport egg on a map, fading the screen when it fires.
class TeleporterProcess : public Process
{
public:
	uint16_t destMap;
	uint16_t destEgg;

	TeleporterProcess(uint16_t);
	TeleporterProcess(int16_t, int16_t, uint8_t);
	virtual void process(void);
	void go(uint8_t);
	static int16_t dontFire;
};

class Egg : public Item
{
public:
	Egg(Referent r) { referent = r; }

	int16_t getEggXRange(void);
	int16_t getEggYRange(void);
	void setEggXRange(uint16_t);
	void setEggYRange(uint16_t);
	int16_t getEggId(void);
	void setEggId(uint16_t);
	int16_t hatch(void);
};

// Hatches an egg when the avatar walks into its range.
class EggHatcher : public Process
{
public:
	uint8_t hatched;

	EggHatcher(uint16_t);
	virtual void process(void);
	static uint8_t eggOutlineVisible;
};

int16_t teleportToEgg(int16_t, int16_t, uint8_t);

#endif
