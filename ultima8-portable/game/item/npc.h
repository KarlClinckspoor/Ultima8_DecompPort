#ifndef NPC_H
#define NPC_H

#include "u8port.h"

#include "u8process.h"
#include "flex.h"
#include "animcach.h"
#include "animprim.h"
#include "init.h"
#include "item.h"
#include "crect.h"

class BaseFile;
class NpcDataFile;
class WorldPoint;

enum Activity : int16_t
{
	Activity_0
};

// One NPC's 49-byte record in npcdata.dat.
struct NpcData
{
	char str;
	char dex;
	char intelligence;
	uint8_t hp;
	uint8_t dir;
	uint16_t lastAnimSet;
	uint8_t frameHi;	// high byte of the frame number
	uint8_t animFrame;	// current frame of the running animation
	uint8_t fallZ;	// height a fall started from
	uint8_t pad_0a;
	uint8_t alignment;	// own alignment in the low nibble, enemies' in the high
	uint8_t pad_0c[15];
	uint16_t invincible : 1;
	uint16_t ascending : 1;
	uint16_t descending : 1;
	uint16_t animLock : 1;
	uint16_t flags1B_4 : 1;
	uint16_t flags1B : 11;
	Referent equip[8];
	int16_t mana;
	uint16_t inUse : 1;	// slot taken by a live NPC
	uint16_t falling : 1;
	uint16_t firstStep : 1;	// first half of a two-step animation done
	uint16_t inCombat : 1;
	uint16_t dead : 1;
	uint16_t flags2F_5 : 1;
	uint16_t combatPending : 1;
	uint16_t balancing : 1;	// the Avatar teeters on an edge
	uint16_t airWalk : 1;
	uint16_t swinging : 1;	// the Avatar is recovering from a swing
	uint16_t immortal : 1;
	uint16_t withstandDeath : 1;
	uint16_t feignDeath : 1;
	uint16_t stunned : 1;
	uint16_t poisoned : 1;
	uint16_t pathfinding : 1;
};

class Npc : public Item
{
public:
	Npc(void) {}
	Npc(Referent r) { referent = r; }

	static void init(void);
	static void uninit(void);
	static void save(void);
	static void load(void);
	Boolean isBusy(void);
	int16_t doAnim(AnimSet, int16_t, int16_t, uint8_t);
	// Borland turned a number into an enum by itself; callers pass numbers.
	int16_t doAnim(int a, int16_t b, int16_t c, uint8_t d) { return doAnim((AnimSet)a, b, c, d); }
	int16_t getNumTypes(Type, uint16_t);
	void setTarget(Referent);
	int16_t getTarget(void);
	int16_t areEnemiesNear(void);
	Boolean isInCombat(void);
	void setAlignment(uint8_t);
	uint8_t getAlignment(void);
	void setEnemyAlignment(uint8_t);
	uint8_t getEnemyAlignment(void);
	Boolean isDead(void);
	void setDead(void);
	void clrDead(void);
	int16_t pathfind(uint16_t, uint16_t, uint16_t, uint16_t);
	int16_t pathfind(uint16_t, uint16_t);
	static int16_t getNpcSlot(void);
	void freeNpcSlot(void);
	void setAirWalkEnabled(Boolean);
	Boolean getAirWalkEnabled(void);
	Boolean isImmortal(void);
	void setImmortal(void);
	void clrImmortal(void);
	Boolean isFeignDeath(void);
	void setFeignDeath(void);
	void clrFeignDeath(void);
	Boolean isStunned(void);
	void setStunned(void);
	void clrStunned(void);
	Boolean isPoisoned(void);
	void setPoisoned(void);
	void clrPoisoned(void);
	Boolean isPathfinding(void);
	void setPathfinding(void);
	void clrPathfinding(void);
	Boolean isInvincible(void);
	void setInvincible(void);
	void clrInvincible(void);
	Boolean isAscending(void);
	void setAscending(void);
	void clrAscending(void);
	Boolean isDescending(void);
	void setDescending(void);
	void clrDescending(void);
	Boolean isAnimLock(void);
	void setAnimLock(void);
	void clrAnimLock(void);
	Boolean isBlocking(void);
	Boolean isWithstandDeath(void);
	void setWithstandDeath(void);
	void clrWithstandDeath(void);
	Boolean isEnemy(Referent);
	Boolean simpleCreate(Type, Frame);
	Boolean create(Type, Frame);
	void insertNpcData(void);
	Boolean legal_create(Type, Frame, WorldPoint &);
	Boolean legal_create(Type, Frame, uint16_t, uint16_t, uint16_t);
	Referent getEquip(int16_t);
	Boolean setEquip(int16_t, Referent);
	void unEquip(int16_t);
	void freeEquip(Referent);
	int16_t findEquip(Referent);
	void clearEquip(void);
	void teleport(uint16_t, uint16_t, uint8_t, uint8_t);
	int16_t getMap(void);
	int16_t setActivity(uint32_t);
	// Declared void; usecode takes what the activity's call left in AX as a pid.
	uint16_t cSetActivity(Activity);
	int16_t schedule(uint32_t);
	uint8_t getDamageAmount(void);
	uint8_t getArmorClass(void);
	char getAttackingDex(void);
	char getDefendingDex(void);
	int16_t getDefenseType(void);
	int16_t setInCombat(void);
	void clrInCombat(void);
	void callForHelp(void);
	char getStr(void);
	char getDex(void);
	char getInt(void);
	uint8_t getHp(void);
	int16_t getMana(void);
	void setStr(char);
	void setDex(char);
	void setInt(char);
	void setHp(uint8_t);
	void setMana(int16_t);
	uint8_t getDir(void);
	int16_t getLastAnimSet(void);
	static NpcDataFile *npcFile;
	static uint8_t namefile[4];
	static NpcData *npcData;

