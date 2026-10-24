#ifndef INTER_H
#define INTER_H

#include "u8port.h"

// A usecode process's stack; it grows down from the top of the buffer.
class Data
{
public:
	char buffer[200];
	char *sp;
	char *top;
	uint16_t bp;

	Data(char *, uint16_t);
	void operator=(Data &);
	void makeFrame(int16_t);

	int16_t popWord(void) { int16_t v = *(int16_t *)sp; sp += 2; return v; }
	int32_t popLong(void) { int32_t v = *(int32_t *)sp; sp += 4; return v; }
	void pushByte(char v) { *(sp -= 1) = v; }
	void pushWord(int16_t v) { *(int16_t *)(sp -= 2) = v; }
	void pushLong(int32_t v) { *(int32_t *)(sp -= 4) = v; }
	void pushPtr(char *p) { *(char **)(sp -= 4) = p; }
	void reserve(int16_t size) { sp -= size; }
	char *stackPtr(int16_t offset) { return sp + offset; }
	int16_t stackWord(int16_t offset) { return *(int16_t *)(sp + offset); }

	// Locals live at negative offsets from bp, arguments at positive ones.
	char *localPtr(int16_t offset) { return (char *)this + bp + offset; }
	char localByte(int16_t offset) { return ((char *)this + bp)[offset]; }
	int16_t localWord(int16_t offset) { return *(int16_t *)((char *)this + bp + offset); }
	int32_t localLong(int16_t offset) { return *(int32_t *)((char *)this + bp + offset); }
	void setLocalByte(int16_t offset, char v) { ((char *)this + bp)[offset] = v; }
	void setLocalWord(int16_t offset, int16_t v) { *(int16_t *)((char *)this + bp + offset) = v; }
	void setLocalLong(int16_t offset, int32_t v) { *(int32_t *)((char *)this + bp + offset) = v; }
};

class Debugger;
class UnkProcess;

class Routine : public Data
{
public:
	char *ip;
	UnkProcess *process;
	uint8_t pad_da[2];
	uint8_t suspended;	// set by a suspend: resuming skips pushing the base return address

	Routine(char *init, uint16_t size, char *code, UnkProcess *owner) : Data(init, size)
	{
		ip = code;
		suspended = 0;
		process = owner;
	}
	void jump(int32_t);
	void jumpFar(int32_t);
	int16_t interpret(int32_t &);
};

// One word of a dependency list: a pid or a DEP_ operator.
class ProcessSel
{
public:
	uint16_t pid;

	ProcessSel(void);
	ProcessSel(uint16_t p) { pid = p; }
	uint8_t isOperator(void) { return (pid & 0x400) ? 1 : 0; }
	uint8_t isProcess(void) { return !isOperator(); }
};

int16_t numToStr(uint16_t);
int32_t strToNum(char *);
void *resolve(uint16_t);

extern Debugger *debugger;
extern uint16_t unkDebug;

#endif
