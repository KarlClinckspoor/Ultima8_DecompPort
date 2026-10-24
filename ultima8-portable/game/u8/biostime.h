#ifndef BIOSTIME_H
#define BIOSTIME_H

#include "u8port.h"

// #include <phapi.h>  (DOS only)
#include "u8process.h"

class BiosTimer : public ProcessRealTime
{
public:
	uint32_t realVector;
	void *protVector;

	BiosTimer(void);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	void setTimerVector(uint32_t, void *);
};

#endif
