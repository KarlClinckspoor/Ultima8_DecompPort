#ifndef CSHRFILE_H
#define CSHRFILE_H

#include "u8port.h"

#include "cfile.h"

// A file opened for shared access, with DOS record locking.
class SharedFile : public BaseFile
{
public:
	SharedFile(void);
	SharedFile(const char *);
	void open(const char *, OpenMode);
	uint8_t lock(int32_t, int32_t);
	uint8_t unlock(int32_t, int32_t);
	virtual int32_t read(char *, int32_t, int32_t = -1);
	virtual int32_t write(char *, int32_t, int32_t = -1);
};

// Opens a shared file and holds its first byte locked while it is open.
class QuickExclusiveSharedFile : public SharedFile
{
public:
	QuickExclusiveSharedFile(const char *);
	virtual ~QuickExclusiveSharedFile(void);
};

// Locks a record for its lifetime, failing after a number of tries.
class RecordLock
{
public:
	SharedFile *file;
	int32_t offset;
	int32_t length;
	int16_t tries;

	RecordLock(SharedFile *, int32_t, int32_t, int16_t);
	~RecordLock(void);
};

#endif
