#include "data/shapes.h"

#include <QDir>
#include <QFile>

QString familyName(int family)
{
	switch (family)
	{
	case FamilyGeneric: return QStringLiteral("generic");
	case FamilyQuality: return QStringLiteral("quality");
	case FamilyQuantity: return QStringLiteral("quantity");
	case FamilyGlobEgg: return QStringLiteral("glob egg");
	case FamilyUsecodeEgg: return QStringLiteral("usecode egg");
	case FamilyBreakable: return QStringLiteral("breakable");
	case FamilyContainer: return QStringLiteral("container");
	case FamilyMonsterEgg: return QStringLiteral("monster egg");
	case FamilyTeleportEgg: return QStringLiteral("teleport egg");
	case FamilyReagent: return QStringLiteral("reagent");
	}
	return QStringLiteral("family %1").arg(family);
}

TypeInfo TypeInfo::decode(const char *p)
{
	TypeInfo t;
	const uint8_t b0 = uint8_t(p[0]), b1 = uint8_t(p[1]);
	const uint16_t w1 = read16(p + 2), w2 = read16(p + 4);
	t.fixed = b0 & 0x01;
	t.solid = b0 & 0x02;
	t.sea = b0 & 0x04;
	t.land = b0 & 0x08;
	t.occl = b0 & 0x10;
	t.bag = b0 & 0x20;
	t.damaging = b0 & 0x40;
	t.noisy = b0 & 0x80;
	t.draw = b1 & 0x01;
	t.ignore = b1 & 0x02;
	t.roof = b1 & 0x04;
	t.trans = b1 & 0x08;
	t.family = b1 >> 4;
	t.equipType = w1 & 0xf;
	t.xSize = (w1 >> 4) & 0xf;
	t.ySize = (w1 >> 8) & 0xf;
	t.zSize = (w1 >> 12) & 0xf;
	t.animType = w2 & 0xf;
	t.animData = (w2 >> 4) & 0xf;
	t.animSpeed = (w2 >> 8) & 0xf;
	t.editor = w2 & 0x1000;
	t.explode = w2 & 0x2000;
	t.unknown1 = w2 & 0x4000;
	t.unknown2 = w2 & 0x8000;
	t.weight = uint8_t(p[6]);
	t.volume = uint8_t(p[7]);
	return t;
}

bool TypeInfo::isEgg() const
{
	return family == FamilyGlobEgg || family == FamilyUsecodeEgg || family == FamilyMonsterEgg ||
		family == FamilyTeleportEgg;
}

bool Frame::hit(int dx, int dy, bool flip) const
{
	int col = dx - left(flip);
	int row = dy - top();
	if (col < 0 || row < 0 || col >= width || row >= height)
		return false;
	if (flip)
		col = width - 1 - col;
	return opaque[size_t(row) * width + col] != 0;
}

// U8PAL.PAL: a 4 byte header, then 256 colours of 6-bit red, green, blue (FilePalette::read).
bool Palette::load(const QString &path, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
	{
		if (error)
			*error = QStringLiteral("Cannot open %1").arg(path);
		return false;
	}
	QByteArray d = f.readAll();
	if (d.size() < 4 + 768)
	{
		if (error)
			*error = QStringLiteral("%1 is too short").arg(path);
		return false;
	}
	for (int i = 0; i < 256; i++)
	{
		auto c = [&](int k) { return (uint8_t(d[4 + i * 3 + k]) & 63) * 255 / 63; };
		colors_[i] = qRgb(c(0), c(1), c(2));
	}
	return true;
}

static QString staticFile(const QString &dir, const QString &name)
{
	// DOS names are case blind; the files may have either case on disk.
	QDir d(dir);
	for (const QString &entry : d.entryList(QDir::Files))
		if (entry.compare(name, Qt::CaseInsensitive) == 0)
			return d.filePath(entry);
	return d.filePath(name);
}

bool Shapes::load(const QString &staticDir, QString *error)
{
	if (!flex_.load(staticFile(staticDir, "U8SHAPES.FLX"), error))
		return false;
	if (!palette_.load(staticFile(staticDir, "U8PAL.PAL"), error))
		return false;
	QFile tf(staticFile(staticDir, "TYPEFLAG.DAT"));
	if (!tf.open(QIODevice::ReadOnly))
	{
		if (error)
			*error = QStringLiteral("Cannot open TYPEFLAG.DAT in %1").arg(staticDir);
		return false;
	}
	QByteArray t = tf.readAll();
	types_.assign(size_t(std::max(flex_.count(), 2048)), TypeInfo());
	for (int i = 0; i * 8 + 8 <= t.size() && i < int(types_.size()); i++)
		types_[i] = TypeInfo::decode(t.constData() + i * 8);
	frames_.clear();
	frames_.resize(size_t(flex_.count()));
	tried_.assign(size_t(flex_.count()), false);
	return true;
}