	Boolean isInFastArea(void) { return (getStatus() & FAST_AREA) ? TRUE : FALSE; }
	Boolean isAtZ254(void) { return getZ() == 254; }

	// Set while combat is about to start; cleared when it does.
	uint8_t isCombatPending(void) { return ((NpcData *)npcData)[referent].combatPending; }
	uint8_t getFrameHi(void) { return ((NpcData *)npcData)[referent].frameHi; }
	void setFrameHi(uint8_t hi) { ((NpcData *)npcData)[referent].frameHi = hi; }
	uint8_t isFalling(void) { return ((NpcData *)npcData)[referent].falling; }
	uint8_t getFallZ(void) { return ((NpcData *)npcData)[referent].fallZ; }
	void setFallZ(uint8_t z) { ((NpcData *)npcData)[referent].fallZ = z; }
	uint8_t isFirstStep(void) { return ((NpcData *)npcData)[referent].firstStep; }
	uint8_t isFlag2F_5(void) { return ((NpcData *)npcData)[referent].flags2F_5; }
	uint8_t isBalancing(void) { return ((NpcData *)npcData)[referent].balancing; }
	uint8_t isSwinging(void) { return ((NpcData *)npcData)[referent].swinging; }
	void setAnimFrame(uint8_t frame) { ((NpcData *)npcData)[referent].animFrame = frame; }
	void setLastAnimSet(int16_t anim) { ((NpcData *)npcData)[referent].lastAnimSet = anim; }
	void setDir(char dir) { ((NpcData *)npcData)[referent].dir = dir; }
};

class Avatar : public Npc
{
public:
	int16_t strengthPoints;		// progress toward the next point of each stat
	int16_t intelligencePoints;
	int16_t dexterityPoints;
	char name[32];
	int16_t sex;
	uint8_t f_2a;
	uint8_t f_2b;
	uint8_t cheater;
	uint8_t canCheat;
	int16_t inStasis;
	int32_t randomSeed;
	uint8_t music;
	uint8_t pad_35[1];
	uint16_t f_36;
	char language;
	char f_39;
	uint8_t effectiveStr;
	uint8_t f_3b;
	Point f_3c;
	Point f_40;
	Point f_44;
	uint8_t f_48;
	uint8_t soundOn;
	uint8_t musicOn;
	uint8_t pad_4b[4];

