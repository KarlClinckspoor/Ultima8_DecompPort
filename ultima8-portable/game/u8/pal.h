#ifndef PAL_H
#define PAL_H

#include "u8port.h"

#include "u8process.h"
#include "flex.h"
#include "cpalette.h"
#include "mlist.h"

class BaseFile;

// Palette transforms; FadeProcess also takes PALETTE_CURRENT.
enum PaletteNum : int16_t
{
	PALETTE_NORMAL,
	PALETTE_GREYSCALE,
	PALETTE_NORED,
	PALETTE_RAINSTORM,
	PALETTE_FIRESTORM,
	PALETTE_SATURATE,
	PALETTE_GBR,
	PALETTE_BRG,
	PALETTE_CURRENT,	// whatever currentPalette is, followed as it changes
	PALETTE_CUSTOM		// a colour or palette given to the fade
};

#define FADE_PROCESS	((ProcessType)0x206)

class RGB
{
public:
	char f_00;	// red, 0-63
	char f_01;	// green
	char f_02;	// blue

	RGB(void);
	RGB(char r, char g, char b) { f_00 = r; f_01 = g; f_02 = b; }
};

// Fades U8GamePalette toward a target palette, `speed` steps per tick.
// A newer fade replaces a running one unless the running one has priority.
class FadeProcess : public ProcessInterrupt
{
public:
	PaletteData *palette;
	int16_t speed;
	int16_t priority;
	uint8_t done;
	uint8_t started;
	uint16_t paletteNum;
	uint8_t followCurrent;

	FadeProcess(RGB, int16_t, int16_t);
	FadeProcess(PaletteData *, int16_t, int16_t);
	FadeProcess(PaletteNum, int16_t, int16_t);
	virtual void process(void);
	virtual ~FadeProcess(void);
	virtual void load(BaseFile *);
	virtual void save(BaseFile *);
	void initPal(PaletteNum);
	uint8_t _init(void);
};

// A 256-colour palette read from a .pal file.
class FilePalette : public PaletteData
{
public:
	FilePalette(char *name) : PaletteData(0, 256) { read(name); }
	void read(char *);
};

class TransformPaletteFile : public FlexFile
{
public:
	TransformPaletteFile(void);
};

class TransformPalette
{
public:
	int16_t count;
	FlexFile *file;
	uint8_t *table;
	uint8_t *tables;	// table + 256

	TransformPalette(void);
	~TransformPalette(void);
	void init(void);
	void uninit(void);
	void load(void);
	void save(void);
	void addPal(void);
	void removePal(int16_t);
};

void initU8Palette(void);
void resetU8Palette(void);
void uninitU8Palette(void);
void activateCurrentPalette(void);
int16_t FadeToBlack(void);
int16_t FadeFromBlack(void);
int16_t FadeToPalette(int16_t, int16_t);
int16_t LightningBolt(void);
int16_t FadeToWhite(void);
int16_t FadeFromWhite(void);

extern uint16_t currentPalette;
extern FilePalette *U8GamePalette;
extern TransformPalette *theTransformPalette;
extern PaletteData *theDefaultPalette;
extern char palFileName[];

#endif
