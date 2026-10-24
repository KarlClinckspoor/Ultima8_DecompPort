// Miles AIL calls: XMIDI driver DLL (AILXMI) and AIL16 timer services. The port's driver
// is sound/xmidi.cpp; the game ran its music from a kernel process, not the AIL timers.
#ifndef AIL_H
#define AIL_H

#include "u8port.h"

typedef uint16_t HSEQUENCE;

void AIL_startup(void);
void AIL_shutdown(const char *signoff_msg);
void XMI_SERVE_DRIVER(void);
void *XMI_describe_driver(void);
int16_t XMI_detect_device(uint16_t io_addr, uint16_t irq, uint16_t dma, uint16_t drq);
void XMI_init_driver(uint16_t io_addr, uint16_t irq, uint16_t dma, uint16_t drq);
void XMI_shutdown_driver(const char *signoff_msg);
int16_t XMI_state_table_size(void);
uint16_t XMI_default_timbre_cache_size(void);
void XMI_define_timbre_cache(void *cache_addr, uint16_t cache_size);
void XMI_install_timbre(int16_t bank, int16_t patch, void *src_addr);
HSEQUENCE XMI_register_sequence(void *form_xmid, uint16_t sequence_num, void *state_table, void *controller_table);
void XMI_release_sequence_handle(HSEQUENCE sequence);
uint16_t XMI_timbre_request(HSEQUENCE sequence);
void XMI_start_sequence(HSEQUENCE sequence);
void XMI_stop_sequence(HSEQUENCE sequence);
void XMI_resume_sequence(HSEQUENCE sequence);
uint16_t XMI_sequence_status(HSEQUENCE sequence);
void XMI_set_relative_volume(HSEQUENCE sequence, uint16_t percent, uint16_t milliseconds);
void XMI_set_relative_tempo(HSEQUENCE sequence, uint16_t percent, uint16_t milliseconds);
int16_t XMI_measure_count(HSEQUENCE sequence);
int16_t XMI_branch_index(HSEQUENCE sequence, uint16_t marker);

#endif
