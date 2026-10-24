// name: DAC
// Ported from DAC.ASM.
// Mixes four 8-bit sample channels into a buffer for the sound card, 128 samples at a time.
//
// Positions and steps are 16.16 fixed point; positions are 16-bit offsets into the sample's
// memory, as they were into its segment. DOS named the sample by its far address (the DAD's
// selector, the data a little way in); here each channel keeps the address it was given as
// its base and offsets count from 0 there, which gives the same lengths and loop points.
// Everything here runs on the game thread (the mixer is driven by DAC_update from the
// kernel's real-time SoundTimer), so the cli/sti pairs of the assembly have nothing to guard.

#include "u8port.h"

#include "sndmgr.h"

#define NUM_CHANNELS 4
#define BLOCK_SAMPLES 128	// output samples mixed per update
#define MIX_RATE 11025		// output rate in Hz
#define SILENT_BLOCKS 20	// silent blocks sent once idle, before pausing the DAC
#define MAX_VOLUME 255

#define CH_IDLE 0
#define CH_PLAYING 1
#define CH_RELEASED 2		// plays on to the end without looping

// One sound channel. Offsets are into the sample's memory (base).
struct Channel
{
	uint8_t state;
	uint16_t loopLen;	// nonzero makes the sample loop
	char *base;		// chSeg
	uint16_t start;
	uint16_t loopStart;
	uint16_t loopEnd;
	uint16_t end;
	uint16_t restart;	// where play resumes on reaching stop when looping
	uint16_t stop;
	uint16_t baseStep;	// step as last set, kept beside the live one
	uint16_t baseFrac;
	uint16_t pos;
	uint16_t posFrac;
	uint16_t step;		// advance per output sample: rate / MIX_RATE
	uint16_t stepFrac;
	int16_t volume;		// 0..255
	const int8_t *volTable;	// VolumeTable row for volume
};

// Implemented in sbdac.cpp, the card's side (SBDAC.ASM).
int16_t DAC_Init(int16_t port, int16_t irq, int16_t dma);
void DAC_Deinit(void);
void DAC_Pause(void);
void DAC_UnPause(void);
void DAC_Output(const int16_t *samples);
void DRV_UpdateHardware(void);

// 16 rows of 256 bytes, built at start-up: row v holds s * v / 16 for each signed sample
// byte s.
static int8_t VolumeTable[16][256];
static Channel channels[NUM_CHANNELS];
static uint16_t silentBlocks = SILENT_BLOCKS;

int16_t DACBuffer[256];	// signed sums; the first BLOCK_SAMPLES are mixed
uint16_t DAC_IsPaused = 0;

static void BuildVolumeTables(void)
{
	for (int16_t v = 0; v < 16; v++)
		for (int16_t b = 0; b < 256; b++)
		{
			int16_t s = (int8_t)b;
			// imul of v * 16 by the sample; the table keeps AH of the product
			uint16_t product = (uint16_t)(v * 16 * s);
			VolumeTable[v][b] = (int8_t)(product >> 8);
		}
}

static void StopAllChannels(void)
{
	for (int16_t i = 0; i < NUM_CHANNELS; i++)
		channels[i].state = CH_IDLE;
}

// Sets the channel's step to rate / MIX_RATE.
static void setStep(Channel *c, uint16_t rate)
{
	uint16_t step = rate / MIX_RATE;
	uint16_t frac = (uint16_t)(((uint32_t)(rate % MIX_RATE) << 16) / MIX_RATE);

	c->step = c->baseStep = step;
	c->stepFrac = c->baseFrac = frac;
}

// int DGTL_init(int port, int irq, int dma, unsigned dmaSelector, unsigned dmaOffset,
//               unsigned dmaParagraph)
// Returns -1 when the card cannot be set up. The DMA buffer belongs to the platform now.
int16_t DGTL_init(int16_t port, int16_t irq, int16_t dma, uint16_t, int16_t, uint16_t)
{
	BuildVolumeTables();
	StopAllChannels();
	return DAC_Init(port, irq, dma);
}

void DGTL_shutdown(void)
{
	StopAllChannels();
	DAC_Deinit();
}

void DGTL_channel_volume(int16_t channel, int16_t volume)
{
	Channel *c;
	int16_t v;
	uint16_t row;

	if (channel > NUM_CHANNELS - 1 || channel < 0)
		return;
	c = &channels[channel];
	c->volume = volume;
	v = volume;
	if (v > MAX_VOLUME)
		v = MAX_VOLUME;
	// Row volume / 16 starts 256 * (volume / 16) bytes in. A negative volume pointed outside
	// the table on DOS; the row number is kept to the table here.
	row = (uint16_t)((uint16_t)(v << 4) >> 8);
	c->volTable = VolumeTable[row & 15];
}

void DGTL_channel_rate(int16_t channel, int16_t rate)
{
	if (channel > NUM_CHANNELS - 1 || channel < 0)
		return;
	setStep(&channels[channel], (uint16_t)rate);
}

// void DGTL_play(int channel, char far *sample, int length, int rate, int volume)
void DGTL_play(int16_t channel, char *data, int16_t length, int16_t rate, int16_t volume)
{
	Channel *c;

	if (channel > NUM_CHANNELS - 1 || channel < 0)
		return;
	c = &channels[channel];
	c->base = data;
	c->start = 0;
	c->loopStart = 0;
	c->loopEnd = 0;
	c->end = (uint16_t)length;
	c->loopLen = 0;
	setStep(c, (uint16_t)rate);
	DGTL_channel_volume(channel, volume);
	c->restart = c->start;
	c->stop = c->end;
	c->pos = c->start;
	c->posFrac = 0;
	c->state = CH_PLAYING;
}

