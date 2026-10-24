#ifndef FLEX_H
#define FLEX_H

#include "u8port.h"

#include "cfile.h"
#include "cshrfile.h"

class FlexHeader;

// Where a record lies in a flex file; an empty record has size 0.
class Index
{
public:
	int32_t offset;
	int32_t size;

	Index(void);
	int16_t operator==(Index &);
	int16_t isEmpty(void);
};

// An archive of numbered records: a FlexHeader, an Index table, then the data.
class FlexFile : public SharedFile
{
public:
	int16_t totalIndexes;

	FlexFile(void);
	FlexFile(char *, OpenMode, int16_t);
	virtual void init(char *, OpenMode, int16_t);
	virtual void getIndex(int16_t, Index &);
	virtual void setIndex(int16_t, Index &);
	virtual void readRecord(int16_t, void *, int32_t = -1, int32_t = 0);
	virtual void writeRecord(int16_t, void *, int32_t, int32_t = 0, uint8_t = 1);
	virtual void setLabel(char *);
	virtual void getLabel(char *);
	virtual void setHeader(FlexHeader *);
	virtual void getHeader(FlexHeader *);
	virtual void relocate(int16_t, int32_t = -1);
	virtual void changeRecordLen(int16_t, Index &, int32_t);
	virtual int32_t getChecksum(void);
	virtual int16_t verifyChecksum(int32_t * = 0);
	void create(char *, int16_t);
	void seekRecord(int16_t);
	void resetTotalIndexes(void);
	void copy(int16_t, FlexFile *, int16_t = -1);
	uint8_t verifyIndexes(void);
};

class FlexHeader
{
public:
	char label[82];
	uint8_t pad_52[2];
	int32_t totalIndexes;
	int32_t version;
	int32_t fileSize;
	int32_t checksum;
	uint8_t pad_64[28];

	FlexHeader(void);
};

// FlexFile::fileError codes
#define FLEX_MISSING	0x2001
#define FLEX_NO_COUNT	0x2002
#define FLEX_BAD_INDEX	0x2003
#define FLEX_BAD_OFFSET	0x2004

#endif
