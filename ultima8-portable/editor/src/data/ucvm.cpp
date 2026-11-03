#include "data/ucvm.h"

#include <QObject>

#include <algorithm>
#include <cstring>

#include "data/ucode.h"
#include "data/usecode.h"

// Far pointers name a segment in their high word: the heap, a process's stack (by its pid), or
// a class's code (return addresses).
namespace
{

enum : uint32_t { SegHeap = 1, SegStack = 0x100, SegCode = 0x4000 };
enum { HeapSize = 20000, StackSize = 200, Head = 1 };

}

struct UcVm::Process
{
	int pid = 0;
	char buf[StackSize];	// the stack (Data::buffer); offsets are from its start, as bp is
	int sp = StackSize;
	uint16_t bp = StackSize - 1;
	int cls = 0;
	int ip = 0;
	QByteArray code;
	int32_t result = 0;
	int32_t temp = 0;
	bool suspended = false;	// resuming does not push the base return address again
	bool waited = false;	// it suspended at least once, so it was no phantom

	int16_t popWord() { int16_t v; std::memcpy(&v, buf + sp, 2); sp += 2; return v; }
	int32_t popLong() { int32_t v; std::memcpy(&v, buf + sp, 4); sp += 4; return v; }
	void pushWord(int16_t v) { sp -= 2; std::memcpy(buf + sp, &v, 2); }
	void pushLong(int32_t v) { sp -= 4; std::memcpy(buf + sp, &v, 4); }
	int16_t wordAt(int at) const { int16_t v; std::memcpy(&v, buf + at, 2); return v; }
	int32_t longAt(int at) const { int32_t v; std::memcpy(&v, buf + at, 4); return v; }
	void setWordAt(int at, int16_t v) { std::memcpy(buf + at, &v, 2); }
	void setLongAt(int at, int32_t v) { std::memcpy(buf + at, &v, 4); }
	bool fits(int at, int n) const { return at >= 0 && n >= 0 && at + n <= StackSize; }
};

UcVm::UcVm(const Usecode &usecode)
	: usecode_(usecode), flags_(0x400, '\0'), heap_(HeapSize, '\0')
{
	// initYamm: a used head block at 1 and one free block after it.
	const int tail = 9;
	setU16(Head + 0, tail);
	setU16(Head + 2, tail);
	setU16(Head + 4, 0);
	setU16(Head + 6, 0);
	setU16(tail + 0, Head);
	setU16(tail + 2, Head);
	setU16(tail + 4, 0);
	setU16(tail + 6, uint16_t((HeapSize - 16) | 0x8000));
}

void UcVm::setFlags(const QByteArray &flags)
{
	flags_ = flags;
	flags_.resize(0x400, '\0');
}

int16_t UcVm::word(const char *args, int at)
{
	int16_t v;
	std::memcpy(&v, args + at, 2);
	return v;
}

int32_t UcVm::dword(const char *args, int at)
{
	int32_t v;
	std::memcpy(&v, args + at, 4);
	return v;
}

// --- The heap (u8/yamm.cpp) ---

uint16_t UcVm::u16(int at) const
{
	if (at < 0 || at + 2 > heap_.size())
		return 0;
	return uint16_t(uint8_t(heap_[at]) | uint8_t(heap_[at + 1]) << 8);
}

void UcVm::setU16(int at, uint16_t v)
{
	if (at < 0 || at + 2 > heap_.size())
		return;
	heap_[at] = char(v & 0xff);
	heap_[at + 1] = char(v >> 8);
}

uint16_t UcVm::alloc(uint16_t n)
{
	for (uint16_t pos = u16(Head); pos != Head; pos = u16(pos))
	{
		const uint16_t raw = u16(pos + 6);
		const uint16_t size = raw & 0x7fff;
		if (!(raw & 0x8000) || size < n)
			continue;
		const uint16_t extra = size - n;
		uint16_t slack = 0;
		if (extra > 8)
		{
			const uint16_t next = u16(pos);
			const uint16_t split = uint16_t(pos + n + 8);
			setU16(split, next);
			setU16(split + 2, pos);
			setU16(next + 2, split);
			setU16(pos, split);
			setU16(split + 6, uint16_t((extra - 8) | 0x8000));
			setU16(split + 4, 0);
		}
		else
			slack = extra;
		setU16(pos + 6, uint16_t(n + slack));
		setU16(pos + 4, 0);
		std::memset(heap_.data() + pos + 8, 0, n + slack);
		return uint16_t(pos + 8);
	}
	if (!stopped_)
	{
		stopped_ = true;
		why_ = QObject::tr("the usecode ran out of string memory");
	}
	return 0;
}

