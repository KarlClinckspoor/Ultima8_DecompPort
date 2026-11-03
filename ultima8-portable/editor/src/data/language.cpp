#include "data/language.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <algorithm>
#include <limits>
#include <map>

#include "data/flex.h"

namespace
{

QString fail(QString *error, const QString &message)
{
	if (error)
		*error = message;
	return QString();
}

bool readFile(const QString &path, QByteArray *out, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
	{
		fail(error, QObject::tr("Cannot read %1: %2").arg(QDir::toNativeSeparators(path), f.errorString()));
		return false;
	}
	*out = f.readAll();
	return true;
}

bool writeFile(const QString &path, const QByteArray &data, QString *error)
{
	QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::WriteOwner);
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(data) != data.size())
	{
		fail(error, QObject::tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), f.errorString()));
		return false;
	}
	return true;
}

// A path below dir whose parts are matched without regard to case, as DOS did.
QString findIn(const QString &dir, const QString &relative)
{
	QString at = dir;
	for (const QString &part : relative.split('/'))
	{
		QString found = part;
		for (const QString &e : QDir(at).entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
			if (e.compare(part, Qt::CaseInsensitive) == 0)
			{
				found = e;
				break;
			}
		at = QDir(at).filePath(found);
	}
	return at;
}

// --- Flex files ---

struct FlexIndex
{
	uint32_t offset;
	uint32_t size;
};

bool flexIndex(const QByteArray &d, std::vector<FlexIndex> *out, QString *error)
{
	if (d.size() < 0x80)
	{
		fail(error, QObject::tr("too short to be a flex file"));
		return false;
	}
	const uint32_t n = read32(d.constData() + 0x54);
	if (0x80 + qint64(n) * 8 > d.size())
	{
		fail(error, QObject::tr("its index runs past its end"));
		return false;
	}
	out->resize(n);
	for (uint32_t i = 0; i < n; i++)
	{
		(*out)[i].offset = read32(d.constData() + 0x80 + i * 8);
		(*out)[i].size = read32(d.constData() + 0x80 + i * 8 + 4);
		if (qint64((*out)[i].offset) + (*out)[i].size > d.size())
		{
			fail(error, QObject::tr("record %1 runs past its end").arg(i));
			return false;
		}
	}
	return true;
}

void put32(QByteArray &b, int at, uint32_t v)
{
	for (int i = 0; i < 4; i++)
		b[at + i] = char((v >> (8 * i)) & 0xff);
}

void put16(QByteArray &b, int at, uint16_t v)
{
	b[at] = char(v & 0xff);
	b[at + 1] = char(v >> 8);
}

// A flex file with the original's header and these records, laid out in the order the
// original's are (EUSECODE.FLX keeps records 0 and 1 at its end), and the checksum
// FlexFile::getChecksum gives where the original has one.
QByteArray writeFlex(const QByteArray &original, const std::vector<FlexIndex> &old, const QVector<QByteArray> &records)
{
	const int n = records.size();
	QByteArray out = original.left(0x80);
	put32(out, 0x54, uint32_t(n));
	QByteArray index(n * 8, '\0');
	std::vector<int> order(n);
	for (int i = 0; i < n; i++)
		order[i] = i;
	auto place = [&](int i) {
		return i < int(old.size()) && old[i].size ? qint64(old[i].offset) : std::numeric_limits<qint64>::max();
	};
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return place(a) < place(b); });
	QByteArray body;
	const uint32_t start = uint32_t(0x80 + n * 8);
	for (int i : order)
		if (!records[i].isEmpty())
		{
			put32(index, i * 8, start + uint32_t(body.size()));
			put32(index, i * 8 + 4, uint32_t(records[i].size()));
			body.append(records[i]);
		}
	out.append(index);
	out.append(body);
	if (read32(original.constData() + 0x60) != 0)
	{
		int32_t sum = 0;
		for (int i = 0x80; i < out.size() - 0x80; i++)
			sum += int8_t(out[i]);
		put32(out, 0x60, uint32_t(sum));
	}
	return out;
}

// --- Usecode (u8/inter.cpp) ---

