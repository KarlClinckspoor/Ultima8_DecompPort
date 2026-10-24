#ifndef DSFXMAN_H
#define DSFXMAN_H

#include "u8port.h"

#include "process.h"

class DAD;
class SoundEffectFlex;
class SpeechFlex;

// Sample number of the current speech; its data comes from speechFile.
#define SPEECH_SAMPLE	255

// A sound that follows an item: its volume falls with the item's distance.
struct Speaker
{
	int32_t handle;
	uint16_t item;
	int16_t maxVolume;
};

class SpeakerManager : public Process
{
public:
	Speaker *speakers;
	int16_t count;

	SpeakerManager(int16_t);
	virtual void process(void);
	virtual ~SpeakerManager(void);
	virtual void freeMemory(void);
	virtual void postLoad(void);
	void add(int32_t, uint16_t, int16_t);
	void remove(int16_t, uint16_t);
	void remove(int32_t);
	void flush(void);
};

// Streams the next chunk of each large sample.
class SoundTimerIITheSequel : public ProcessRealTime
{
public:
	SoundTimerIITheSequel(void);
	virtual void process(void);
	virtual ~SoundTimerIITheSequel(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	void start(void);
	void stop(void);
};

// Services the sound driver and resumes suspended ambient sounds.
class SoundTimer : public ProcessRealTime
{
public:
	SoundTimer(void);
	virtual void process(void);
	virtual ~SoundTimer(void);
	virtual void interruptHandler(uint32_t, uint32_t);
	void start(void);
	void stop(void);
};

// A sound card's settings, as kept in u8.ini.
class SoundCard
{
public:
	int16_t type;
	int16_t port;
	int16_t irq;
	int16_t dma;

	SoundCard(void) { type = port = irq = 0; dma = -1; }
};

uint8_t sample_load(int16_t, DAD *&);
void sample_load_chunk(int16_t, char *, int16_t, int32_t);
void sample_report(int32_t, int16_t);
int16_t GetVolumeByDistance(uint16_t, int16_t);
void checkFlush(int16_t);
int32_t ActualSamplePlay(int16_t, uint16_t, uint8_t, uint8_t, int16_t);
int32_t ActualSamplePlayAmbient(int16_t, uint16_t, uint8_t, uint8_t, int16_t);
void soundInit(int16_t, int16_t, int16_t);
void soundDeInit(void);
uint8_t playSpeech(int16_t, char *);
void playSFX(int16_t);
void playSFX(int16_t, uint8_t);
void singlePlay(int16_t);
void playSFX(int16_t, int16_t);
void playSFX(int16_t, int16_t, uint16_t);
void playAmbientSFX(int16_t);
void playAmbientSFX(int16_t, int16_t);
void playAmbientSFX(int16_t, int16_t, uint16_t);
void stopSFX(int16_t, uint16_t);
void stopSFX(int16_t);
void discardSFX(int16_t);
void setVolumeSFX(int16_t, int16_t);
void rewindSFX(int16_t);
uint8_t isSFXPlaying(int16_t);
void getSoundStuff(int16_t &, int16_t &, int16_t &, int16_t &, int16_t &, int16_t &, int16_t &, int16_t &, uint8_t &, uint8_t &, char *, char *);
void playCombatMusic(char);
void playMusic(char);
void restoreMusic(void);
void changePitchSFX(int16_t, int16_t);

extern SoundEffectFlex *SFXfile;
extern SpeechFlex *speechFile;
extern int16_t *voices;
extern char currentLanguage;
extern SoundTimer *TheSoundTimer;
extern SoundTimerIITheSequel *TheSoundTimerII;
extern SpeakerManager *SpkrMgr;
extern char *soundDir;
extern char *soundFileName;
extern uint16_t discards[10];
extern uint16_t speechEnded;
extern char *speechQueue;
extern uint16_t speechIndex;
extern char *decompressionBuffer;
extern uint8_t speaking;
extern char *soundNames[4];
extern char *musicNames[7];
extern SoundCard mcard;
extern SoundCard scard;

#endif
