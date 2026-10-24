#ifndef MAIN_H
#define MAIN_H

#include "u8port.h"

#include "stepflex.h"

class CommandLineOptions;

class PatchFlexFile : public StepFlexFile
{
public:
	PatchFlexFile(char *name) : StepFlexFile(name, -1, 0) {}
	void grabHeader(void);
};

void kernelDump(int16_t, int16_t);
void displayHelp(void);
void performCheatCheck(char *);
void parseCommandLine(int16_t, char **, CommandLineOptions &);
void mainLoopFunction(void);
void checkForPatchFiles(void);

extern uint16_t cheatcheck1;
extern uint32_t cheatcheck2;
extern uint32_t cheatcheck3;
extern char DateOfBuild[12];
extern char TimeOfBuild[9];
extern char Version[5];
extern char UsecodeVersion[5];
extern char IntroVersion[5];
extern char EndgameVersion[5];
extern char SpeechVersion[5];
extern char Flavor[7];
extern uint16_t _stklen;
extern uint8_t gameLoopCounter[1];
extern uint8_t inGameMode;
extern char *validArgs[11];
extern char *patchPaths[4];

#endif
