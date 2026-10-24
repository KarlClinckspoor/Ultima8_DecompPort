#ifndef SPCHFLEX_H
#define SPCHFLEX_H

#include "u8port.h"

#include "flex.h"
#include "cfile.h"

// A flex of sound samples; offsets[i] is where record i's sample data
// starts, past its 32-byte header, or 0 if it has none.
class SoundEffectFlex : public FlexFile
{
public:
	int32_t *offsets;

	SoundEffectFlex(char *, OpenMode);
	virtual ~SoundEffectFlex(void);
};

// Speech samples; record 0 lists the phrases they say, one per record.
class SpeechFlex : public SoundEffectFlex
{
public:
	int16_t numPhrases;
	char **phrases;

	SpeechFlex(char *, OpenMode);
	virtual ~SpeechFlex(void);
	int16_t search(char *);
};

#endif
