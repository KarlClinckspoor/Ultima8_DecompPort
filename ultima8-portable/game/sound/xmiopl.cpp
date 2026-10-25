// The AdLib side of the XMIDI driver (A16SBFM.DLL on DOS, the Miles Sound Blaster FM
// driver); see xmidrv.h.
//
// The nine two-operator voices of an OPL2 play the notes. Timbres come from the game's global
// timbre library (MUSIC.FLX record 103h, XMI_install_timbre): a length word (14), a signed
// transposition, then the registers 20h, 40h, 60h, 80h, E0h of the modulator, C0h, and the
// same five of the carrier. A program change takes the timbre of the channel's patch bank
// (controller 114) and program; MIDI channel 10 is the percussion channel, where each note
// is a timbre of bank 127 of its own, played at the note its transposition names. As in the
// Miles drivers, velocity is scaled coarsely (16 steps from 82 to 127), volume and
// expression scale the carrier (and the modulator of an additive timbre), modulation of 64
// or more switches on vibrato, and the bender covers 12 semitones each way.

#include "u8port.h"
#include "plat.h"

#include "xmidrv.h"

#define VOICES 9
#define MAX_TIMBRES 256
#define PERCUSSION_CHANNEL 9
#define PERCUSSION_BANK 127
#define BEND_RANGE 12
#define OPL_PORT 0x388

struct Timbre
{
	uint8_t used;
	uint8_t bank;
	uint8_t patch;
	int8_t transpose;
	uint8_t modulator[5];	// 20h 40h 60h 80h E0h
	uint8_t feedback;	// C0h
	uint8_t carrier[5];
};

struct Channel
{
	const Timbre *timbre;
	uint8_t bank;
	uint8_t volume;
	uint8_t expression;
	uint8_t modulation;
	uint8_t sustain;
	uint8_t protect;	// voice protect, controller 112
	uint16_t bend;
};

struct Voice
{
	uint8_t used;
	uint8_t channel;
	uint8_t note;		// as the MIDI note on named it
	int16_t playNote;	// the note sounded, transposition included
	uint8_t velocity;	// scaled
	uint8_t sustained;
	uint32_t age;
	const Timbre *timbre;
	uint8_t b0;		// last B0h value
};

static Timbre timbres[MAX_TIMBRES];
static Channel channels[16];
static Voice voices[VOICES];
static uint32_t noteCounter;

// Operator register offsets of each voice: the modulator's, the carrier's is 3 more.
static const uint8_t operatorOffset[VOICES] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12};

static void opl(uint8_t reg, uint8_t value)
{
	plat_opl_write(OPL_PORT, reg);
	plat_opl_write(OPL_PORT + 1, value);
}

static const Timbre *findTimbre(uint8_t bank, uint8_t patch)
{
	for (int16_t i = 0; i < MAX_TIMBRES; i++)
		if (timbres[i].used && timbres[i].bank == bank && timbres[i].patch == patch)
			return &timbres[i];
	return 0;
}

static void resetChannels(void)
{
	for (int16_t c = 0; c < 16; c++)
	{
		Channel *ch = &channels[c];

		ch->timbre = 0;
		ch->bank = 0;
		ch->volume = 127;
		ch->expression = 127;
		ch->modulation = 0;
		ch->sustain = 0;
		ch->protect = 0;
		ch->bend = 0x2000;
	}
}

static void init(void)
{
	if (!plat_opl_available())
		return;
	opl(0x01, 0x20);	// waveform select
	opl(0x08, 0x00);
	opl(0xBD, 0x00);	// melodic mode
	for (uint8_t v = 0; v < VOICES; v++)
	{
		opl((uint8_t)(0xB0 + v), 0);
		opl((uint8_t)(0x40 + operatorOffset[v]), 0x3F);
		opl((uint8_t)(0x43 + operatorOffset[v]), 0x3F);
	}
	memset(voices, 0, sizeof voices);
	resetChannels();
}

static void shutdown(const char *)
{
	for (uint8_t v = 0; v < VOICES; v++)
		opl((uint8_t)(0xB0 + v), 0);
	memset(voices, 0, sizeof voices);
}

