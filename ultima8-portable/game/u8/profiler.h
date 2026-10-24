#ifndef PROFILER_H
#define PROFILER_H

#include "u8port.h"

#include "process.h"

class Profiler : public ProcessRealTime
{
public:
	Profiler(void);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	void start(void);
	void stop(void);
};

#endif