// Operand bytes after each opcode; -1 is not an opcode. 0x0d (push string) and 0x5c (debug
// unit) are sized by decode.
int operandBytes(uint8_t op)
{
	static int table[256];
	static bool made = false;
	if (!made)
	{
		std::fill(std::begin(table), std::end(table), -1);
		const int lens[][2] = {{0x00, 1}, {0x01, 1}, {0x02, 1}, {0x03, 2}, {0x08, 0}, {0x09, 3}, {0x0a, 1},
			{0x0b, 2}, {0x0c, 4}, {0x0e, 2}, {0x0f, 3}, {0x10, 4}, {0x11, 4}, {0x12, 0}, {0x13, 0}, {0x14, 0},
			{0x15, 0}, {0x16, 0}, {0x17, 0}, {0x18, 1}, {0x19, 1}, {0x1a, 1}, {0x1b, 1}, {0x38, 2}, {0x3e, 1},
			{0x3f, 1}, {0x40, 1}, {0x41, 1}, {0x42, 2}, {0x43, 1}, {0x44, 2}, {0x45, 2}, {0x4b, 1}, {0x4c, 1},
			{0x4d, 1}, {0x4e, 3}, {0x4f, 3}, {0x50, 0}, {0x51, 2}, {0x52, 2}, {0x53, 0}, {0x54, 2}, {0x55, 0},
			{0x56, 0}, {0x57, 6}, {0x58, 8}, {0x59, 0}, {0x5a, 1}, {0x5b, 2}, {0x5d, 0}, {0x5e, 0}, {0x5f, 0},
			{0x60, 0}, {0x61, 0}, {0x62, 1}, {0x63, 1}, {0x64, 1}, {0x65, 1}, {0x66, 1}, {0x67, 1}, {0x68, 0},
			{0x69, 1}, {0x6a, 0}, {0x6b, 0}, {0x6c, 2}, {0x6d, 0}, {0x6e, 1}, {0x6f, 1}, {0x70, 3}, {0x73, 0},
			{0x74, 1}, {0x75, 4}, {0x76, 4}, {0x77, 0}, {0x78, 0}, {0x79, 0}};
		for (const auto &l : lens)
			table[l[0]] = l[1];
		for (int op = 0x1c; op < 0x38; op++)
			if (table[op] < 0)
				table[op] = 0;
		for (int op = 0x39; op <= 0x3d; op++)
			table[op] = 0;
		made = true;
	}
	return table[op];
}

struct Instruction
{
	int at;
	uint8_t op;
	int size;
};

// The instructions of a class's code (after its 12-byte header), from 0x80, past the 32
// event offsets.
bool decode(const QByteArray &code, std::vector<Instruction> *out, QString *error)
{
	out->clear();
	int at = 0x80;
	const auto *c = reinterpret_cast<const uint8_t *>(code.constData());
	while (at < code.size())
	{
		const uint8_t op = c[at];
		int size;
		if (op == 0x0d)
		{
			if (at + 3 > code.size())
				break;
			size = 3 + (c[at + 1] | c[at + 2] << 8) + 1;
		}
		else if (op == 0x5c)
			size = 12;
		else if (operandBytes(op) >= 0)
			size = 1 + operandBytes(op);
		else
		{
			fail(error, QObject::tr("unknown opcode %1 at %2").arg(op, 2, 16, QChar('0')).arg(at, 0, 16));
			return false;
		}
		out->push_back({at, op, size});
		at += size;
	}
	if (at != code.size())
	{
		fail(error, QObject::tr("the last instruction runs past the end"));
		return false;
	}
	return true;
}

int16_t s16(const QByteArray &b, int at)
{
	return int16_t(read16(b.constData() + at));
}

}

// --- Tables ---

