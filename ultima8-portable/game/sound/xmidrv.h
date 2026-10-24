// The XMIDI driver that took the place of AILXMI.DLL.
//
// U8.EXE loaded the Miles AIL 2.0 XMIDI driver the installer had copied in as AILXMI.DLL (the
// MT-32 driver A16MT32.DLL or the Sound Blaster FM one A16SBFM.DLL) and called it through
// the XMI_* functions of ail.h. Those drivers were 16-bit protected-mode code; this is a
// driver of the same interface written for the host: a sequencer for XMIDI files
// (xmidi.cpp) in front of two synth drivers, the MT-32 through the platform's MIDI output
// (xmimt32.cpp) and the AdLib through its OPL (xmiopl.cpp).
#ifndef XMIDRV_H
#define XMIDRV_H

#include "u8port.h"

// A synth driver: the MIDI messages the sequencer plays, after it has taken out the XMIDI
// controllers that only steer the sequencer.
struct XmiSynth
{
	const char *suffix;	// the driver's timbre file suffix ("AD", "MT")
	void (*init)(void);
	void (*shutdown)(const char *message);
	void (*noteOn)(uint8_t channel, uint8_t note, uint8_t velocity);
	void (*noteOff)(uint8_t channel, uint8_t note);
	void (*control)(uint8_t channel, uint8_t controller, uint8_t value);
	void (*program)(uint8_t channel, uint8_t program);
	void (*pitchBend)(uint8_t channel, uint16_t value);	// 0..3FFFh, 2000h centred
	void (*aftertouch)(uint8_t status, uint8_t a, uint8_t b);
	void (*sysex)(const uint8_t *data, uint32_t length);	// F0 ... F7
	// Timbres: XMI_install_timbre's data, and whether one is there.
	void (*installTimbre)(uint8_t bank, uint8_t patch, const uint8_t *data);
	uint8_t (*hasTimbre)(uint8_t bank, uint8_t patch);
};

extern const XmiSynth xmiMt32;
extern const XmiSynth xmiOpl;

#endif
