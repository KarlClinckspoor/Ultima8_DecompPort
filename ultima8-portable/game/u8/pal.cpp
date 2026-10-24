// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: PAL.C

#include "u8port.h"

#include "process.h"
#include "cfile.h"
#include "cdesc.h"
#include "cpalette.h"
#include "error.h"
#include "filespec.h"
#include "pal.h"
#include "scratchm.h"

// Inline in the shared headers when this file was built.
inline RGB::RGB(void) {}
inline Index::Index(void) { size = 0; offset = 0; }
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline void ProcessInterrupt::interruptHandler(uint32_t, uint32_t) {}
inline TransformPaletteFile::TransformPaletteFile(void) :
	FlexFile(fileSpec(0, StaticDir, transformPaletteFileName, 0), (OpenMode)2, 1)
{
}

// Drops a palette merge left half done.
#define STOP_MERGE() \
	if (!init && iterations) \
	{ \
		iterations = 0; \
		delete error; \
		delete delta; \
		delete vector; \
		init = 1; \
	}

uint16_t currentPalette = 0;
FilePalette *U8GamePalette = 0;
TransformPalette *theTransformPalette = 0;
PaletteData *theDefaultPalette = 0;
char palFileName[] = "u8pal.pal";

FadeProcess::FadeProcess(RGB color, int16_t pri, int16_t spd)
{
	priority = pri;
	speed = spd;
	palette = new PaletteData(0, 256);
	RGB *colors = palette->colors;
	for (int16_t i = 0; i < 256; i++)
		colors[i] = color;
	started = 0;
	followCurrent = 0;
	paletteNum = PALETTE_CUSTOM;
}

FadeProcess::FadeProcess(PaletteData *target, int16_t pri, int16_t spd)
{
	priority = pri;
	speed = spd;
	palette = new PaletteData(0, 256);
	palette->copy_from(*target);
	started = 0;
	followCurrent = 0;
	paletteNum = PALETTE_CUSTOM;
}

FadeProcess::FadeProcess(PaletteNum num, int16_t pri, int16_t spd)
{
	paletteNum = num;
	priority = pri;
	speed = spd;
	palette = new PaletteData(0, 256);
	if (paletteNum == PALETTE_CURRENT)
	{
		paletteNum = currentPalette;
		followCurrent = 1;
	}
	else
		followCurrent = 0;
	initPal((PaletteNum)paletteNum);
	started = 0;
}

void FadeProcess::initPal(PaletteNum num)
{
	int16_t i, c;
	palette->copy_from(*theDefaultPalette);
	RGB *colors = palette->colors;
	switch (num)
	{
	case PALETTE_GREYSCALE:
		for (i = 0; i < 256; i++)
		{
			c = (colors[i].f_00 * 3 + colors[i].f_01 * 4 + colors[i].f_02 * 2 + 4) >> 3;
			if (c > 63)
				c = 63;
			colors[i].f_00 = colors[i].f_01 = colors[i].f_02 = c;
		}
		break;
	case PALETTE_NORED:
		for (i = 0; i < 256; i++)
			colors[i].f_00 = 0;
		break;
	case PALETTE_RAINSTORM:
		for (i = 0; i < 256; i++)
		{
			c = (colors[i].f_00 * 3 + colors[i].f_01 * 4 + colors[i].f_02 * 2 + 4) >> 3;
			if (c > 63)
				c = 63;
			colors[i].f_00 = ((colors[i].f_00 + c) >> 2) + 12;
			colors[i].f_01 = ((colors[i].f_01 + c) >> 2) + 12;
			colors[i].f_02 = ((colors[i].f_02 + c) >> 2) + 12;
		}
		break;
	case PALETTE_FIRESTORM:
		for (i = 0; i < 256; i++)
		{
			c = (colors[i].f_00 * 3 + colors[i].f_01 * 4 + colors[i].f_02 * 2 + 4) >> 3;
			if (c > 63)
				c = 63;
			colors[i].f_00 = ((colors[i].f_00 + c) >> 1) + 12;
			if (colors[i].f_00 > 63)
				colors[i].f_00 = 63;
			colors[i].f_01 = (colors[i].f_01 + (c >> 1)) >> 1;
			colors[i].f_02 = colors[i].f_02 >> 1;
		}
		break;
	case PALETTE_SATURATE:
		for (i = 0; i < 256; i++)
		{
			int16_t v;
			c = (colors[i].f_00 * 3 + colors[i].f_01 * 4 + colors[i].f_02 * 2 + 4) >> 3;
			v = (colors[i].f_00 - c) * 2 + c;
			if (v < 0)
				v = 0;
			if (v > 63)
				v = 63;
			colors[i].f_00 = v;
			v = (colors[i].f_01 - c) * 2 + c;
			if (v < 0)
				v = 0;
			if (v > 63)
				v = 63;
			colors[i].f_01 = v;
			v = (colors[i].f_02 - c) * 2 + c;
			if (v < 0)
				v = 0;
			if (v > 63)
				v = 63;
			colors[i].f_02 = v;
		}
		break;
	case PALETTE_GBR:
		for (i = 0; i < 256; i++)
		{
			c = colors[i].f_00;
			colors[i].f_00 = colors[i].f_01;
			colors[i].f_01 = colors[i].f_02;
			colors[i].f_02 = c;
		}
		break;
	case PALETTE_BRG:
		for (i = 0; i < 256; i++)
		{
			c = colors[i].f_00;
			colors[i].f_00 = colors[i].f_02;
			colors[i].f_02 = colors[i].f_01;
			colors[i].f_01 = c;
		}
		break;
	}
}

