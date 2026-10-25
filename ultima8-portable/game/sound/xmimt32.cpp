// The MT-32 side of the XMIDI driver (A16MT32.DLL on DOS); see xmidrv.h.
//
// Notes, controllers and programs go to the MT-32 as they are. Besides the controllers the
// sequencer takes, the Miles MT-32 driver gave some XMIDI controllers meanings of its own,
// kept here: 114 selects the patch bank, 58-63 set a patch's reverb and bender range and the
// system reverb, and 32-46 build SysEx messages a byte at a time, in three queues.

#include "u8port.h"
#include "plat.h"

#include "xmidrv.h"

#define PATCH_REVERB 59
#define PATCH_BENDER 60
#define REVERB_MODE 61
#define REVERB_TIME 62
#define REVERB_LEVEL 63
#define SYSEX_FIRST 32
#define SYSEX_LAST 46
#define SYSEX_QUEUE_SIZE 32

struct SysexQueue
{
	uint32_t address;	// 21 bits: three 7-bit address bytes
	uint8_t data[SYSEX_QUEUE_SIZE];
	uint8_t used;
};

static uint8_t program[16];
static uint8_t bank[16];
static SysexQueue queues[3];

static void send(uint8_t a, uint8_t b, uint8_t c, int16_t length)
{
	uint8_t m[3] = {a, b, c};

	plat_midi_send(m, length);
}

// A Roland DT1 message: data to an MT-32 address.
static void write(uint32_t address, const uint8_t *data, uint16_t length)
{
	uint8_t message[SYSEX_QUEUE_SIZE + 16];
	uint8_t sum = 0;
	uint16_t n = 0;

	if (length > SYSEX_QUEUE_SIZE)
		length = SYSEX_QUEUE_SIZE;
	message[n++] = 0xF0;
	message[n++] = 0x41;	// Roland
	message[n++] = 0x10;	// device 17
	message[n++] = 0x16;	// MT-32
	message[n++] = 0x12;	// DT1
	message[n++] = (uint8_t)(address >> 14 & 0x7F);
	message[n++] = (uint8_t)(address >> 7 & 0x7F);
	message[n++] = (uint8_t)(address & 0x7F);
	for (uint16_t i = 0; i < length; i++)
		message[n++] = data[i] & 0x7F;
	for (uint16_t i = 5; i < n; i++)
		sum = (uint8_t)(sum + message[i]);
	message[n++] = (uint8_t)((128 - (sum & 0x7F)) & 0x7F);
	message[n++] = 0xF7;
	plat_midi_send(message, (int16_t)n);
}

// MT-32 addresses as 7-bit groups.
static uint32_t address(uint8_t a, uint8_t b, uint8_t c)
{
	return (uint32_t)a << 14 | (uint32_t)b << 7 | c;
}

// The Miles MT-32 driver's set-up: parts 1-8 and rhythm on MIDI channels 2-10, the partials
// shared out 3/4 between the parts (the power-on table leaves parts 6-8 none), a quieter
// reverb, and each channel at full volume, panned centre.
static void init(void)
{
	static const uint8_t channels[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	static const uint8_t partials[9] = {3, 4, 3, 4, 3, 4, 3, 4, 4};
	static const uint8_t reverb[3] = {0, 3, 2};	// mode, time, level

	for (uint8_t c = 0; c < 16; c++)
	{
		program[c] = 0;
		bank[c] = 0;
		send((uint8_t)(0xB0 | c), 123, 0, 3);	// all notes off
	}
	memset(queues, 0, sizeof queues);
	write(address(0x10, 0, 0x0D), channels, 9);
	write(address(0x10, 0, 0x04), partials, 9);
	write(address(0x10, 0, 0x01), reverb, 3);
	for (uint8_t c = 1; c < 10; c++)
	{
		send((uint8_t)(0xB0 | c), 7, 127, 3);	// volume
		send((uint8_t)(0xB0 | c), 10, 64, 3);	// pan
	}
}

// The message shows on the MT-32's display, as the driver showed it when the game ended.
static void shutdown(const char *message)
{
	uint8_t text[20];
	uint16_t i;

	for (uint8_t c = 0; c < 16; c++)
		send((uint8_t)(0xB0 | c), 123, 0, 3);
	if (message == 0)
		return;
	for (i = 0; i < 20; i++)
		text[i] = ' ';
	for (i = 0; i < 20 && message[i]; i++)
		text[i] = (uint8_t)message[i];
	write(address(0x20, 0, 0), text, 20);
}

static void noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
	send((uint8_t)(0x90 | channel), note, velocity, 3);
}

