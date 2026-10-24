#ifndef SNDMGR_H
#define SNDMGR_H

#include "u8port.h"

// A loaded sample: a 32-byte header, then the 8-bit data.
class DAD
{
public:
	int32_t size;
	int16_t rate;
	int16_t f_06;
	int16_t f_08;
	int16_t f_0a;
	int16_t f_0c;
	uint8_t pad_0e[18];
	char data[1];
};

struct SampleEntry
{
	int16_t users;
	DAD *dad;
};

// Samples over STREAM_SIZE bytes play from two STREAM_CHUNK buffers that
// are refilled from the file while the other one plays.
#define STREAM_CHUNK	0x7F00
#define STREAM_SIZE		0xFE00L

#define NUM_CHANNELS	4

struct SoundChannel
{
	int32_t handle;
	int16_t sample;
	int16_t priority;
	int16_t volume;
	int16_t rate;
	char ambient;
	char streamed;
	int16_t chunksLeft;
	int16_t lastChunkSize;
	char *bufferA;
	char *bufferB;
	int32_t filePos;
	char playB;
	char needLoad;
};

class SndMgr
{
public:
	int16_t sampleCount;
	SampleEntry *samples;
	uint16_t dmaParagraph;
	uint16_t dmaSelector;
	int16_t maxSuspended;
	int16_t numSuspended;
	SoundChannel *suspended;
	int16_t activeChannels;
	int32_t lastHandle;
	int32_t memoryUsed;
	SoundChannel channels[NUM_CHANNELS];

	SndMgr(int16_t, int16_t);
	~SndMgr(void);
	uint8_t init_sound_blaster(int16_t, int16_t, int16_t);
	uint8_t load(int16_t, DAD *);
	void stop_channel(int16_t);
	void discard(int16_t);
	int32_t play(int16_t, int16_t, int16_t, int16_t);
	void rewind(int32_t);
	void stop(int32_t);
	int32_t play_ambient(int16_t, int16_t, int16_t, int16_t);
	void release_ambient(int32_t);
	void stop_ambient(int32_t);
	int16_t get_sample_number(int32_t);
	int32_t get_sample_handle(int16_t);
	void stop_all(void);
	void flush(void);
	void set_volume(int32_t, int16_t);
	void set_pitch(int32_t &, int16_t);
	void change_pitch(int32_t &, int16_t);
	void update(void);
	void update_ambient(void);
	void update_huge_samples(void);
	int16_t get_channel(int16_t, int16_t);
	void suspend_ambient(int16_t);
	void resume_ambient(int16_t);
};

void SoundBlasterExitListHook(void);
extern "C" void digital_call_back(int16_t);

// Digital sound driver.
extern "C" {
int16_t DGTL_init(int16_t port, int16_t irq, int16_t dma, uint16_t selector, int16_t, uint16_t paragraph);
void DGTL_shutdown(void);
void DGTL_channel_volume(int16_t channel, int16_t volume);
void DGTL_channel_rate(int16_t channel, int16_t rate);
void DGTL_play(int16_t channel, char *data, int16_t length, int16_t rate, int16_t volume);
void DGTL_stop(int16_t channel);
void DGTL_play_ambient(int16_t channel, char *data, int16_t length, int16_t, int16_t, int16_t rate, int16_t volume);
void DGTL_release(int16_t channel);
uint8_t DAC_update(void);
extern uint8_t DACBuffer[512];
}

extern SndMgr *SoundManager;
extern uint8_t SoundSystemActive;

#endif
