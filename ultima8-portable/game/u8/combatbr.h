#ifndef COMBATBR_H
#define COMBATBR_H

#include "u8port.h"

#include "u8process.h"
#include "mlist.h"

class TypeChanger : public Process
{
public:
	uint16_t newType;

	TypeChanger(uint16_t, uint16_t);
	virtual void process(void);
};

class WaitForVictim : public ProcessInterrupt
{
public:
	uint8_t victimFound;	// set when the wait runs out
	uint16_t range;
	uint16_t waitPid;

	WaitForVictim(uint16_t, int16_t);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

class CombatBrain : public ProcessInterrupt
{
public:
	uint16_t target;
	uint16_t fixedTarget;
	uint16_t shiftWaitPid;	// a hidden changeling waits on it before coming out
	uint8_t f_34;
	uint8_t f_35;
	uint8_t f_36;
	uint8_t changeling;	// type 357, which hides as type 1

	CombatBrain(uint16_t, uint16_t);
	virtual void process(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	int16_t getTarget(void);
	void setTarget(uint16_t);
	void lockTarget(uint16_t);
	uint8_t selectTarget(int16_t);
	uint8_t canHitTarget(void);
	void attackTarget(void);
	void gotoTarget(void);
	uint8_t isFacingTarget(void);
	uint8_t faceTarget(void);
	uint8_t hasTarget(void);
	void wander(uint8_t);
};

class DataTable
{
public:
	static int16_t getNumMonsterRecords(void);
	static int16_t getMonsterRecord(uint16_t);
	static uint16_t monsterTypes[31];
	static uint8_t data[416];
};

uint8_t isFeignable(uint16_t);

#endif
