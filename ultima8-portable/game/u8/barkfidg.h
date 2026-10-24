#ifndef BARKFIDG_H
#define BARKFIDG_H

#include "u8port.h"

#include "u8process.h"
#include "item.h"

// Keeps a barking NPC playing its talk animation while its bark is shown.
class BarkFidget : public ProcessInterrupt
{
public:
	uint8_t f_2e;

	BarkFidget(uint16_t, uint8_t);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	Boolean hasBark(void);
};

#endif