	Avatar(void);
	char getEffectiveStr(void);
	void setEffectiveStr(int16_t);
	int16_t setInCombat(void);
	void clrInCombat(void);
	void init(void);
	void accumulateStrength(int16_t);
	void accumulateIntelligence(int16_t);
	void accumulateDexterity(int16_t);
	Boolean setLanguage(Language);
	void load(void);
	void save(char *);
	static char LanguageDescChar[15];

	int16_t getLanguageChar(void) { return LanguageDescChar[language]; }
};

class ClearFeignDeath : public Process
{
public:
	ClearFeignDeath(uint16_t);
	virtual void process(void);
};

class DeathLoader : public Process
{
public:
	Boolean dead;	// set when the avatar dies

	DeathLoader(void);
	virtual void process(void);
	virtual void load(BaseFile *);
};

class BlockEnd : public AnimPrimitive
{
public:
	BlockEnd(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)60, dir, 0, 0, 0) {}
};

class BlockStart : public AnimPrimitive
{
public:
	BlockStart(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)59, dir, 0, 0, 0) {}
};

class Kick : public AnimPrimitive
{
public:
	Kick(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)58, dir, 0, 0, 0) {}
};

class Burn : public AnimPrimitive
{
public:
	Burn(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)57, dir, 0, 0, 0) {}
};

class Drown : public AnimPrimitive
{
public:
	Drown(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)56, dir, 0, 0, 0) {}
};

class Work : public AnimPrimitive
{
public:
	Work(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)55, dir, 0, 0, 0) {}
};

class GiveItem : public AnimPrimitive
{
public:
	GiveItem(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)54, dir, 0, 0, 0) {}
};

class Talk : public AnimPrimitive
{
public:
	Talk(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)53, dir, 0, 0, 0) {}
};

class SitUp : public AnimPrimitive
{
public:
	SitUp(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)52, dir, 0, 0, 0) {}
};

class SitDown : public AnimPrimitive
{
public:
	SitDown(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)51, dir, 0, 0, 0) {}
};

class KneelUp : public AnimPrimitive
{
public:
	KneelUp(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)50, dir, 0, 0, 0) {}
};

class KneelDown : public AnimPrimitive
{
public:
	KneelDown(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)49, dir, 0, 0, 0) {}
};

class Fidget2 : public AnimPrimitive
{
public:
	Fidget2(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)48, dir, 0, 0, 0) {}
};

class Fidget : public AnimPrimitive
{
public:
	Fidget(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)47, dir, 0, 0, 0) {}
};

class LedgeClimb : public AnimPrimitive
{
public:
	LedgeClimb(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)46, dir, 0, 0, 0) {}
};

class LedgeSwing : public AnimPrimitive
{
public:
	LedgeSwing(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)45, dir, 0, 0, 0) {}
};

class FallRecDmg : public AnimPrimitive
{
public:
	FallRecDmg(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)44, dir, 0, 0, 0) {}
};

class FallRecHurt : public AnimPrimitive
{
public:
	FallRecHurt(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)43, dir, 0, 0, 0) {}
};

class EdgeBalance : public AnimPrimitive
{
public:
	EdgeBalance(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)42, dir, 0, 0, 0) {}
};

class AltitudeFall : public AnimPrimitive
{
public:
	AltitudeFall(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)41, dir, 0, 0, 0) {}
};

class SwingWeapon2 : public AnimPrimitive
{
public:
	SwingWeapon2(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)39, dir, 0, 0, 0) {}
};

class HighReach : public AnimPrimitive
{
public:
	HighReach(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)38, dir, 0, 0, 0) {}
};

class MidReach : public AnimPrimitive
{
public:
	MidReach(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)37, dir, 0, 0, 0) {}
};

class LowReach : public AnimPrimitive
{
public:
	LowReach(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)36, dir, 0, 0, 0) {}
};

class Kneel : public AnimPrimitive
{
public:
	Kneel(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)35, dir, 0, 0, 0) {}
};

class Bow : public AnimPrimitive
{
public:
	Bow(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)34, dir, 0, 0, 0) {}
};

class LookRight : public AnimPrimitive
{
public:
	LookRight(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)33, dir, 0, 0, 0) {}
};

class LookLeft : public AnimPrimitive
{
public:
	LookLeft(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)32, dir, 0, 0, 0) {}
};

