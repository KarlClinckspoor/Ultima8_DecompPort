#ifndef FILEBOX_H
#define FILEBOX_H

#include "u8port.h"

// #include <dir.h>  (DOS only)
#include "basecam.h"
#include "combobox.h"
#include "filespec.h"
#include "listbox.h"

class Event;
class NewGump;
class NewGumpId;

class FileBox : public ComboBox
{
public:
	char fileSpec[80];

	FileBox(NewGump *, char *);
	virtual void commandPrivate(Event &);
	virtual void init(void);
	virtual void unInit(void);
	void setFileSpec(char *);
	void setDrive(char);
	void setWild(char *);
	char *getDesc(void);
};

class DirectoryBox : public ListBoxGump
{
public:
	char **dirs;

	DirectoryBox(NewGump *, char *);
	virtual void commandMouseLeft(Event &);
	uint8_t setDir(char *);
};

class FileSelector : public PanelGump
{
public:
	char path[MAXDIR];
	int16_t drive;
	FileSpec spec;
	FileSpec *result;
	char driveNames[26][2];
	char *names[6];		// the caller's buttons

	FileSelector(NewGumpId, NewGump *, char *, FileSpec *, ...);
	virtual ~FileSelector(void);
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t);
	uint8_t buildResult(void);
};

#endif
