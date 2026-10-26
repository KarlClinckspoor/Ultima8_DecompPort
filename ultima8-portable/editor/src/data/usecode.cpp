#include "data/usecode.h"

#include <functional>

bool Usecode::load(const QString &path, QString *error)
{
	names_.clear();
	teleports_.clear();
	if (!flex_.load(path, error))
		return false;
	const char *n = flex_.data(1);
	int size = flex_.size(1);
	for (int at = 4; n && at + 13 <= size; at += 13)
	{
		QByteArray name(n + at, 13);
		int nul = name.indexOf('\0');
		names_.append(QString::fromLatin1(nul >= 0 ? name.left(nul) : name));
	}
	for (int c = 0; c + 2 < flex_.count(); c++)
		if (flex_.size(c + 2) > 0)
			scan(c, flex_.data(c + 2), flex_.size(c + 2));
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
