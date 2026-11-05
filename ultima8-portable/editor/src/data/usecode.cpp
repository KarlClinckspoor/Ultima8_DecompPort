#include "data/usecode.h"

#include <QFile>

#include <algorithm>
#include <functional>

#include "data/language.h"
#include "data/ucode.h"

bool Usecode::load(const QString &path, QString *error)
{
	names_.clear();
	events_.clear();
	teleports_.clear();
	flagUses_.clear();
	entries_.clear();
	strings_.clear();
	if (!flex_.load(path, error))
		return false;
	QFile f(path);
	if (f.open(QIODevice::ReadOnly))
		file_ = f.readAll();
	const char *n = flex_.data(1);
	int size = flex_.size(1);
	// The overload table: 13 bytes a class, the events it handles (32 bits) and its name.
	for (int at = 4; n && at + 13 <= size + 4; at += 13)
	{
		events_.append(read32(n + at - 4));
		QByteArray name(n + at, std::min(9, size - at));
		int nul = name.indexOf('\0');
		names_.append(QString::fromLatin1(nul >= 0 ? name.left(nul) : name));
	}
	for (int c = 0; c + 2 < flex_.count(); c++)
		if (flex_.size(c + 2) > 0)
		{
			scan(c, flex_.data(c + 2), flex_.size(c + 2));
			index(c);
		}
	return true;
}

QString Usecode::className(int classId) const
{
	if (classId < 0 || classId >= names_.size())
		return QString();
	return names_[classId];
}

bool Usecode::hasClass(int classId) const
{
	return flex_.size(classId + 2) > 0;
}

QByteArray Usecode::code(int classId) const
{
	if (flex_.size(classId + 2) <= 12)
		return QByteArray();
	return flex_.record(classId + 2).mid(12);
}

uint32_t Usecode::events(int classId) const
{
	return classId >= 0 && classId < events_.size() ? events_[classId] : 0;
}

int Usecode::eventOffset(int classId, int event) const
{
	if (event < 0 || event >= 32 || !(events(classId) & (1u << event)) || flex_.size(classId + 2) < 12 + 128)
		return 0;
	return int(read32(flex_.data(classId + 2) + 12 + event * 4));
}

// The routines (events, and the targets of calls and spawns into the class), the flag reads
// and writes, and the strings, of one class.
void Usecode::index(int classId)
{
	const QByteArray c = code(classId);
	std::vector<Ucode::Instruction> ins;
	if (c.size() < 128 || !Ucode::decode(c, &ins, nullptr))
		return;
	QVector<int> starts;
	for (int e = 0; e < 32; e++)
		if (int off = eventOffset(classId, e))
			starts << off;
	auto word = [&](int at) { return int(read16(c.constData() + at)); };
	QVector<QPair<int, QString>> &strings = strings_[classId];
	for (size_t k = 0; k < ins.size(); k++)
	{
		const Ucode::Instruction &i = ins[k];
		if (i.op == 0x0d)
			strings.append({i.at, Language::fromCp437(c.mid(i.at + 3, i.size - 4))});
		else if ((i.op == 0x11 && word(i.at + 1) == classId) || (i.op == 0x57 && word(i.at + 3) == classId))
			starts << word(i.at + (i.op == 0x11 ? 3 : 5));
		else if (i.op == 0x4e || i.op == 0x4f)
		{
			FlagUse u{word(i.at + 1), uint8_t(c[i.at + 3]), i.op == 0x4f, classId, i.at};
			if (u.write && k > 0)
			{
				const Ucode::Instruction &p = ins[k - 1];
				if (p.op == 0x0a)
					u.value = int8_t(c[p.at + 1]);
				else if (p.op == 0x0b)
					u.value = int16_t(word(p.at + 1));
			}
			flagUses_.append(u);
		}
	}
	std::sort(starts.begin(), starts.end());
	starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
	entries_[classId] = starts;
}