bool TranslationTable::load(const QString &path, QString *error)
{
	QByteArray bytes;
	if (!readFile(path, &bytes, error))
		return false;
	QJsonParseError parse;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse);
	if (!doc.isObject())
	{
		fail(error, QObject::tr("%1 is not a translation table: %2").arg(QFileInfo(path).fileName(), parse.errorString()));
		return false;
	}
	const QJsonObject o = doc.object();
	if (!o.value("strings").isArray())
	{
		fail(error, QObject::tr("%1 is not a translation table: it has no strings").arg(QFileInfo(path).fileName()));
		return false;
	}
	language = o.value("language").toString();
	folder = o.value("folder").toString();
	fontPatch.clear();
	if (!o.value("font_patch").toString().isEmpty())
		fontPatch = QFileInfo(path).dir().filePath(o.value("font_patch").toString());
	strings.clear();
	keep.clear();
	count = translated = 0;
	for (const QJsonValue &k : o.value("keep").toArray())
	{
		const QJsonArray a = k.toArray();
		keep.insert(key(a.at(0).toInt(), a.at(1).toInt()), true);
	}
	for (const QJsonValue &v : o.value("strings").toArray())
	{
		const QJsonObject s = v.toObject();
		count++;
		const QString tr = s.value("tr").toString();
		if (tr.isEmpty())
			continue;
		translated++;
		strings.insert(s.value("en").toString(), Entry{tr, s.value("cls").toString()});
	}
	return true;
}

bool FontPatch::load(const QString &path, QString *error)
{
	QByteArray bytes;
	if (!readFile(path, &bytes, error))
		return false;
	const QJsonObject o = QJsonDocument::fromJson(bytes).object();
	if (o.value("format").toString() != "u8fonts-glyphs 1")
	{
		fail(error, QObject::tr("%1 is not a font patch").arg(QFileInfo(path).fileName()));
		return false;
	}
	glyphs.clear();
	characters.clear();
	for (const QJsonValue &v : o.value("glyphs").toArray())
	{
		const QJsonObject g = v.toObject();
		glyphs.append({g.value("font").toInt(), g.value("code").toInt(),
			QByteArray::fromBase64(g.value("frame").toString().toLatin1())});
	}
	const QJsonObject chars = o.value("characters").toObject();
	for (auto it = chars.begin(); it != chars.end(); ++it)
		if (it.key().size() == 1)
			characters.insert(it.key().at(0), uint8_t(it.value().toInt()));
	baseSha256 = o.value("base").toObject().value("sha256").toString().toLatin1();
	baseSize = o.value("base").toObject().value("size").toInteger();
	return true;
}

QByteArray FontPatch::apply(const QByteArray &fonts, QString *error) const
{
	std::vector<FlexIndex> index;
	if (!flexIndex(fonts, &index, error))
		return QByteArray();
	QVector<QByteArray> records;
	for (const FlexIndex &e : index)
		records.append(fonts.mid(int(e.offset), int(e.size)));
	for (const Glyph &g : glyphs)
	{
		if (g.font < 0 || g.font >= records.size() || records[g.font].size() < 6)
			return fail(error, QObject::tr("U8FONTS.FLX has no font %1").arg(g.font)).toLatin1();
		QByteArray &shape = records[g.font];
		const int frames = read16(shape.constData() + 4);
		if (g.code < 0 || g.code >= frames || 6 + (g.code + 1) * 6 > shape.size())
			return fail(error, QObject::tr("font %1 has no character %2").arg(g.font).arg(g.code)).toLatin1();
		const uint32_t at = uint32_t(shape.size());
		const int slot = 6 + g.code * 6;
		shape[slot] = char(at & 0xff);
		shape[slot + 1] = char((at >> 8) & 0xff);
		shape[slot + 2] = char((at >> 16) & 0xff);
		shape[slot + 3] = 0;
		put16(shape, slot + 4, uint16_t(g.frame.size()));
		shape.append(g.frame);
	}
	return writeFlex(fonts, index, records);
}

// --- Usecode ---

