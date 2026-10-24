// flags: -P -2 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: MLIST.C

#include "u8port.h"

#include "u8process.h"
#include "cfile.h"
#include "kernel.h"
#include "mlist.h"
#include "error.h"

// Builds the list from DEP_END-terminated words. With an owner, the list is
// what it waits on: each pid gets linked both ways.
// Builds the list from the words after owner, up to and including DEP_END.
int16_t List::constructList(uint16_t owner, ...)
{
	uint16_t words[0x400];
	uint16_t n = 0;
	va_list args;

	// The DOS build scanned its own stack for DEP_END, at most 0x400 words.
	va_start(args, owner);
	do {
		if (n == 0x400)
			halt(__FILE__, 89);	// __LINE__
		words[n] = (uint16_t)va_arg(args, int);
	} while (words[n++] != DEP_END);
	va_end(args);
	return constructList(owner, words, n);
}

int16_t List::constructList(uint16_t owner, const uint16_t *args, uint16_t n)
{
	uint16_t i;
	PidEntry entry;
	Process *member = 0;
	Process *ownerProcess = 0;

	if (owner)
		ownerProcess = Kernel::getProcess(owner);
	if (data)
		delete[] data;
	data = new uint16_t[n];
	if (data == 0)
		halt(__FILE__, 110);	// __LINE__
	for (i = 0; i < n; i++) {
		entry = *args++;
		data[i] = entry.value;
		if (owner && entry.isPid()) {
			member = Kernel::getProcess(entry.value);
			member->setActiveDep(owner);
			ownerProcess->setPassiveDep(entry.value);
			if (member->ref == 0)
				member->setRef(ownerProcess->ref);
			else if (ownerProcess->ref == 0)
				ownerProcess->setRef(member->ref);
		}
	}
	count = n;
	// Checks the last member added.
	if (owner)
		member->checkDependencies(0);
	return count;
}

void List::clear(void)
{
	if (data == 0)
		halt(__FILE__, 178);	// __LINE__
	delete data;
	count = 0;
	data = 0;
}

List::~List(void)
{
	if (isAllocated())
		clear();
}

int16_t List::addToList(uint16_t value)
{
	uint16_t n;
	uint16_t i;

	if (isAllocated()) {
		n = count;
		for (i = 0; i < n; i++)
			if (data[i] == value)
				return n;
	}
	return appendToList(value);
}

int16_t List::appendToList(uint16_t value)
{
	uint16_t *newData;
	uint16_t i;
	uint16_t n;

	if (isAllocated()) {
		n = count;
		// A list of linear processes holds one process of its type.
		if (linearType) {
			ProcessType type;
			PidEntry entry(value);
			PidEntry other;
			Process *p;

			if (entry.isPid()) {
				p = Kernel::getProcess(entry.getPid());
				type = p->processType;
				if (linearType == type)
					for (i = 0; i < n; i++) {
						other = data[i];
						if (other.isPid()) {
							Process *q = Kernel::getProcess(other.getPid());
							if (q->processType == linearType)
								halt(__FILE__, 246);	// __LINE__
						}
					}
			}
		}
		newData = new uint16_t[n + 1];
		if (!newData)
			halt(__FILE__, 258);	// __LINE__
		n--;
		for (i = 0; i < n; i++)
			newData[i] = data[i];
		newData[i] = value;
		i++;
		newData[i] = DEP_END;
		delete data;
		data = newData;
		count = n + 2;
		return count;
	}
	return constructList(0, value, DEP_END);
}

// Returns the index the value had.
int16_t List::removeFromList(uint16_t value)
{
	uint16_t n;
	uint16_t remaining;
	uint16_t i;
	uint16_t found;
	uint16_t *newData;
	uint16_t k;

	if (isAllocated()) {
		n = count;
		for (i = 0, remaining = 0; i < n; i++)
			if (getMember(i) != value)
				remaining++;
		if (remaining == 1) {
			clear();
			return 0;
		}
		newData = new uint16_t[remaining];
		if (!newData)
			halt(__FILE__, 322);	// __LINE__
		for (i = 0, k = 0; i < n; i++)
			if (getMember(i) != value)
				newData[k++] = getMember(i);
			else
				found = i;
		delete data;
		data = newData;
		count = remaining;
		return found;
	}
	halt(__FILE__, 345);	// __LINE__
}

uint16_t List::getMember(uint16_t index)
{
	if (count <= index)
		halt(__FILE__, 352);	// __LINE__
	return data[index];
}

void List::setMember(uint16_t index, uint16_t value)
{
	if (count <= index)
		halt(__FILE__, 361);	// __LINE__
	data[index] = value;
}

int16_t List::isInList(uint16_t value)
{
	uint16_t i;

	if (isAllocated())
		for (i = 0; i < count; i++)
			if (getMember(i) == value)
				return i;
	return 0;
}

PidEntry List::isPidInList(uint16_t pid)
{
	uint16_t i;
	PidEntry entry;

	if (isAllocated())
		for (i = 0; i < count; i++) {
			entry = getMember(i);
			if (entry.isPid() && entry.getPid() == pid)
				return entry;
		}
	return PidEntry(0);
}

PidEntry List::isPidInList(uint16_t pid, uint16_t &index)
{
	uint16_t i;
	PidEntry entry;

	if (isAllocated())
		for (i = 0; i < count; i++) {
			entry = getMember(i);
			if (entry.isPid() && entry.getPid() == pid) {
				index = i;
				return entry;
			}
		}
	return PidEntry(0);
}

void List::setListSize(uint16_t size)
{
	if (isAllocated())
		halt(__FILE__, 444);	// __LINE__
	data = new uint16_t[size];
	count = size;
	if (data == 0)
		halt(__FILE__, 450);	// __LINE__
}

void List::load(BaseFile *file)
{
	uint16_t n = count;

	if (isAllocated()) {
		// The saved pointer is stale.
		data = 0;
		setListSize(n);
		file->read((char *)data, n * 2);
	}
}

void List::save(BaseFile *file)
{
	if (isAllocated())
		file->write((char *)data, count * 2);
}