uint8_t FadeProcess::_init(void)
{
	FadeProcess *other = (FadeProcess *)Kernel::findValidProcess(0, FADE_PROCESS);
	if (other)
	{
		int16_t otherPriority = other->priority;
		if (priority <= otherPriority)
		{
			done = 1;
			delete palette;
			palette = 0;
			return 0;
		}
		other->fail(0);
	}
	setProcessType(FADE_PROCESS);
	done = 0;
	started = 1;
	return 1;
}

FadeProcess::~FadeProcess(void)
{
	if (!done)
		STOP_MERGE();
	if (palette)
	{
		delete palette;
		palette = 0;
	}
}

void FadeProcess::process(void)
{
	if (paletteNum != currentPalette)
	{
		if (followCurrent && paletteNum != currentPalette)
		{
			STOP_MERGE();
			paletteNum = currentPalette;
			initPal((PaletteNum)paletteNum);
		}
		else if (paletteNum != PALETTE_CUSTOM)
		{
			fail(0);
			return;
		}
	}
	if (!started)
		_init();
	if (!palette)
	{
		fail(0);
		return;
	}
	if (speed == 0)
	{
		U8GamePalette->copy_from(*palette);
		U8GamePalette->activate(1);
		done = 1;
		pop(0);
	}
	else
	{
		for (int16_t i = 0; i < speed; i++)
		{
			if (!U8GamePalette->fade_step(*palette))
			{
				done = 1;
				break;
			}
		}
		U8GamePalette->activate(1);
		if (done)
			pop(0);
	}
}

void FadeProcess::load(BaseFile *file)
{
	Process::load(file);
	palette = new PaletteData(0, 256);
	file->read((char *)palette->colors, 768);
}

void FadeProcess::save(BaseFile *file)
{
	Process::save(file);
	file->write((char *)palette->colors, 768);
}

// A palette file holds 256 colours either as words (0x600 bytes) or as
// bytes after a 4-byte header (0x304 bytes).
void FilePalette::read(char *name)
{
	int16_t wide[256][3];
	char raw[768];
	BaseFile file(name, (OpenMode)2);
	int32_t size = file.size();
	if (size == 0x600)
	{
		file.read((char *)wide, size, 0);
		for (int16_t i = 0; i < 256; i++)
		{
			colors[i].f_00 = wide[i][0];
			colors[i].f_01 = wide[i][1];
			colors[i].f_02 = wide[i][2];
		}
	}
	else if (size == 0x304)
	{
		file.read(raw, size - 4, 4);
		for (int16_t i = 0; i < 256; i++)
		{
			colors[i].f_00 = raw[i * 3];
			colors[i].f_01 = raw[i * 3 + 1];
			colors[i].f_02 = raw[i * 3 + 2];
		}
	}
	else
		halt(__FILE__, 377);	// __LINE__
}

