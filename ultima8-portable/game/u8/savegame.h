#ifndef SAVEGAME_H
#define SAVEGAME_H

#include "u8port.h"

#include "cfile.h"

// One saved game: every file in a folder packed into "u8save.NNN".
class SavedGames
{
public:
	char name[80];
	int16_t number;
	BaseFile file;
	uint8_t isNew;

	SavedGames(void);
	void setNum(int16_t);
	void unpack(int16_t);
	void pack(int16_t, char *);
};

extern char *gameFileListId;

#endif
