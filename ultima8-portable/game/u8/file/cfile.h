#ifndef CFILE_H
#define CFILE_H

#include "u8port.h"

// DOS open modes: access in the low nibble, sharing in the high one.
enum OpenMode : int16_t
{
	ReadOnly = 0x00,
	WriteOnly = 0x01,
	ReadWrite = 0x02,
	NoShareReadOnly = 0x10,
	NoShareWriteOnly = 0x11,
	NoShareReadWrite = 0x12,
	ReadShareReadOnly = 0x20,
	ReadShareWriteOnly = 0x21,
	ReadShareReadWrite = 0x22,
	WriteShareReadOnly = 0x30,
	WriteShareWriteOnly = 0x31,
	WriteShareReadWrite = 0x32,
	RWShareReadOnly = 0x40,
	RWShareWriteOnly = 0x41,
	RWShareReadWrite = 0x42
};

enum SeekMode : int16_t
{
	FromStart,
	FromCurrent,
	FromEnd
};

// fileError codes
#define FILE_OPEN_FAILED	0x1910
#define FILE_WRONG_STATE	0x1911
#define FILE_WRITE_FAILED	0x1920
#define FILE_READ_FAILED	0x1930
#define FILE_SEEK_FAILED	0x1940
#define FILE_STAT_FAILED	0x1950
#define FILE_REMOVE_FAILED	0x1961
#define FILE_RENAME_FAILED	0x1962
#define FILE_RENAME_OPEN	0x1963
#define FILE_RESIZE_FAILED	0x1965

class BaseFile
{
public:
	char filename[80];
	int16_t handle;
	OpenMode mode;

	BaseFile(void);
	BaseFile(const char *, OpenMode);
	~BaseFile(void);
	virtual int32_t read(char *, int32_t, int32_t = -1);
	virtual int32_t write(char *, int32_t, int32_t = -1);
	virtual void fileError(int16_t, char * = 0, int16_t = -1);
	void clear(void);
	void open(const char *, OpenMode);
	uint8_t eof(void);
	void forceOpen(const char *, OpenMode);
	void remove(void);
	void rename(const char *);
	void openAppend(const char *, OpenMode);
	void create(const char *, OpenMode);
	void close(void);
	int32_t seek(int32_t, SeekMode = FromStart);
	int16_t readString(char *, int16_t, int32_t = -1);
	int32_t size(void);
	int32_t getTime(void);
	char readable(void);
	char writeable(void);
	char executable(void);
	int16_t flush(void);
	int32_t tell(void);
	void relocate(int32_t, int32_t, int32_t, BaseFile * = 0);
	void setSize(int32_t);
	void setFilename(char *);
	uint8_t is_valid(void);
};

int16_t TranslateMode(OpenMode);

extern char *FileModeName[16];

// DOS file routines (OPENFILE.ASM, READFILE.ASM, ...); each sets fileerr to the DOS error
// code, or 0.
int16_t OpenFile(const char *);
int16_t SafeOpenFile(const char *, int16_t);
int16_t SafeCreateFile(const char *, int16_t);
int16_t CloseFile(int16_t);
int32_t ReadFile(int16_t, int32_t, int32_t, void *);
char WriteFile(int16_t, int32_t, int32_t, void *);
int32_t SeekFile(int16_t, int32_t, int16_t);
int16_t file_attribs(const char *);
uint8_t file_exists(const char *);
// The DOS error code a failed open of this path gives (2 not found, 3 no such folder, 5 denied).
int16_t DosOpenError(const char *);
extern int16_t fileerr;
extern uint32_t FileBytesTransferred;

#endif
