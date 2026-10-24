#ifndef FILESPEC_H
#define FILESPEC_H

#include "u8port.h"

// #include <dir.h>  (DOS only)

char *fileSpec(char *, char *, char *, char *);

// A path split into its parts, and a search through the files it matches.
class FileSpec
{
public:
	char foundName[MAXFILE];
	char foundExt[MAXEXT];
	struct ffblk found;
	int16_t attrib;
	int16_t done;
	char drive[MAXDRIVE];
	char dir[MAXDIR];
	char name[MAXFILE];
	char ext[MAXEXT];

	FileSpec(void);
	FileSpec(char *, char *, char *, char *);
	FileSpec(char *, int16_t);
	void become(char *, int16_t);
	void unlinkAll(void);
	int16_t exists(void);
	int16_t getNext(void);
	int16_t isYounger(FileSpec &);
	int16_t isYounger(uint32_t);
	int32_t getDate(void);
	void getFirst(void);
	void clear(void);
	void reset(void);
	char *get(int16_t);
	operator char *(void) { return fileSpec(drive, dir, name, ext); }
};

char *fileSpec(char *, char *, char *, char *);

#endif
