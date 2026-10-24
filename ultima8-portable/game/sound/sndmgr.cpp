// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: ..\SOUND\SNDMGR.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include <stdio.h>
#include <stdlib.h>
// #include <phapi.h>  (DOS only)
#include "cexit.h"
#include "sndmgr.h"
#include "dsfxman.h"

uint8_t SoundSystemActive = 0;
SndMgr *SoundManager = 0;

SndMgr::SndMgr(int16_t numSamples, int16_t numSuspend)
{
	sampleCount = numSamples;
	samples = (SampleEntry *)malloc(sampleCount * sizeof(SampleEntry));
	memset(samples, 0, sampleCount * sizeof(SampleEntry));
	maxSuspended = numSuspend;
	numSuspended = 0;
	suspended = (SoundChannel *)malloc(maxSuspended * sizeof(SoundChannel));
	memset(suspended, 0, maxSuspended * sizeof(SoundChannel));
	memset(channels, 0, sizeof(channels));
	dmaParagraph = 0;
	dmaSelector = 0;
	activeChannels = 0;
	lastHandle = 0;
	memoryUsed = 0;
}

SndMgr::~SndMgr(void)
{
	if (SoundSystemActive)
	{
		DGTL_shutdown();
	}
	SoundSystemActive = 0;
	if (samples)
	{
		for (int16_t i = 0; i < 256; i++)
		{
			if (samples[i].dad)
			{
				delete samples[i].dad;
				samples[i].dad = 0;
			}
		}
		delete samples;
		samples = 0;
	}
	if (suspended)
	{
		delete suspended;
		suspended = 0;
	}
}

void SoundBlasterExitListHook(void)
{
	if (SoundSystemActive)
	{
		SoundSystemActive = 0;
		DGTL_shutdown();
	}
}

// DOS took a 256-byte DMA buffer below 1 MB here (two halves of 128 samples, which the card
// played in turn while DAC_update refilled the other), and set the Sound Blaster mixer's
// register 83h. The platform's sound output takes the samples instead (sbdac.cpp).
uint8_t SndMgr::init_sound_blaster(int16_t irq, int16_t port, int16_t dma)
{
	if (DGTL_init(port, irq, dma, 0, 0, 0) == -1)
	{
		return 0;
	}
	SoundSystemActive = 1;
	return 1;
}

uint8_t SndMgr::load(int16_t sample, DAD *dad)
{
	if (samples[sample].users > 0)
	{
		samples[sample].users++;
	}
	else
	{
		if (dad)
		{
			samples[sample].dad = dad;
		}
		else if (!sample_load(sample, samples[sample].dad))
		{
			return 0;
		}
		if (samples[sample].dad)
		{
			samples[sample].users++;
			memoryUsed += samples[sample].dad->size + 32;
		}
	}
	return 1;
}

void SndMgr::stop_channel(int16_t channel)
{
	if (channels[channel].handle == 0)
	{
		return;
	}
	activeChannels--;
	channels[channel].handle = 0;
	DGTL_stop(channel);
}

void SndMgr::discard(int16_t sample)
{
	int16_t i;

	if (samples == 0)
	{
		return;
	}
	if (samples[sample].users <= 0)
	{
		return;
	}
	samples[sample].users--;
	if (samples[sample].users != 0)
	{
		return;
	}
	for (i = 0; i < NUM_CHANNELS; i++)
	{
		if (channels[i].sample == sample)
		{
			stop_channel(i);
		}
	}
	if (numSuspended)
	{
		for (i = 0; i < maxSuspended; i++)
		{
			if (suspended[i].sample == sample)
			{
				suspended[i].handle = 0;
				numSuspended--;
			}
		}
	}
	if (samples[sample].dad)
	{
		memoryUsed -= samples[sample].dad->size + 32;
		delete samples[sample].dad;
		samples[sample].dad = 0;
	}
}

