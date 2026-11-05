#include "data/disasm.h"

#include <QHash>
#include <QMutex>

#include <vector>

#include "data/language.h"
#include "data/ucode.h"
#include "data/usecode.h"

namespace Ucode
{

namespace
{

int s8(const QByteArray &c, int at) { return int(int8_t(c[at])); }
int u8(const QByteArray &c, int at) { return int(uint8_t(c[at])); }
int u16(const QByteArray &c, int at) { return u8(c, at) | u8(c, at + 1) << 8; }
int s16(const QByteArray &c, int at) { return int(int16_t(u16(c, at))); }
int32_t s32(const QByteArray &c, int at) { return int32_t(uint32_t(u16(c, at)) | uint32_t(u16(c, at + 2)) << 16); }

QString local(int offset)
{
	return offset < 0 ? QStringLiteral("[bp-%1]").arg(-offset, 2, 16, QLatin1Char('0'))
		: QStringLiteral("[bp+%1]").arg(offset, 2, 16, QLatin1Char('0'));
}

// The name of an opcode without operands of its own.
QString plain(uint8_t op)
{
	static const QHash<int, QString> names{
		{0x08, "pop result"}, {0x12, "pop temp"}, {0x13, "pop temp (long)"},
		{0x14, "add"}, {0x15, "add (long)"}, {0x16, "concat strings"}, {0x17, "concat lists"},
		{0x1c, "sub"}, {0x1d, "sub (long)"}, {0x1e, "mul"}, {0x1f, "mul (long)"}, {0x20, "div"}, {0x21, "div (long)"},
		{0x22, "mod"}, {0x23, "mod (long)"}, {0x24, "=="}, {0x25, "== (long)"}, {0x26, "== (strings)"},
		{0x28, ">"}, {0x29, "> (long)"}, {0x2a, ">="}, {0x2b, ">= (long)"}, {0x2c, "<"}, {0x2d, "< (long)"},
		{0x2e, "<="}, {0x2f, "<= (long)"}, {0x30, "not"}, {0x31, "not (long)"}, {0x32, "and"}, {0x33, "and (long)"},
		{0x34, "or"}, {0x35, "or (long)"}, {0x36, "!="}, {0x37, "!= (long)"}, {0x39, "bit and"}, {0x3a, "bit or"},
		{0x3b, "bit not"}, {0x3c, "shift left"}, {0x3d, "shift right"},
		{0x50, "return"}, {0x53, "suspend"}, {0x55, "and implies"}, {0x56, "or implies"}, {0x59, "push pid"},
		{0x5d, "push temp (byte)"}, {0x5e, "push temp"}, {0x5f, "push temp (long)"}, {0x60, "word to long"},
		{0x61, "long to word"}, {0x68, "duplicate string"}, {0x6a, "pointer to string"}, {0x6b, "string to pointer"},
		{0x6d, "push result"}, {0x73, "search next"}, {0x77, "set process info"}, {0x78, "exclusive"},
		{0x79, "end"}};
	return names.value(op, QStringLiteral("op %1").arg(op, 2, 16, QLatin1Char('0')));
}

}

bool disassemble(const Usecode &usecode, int classId, QVector<Line> *out, QString *error)
{
	out->clear();
	const QByteArray c = usecode.code(classId);
	std::vector<Instruction> ins;
	if (c.size() < 128 || !decode(c, &ins, error))
	{
		if (error && error->isEmpty())
			*error = QObject::tr("class %1 has no code").arg(classId);
		return false;
	}
	// Routines start at the events, where the class calls itself, and where others call it.
	QVector<int> starts = usecode.routineStarts(classId);
	for (const auto &c : callers(usecode, classId))
	{
		const QByteArray from = usecode.code(c.first);
		const int op = u8(from, c.second);
		const int target = u16(from, c.second + (op == 0x11 ? 3 : 5));
		if (!starts.contains(target))
			starts << target;
	}
	QHash<int, int> eventAt;
	for (int e = 0; e < EventCount; e++)
		if (const int off = usecode.eventOffset(classId, e))
			eventAt.insert(off, e);
	for (const Instruction &i : ins)
	{
		if (starts.contains(i.at) || eventAt.contains(i.at))
		{
			Line h;
			h.kind = Line::Head;
			h.at = i.at;
			h.event = eventAt.value(i.at, -1);
			out->append(h);
		}
		Line l;
		l.at = i.at;
		const int a = i.at + 1;	// the operands
		const int end = i.at + i.size;
		switch (i.op)
		{
		case 0x00: l.op = "store byte"; l.args = local(s8(c, a)); break;
		case 0x01: l.op = "store"; l.args = local(s8(c, a)); break;
		case 0x02: l.op = "store long"; l.args = local(s8(c, a)); break;
		case 0x03: l.op = "store bytes"; l.args = QStringLiteral("%1, %2").arg(local(s8(c, a))).arg(s8(c, a + 1)); break;
		case 0x09: l.op = "store list element"; l.args = QStringLiteral("%1, size %2").arg(local(s8(c, a))).arg(s8(c, a + 1)); break;
		case 0x0a: l.op = "push"; l.args = QString::number(s8(c, a)); break;
		case 0x0b: l.op = "push"; l.args = QString::number(s16(c, a));
			if (s16(c, a) > 9 || s16(c, a) < -9)
				l.args += QStringLiteral(" (0x%1)").arg(u16(c, a), 0, 16);
			break;
		case 0x0c: l.op = "push long"; l.args = QStringLiteral("0x%1").arg(uint32_t(s32(c, a)), 8, 16, QLatin1Char('0')); break;
		case 0x0d:
			l.op = "push string";
			l.note = QStringLiteral("\"%1\"").arg(Language::fromCp437(c.mid(i.at + 3, i.size - 4)));
			break;
		case 0x0e: l.op = "make list"; l.args = QStringLiteral("%1 of size %2").arg(s8(c, a + 1)).arg(s8(c, a)); break;
		case 0x0f:
		{
			const int n = u16(c, a + 1);
			const Intrinsic *in = intrinsic(n);
			l.op = "call intrinsic";
			l.args = QStringLiteral("%1, %2 bytes").arg(n).arg(u8(c, a));
			l.note = in ? QString::fromLatin1(in->name) : QObject::tr("(none)");
			break;
		}
		case 0x10: l.op = "call near"; l.jump = (end + s16(c, a)) & 0xffff; l.args = QStringLiteral("%1").arg(l.jump, 4, 16, QLatin1Char('0')); break;
		case 0x11:
			l.op = "call";
			l.callClass = u16(c, a);
			l.callOffset = u16(c, a + 2);
			break;
		case 0x18: l.op = "list xor"; l.args = QStringLiteral("size %1").arg(s8(c, a)); break;
		case 0x19: l.op = "string list xor"; l.args = QStringLiteral("size %1").arg(s8(c, a)); break;
		case 0x1a: l.op = "string list subtract"; l.args = QStringLiteral("size %1").arg(s8(c, a)); break;
		case 0x1b: l.op = "list subtract"; l.args = QStringLiteral("size %1").arg(s8(c, a)); break;
		case 0x27: l.op = "== (bytes)"; l.args = QString::number(s8(c, a)); break;
		case 0x38: l.op = "in list"; l.args = QStringLiteral("size %1").arg(s8(c, a)); break;
		case 0x3e: l.op = "push byte"; l.args = local(s8(c, a)); break;
		case 0x3f: l.op = "push"; l.args = local(s8(c, a)); break;
		case 0x40: l.op = "push long"; l.args = local(s8(c, a)); break;
		case 0x41: l.op = "push string"; l.args = local(s8(c, a)); break;
		case 0x42: l.op = "push list"; l.args = QStringLiteral("%1, size %2").arg(local(s8(c, a))).arg(s8(c, a + 1)); break;
		case 0x43: l.op = "push string list"; l.args = local(s8(c, a)); break;
		case 0x44: l.op = "list element"; l.args = QStringLiteral("size %1").arg(s8(c, a)); break;
		case 0x45: l.op = "push bytes"; l.args = QStringLiteral("%1, %2").arg(local(s8(c, a))).arg(s8(c, a + 1)); break;
		case 0x4b: l.op = "push address"; l.args = local(s8(c, a)); break;
		case 0x4c: l.op = "load through pointer"; l.args = QStringLiteral("%1 bytes").arg(s8(c, a)); break;
		case 0x4d: l.op = "store through pointer"; l.args = QStringLiteral("%1 bytes").arg(s8(c, a)); break;
		case 0x4e:
		case 0x4f:
			l.op = i.op == 0x4e ? "push flag" : "store flag";
			l.flagBit = u16(c, a);
			l.args = QStringLiteral("%1, %2 bit%3").arg(l.flagBit).arg(u8(c, a + 2)).arg(u8(c, a + 2) == 1 ? "" : "s");
			break;
		case 0x51: l.op = "if false, go to"; l.jump = (end + s16(c, a)) & 0xffff; break;
		case 0x52: l.op = "go to"; l.jump = (end + s16(c, a)) & 0xffff; break;
		case 0x54: l.op = "implies"; l.args = QStringLiteral("%1 processes").arg(s8(c, a)); break;
		case 0x57:
			l.op = "spawn";
			l.callClass = u16(c, a + 2);
			l.callOffset = u16(c, a + 4);
			l.args = QStringLiteral("%1 argument bytes").arg(s8(c, a));
			break;
		case 0x58:
			l.op = "spawn inline";
			l.jump = (u16(c, a + 2) + s16(c, a + 4)) & 0xffff;
			break;
		case 0x5a: l.op = "locals"; l.args = QString::number(u8(c, a)); break;
		case 0x5b: l.op = "line"; l.args = QString::number(u16(c, a)); break;
		case 0x5c: l.op = "unit"; l.note = QString::fromLatin1(c.mid(a + 2, 9)).section(QChar(0), 0, 0); break;
		case 0x62:
		case 0x63:
		case 0x64: l.op = i.op == 0x62 ? "free string" : i.op == 0x63 ? "free string list" : "free list";
			l.args = local(s8(c, a)); break;
		case 0x65:
		case 0x66:
		case 0x67: l.op = i.op == 0x65 ? "free string" : i.op == 0x66 ? "free list" : "free string list";
			l.args = QStringLiteral("[sp+%1]").arg(s8(c, a), 2, 16, QLatin1Char('0')); break;
		case 0x69: l.op = "push string address"; l.args = local(s8(c, a)); break;
		case 0x6c: l.op = "give heap"; l.args = QStringLiteral("%1, %2").arg(s8(c, a)).arg(s8(c, a + 1)); break;
		case 0x6e: l.op = "add sp"; l.args = QString::number(s8(c, a)); break;
		case 0x6f: l.op = "push sp address"; l.args = QString::number(s8(c, a)); break;
		case 0x70:
		{
			// The finders of u8/inter.cpp's case 0x70.
			static const char *const kinds[] = {"", "", "area", "area, inside containers too", "container",
				"container, and containers in it", "on top of"};
			const int k = s8(c, a + 2);
			l.op = "search";
			l.args = QStringLiteral("%1, %2 bytes").arg(k >= 0 && k < 7 && *kinds[k] ? QString::fromLatin1(kinds[k])
				: QString::number(k)).arg(s8(c, a + 1));
			break;
		}
		case 0x74: l.op = "push token"; l.args = QString::number(u8(c, a)); break;
		case 0x75:
		case 0x76:
			l.op = i.op == 0x75 ? "for each in list" : "for each in string list";
			l.args = QStringLiteral("%1, size %2").arg(local(s8(c, a))).arg(s8(c, a + 1));
			l.jump = (end + s16(c, a + 2)) & 0xffff;
			break;
		default: l.op = plain(i.op); break;
		}
		if (l.callClass >= 0)
		{
			const QString name = usecode.className(l.callClass);
			l.note = name.isEmpty() ? QStringLiteral("class %1").arg(l.callClass) : name;
		}
		out->append(l);
	}
	return true;
}

QVector<QPair<int, int>> callers(const Usecode &usecode, int classId)
{
	static QMutex lock;
	static const char *built = nullptr;	// the file it was built for
	static QHash<int, QVector<QPair<int, int>>> index;
	QMutexLocker hold(&lock);
	if (built != usecode.file().constData())
	{
		built = usecode.file().constData();
		index.clear();
		for (int k = 0; k < usecode.classCount(); k++)
		{
			const QByteArray c = usecode.code(k);
			std::vector<Instruction> ins;
			if (c.size() < 128 || !decode(c, &ins, nullptr))
				continue;
			for (const Instruction &i : ins)
				if (i.op == 0x11 || i.op == 0x57)
				{
					const int target = u16(c, i.at + (i.op == 0x11 ? 1 : 3));
					if (target != k)
						index[target].append({k, i.at});
				}
		}
	}
	return index.value(classId);
}

}
