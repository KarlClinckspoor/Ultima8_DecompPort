// A usecode machine for the editor: runs a class's routines as the game's interpreter does
// (u8/inter.cpp), with its string and list heap (u8/yamm.cpp), against a Host that answers the
// intrinsics. Processes run to their end at once: a spawned routine finishes before its
// spawner goes on, and waits do not wait. What the game does to the world is up to the Host.
#pragma once

#include <QByteArray>
#include <QString>

#include <array>
#include <cstdint>
#include <vector>

class Usecode;

class UcVm
{
public:
	// Answers intrinsics. Returns false to stop the run, saying why in *why.
	class Host
	{
	public:
		virtual ~Host() = default;
		virtual bool intrinsic(UcVm &vm, int number, const char *args, int argBytes, uint32_t *result,
			QString *why) = 0;
	};

	explicit UcVm(const Usecode &usecode);

	// Global flags (0x400 bytes, as FLAG.DAT holds them).
	void setFlags(const QByteArray &flags);
	const QByteArray &flags() const { return flags_; }

	// Runs an item's event routine (spawnUnk: the item's referent as its this, the event's
	// argument) to its end. False if it stopped first: the host stopped it, it used something
	// the machine does not do, or it ran too long; *why says which.
	bool runEvent(uint16_t referent, int classId, int event, uint32_t arg, Host &host, QString *why,
		int maxSteps = 200000);

	// For hosts: what a far pointer (an argument) points at.
	QByteArray stringAt(uint32_t farPtr) const;	// a NUL-ended string
	uint16_t wordAt(uint32_t farPtr) const;		// an Item *'s referent, a short &
	// A string handle (Yamm), for intrinsics that return one (getName, numToStr).
	uint16_t newString(const QByteArray &text);
	QByteArray stringOf(uint16_t handle) const;
	// The elements of a string list handle.
	QList<QByteArray> stringList(uint16_t handle) const;
	static int16_t word(const char *args, int at);
	static int32_t dword(const char *args, int at);

private:
	struct Process;
	// Runs a process until it returns (1) or, with untilSuspend, suspends (2); 0 if it stopped.
	int run(Process &p, Host &host, QString *why, bool untilSuspend);
	char *resolve(uint32_t farPtr, int bytes) const;
	uint32_t farOf(const Process &p, int offset) const;

	// The heap of strings and lists (u8/yamm.cpp): 20000 bytes of blocks, a handle being the
	// offset just past a block's header.
	uint16_t alloc(uint16_t n);
	uint16_t allocString(const char *s);
	void release(uint16_t handle);
	uint16_t u16(int at) const;
	void setU16(int at, uint16_t v);
	// YammList.
	void listKill(uint16_t list, bool isString);
	bool listTraverse(uint16_t list, uint16_t &pos) const;
	uint16_t listCopy(uint16_t list, uint16_t size, bool isString);
	char *listElement(uint16_t list, int index, bool isString);
	uint16_t listAppend(uint16_t list, uint16_t node);
	uint16_t listUnion(uint16_t list, uint16_t other, uint16_t size, bool isString);
	uint16_t listRemove(uint16_t list, uint16_t other, uint16_t size, bool isString);
	bool listContains(uint16_t list, const char *element, uint16_t size, bool isString);
	void listSet(uint16_t list, int index, const char *element, uint16_t size, bool isString);

	const Usecode &usecode_;
	QByteArray flags_;
	mutable QByteArray heap_;
	std::vector<Process *> processes_;	// running, innermost last
	int nextPid_ = 1;
	int steps_ = 0;
	int maxSteps_ = 0;
	bool stopped_ = false;
	QString why_;
};
