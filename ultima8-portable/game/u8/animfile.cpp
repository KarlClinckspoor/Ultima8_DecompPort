// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ANIMFILE.C

#include "u8port.h"

// #include <io.h>  (DOS only)
#include <string.h>
#include "error.h"
#include "filespec.h"
#include "aheader.h"
#include "anode.h"
#include "astream.h"
#include "animfile.h"

// Inline in the shared headers when this file was built.
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline FileSpec::FileSpec(char *path, int16_t attrib) { become(path, attrib); }

char animationFile[] = "anim.dat";

AnimFile::AnimFile(char *name) : BaseFile(name, RWShareReadWrite)
{
	if (strlen(name) > 80)
		halt(__FILE__, 27);	// __LINE__
	strcpy(animName, name);
}

// Writes an empty table of shape offsets.
void AnimFile::initialize(char *name)
{
	int16_t i;
	int32_t zero = 0;

	create(name, ReadWrite);
	setFilename(name);
	for (i = 0; i < 0x800; i++)
		write((char *)&zero, 4);
	close();
}

Boolean AnimFile::load(uint16_t shape, AnimSet anim, AnimStream *stream)
{
	AnimHeader header;

	if (!getAnimHeader(shape, &header))
		return FALSE;
	if (!header.hasAnim(anim))
		return FALSE;
	read((char *)&stream->header, 4, header.data[anim]);
	stream->allocate();
	read((char *)stream->nodes, 8 * sizeof(AnimNode) * stream->header.getFrames());
	return TRUE;
}

// Appends the animation to the file; a null stream removes it.
void AnimFile::save(uint16_t shape, AnimSet anim, AnimStream *stream)
{
	int32_t offset;
	AnimHeader header;

	if (!getAnimHeader(shape, &header))
		createHeader(shape);
	if (stream)
	{
		if (!stream->nodes)
			halt(__FILE__, 83);	// __LINE__
		offset = size();
		header.setAnim(anim, offset);
		setAnimHeader(shape, &header);
		write((char *)&stream->header, 4, offset);
		write((char *)stream->nodes, 8 * sizeof(AnimNode) * stream->header.getFrames());
	}
	else
	{
		header.clearAnim(anim);
		setAnimHeader(shape, &header);
	}
}

// Rewrites the file without its dead space and returns the new size.
int32_t AnimFile::optimize(void)
{
	int16_t shape;
	int16_t anim;
	AnimFile temp;
	AnimHeader header;
	AnimStream stream;
	int32_t offset;
	int32_t newSize;
	FileSpec spec(animName, 0);

	strcpy(spec.name, "anim_opt");
	open(animName, RWShareReadWrite);
	temp.initialize(spec);
	temp.open(temp.filename, ReadWrite);
	for (shape = 0; shape < 0x800; shape++)
	{
		if (getAnimHeader(shape, &header) && header.isData())
		{
			temp.setAnimHeader(shape, &header);
			for (anim = 0; anim < 64; anim++)
			{
				if (header.hasAnim((AnimSet)anim))
				{
					offset = header.data[anim];
					read((char *)&stream.header, 4, offset);
					if (stream.header.getFrames())
					{
						stream.allocate();
						read((char *)stream.nodes, 8 * sizeof(AnimNode) * stream.header.frames);
						temp.save(shape, (AnimSet)anim, &stream);
					}
					else
						temp.save(shape, (AnimSet)anim, 0);
				}
			}
		}
	}
	newSize = temp.size();
	relocate(0, 0, temp.size(), &temp);
	setSize(tell());
	temp.close();
	temp.remove();
	close();
	return newSize;
}

Boolean AnimFile::getAnimHeader(uint16_t shape, AnimHeader *header)
{
	int32_t offset;

	read((char *)&offset, 4, shape * 4);
	if (offset)
	{
		read((char *)header, sizeof(AnimHeader), offset);
		return TRUE;
	}
	return FALSE;
}

Boolean AnimFile::setAnimHeader(uint16_t shape, AnimHeader *header)
{
	int32_t offset;

	read((char *)&offset, 4, shape * 4);
	if (offset)
	{
		write((char *)header, sizeof(AnimHeader), offset);
		return TRUE;
	}
	return FALSE;
}

// Appends an empty header for the shape and points its table entry at it.
void AnimFile::createHeader(uint16_t shape)
{
	int32_t offset = size();
	int32_t pos;
	AnimHeader header;

	pos = shape * 4;
	write((char *)&offset, 4, pos);
	write((char *)&header, sizeof(AnimHeader), offset);
}

void AnimFile::clearHeader(uint16_t shape)
{
	int32_t zero = 0;
	int32_t pos = shape * 4;

	write((char *)&zero, 4, pos);
}

void AnimFile::setFilename(char *name)
{
	if (strlen(name) > 80)
		halt(__FILE__, 283);	// __LINE__
	strcpy(animName, name);
	BaseFile::setFilename(animName);
}