static void noteOff(uint8_t channel, uint8_t note)
{
	send((uint8_t)(0x80 | channel), note, 0, 3);
}

// One of the SysEx-building controllers: within each queue's five, the three address bytes,
// a data byte, and a final data byte that sends the message.
static void sysexController(uint8_t controller, uint8_t value)
{
	uint8_t n = (uint8_t)(controller - SYSEX_FIRST);
	SysexQueue *q = &queues[n / 5];
	uint8_t send = 0;

	switch (n % 5)
	{
	case 0:
		q->address = (q->address & 0x003FFF) | (uint32_t)value << 14;
		return;
	case 1:
		q->address = (q->address & 0x1FC07F) | (uint32_t)value << 7;
		return;
	case 2:
		q->address = (q->address & 0x1FFF80) | value;
		return;
	case 3:
		if (q->used < SYSEX_QUEUE_SIZE)
		{
			q->data[q->used++] = value;
			send = q->used >= SYSEX_QUEUE_SIZE;
		}
		break;
	case 4:
		// The last byte goes out with the rest; a later final byte rewrites it.
		if (q->used < SYSEX_QUEUE_SIZE)
		{
			q->data[q->used] = value;
			write(q->address, q->data, (uint16_t)(q->used + 1));
			q->address = (q->address + q->used) & 0x1FFFFF;
			q->used = 0;
		}
		return;
	}
	if (send)
	{
		write(q->address, q->data, q->used);
		q->address = (q->address + q->used) & 0x1FFFFF;
		q->used = 0;
	}
}

static void control(uint8_t channel, uint8_t controller, uint8_t value)
{
	if (controller >= SYSEX_FIRST && controller <= SYSEX_LAST)
	{
		sysexController(controller, value);
		return;
	}
	switch (controller)
	{
	case 114:	// patch bank select
		bank[channel] = value;
		return;
	case 110:	// channel lock
	case 111:	// channel lock protect
	case 112:	// voice protect
	case 113:	// timbre protect
	case 115:	// indirect controller prefix
		return;
	case PATCH_REVERB:
	case PATCH_BENDER:
		// Patch memory (05 00 00): eight bytes a patch; the bender range is byte 4 and the
		// reverb switch byte 6. The program is sent again so the part takes them up.
		{
			uint8_t v = value;
			uint16_t at = (uint16_t)(program[channel] * 8 + (controller == PATCH_REVERB ? 6 : 4));

			write(address(0x05, (uint8_t)(at >> 7), (uint8_t)(at & 0x7F)), &v, 1);
			send((uint8_t)(0xC0 | channel), program[channel], 0, 2);
		}
		return;
	case REVERB_MODE:
	case REVERB_TIME:
	case REVERB_LEVEL:
		{
			uint8_t v = value;
			write(address(0x10, 0, (uint8_t)(controller - REVERB_MODE + 1)), &v, 1);
		}
		return;
	case 58:	// rhythm key timbre: for custom timbres, which U8 does not install on the MT-32
		return;
	default:
		send((uint8_t)(0xB0 | channel), controller, value, 3);
		return;
	}
}

static void programChange(uint8_t channel, uint8_t patch)
{
	program[channel] = patch;
	send((uint8_t)(0xC0 | channel), patch, 0, 2);
}

static void pitchBend(uint8_t channel, uint16_t value)
{
	send((uint8_t)(0xE0 | channel), (uint8_t)(value & 0x7F), (uint8_t)(value >> 7 & 0x7F), 3);
}

static void aftertouch(uint8_t status, uint8_t a, uint8_t b)
{
	send(status, a, b, (status & 0xF0) == 0xD0 ? 2 : 3);
}

static void sysex(const uint8_t *data, uint32_t length)
{
	plat_midi_send(data, (int16_t)length);
}

// U8 puts its MT-32 timbres in the music's own SysEx; it asks the driver for none.
static void installTimbre(uint8_t, uint8_t, const uint8_t *)
{
}

static uint8_t hasTimbre(uint8_t, uint8_t)
{
	return 1;
}

const XmiSynth xmiMt32 = {
	"MT", init, shutdown, noteOn, noteOff, control, programChange, pitchBend, aftertouch, sysex,
	installTimbre, hasTimbre
};
