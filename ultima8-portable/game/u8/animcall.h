#ifndef ANIMCALL_H
#define ANIMCALL_H

#include "u8port.h"

#include "process.h"
#include "mlist.h"

// Makes an item and pops it at a spot.
class ItemCreator : public ProcessInterrupt
{
public:
	uint16_t type;
	uint16_t frame;
	uint16_t x;
	uint16_t y;
	uint8_t z;

	ItemCreator(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint8_t);
	virtual void process(void);
};

// Destroys its item, or the item another process made.
class ItemDestroyer : public ProcessInterrupt
{
public:

	ItemDestroyer(uint16_t);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

// Makes an item at a spot and runs its frames to the last one.
class FutureAnim : public ProcessInterrupt
{
public:
	uint16_t firstFrame;
	uint16_t lastFrame;
	uint16_t type;
	uint16_t x;
	uint16_t y;
	uint8_t z;
	uint8_t started;

	FutureAnim(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint8_t);
	virtual void process(void);
};

int16_t createItem(uint16_t, uint16_t, uint16_t);
void generalAnimCall(uint16_t);
void deathAnimCall(uint16_t);
void hitAnimCall(uint16_t, uint16_t);

#endif
