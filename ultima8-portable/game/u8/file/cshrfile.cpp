// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FILE\CSHRFILE.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
#include "cfile.h"
#include "cshrfile.h"
#include "fexist.h"

// Inline in the shared headers when this file was built.
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline SharedFile::SharedFile(void) {}

#define LOCK_FAILED	0x1960

SharedFile::SharedFile(const char *name) :
	BaseFile(name, RWShareReadWrite)
{
}

void SharedFile::open(const char *name, OpenMode mode)
{
	BaseFile::open(name, mode);
}

// DOS lock region; TRUE on success.
uint8_t SharedFile::lock(int32_t start, int32_t length)
{
	uint16_t startHigh = start >> 16;
	uint16_t startLow = start;
	uint16_t lengthHigh = length >> 16;
	uint16_t lengthLow = length;
	int16_t file = handle;
	uint8_t locked;

	_AH = 0x5c;
	_AL = 0;
	_BX = file;
	_CX = startHigh;
	_DX = startLow;
	_SI = lengthHigh;
	_DI = lengthLow;
	geninterrupt(0x21);
	locked = !(_FLAGS & 1);
	return locked;
}

uint8_t SharedFile::unlock(int32_t start, int32_t length)
{
	uint16_t startHigh = start >> 16;
	uint16_t startLow = start;
	uint16_t lengthHigh = length >> 16;
	uint16_t lengthLow = length;
	int16_t file = handle;
	uint8_t unlocked;

	_AH = 0x5c;
	_AL = 1;
	_BX = file;
	_CX = startHigh;
	_DX = startLow;
	_SI = lengthHigh;
	_DI = lengthLow;
	geninterrupt(0x21);
	unlocked = !(_FLAGS & 1);
	return unlocked;
}

int32_t SharedFile::read(char *buffer, int32_t count, int32_t pos)
{
	return BaseFile::read(buffer, count, pos);
}

int32_t SharedFile::write(char *buffer, int32_t count, int32_t pos)
{
	return BaseFile::write(buffer, count, pos);
}

// Waits until no one else has the file, then keeps byte 0 locked.
QuickExclusiveSharedFile::QuickExclusiveSharedFile(const char *name)
{
	if (!FileExists(name))
		forceOpen(name, RWShareReadWrite);
	else
		BaseFile::open(name, RWShareReadWrite);
	SharedFile other(name);
	while (!lock(0, 1))
		;
	while (!other.lock(0, 2))
		;
	close();
	BaseFile::open(name, RWShareReadWrite);
	lock(0, 1);
	other.unlock(0, 2);
}

QuickExclusiveSharedFile::~QuickExclusiveSharedFile(void)
{
	unlock(0, 1);
}

RecordLock::RecordLock(SharedFile *lockFile, int32_t lockLength, int32_t lockOffset, int16_t lockTries)
{
	file = lockFile;
	offset = lockOffset;
	length = lockLength;
	tries = lockTries;
	int16_t i = 0;
	while (!file->lock(offset, length))
	{
		if (tries == i)
			file->fileError(LOCK_FAILED, __FILE__, 315);	// __LINE__
		i++;
	}
}

RecordLock::~RecordLock(void)
{
	int16_t i = 0;
	while (!file->unlock(offset, length))
	{
		if (tries == i)
			file->fileError(LOCK_FAILED, __FILE__, 330);	// __LINE__
		i++;
	}
}
