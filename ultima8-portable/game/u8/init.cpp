// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r- -y
// name: INIT.C

#include "u8port.h"

#include <stdio.h>
#include <stdlib.h>
// #include <dos.h>  (DOS only)
// #include <dir.h>  (DOS only)
// #include <fcntl.h>  (DOS only)
// #include <phapi.h>  (DOS only)
#include "cexit.h"
#include "lsystem.h"
#include "scratchm.h"
#include "filespec.h"
#include "fexist.h"
#include "deskip.h"
#include "dsfxman.h"
#include "amusic.h"
#include "cache.h"
#include "handler.h"
#include "shaphand.h"
#include "savegame.h"
#include "pal.h"
#include "gametime.h"
#include "uprocess.h"
#include "dlist.h"
#include "itemcach.h"
#include "mapfile.h"
#include "item.h"
#include "type.h"
#include "glob.h"
#include "npc.h"
#include "debugump.h"
#include "appdebug.h"
#include "wpncache.h"
#include "chargen.h"
#include "kernel.h"
#include "irgump.h"
#include "u8point.h"
#include "u8dispat.h"
#include "world.h"
#include "camera.h"
#include "basecam.h"
#include "occlude.h"
#include "statrest.h"
#include "u8stdint.h"
#include "hardware.h"
#include "wait.h"
#include "spanker.h"
#include "init.h"

#define NUM_FILES	40

#define TRACE_STARTUP	((TraceLevel)50)

// Runs one startup step, then traces its line number.
#define INIT_STEP(step, line)	step; trace(TRACE_STARTUP, "%s %d\r\n", __FILE__, line)

#include "cfile.h"

int16_t sport = 0x220;
int16_t sirq = 7;
int16_t sdrq = 1;
int16_t sdma = 1;
int16_t mport = 0x220;
int16_t mirq = 7;
int16_t mdrq = 1;
int16_t mdma = 1;
static char ExeName_text[] = "u8.exe";
char *ExeName = ExeName_text;
static char lines_text[] = "---------------------------------------------------------------------";
char *lines = lines_text;
uint8_t doneIniting = FALSE;
uint8_t demoing = FALSE;
uint8_t musicOn;
uint8_t soundOn;
LargeGameSystem *theSystem;
uint16_t _openfd[NUM_FILES];
FileHandleExtender fileHandleExtender;
uint8_t old0[4];

U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }

// The DOS build raised DOS's limit of open files to NUM_FILES (int 21h function 67h) and
// set up Borland's table of them; the host has no such limit.
FileHandleExtender::FileHandleExtender(void)
{
	int16_t i;

	for (i = 0; i < NUM_FILES; i++)
		_openfd[i] = i < 5 ? 0 : 0xFFFF;
	closed = FALSE;
}

void FileHandleExtender::uninit(void)
{
	closed = TRUE;
}

FileHandleExtender::~FileHandleExtender(void)
{
	uninit();
}

// Opened the executable until DOS ran out of handles; the host never does.
uint8_t FileHandleExtender::check(void)
{
	return TRUE;
}

void UNINITFILEHANDLEEXTENDER(uint16_t)
{
	fileHandleExtender.uninit();
}

int16_t controlBreakHandler(void)
{
	exit(1);
	return 0;
}

void initDispatcher(int16_t mode)
{
	new U8Dispatcher(mode);
	if (!theU8Dispatcher)
		outOfMemory(__FILE__, 264);	// __LINE__
}

void uninitDispatcher(void)
{
	if (theU8Dispatcher)
	{
		theU8Dispatcher->pop(0);
		theU8Dispatcher = 0;
	}
}

void initWorldGump(void)
{
	theU8Dispatcher->pushBase(new World);
	ItemCache::load();
}

void initLanguage(Language language)
{
	char c;

	avatar.load();

	if (language != -1 && avatar.language != language)
		avatar.setLanguage(language);

	c = avatar.getLanguageChar();
	usecodeFileName[0] = c;
	introFileName[0] = c;
	creditDataFile[0] = c;
}

