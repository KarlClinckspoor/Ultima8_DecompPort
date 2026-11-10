#include "data/ucasm.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QRegularExpression>
#include <QVector>
#include <QSet>
#include <QStringList>

#include <cstdint>
#include <vector>

#include "data/ucode.h"
#include "data/usecode.h"

namespace Ucasm
{

namespace
{

// The comparisons follow the interpreter (u8/inter.cpp): 0x28 is a < b, with a pushed first.
// Operand fields, one letter each, in the order the bytes come:
//   b s8   B u8   w s16   W u16   l u32 (hex)
//   r  a 16 bit branch distance from the instruction's end, written as a label
//   h  the high word of call_near's 32 bit distance: "sx" when it is the sign of the low word
//   c  a class number (u16)
//   o  an offset in the class named just before (a label when it is this class's)
//   p  spawn_inline's routine start, d its code, as a distance from p (a label when this class's)
//   s  a string (length, bytes, NUL)
//   U  a debug unit: a word, then nine bytes
struct OpDef
{
	uint8_t op;
	const char *name;
	const char *fields;
};

const OpDef defs[] = {
	{0x00, "store_byte", "b"}, {0x01, "store", "b"}, {0x02, "store_long", "b"}, {0x03, "store_bytes", "bb"},
	{0x08, "pop_result", ""}, {0x09, "store_list_element", "bbB"}, {0x0a, "push_byte_imm", "b"},
	{0x0b, "push_imm", "w"}, {0x0c, "push_long_imm", "l"}, {0x0d, "push_string", "s"}, {0x0e, "make_list", "bb"},
	{0x0f, "call_intrinsic", "BW"}, {0x10, "call_near", "rh"}, {0x11, "call", "co"}, {0x12, "pop_temp", ""},
	{0x13, "pop_temp_long", ""}, {0x14, "add", ""}, {0x15, "add_long", ""}, {0x16, "concat_strings", ""},
	{0x17, "concat_lists", ""}, {0x18, "list_xor", "B"}, {0x19, "string_list_xor", "B"},
	{0x1a, "string_list_subtract", "B"}, {0x1b, "list_subtract", "B"}, {0x1c, "sub", ""}, {0x1d, "sub_long", ""},
	{0x1e, "mul", ""}, {0x1f, "mul_long", ""}, {0x20, "div", ""}, {0x21, "div_long", ""}, {0x22, "mod", ""},
	{0x23, "mod_long", ""}, {0x24, "eq", ""}, {0x25, "eq_long", ""}, {0x26, "eq_strings", ""}, {0x28, "lt", ""},
	{0x29, "lt_long", ""}, {0x2a, "le", ""}, {0x2b, "le_long", ""}, {0x2c, "gt", ""}, {0x2d, "gt_long", ""},
	{0x2e, "ge", ""}, {0x2f, "ge_long", ""}, {0x30, "not", ""}, {0x31, "not_long", ""}, {0x32, "and", ""},
	{0x33, "and_long", ""}, {0x34, "or", ""}, {0x35, "or_long", ""}, {0x36, "ne", ""}, {0x37, "ne_long", ""},
	{0x38, "in_list", "bb"}, {0x39, "bit_and", ""}, {0x3a, "bit_or", ""}, {0x3b, "bit_not", ""},
	{0x3c, "shift_left", ""}, {0x3d, "shift_right", ""}, {0x3e, "push_byte_local", "b"}, {0x3f, "push_local", "b"},
	{0x40, "push_long_local", "b"}, {0x41, "push_string_local", "b"}, {0x42, "push_list_local", "bb"},
	{0x43, "push_string_list_local", "b"}, {0x44, "list_element", "bb"}, {0x45, "push_bytes_local", "bb"},
	{0x4b, "push_address", "b"}, {0x4c, "load_ptr", "b"}, {0x4d, "store_ptr", "b"}, {0x4e, "push_flag", "WB"},
	{0x4f, "store_flag", "WB"}, {0x50, "return", ""}, {0x51, "branch_false", "r"}, {0x52, "branch", "r"},
	{0x53, "suspend", ""}, {0x54, "implies", "BB"}, {0x55, "and_implies", ""}, {0x56, "or_implies", ""},
	{0x57, "spawn", "BBco"}, {0x58, "spawn_inline", "cpdBB"}, {0x59, "push_pid", ""}, {0x5a, "locals", "B"},
	{0x5b, "line", "W"}, {0x5c, "unit", "U"}, {0x5d, "push_temp_byte", ""}, {0x5e, "push_temp", ""},
	{0x5f, "push_temp_long", ""}, {0x60, "word_to_long", ""}, {0x61, "long_to_word", ""},
	{0x62, "free_string_local", "b"}, {0x63, "free_string_list_local", "b"}, {0x64, "free_list_local", "b"},
	{0x65, "free_string_sp", "b"}, {0x66, "free_list_sp", "b"}, {0x67, "free_string_list_sp", "b"},
	{0x68, "dup_string", ""}, {0x69, "push_string_address", "b"}, {0x6a, "ptr_to_string", ""},
	{0x6b, "string_to_ptr", ""}, {0x6c, "give_heap", "bb"}, {0x6d, "push_result", ""}, {0x6e, "add_sp", "b"},
	{0x6f, "push_sp_address", "b"}, {0x70, "search", "bbb"}, {0x73, "search_next", ""}, {0x74, "push_token", "B"},
	{0x75, "for_each", "bbr"}, {0x76, "for_each_string", "bbr"}, {0x77, "set_process_info", ""},
	{0x78, "exclusive", ""}, {0x79, "end", ""},
};

const OpDef *byOpcode(uint8_t op)
{
	static const OpDef *table[256];
	static bool made = false;
	if (!made)
	{
		for (const OpDef &d : defs)
			table[d.op] = &d;
		made = true;
	}
	return table[op];
}

const OpDef *byName(const QString &name)
{
	static QHash<QString, const OpDef *> table;
	if (table.isEmpty())
		for (const OpDef &d : defs)
			table.insert(QString::fromLatin1(d.name), &d);
	return table.value(name, nullptr);
}

// An opcode the game never uses: op_XX, its operand bytes as u8s.
QString genericName(uint8_t op)
{
	return QStringLiteral("op_%1").arg(op, 2, 16, QLatin1Char('0'));
}

int u16(const QByteArray &c, int at) { return uint8_t(c[at]) | uint8_t(c[at + 1]) << 8; }
int s16(const QByteArray &c, int at) { return int(int16_t(u16(c, at))); }
uint32_t u32(const QByteArray &c, int at) { return uint32_t(u16(c, at)) | uint32_t(u16(c, at + 2)) << 16; }

QString label(int offset)
{
	return QStringLiteral("L%1").arg(offset, 4, 16, QLatin1Char('0'));
}

QString quoted(const QByteArray &bytes)
{
	QString out = QStringLiteral("\"");
	for (char ch : bytes)
	{
		const uint8_t b = uint8_t(ch);
		if (b == '"' || b == '\\')
		{
			out += QChar('\\');
			out += QChar(b);
		}
		else if (b >= 0x20 && b < 0x7f)
			out += QChar(b);
		else
			out += QStringLiteral("\\x%1").arg(b, 2, 16, QLatin1Char('0'));
	}
	return out + QChar('"');
}

// The bytes of a quoted string; false if it is not one.
bool unquote(const QString &token, QByteArray *out)
{
	out->clear();
	if (token.size() < 2 || token[0] != '"' || token[token.size() - 1] != '"')
		return false;
	for (int i = 1; i < token.size() - 1; i++)
	{
		const QChar ch = token[i];
		if (ch != '\\')
		{
			if (ch.unicode() > 0xff)
				return false;
			out->append(char(ch.unicode()));
			continue;
		}
		if (++i >= token.size() - 1)
			return false;
		if (token[i] == '"' || token[i] == '\\')
			out->append(char(token[i].unicode()));
		else if (token[i] == 'x' && i + 2 < token.size())
		{
			bool ok;
			const int v = token.mid(i + 1, 2).toInt(&ok, 16);
			if (!ok)
				return false;
			out->append(char(v));
			i += 2;
		}
		else
			return false;
	}
	return true;
}

}

QString mnemonic(uint8_t opcode)
{
	const OpDef *d = byOpcode(opcode);
	return d ? QString::fromLatin1(d->name) : genericName(opcode);
}

static QString printImpl(const QByteArray &record, int classId, const Usecode *names, QString *error,
	bool header, int from, int to)
{
	if (record.size() < 12 + 0x80)
	{
		if (error)
			*error = QObject::tr("the record is too short");
		return QString();
	}
	const QByteArray c = record.mid(12);
	std::vector<Ucode::Instruction> ins;
	if (!Ucode::decode(c, &ins, error))
		return QString();
	QSet<int> boundary;
	for (const auto &i : ins)
		boundary.insert(i.at);
	boundary.insert(c.size());

	// Pass 1: the labels.
	QSet<int> labels;
	auto want = [&](int target) {
		if (boundary.contains(target))
			labels.insert(target);
	};
	for (int e = 0; e < 32; e++)
	{
		const uint32_t v = u32(c, e * 4);
		if (v && v <= 0xffff)
			want(int(v));
	}
	for (const auto &i : ins)
	{
		const OpDef *d = byOpcode(i.op);
		if (!d)
			continue;
		int p = i.at + 1;
		int cls = -1, base = 0;
		for (const char *f = d->fields; *f; f++)
		{
			switch (*f)
			{
			case 'b': case 'B': p += 1; break;
			case 'w': case 'W': case 'h': p += 2; break;
			case 'l': p += 4; break;
			case 'r': want((i.at + i.size + s16(c, p)) & 0xffff); p += 2; break;
			case 'c': cls = u16(c, p); p += 2; break;
			case 'o': if (cls == classId) want(u16(c, p)); p += 2; break;
			case 'p': base = u16(c, p); if (cls == classId) want(base); p += 2; break;
			case 'd': if (cls == classId) want((base + s16(c, p)) & 0xffff); p += 2; break;
			case 's': p = i.at + i.size; break;
			case 'U': p += 11; break;
			}
		}
	}

	QString out;
	out += QStringLiteral("class %1").arg(classId);
	if (names && !names->className(classId).isEmpty())
		out += QStringLiteral("    ; %1").arg(names->className(classId));
	out += QChar('\n');
	const uint32_t size = u32(record, 4);
	out += QStringLiteral("header %1 %2\n").arg(int64_t(u32(record, 0)) - int64_t(size)).arg(int64_t(u32(record, 8)) - int64_t(size));
	for (int e = 0; e < 32; e++)
	{
		const uint32_t v = u32(c, e * 4);
		if (!v)
			continue;
		const QString name = Ucode::eventName(e);
		out += QStringLiteral("event %1 %2").arg(e).arg(v <= 0xffff && boundary.contains(int(v)) ? label(int(v)) : QStringLiteral("#%1").arg(v));
		if (!name.isEmpty())
			out += QStringLiteral("    ; %1").arg(name);
		out += QChar('\n');
	}
	out += QStringLiteral("code\n");
	if (!header)
		out.clear();

	for (const auto &i : ins)
	{
		if (i.at < from || i.at >= to)
			continue;
		if (labels.contains(i.at))
			out += label(i.at) + QStringLiteral(":\n");
		const OpDef *d = byOpcode(i.op);
		QStringList args;
		int p = i.at + 1;
		if (!d)
		{
			for (; p < i.at + i.size; p++)
				args << QString::number(uint8_t(c[p]));
		}
		else
		{
			int cls = -1, base = 0;
			for (const char *f = d->fields; *f; f++)
			{
				switch (*f)
				{
				case 'b': args << QString::number(int8_t(c[p])); p += 1; break;
				case 'B': args << QString::number(uint8_t(c[p])); p += 1; break;
				case 'w': args << QString::number(s16(c, p)); p += 2; break;
				case 'W': args << QString::number(u16(c, p)); p += 2; break;
				case 'l': args << QStringLiteral("0x%1").arg(u32(c, p), 8, 16, QLatin1Char('0')); p += 4; break;
				case 'r':
				{
					const int t = (i.at + i.size + s16(c, p)) & 0xffff;
					args << (boundary.contains(t) ? label(t) : QStringLiteral("#%1").arg(s16(c, p)));
					p += 2;
					break;
				}
				case 'h':
				{
					const int lo = s16(c, p - 2);
					const int hi = u16(c, p);
					args << (hi == (lo < 0 ? 0xffff : 0) ? QStringLiteral("sx") : QStringLiteral("0x%1").arg(hi, 4, 16, QLatin1Char('0')));
					p += 2;
					break;
				}
				case 'c': cls = u16(c, p); args << QString::number(cls); p += 2; break;
				case 'o':
					args << (cls == classId && boundary.contains(u16(c, p)) ? label(u16(c, p)) : QString::number(u16(c, p)));
					p += 2;
					break;
				case 'p':
					base = u16(c, p);
					args << (cls == classId && boundary.contains(base) ? label(base) : QString::number(base));
					p += 2;
					break;
				case 'd':
				{
					const int t = (base + s16(c, p)) & 0xffff;
					args << (cls == classId && boundary.contains(t) ? label(t) : QStringLiteral("#%1").arg(s16(c, p)));
					p += 2;
					break;
				}
				case 's':
					args << quoted(c.mid(i.at + 3, i.size - 4));
					p = i.at + i.size;
					break;
				case 'U':
					args << QString::number(u16(c, p)) << quoted(c.mid(p + 2, 9));
					p += 11;
					break;
				}
			}
		}
		out += QStringLiteral("    %1").arg(d ? QString::fromLatin1(d->name) : genericName(i.op));
		if (!args.isEmpty())
			out += QChar(' ') + args.join(QStringLiteral(", "));
		out += QStringLiteral("    ; %1").arg(i.at, 4, 16, QLatin1Char('0'));
		if (i.op == 0x0f)
		{
			const Ucode::Intrinsic *in = Ucode::intrinsic(u16(c, i.at + 2));
			if (in)
				out += QStringLiteral(" %1").arg(QString::fromLatin1(in->name));
		}
		out += QChar('\n');
		if (i.op == 0x79)
			out += QChar('\n');
	}
	if (labels.contains(c.size()) && to >= c.size())
		out += label(c.size()) + QStringLiteral(":\n");
	return out;
}

QString print(const QByteArray &record, int classId, const Usecode *names, QString *error)
{
	return printImpl(record, classId, names, error, true, 0, 0x7fffffff);
}

QString printRange(const QByteArray &record, int classId, int from, int to)
{
	return printImpl(record, classId, nullptr, nullptr, false, from, to);
}

bool decodeInsns(const QByteArray &code, QVector<Insn> *out, QString *error)
{
	std::vector<Ucode::Instruction> ins;
	if (!Ucode::decode(code, &ins, error))
		return false;
	out->clear();
	for (const auto &i : ins)
	{
		Insn r;
		r.at = i.at;
		r.size = i.size;
		r.op = i.op;
		const OpDef *d = byOpcode(i.op);
		r.name = d ? QString::fromLatin1(d->name) : genericName(i.op);
		int p = i.at + 1;
		if (!d)
		{
			for (; p < i.at + i.size; p++)
				r.args << uint8_t(code[p]);
		}
		else
		{
			for (const char *f = d->fields; *f; f++)
			{
				switch (*f)
				{
				case 'b': r.args << int8_t(code[p]); p += 1; break;
				case 'B': r.args << uint8_t(code[p]); p += 1; break;
				case 'w': case 'r': case 'd': r.args << s16(code, p); p += 2; break;
				case 'W': case 'h': case 'c': case 'o': case 'p': r.args << u16(code, p); p += 2; break;
				case 'l': r.args << u32(code, p); p += 4; break;
				case 's': r.text = code.mid(i.at + 3, i.size - 4); p = i.at + i.size; break;
				case 'U': r.args << u16(code, p); r.text = code.mid(p + 2, 9); p += 11; break;
				}
			}
		}
		out->append(r);
	}
	return true;
}

namespace
{

struct Parsed
{
	const OpDef *def = nullptr;
	uint8_t op = 0;
	QStringList args;
	QByteArray text;	// a string's bytes
	int line = 0;
	int at = 0;
	int size = 0;
};

// The line without its comment.
QString stripComment(const QString &line)
{
	bool inString = false;
	for (int i = 0; i < line.size(); i++)
	{
		if (line[i] == '"')
			inString = !inString;
		else if (line[i] == '\\' && inString)
			i++;
		else if (line[i] == ';' && !inString)
			return line.left(i);
	}
	return line;
}

// Splits "a, "b, c", d" at the commas outside strings.
QStringList splitArgs(const QString &s)
{
	QStringList out;
	QString cur;
	bool inString = false;
	for (int i = 0; i < s.size(); i++)
	{
		if (s[i] == '"')
			inString = !inString;
		if (s[i] == '\\' && inString && i + 1 < s.size())
		{
			cur += s[i];
			cur += s[++i];
			continue;
		}
		if (s[i] == ',' && !inString)
		{
			out << cur.trimmed();
			cur.clear();
		}
		else
			cur += s[i];
	}
	if (!cur.trimmed().isEmpty() || !out.isEmpty())
		out << cur.trimmed();
	return out;
}

bool isLabelName(const QString &t)
{
	if (t.isEmpty() || !(t[0].isLetter() || t[0] == '_'))
		return false;
	return t != QStringLiteral("sx");
}

int fieldSize(char f)
{
	switch (f)
	{
	case 'b': case 'B': return 1;
	case 'l': return 4;
	default: return 2;
	}
}

}

QByteArray assemble(const QString &text, int *classId, QString *error)
{
	auto fail = [&](int line, const QString &why) {
		if (error)
			*error = line > 0 ? QObject::tr("line %1: %2").arg(line).arg(why) : why;
		return QByteArray();
	};
	int cls = -1;
	int64_t extra0 = 0, extra2 = 0;
	QVector<QPair<int, QString>> events;	// event, target token
	std::vector<Parsed> code;
	QHash<QString, int> labels;
	int at = 0x80;
	bool inCode = false;
	const QStringList lines = text.split(QChar('\n'));
	for (int n = 0; n < lines.size(); n++)
	{
		const int lineNo = n + 1;
		const QString line = stripComment(lines[n]).trimmed();
		if (line.isEmpty())
			continue;
		if (line.endsWith(QChar(':')) && !line.contains(QChar(' ')))
		{
			const QString name = line.left(line.size() - 1);
			if (!isLabelName(name))
				return fail(lineNo, QObject::tr("%1 is not a label").arg(name));
			if (!inCode)
				return fail(lineNo, QObject::tr("a label before `code`"));
			if (labels.contains(name))
				return fail(lineNo, QObject::tr("the label %1 is defined twice").arg(name));
			labels.insert(name, at);
			continue;
		}
		const int space = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
		const QString word = space < 0 ? line : line.left(space);
		const QString rest = space < 0 ? QString() : line.mid(space).trimmed();
		if (!inCode)
		{
			const QStringList parts = rest.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
			if (word == QStringLiteral("class") && parts.size() == 1)
				cls = parts[0].toInt();
			else if (word == QStringLiteral("header") && parts.size() == 2)
			{
				extra0 = parts[0].toLongLong();
				extra2 = parts[1].toLongLong();
			}
			else if (word == QStringLiteral("event") && parts.size() == 2)
			{
				bool ok;
				const int e = parts[0].toInt(&ok);
				if (!ok || e < 0 || e > 31)
					return fail(lineNo, QObject::tr("an event is 0 to 31"));
				events.append({e, parts[1]});
			}
			else if (word == QStringLiteral("code") && rest.isEmpty())
				inCode = true;
			else
				return fail(lineNo, QObject::tr("unexpected `%1`").arg(line));
			continue;
		}
		Parsed ins;
		ins.line = lineNo;
		ins.at = at;
		ins.def = byName(word);
		if (ins.def)
			ins.op = ins.def->op;
		else if (word.startsWith(QStringLiteral("op_")) && word.size() == 5)
		{
			bool ok;
			ins.op = uint8_t(word.mid(3).toInt(&ok, 16));
			if (!ok || byOpcode(ins.op) || Ucode::operandBytes(ins.op) < 0)
				return fail(lineNo, QObject::tr("unknown instruction %1").arg(word));
		}
		else
			return fail(lineNo, QObject::tr("unknown instruction %1").arg(word));
		ins.args = splitArgs(rest);
		int size = 1;
		if (ins.def)
		{
			int expected = 0;
			for (const char *f = ins.def->fields; *f; f++)
				expected += *f == 's' ? 1 : *f == 'U' ? 2 : 1;
			if (ins.args.size() != expected)
				return fail(lineNo, QObject::tr("%1 takes %2 operands").arg(word).arg(expected));
			for (const char *f = ins.def->fields; *f; f++)
			{
				if (*f == 's')
				{
					if (!unquote(ins.args[0], &ins.text))
						return fail(lineNo, QObject::tr("a quoted string is expected"));
					size += 2 + ins.text.size() + 1;	// the length, the bytes, a NUL (the opcode is in size)
				}
				else if (*f == 'U')
					size += 11;
				else
					size += fieldSize(*f);
			}
		}
		else
		{
			size += Ucode::operandBytes(ins.op);
			if (ins.args.size() != Ucode::operandBytes(ins.op))
				return fail(lineNo, QObject::tr("%1 takes %2 operands").arg(word).arg(Ucode::operandBytes(ins.op)));
		}
		ins.size = size;
		at += size;
		code.push_back(ins);
	}
	if (cls < 0)
		return fail(0, QObject::tr("no `class` line"));
	if (!inCode)
		return fail(0, QObject::tr("no `code` line"));
	if (at + 12 > 0xffff)
		return fail(0, QObject::tr("the class would be %1 bytes; a class must stay under 64K").arg(at + 12));

	auto number = [&](const QString &t, bool *ok) -> int64_t {
		if (t.startsWith(QChar('#')))
			return t.mid(1).toLongLong(ok, 0);
		return t.toLongLong(ok, 0);
	};
	// A branch target or offset: a label, or a number.
	auto target = [&](const QString &t, int line, bool *ok, bool *isRaw = nullptr) -> int64_t {
		*ok = true;
		if (isRaw)
			*isRaw = t.startsWith(QChar('#'));
		if (isLabelName(t))
		{
			auto l = labels.constFind(t);
			if (l == labels.constEnd())
			{
				*ok = false;
				fail(line, QObject::tr("the label %1 is not defined").arg(t));
				return 0;
			}
			return *l;
		}
		const int64_t v = number(t, ok);
		if (!*ok)
			fail(line, QObject::tr("%1 is not a number or label").arg(t));
		return v;
	};

	QByteArray body(at, '\0');
	auto put = [&](int pos, int64_t v, int bytes) {
		for (int i = 0; i < bytes; i++)
			body[pos + i] = char((v >> (8 * i)) & 0xff);
	};
	for (const auto &ev : events)
	{
		bool ok;
		const int64_t v = target(ev.second, 0, &ok);
		if (!ok)
			return fail(0, QObject::tr("event %1: %2").arg(ev.first).arg(*error));
		put(ev.first * 4, v, 4);
	}
	for (const Parsed &ins : code)
	{
		body[ins.at] = char(ins.op);
		int p = ins.at + 1;
		const int end = ins.at + ins.size;
		if (!ins.def)
		{
			for (const QString &a : ins.args)
			{
				bool ok;
				const int64_t v = number(a, &ok);
				if (!ok)
					return fail(ins.line, QObject::tr("%1 is not a number").arg(a));
				put(p++, v, 1);
			}
			continue;
		}
		int arg = 0;
		int64_t base = 0;
		int64_t lastLow = 0;
		for (const char *f = ins.def->fields; *f; f++)
		{
			const QString a = ins.args[arg++];
			bool ok = true;
			switch (*f)
			{
			case 'b': case 'B': case 'w': case 'W': case 'l': case 'c':
			{
				const int64_t v = number(a, &ok);
				if (!ok)
					return fail(ins.line, QObject::tr("%1 is not a number").arg(a));
				put(p, v, fieldSize(*f));
				p += fieldSize(*f);
				break;
			}
			case 'r':
			{
				bool raw;
				int64_t v = target(a, ins.line, &ok, &raw);
				if (!ok)
					return QByteArray();
				v = raw || !isLabelName(a) ? v : (v - end) & 0xffff;
				lastLow = int16_t(v & 0xffff);
				put(p, v, 2);
				p += 2;
				break;
			}
			case 'h':
			{
				if (a == QStringLiteral("sx"))
					put(p, lastLow < 0 ? 0xffff : 0, 2);
				else
				{
					const int64_t v = number(a, &ok);
					if (!ok)
						return fail(ins.line, QObject::tr("%1 is not a number").arg(a));
					put(p, v, 2);
				}
				p += 2;
				break;
			}
			case 'o':
			case 'p':
			{
				const int64_t v = target(a, ins.line, &ok);
				if (!ok)
					return QByteArray();
				put(p, v, 2);
				if (*f == 'p')
					base = v;
				p += 2;
				break;
			}
			case 'd':
			{
				bool raw;
				int64_t v = target(a, ins.line, &ok, &raw);
				if (!ok)
					return QByteArray();
				if (!raw && isLabelName(a))
					v -= base;
				put(p, v, 2);
				p += 2;
				break;
			}
			case 's':
				put(p, ins.text.size(), 2);
				body.replace(p + 2, ins.text.size(), ins.text);
				p = end;
				break;
			case 'U':
			{
				const int64_t w = number(a, &ok);
				QByteArray name;
				if (!ok || !unquote(ins.args[arg++], &name) || name.size() != 9)
					return fail(ins.line, QObject::tr("unit takes a number and a 9 byte string"));
				put(p, w, 2);
				body.replace(p + 2, 9, name);
				p += 11;
				break;
			}
			}
		}
	}
	const int64_t size = 12 + body.size();
	QByteArray record(12, '\0');
	auto put32 = [&](int pos, int64_t v) {
		for (int i = 0; i < 4; i++)
			record[pos + i] = char((v >> (8 * i)) & 0xff);
	};
	put32(0, size + extra0);
	put32(4, size);
	put32(8, size + extra2);
	if (classId)
		*classId = cls;
	return record + body;
}

}
