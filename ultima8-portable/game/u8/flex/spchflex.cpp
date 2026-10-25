// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FLEX\SPCHFLEX.C

#include "u8port.h"

#include <stddef.h>
#include <string.h>
#include "cfile.h"
#include "cexit.h"
#include "spchflex.h"

// Inline in the shared headers when this file was built.
U8_INLINE Index::Index(void) { size = 0; offset = 0; }
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }

SoundEffectFlex::SoundEffectFlex(char *name, OpenMode mode) :
	FlexFile(name, mode, -1)
{
	offsets = NULL;
	offsets = new int32_t[totalIndexes];
	if (offsets == NULL)
		outOfMemory(__FILE__, 15);	// __LINE__
	Index index;
	for (int16_t i = 1; i < totalIndexes; i++)
	{
		offsets[i] = 0;
		getIndex(i, index);
		if (index.size >= 32)
		{
			uint8_t header[32];
			readRecord(i, header, 32, 0);
			if (header[6] == 1 && header[7] == 0)
				offsets[i] = *(uint32_t *)header + 32;
		}
	}
}

SoundEffectFlex::~SoundEffectFlex(void)
{
	if (offsets)
	{
		delete offsets;
		offsets = NULL;
	}
}

SpeechFlex::SpeechFlex(char *name, OpenMode mode) :
	SoundEffectFlex(name, mode)
{
	Index index;
	getIndex(0, index);
	char *text = new char[index.size + 1];
	readRecord(0, text, -1, 0);
	text[index.size] = 0;

	numPhrases = 0;
	int16_t i;
	for (i = 0; i < index.size; i++)
		if (text[i] == 0)
			numPhrases++;
	if (text[index.size - 1] != 0)
		numPhrases++;

	phrases = NULL;
	phrases = new char *[numPhrases + 1];
	int16_t phrase = 0;
	i = 0;
	char *copy = NULL;
	while (i < index.size)
	{
		char buffer[256];
		int16_t length = 0;
		while (text[i] != 0)
		{
			buffer[length] = text[i];
			i++;
			length++;
		}
		buffer[length] = 0;
		// Trim trailing spaces.
		if (buffer[length - 1] == ' ')
		{
			length--;
			while (buffer[length] == ' ')
				length--;
			length++;
		}
		buffer[length] = 0;
		copy = new char[length + 1];
		int16_t n = phrase;
		phrases[n] = copy;
		int16_t m = n;
		strcpy(phrases[m], buffer);
		phrase++;
		i++;
	}
	if (text)
		delete text;
}

SpeechFlex::~SpeechFlex(void)
{
	if (phrases)
	{
		for (int16_t i = 0; i < numPhrases; i++)
			if (phrases[i])
			{
				delete phrases[i];
				phrases[i] = NULL;
			}
		delete phrases;
		phrases = NULL;
	}
}

// Number (from 1) of the phrase that name starts with, or 0.
int16_t SpeechFlex::search(char *name)
{
	int16_t i, j;
	int16_t found;

	for (i = 0, found = 0; i < numPhrases; i++)
	{
		j = 0;
		while (phrases[i][j] != 0 && phrases[i][j] == name[j])
			j++;
		if (!phrases[i][j])
		{
			found = i + 1;
			break;
		}
	}
	return found;
}
