#ifndef CLLIST_H
#define CLLIST_H

#include "u8port.h"

class Link;

class LinkedList
{
public:
	Link *head;
	Link *tail;

	LinkedList(void);
	virtual ~LinkedList(void);
	void addToHead(Link *);
	void addToTail(Link *);
	void destroy(Link *, Link *);
	void insertAfter(Link *, Link *);
	void remove(Link *, Link *);
	void kill(void);
	int16_t traverse(Link *&);
	int32_t count(void);
};

#endif