void initDeathLoader(void)
{
	if (!theDeathLoader)
		theDeathLoader = new DeathLoader;
}

void uninitDeathLoader(void)
{
	if (theDeathLoader)
		theDeathLoader = 0;
}

void createInitialProcesses(void)
{
	new StatRestorer;
	new HeadShaker;
	new ProcessMonitor;
}

void initSystem(CommandLineOptions options)
{
	if (!fileHandleExtender.check())
	{
		trace(TRACE_ALWAYS,
			"%s\r\n"
			"You must set FILES=%d in your CONFIG.SYS file.\r\n\r\n"
			"Vous devez configurer le param\x8Atre FILES=%d dans votre fichier CONFIG.SYS.\r\n\r\n"
			"Setzen Sie bitte in Ihrer CONFIG.SYS Datei FILES=%d.\r\n"
			"%s\r\n\r\n",
			lines, NUM_FILES, NUM_FILES, NUM_FILES, lines);
		Exit(1);
	}

	demoing = options.demo;
	traceLevel = 0xFE;
	// DOS: ctrlbrk(controlBreakHandler), Ctrl-Break ended the game. Closing the window does.

	// Step line numbers are the shipped __LINE__ values.
	INIT_STEP(initKernel(), 384);

	if (FileExists(fileSpec(0, StaticDir, "u8shapes.cmp", 0)))
	{
		trace(TRACE_ALWAYS,
			"%s\r\n"
			"Decompressing the shapes file.\r\n"
			"This only happens once and takes about 10 minutes.\r\n"
			"Do not interrupt this process or you will have to re-install.\r\n\r\n"
			"D\x82" "compression du fichier formes.\r\n"
			"Cela n'arrive qu'une seule fois et prend environ 10 minutes.\r\n"
			"Ne pas interrompre le processus ou il faudra r\x82installer.\r\n\r\n"
			"Die Formendatei wird gerade entpackt.\r\n"
			"Dieser Vorgang geschieht nur einmal und dauert ungef\x84hr zehn Minuten.\r\n"
			"Wenn Sie diesen Vorgang unterbrechen, m\x81ssen Sie erneut installieren.\r\n",
			lines);
		deskip(fileSpec(0, StaticDir, "u8shapes.cmp", 0),
			fileSpec(0, StaticDir, "u8shapes.flx", 0), 0);
	}

	getSoundStuff(sport, sirq, sdma, sdrq, mport, mirq, mdma, mdrq,
		musicOn, soundOn, 0, 0);

	if (!demoing)
	{
		INIT_STEP(initCache(options.cacheSize), 433);
	}

	if (!FileExists(GamedatDir))
		mkdir(GamedatDir);

	if (!FileExists(fileSpec(0, GamedatDir, nonfixedFileDesc, 0)))
	{
		SavedGames savedGames;
		savedGames.unpack(0);
	}

	INIT_STEP(theSystem = new LargeGameSystem, 483);
	INIT_STEP(theSystem->init(), 484);
	INIT_STEP(initU8Palette(), 485);
	INIT_STEP(initGameTime(), 486);

	if (!demoing)
	{
		INIT_STEP(initScratchBuffer(), 490);
		INIT_STEP(initLanguage(options.language), 491);
		INIT_STEP(initUnk(), 492);
		INIT_STEP(initDisplayList(), 493);
		INIT_STEP(initItemCache(), 494);
		INIT_STEP(GlobalTypes.loadTypes(), 495);
		INIT_STEP(initCacheHandler(), 496);
		INIT_STEP(initShapeHandler(), 497);
		INIT_STEP(initGlobExpander(), 498);
		INIT_STEP(initDeathLoader(), 499);
		INIT_STEP(initDebugger(), 500);
		INIT_STEP(initAnimCache(), 501);
		INIT_STEP(GlobalFlag::load(GamedatDir), 502);
	}

	INIT_STEP(initDispatcher(options.recordMode), 515);

	if (!demoing)
	{
		INIT_STEP(initNpcs(), 519);
		INIT_STEP(initWorldGump(), 524);
		INIT_STEP(initU8Camera(), 525);
		INIT_STEP(initDebug(), 526);
		createInitialProcesses();

		Kernel::load(GamedatDir);
		loadGumps();

		TMMousePointer::hide();
		if (U8MousePointer::modes[U8MousePointer::sp] == 35)
		{
			U8MousePointer::popMode();
			U8MousePointer::popMode();
			U8MousePointer::popMode();
		}
	}

	if (!TheMusicProcess && musicOn)
		musicInit("music.flx", mport, mirq, mdma, mdrq);
	if (soundOn)
		soundInit(sirq, sport, sdma);

	doneIniting = TRUE;
}

