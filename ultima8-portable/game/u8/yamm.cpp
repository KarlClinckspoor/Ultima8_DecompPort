// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: YAMM.C

#include "u8port.h"

// #include <conio.h>  (DOS only)
#include <string.h>
#include "cfile.h"
#include "error.h"
#include "yamm.h"

// Inline in YAMM.H when this file was built.
inline void *Yamm::resolve(uint16_t pos) { return buffer + pos; }
inline void Yamm::setPid(uint16_t pos, uint16_t pid) { ((YammBlock *)resolve(pos) - 1)->pid = pid; }
inline YammList::YammList(uint16_t l, uint16_t s, int16_t str) { list = l; size = s; isString = str; }
inline uint8_t YammBlock::isFree(void) { return (size & 0x8000) ? 1 : 0; }
inline uint16_t YammBlock::getSize(void) { return size & 0x7fff; }
inline void YammBlock::setFree(void) { size |= 0x8000; pid = 0; }
inline void YammBlock::setUsed(void) { size &= 0x7fff; }
inline void YammBlock::grow(uint16_t n) { size += n; }
inline void YammBlock::setSize(uint16_t n) { size = n; }

uint16_t Yamm::numBlocks;
char *Yamm::buffer;
YammBlock *Yamm::head;
uint16_t Yamm::currentUnkPid;

int16_t initYamm(void)
{
	Yamm::currentUnkPid = 0;
	Yamm::buffer = new char[YAMM_SIZE];
	if (Yamm::buffer == 0)
		outOfMemory(__FILE__, 41);	// __LINE__
	memset(Yamm::buffer, 0, YAMM_SIZE);
	uint16_t tail = 9;
	YammBlock *last = (YammBlock *)Yamm::resolve(tail);
	Yamm::head = (YammBlock *)Yamm::resolve(1);
	Yamm::head->prev = Yamm::head->next = tail;
	last->prev = last->next = 1;
	Yamm::head->size = 0;
	Yamm::head->setUsed();
	Yamm::head->pid = 0;
	last->size = YAMM_SIZE - 16;
	last->setFree();
	last->pid = 0;
	Yamm::numBlocks = 2;
}

void uninitYamm(void)
{
	if (Yamm::buffer) {
		delete Yamm::buffer;
		Yamm::buffer = 0;
	}
}

// Allocates a copy of the string; returns its handle.
int16_t Yamm::flimFlam(char *s)
{
	uint16_t len = strlen(s) + 1;
	int16_t pos = flimFlam(len);
	if (pos)
		strcpy((char *)resolve(pos), s);
	return pos;
}

// First-fit allocation; returns the handle.
int16_t Yamm::flimFlam(uint16_t n)
{
	uint16_t pos;
	YammBlock *block;
	for (pos = head->next; pos != 1; pos = block->next) {
		block = (YammBlock *)resolve(pos);
		uint16_t size = block->getSize();
		if (block->isFree() && size >= n) {
			uint16_t extra = size - n;
			uint16_t slack = 0;
			if (extra > 8) {
				YammBlock *next = (YammBlock *)resolve(block->next);
				uint16_t splitPos = pos + n + 8;
				YammBlock *split = (YammBlock *)resolve(splitPos);
				split->next = block->next;
				split->prev = pos;
				next->prev = block->next = splitPos;
				split->setSize(extra - 8);
				split->setFree();
				numBlocks++;
			} else
				slack = extra;
			block->setSize(n + slack);
			block->pid = currentUnkPid;
			block->setUsed();
			return pos + 8;
		}
	}
	halt(__FILE__, 131);	// __LINE__
	return 0;
}

// Frees every block owned by the process.
void Yamm::clean(uint16_t pid)
{
	uint16_t pos;
	YammBlock *block;
	for (pos = head->next; pos != 1; pos = block->next) {
		block = (YammBlock *)resolve(pos);
		if (block->pid == pid && !block->isFree())
			scram(pos + 8);
	}
}

// Frees a handle, merging it with free neighbours.
void Yamm::scram(uint16_t pos)
{
	if (pos == 0)
		return;
	pos -= 8;
	YammBlock *block = (YammBlock *)resolve(pos);
	YammBlock *prev = (YammBlock *)resolve(block->prev);
	YammBlock *next = (YammBlock *)resolve(block->next);
	block->setFree();
	if (next->isFree()) {
		block->next = next->next;
		YammBlock *after = (YammBlock *)resolve(next->next);
		after->prev = pos;
		block->grow(next->getSize() + 8);
		next = after;
		numBlocks--;
	}
	if (prev->isFree()) {
		prev->next = block->next;
		next->prev = block->prev;
		prev->grow(block->getSize() + 8);
		numBlocks--;
	}
}

