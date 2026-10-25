#ifndef ITEM_H
#define ITEM_H

#include "u8port.h"

#include "u8process.h"
#include "animprim.h"
#include "dispatch.h"

class Event;
class WorldPoint;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

typedef uint8_t Boolean;
typedef uint16_t Referent;
typedef uint16_t Type;
typedef uint16_t Frame;
typedef uint16_t Quality;
typedef int16_t Quantity;

// Item status bits (Origin's usecode names where known).
#define IN_DISPLAY_LIST		0x0001
#define DISPOSABLE			0x0002
#define OWNED				0x0004
#define CONTAINED			0x0008
#define INVISIBLE			0x0010
#define FLIPPED				0x0020
#define FAST_ONLY			0x0080
#define CONTAINER_GUMP_OPEN	0x0100
#define ETHEREAL			0x0800
#define FAST_AREA			0x2000

class Item
{
public:
	Referent referent;

	Item(void) { referent = 0; }
	Item(Referent r) { referent = r; }
	Item &operator=(Item item) { referent = item.referent; return *this; }
	Boolean isValid(void) { return referent != 0; }
	void set(Referent r) { referent = r; }
	Boolean isAvatar(void) { return referent == 1; }
	// z 254 marks an item taken off the map.
	Boolean isOffMap(void) { return getZ() == 0xfe; }
	Boolean isFastOnly(void) { return (getStatus() & FAST_ONLY) ? TRUE : FALSE; }

	int16_t hurl(int16_t, int16_t, int16_t, int16_t);
	void fall(void);
	void getScreenXY(uint16_t &, uint16_t &);
	int16_t getX(void);
	int16_t getY(void);
	int16_t getGumpX(void);
	int16_t getGumpY(void);
	void setGumpXY(int16_t, int16_t);
	uint8_t getZ(void);
	int16_t getCX(void);
	int16_t getCY(void);
	char getCZ(void);
	void setX(uint16_t);
	void setY(uint16_t);
	void setZ(uint8_t);
	WorldPoint getLoc(void);
	int16_t getNext(void);
	int16_t getWeight(void);
	int16_t getWeightIncludingContents(void);
	int16_t getVolume(void);
	int16_t getCapacity(void);
	int16_t getSurfaceWeight(void);
	void setNext(uint16_t);
	int16_t getType(void);
	void setType(uint16_t);
	int16_t getFrame(void);
	void setFrame(uint16_t);
	void setNpcNum(int16_t);
	void setNpcArray(int16_t);
	int16_t getNpcArray(void);
	void setMapArray(int16_t);
	int16_t getMapArray(void);
	int16_t getQ(void);
	int16_t getQuality(void);
	void setQuality(int16_t);
	int16_t getUnkEggType(void);
	void setUnkEggType(int16_t);
	int16_t getQuantity(void);
	void setQuantity(int16_t);
	int16_t getContents(void);
	int16_t getContainer(void);
	int16_t getRootContainer(void);
	int16_t getGlobNum(void);
	void setGlobNum(uint16_t);
	void setQ(uint16_t);
	void setContents(uint16_t);
	int16_t getFamily(void);
	char getTypeFlag(int16_t);
	int16_t getStatus(void);
	Boolean legal_create(Type, Frame, WorldPoint &);
	Boolean legal_create(Type, Frame, uint16_t, uint16_t, uint16_t);
	Boolean legal_create(Type, Frame, Referent, Quantity);
	Boolean create(Type, Frame);
	void pop(uint16_t, uint16_t, uint8_t);
	void pop(uint16_t);
	void pop(void);
	void pop(WorldPoint &);
	void pop(WorldPoint &&p) { pop(p); }
	void popToEnd(uint16_t);
	void push(void);
	void destroy(void);
	void destroyContents(void);
	void removeContents(void);
	void move(uint16_t, uint16_t, uint8_t);
	void move(uint16_t);
	Boolean legal_move(WorldPoint &, uint16_t, uint16_t);
	Boolean legal_move(WorldPoint &&p, uint16_t a, uint16_t b) { return legal_move(p, a, b); }
	Boolean legal_move(Referent &, uint16_t);
	Boolean superlegal_move(WorldPoint &, uint16_t, uint16_t, uint16_t, Boolean *, Referent *);
	void move(WorldPoint &);
	void move(WorldPoint &&p) { move(p); }
	void move(char);
	char getDirToCoords(uint16_t, uint16_t);
	int16_t getDirFromCoords(uint16_t, uint16_t);
	int16_t getDirFromItem(Referent);
	char getDirToItem(Referent);
	void getFootpad(int16_t &, int16_t &, int16_t &);
	Boolean touches(Referent);
	Boolean overlaps(Referent);
	Boolean overlapsXY(Referent);
	Boolean isOn(Referent);
	Boolean isCompletelyOn(Referent);
	Boolean isAbove(Referent);
	Boolean isUnder(Referent);
	Boolean ascend(int16_t);
	void getPoint(WorldPoint &);
	Boolean isNpc(void);
	Boolean isInNpc(void);
	void touch(void);
	void setStatus(int16_t);
	void orStatus(int16_t);
	void andStatus(int16_t);
	int16_t look(void);
	int16_t use(void);
	int16_t hit(Referent, int16_t);
	int16_t gotHit(Referent, int16_t);
	int16_t anim(void);
	int16_t bark(char *);
	void grab(void);
	int16_t release(void);
	int16_t equip(void);
	int16_t unequip(void);
	int16_t cachein(void);
	int16_t combine(void);
	int16_t calledFromAnim(void);
	int16_t enterFastArea(void);
	int16_t leaveFastArea(void);
	int16_t cast(uint16_t);
	int16_t justMoved(void);
	int16_t animGetHit(uint16_t);
	int16_t guardianBark(int16_t);
	int16_t AvatarStoleSomething(Referent);
	Boolean findTarget(int16_t, int16_t);
	// Declared void, but each returned what it called left in DX:AX, which usecode uses: the
	// slider's value, and the pid of the answer menu's process.
	int32_t getSliderInput(int16_t, int16_t, int16_t);
	uint16_t ask(uint16_t);
	void openGump(int16_t);
	void closeGump(void);
	Boolean isGumpOpen(void);
	int16_t getDamageType(void);
	void receiveHit(Referent, char, int16_t, uint16_t);
	Boolean isExplosive(void);
	Boolean isFixed(void);
	int16_t explode(void);
	Boolean canReach(Referent, int16_t, const WorldPoint *, Boolean);
	Boolean canReach(Referent, int16_t);
	void preload(void);
	int16_t getRange(Referent);
	int16_t getRange(Type, uint16_t, uint16_t);
	int16_t getQHi(void);
	void setQHi(int16_t);
	int16_t getQLo(void);
	void setQLo(int16_t);
	void shoot(WorldPoint &, int16_t, int16_t);
};

