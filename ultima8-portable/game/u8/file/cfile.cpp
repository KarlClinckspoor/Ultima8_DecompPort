// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FILE\CFILE.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <io.h>  (DOS only)
#include <stdio.h>
#include <string.h>
// #include <sys\stat.h>  (DOS only)
#include "cfile.h"
#include "plat.h"
#include "cexit.h"
#include "system.h"

// Inline in the shared headers when this file was built.
U8_INLINE SimpleVirtualString::SimpleVirtualString(void) { string = 0; size = 0; }
U8_INLINE SimpleVirtualString::~SimpleVirtualString(void) { free(); }
U8_INLINE SimpleVirtualString::operator char *(void) { return string; }
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }

char *FileModeName[16] = {"ReadOnly", "WriteOnly", "ReadWrite", "NoShareReadOnly", "NoShareWriteOnly", "NoShareReadWrite", "ReadShareReadOnly", "ReadShareWriteOnly", "ReadShareReadWrite", "WriteShareReadOnly", "WriteShareWriteOnly", "WriteShareReadWrite", "RWShareReadOnly", "RWShareWriteOnly", "RWShareReadWrite", "Bad Mode"};

// Index of a mode in FileModeName.
int16_t TranslateMode(OpenMode mode)
{
	int16_t index;

	switch (mode)
	{
	case ReadOnly:				index = 0;	break;
	case WriteOnly:				index = 1;	break;
	case ReadWrite:				index = 2;	break;
	case NoShareReadOnly:		index = 3;	break;
	case NoShareWriteOnly:		index = 4;	break;
	case NoShareReadWrite:		index = 5;	break;
	case ReadShareReadOnly:		index = 6;	break;
	case ReadShareWriteOnly:	index = 7;	break;
	case ReadShareReadWrite:	index = 8;	break;
	case WriteShareReadOnly:	index = 9;	break;
	case WriteShareWriteOnly:	index = 10;	break;
	case WriteShareReadWrite:	index = 11;	break;
	case RWShareReadOnly:		index = 12;	break;
	case RWShareWriteOnly:		index = 13;	break;
	case RWShareReadWrite:		index = 14;	break;
	default:					index = 15;	break;
	}
	return index;
}

void BaseFile::fileError(int16_t error, char *file, int16_t line)
{
	char source[30];

	if (file)
		sprintf(source, "%s (%d)", file, line);
	else
		strcpy(source, "None");
	SimpleVirtualString message;
	message.assemble("Filename =%s\nMode     =%s\nError    =%04x\nDOS Error=%04x\nSource   =%s\n",
		filename, FileModeName[TranslateMode(mode)], error, fileerr, source);
	Fatal(message);
}

BaseFile::BaseFile(void)
{
	filename[0] = 0;
	clear();
}

BaseFile::BaseFile(const char *name, OpenMode mode)
{
	filename[0] = 0;
	clear();
	open(name, mode);
}

void BaseFile::clear(void)
{
	handle = -1;
	mode = ReadWrite;
}

void BaseFile::open(const char *name, OpenMode mode)
{
	strcpy(filename, name);
	this->mode = mode;
	if (handle == -1)
	{
		handle = SafeOpenFile(name, this->mode);
		if (fileerr)
			fileError(FILE_OPEN_FAILED);
	}
	else
		fileError(FILE_WRONG_STATE);
}

uint8_t BaseFile::eof(void)
{
	int32_t pos = tell();
	seek(0, FromEnd);
	int32_t end = tell();
	seek(pos, FromStart);
	return pos >= end;
}

// Opens a fresh copy of the file, creating it.
void BaseFile::forceOpen(const char *name, OpenMode mode)
{
	strcpy(filename, name);
	this->mode = mode;
	unlink(name);
	if (handle == -1)
	{
		handle = SafeOpenFile(name, this->mode);
		if (fileerr)
		{
			if (fileerr == 2)
			{
				handle = -1;
				create(name, mode);
			}
			else
				fileError(FILE_WRONG_STATE);
		}
		else
			fileError(FILE_WRONG_STATE);
	}
	else
		fileError(FILE_WRONG_STATE);
}

void BaseFile::remove(void)
{
	if (is_valid())
		close();
	if (u8_unlink(filename))
		fileError(FILE_REMOVE_FAILED);
}

void BaseFile::rename(const char *name)
{
	if (handle == -1)
	{
		if (::rename(filename, name))
			fileError(FILE_RENAME_FAILED);
		strcpy(filename, name);
	}
	else
		fileError(FILE_RENAME_OPEN);
}

void BaseFile::openAppend(const char *name, OpenMode mode)
{
	strcpy(filename, name);
	this->mode = mode;
	if (handle == -1)
	{
		open(name, mode);
		if (fileerr)
		{
			if (fileerr == 2)
			{
				handle = -1;
				create(name, mode);
			}
			else
				fileError(FILE_WRONG_STATE);
		}
		else
			seek(0, FromEnd);
	}
	else
		fileError(FILE_WRONG_STATE);
}

void BaseFile::create(const char *name, OpenMode mode)
{
	strcpy(filename, name);
	this->mode = mode;
	if (handle == -1)
	{
		if (mode > NoShareReadWrite)
			handle = SafeCreateFile(name, 0x80);
		else
			handle = SafeCreateFile(name, 0);
		close();
		open(name, mode);
		if (fileerr)
			fileError(FILE_OPEN_FAILED);
	}
	else
		fileError(FILE_WRONG_STATE);
}

void BaseFile::close(void)
{
	if (is_valid())
	{
		CloseFile(handle);
		clear();
	}
}

