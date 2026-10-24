#ifndef STEPFLEX_H
#define STEPFLEX_H

#include "u8port.h"

#include "flex.h"
#include "cfile.h"
#include "filespec.h"

// A FlexFile whose changes go to a temporary flex (same name, extension
// .$$$) until commit() merges them into a new copy of the file.
class StepFlexFile : public FlexFile
{
public:
	FlexFile flexFile;
	FileSpec tempSpec;

	StepFlexFile(char *, int16_t, uint8_t);
	virtual ~StepFlexFile(void);
	virtual void init(char *, OpenMode, int16_t);
	virtual void getIndex(int16_t, Index &);
	virtual void setIndex(int16_t, Index &);
	virtual void readRecord(int16_t, void *, int32_t = -1, int32_t = 0);
	virtual void writeRecord(int16_t, void *, int32_t, int32_t = 0, uint8_t = 1);
	virtual void relocate(int16_t, int32_t = -1);
	virtual void changeRecordLen(int16_t, Index &, int32_t);
	virtual void initTemp(void);
	int16_t tempIsLater(int16_t);
	void commit(void);
	void flush(void);
	void close(void);
};

#endif