const TypeInfo &Shapes::type(int shape) const
{
	static const TypeInfo none;
	if (shape < 0 || shape >= int(types_.size()))
		return none;
	return types_[shape];
}

// A shape record: max width, max height, frame count (16 bits each), then 6 bytes a frame: a
// 24-bit offset, a byte, a 16-bit length (xcache/shaphand.h).
int Shapes::frameCount(int shape) const
{
	const char *p = flex_.data(shape);
	if (!p || flex_.size(shape) < 6)
		return 0;
	return read16(p + 4);
}

const Frame *Shapes::frame(int shape, int frame) const
{
	if (shape < 0 || shape >= flex_.count() || frame < 0)
		return nullptr;
	QMutexLocker hold(&lock_);
	auto &list = frames_[shape];
	if (!tried_[shape])
	{
		tried_[shape] = true;
		int n = frameCount(shape);
		list.resize(size_t(n));
	}
	if (frame >= int(list.size()))
		return nullptr;
	if (!list[frame])
	{
		Frame f = decode(shape, frame);
		if (f.width <= 0 || f.height <= 0)
			return nullptr;
		list[frame] = std::make_unique<Frame>(std::move(f));
	}
	return list[frame].get();
}

// A frame on disk: 8 bytes (its shape and frame numbers and 4 unused), then the five words of
// SKIPFRAME (compression, width, height, x and y hot spot) and one word a row giving the row's
// data as an offset from that word. A row is skip, run pairs until the width is used up; in a
// compressed frame the run is count*2 + fill, and a fill run has one colour byte
// (u8/skipdraw.h).
Frame Shapes::decode(int shape, int frame) const
{
	Frame f;
	const char *s = flex_.data(shape);
	const int size = flex_.size(shape);
	if (!s || 6 + (frame + 1) * 6 > size)
		return f;
	const char *slot = s + 6 + frame * 6;
	uint32_t offset = uint32_t(uint8_t(slot[0])) | uint32_t(uint8_t(slot[1])) << 8 |
		uint32_t(uint8_t(slot[2])) << 16;
	if (offset + 18 > uint32_t(size))
		return f;
	const char *h = s + offset + 8;
	const bool compressed = read16(h) & 1;
	f.width = int16_t(read16(h + 2));
	f.height = int16_t(read16(h + 4));
	f.xoff = int16_t(read16(h + 6));
	f.yoff = int16_t(read16(h + 8));
	if (f.width <= 0 || f.height <= 0 || f.width > 2048 || f.height > 2048 ||
		offset + 18 + uint32_t(f.height) * 2 > uint32_t(size))
	{
		f.width = f.height = 0;
		return f;
	}
	const char *end = s + size;
	f.pixels.assign(size_t(f.width) * f.height, 0);
	f.opaque.assign(size_t(f.width) * f.height, 0);
	for (int row = 0; row < f.height; row++)
	{
		const char *entry = h + 10 + row * 2;
		const uint8_t *p = reinterpret_cast<const uint8_t *>(entry + read16(entry));
		const uint8_t *stop = reinterpret_cast<const uint8_t *>(end);
		int x = 0;
		while (x < f.width && p < stop)
		{
			x += *p++;
			if (x >= f.width || p >= stop)
				break;
			int run = *p++;
			bool fill = false;
			if (compressed)
			{
				fill = run & 1;
				run >>= 1;
			}
			for (int i = 0; i < run && x < f.width; i++, x++)
			{
				if (p >= stop)
					break;
				size_t at = size_t(row) * f.width + x;
				f.pixels[at] = fill ? *p : *p++;
				f.opaque[at] = 1;
			}
			if (fill)
				p++;
		}
	}
	f.image = QImage(f.width, f.height, QImage::Format_ARGB32_Premultiplied);
	for (int row = 0; row < f.height; row++)
	{
		QRgb *line = reinterpret_cast<QRgb *>(f.image.scanLine(row));
		for (int x = 0; x < f.width; x++)
		{
			size_t at = size_t(row) * f.width + x;
			line[x] = f.opaque[at] ? palette_.color(f.pixels[at]) : 0;
		}
	}
	f.flipped = f.image.mirrored(true, false);
	return f;
}
