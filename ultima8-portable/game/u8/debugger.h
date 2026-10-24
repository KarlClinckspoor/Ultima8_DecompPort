#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "u8port.h"

class BreakPoint
{
public:
	char file[9];
	int16_t line;
};

// One entry of the usecode call stack the debugger follows.
class Info
{
public:
	char file[9];
	char *symbols;
	char *data;
	char *process;
};

class Debugger
{
public:
	int16_t numBreaks;
	BreakPoint breaks[10];
	uint16_t currentLine;
	uint8_t tracing;
	uint8_t steppingOver;
	int32_t stepDepth;	// calls entered since the step began
	int16_t currentInfo;
	Info infos[30];

	Debugger(void);
	virtual void atBreakPoint(void);
	virtual void graphicVideoMode(void);
	Info *getCurrentInfo(void) { return currentInfo ? &infos[currentInfo - 1] : 0; }
	void lineChange(uint16_t);
	void addBreakPoint(char *, int16_t);
	void removeBreakPoint(char *, int16_t);
	int16_t getNextBreak(char *, int16_t);
	uint8_t isBreakPointAt(char *, int16_t);
	void pushFile(char *, char *, char *, char *);
	void pushFile(Info *);
	void popFile(void);
	void stepOver(void);
	void reset(void);
	char *currentFile(void);
};

#endif