// The item found under a screen point.
class FastItem : public Item
{
public:
	FastItem(void) {}
	FastItem(Referent r) { referent = r; }
};

class ReleaseTrigger : public Process
{
public:
	ReleaseTrigger(Referent);
	virtual void process(void);
};

class SpeechProcess : public Process
{
public:
	SpeechProcess(void);
	virtual void process(void);
};

class Enliven : public Process
{
public:
	Enliven(Referent);
	virtual void process(void);
};

class WaitForKeyGump : public NewGump
{
public:
	uint16_t f_34;

	WaitForKeyGump(uint16_t);
	virtual void commandKeyboard(Event &);
};

class DeathGumpProcess : public Process
{
public:
	DeathGumpProcess(void);
	virtual void process(void);
};

class AvatarDeathWatch : public Process
{
public:
	uint8_t f_2a;
	NewGump *gump;

	AvatarDeathWatch(void);
	virtual void process(void);
	void setGump(NewGump *g) { gump = g; }
};

class UnstunNpc : public Process
{
public:
	UnstunNpc(Referent);
	virtual void process(void) { pop(0); }
	virtual void pop(int32_t);
	virtual void fail(int32_t);
};

class GetUp : public AnimPrimitive
{
public:
	GetUp(void) : AnimPrimitive(0, (AnimSet)0, 0, 0, 0, 0) {}
	GetUp(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)4, dir, 0, 0, 0) {}
};

class FallDown : public AnimPrimitive
{
public:
	FallDown(void) : AnimPrimitive(0, (AnimSet)0, 0, 0, 0, 0) {}
	FallDown(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)14, dir, 0, 0, 0) {}
};

void checkReferent(uint16_t, char *, int16_t);
void selectiveProcessKill(Referent);
Boolean killAllButDeathAnim(Referent, Boolean, Boolean);
void checkForVictoryMusic(void);

extern uint16_t dirToXOffset[8];
extern uint16_t dirToYOffset[8];
extern char *AvatarDeathYell[3];

#endif