// 0..127 from the channel's volume and expression and the note's velocity.
static uint16_t loudness(const Voice *v)
{
	const Channel *ch = &channels[v->channel];
	uint16_t level = (uint16_t)(ch->volume * ch->expression * 2) >> 8;

	if (level)
		level++;
	level = (uint16_t)(level * v->velocity * 2) >> 8;
	if (level)
		level++;
	return level > 127 ? 127 : level;
}

static uint8_t scaledLevel(uint8_t reg40, uint16_t level)
{
	uint16_t output = (uint16_t)((~reg40 & 0x3F) * level / 127);

	return (uint8_t)((reg40 & 0xC0) | (~output & 0x3F));
}

static void setVolume(uint8_t n)
{
	const Voice *v = &voices[n];
	uint16_t level = loudness(v);
	uint8_t mod = v->timbre->modulator[1];

	if (v->timbre->feedback & 1)	// additive: the modulator is heard too
		mod = scaledLevel(mod, level);
	opl((uint8_t)(0x40 + operatorOffset[n]), mod);
	opl((uint8_t)(0x43 + operatorOffset[n]), scaledLevel(v->timbre->carrier[1], level));
}

static void setCharacter(uint8_t n)
{
	const Voice *v = &voices[n];
	uint8_t vibrato = channels[v->channel].modulation >= 64 ? 0x40 : 0;

	opl((uint8_t)(0x20 + operatorOffset[n]), (uint8_t)(v->timbre->modulator[0] | vibrato));
	opl((uint8_t)(0x23 + operatorOffset[n]), (uint8_t)(v->timbre->carrier[0] | vibrato));
}

static void setTimbre(uint8_t n)
{
	const Voice *v = &voices[n];
	uint8_t op = operatorOffset[n];

	setCharacter(n);
	setVolume(n);
	opl((uint8_t)(0x60 + op), v->timbre->modulator[2]);
	opl((uint8_t)(0x63 + op), v->timbre->carrier[2]);
	opl((uint8_t)(0x80 + op), v->timbre->modulator[3]);
	opl((uint8_t)(0x83 + op), v->timbre->carrier[3]);
	opl((uint8_t)(0xE0 + op), v->timbre->modulator[4]);
	opl((uint8_t)(0xE3 + op), v->timbre->carrier[4]);
	opl((uint8_t)(0xC0 + n), v->timbre->feedback);
}

// Starts (or retunes) the voice's note: the OPL's block and F-number for its pitch.
static void setFrequency(uint8_t n, uint8_t keyOn)
{
	Voice *v = &voices[n];
	double semitones = v->playNote + (channels[v->channel].bend - 0x2000) * (double)BEND_RANGE / 0x2000;
	double hz;
	uint16_t fnum = 0;
	uint8_t block;

	while (semitones < 12)
		semitones += 12;
	while (semitones > 107)
		semitones -= 12;
	hz = 440.0 * pow(2.0, (semitones - 69) / 12.0);
	for (block = 0; block < 8; block++)
	{
		double f = hz * (double)(1 << (20 - block)) / 49716.0;
		if (f < 1023.5)
		{
			fnum = (uint16_t)(f + 0.5);
			break;
		}
	}
	if (block == 8)
	{
		block = 7;
		fnum = 1023;
	}
	v->b0 = (uint8_t)((fnum >> 8 & 3) | block << 2 | (keyOn ? 0x20 : 0));
	opl((uint8_t)(0xA0 + n), (uint8_t)fnum);
	opl((uint8_t)(0xB0 + n), v->b0);
}

static void release(uint8_t n)
{
	Voice *v = &voices[n];

	v->used = 0;
	v->sustained = 0;
	opl((uint8_t)(0xB0 + n), (uint8_t)(v->b0 & ~0x20));
}

// A free voice, else the one sounding longest on a channel without voice protection (or
// longest of all).
static uint8_t allocate(void)
{
	int16_t best = -1;

	for (uint8_t n = 0; n < VOICES; n++)
		if (!voices[n].used)
			return n;
	for (int pass = 0; pass < 2 && best < 0; pass++)
		for (uint8_t n = 0; n < VOICES; n++)
		{
			if (pass == 0 && channels[voices[n].channel].protect >= 64)
				continue;
			if (best < 0 || voices[n].age < voices[best].age)
				best = n;
		}
	release((uint8_t)best);
	return (uint8_t)best;
}

