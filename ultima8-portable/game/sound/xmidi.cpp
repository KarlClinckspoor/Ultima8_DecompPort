// The XMIDI sequencer and the XMI_* interface of AILXMI.DLL; see xmidrv.h.
//
// An XMIDI file is an IFF FORM XDIR (an INFO chunk with the number of sequences) followed by
// a CAT of FORM XMID, one per sequence, each with an optional TIMB chunk (the timbres it
// uses, patch and bank), an optional RBRN chunk (branch points: marker, offset into the
// events) and its EVNT chunk. Events are MIDI messages without running status; a note on
// carries its own duration (a variable-length number of intervals) and there are no note
// offs; bytes below 80h between events are intervals to wait. An interval is 1/120 s at
// 100% tempo.
//
// U8 calls XMI_SERVE_DRIVER from the kernel's real-time MusicProcess, at the driver's
// service rate (or 60 Hz with Music::speedhack, which doubles the tempo to make up for it).

#include "u8port.h"
#include "plat.h"

#include "ail.h"
#include "amusic.h"
#include "xmidrv.h"

#define MAX_SEQUENCES 8
#define MAX_NOTES 96
#define MAX_LOOPS 4
#define INTERVAL_HZ 120
#define SERVICE_RATE 120

#define SEQ_STOPPED 0
#define SEQ_PLAYING 1
#define SEQ_DONE 2

// XMIDI controllers the sequencer itself acts on.
#define XMI_FOR_LOOP 116
#define XMI_NEXT_BREAK 117
#define XMI_CLEAR_BEAT_BAR 118
#define XMI_CALLBACK 119
#define XMI_BRANCH_INDEX 120

struct XmiNote
{
	uint8_t channel;
	uint8_t note;
	int32_t left;	// intervals still to sound
};

struct XmiLoop
{
	uint32_t pos;
	uint8_t count;	// 0 for ever
};

struct XmiSequence
{
	uint8_t used;
	uint16_t status;
	const uint8_t *timb;	// patch, bank pairs
	uint16_t timbCount;
	uint16_t timbNext;
	const uint8_t *evnt;
	uint32_t evntSize;
	const uint8_t *rbrn;	// marker word, offset dword
	uint16_t rbrnCount;
	uint32_t pos;
	int32_t delay;
	XmiNote notes[MAX_NOTES];
	int16_t numNotes;
	XmiLoop loops[MAX_LOOPS];
	int16_t loopDepth;
	// relative volume and tempo, in percent, with their ramps (per interval)
	int32_t volume, volumeTarget, volumeStep;
	int32_t tempo, tempoTarget, tempoStep;
	int32_t tempoAccum;
	uint8_t channelVolume[16];
	uint8_t channelUsed[16];
	// beat and measure counting from the tempo and time signature meta events
	uint32_t usPerQuarter;
	uint8_t beatsPerBar;
	uint8_t denominator;	// as a power of two
	int64_t beatAccum;	// in microseconds of music
	int16_t beat;
	int16_t measure;
};

static const XmiSynth *synth = &xmiOpl;
static XmiSequence sequences[MAX_SEQUENCES];
static uint8_t driverReady;

static drvr_desc description = {
	0x0200,		// min_API_version
	4,		// drvr_type: XMIDI
	"AD",
	0,
	0x388, -1, -1, -1,
	SERVICE_RATE,
	0
};

// ---- the file ----

static uint32_t be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint16_t le16(const uint8_t *p)
{
	return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)le16(p) | (uint32_t)le16(p + 2) << 16;
}

