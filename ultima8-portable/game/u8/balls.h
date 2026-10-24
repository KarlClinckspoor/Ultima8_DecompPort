#ifndef BALLS_H
#define BALLS_H

#include "u8port.h"

#include "u8process.h"
#include "mlist.h"

// Ball frame; frames 1 and 4 explode on contact.
enum BallType : int16_t
{
	BallType_0
};

// Bit 1: the ball moves in z; bit 2: it trails child balls.
enum BallVariety : int16_t
{
	BallVariety_0
};

class Explode : public Process
{
public:
	char count;

	Explode(uint16_t);
	virtual void process(void);
};

class Ball : public ProcessInterrupt
{
public:
	char dx;
	char dy;
	char dz;
	uint16_t target;
	BallVariety variety;
	uint16_t *children;
	char maxSpeed;
	BallType type;
	int16_t numChildren;

	Ball(BallVariety, uint16_t, char);
	virtual void process(void);
	uint8_t init(BallType, uint16_t, uint16_t, uint16_t);
	int16_t cantMove(void);
	void generateChild(void);
	void reflect(void);
	void addSpeed(void);
	void explode(void);
};

void TonysBalls(int16_t, int16_t, uint16_t, uint16_t, uint16_t);

#endif