void Yamm::heyMan(void)
{
	trace(TRACE_ALWAYS, "pos  next prev free size pid\r\n");
	uint16_t pos = 1;
	for (uint16_t i = 0; i < numBlocks; i++) {
		YammBlock *block = (YammBlock *)resolve(pos);
		trace(TRACE_ALWAYS, "%04X %04X %04X %04X %04X %04X\r\n", pos, block->next, block->prev,
			block->isFree(), block->getSize(), block->pid);
		pos = block->next;
	}
}

void Yamm::damn(uint16_t where, char *file, uint16_t line)
{
	halt(file, line, "Yamm damn @%d", where);
}

// Checks that both chains close and the blocks cover the heap.
void Yamm::shazamm(char *file, uint16_t line)
{
	YammBlock *forward = head;
	YammBlock *back = head;
	uint32_t total = 0;
	for (uint16_t i = 0; i < numBlocks; i++) {
		forward = (YammBlock *)resolve(forward->next);
		back = (YammBlock *)resolve(back->prev);
		total += forward->getSize() + forward->getSize() + 16;
	}
	if (forward != back || forward != head)
		damn(229, file, line);	// __LINE__
	if (total / 2 != YAMM_SIZE)
		damn(233, file, line);	// __LINE__
}

void Yamm::load(BaseFile &file)
{
	file.read((char *)&numBlocks, 2);
	if (buffer)
		delete buffer;
	buffer = new char[YAMM_SIZE];
	if (buffer == 0)
		outOfMemory(__FILE__, 247);	// __LINE__
	file.read(buffer, YAMM_SIZE);
	head = (YammBlock *)resolve(8);
}

void Yamm::save(BaseFile &file)
{
	file.write((char *)&numBlocks, 2);
	file.write(buffer, YAMM_SIZE);
}

void YammList::kill(void)
{
	uint16_t pos = list;
	while (pos) {
		YammNode *node = (YammNode *)Yamm::resolve(pos);
		uint16_t next = node->next;
		if (isString)
			Yamm::scram(node->data);
		Yamm::scram(pos);
		pos = next;
	}
}

// Steps pos to the next node; 0 starts at the head.
uint8_t YammList::traverse(uint16_t &pos)
{
	if (pos)
		pos = ((YammNode *)Yamm::resolve(pos))->next;
	else
		pos = list;
	return pos == 0 ? 0 : 1;
}

uint16_t YammList::operator=(YammList &other)
{
	kill();
	uint16_t pos = 0;
	YammNode *last = 0;
	size = other.size;
	isString = other.isString;
	while (other.traverse(pos)) {
		uint16_t copy = Yamm::flimFlam(size + 2);
		YammNode *node = (YammNode *)Yamm::resolve(copy);
		YammNode *from = (YammNode *)Yamm::resolve(pos);
		node->next = 0;
		if (isString) {
			char *s = (char *)Yamm::resolve(from->data);
			uint16_t len = strlen(s) + 1;
			uint16_t str = Yamm::flimFlam(len);
			if (str == 0)
				halt(__FILE__, 306);	// __LINE__
			memcpy(Yamm::resolve(str), s, len);
			node->data = str;
		} else
			memcpy(&node->data, &from->data, size);
		if (last)
			last->next = copy;
		else
			list = copy;
		last = node;
	}
	return list;
}

void *YammList::operator[](int16_t index)
{
	uint16_t pos = 0;
	int16_t i = 0;
	while (i++ <= index)
		if (!traverse(pos))
			return 0;
	if (isString)
		return Yamm::resolve(*(uint16_t *)Yamm::resolve(pos + 2));
	return Yamm::resolve(pos + 2);
}

char *YammList::resolveString(uint16_t pos)
{
	return (char *)Yamm::resolve(*(uint16_t *)Yamm::resolve(pos + 2));
}

// Appends a node.
void YammList::operator+=(uint16_t node)
{
	uint16_t pos = 0;
	uint16_t last = 0;
	while (traverse(pos))
		last = pos;
	if (last)
		((YammNode *)Yamm::resolve(last))->next = node;
	else
		list = node;
}