uint16_t UcVm::allocString(const char *s)
{
	const size_t len = std::strlen(s) + 1;
	const uint16_t h = alloc(uint16_t(len));
	if (h)
		std::memcpy(heap_.data() + h, s, len);
	return h;
}

void UcVm::release(uint16_t handle)
{
	if (handle < 9 || handle >= HeapSize)
		return;
	const uint16_t pos = uint16_t(handle - 8);
	uint16_t prev = u16(pos + 2);
	uint16_t next = u16(pos);
	setU16(pos + 6, u16(pos + 6) | 0x8000);
	setU16(pos + 4, 0);
	if (u16(next + 6) & 0x8000)
	{
		const uint16_t after = u16(next);
		setU16(pos, after);
		setU16(after + 2, pos);
		setU16(pos + 6, uint16_t(u16(pos + 6) + (u16(next + 6) & 0x7fff) + 8));
		next = after;
	}
	if (u16(prev + 6) & 0x8000)
	{
		setU16(prev, u16(pos));
		setU16(next + 2, u16(pos + 2));
		setU16(prev + 6, uint16_t(u16(prev + 6) + (u16(pos + 6) & 0x7fff) + 8));
	}
}

uint16_t UcVm::newString(const QByteArray &text)
{
	return allocString(text.constData());
}

QByteArray UcVm::stringOf(uint16_t handle) const
{
	if (handle == 0 || handle >= HeapSize)
		return QByteArray();
	return QByteArray(heap_.constData() + handle);
}

void UcVm::listKill(uint16_t list, bool isString)
{
	uint16_t pos = list;
	int guard = 0;
	while (pos && guard++ < 10000)
	{
		const uint16_t next = u16(pos);
		if (isString)
			release(u16(pos + 2));
		release(pos);
		pos = next;
	}
}

bool UcVm::listTraverse(uint16_t list, uint16_t &pos) const
{
	pos = pos ? u16(pos) : list;
	return pos != 0;
}

uint16_t UcVm::listCopy(uint16_t list, uint16_t size, bool isString)
{
	uint16_t head = 0, last = 0, pos = 0;
	while (listTraverse(list, pos))
	{
		const uint16_t node = alloc(uint16_t(size + 2));
		if (!node)
			return head;
		setU16(node, 0);
		if (isString)
			setU16(node + 2, allocString(heap_.constData() + u16(pos + 2)));
		else
			std::memcpy(heap_.data() + node + 2, heap_.constData() + pos + 2, size);
		if (last)
			setU16(last, node);
		else
			head = node;
		last = node;
	}
	return head;
}

char *UcVm::listElement(uint16_t list, int index, bool isString)
{
	uint16_t pos = 0;
	int i = 0;
	while (i++ <= index)
		if (!listTraverse(list, pos))
			return nullptr;
	if (isString)
		return heap_.data() + u16(pos + 2);
	return heap_.data() + pos + 2;
}

uint16_t UcVm::listAppend(uint16_t list, uint16_t node)
{
	uint16_t pos = 0, last = 0;
	while (listTraverse(list, pos))
		last = pos;
	if (last)
		setU16(last, node);
	else
		list = node;
	return list;
}

uint16_t UcVm::listUnion(uint16_t list, uint16_t other, uint16_t size, bool isString)
{
	uint16_t pos = 0;
	while (listTraverse(other, pos))
	{
		const char *a = heap_.constData() + pos + 2;
		bool found = false;
		uint16_t mine = 0;
		while (!found && listTraverse(list, mine))
		{
			const char *b = heap_.constData() + mine + 2;
			if (isString)
				found = std::strcmp(heap_.constData() + u16(pos + 2), heap_.constData() + u16(mine + 2)) == 0;
			else
				found = std::memcmp(a, b, size) == 0;
		}
		if (found)
			continue;
		const uint16_t node = alloc(uint16_t(size + 2));
		if (!node)
			return list;
		setU16(node, 0);
		if (isString)
			setU16(node + 2, allocString(heap_.constData() + u16(pos + 2)));
		else
			std::memcpy(heap_.data() + node + 2, heap_.constData() + pos + 2, size);
		list = listAppend(list, node);
	}
	return list;
}

uint16_t UcVm::listRemove(uint16_t list, uint16_t other, uint16_t size, bool isString)
{
	uint16_t pos = 0;
	while (listTraverse(other, pos))
	{
		uint16_t mine = 0, prev = 0;
		bool found = false;
		while (listTraverse(list, mine))
		{
			if (isString)
				found = std::strcmp(heap_.constData() + u16(pos + 2), heap_.constData() + u16(mine + 2)) == 0;
			else
				found = std::memcmp(heap_.constData() + pos + 2, heap_.constData() + mine + 2, size) == 0;
			if (found)
				break;
			prev = mine;
		}
		if (!found)
			continue;
		if (prev)
			setU16(prev, u16(mine));
		else
			list = u16(mine);
		if (isString)
			release(u16(mine + 2));
		release(mine);
	}
	return list;
}