class Cast3 : public AnimPrimitive
{
public:
	Cast3(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)31, dir, 0, 0, 0) {}
};

class Cast2 : public AnimPrimitive
{
public:
	Cast2(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)30, dir, 0, 0, 0) {}
};

class Cast1 : public AnimPrimitive
{
public:
	Cast1(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)29, dir, 0, 0, 0) {}
};

class ExitCast : public AnimPrimitive
{
public:
	ExitCast(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)28, dir, 0, 0, 0) {}
};

class EnterCast : public AnimPrimitive
{
public:
	EnterCast(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)27, dir, 0, 0, 0) {}
};

class NinjaFlip : public AnimPrimitive
{
public:
	NinjaFlip(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)18, dir, 0, 0, 0) {}
};

class FallRecovery : public AnimPrimitive
{
public:
	FallRecovery(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)16, dir, 0, 0, 0) {}
};

class CombatStand : public AnimPrimitive
{
public:
	CombatStand(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)15, dir, 0, 0, 0) {}
};

class GetHit : public AnimPrimitive
{
public:
	GetHit(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)13, dir, 0, 0, 0) {}
};

class CarefulStep : public AnimPrimitive
{
public:
	CarefulStep(Referent npc, char dir, WorldPoint *dest = 0, Referent target = 0) :
		AnimPrimitive(npc, (AnimSet)12, dir, dest, target, 0) {}
};

class RunningLeap : public AnimPrimitive
{
public:
	RunningLeap(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)10, dir, 0, 0, 0) {}
};

class Retreat : public AnimPrimitive
{
public:
	Retreat(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)9, dir, 0, 0, 0) {}
};

class Advance : public AnimPrimitive
{
public:
	Advance(uint16_t npc, char dir, WorldPoint *dest = 0, uint16_t target = 0) :
		AnimPrimitive(npc, ANIM_ADVANCE, dir, dest, target, 0) {}
};

class SwingWeapon : public AnimPrimitive
{
public:
	SwingWeapon(Referent npc, char dir, WorldPoint *dest = 0, Referent target = 0) :
		AnimPrimitive(npc, (AnimSet)7, dir, dest, 0, target) {}
};

class SheathWeapon : public AnimPrimitive
{
public:
	SheathWeapon(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)6, dir, 0, 0, 0) {}
};

class DrawWeapon : public AnimPrimitive
{
public:
	DrawWeapon(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)5, dir, 0, 0, 0) {}
};

class VertLeap : public AnimPrimitive
{
public:
	VertLeap(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)3, dir, 0, 0, 0) {}
};

class Stand : public AnimPrimitive
{
public:
	Stand(Referent npc, char dir) : AnimPrimitive(npc, (AnimSet)2, dir, 0, 0, 0) {}
};

class Run : public AnimPrimitive
{
public:
	Run(Referent npc, char dir, WorldPoint *dest = 0, Referent target = 0) :
		AnimPrimitive(npc, (AnimSet)1, dir, dest, target, 0) {}
};

class Walk : public AnimPrimitive
{
public:
	Walk(uint16_t npc, char dir, WorldPoint *dest = 0, uint16_t target = 0) :
		AnimPrimitive(npc, ANIM_WALK, dir, dest, target, 0) {}
};

class NpcDataFile : public FlexFile
{
public:
	NpcDataFile(void) {}
};

void initNpcs(void);
void uninitNpcs(void);
void SetAvatarInCombat(void);
void ClrAvatarInCombat(void);
int16_t IsAvatarInCombat(void);
int16_t getName(void);
int16_t getSex(void);
int16_t getAvatarInStasis(void);
void setAvatarInStasis(int16_t);
void AccumulateStrength(int16_t);
void AccumulateIntelligence(int16_t);
void AccumulateDexterity(int16_t);
char AvatarCanCheat(void);
void MakeAvatarACheater(void);
void realAvatarDeath(void);
char avatarDotMusic(void);
int16_t avatarDotGetLanguage(void);
Boolean avatarDotIsInCombat(void);

extern char *NpcDataFileName;
extern char *NpcNameFileName;
extern char *AvatarFileName;
extern DeathLoader *theDeathLoader;
extern uint8_t avatarSettingCombat;
extern Avatar avatar;

#endif
