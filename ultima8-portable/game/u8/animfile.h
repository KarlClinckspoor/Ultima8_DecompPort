#ifndef ANIMFILE_H
#define ANIMFILE_H

#include "u8port.h"

#include "animcach.h"
#include "cfile.h"
#include "item.h"

class AnimHeader;
class AnimStream;

// anim.dat: one offset per shape to an AnimHeader, which holds one offset per animation.
class AnimFile : public BaseFile
{
public:
	char animName[81];

	AnimFile(void) {}
	AnimFile(char *);
	void initialize(char *);
	Boolean load(uint16_t, AnimSet, AnimStream *);
	void save(uint16_t, AnimSet, AnimStream *);
	int32_t optimize(void);
	Boolean getAnimHeader(uint16_t, AnimHeader *);
	Boolean setAnimHeader(uint16_t, AnimHeader *);
	void createHeader(uint16_t);
	void clearHeader(uint16_t);
	void setFilename(char *);
};

extern char animationFile[];

#endif
