#include "data/ucode.h"

#include <QObject>

#include <algorithm>
#include <cstring>

namespace Ucode
{

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
			if (error)
				*error = QObject::tr("unknown opcode %1 at %2").arg(op, 2, 16, QChar('0')).arg(at, 0, 16);
			return false;
		}
		out->push_back({at, op, size});
		at += size;
	}
	if (at != code.size())
	{
		if (error)
			*error = QObject::tr("the last instruction runs past the end");
		return false;
	}
	return true;
}

QString eventName(int event)
{
	static const char *const names[] = {"look", "use", "anim", "setActivity", "cachein", "hit", "gotHit",
		"hatch", "schedule", "release", "equip", "unequip", "combine", "", "calledFromAnim",
		"enterFastArea", "leaveFastArea", "cast", "justMoved", "AvatarStoleSomething", "animGetHit",
		"guardianBark"};
	return event >= 0 && event < int(sizeof names / sizeof names[0]) ? QString::fromLatin1(names[event]) : QString();
}

static const Intrinsic intrinsics[] = {
#include "data/intrinsics.inc"
};

int intrinsicCount()
{
	return int(sizeof intrinsics / sizeof intrinsics[0]);
}

const Intrinsic *intrinsic(int number)
{
	return number >= 0 && number < intrinsicCount() ? &intrinsics[number] : nullptr;
}

int intrinsicArgBytes(int number)
{
	const Intrinsic *i = intrinsic(number);
	if (!i)
		return 0;
	int bytes = i->member ? 4 : 0;
	for (const char *a = i->args; *a; a++)
		bytes += (*a == 'p' || *a == 'l') ? 4 : 2;
	return bytes;
}

}
