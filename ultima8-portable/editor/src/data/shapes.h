// Shapes (U8SHAPES.FLX), their type flags (TYPEFLAG.DAT) and the palette (U8PAL.PAL).
#pragma once

#include <QImage>
#include <QMutex>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

#include "data/flex.h"

// Shape families (item/type.h).
enum Family
{
	FamilyGeneric = 0,
	FamilyQuality = 1,
	FamilyQuantity = 2,
	FamilyGlobEgg = 3,
	FamilyUsecodeEgg = 4,	// UNKEGG: hatching runs usecode class quality + 1151
	FamilyBreakable = 5,
	FamilyContainer = 6,
	FamilyMonsterEgg = 7,
	FamilyTeleportEgg = 8,
	FamilyReagent = 9,
};

QString familyName(int family);

// One 8-byte TYPEFLAG.DAT record, unpacked (item/type.h, TypeFlag: Borland bit fields from the
// low bit up, two 16-bit units, then weight and volume).
struct TypeInfo
{
	bool fixed = false, solid = false, sea = false, land = false, occl = false, bag = false;
	bool damaging = false, noisy = false, draw = false, ignore = false, roof = false, trans = false;
	bool editor = false, explode = false, unknown1 = false, unknown2 = false;
	uint8_t family = 0, equipType = 0, xSize = 0, ySize = 0, zSize = 0;
	uint8_t animType = 0, animData = 0, animSpeed = 0, weight = 0, volume = 0;

	static TypeInfo decode(const char *p);
	bool isEgg() const;
	// Footpad in world units; a loose shape with no height counts as 1 high (getWorldSize).
	int worldX() const { return xSize * 32; }
	int worldY() const { return ySize * 32; }
	int worldZ() const { return zSize == 0 && !fixed ? 1 : zSize * 8; }
};

// One frame, drawn to an image. The hot spot (xoff, yoff) is where the item's screen position
// lands; a flipped item mirrors the frame about it (SKIPDRAW.ASM, FLIPSKIP.ASM).
struct Frame
{
	int width = 0;
	int height = 0;
	int xoff = 0;
	int yoff = 0;
	QImage image;		// ARGB32 premultiplied, transparent where the frame skips
	QImage flipped;
	std::vector<uint8_t> pixels;	// palette indices, 0xff... see opaque
	std::vector<uint8_t> opaque;	// 1 where the frame draws

	// The frame's rectangle with its hot spot at (0, 0).
	int left(bool flip) const { return flip ? -(width - 1 - xoff) : -xoff; }
	int top() const { return -yoff; }
	bool hit(int dx, int dy, bool flip) const;
};

class Palette
{
public:
	bool load(const QString &path, QString *error);
	QRgb color(int index) const { return colors_[index & 0xff]; }

private:
	QRgb colors_[256] = {};
};

class Shapes
{
public:
	bool load(const QString &staticDir, QString *error);
	int count() const { return flex_.count(); }
	int frameCount(int shape) const;
	// The frame, decoded the first time it is asked for; null when there is none. Thread safe.
	const Frame *frame(int shape, int frame) const;
	const TypeInfo &type(int shape) const;
	const Palette &palette() const { return palette_; }

private:
	Frame decode(int shape, int frame) const;

	Flex flex_;
	Palette palette_;
	std::vector<TypeInfo> types_;
	mutable QMutex lock_;
	mutable std::vector<std::vector<std::unique_ptr<Frame>>> frames_;
	mutable std::vector<bool> tried_;
};