// Finds sequence number index in an XMIDI file and fills in its chunks.
static uint8_t findSequence(const uint8_t *form, uint16_t index, XmiSequence *s)
{
	const uint8_t *p = form;
	const uint8_t *end;
	uint16_t count = 1;
	uint16_t n = 0;

	if (memcmp(p, "FORM", 4) != 0)
		return 0;
	if (memcmp(p + 8, "XDIR", 4) == 0)
	{
		uint32_t length = be32(p + 4);
		const uint8_t *q = p + 12;
		const uint8_t *dirEnd = p + 8 + length;

		while (q + 8 <= dirEnd)
		{
			uint32_t chunk = be32(q + 4);
			if (memcmp(q, "INFO", 4) == 0 && chunk >= 2)
				count = le16(q + 8);
			q += 8 + ((chunk + 1) & ~1u);
		}
		p = p + 8 + ((length + 1) & ~1u);
		if (memcmp(p, "CAT ", 4) != 0)
			return 0;
		end = p + 8 + be32(p + 4);
		p += 12;	// "CAT ", length, "XMID"
	}
	else if (memcmp(p + 8, "XMID", 4) == 0)
		end = p + 8 + be32(p + 4);
	else
		return 0;
	if (index >= count)
		return 0;

	// FORM XMID ... in turn
	while (p + 12 <= end)
	{
		uint32_t length;
		const uint8_t *q;
		const uint8_t *formEnd;

		if (memcmp(p, "FORM", 4) != 0 || memcmp(p + 8, "XMID", 4) != 0)
			return 0;
		length = be32(p + 4);
		formEnd = p + 8 + length;
		if (n == index)
		{
			s->timb = 0;
			s->timbCount = 0;
			s->rbrn = 0;
			s->rbrnCount = 0;
			s->evnt = 0;
			for (q = p + 12; q + 8 <= formEnd; )
			{
				uint32_t chunk = be32(q + 4);
				if (memcmp(q, "TIMB", 4) == 0)
				{
					s->timbCount = le16(q + 8);
					s->timb = q + 10;
				}
				else if (memcmp(q, "RBRN", 4) == 0)
				{
					s->rbrnCount = le16(q + 8);
					s->rbrn = q + 10;
				}
				else if (memcmp(q, "EVNT", 4) == 0)
				{
					s->evnt = q + 8;
					s->evntSize = chunk;
				}
				q += 8 + ((chunk + 1) & ~1u);
			}
			return s->evnt != 0;
		}
		p = formEnd + (length & 1);
		n++;
	}
	return 0;
}

// ---- playing ----

static XmiSequence *get(HSEQUENCE h)
{
	if (h >= MAX_SEQUENCES || !sequences[h].used)
		return 0;
	return &sequences[h];
}

static uint32_t readVlq(XmiSequence *s)
{
	uint32_t v = 0;
	uint8_t b;

	do
	{
		if (s->pos >= s->evntSize)
			return v;
		b = s->evnt[s->pos++];
		v = v << 7 | (b & 0x7f);
	} while (b & 0x80);
	return v;
}

static void sendVolume(XmiSequence *s, uint8_t channel)
{
	int32_t v = s->channelVolume[channel] * s->volume / 100;

	synth->control(channel, 7, (uint8_t)(v > 127 ? 127 : v < 0 ? 0 : v));
}

static void notesOff(XmiSequence *s)
{
	for (int16_t i = 0; i < s->numNotes; i++)
		synth->noteOff(s->notes[i].channel, s->notes[i].note);
	s->numNotes = 0;
	for (uint8_t c = 0; c < 16; c++)
		if (s->channelUsed[c])
			synth->control(c, 64, 0);	// sustain off
}

static void resetSequence(XmiSequence *s)
{
	s->pos = 0;
	s->delay = 0;
	s->numNotes = 0;
	s->loopDepth = 0;
	s->tempoAccum = 0;
	s->usPerQuarter = 500000;
	s->beatsPerBar = 4;
	s->denominator = 2;
	s->beatAccum = 0;
	s->beat = 0;
	s->measure = 0;
	for (int16_t c = 0; c < 16; c++)
	{
		s->channelVolume[c] = 127;
		s->channelUsed[c] = 0;
	}
}

static void endSequence(XmiSequence *s)
{
	notesOff(s);
	s->status = SEQ_DONE;
}

static void control(XmiSequence *s, uint8_t channel, uint8_t controller, uint8_t value)
{
	switch (controller)
	{
	case XMI_FOR_LOOP:
		if (s->loopDepth < MAX_LOOPS)
		{
			s->loops[s->loopDepth].pos = s->pos;
			s->loops[s->loopDepth].count = value;
			s->loopDepth++;
		}
		return;
	case XMI_NEXT_BREAK:
		if (s->loopDepth > 0)
		{
			XmiLoop *loop = &s->loops[s->loopDepth - 1];

			if (value < 64)
				s->loopDepth--;	// break
			else if (loop->count == 0 || --loop->count != 0)
				s->pos = loop->pos;
			else
				s->loopDepth--;
		}
		return;
	case XMI_CLEAR_BEAT_BAR:
		s->beat = 0;
		s->measure = 0;
		s->beatAccum = 0;
		return;
	case XMI_CALLBACK:
	case XMI_BRANCH_INDEX:
		return;
	case 7:
		s->channelVolume[channel] = value;
		sendVolume(s, channel);
		return;
	default:
		synth->control(channel, controller, value);
		return;
	}
}