void SimpleExitSystem::exit(void)
{
	if (initialized)
	{
		initialized = FALSE;

		if (theSystem)
		{
			delete theSystem;
			theSystem = 0;
		}

		uninitAnimCache();

		if (!demoing)
		{
			uninitDeathLoader();
			uninitGlobExpander();
			uninitDisplayList();
			uninitShapeHandler();
		}

		uninitU8Palette();
		if (musicOn) musicDeinit();
		if (soundOn) soundDeInit();

		if (!demoing)
		{
			uninitCache();
			uninitScratchBuffer();
		}

		uninitDispatcher();
		uninitKernel();
		Spanky::uninit();

		printf(thanks[avatar.language]);
	}
}

void load(int16_t slot)
{
	if (soundOn)
		soundDeInit();
	if (musicOn)
		musicDeinit();

	Kernel::restart();
	Kernel::freePhantoms();
	ItemCache::itemFile->close();

	if (slot != -1)
	{
		SavedGames savedGames;
		savedGames.unpack(slot);
	}

	Kernel::load(GamedatDir);
	Npc::load();
	avatar.load();
	soundOn = avatar.soundOn;
	musicOn = avatar.musicOn;
	ItemCache::load();
	GlobalFlag::load(GamedatDir);
	ItemCache::itemFile->restart();
	loadGumps();

	if (musicOn)
		musicInit("music.flx", mport, mirq, mdma, mdrq);
	if (soundOn)
	{
		soundInit(sirq, sport, sdma);
		uint16_t pid = guardianBark(-4);
		if (pid)
		{
			Wait *wait = new Wait(30, 0);
			wait->then(Kernel::getProcess(pid));
			wait->start();
		}
	}

	Camera::init();
	theZbuffer.make();
	Camera::setCenterOn(1);
	theBaseCamera->show();
}

void save(int16_t slot)
{
	if (((Npc *)&avatar)->isDead()) return;

	if (soundOn)
		soundDeInit();
	if (musicOn)
		musicDeinit();

	saveGumps();
	Kernel::save(GamedatDir);
	ItemCache::itemFile->commit();
	GlobalFlag::save(GamedatDir);
	ItemCache::save(GamedatDir);
	Npc::save();
	avatar.soundOn = soundOn;
	avatar.musicOn = musicOn;
	avatar.save(0);
	ItemCache::itemFile->resetChecksum();

	if (slot != -1)
	{
		SavedGames savedGames;
		savedGames.pack(slot, GamedatDir);
	}

	ItemCache::itemFile->initTemp();

	if (musicOn)
		musicInit("music.flx", mport, mirq, mdma, mdrq);
	if (soundOn)
	{
		soundInit(sirq, sport, sdma);
		uint16_t pid = guardianBark(-4);
		if (pid)
		{
			Wait *wait = new Wait(120, 0);
			wait->then(Kernel::getProcess(pid));
			wait->start();
		}
	}
}
