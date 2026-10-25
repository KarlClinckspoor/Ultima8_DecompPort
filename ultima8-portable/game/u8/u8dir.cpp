// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r- -y
// name: U8DIR.C

#include "u8port.h"

#include <string.h>
#include "npc.h"
#include "fexist.h"
#include "filespec.h"
#include "u8dir.h"
#include "scratchm.h"

char StaticDir[9] = "static";
char GamedatDir[9] = "gamedat";
char SavedgamesDir[9] = "savegame";
char SoundDir[9] = "sound";
char EditorDir[9] = "editor";
char UsecodeDir[80] = "usecode";
static char shapeFileName_text[] = "u8shapes.flx";
char *shapeFileName = shapeFileName_text;
static char typeNamesFileName_text[] = "typename.dat";
char *typeNamesFileName = typeNamesFileName_text;
static char fixedFileDesc_text[] = "fixed.dat";
char *fixedFileDesc = fixedFileDesc_text;
static char nonfixedFileDesc_text[] = "nonfixed.dat";
char *nonfixedFileDesc = nonfixedFileDesc_text;
static char globFileName_text[] = "glob.flx";
char *globFileName = globFileName_text;
static char endgameFileName_text[] = "endgame.skf";
char *endgameFileName = endgameFileName_text;
static char transformPaletteFileName_text[] = "xformpal.dat";
char *transformPaletteFileName = transformPaletteFileName_text;
static char musicFileName_text[] = "music.flx";
char *musicFileName = musicFileName_text;
static char gumpsFileName_text[] = "u8gumps.flx";
char *gumpsFileName = gumpsFileName_text;
static char fontsFileName_text[] = "u8fonts.flx";
char *fontsFileName = fontsFileName_text;
static char quoteDataFile_text[] = "quotes.dat";
char *quoteDataFile = quoteDataFile_text;
static char usecodeFileName_text[] = "eusecode.flx";
char *usecodeFileName = usecodeFileName_text;
static char introFileName_text[] = "eintro.skf";
char *introFileName = introFileName_text;
static char speechFileName_text[] = "espeech.flx";
char *speechFileName = speechFileName_text;
static char creditDataFile_text[] = "ecredits.dat";
char *creditDataFile = creditDataFile_text;
static char copyrightStatement_text[] = "Ultima VIII: Pagan\015\012Copyright (c) 1994 by Origin Systems.\015\012All Rights Reserved.\015\012";
char *copyrightStatement = copyrightStatement_text;
static char endGameFlagName_text[] = "I81b4u83";
char *endGameFlagName = endGameFlagName_text;
static char quotesFlagName_text[] = "OuPb4Ip";
char *quotesFlagName = quotesFlagName_text;
static char flagExtension_text[] = "iCu";
char *flagExtension = flagExtension_text;
char *thanks[15] = {"Rest, Avatar...you will need it!\015\012", "Va prendre des forces, Avatar...tu en auras besoin!\015\012", "Ruht Euch aus, Avatar...denn in Pagan \201berleben nur die Starken!\015\012", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Use the current language's copy of a file if it exists.
// The first letter of the name is the language letter, e.g. "eusecode.flx".
void languageSensitiveFile(char *dir, char *name)
{
	char path[80];

	strcpy(path, name);
	path[0] = avatar.getLanguageChar();

	if (!FileExists(fileSpec(NULL, dir, path, NULL)))
		return;

	strcpy(name, path);

}