int32_t SndMgr::play(int16_t sample, int16_t priority, int16_t volume, int16_t pitch)
{
	if (samples[sample].users == 0)
	{
		return 0;
	}
	if (samples[sample].dad == 0)
	{
		samples[sample].users = 0;
		return 0;
	}
	int16_t channel = get_channel(sample, priority);
	if (channel == -1)
	{
		return 0;
	}
	activeChannels++;
	channels[channel].handle = ++lastHandle;
	channels[channel].sample = sample;
	channels[channel].priority = priority;
	channels[channel].volume = volume;
	channels[channel].rate = samples[sample].dad->rate - pitch;
	channels[channel].ambient = 0;
	channels[channel].streamed = 0;
	channels[channel].needLoad = 0;
	sample_report(channels[channel].handle, 1);
	if (samples[sample].dad->size > STREAM_SIZE)
	{
		channels[channel].streamed = 1;
		channels[channel].filePos = STREAM_SIZE;
		channels[channel].chunksLeft = samples[sample].dad->size / STREAM_CHUNK - 1;
		channels[channel].lastChunkSize = samples[sample].dad->size % STREAM_CHUNK;
		if (channels[channel].lastChunkSize)
		{
			channels[channel].chunksLeft++;
		}
		channels[channel].playB = 1;
		channels[channel].bufferA = samples[sample].dad->data;
		channels[channel].bufferB = samples[sample].dad->data + STREAM_CHUNK;
		DGTL_play(channel, channels[channel].bufferA, STREAM_CHUNK,
			channels[channel].rate, channels[channel].volume);
	}
	else
	{
		channels[channel].chunksLeft = 0;
		DGTL_play(channel, samples[sample].dad->data, samples[sample].dad->size,
			channels[channel].rate, channels[channel].volume);
	}
	return channels[channel].handle;
}

void SndMgr::rewind(int32_t handle)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS && channels[i].handle != handle; i++)
		;
	if (i < NUM_CHANNELS)
	{
		sample_report(channels[i].handle, 0);
		sample_report(channels[i].handle, 1);
		DGTL_play(i, samples[channels[i].sample].dad->data, samples[channels[i].sample].dad->size,
			samples[channels[i].sample].dad->rate, channels[i].volume);
	}
}

void SndMgr::stop(int32_t handle)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS && channels[i].handle != handle; i++)
		;
	if (i < NUM_CHANNELS)
	{
		sample_report(channels[i].handle, 0);
		stop_channel(i);
	}
}

int32_t SndMgr::play_ambient(int16_t sample, int16_t priority, int16_t volume, int16_t pitch)
{
	if (samples[sample].users == 0)
	{
		return 0;
	}
	int16_t channel = get_channel(sample, priority);
	if (channel == -1)
	{
		return 0;
	}
	activeChannels++;
	channels[channel].handle = ++lastHandle;
	channels[channel].sample = sample;
	channels[channel].priority = priority;
	channels[channel].volume = volume;
	channels[channel].rate = samples[sample].dad->rate - pitch;
	channels[channel].ambient = 1;
	channels[channel].streamed = 0;
	channels[channel].needLoad = 0;
	sample_report(channels[channel].handle, 1);
	DGTL_play_ambient(channel, samples[sample].dad->data, samples[sample].dad->size,
		samples[sample].dad->f_08, samples[sample].dad->f_0c,
		channels[channel].rate, channels[channel].volume);
	return channels[channel].handle;
}

void SndMgr::release_ambient(int32_t handle)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS && channels[i].handle != handle; i++)
		;
	if (i < NUM_CHANNELS)
	{
		if (channels[i].ambient == 0)
		{
			return;
		}
		sample_report(channels[i].handle, 2);
		DGTL_release(i);
		return;
	}
	else if (numSuspended)
	{
		for (i = 0; i < maxSuspended && suspended[i].handle != handle; i++)
			;
		if (i < maxSuspended)
		{
			sample_report(suspended[i].handle, 0);
			numSuspended--;
			suspended[i].handle = 0;
		}
	}
}

void SndMgr::stop_ambient(int32_t handle)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS && channels[i].handle != handle; i++)
		;
	if (i < NUM_CHANNELS)
	{
		sample_report(channels[i].handle, 0);
		stop_channel(i);
		return;
	}
	if (numSuspended)
	{
		for (i = 0; i < maxSuspended && suspended[i].handle != handle; i++)
			;
		if (i < maxSuspended)
		{
			sample_report(suspended[i].handle, 0);
			numSuspended--;
			suspended[i].handle = 0;
		}
	}
}

