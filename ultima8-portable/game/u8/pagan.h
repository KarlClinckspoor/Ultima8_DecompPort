#ifndef PAGAN_H
#define PAGAN_H

#include "u8port.h"

#include "u8process.h"

class LinkedList;
class Msg;

// Writes the flag file that unlocks the quotes, once the credits end.
class MakeSemi : public Process
{
public:
	virtual void process(void);
};

// Escape stops the credits or quotes.
class CoolKeyboard : public Process
{
public:
	CoolKeyboard(void);
	virtual void process(void);
};

// Scrolls the credits or quotes text over a saved copy of the screen.
class PaganProcess : public ProcessRealTime
{
public:
	uint8_t pad_38[2];
	int16_t music;
	LinkedList *messages;
	int16_t state;		// -1 starting, 0 showing, 1 next message due, 2 ending
	uint16_t offset;	// into text
	char *text;
	char *line;		// the message being shown
	char *next;		// the message after it
	Msg *current;
	uint8_t f_54;
	char *background;

	PaganProcess(char *, uint8_t, int16_t, int16_t);
	virtual void process(void);
	virtual ~PaganProcess(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	uint8_t setupText(void);
	void kill(Msg *);
	static Msg *lnk;
};

void runCredits(uint8_t);
void runQuotes(void);

#endif
