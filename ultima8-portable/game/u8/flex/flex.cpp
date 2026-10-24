// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FLEX\FLEX.C

#include "u8port.h"

#include <stddef.h>
#include <string.h>
#include "flex.h"
#include "cfile.h"
#include "fexist.h"

// Inline in the shared headers when this file was built.
inline Index::Index(void) { size = 0; offset = 0; }
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline SharedFile::SharedFile(void) {}

int16_t Index::operator==(Index &other)
{
	return memcmp(this, &other, sizeof(Index)) == 0;
}

// Makes a new file with count empty records.
void FlexFile::create(char *name, int16_t count)
{
	if (count == -1)
		fileError(FLEX_NO_COUNT, __FILE__, 39);	// __LINE__
	totalIndexes = count;
	forceOpen(name, ReadWrite);
	FlexHeader header;
	header.totalIndexes = count;
	write((char *)&header, sizeof(FlexHeader), 0);
	Index index;
	index.size = 0;
	index.offset = 0;
	for (int16_t i = 0; i < totalIndexes; i++)
		write((char *)&index, sizeof(Index), -1);
}

FlexFile::FlexFile(char *name, OpenMode mode, int16_t count)
{
	init(name, mode, count);
}

// Opens the file; a missing one is created when opened for writing.
void FlexFile::init(char *name, OpenMode mode, int16_t count)
{
	uint8_t exists = FileExists(name);

	if (mode == ReadOnly && !exists)
		fileError(FLEX_MISSING, __FILE__, 66);	// __LINE__
	else if (mode == ReadWrite && !exists)
	{
		create(name, count);
		return;
	}
	else
		open(name, mode);
	resetTotalIndexes();
}

void FlexFile::getIndex(int16_t record, Index &index)
{
	if (record >= totalIndexes)
	{
		fileError(FLEX_BAD_INDEX, __FILE__, 80);	// __LINE__
		return;
	}
	if (read((char *)&index, sizeof(Index), sizeof(Index) * record + sizeof(FlexHeader)) != sizeof(Index))
		fileError(FILE_READ_FAILED, __FILE__, 84);	// __LINE__
}

void FlexFile::setIndex(int16_t record, Index &index)
{
	write((char *)&index, sizeof(Index), sizeof(Index) * record + sizeof(FlexHeader));
}

// Reads length bytes (-1: the whole record) from offset within the record.
void FlexFile::readRecord(int16_t record, void *buffer, int32_t length, int32_t offset)
{
	Index index;

	getIndex(record, index);
	if (length == -1)
		length = index.size;
	if (index.size != 0)
		if (read((char *)buffer, length, index.offset + offset) != length)
			fileError(FILE_READ_FAILED, __FILE__, 104);	// __LINE__
}

// Writes into a record, placing a new one at the end of the file. With
// resize set, the record becomes offset + length bytes long.
void FlexFile::writeRecord(int16_t record, void *buffer, int32_t length, int32_t offset, uint8_t resize)
{
	Index index;

	getIndex(record, index);
	if (index.offset == 0)
	{
		if (offset != 0)
			fileError(FLEX_BAD_OFFSET, __FILE__, 115);	// __LINE__
		index.offset = size();
		index.size = length;
		setIndex(record, index);
	}
	if (resize && length + offset != index.size)
		changeRecordLen(record, index, length + offset);
	if (length != 0)
		write((char *)buffer, length, index.offset + offset);
	flush();
}

// A record that grows moves to the end of the file.
void FlexFile::changeRecordLen(int16_t record, Index &index, int32_t length)
{
	if (index.size < length)
		index.offset = size();
	index.size = length;
	setIndex(record, index);
}

void FlexFile::seekRecord(int16_t record)
{
	Index index;

	getIndex(record, index);
	seek(index.offset, FromStart);
}

void FlexFile::setLabel(char *label)
{
	char text[82];

	memset(text, 0x1a, sizeof(text));
	strncpy(text, label, 79);
	write(label, sizeof(text), 0);
}

void FlexFile::getLabel(char *label)
{
	read(label, 80, 0);
}

void FlexFile::setHeader(FlexHeader *header)
{
	write((char *)header, sizeof(FlexHeader), 0);
	totalIndexes = header->totalIndexes;
}

void FlexFile::getHeader(FlexHeader *header)
{
	read((char *)header, sizeof(FlexHeader), 0);
	totalIndexes = header->totalIndexes;
}

void FlexFile::resetTotalIndexes(void)
{
	read((char *)&totalIndexes, sizeof(totalIndexes), 0x54);
}

// Moves a record's data to offset (-1: the end of the file).
void FlexFile::relocate(int16_t record, int32_t offset)
{
	int32_t end = size();

	if (offset == -1)
		offset = end;
	Index index;
	getIndex(record, index);
	if (index.size)
	{
		if (offset == end && index.offset + index.size == end)
			return;
		BaseFile::relocate(index.offset, offset, index.size);
	}
	index.offset = offset;
	setIndex(record, index);
	flush();
}

// Copies record from of this file into record to of dest.
void FlexFile::copy(int16_t from, FlexFile *dest, int16_t to)
{
	if (to == -1)
		to = from;
	Index source;
	Index target;
	getIndex(from, source);
	dest->getIndex(to, target);
	dest->changeRecordLen(to, target, source.size);
	int32_t sourceOffset = source.offset;
	int32_t targetOffset = target.offset;
	dest->BaseFile::relocate(sourceOffset, targetOffset, source.size, this);
	dest->flush();
}

// Sum of all bytes after the header, or -1 without memory for a buffer.
int32_t FlexFile::getChecksum(void)
{
	uint16_t length = 65000;
	char *buffer = NULL;

	while (buffer == NULL)
	{
		buffer = new char[length];
		if (buffer == NULL)
		{
			if (length <= 1000)
				return -1;
			length -= 1000;
		}
	}
	int32_t end = size() - sizeof(FlexHeader);
	int32_t pos = sizeof(FlexHeader);
	int32_t sum = 0;
	seek(pos, FromStart);
	while (pos < end)
	{
		int32_t chunk = (int32_t)length > end - pos ? end - pos : (int32_t)length;
		BaseFile::read(buffer, chunk, -1);
		for (uint16_t i = 0; i < chunk; i++)
			sum += buffer[i];
		pos += chunk;
	}
	delete buffer;
	return sum;
}

int16_t FlexFile::verifyChecksum(int32_t *checksum)
{
	int32_t sum = getChecksum();
	FlexHeader header;

	getHeader(&header);
	if (checksum)
		*checksum = sum;
	return header.checksum == sum;
}

// FALSE if any record runs past the end of the file.
uint8_t FlexFile::verifyIndexes(void)
{
	int32_t end = size();

	for (int16_t i = 0; i < totalIndexes; i++)
	{
		Index index;
		getIndex(i, index);
		if (index.size + index.offset > end)
			return 0;
	}
	return 1;
}

FlexHeader::FlexHeader(void)
{
	memset(this, 0, sizeof(FlexHeader));
	memset(label, 0x1a, sizeof(label));
	version = 1;
}