namespace Language
{

QHash<int, QVector<QByteArray>> usecodeStrings(const QByteArray &usecode, QString *error)
{
	QHash<int, QVector<QByteArray>> out;
	std::vector<FlexIndex> index;
	if (!flexIndex(usecode, &index, error))
		return out;
	std::vector<Instruction> ins;
	for (int r = 2; r < int(index.size()); r++)
	{
		if (index[r].size <= 12)
			continue;
		const QByteArray code = usecode.mid(int(index[r].offset) + 12, int(index[r].size) - 12);
		if (!decode(code, &ins, error))
		{
			out.clear();
			return out;
		}
		QVector<QByteArray> &list = out[r - 2];
		for (const Instruction &i : ins)
			if (i.op == 0x0d)
				list.append(code.mid(i.at + 3, i.size - 4));
	}
	return out;
}

QByteArray rebuildUsecode(const QByteArray &usecode, const QHash<int, QVector<QByteArray>> &strings, QString *error)
{
	std::vector<FlexIndex> index;
	if (!flexIndex(usecode, &index, error))
		return QByteArray();
	struct Class
	{
		QByteArray code;		// the old code
		std::vector<Instruction> ins;
		QByteArray rebuilt;		// the new code
		std::map<int, int> moved;	// old offset of each instruction (and of the end) -> new
	};
	std::map<int, Class> classes;
	for (int r = 2; r < int(index.size()); r++)
	{
		if (index[r].size == 0)
			continue;
		const int c = r - 2;
		Class &k = classes[c];
		k.code = usecode.mid(int(index[r].offset) + 12, int(index[r].size) - 12);
		if (!decode(k.code, &k.ins, error))
			return QByteArray();
		auto given = strings.constFind(c);
		int n = 0;
		k.rebuilt = k.code.left(0x80);
		for (const Instruction &i : k.ins)
		{
			k.moved[i.at] = k.rebuilt.size();
			if (i.op == 0x0d)
			{
				QByteArray s = given != strings.cend() ? (n < given->size() ? given->at(n) : QByteArray())
					: k.code.mid(i.at + 3, i.size - 4);
				if (given != strings.cend() && n >= given->size())
					return fail(error, QObject::tr("class %1 has more strings than given").arg(c)).toLatin1();
				if (s.contains('\0') || s.size() > 0xfff0)
					return fail(error, QObject::tr("class %1: string %2 cannot be stored").arg(c).arg(n)).toLatin1();
				n++;
				k.rebuilt.append(char(0x0d));
				k.rebuilt.append(char(s.size() & 0xff));
				k.rebuilt.append(char(s.size() >> 8));
				k.rebuilt.append(s);
				k.rebuilt.append('\0');
			}
			else
				k.rebuilt.append(k.code.mid(i.at, i.size));
		}
		k.moved[k.code.size()] = k.rebuilt.size();
		if (given != strings.cend() && n != given->size())
			return fail(error, QObject::tr("class %1 has %2 strings, %3 given").arg(c).arg(n).arg(given->size())).toLatin1();
	}

	QString bad;
	auto where = [&](int c, int offset) -> int {
		auto k = classes.find(c);
		if (k == classes.end())
			return offset;
		auto m = k->second.moved.find(offset);
		if (m == k->second.moved.end())
		{
			bad = QObject::tr("class %1: %2 is not an instruction").arg(c).arg(offset, 0, 16);
			return offset;
		}
		return m->second;
	};

	QVector<QByteArray> records;
	for (const FlexIndex &e : index)
		records.append(usecode.mid(int(e.offset), int(e.size)));
	for (auto &[c, k] : classes)
	{
		QByteArray &out = k.rebuilt;
		for (int e = 0; e < 32; e++)
		{
			const uint32_t off = read32(k.code.constData() + e * 4);
			if (off)
				put32(out, e * 4, uint32_t(where(c, int(off))));
		}
		for (const Instruction &i : k.ins)
		{
			const int na = k.moved[i.at];
			switch (i.op)
			{
			case 0x51:	// branch if false, branch: 16 bits from the instruction's end,
			case 0x52:	// wrapping at 64K (some classes are over 32K)
			case 0x75:	// list loops: their exit, in operand bytes 3-4
			case 0x76:
			{
				const int pos = i.op <= 0x52 ? 1 : 3;
				const int target = (i.at + i.size + s16(k.code, i.at + pos)) & 0xffff;
				put16(out, na + pos, uint16_t((where(c, target) - (na + i.size)) & 0xffff));
				break;
			}
			case 0x11:	// call: class, offset
			case 0x57:	// spawn: class, offset in operand bytes 3-6
			{
				const int pos = i.op == 0x11 ? 1 : 3;
				const int unit = read16(k.code.constData() + i.at + pos);
				const int off = read16(k.code.constData() + i.at + pos + 2);
				if (classes.count(unit))
					put16(out, na + pos + 2, uint16_t(where(unit, off)));
				break;
			}
			case 0x58:	// spawn inline: class, the routine's start, and a delta to the code it runs
			{
				const int unit = read16(k.code.constData() + i.at + 1);
				const int base = read16(k.code.constData() + i.at + 3);
				const int delta = s16(k.code, i.at + 5);
				if (classes.count(unit))
				{
					const int target = where(unit, base + delta);
					auto m = classes[unit].moved.find(base);
					const int nb = m != classes[unit].moved.end() ? m->second : base;
					put16(out, na + 3, uint16_t(nb));
					put16(out, na + 5, uint16_t(int16_t(target - nb)));
				}
				break;
			}
			default:
				break;
			}
		}
		if (!bad.isEmpty())
			return fail(error, bad).toLatin1();
		if (out.size() + 12 > 0xffff)
			return fail(error, QObject::tr("class %1 would be %2 bytes; a class must stay under 64K").arg(c).arg(out.size() + 12)).toLatin1();
		// The three numbers before the code all move with its size.
		const QByteArray &old = records[c + 2];
		const int64_t delta = int64_t(out.size()) + 12 - read32(old.constData() + 4);
		QByteArray rec(12, '\0');
		for (int f = 0; f < 3; f++)
			put32(rec, f * 4, uint32_t(int64_t(read32(old.constData() + f * 4)) + delta));
		records[c + 2] = rec + out;
	}
	return writeFlex(usecode, index, records);
}

QString fromCp437(const QByteArray &bytes)
{
	static const char16_t high[128] = {0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
		0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4,
		0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
		0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD,
		0x00BC, 0x00A1, 0x00AB, 0x00BB, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
		0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510, 0x2514, 0x2534, 0x252C, 0x251C,
		0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
		0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588,
		0x2584, 0x258C, 0x2590, 0x2580, 0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
		0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229, 0x2261, 0x00B1, 0x2265, 0x2264,
		0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0};
	QString out;
	out.reserve(bytes.size());
	for (char ch : bytes)
	{
		const uint8_t b = uint8_t(ch);
		out.append(b < 128 ? QChar(b) : QChar(high[b - 128]));
	}
	return out;
}

QByteArray encode(const QString &text, const QHash<QChar, uint8_t> &characters, bool carved, QString *error)
{
	static const QHash<QChar, QString> quotes = {{QChar(0x2019), "'"}, {QChar(0x2018), "'"}, {QChar(0x201c), "\""},
		{QChar(0x201d), "\""}, {QChar(0x2014), "-"}, {QChar(0x2013), "-"}, {QChar(0x2026), "..."}, {QChar(0x00a0), " "}};
	QString t;
	for (QChar ch : text)
		t += quotes.value(ch, QString(ch));
	if (carved)
	{
		QString plain;
		for (QChar ch : t.normalized(QString::NormalizationForm_D))
			if (ch.category() != QChar::Mark_NonSpacing)
				plain += ch;
		t = plain;
	}
	QByteArray out;
	out.reserve(t.size());
	for (QChar ch : t)
	{
		if (characters.contains(ch))
			out.append(char(characters.value(ch)));
		else if (ch.unicode() < 128)
			out.append(char(ch.unicode()));
		else
		{
			fail(error, QObject::tr("The fonts have no \"%1\" (in \"%2\").").arg(ch).arg(text.left(60)));
			return QByteArray();
		}
	}
	return out;
}

bool build(const Options &o, Result *result, QString *error, const std::function<void(const QString &)> &progress)
{
	auto say = [&](const QString &s) {
		if (progress)
			progress(s);
	};
	Result r;
	const QString english = QDir(o.englishDir).absolutePath();
	const QString usecodePath = findIn(english, "USECODE/EUSECODE.FLX");
	const QString fontsPath = findIn(english, "STATIC/U8FONTS.FLX");
	if (!QFile::exists(usecodePath) || !QFile::exists(fontsPath))
	{
		fail(error, QObject::tr("%1 is not an English Ultima VIII folder: it needs USECODE/EUSECODE.FLX and STATIC/U8FONTS.FLX.")
			.arg(QDir::toNativeSeparators(english)));
		return false;
	}
	const QString output = QDir(o.outputDir).absolutePath();
	if (QDir(output).exists() && !QDir(output).isEmpty())
	{
		fail(error, QObject::tr("%1 already exists. Choose a new folder.").arg(QDir::toNativeSeparators(output)));
		return false;
	}
	if (output == english || output.startsWith(english + '/'))
	{
		fail(error, QObject::tr("The new folder cannot be inside the English one."));
		return false;
	}

	say(QObject::tr("Reading the translation"));
	TranslationTable table;
	if (!table.load(o.tablePath, error))
		return false;
	FontPatch patch;
	if (table.fontPatch.isEmpty())
	{
		fail(error, QObject::tr("The table names no font patch."));
		return false;
	}
	if (!patch.load(table.fontPatch, error))
		return false;

	say(QObject::tr("Translating the usecode"));
	QByteArray usecode, fonts;
	if (!readFile(usecodePath, &usecode, error) || !readFile(fontsPath, &fonts, error))
		return false;
	QString why;
	QHash<int, QVector<QByteArray>> strings = usecodeStrings(usecode, &why);
	if (strings.isEmpty())
	{
		fail(error, QObject::tr("Cannot read EUSECODE.FLX: %1").arg(why));
		return false;
	}
	for (auto it = strings.begin(); it != strings.end(); ++it)
		for (int n = 0; n < it->size(); n++)
		{
			if (table.keep.contains(TranslationTable::key(it.key(), n)))
				continue;
			auto e = table.strings.constFind(fromCp437(it->at(n)));
			if (e == table.strings.cend())
			{
				r.untranslated++;
				continue;
			}
			const bool carved = e->className.contains("GRAVE") || e->className.contains("PLAQUE");
			const QByteArray bytes = encode(e->translation, patch.characters, carved, error);
			if (bytes.isEmpty() && !e->translation.isEmpty())
				return false;
			(*it)[n] = bytes;
			r.translated++;
		}
	const QByteArray newUsecode = rebuildUsecode(usecode, strings, &why);
	if (newUsecode.isEmpty())
	{
		fail(error, QObject::tr("Cannot rebuild the usecode: %1").arg(why));
		return false;
	}
	std::vector<FlexIndex> idx;
	flexIndex(newUsecode, &idx, nullptr);
	for (size_t i = 2; i < idx.size(); i++)
		r.largestClass = std::max(r.largestClass, int(idx[i].size));

	say(QObject::tr("Adding the letters to the fonts"));
	if (patch.baseSize && (fonts.size() != patch.baseSize
		|| QCryptographicHash::hash(fonts, QCryptographicHash::Sha256).toHex() != patch.baseSha256))
		r.warnings << QObject::tr("U8FONTS.FLX is not the one the font patch was made for; the letters were added anyway.");
	const QByteArray newFonts = patch.apply(fonts, &why);
	if (newFonts.isEmpty())
	{
		fail(error, QObject::tr("Cannot patch the fonts: %1").arg(why));
		return false;
	}

	say(QObject::tr("Copying the English folder"));
	static const QRegularExpression voice("^E[0-9]+\\.FLX$", QRegularExpression::CaseInsensitiveOption);
	if (!QDir().mkpath(output))
	{
		fail(error, QObject::tr("Cannot make %1.").arg(QDir::toNativeSeparators(output)));
		return false;
	}
	QDirIterator it(english, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		const QString from = it.next();
		const QString rel = QDir(english).relativeFilePath(from);
		const QString top = rel.section('/', 0, 0);
		const QFileInfo info = it.fileInfo();
		if (top.compare("GAMEDAT", Qt::CaseInsensitive) == 0 && rel.contains('/'))
			continue;	// a game in progress stays behind
		if (rel.endsWith(".orig", Qt::CaseInsensitive))
			continue;
		const QString to = QDir(output).filePath(rel);
		if (info.isDir())
		{
			QDir().mkpath(to);
			continue;
		}
		if (o.removeVoices && top.compare("SOUND", Qt::CaseInsensitive) == 0 && voice.match(info.fileName()).hasMatch())
		{
			r.voicesRemoved++;
			continue;
		}
		if (!QFile::copy(from, to))
		{
			fail(error, QObject::tr("Cannot copy %1.").arg(QDir::toNativeSeparators(from)));
			return false;
		}
		QFile::setPermissions(to, QFile::permissions(to) | QFileDevice::WriteOwner);
	}

	say(QObject::tr("Writing the new files"));
	if (!writeFile(findIn(output, "USECODE/EUSECODE.FLX"), newUsecode, error)
		|| !writeFile(findIn(output, "STATIC/U8FONTS.FLX"), newFonts, error))
		return false;
	if (result)
		*result = r;
	return true;
}

}