QString Usecode::routineAt(int classId, int offset) const
{
	const QVector<int> starts = entries_.value(classId);
	int start = 0;
	for (int s : starts)
		if (s <= offset)
			start = s;
	for (int e = 0; e < 32; e++)
		if (start && eventOffset(classId, e) == start)
			return Ucode::eventName(e).isEmpty() ? QStringLiteral("event %1").arg(e) : Ucode::eventName(e);
	return QStringLiteral("routine %1").arg(start, 4, 16, QChar('0'));
}

QStringList Usecode::stringsAround(int classId, int offset, int n) const
{
	const QVector<int> starts = entries_.value(classId);
	int from = 0, to = INT_MAX;
	for (int s : starts)
	{
		if (s <= offset)
			from = s;
		else if (s < to)
			to = s;
	}
	QStringList before, after;
	for (const auto &s : strings_.value(classId))
	{
		if (s.first < from || s.first >= to)
			continue;
		if (s.first < offset)
			before << s.second;
		else
			after << s.second;
	}
	QStringList out;
	for (int i = std::max(0, int(before.size()) - n); i < before.size(); i++)
		out << before[i];
	out << QStringLiteral("\u25c6");	// where the flag is used
	for (int i = 0; i < std::min(n, int(after.size())); i++)
		out << after[i];
	return out;
}

// Intrinsic numbers are the order of the table in u8/intrinsics.cpp.
enum { IntrinsicNpcTeleport = 158, IntrinsicTeleportToEgg = 204 };

// Constant pushes just before a call: push s8 is 0a nn, push u16 is 0b nn nn (u8/inter.cpp).
// Reading backwards, a byte can belong to either, so each way is tried until all the values are
// found. values[0] is the push nearest the call, which is the function's first argument.
static bool constantsBefore(const uint8_t *code, int end, int count, std::vector<int> &values)
{
	std::function<bool(int, int)> walk = [&](int at, int left) -> bool {
		if (left == 0)
			return true;
		if (at >= 3 && code[at - 3] == 0x0b)
		{
			values.push_back(code[at - 2] | code[at - 1] << 8);
			if (walk(at - 3, left - 1))
				return true;
			values.pop_back();
		}
		if (at >= 2 && code[at - 2] == 0x0a)
		{
			values.push_back(int(int8_t(code[at - 1])));
			if (walk(at - 2, left - 1))
				return true;
			values.pop_back();
		}
		return false;
	};
	values.clear();
	return walk(end, count);
}

// Finds calls of the two teleport intrinsics (0f, argument bytes, number) whose arguments are
// all constants. This reads the bytes, not the decoded code, so a match inside other data is
// possible; the values are checked to be sensible.
void Usecode::scan(int classId, const char *data, int size)
{
	const uint8_t *code = reinterpret_cast<const uint8_t *>(data);
	std::vector<int> v;
	for (int at = 0; at + 4 <= size; at++)
	{
		if (code[at] != 0x0f)
			continue;
		int number = code[at + 2] | code[at + 3] << 8;
		if (number == IntrinsicTeleportToEgg && code[at + 1] == 6)
		{
			// teleportToEgg(short map, int egg, unsigned char fade)
			if (!constantsBefore(code, at, 3, v))
				continue;
			if (v[0] < 0 || v[0] > 255 || v[1] < 0 || v[1] > 255)
				continue;
			UsecodeTeleport t{UsecodeTeleport::ToEgg, classId, v[0]};
			t.egg = v[1];
			teleports_.append(t);
		}
		else if (number == IntrinsicNpcTeleport && code[at + 1] == 12)
		{
			// Npc::teleport(x, y, z, map), the NPC's reference pushed last, as two bytes
			// (a push of a local or of the item itself).
			if (at < 2 || !constantsBefore(code, at - 2, 4, v))
				continue;
			if (v[3] < 0 || v[3] > 255 || v[0] < 0 || v[1] < 0 || v[2] < 0 || v[2] > 255)
				continue;
			UsecodeTeleport t{UsecodeTeleport::NpcTeleport, classId, v[3]};
			t.x = v[0];
			t.y = v[1];
			t.z = v[2];
			teleports_.append(t);
		}
	}
}