static void noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
	const Timbre *t;
	uint8_t n;
	Voice *v;

	if (!plat_opl_available())
		return;
	if (channel == PERCUSSION_CHANNEL)
		t = findTimbre(PERCUSSION_BANK, note);
	else
		t = channels[channel].timbre;
	if (t == 0)
		return;
	n = allocate();
	v = &voices[n];
	v->used = 1;
	v->channel = channel;
	v->note = note;
	v->playNote = channel == PERCUSSION_CHANNEL ? t->transpose : (int16_t)(note + t->transpose);
	v->velocity = (uint8_t)(82 + 3 * ((velocity & 0x7F) >> 3));
	v->sustained = 0;
	v->age = ++noteCounter;
	v->timbre = t;
	opl((uint8_t)(0xB0 + n), 0);	// key off before the new timbre
	setTimbre(n);
	setFrequency(n, 1);
}

static void noteOff(uint8_t channel, uint8_t note)
{
	for (uint8_t n = 0; n < VOICES; n++)
	{
		Voice *v = &voices[n];

		if (!v->used || v->channel != channel || v->note != note)
			continue;
		if (channels[channel].sustain >= 64)
			v->sustained = 1;
		else
			release(n);
	}
}

static void control(uint8_t channel, uint8_t controller, uint8_t value)
{
	Channel *ch = &channels[channel];

	switch (controller)
	{
	case 1:		// modulation
		ch->modulation = value;
		for (uint8_t n = 0; n < VOICES; n++)
			if (voices[n].used && voices[n].channel == channel)
				setCharacter(n);
		return;
	case 7:		// volume
	case 11:	// expression
		if (controller == 7)
			ch->volume = value;
		else
			ch->expression = value;
		for (uint8_t n = 0; n < VOICES; n++)
			if (voices[n].used && voices[n].channel == channel)
				setVolume(n);
		return;
	case 64:	// sustain
		ch->sustain = value;
		if (value < 64)
			for (uint8_t n = 0; n < VOICES; n++)
				if (voices[n].used && voices[n].channel == channel && voices[n].sustained)
					release(n);
		return;
	case 112:	// voice protect
		ch->protect = value;
		return;
	case 114:	// patch bank select
		ch->bank = value;
		return;
	case 121:	// reset controllers
		ch->volume = 127;
		ch->expression = 127;
		ch->modulation = 0;
		ch->sustain = 0;
		ch->bend = 0x2000;
		return;
	case 120:	// all sound off
	case 123:	// all notes off
		for (uint8_t n = 0; n < VOICES; n++)
			if (voices[n].used && voices[n].channel == channel)
				release(n);
		return;
	default:
		return;
	}
}

// A patch with no timbre installed leaves the channel's timbre as it was, as in Miles.
static void programChange(uint8_t channel, uint8_t patch)
{
	const Timbre *t = findTimbre(channels[channel].bank, patch);

	if (t)
		channels[channel].timbre = t;
}

static void pitchBend(uint8_t channel, uint16_t value)
{
	channels[channel].bend = value;
	for (uint8_t n = 0; n < VOICES; n++)
		if (voices[n].used && voices[n].channel == channel)
			setFrequency(n, 1);
}

static void aftertouch(uint8_t, uint8_t, uint8_t)
{
}

static void sysex(const uint8_t *, uint32_t)
{
}

static void installTimbre(uint8_t bank, uint8_t patch, const uint8_t *data)
{
	Timbre *t = (Timbre *)findTimbre(bank, patch);
	uint16_t length = (uint16_t)(data[0] | data[1] << 8);

	if (length < 14)
		return;
	if (t == 0)
	{
		for (int16_t i = 0; i < MAX_TIMBRES && t == 0; i++)
			if (!timbres[i].used)
				t = &timbres[i];
		if (t == 0)
			return;
	}
	t->used = 1;
	t->bank = bank;
	t->patch = patch;
	t->transpose = (int8_t)data[2];
	memcpy(t->modulator, data + 3, 5);
	t->feedback = data[8];
	memcpy(t->carrier, data + 9, 5);
}

static uint8_t hasTimbre(uint8_t bank, uint8_t patch)
{
	return findTimbre(bank, patch) != 0;
}

const XmiSynth xmiOpl = {
	"AD", init, shutdown, noteOn, noteOff, control, programChange, pitchBend, aftertouch, sysex,
	installTimbre, hasTimbre
};
