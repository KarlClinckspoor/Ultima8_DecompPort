#ifndef YAMM_H
#define YAMM_H

#include "u8port.h"

class BaseFile;

#define YAMM_SIZE 20000

// Heap block header; a handle is the offset just past it.
struct YammBlock
{
	uint16_t next;
	uint16_t prev;
	uint16_t pid;
	uint16_t size;	// top bit set when free

	uint8_t isFree(void);
	uint16_t getSize(void);
	void setFree(void);
	void setUsed(void);
	void grow(uint16_t);
	void setSize(uint16_t);
};

// List node: next handle, then the element (a string handle in string lists).
struct YammNode
{
	uint16_t next;
	uint16_t data;
};

class Yamm
{
public:
	static void *resolve(uint16_t);
	static void setPid(uint16_t, uint16_t);
	static int16_t flimFlam(char *);
	static int16_t flimFlam(uint16_t);
	static void clean(uint16_t);
	static void scram(uint16_t);
	static void heyMan(void);
	static void damn(uint16_t, char *, uint16_t);
	static void shazamm(char *, uint16_t);
	static void load(BaseFile &);
	static void save(BaseFile &);
	static uint16_t numBlocks;
	static char *buffer;
	static YammBlock *head;
	static uint16_t currentUnkPid;
};

class YammList
{
public:
	uint16_t list;
	uint16_t size;
	uint8_t isString;

	YammList(void) { list = 0; }
	YammList(uint16_t, uint16_t, int16_t);
	void set(uint16_t l, uint16_t s, int16_t str) { list = l; size = s; isString = str; }
	void kill(void);
	uint8_t traverse(uint16_t &);
	uint16_t operator=(YammList &);
	void *operator[](int16_t);
	char *resolveString(uint16_t);
	void operator+=(uint16_t);
	void operator^=(uint16_t);
	void operator-=(uint16_t);
	int16_t in(char *);
	void setElement(int16_t, char *);
	void changePid(uint16_t);
};

int16_t initYamm(void);
void uninitYamm(void);
void printYammLongList(uint16_t);
void printYammIntList(uint16_t);
void printYammStringList(uint16_t);
void f7key(void);
void f10key(void);

#endif