// Plays events until an interval count; returns 0 at the end of the sequence.
static uint8_t playEvents(XmiSequence *s)
{
	while (s->pos < s->evntSize)
	{
		uint8_t status = s->evnt[s->pos];
		uint8_t channel = status & 15;

		if (status < 0x80)
		{
			// intervals to wait: the sum of every such byte in a row
			int32_t wait = 0;
			while (s->pos < s->evntSize && s->evnt[s->pos] < 0x80)
				wait += s->evnt[s->pos++];
			if (wait > 0)
			{
				s->delay = wait;
				return 1;
			}
			continue;
		}
		s->pos++;
		switch (status >> 4)
		{
		case 0x8:
			synth->noteOff(channel, s->evnt[s->pos]);
			s->pos += 2;
			break;
		case 0x9:
		{
			uint8_t note = s->evnt[s->pos];
			uint8_t velocity = s->evnt[s->pos + 1];
			uint32_t duration;

			s->pos += 2;
			duration = readVlq(s);
			s->channelUsed[channel] = 1;
			if (velocity == 0)
			{
				synth->noteOff(channel, note);
				break;
			}
			synth->noteOn(channel, note, velocity);
			if (s->numNotes < MAX_NOTES)
			{
				XmiNote *n = &s->notes[s->numNotes++];
				n->channel = channel;
				n->note = note;
				// a note of no length still sounds for an instant
				n->left = duration ? (int32_t)duration : 1;
			}
			else
				synth->noteOff(channel, note);
			break;
		}
		case 0xA:
			synth->aftertouch(status, s->evnt[s->pos], s->evnt[s->pos + 1]);
			s->pos += 2;
			break;
		case 0xB:
		{
			uint8_t controller = s->evnt[s->pos];
			uint8_t value = s->evnt[s->pos + 1];

			s->pos += 2;
			s->channelUsed[channel] = 1;
			control(s, channel, controller, value);
			break;
		}
		case 0xC:
			s->channelUsed[channel] = 1;
			synth->program(channel, s->evnt[s->pos++]);
			break;
		case 0xD:
			synth->aftertouch(status, s->evnt[s->pos++], 0);
			break;
		case 0xE:
			s->channelUsed[channel] = 1;
			synth->pitchBend(channel, (uint16_t)(s->evnt[s->pos] | s->evnt[s->pos + 1] << 7));
			s->pos += 2;
			break;
		default:
			if (status == 0xFF)
			{
				uint8_t type = s->evnt[s->pos++];
				uint32_t length = readVlq(s);
				const uint8_t *data = s->evnt + s->pos;

				s->pos += length;
				if (type == 0x2F)
					return 0;
				if (type == 0x51 && length == 3)
					s->usPerQuarter = (uint32_t)data[0] << 16 | data[1] << 8 | data[2];
				else if (type == 0x58 && length >= 2)
				{
					s->beatsPerBar = data[0] ? data[0] : 4;
					s->denominator = data[1];
				}
			}
			else if (status == 0xF0 || status == 0xF7)
			{
				uint32_t length = readVlq(s);
				uint8_t message[512];

				if (status == 0xF0 && length + 1 <= sizeof message && s->pos + length <= s->evntSize)
				{
					message[0] = 0xF0;
					memcpy(message + 1, s->evnt + s->pos, length);
					synth->sysex(message, length + 1);
				}
				s->pos += length;
			}
			break;
		}
	}
	return 0;
}