int16_t SndMgr::get_sample_number(int32_t handle)
{
	for (int16_t i = 0; i < NUM_CHANNELS; i++)
	{
		if (channels[i].handle == handle)
		{
			return channels[i].sample;
		}
	}
	return -1;
}

int32_t SndMgr::get_sample_handle(int16_t sample)
{
	for (int16_t i = 0; i < NUM_CHANNELS; i++)
	{
		if (channels[i].sample == sample)
		{
			return channels[i].handle;
		}
	}
	return 0xFFFF;
}

void SndMgr::stop_all(void)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS; i++)
	{
		if (channels[i].handle)
		{
			sample_report(channels[i].handle, 0);
			stop_channel(i);
		}
	}
	for (i = 0; i < maxSuspended; i++)
	{
		if (suspended[i].handle)
		{
			sample_report(suspended[i].handle, 0);
			numSuspended--;
			suspended[i].handle = 0;
		}
	}
}

void SndMgr::flush(void)
{
	stop_all();
	memset(suspended, 0, maxSuspended * sizeof(SoundChannel));
	memset(channels, 0, sizeof(channels));
	for (int16_t i = 0; i < sampleCount; i++)
	{
		if (samples[i].users)
		{
			memoryUsed -= samples[i].dad->size + 32;
			if (samples[i].dad)
			{
				delete samples[i].dad;
				samples[i].dad = 0;
			}
			samples[i].users = 0;
		}
	}
}

void SndMgr::set_volume(int32_t handle, int16_t volume)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS && channels[i].handle != handle; i++)
		;
	if (i < NUM_CHANNELS)
	{
		DGTL_channel_volume(i, volume);
		channels[i].volume = volume;
		return;
	}
	if (numSuspended)
	{
		for (i = 0; i < maxSuspended && suspended[i].handle != handle; i++)
			;
		if (i < maxSuspended)
		{
			suspended[i].volume = volume;
		}
	}
}

void SndMgr::set_pitch(int32_t &handle, int16_t rate)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS && channels[i].handle != handle; i++)
		;
	if (i < NUM_CHANNELS)
	{
		channels[i].rate = rate;
		DGTL_channel_rate(i, channels[i].rate);
		return;
	}
	if (numSuspended)
	{
		for (i = 0; i < maxSuspended && suspended[i].handle != handle; i++)
			;
		if (i < maxSuspended)
		{
			suspended[i].rate = rate;
		}
	}
}

void SndMgr::change_pitch(int32_t &handle, int16_t delta)
{
	int16_t i;

	for (i = 0; i < NUM_CHANNELS && channels[i].handle != handle; i++)
		;
	if (i < NUM_CHANNELS)
	{
		channels[i].rate += delta;
		DGTL_channel_rate(i, channels[i].rate);
		return;
	}
	if (numSuspended)
	{
		for (i = 0; i < maxSuspended && suspended[i].handle != handle; i++)
			;
		if (i < maxSuspended)
		{
			suspended[i].rate += delta;
		}
	}
}

void SndMgr::update(void)
{
	DAC_update();
}

// Gives free channels back to suspended ambient sounds.
void SndMgr::update_ambient(void)
{
	int16_t i = 0;

	while (activeChannels < NUM_CHANNELS && numSuspended > 0)
	{
		if (channels[i].handle == 0)
		{
			resume_ambient(i);
		}
		i++;
		if (i == NUM_CHANNELS)
		{
			break;
		}
	}
}