void DGTL_stop(int16_t channel)
{
	if (channel > NUM_CHANNELS - 1 || channel < 0)
		return;
	channels[channel].state = CH_IDLE;
}

// void DGTL_play_ambient(int channel, char far *sample, int length, int loopStart,
//                        int loopLength, int rate, int volume)
// Plays to the loop end, then repeats the loop until released.
void DGTL_play_ambient(int16_t channel, char *data, int16_t length, int16_t loopStart, int16_t loopLength,
	int16_t rate, int16_t volume)
{
	Channel *c;

	if (channel > NUM_CHANNELS - 1 || channel < 0)
		return;
	c = &channels[channel];
	c->base = data;
	c->start = 0;
	c->loopStart = (uint16_t)loopStart;
	c->loopEnd = (uint16_t)(c->loopStart + loopLength);
	c->end = (uint16_t)length;
	c->loopLen = (uint16_t)loopLength;
	setStep(c, (uint16_t)rate);
	DGTL_channel_volume(channel, volume);
	c->restart = c->loopStart;
	c->stop = c->loopLen ? c->loopEnd : c->end;
	c->pos = c->start;
	c->posFrac = 0;
	c->state = CH_PLAYING;
}

// Lets a looping channel play on to its end.
void DGTL_release(int16_t channel)
{
	Channel *c;

	if (channel > NUM_CHANNELS - 1 || channel < 0)
		return;
	c = &channels[channel];
	c->stop = c->end;
	c->state = CH_RELEASED;
}

// Swaps in a new sample without moving the play position.
void DGTL_set_voice(int16_t channel, char *data, int16_t length, int16_t loopStart, int16_t loopLength)
{
	Channel *c;

	if (channel > NUM_CHANNELS - 1 || channel < 0)
		return;
	c = &channels[channel];
	c->base = data;
	c->start = 0;
	c->end = (uint16_t)length;
	c->loopStart = (uint16_t)loopStart;
	c->loopEnd = (uint16_t)(c->loopStart + loopLength);
	c->loopLen = (uint16_t)loopLength;
}

// Adds one playing channel into DACBuffer. A channel reaching stop loops back to restart
// while looping, else goes idle and calls back with its number.
static void mixChannel(Channel *c, int16_t number)
{
	uint32_t blockEnd;
	uint16_t si, cx, blockStart, runEnd;
	uint16_t mixIndex;

	if (c->state == CH_IDLE)
		return;
	// Mix the whole block at once when its last sample, pos + 128 * step (rounded up),
	// still lies before stop. The assembly kept only the low 32 bits of the sum.
	blockEnd = (((uint32_t)c->step << 16 | c->stepFrac) << 7) + 0xFFFFu
		+ ((uint32_t)c->pos << 16 | c->posFrac);
	if ((uint16_t)(blockEnd >> 16) < c->stop)
	{
		const int8_t *vol = c->volTable;
		uint32_t at;

		si = c->pos;
		blockStart = si;
		cx = c->posFrac;
		for (int16_t n = 0; n < BLOCK_SAMPLES; n++)
		{
			DACBuffer[n] += vol[(uint8_t)c->base[si]];
			at = (uint32_t)cx + c->stepFrac;
			cx = (uint16_t)at;
			si = (uint16_t)(si + c->step + (at >> 16));
		}
		c->posFrac = cx;
		c->pos = (uint16_t)(c->pos + (uint16_t)(si - blockStart));
		return;
	}
	// Otherwise one sample at a time, up to stop.
	mixIndex = 0;
	for (;;)
	{
		uint16_t runLength = (uint16_t)(c->stop - c->pos);
		uint32_t at;

		si = c->pos;
		blockStart = si;
		runEnd = (uint16_t)(c->pos + runLength);
		cx = c->posFrac;
		for (;;)
		{
			DACBuffer[mixIndex] += c->volTable[(uint8_t)c->base[si]];
			mixIndex++;
			at = (uint32_t)cx + c->stepFrac;
			cx = (uint16_t)at;
			si = (uint16_t)(si + c->step + (at >> 16));
			if (si >= runEnd)
				break;	// ended
			if (mixIndex == BLOCK_SAMPLES)
			{
				c->posFrac = cx;
				c->pos = (uint16_t)(c->pos + (uint16_t)(si - blockStart));
				return;
			}
		}
		// ended
		if (c->state != CH_PLAYING || c->loopLen == 0)
		{
			c->state = CH_IDLE;
			digital_call_back(number);
			return;
		}
		c->posFrac = cx;
		c->pos = c->restart;
		if (mixIndex == BLOCK_SAMPLES)
			return;
	}
}

// void DRV_UpdateHardware(void)
// Mixes the next block and hands it to the card. With every channel idle it sends
// SILENT_BLOCKS more of silence, then pauses the DAC.
void DRV_UpdateHardware(void)
{
	uint8_t busy = (uint8_t)(channels[0].state + channels[1].state + channels[2].state + channels[3].state);

	if (busy == 0)
	{
		if (DAC_IsPaused)
			return;
		if ((int16_t)silentBlocks < 0)
		{
			DAC_Pause();
			return;
		}
		silentBlocks--;
		memset(DACBuffer, 0, BLOCK_SAMPLES * sizeof(int16_t));
		DAC_Output(DACBuffer);
		return;
	}
	silentBlocks = SILENT_BLOCKS;
	if (DAC_IsPaused)
		DAC_UnPause();
	memset(DACBuffer, 0, BLOCK_SAMPLES * sizeof(int16_t));
	mixChannel(&channels[0], 0);
	mixChannel(&channels[1], 1);
	mixChannel(&channels[2], 2);
	mixChannel(&channels[3], 3);
	DAC_Output(DACBuffer);
}