// Appends copies of the other list's elements not already here.
void YammList::operator^=(uint16_t otherList)
{
	uint16_t *node;
	char *s;
	uint16_t len;
	uint16_t copy;
	YammList other(otherList, size, isString);
	uint16_t pos = 0;
	while (other.traverse(pos)) {
		uint16_t mine = 0;
		uint16_t *a = (uint16_t *)Yamm::resolve(pos + 2);
		while (traverse(mine)) {
			uint16_t *b = (uint16_t *)Yamm::resolve(mine + 2);
			if (isString) {
				char *s1 = (char *)Yamm::resolve(*a);
				char *s2 = (char *)Yamm::resolve(*b);
				if (strcmp(s1, s2) == 0)
					goto next;
			} else if (memcmp(a, b, size) == 0)
				goto next;
		}
		copy = Yamm::flimFlam(size + 2);
		node = (uint16_t *)Yamm::resolve(copy);
		*node++ = 0;
		if (isString) {
			s = (char *)Yamm::resolve(*a);
			len = strlen(s) + 1;
			uint16_t str = Yamm::flimFlam(len);
			memcpy(Yamm::resolve(str), s, len);
			*node = str;
		} else
			memcpy(node, a, size);
		*this += copy;
next:	;
	}
}

// Removes the elements found in the other list.
void YammList::operator-=(uint16_t otherList)
{
	YammList other(otherList, size, isString);
	uint16_t pos = 0;
	while (other.traverse(pos)) {
		uint16_t mine = 0;
		uint16_t *a = (uint16_t *)Yamm::resolve(pos + 2);
		uint8_t found = 0;
		uint16_t *prev = 0;
		uint16_t *b;
		while (traverse(mine)) {
			b = (uint16_t *)Yamm::resolve(mine + 2);
			if (isString) {
				char *s1 = (char *)Yamm::resolve(*a);
				char *s2 = (char *)Yamm::resolve(*b);
				if (strcmp(s1, s2) == 0) {
					found++;
					break;
				}
			} else if (memcmp(a, b, size) == 0) {
				found++;
				break;
			}
			prev = b;
		}
		if (found) {
			b--;
			if (prev) {
				prev--;
				*prev = *b;
			} else
				list = *b;
			if (isString) {
				b++;
				Yamm::scram(*b);
			}
			Yamm::scram(mine);
		}
	}
}

int16_t YammList::in(char *element)
{
	if (isString)
		element = (char *)Yamm::resolve(*(uint16_t *)element);
	uint16_t pos = 0;
	while (traverse(pos)) {
		if (isString && strcmp(resolveString(pos), element) == 0)
			return 1;
		if (memcmp(Yamm::resolve(pos + 2), element, size) == 0)
			return 1;
	}
	return 0;
}

void YammList::setElement(int16_t index, char *element)
{
	uint16_t pos = 0;
	int16_t i = 0;
	while (i++ < index)
		if (!traverse(pos))
			return;
	if (isString) {
		YammNode *node = (YammNode *)Yamm::resolve(pos);
		Yamm::scram(node->data);
	}
	memcpy(Yamm::resolve(pos + 2), element, size);
}

void YammList::changePid(uint16_t pid)
{
	uint16_t pos = 0;
	while (traverse(pos)) {
		if (isString)
			Yamm::setPid(*(uint16_t *)Yamm::resolve(pos + 2), pid);
		Yamm::setPid(pos, pid);
	}
}

void printYammLongList(uint16_t list)
{
	YammList l(list, 4, 0);
	uint16_t pos = 0;
	while (l.traverse(pos))
		trace((TraceLevel)50, "%ld\r\n", *(int32_t *)Yamm::resolve(pos + 2));
}

void printYammIntList(uint16_t list)
{
	YammList l(list, 2, 0);
	uint16_t pos = 0;
	while (l.traverse(pos))
		trace((TraceLevel)50, "%d\r\n", *(int32_t *)Yamm::resolve(pos + 2));
}

void printYammStringList(uint16_t list)
{
	YammList l(list, 2, 0);
	uint16_t pos = 0;
	while (l.traverse(pos)) {
		char *s = l.resolveString(pos);
		trace((TraceLevel)50, "%s\r\n", s);
	}
}

void f7key(void)
{
	Yamm::heyMan();
}

void f10key(void)
{
	Yamm::heyMan();
	getch();
}