// Loads the next chunk of each streamed sample into the buffer not playing.
void SndMgr::update_huge_samples(void)
{
	int16_t i;
	char *buffer;

	for (i = 0; i < NUM_CHANNELS; i++)
	{
		if (channels[i].needLoad)
		{
			if (channels[i].playB)
			{
				buffer = channels[i].bufferA;
			}
			else
			{
				buffer = channels[i].bufferB;
			}
			if (channels[i].chunksLeft == 2)
			{
				sample_load_chunk(channels[i].sample, buffer, channels[i].lastChunkSize, channels[i].filePos);
			}
			else
			{
				sample_load_chunk(channels[i].sample, buffer, STREAM_CHUNK, channels[i].filePos);
			}
			channels[i].playB = !channels[i].playB;
			channels[i].filePos += STREAM_CHUNK;
			channels[i].chunksLeft--;
			channels[i].needLoad = 0;
		}
	}
}

// Finds a free channel, or takes the one with the lowest priority if it is
// not above the new sound's.
int16_t SndMgr::get_channel(int16_t sample, int16_t priority)
{
	int16_t i;

	if (activeChannels < NUM_CHANNELS)
	{
		if (samples[sample].users == 0 && load(sample, 0) == 0)
		{
			return -1;
		}
		for (i = 0; i < NUM_CHANNELS && channels[i].handle; i++)
			;
		if (i >= NUM_CHANNELS)
		{
			Fatal("Play count mismatch, pc=%u", activeChannels);
		}
	}
	else
	{
		i = 0;
		for (int16_t j = 1; j < NUM_CHANNELS; j++)
		{
			if (channels[j].priority < channels[i].priority)
			{
				i = j;
			}
		}
		if (channels[i].priority <= priority)
		{
			if (samples[sample].users == 0 && load(sample, 0) == 0)
			{
				return -1;
			}
			if (channels[i].ambient == 1)
			{
				suspend_ambient(i);
			}
			else
			{
				sample_report(channels[i].handle, 0);
			}
			stop_channel(i);
		}
		else
		{
			return -1;
		}
	}
	return i;
}

void SndMgr::suspend_ambient(int16_t channel)
{
	int16_t i;

	if (numSuspended == maxSuspended)
	{
		// The original reads channels[i] before i is set.
		sample_report(channels[i].handle, 0);
		return;
	}
	for (i = 0; i < maxSuspended && suspended[i].handle; i++)
		;
	if (i < maxSuspended)
	{
		numSuspended++;
		suspended[i] = channels[channel];
		sample_report(suspended[i].handle, 3);
	}
}

void SndMgr::resume_ambient(int16_t channel)
{
	int16_t i;

	for (i = 0; i < maxSuspended && !suspended[i].handle; i++)
		;
	if (i < maxSuspended)
	{
		numSuspended--;
		channels[channel] = suspended[i];
		suspended[i].handle = 0;
		activeChannels++;
		sample_report(channels[channel].handle, 4);
		DGTL_play_ambient(channel, samples[channels[channel].sample].dad->data,
			samples[channels[channel].sample].dad->size,
			samples[channels[channel].sample].dad->f_08,
			samples[channels[channel].sample].dad->f_0c,
			channels[channel].rate, channels[channel].volume);
	}
}

// Called by the driver when a channel finishes its buffer.
void digital_call_back(int16_t channel)
{
	if (SoundManager->channels[channel].streamed)
	{
		if (SoundManager->channels[channel].chunksLeft)
		{
			char *buffer;
			if (SoundManager->channels[channel].playB)
			{
				buffer = SoundManager->channels[channel].bufferB;
			}
			else
			{
				buffer = SoundManager->channels[channel].bufferA;
			}
			if (SoundManager->channels[channel].chunksLeft > 1)
			{
				SoundManager->channels[channel].needLoad = 1;
				DGTL_play(channel, buffer, STREAM_CHUNK,
					SoundManager->channels[channel].rate, SoundManager->channels[channel].volume);
				return;
			}
			DGTL_play(channel, buffer, SoundManager->channels[channel].lastChunkSize,
				SoundManager->channels[channel].rate, SoundManager->channels[channel].volume);
			SoundManager->channels[channel].chunksLeft = 0;
			return;
		}
		sample_report(SoundManager->channels[channel].handle, 0);
		SoundManager->activeChannels--;
		SoundManager->channels[channel].handle = 0;
		return;
	}
	sample_report(SoundManager->channels[channel].handle, 0);
	SoundManager->activeChannels--;
	SoundManager->channels[channel].handle = 0;
}