TransformPalette::TransformPalette(void)
{
	table = tables = 0;
	count = 0;
}

TransformPalette::~TransformPalette(void)
{
	if (table)
		uninit();
}

void TransformPalette::init(void)
{
	file = new TransformPaletteFile;
	if (!file)
		halt(__FILE__, 401);	// __LINE__
	load();
}

void TransformPalette::uninit(void)
{
	delete table;
	table = tables = 0;
	if (file)
		delete file;
	file = 0;
}

void TransformPalette::load(void)
{
	Index index;
	file->getIndex(0, index);
	if (table)
		delete table;
	count = (index.size >> 8) - 2;
	if (count < 7)
		count = 7;
	table = new uint8_t[0x900];
	file->readRecord(0, table);
	tables = table + 256;
}

void TransformPalette::save(void)
{
	file->writeRecord(0, table, (count + 2) << 8);
}

void TransformPalette::addPal(void)
{
}

void TransformPalette::removePal(int16_t)
{
}

void initU8Palette(void)
{
	U8GamePalette = new FilePalette(fileSpec(0, StaticDir, palFileName, 0));
	theDefaultPalette = new PaletteData(0, 256);
	theDefaultPalette->copy_from(*U8GamePalette);
	theTransformPalette = new TransformPalette;
	theTransformPalette->init();
	RGB black(0, 0, 0);
	U8GamePalette->clear(black, U8GamePalette->start, U8GamePalette->count);
	RGB *colors = U8GamePalette->colors;
	colors[7].f_00 = 58;
	colors[7].f_01 = 58;
	colors[7].f_02 = 58;
	U8GamePalette->activate(0, 256, 0);
}

void resetU8Palette(void)
{
	U8GamePalette->read(fileSpec(0, StaticDir, palFileName, 0));
	U8GamePalette->activate(0, 256, 0);
	theDefaultPalette->copy_from(*U8GamePalette);
}

void uninitU8Palette(void)
{
	if (U8GamePalette)
	{
		delete U8GamePalette;
		U8GamePalette = 0;
	}
	if (theDefaultPalette)
	{
		delete theDefaultPalette;
		theDefaultPalette = 0;
	}
	if (theTransformPalette)
	{
		theTransformPalette->uninit();
		delete theTransformPalette;
		theTransformPalette = 0;
	}
}

void activateCurrentPalette(void)
{
	U8GamePalette->activate(0);
}

int16_t FadeToBlack(void)
{
	FadeProcess *fade = new FadeProcess(RGB(0, 0, 0), 0x7fff, 1);
	return fade->pid;
}

int16_t FadeFromBlack(void)
{
	FadeProcess *fade = new FadeProcess(PALETTE_CURRENT, 0x7fff, 1);
	return fade->pid;
}

int16_t FadeToPalette(int16_t num, int16_t pri)
{
	currentPalette = num;
	FadeProcess *fade = new FadeProcess((PaletteNum)num, pri, 1);
	return fade->pid;
}

// Flashes the screen white and fades back, restarting a running lightning flash.
int16_t LightningBolt(void)
{
	RGB colors[256];
	FadeProcess *fade;
	FadeProcess *old = (FadeProcess *)Kernel::findValidProcess(0, FADE_PROCESS);
	if (old)
	{
		int16_t pri = old->priority;
		int16_t spd = old->speed;
		if (pri != -1 || spd != 4)
			return 0;
		fade = new FadeProcess(old->palette, -1, 4);
		old->fail(0);
	}
	else
		fade = new FadeProcess(U8GamePalette, -1, 4);
	for (int16_t i = 0; i < 256; i++)
		U8GamePalette->colors[i] = RGB(63, 63, 63);
	return fade->pid;
}

int16_t FadeToWhite(void)
{
	FadeProcess *fade = new FadeProcess(RGB(63, 63, 63), 0x7fff, 1);
	return fade->pid;
}

int16_t FadeFromWhite(void)
{
	FadeProcess *fade = new FadeProcess(PALETTE_CURRENT, 0x7fff, 1);
	return fade->pid;
}
