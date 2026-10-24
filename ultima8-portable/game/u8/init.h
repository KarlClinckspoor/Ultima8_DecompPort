#ifndef INIT_H
#define INIT_H

#include "u8port.h"

class CommandLineOptions;
class LargeGameSystem;

enum Language
{
	Language_0,
	ENGLISH = 0,
	FRENCH = 1,
	GERMAN = 2
};

class CommandLineOptions
{
public:
	int16_t recordMode;		// 1 record, 2 playback
	int16_t test;
	Language language;	// -1 keeps the saved one
	int16_t demo;
	int16_t f_08;
	int32_t cacheSize;

	CommandLineOptions(void)
	{
		recordMode = 0;
		test = 0;
		language = (Language)-1;
		demo = 0;
		f_08 = 0;
		cacheSize = 0x800000L;
	}
};

// Raises the DOS file handle limit and widens the runtime's handle table to match.
class FileHandleExtender
{
public:
	uint8_t closed;

	FileHandleExtender(void);
	~FileHandleExtender(void);
	void uninit(void);
	uint8_t check(void);
};

class SimpleExitSystem
{
public:
	static void exit(void);
	static uint8_t initialized;
};

void UNINITFILEHANDLEEXTENDER(uint16_t);
int16_t controlBreakHandler(void);
void initDispatcher(int16_t);
void uninitDispatcher(void);
void initWorldGump(void);
void initLanguage(Language);
void initDeathLoader(void);
void uninitDeathLoader(void);
void createInitialProcesses(void);
void initSystem(CommandLineOptions);
void load(int16_t);
void save(int16_t);

extern int16_t sport;
extern int16_t sirq;
extern int16_t sdrq;
extern int16_t sdma;
extern int16_t mport;
extern int16_t mirq;
extern int16_t mdrq;
extern int16_t mdma;
extern char *ExeName;
extern char *lines;
extern uint8_t doneIniting;
extern uint8_t demoing;
extern uint8_t musicOn;
extern uint8_t soundOn;
extern LargeGameSystem *theSystem;
extern uint16_t _openfd[40];
extern FileHandleExtender fileHandleExtender;
extern uint8_t old0[4];

#endif
