#ifndef CEXIT_H
#define CEXIT_H

#include "u8port.h"

#include "error.h"
#include "ferror.h"

// Objects whose exit_code runs at exit, newest first.
class ExitSystem
{
public:
	ExitSystem *next;

	ExitSystem(void) { next = root; root = this; }
	virtual void exit_code(void);
	static void exit(void);
	static ExitSystem *root;
};

void Exit(int16_t);
// Exit(0) chose the int version on Borland; 0 now converts to either.
inline void Exit(int code) { Exit((int16_t)code); }
void Exit(const char *, ...);

extern uint8_t simpleExitSystem[4];

#endif