int32_t BaseFile::seek(int32_t offset, SeekMode from)
{
	int32_t pos = -1;

	if (is_valid())
		pos = SeekFile(handle, offset, from);
	if (fileerr)
		fileError(FILE_SEEK_FAILED);
	return pos;
}

// Reads count bytes at pos (-1: the current position). DOS read buffers over 64K in pieces
// through the descriptor's linear address; the host reads them whole.
int32_t BaseFile::read(char *buffer, int32_t count, int32_t pos)
{
	int32_t total = -1;

	if (is_valid())
		total = ReadFile(handle, pos, count, buffer);
	else
		fileError(FILE_READ_FAILED);
	if (fileerr)
		fileError(FILE_READ_FAILED);
	return total;
}

// Reads one line of at most max - 1 characters and leaves the file after it.
int16_t BaseFile::readString(char *buffer, int16_t max, int32_t pos)
{
	int16_t i = 0;
	int32_t start = tell();

	if (is_valid())
	{
		ReadFile(handle, pos, max, buffer);
		if (fileerr)
			fileError(FILE_READ_FAILED);
		while (buffer[i] != '\n' && i < max - 1)
			i++;
		if (i != max - 1)
		{
			buffer[i + 1] = 0;
			seek(start + i + 1, FromStart);
		}
		else
			buffer[max - 1] = 0;
	}
	else
		fileError(FILE_READ_FAILED);
	return i;
}

int32_t BaseFile::write(char *buffer, int32_t count, int32_t pos)
{
	int32_t total = -1;

	if (is_valid())
	{
		// DOS wrote buffers over 64K in pieces and returned -1 for them, whatever happened.
		WriteFile(handle, pos, count, buffer);
		if (count <= 0xffffL)
		{
			if (fileerr == 0)
				total = count;
			else
				fileError(FILE_WRITE_FAILED);
		}
	}
	else
		fileError(FILE_WRITE_FAILED);
	return total;
}

int32_t BaseFile::size(void)
{
	int32_t length = 0;

	if (is_valid())
	{
		int32_t pos = SeekFile(handle, 0, FromCurrent);
		length = SeekFile(handle, 0, FromEnd);
		SeekFile(handle, pos, FromStart);
		if (fileerr)
			fileError(FILE_STAT_FAILED);
	}
	else
		fileError(FILE_STAT_FAILED);
	return length;
}

// The DOS build returned the low 16 bits of the file's access time.
int32_t BaseFile::getTime(void)
{
	uint16_t time = 0;

	if (is_valid())
		time = (uint16_t)plat_file_handle_time(handle);
	else
		fileError(FILE_STAT_FAILED);
	return time;
}

// The host's files are all readable and writable by the game; DOS's stat gave the same.
char BaseFile::readable(void)
{
	if (!is_valid())
		fileError(FILE_STAT_FAILED);
	return 1;
}

char BaseFile::writeable(void)
{
	if (!is_valid())
		fileError(FILE_STAT_FAILED);
	return 1;
}

char BaseFile::executable(void)
{
	if (!is_valid())
		fileError(FILE_STAT_FAILED);
	return 1;
}

// DOS commit file. The platform writes through at once.
int16_t BaseFile::flush(void)
{
	return is_valid();
}

int32_t BaseFile::tell(void)
{
	return SeekFile(handle, 0, FromCurrent);
}

// Copies count bytes from offset from in dest (this file if NULL) to
// offset to in this file, back to front when the ranges overlap forwards.
void BaseFile::relocate(int32_t from, int32_t to, int32_t count, BaseFile *dest)
{
	if (count == 0)
		return;
	if (dest == NULL)
		dest = this;
	int32_t distance = to - from;
	if (distance == 0 && dest == this)
		return;
	if (distance < 0)
		distance = -distance;
	uint8_t backwards = 0;
	uint16_t length = count < 0xfe4fL ? (uint16_t)count : 0xfe4f;
	if (distance < count && from < to && dest == this)
	{
		from = from + count - length;
		to = to + count - length;
		backwards = 1;
	}
	char *buffer = NULL;
	while (length)
	{
		buffer = new char[length];
		if (buffer)
			break;
		length >>= 1;
	}
	if (!length)
		outOfMemory(__FILE__, 951);	// __LINE__
	while (count)
	{
		dest->seek(from, FromStart);
		dest->read(buffer, length, -1);
		seek(to, FromStart);
		write(buffer, length, -1);
		count -= length;
		if (!backwards)
		{
			from += length;
			to += length;
			length = length > count ? count : length;
		}
		else
		{
			length = length > count ? count : length;
			from -= length;
			to -= length;
		}
	}
	delete buffer;
}

void BaseFile::setSize(int32_t size)
{
	if (!is_valid())
		fileError(FILE_WRONG_STATE);
	// chsize: cut the file, or grow it with zeros, keeping the position.
	int32_t pos = SeekFile(handle, 0, FromCurrent);
	int32_t length = SeekFile(handle, 0, FromEnd);
	uint8_t ok = 1;
	if (size < length)
	{
		SeekFile(handle, size, FromStart);
		ok = (uint8_t)plat_file_truncate(handle);
	}
	else if (size > length)
	{
		static const char zeros[512] = {0};
		while (length < size && ok)
		{
			int32_t n = size - length > 512 ? 512 : size - length;
			ok = plat_file_write(handle, zeros, n) == n;
			length += n;
		}
	}
	SeekFile(handle, pos, FromStart);
	if (!ok)
		fileError(FILE_RESIZE_FAILED);
}

void BaseFile::setFilename(char *name)
{
	strcpy(filename, name);
}