bool UcVm::listContains(uint16_t list, const char *element, uint16_t size, bool isString)
{
	const char *wanted = element;
	if (isString)
	{
		uint16_t h;
		std::memcpy(&h, element, 2);
		wanted = heap_.constData() + h;
	}
	uint16_t pos = 0;
	while (listTraverse(list, pos))
	{
		if (isString && std::strcmp(heap_.constData() + u16(pos + 2), wanted) == 0)
			return true;
		if (std::memcmp(heap_.constData() + pos + 2, wanted, size) == 0)
			return true;
	}
	return false;
}

void UcVm::listSet(uint16_t list, int index, const char *element, uint16_t size, bool isString)
{
	uint16_t pos = 0;
	int i = 0;
	while (i++ < index)
		if (!listTraverse(list, pos))
			return;
	if (isString)
		release(u16(pos + 2));
	std::memcpy(heap_.data() + pos + 2, element, size);
}

QList<QByteArray> UcVm::stringList(uint16_t handle) const
{
	QList<QByteArray> out;
	uint16_t pos = 0;
	int guard = 0;
	while (listTraverse(handle, pos) && guard++ < 1000)
		out << QByteArray(heap_.constData() + u16(pos + 2));
	return out;
}

// --- Pointers ---

char *UcVm::resolve(uint32_t farPtr, int bytes) const
{
	const uint32_t seg = farPtr >> 16;
	const int off = int(farPtr & 0xffff);
	if (seg == SegHeap)
		return off + bytes <= HeapSize ? const_cast<char *>(heap_.constData()) + off : nullptr;
	if (seg >= SegStack && seg < SegCode)
		for (Process *p : processes_)
			if (uint32_t(SegStack + p->pid) == seg)
				return p->fits(off, bytes) ? p->buf + off : nullptr;
	return nullptr;
}

uint32_t UcVm::farOf(const Process &p, int offset) const
{
	return uint32_t(SegStack + p.pid) << 16 | uint32_t(offset & 0xffff);
}

QByteArray UcVm::stringAt(uint32_t farPtr) const
{
	const char *s = resolve(farPtr, 1);
	if (!s)
		return QByteArray();
	const uint32_t seg = farPtr >> 16;
	const int room = seg == SegHeap ? HeapSize - int(farPtr & 0xffff) : StackSize - int(farPtr & 0xffff);
	return QByteArray(s, int(strnlen(s, size_t(std::max(room, 0)))));
}

uint16_t UcVm::wordAt(uint32_t farPtr) const
{
	const char *p = resolve(farPtr, 2);
	if (!p)
		return 0;
	uint16_t v;
	std::memcpy(&v, p, 2);
	return v;
}

// --- Running ---

namespace
{

// Keeps a process findable by its pid (for far pointers into its stack) while it lives.
template<class P> struct Live
{
	std::vector<P *> &list;
	P *process;
	Live(std::vector<P *> &l, P *p) : list(l), process(p) { list.push_back(p); }
	~Live() { list.erase(std::find(list.begin(), list.end(), process)); }
};

}

bool UcVm::runEvent(uint16_t referent, int classId, int event, uint32_t arg, Host &host, QString *why, int maxSteps)
{
	const int entry = usecode_.eventOffset(classId, event);
	if (!entry)
	{
		if (why)
			*why = QObject::tr("class %1 has no %2 event").arg(usecode_.className(classId), Ucode::eventName(event));
		return false;
	}
	stopped_ = false;
	why_.clear();
	steps_ = 0;
	maxSteps_ = maxSteps;
	// UnkProcess: the this data (the referent), the argument, a pointer back to the this data.
	Process p;
	p.pid = nextPid_++;
	p.cls = classId;
	p.ip = entry;
	p.code = usecode_.code(classId);
	p.sp -= 2;
	p.setWordAt(p.sp, int16_t(referent));
	const int thisAt = p.sp;
	p.sp -= 4;
	p.setLongAt(p.sp, int32_t(arg));
	p.sp -= 4;
	p.setLongAt(p.sp, int32_t(farOf(p, thisAt)));
	Live<Process> live(processes_, &p);
	return run(p, host, why, false) == 1;
}

