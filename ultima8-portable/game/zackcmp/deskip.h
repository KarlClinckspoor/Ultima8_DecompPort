#ifndef DESKIP_H
#define DESKIP_H

#include "u8port.h"

#include "flex.h"
#include "cfile.h"

class Vport;

// One frame's place in a shape record; the top bit of offset marks a
// frame already rebuilt by decompressShape. These structs are read from and
// written to the shape files as they are, so they keep their DOS sizes.
struct FrameIndex
{
	int32_t offset;
	int16_t length;
};

// A shape record: a header, then one FrameIndex per frame.
class SkipShape
{
public:
	int16_t maxWidth;
	int16_t maxHeight;
	int16_t frameCount;
	FrameIndex frames[1];

	uint8_t *getFrame(int16_t);
};

// A frame's skip table header; the row data follows.
struct SkipHeader
{
	int16_t flags;
	uint16_t width;
	uint16_t height;
	int16_t xoff;
	int16_t yoff;
};

// What precedes each rebuilt frame's skip table.
struct FrameOwner
{
	int16_t shape;
	int16_t frame;
	int32_t unknown;
};

static_assert(sizeof(FrameIndex) == 6, "FrameIndex keeps its DOS size");
static_assert(sizeof(SkipHeader) == 10, "SkipHeader keeps its DOS size");
static_assert(sizeof(FrameOwner) == 8, "FrameOwner keeps its DOS size");

// A flex file whose writes can be gathered in a large memory buffer.
class ZackFile : public FlexFile
{
public:
	char *buffer;
	int32_t bufferSize;
	int32_t used;
	int16_t buffering;

	ZackFile(void);
	virtual int32_t read(char *, int32_t, int32_t = -1);
	virtual int32_t write(char *, int32_t, int32_t = -1);
	virtual ~ZackFile(void);
	void startBuffering(void);
	void stopBuffering(void);
	void dumpIt(void);
	void seek(int32_t, SeekMode = FromStart);
	int32_t tell(void);
	void create(char *, int16_t);
};

void slam(Vport *);
void waitKey(void);
void checkKey(void);
void reconstruct_skipdraw(Vport *, int16_t, int16_t, uint8_t *, Vport *);
void decompressShape(char *, ZackFile &, Vport &, Vport &, Vport &);
void deskip(char *, char *, char *);

extern uint16_t invisible;
extern int32_t totalBytes;
extern uint16_t typeNumber;
extern char escapeBytes[5];
extern uint8_t isEscapeByte[256];
extern uint8_t *outputFrameBuffer;

#endif
