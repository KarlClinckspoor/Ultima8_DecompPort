#ifndef ANIM_H
#define ANIM_H

#include "u8port.h"

#include "process.h"
#include "chargen.h"
#include "type.h"

class Point;

// Keeps the Avatar from swinging again until a Wait after the swing ends.
class AvatarSwingToggler : public Process
{
public:
	AvatarSwingToggler(uint16_t);
	virtual void process(void) { pop(0); }
	virtual void pop(int32_t);
	virtual void fail(int32_t);
};

void waitForKey(void);
char *getAvatarStatus(char *);
uint8_t createItem(uint16_t, uint16_t, uint16_t, uint16_t, uint8_t);
void f1key(void);
void f2key(void);
void f3key(void);
void f4key(void);
void f5key(void);
void shiftF8Key(void);
void ctrlF8Key(void);
void altF8Key(void);
void jumpAvatar(char, char, Point *);
void carefulStepAvatar(char, char);
void avatarSwing(uint8_t);
void ctrlF1Key(void);
void ctrlF2Key(void);
void ctrlF3Key(void);
void ctrlF4Key(void);
void ctrlF5Key(void);
void shiftF1Key(void);
void shiftF2Key(void);
void shiftF3Key(void);
void shiftF4Key(void);
void shiftF5Key(void);
void altF1Key(void);
void altF2Key(void);
void altF3Key(void);
void altF4Key(void);
void altF5Key(void);
void handleTracker(uint16_t, uint16_t, uint16_t);
void igniteChaos(uint16_t, uint16_t, uint8_t);
uint8_t turnToFace(uint16_t, char, char, uint8_t);

extern char targetedJump;
extern WorldPoint avysDropZone;

#endif
