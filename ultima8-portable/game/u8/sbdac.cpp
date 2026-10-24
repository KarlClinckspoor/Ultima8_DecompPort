// name: SBDAC
// Ported from SBDAC.ASM, with what it used of SB.ASM.
// Plays the mixer's output.
//
// On DOS the Sound Blaster played a looping 256-byte DMA buffer, two halves of 128 samples at
// 11025 Hz; DAC_update, called from the kernel's 140 Hz SoundTimer, mixed a new block into a
// half once the card had moved on to the other. The platform plays a queue of samples
// instead, so DAC_update mixes whenever the queue runs short. The card's detection, DSP and
// DMA programming (SB.ASM) have nothing to do on the host.

#include "u8port.h"
#include "plat.h"

#include "sndmgr.h"

#define DMA_HALF 128		// samples in a block
#define SAMPLE_RATE 11025
// Blocks kept queued ahead: the two the DMA buffer held, and a few more, since the host's
// timer runs the SoundTimer only as often as the game pumps (about 50 ms of sound).
#define BLOCKS_AHEAD 4

extern uint16_t DAC_IsPaused;
void DRV_UpdateHardware(void);

uint16_t mondo = 0;	// counts DAC_update calls
static uint8_t clipTable[1024];
static uint8_t dmaStarted = 0;

// The mixed sums (each channel adds -128..127) to unsigned bytes: 384 zeros, a ramp, then
// 384 FFh, indexed by sum + 512.
static void buildClipTable(void)
{
	for (int16_t i = 0; i < 1024; i++)
		clipTable[i] = i < 384 ? 0 : i < 640 ? (uint8_t)(i - 384) : 0xFF;
}

uint8_t DAC_update(void)
{
	int16_t blocks = 0;

	mondo++;
	if (!dmaStarted)
		return 0;
	// As DAC_AreYouHungry: a block is due once the card has played into what is queued.
	while (plat_pcm_pending() < BLOCKS_AHEAD * DMA_HALF && blocks < BLOCKS_AHEAD)
	{
		uint32_t before = (uint32_t)plat_pcm_pending();

		DRV_UpdateHardware();
		blocks++;
		// Paused (everything idle and the silence sent): nothing more to queue.
		if ((uint32_t)plat_pcm_pending() == before)
			break;
	}
	return 0;
}

int16_t DAC_Init(int16_t, int16_t, int16_t)
{
	buildClipTable();
	plat_pcm_start(SAMPLE_RATE);
	dmaStarted = 1;
	DAC_IsPaused = 0;
	return 0;
}

void DAC_Deinit(void)
{
	if (dmaStarted)
	{
		dmaStarted = 0;
		plat_pcm_stop();
	}
}

void DAC_Pause(void)
{
	DAC_IsPaused = 1;
}

void DAC_UnPause(void)
{
	DAC_IsPaused = 0;
}

// Converts a block of sums and queues it.
void DAC_Output(const int16_t *samples)
{
	uint8_t block[DMA_HALF];

	for (int16_t i = 0; i < DMA_HALF; i++)
	{
		int16_t index = (int16_t)(samples[i] + 512);

		// 4 channels of -128..127 stay in the table; DOS read outside it for anything else
		if (index < 0)
			index = 0;
		else if (index > 1023)
			index = 1023;
		block[i] = clipTable[index];
	}
	plat_pcm_queue(block, DMA_HALF);
}