// One interval of the sequence.
static void runInterval(XmiSequence *s)
{
	int16_t i;

	// notes whose time is up
	for (i = 0; i < s->numNotes; )
	{
		if (--s->notes[i].left <= 0)
		{
			synth->noteOff(s->notes[i].channel, s->notes[i].note);
			s->notes[i] = s->notes[--s->numNotes];
		}
		else
			i++;
	}
	// beats and measures: an interval is 1/120 s of music; a beat is a quarter note scaled
	// by the time signature's denominator
	{
		int64_t beatLength = (int64_t)s->usPerQuarter * 4 >> (s->denominator > 6 ? 6 : s->denominator);

		s->beatAccum += 1000000 / INTERVAL_HZ;
		while (beatLength > 0 && s->beatAccum >= beatLength)
		{
			s->beatAccum -= beatLength;
			if (++s->beat >= s->beatsPerBar)
			{
				s->beat = 0;
				s->measure++;
			}
		}
	}
	// ramps
	if (s->volume != s->volumeTarget)
	{
		s->volume += s->volumeStep;
		if ((s->volumeStep > 0 && s->volume > s->volumeTarget) || (s->volumeStep < 0 && s->volume < s->volumeTarget)
			|| s->volumeStep == 0)
			s->volume = s->volumeTarget;
		for (uint8_t c = 0; c < 16; c++)
			if (s->channelUsed[c])
				sendVolume(s, c);
	}
	if (s->tempo != s->tempoTarget)
	{
		s->tempo += s->tempoStep;
		if ((s->tempoStep > 0 && s->tempo > s->tempoTarget) || (s->tempoStep < 0 && s->tempo < s->tempoTarget)
			|| s->tempoStep == 0)
			s->tempo = s->tempoTarget;
	}
	// events
	if (s->delay > 0 && --s->delay > 0)
		return;
	if (!playEvents(s))
		endSequence(s);
}

// ---- the driver's interface ----

// AIL16.ASM, the timer services. U8 registered no AIL timers (the music plays from a kernel
// process that serves the driver), so there is nothing for them to do: AIL_shutdown only
// released timers, and API_timer, which the AilTimer process calls, served them.
void AIL_startup(void)
{
}

void AIL_shutdown(const char *)
{
}

void API_timer(void)
{
}

void XMI_SERVE_DRIVER(void)
{
	for (int16_t h = 0; h < MAX_SEQUENCES; h++)
	{
		XmiSequence *s = &sequences[h];

		if (!s->used || s->status != SEQ_PLAYING)
			continue;
		s->tempoAccum += s->tempo;
		while (s->tempoAccum >= 100 && s->status == SEQ_PLAYING)
		{
			s->tempoAccum -= 100;
			runInterval(s);
		}
	}
}

// Which synth plays: the MT-32 when the player has one (Munt with its ROMs), else the
// AdLib. The DOS game asked AILXMI.DLL, which the installer had chosen.
void *XMI_describe_driver(void)
{
	synth = plat_music_device() == 'r' ? &xmiMt32 : &xmiOpl;
	memcpy(description.data_suffix, synth->suffix, 3);
	description.default_IO = synth == &xmiMt32 ? 0x330 : 0x388;
	return &description;
}

int16_t XMI_detect_device(uint16_t, uint16_t, uint16_t, uint16_t)
{
	return plat_music_device() != 0;
}

void XMI_init_driver(uint16_t, uint16_t, uint16_t, uint16_t)
{
	memset(sequences, 0, sizeof sequences);
	synth->init();
	driverReady = 1;
}

void XMI_shutdown_driver(const char *message)
{
	if (!driverReady)
		return;
	for (int16_t h = 0; h < MAX_SEQUENCES; h++)
		if (sequences[h].used)
			notesOff(&sequences[h]);
	memset(sequences, 0, sizeof sequences);
	synth->shutdown(message);
	driverReady = 0;
}

// The game allocates a state table per sequence; this driver keeps its own.
int16_t XMI_state_table_size(void)
{
	return 16;
}

uint16_t XMI_default_timbre_cache_size(void)
{
	return synth == &xmiOpl ? 2048 : 0;
}

void XMI_define_timbre_cache(void *, uint16_t)
{
}

void XMI_install_timbre(int16_t bank, int16_t patch, void *src)
{
	synth->installTimbre((uint8_t)bank, (uint8_t)patch, (const uint8_t *)src);
}