int UcVm::run(Process &p, Host &host, QString *why, bool untilSuspend)
{
	auto fail = [&](const QString &reason) {
		if (!stopped_)
		{
			stopped_ = true;
			why_ = reason;
		}
		if (why)
			*why = why_;
		return 0;
	};
	auto code = [&](int at) -> uint8_t { return at >= 0 && at < p.code.size() ? uint8_t(p.code[at]) : 0; };
	auto s8 = [&](int at) -> int16_t { return int16_t(int8_t(code(at))); };
	auto w16 = [&](int at) -> int16_t { return int16_t(code(at) | code(at + 1) << 8); };
	auto l32 = [&](int at) -> int32_t {
		return int32_t(uint32_t(code(at)) | uint32_t(code(at + 1)) << 8 | uint32_t(code(at + 2)) << 16 | uint32_t(code(at + 3)) << 24);
	};
	auto local = [&](int16_t off) { return int(p.bp) + off; };
	auto checkStack = [&](int need) { return p.sp - need >= 0 && p.sp <= StackSize; };
	auto heapPtr = [&](uint16_t h) -> char * { return heap_.data() + (h < HeapSize ? h : 0); };

	if (!p.suspended)
		p.pushLong(0);	// the base return address: returning to it ends the process
	p.suspended = false;
	int32_t &temp = p.temp;
	for (;;)
	{
		if (stopped_)
			return fail(why_);
		if (++steps_ > maxSteps_)
			return fail(QObject::tr("it ran for more than %1 steps").arg(maxSteps_));
		if (p.ip < 0 || p.ip >= p.code.size())
			return fail(QObject::tr("it ran off the end of %1").arg(usecode_.className(p.cls)));
		if (!checkStack(8))
			return fail(QObject::tr("its stack overflowed"));
		const int at = p.ip;
		const uint8_t op = code(at);
		int16_t w1, w2, w3, w4, w5;
		int32_t l1, l2;
		p.ip++;
		switch (op)
		{
		case 0x5a:	// allocate locals
		{
			const int size = s8(p.ip++);
			const uint16_t old = p.bp;
			p.pushWord(int16_t(old));
			p.bp = uint16_t(p.sp);
			p.sp -= size;
			if (!p.fits(p.sp, size))
				return fail(QObject::tr("its stack overflowed"));
			std::memset(p.buf + p.sp, 0, size_t(size));
			break;
		}
		case 0x5b: p.ip += 2; break;	// debug line
		case 0x5c: p.ip += 11; break;	// debug unit
		case 0x00:	// store local byte
			w1 = s8(p.ip++);
			p.buf[local(w1)] = char(p.popWord());
			break;
		case 0x01: w1 = s8(p.ip++); p.setWordAt(local(w1), p.popWord()); break;
		case 0x02: w1 = s8(p.ip++); p.setLongAt(local(w1), p.popLong()); break;
		case 0x03:	// store local bytes
			w1 = s8(p.ip++);
			w2 = s8(p.ip++);
			if (!p.fits(local(w1), w2))
				return fail(QObject::tr("a store went outside the stack"));
			std::memmove(p.buf + local(w1), p.buf + p.sp, size_t(w2));
			break;
		case 0x08: p.result = p.popLong(); break;
		case 0x4d:	// store pointer
		{
			w1 = s8(p.ip++);
			char *dest = resolve(uint32_t(p.popLong()), w1);
			if (!dest)
				return fail(QObject::tr("it wrote through a pointer the editor does not follow"));
			std::memmove(dest, p.buf + p.sp, size_t(w1));
			p.sp += w1;
			break;
		}
		case 0x74: p.sp -= 1; p.buf[p.sp] = char(code(p.ip++)); break;	// search token
		case 0x0a: p.pushWord(s8(p.ip++)); break;
		case 0x0b: p.pushWord(w16(p.ip)); p.ip += 2; break;
		case 0x0c: p.pushLong(l32(p.ip)); p.ip += 4; break;
		case 0x3e: w1 = s8(p.ip++); p.pushWord(int16_t(p.buf[local(w1)])); break;
		case 0x3f: w1 = s8(p.ip++); p.pushWord(p.wordAt(local(w1))); break;
		case 0x40: w1 = s8(p.ip++); p.pushLong(p.longAt(local(w1))); break;
		case 0x45:	// load local bytes
		{
			w1 = s8(p.ip++);
			w2 = s8(p.ip++);
			const int from = p.sp;
			p.sp -= w2;
			std::memmove(p.buf + from - w2, p.buf + local(w1), size_t(w2));
			break;
		}
		case 0x6d: p.pushLong(p.result); break;
		case 0x4c:	// load pointer
		{
			w1 = s8(p.ip++);
			const char *src = resolve(uint32_t(p.popLong()), w1);
			if (!src)
				return fail(QObject::tr("it read through a pointer the editor does not follow"));
			const int from = p.sp;
			p.sp -= w1;
			std::memmove(p.buf + from - w1, src, size_t(w1));
			break;
		}
		case 0x4e:	// load global
		{
			const int bit = uint16_t(w16(p.ip));
			const int width = code(p.ip + 2);
			p.ip += 3;
			uint32_t word = 0;
			if (bit / 8 + 4 <= flags_.size())
				std::memcpy(&word, flags_.constData() + bit / 8, 4);
			const uint32_t mask = ((1u << width) - 1) << (bit & 7);
			p.pushWord(int16_t((word & mask) >> (bit & 7)));
			break;
		}
		case 0x4f:	// store global
		{
			const int bit = uint16_t(w16(p.ip));
			const int width = code(p.ip + 2);
			p.ip += 3;
			const int16_t v = p.popWord();
			if (bit / 8 + 4 <= flags_.size())
			{
				uint32_t word;
				std::memcpy(&word, flags_.constData() + bit / 8, 4);
				const uint32_t mask = ((1u << width) - 1) << (bit & 7);
				word = (word & ~mask) | (uint32_t(int32_t(v)) << (bit & 7));
				std::memcpy(flags_.data() + bit / 8, &word, 4);
			}
			break;
		}
		case 0x0e:	// make list
		{
			w1 = s8(p.ip++);
			w2 = s8(p.ip++);
			w4 = 0;
			w5 = 0;
			for (w3 = 0; w3 < w2; w3++)
			{
				w4 = int16_t(alloc(uint16_t(w1 + 2)));
				if (!w4)
					return fail(why_);
				std::memcpy(heapPtr(w4) + 2, p.buf + p.sp + w3 * w1, size_t(w1));
				setU16(w4, uint16_t(w5));
				w5 = w4;
			}
			p.sp += w1 * w2;
			p.pushWord(w4);
			break;
		}
		case 0x42:	// load local list
			w1 = s8(p.ip++);
			w2 = s8(p.ip++);
			p.pushWord(int16_t(listCopy(uint16_t(p.wordAt(local(w1))), uint16_t(w2), false)));
			break;
		case 0x64: w1 = s8(p.ip++); listKill(uint16_t(p.wordAt(local(w1))), false); break;
		case 0x44:	// load list element
		{
			w1 = p.popWord();
			w2 = p.popWord();
			w3 = s8(p.ip++);
			w4 = s8(p.ip++);
			if (w3 == 1)
				p.pushWord(0);
			else
				p.sp -= w3;
			const char *e = listElement(uint16_t(w2), w1 - 1, w4 & 1);
			if (e)
			{
				if (w4 & 1)
					p.setWordAt(p.sp, int16_t(allocString(e)));
				else
					std::memcpy(p.buf + p.sp, e, size_t(w3));
			}
			else
				std::memset(p.buf + p.sp, 0, size_t(w3));
			if (w4 & 2)
				listKill(uint16_t(w2), w4 & 1);
			break;
		}
		case 0x17:	// concat list
			w2 = p.popWord();
			w1 = p.popWord();
			p.pushWord(int16_t(listAppend(uint16_t(w1), uint16_t(w2))));
			break;
		case 0x66: w1 = s8(p.ip++); listKill(uint16_t(p.wordAt(p.sp + w1)), false); break;
		case 0x38:	// list contains
		{
			w1 = s8(p.ip++);
			w4 = s8(p.ip++);
			w2 = p.popWord();
			const bool in = listContains(uint16_t(w2), p.buf + p.sp, uint16_t(w1), w4);
			listKill(uint16_t(w2), w4);
			if (w4)
				release(uint16_t(p.wordAt(p.sp)));
			p.sp += w1;
			p.pushWord(in);
			break;
		}
		case 0x09:	// store list element
			w1 = p.popWord();
			w2 = s8(p.ip++);
			w3 = s8(p.ip++);
			w4 = s8(p.ip++);
			listSet(uint16_t(p.wordAt(local(w2))), w1, p.buf + p.sp, uint16_t(w3), w4);
			break;
		case 0x43: w1 = s8(p.ip++); p.pushWord(int16_t(listCopy(uint16_t(p.wordAt(local(w1))), 2, true))); break;
		case 0x63: w1 = s8(p.ip++); listKill(uint16_t(p.wordAt(local(w1))), true); break;
		case 0x18:	// xor list
		case 0x19:	// xor string list
		case 0x1a:	// subtract string list
		case 0x1b:	// subtract list
		{
			w1 = s8(p.ip++);
			w3 = p.popWord();
			w2 = p.popWord();
			const bool str = op == 0x19 || op == 0x1a;
			const uint16_t left = (op == 0x1a || op == 0x1b) ? listRemove(uint16_t(w2), uint16_t(w3), uint16_t(w1), str)
				: listUnion(uint16_t(w2), uint16_t(w3), uint16_t(w1), str);
			p.pushWord(int16_t(left));
			listKill(uint16_t(w3), str);
			break;
		}
		case 0x67: w1 = s8(p.ip++); listKill(uint16_t(p.wordAt(p.sp + w1)), true); break;
		case 0x0d:	// push string
		{
			const int length = uint16_t(w16(p.ip));
			p.ip += 2;
			const uint16_t h = alloc(uint16_t(length + 1));
			if (!h)
				return fail(why_);
			std::memcpy(heapPtr(h), p.code.constData() + p.ip, size_t(length));
			heapPtr(h)[length] = 0;
			p.pushWord(int16_t(h));
			p.ip += length + 1;
			break;
		}
		case 0x41:	// load local string
			w1 = s8(p.ip++);
			p.pushWord(int16_t(allocString(heapPtr(uint16_t(p.wordAt(local(w1)))))));
			break;
		case 0x68:	// duplicate string
		{
			const QByteArray s(heapPtr(uint16_t(p.popWord())));
			p.pushWord(int16_t(allocString(s.constData())));
			break;
		}
		case 0x16:	// concat string
		case 0x26:	// equal string
		{
			w2 = p.popWord();
			w1 = p.popWord();
			const QByteArray a(heapPtr(uint16_t(w1))), b(heapPtr(uint16_t(w2)));
			int16_t r;
			if (op == 0x26)
				r = a == b;
			else
				r = int16_t(allocString((a + b).constData()));
			release(uint16_t(w2));
			release(uint16_t(w1));
			p.pushWord(r);
			break;
		}
		case 0x65: w1 = s8(p.ip++); release(uint16_t(p.wordAt(p.sp + w1))); break;
		case 0x62: w1 = s8(p.ip++); release(uint16_t(p.wordAt(local(w1)))); break;
		case 0x6a:	// pointer to string
		{
			const QByteArray s = stringAt(uint32_t(p.popLong()));
			p.pushWord(int16_t(allocString(s.constData())));
			break;
		}
		case 0x6b: p.pushLong(int32_t(SegHeap << 16 | uint16_t(p.popWord()))); break;
		case 0x69: w1 = s8(p.ip++); p.pushLong(int32_t(SegHeap << 16 | uint16_t(p.wordAt(local(w1))))); break;
		case 0x14: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(int16_t(a + b)); break; }
		case 0x15: { int32_t a = p.popLong(), b = p.popLong(); p.pushLong(int32_t(uint32_t(a) + uint32_t(b))); break; }
		case 0x1c: w2 = p.popWord(); w1 = p.popWord(); p.pushWord(int16_t(w1 - w2)); break;
		case 0x1d: l2 = p.popLong(); l1 = p.popLong(); p.pushLong(int32_t(uint32_t(l1) - uint32_t(l2))); break;
		case 0x1e: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(int16_t(a * b)); break; }
		case 0x1f: { int32_t a = p.popLong(), b = p.popLong(); p.pushLong(int32_t(uint32_t(a) * uint32_t(b))); break; }
		case 0x20: w2 = p.popWord(); w1 = p.popWord(); p.pushWord(w2 ? int16_t(w1 / w2) : int16_t(0)); break;
		case 0x21: l2 = p.popLong(); l1 = p.popLong(); p.pushLong(l2 ? l1 / l2 : 0); break;
		case 0x22: w2 = p.popWord(); w1 = p.popWord(); p.pushWord(w2 ? int16_t(w1 % w2) : int16_t(0)); break;
		case 0x23: l2 = p.popLong(); l1 = p.popLong(); p.pushLong(l2 ? l1 % l2 : 0); break;
		case 0x24: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(a == b); break; }
		case 0x25: { int32_t a = p.popLong(), b = p.popLong(); p.pushWord(a == b); break; }
		case 0x27: w1 = s8(p.ip++); p.pushWord(std::memcmp(p.buf + p.sp, p.buf + p.sp + w1, size_t(w1)) == 0); break;
		case 0x36: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(a != b); break; }
		case 0x37: { int32_t a = p.popLong(), b = p.popLong(); p.pushWord(a != b); break; }
		// Comparisons: the operands come off the stack in reverse.
		case 0x28: { int16_t f = p.popWord(), s = p.popWord(); p.pushWord(f > s); break; }
		case 0x29: { int32_t f = p.popLong(), s = p.popLong(); p.pushWord(f > s); break; }
		case 0x2a: { int16_t f = p.popWord(), s = p.popWord(); p.pushWord(f >= s); break; }
		case 0x2b: { int32_t f = p.popLong(), s = p.popLong(); p.pushWord(f >= s); break; }
		case 0x2c: { int16_t f = p.popWord(), s = p.popWord(); p.pushWord(f < s); break; }
		case 0x2d: { int32_t f = p.popLong(), s = p.popLong(); p.pushWord(f < s); break; }
		case 0x2e: { int16_t f = p.popWord(), s = p.popWord(); p.pushWord(f <= s); break; }
		case 0x2f: { int32_t f = p.popLong(), s = p.popLong(); p.pushWord(f <= s); break; }
		case 0x32: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(a && b); break; }
		case 0x33: { int32_t a = p.popLong(), b = p.popLong(); p.pushWord(a && b); break; }
		case 0x34: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(a || b); break; }
		case 0x35: { int32_t a = p.popLong(), b = p.popLong(); p.pushWord(a || b); break; }
		case 0x30: p.pushWord(!p.popWord()); break;
		case 0x31: p.pushWord(!p.popLong()); break;
		case 0x39: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(int16_t(a & b)); break; }
		case 0x3a: { int16_t a = p.popWord(), b = p.popWord(); p.pushWord(int16_t(a | b)); break; }
		case 0x3c: { int16_t f = p.popWord(), s = p.popWord(); p.pushWord(int16_t(f << s)); break; }
		case 0x3d: { int16_t f = p.popWord(), s = p.popWord(); p.pushWord(int16_t(f >> s)); break; }
		case 0x3b: p.pushWord(int16_t(~p.popWord())); break;
		case 0x57:	// spawn: run the routine to its end now
		{
			const uint32_t thisPtr = uint32_t(p.popLong());
			const int argBytes = s8(p.ip++);
			const int thisSize = s8(p.ip++);
			const int unit = uint16_t(w16(p.ip));
			const int entry = uint16_t(w16(p.ip + 2));
			p.ip += 4;
			if (!usecode_.hasClass(unit))
				return fail(QObject::tr("it spawns class %1, which has no code").arg(unit));
			Process child;
			child.pid = nextPid_++;
			child.cls = unit;
			child.ip = entry;
			child.code = usecode_.code(unit);
			const char *thisData = thisSize ? resolve(thisPtr, thisSize) : nullptr;
			if (thisSize && !thisData)
				return fail(QObject::tr("it spawns a routine on something the editor does not follow"));
			child.sp -= thisSize;
			if (thisSize)
				std::memcpy(child.buf + child.sp, thisData, size_t(thisSize));
			const int childThis = child.sp;
			child.sp -= argBytes;
			std::memcpy(child.buf + child.sp, p.buf + p.sp, size_t(argBytes));
			if (thisSize)
			{
				child.sp -= 4;
				child.setLongAt(child.sp, int32_t(farOf(child, childThis)));
			}
			Live<Process> live(processes_, &child);
			const int status = run(child, host, why, false);
			if (!status)
				return 0;
			// It has run to its end: one that never waited was a phantom, whose result goes
			// straight back; otherwise its pid, and its result once waited for.
			temp = child.waited ? child.pid : 0;
			p.result = child.result;
			break;
		}
		case 0x58:	// spawn inline: the code after this, as a process of its own
		{
			const int unit = uint16_t(w16(p.ip));
			const int base = uint16_t(w16(p.ip + 2));
			const int delta = w16(p.ip + 4);
			const int thisSize = s8(p.ip + 6);
			const int argBytes = s8(p.ip + 7);
			p.ip += 8;
			Process child;
			child.pid = nextPid_++;
			child.cls = unit;
			child.ip = base + delta;
			child.code = usecode_.code(unit);
			const char *thisData = resolve(uint32_t(p.longAt(p.bp + 6)), thisSize);
			child.sp -= thisSize;
			if (thisSize && thisData)
				std::memcpy(child.buf + child.sp, thisData, size_t(thisSize));
			const int childThis = child.sp;
			child.sp -= argBytes;
			if (p.fits(p.bp + 10, argBytes))
				std::memcpy(child.buf + child.sp, p.buf + p.bp + 10, size_t(argBytes));
			if (thisSize)
			{
				child.sp -= 4;
				child.setLongAt(child.sp, int32_t(farOf(child, childThis)));
			}
			Live<Process> live(processes_, &child);
			// It runs until it first suspends (its frame made), then gets a copy of the
			// spawner's locals under its own frame, and here runs on to its end.
			int status = run(child, host, why, true);
			if (!status)
				return 0;
			int16_t pid = 0;
			if (status == 2)
			{
				const int locals = int(p.bp) - p.sp;
				if (locals < 0 || child.sp - locals < 0)
					return fail(QObject::tr("its stack overflowed"));
				child.sp -= locals;
				std::memcpy(child.buf + child.sp, p.buf + p.sp, size_t(locals));
				if (!run(child, host, why, false))
					return 0;
				pid = int16_t(child.pid);
			}
			p.pushWord(pid);
			break;
		}
		case 0x54:	// process dependency: the waits are over already
		{
			const int count = s8(p.ip++);
			p.ip++;
			QVector<int16_t> pids;
			for (int k = 0; k < count; k++)
				pids << p.popWord();
			p.popWord();
			for (int16_t pid : pids)
				if (!(pid & 0x400))
					p.pushWord(pid);
			break;
		}
		case 0x55: p.pushWord(0x401); break;	// and implies (DEP_AND)
		case 0x56: p.pushWord(0x402); break;	// or implies (DEP_OR)
		case 0x53:	// suspend: nothing here waits, so it goes straight on
			p.waited = true;
			if (untilSuspend)
			{
				p.suspended = true;
				return 2;
			}
			break;
		case 0x77: p.popWord(); p.popWord(); break;	// set process type and ref
		case 0x78: break;	// exclude process: no other runs
		case 0x59: p.pushWord(int16_t(p.pid)); break;
		case 0x6c: p.ip += 2; break;	// transfer ownership of heap blocks
		case 0x0f:	// call intrinsic
		{
			const int argBytes = code(p.ip);
			const int number = uint16_t(w16(p.ip + 1));
			p.ip += 3;
			if (!p.fits(p.sp, argBytes))
				return fail(QObject::tr("its stack overflowed"));
			uint32_t result = 0;
			QString reason;
			if (!host.intrinsic(*this, number, p.buf + p.sp, argBytes, &result, &reason))
				return fail(reason);
			temp = int32_t(result);
			break;
		}
		case 0x10:	// call near
		case 0x11:	// call far
		{
			l1 = l32(p.ip);
			p.ip += 4;
			p.pushLong(int32_t((SegCode + uint32_t(p.cls)) << 16 | uint32_t(p.ip)));
			if (op == 0x11)
			{
				const int unit = uint16_t(l1 & 0xffff);
				if (!usecode_.hasClass(unit))
					return fail(QObject::tr("it calls class %1, which has no code").arg(unit));
				p.cls = unit;
				p.code = usecode_.code(unit);
				p.ip = uint16_t(uint32_t(l1) >> 16);
			}
			else
				p.ip += int16_t(l1);
			break;
		}
		case 0x5d: p.pushWord(int16_t(temp & 0xff)); break;
		case 0x5e: p.pushWord(int16_t(temp)); break;
		case 0x5f: p.pushLong(temp); break;
		case 0x6e: w1 = s8(p.ip++); p.sp -= w1; break;
		case 0x50:	// return
		{
			p.sp = p.bp;
			p.bp = uint16_t(p.popWord());
			l1 = p.popLong();
			if (l1 == 0)
				return 1;
			const uint32_t ret = uint32_t(l1);
			const int cls = int((ret >> 16) - SegCode);
			if (cls != p.cls)
			{
				p.cls = cls;
				p.code = usecode_.code(cls);
			}
			p.ip = int(ret & 0xffff);
			break;
		}
		case 0x70:	// search start
		case 0x73:	// search next
			return fail(QObject::tr("it searches the world for items"));
		case 0x75:	// iterate list
		case 0x76:	// iterate string list
		{
			w2 = s8(p.ip++);
			w3 = s8(p.ip++);
			w4 = w16(p.ip);
			p.ip += 2;
			w1 = p.wordAt(p.sp);
			if (w1 == -1)
				w1 = p.wordAt(p.sp + 2);
			if (w1 == 0)
			{
				p.ip += w4;
				listKill(uint16_t(p.wordAt(p.sp + 2)), op == 0x76);
				p.popLong();
			}
			else
			{
				p.setWordAt(p.sp, int16_t(u16(uint16_t(w1))));
				if (op == 0x76)
				{
					release(uint16_t(p.wordAt(local(w2))));
					p.setWordAt(local(w2), int16_t(allocString(heapPtr(u16(uint16_t(w1) + 2)))));
				}
				else
					std::memcpy(p.buf + local(w2), heapPtr(uint16_t(w1)) + 2, size_t(w3));
			}
			break;
		}
		case 0x51:	// branch if false
			l1 = p.popWord();
			w1 = w16(p.ip);
			p.ip += 2;
			if (l1 == 0)
				p.ip = (p.ip + w1) & 0xffff;
			break;
		case 0x52: w1 = w16(p.ip); p.ip = (p.ip + 2 + w1) & 0xffff; break;
		case 0x60: p.pushLong(p.popWord()); break;
		case 0x61: p.pushWord(int16_t(p.popLong())); break;
		case 0x6f: w1 = s8(p.ip++); p.pushLong(int32_t(farOf(p, p.sp - w1))); break;
		case 0x4b: w1 = s8(p.ip++); p.pushLong(int32_t(farOf(p, local(w1)))); break;
		case 0x12: temp = p.popWord(); break;
		case 0x13: temp = p.popLong(); break;
		default:
			return fail(QObject::tr("opcode %1 at %2:%3 is not one the editor runs")
				.arg(op, 2, 16, QChar('0')).arg(usecode_.className(p.cls)).arg(at, 0, 16));
		}
	}
}
