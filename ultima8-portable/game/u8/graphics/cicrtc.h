#ifndef CICRTC_H
#define CICRTC_H

#include "u8port.h"

void InitializeCrtc(void);

extern uint16_t CrtcStatusPort;
extern uint16_t CrtcAddressRegister;
extern uint16_t CrtcDataRegister;
extern uint16_t BiosSegment;

#endif