HSEQUENCE XMI_register_sequence(void *form, uint16_t number, void *, void *)
{
	for (HSEQUENCE h = 0; h < MAX_SEQUENCES; h++)
	{
		XmiSequence *s = &sequences[h];

		if (s->used)
			continue;
		memset(s, 0, sizeof *s);
		if (form == 0 || !findSequence((const uint8_t *)form, number, s))
			return 0xffff;
		s->used = 1;
		s->status = SEQ_STOPPED;
		s->volume = s->volumeTarget = 100;
		s->tempo = s->tempoTarget = 100;
		resetSequence(s);
		return h;
	}
	return 0xffff;
}

void XMI_release_sequence_handle(HSEQUENCE h)
{
	XmiSequence *s = get(h);

	if (s == 0)
		return;
	if (s->status == SEQ_PLAYING)
		notesOff(s);
	s->used = 0;
}

// The next timbre the sequence uses that the driver has not got, as bank << 8 | patch;
// FFFFh when there are no more.
uint16_t XMI_timbre_request(HSEQUENCE h)
{
	XmiSequence *s = get(h);

	if (s == 0)
		return 0xffff;
	while (s->timbNext < s->timbCount)
	{
		uint8_t patch = s->timb[2 * s->timbNext];
		uint8_t bank = s->timb[2 * s->timbNext + 1];

		s->timbNext++;
		if (!synth->hasTimbre(bank, patch))
			return (uint16_t)(bank << 8 | patch);
	}
	return 0xffff;
}

void XMI_start_sequence(HSEQUENCE h)
{
	XmiSequence *s = get(h);

	if (s == 0)
		return;
	if (s->status == SEQ_PLAYING)
		notesOff(s);
	resetSequence(s);
	s->status = SEQ_PLAYING;
}

void XMI_stop_sequence(HSEQUENCE h)
{
	XmiSequence *s = get(h);

	if (s == 0 || s->status != SEQ_PLAYING)
		return;
	notesOff(s);
	s->status = SEQ_STOPPED;
}

void XMI_resume_sequence(HSEQUENCE h)
{
	XmiSequence *s = get(h);

	if (s == 0 || s->status != SEQ_STOPPED)
		return;
	s->status = SEQ_PLAYING;
	for (uint8_t c = 0; c < 16; c++)
		if (s->channelUsed[c])
			sendVolume(s, c);
}

uint16_t XMI_sequence_status(HSEQUENCE h)
{
	XmiSequence *s = get(h);

	return s ? s->status : SEQ_DONE;
}

static void ramp(int32_t &value, int32_t &target, int32_t &step, uint16_t percent, uint16_t milliseconds)
{
	int32_t intervals = (int32_t)milliseconds * INTERVAL_HZ / 1000;

	target = percent;
	if (intervals <= 0)
	{
		value = target;
		step = 0;
		return;
	}
	step = (target - value) / intervals;
	if (step == 0)
		step = target > value ? 1 : -1;
}

void XMI_set_relative_volume(HSEQUENCE h, uint16_t percent, uint16_t milliseconds)
{
	XmiSequence *s = get(h);

	if (s == 0)
		return;
	ramp(s->volume, s->volumeTarget, s->volumeStep, percent, milliseconds);
	for (uint8_t c = 0; c < 16; c++)
		if (s->channelUsed[c])
			sendVolume(s, c);
}

void XMI_set_relative_tempo(HSEQUENCE h, uint16_t percent, uint16_t milliseconds)
{
	XmiSequence *s = get(h);

	if (s == 0)
		return;
	ramp(s->tempo, s->tempoTarget, s->tempoStep, percent, milliseconds);
}

int16_t XMI_measure_count(HSEQUENCE h)
{
	XmiSequence *s = get(h);

	return s ? s->measure : -1;
}

// Moves the sequence to its branch point marker; 0 when it has none.
int16_t XMI_branch_index(HSEQUENCE h, uint16_t marker)
{
	XmiSequence *s = get(h);

	if (s == 0)
		return 0;
	for (uint16_t i = 0; i < s->rbrnCount; i++)
	{
		const uint8_t *entry = s->rbrn + 6 * i;

		if (le16(entry) == marker)
		{
			uint32_t offset = le32(entry + 2);

			if (offset >= s->evntSize)
				return 0;
			notesOff(s);
			s->pos = offset;
			s->delay = 0;
			s->loopDepth = 0;
			return 1;
		}
	}
	return 0;
}
