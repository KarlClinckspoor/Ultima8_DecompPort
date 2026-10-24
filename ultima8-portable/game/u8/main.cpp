// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r- -y
// name: MAIN.C

#include "u8port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spanker.h"
#include "cexit.h"
#include "scratchm.h"
#include "filespec.h"
#include "hardware.h"
#include "init.h"
#include "anim.h"
#include "world.h"
#include "preamble.h"
#include "kernel.h"
#include "main.h"

uint16_t cheatcheck1 = 0;
uint32_t cheatcheck2 = 0;
uint32_t cheatcheck3 = 0;

char DateOfBuild[] = __DATE__;
char TimeOfBuild[] = __TIME__;
char Version[] = "2.12";
char UsecodeVersion[] = "    ";
char IntroVersion[] = "    ";
char EndgameVersion[] = "    ";
char SpeechVersion[] = "    ";
char Flavor[] = "Game  ";

uint16_t _stklen = 0x8000;

uint8_t gameLoopCounter[1] = {0};
uint8_t inGameMode = TRUE;

// Order matches the switch in parseCommandLine.
char *validArgs[] =
{
	"-help",
	"-record",
	"-playback",
	"-test",
	"-u",
	"-german",
	"-french",
	"-english",
	"-cheat",
	"-demo",
	"-cache"
};

void kernelDump(int16_t, int16_t)
{

}

void displayHelp(void)
{

	printf("No Help.\r\n\r\n");
	exit(0);
}

void performCheatCheck(char *s)
{
	uint32_t sum, product;
	uint16_t mixed, key3, key2, key1, i;

	if (cheatcheck1) return;

	sum = 0;
	product = 0;
	key1 = 0xFF;
	key2 = 0xE5;
	key3 = 0xAA;
	for (i = 0; i < strlen(s); i++)
	{
		mixed = i + s[i];
		mixed ^= key1;
		key1 += key2;
		key2 *= key3;
		key2 -= s[i];
		key3 ^= key1;
		key3++;
		sum += mixed;
		product = sum * key2;
		product <<= 1;
		sum ^= product;
	}
	cheatcheck1 = strlen(s);
	cheatcheck2 = sum;
	cheatcheck3 = product;
}

void parseCommandLine(int16_t argc, char **argv, CommandLineOptions &options)
{
	int16_t i;

	options.recordMode = 0;

	for (i = 1; i < argc; i++)
	{
		for (int16_t arg = 0; arg < 11; arg++)
		{
			if (stricmp(argv[i], validArgs[arg]) == 0)
			{
				switch (arg)
				{
					case 0:
						displayHelp();
						break;

					case 1:
					case 2:
						options.recordMode = arg;
						break;

					case 3:
						options.test = TRUE;
						break;

					case 4:
						strncpy(UsecodeDir, argv[i + 1], 30);
						i++;
						break;

					case 5:
						options.language = GERMAN;
						break;

					case 6:
						options.language = FRENCH;
						break;

					case 7:
						options.language = ENGLISH;
						break;

					case 8:
						performCheatCheck(argv[i + 1]);
						i++;
						break;

					case 10:
						options.cacheSize = atoi(argv[i + 1]);
						options.cacheSize *= 0x100000L;
						break;
				}
			}
		}
	}

}

void mainLoopFunction(void)
{

	Spanky::checkProfiler();

}

void PatchFlexFile::grabHeader(void)
{
	FlexHeader header;

	flexFile.getHeader(&header);
	setHeader(&header);
}

// Flex files that may have a pending patch.
char *patchPaths[] =
{
	"usecode\\*.flx",
	"static\\*.*",
	"sound\\*.*",
	NULL
};

void checkForPatchFiles(void)
{
	FileSpec files;
	int16_t i = 0;

	while (patchPaths[i])
	{
		files.become(patchPaths[i], 0);
		files.getFirst();

		while (files.exists())
		{
			FileSpec patchFile;
			patchFile.become(fileSpec(files.drive, files.dir, files.name, files.ext), 0);

			strcpy(patchFile.ext, "$$$");
			if (patchFile.exists())
			{
				trace(TRACE_ALWAYS, "Patching %s...\r\n", fileSpec(files.drive, files.dir, files.name, files.ext));
				PatchFlexFile flex(fileSpec(files.drive, files.dir, files.name, files.ext));
				flex.grabHeader();
				flex.commit();
			}

			files.getNext();
		}
		i++;
	}

}

void main(int16_t argc, char **argv)
{

	Spanky::init();

	trace(TRACE_ALWAYS, "\r\n\r\n");
	trace(TRACE_ALWAYS, copyrightStatement);
	trace(TRACE_ALWAYS, "\r\n\r\n");

	CommandLineOptions options;

	parseCommandLine(argc, argv, options);

	HardwareChecker hardware(3000000L, 2000000L, 0L, CPU_386);

	if (!hardware.verifyAll(TRUE))
		Exit(1);

	checkForPatchFiles();

	initSystem(options);

	charGen.smallFont = TRUE;

	if (options.test)
	{
		Dispatch(new TestScreen);
		options.test = FALSE;
	}

	preamble();

	Kernel::expandOnDemand = TRUE;

	Kernel::execute(1, mainLoopFunction);
}
