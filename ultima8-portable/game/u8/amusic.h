#ifndef AMUSIC_H
#define AMUSIC_H

#include "u8port.h"

#include "process.h"
#include "cintlist.h"
#include "fastflex.h"
#include "ail.h"

// What XMI_describe_driver points to.
struct drvr_desc
{
	uint16_t min_API_version;
	uint16_t drvr_type;
	char data_suffix[4];
	void *dev_name_table;
	int16_t default_IO;
	int16_t default_IRQ;
	int16_t default_DMA;
	int16_t default_DRQ;
	int16_t service_rate;
	uint16_t display_size;
};

// One playing XMIDI sequence.
class current_type
{
public:
	int16_t measure;
	int16_t restart;
	int16_t file;
	int16_t handle;
	char *st_ptr;
	int16_t active;
};

// One music file listed in the .NIM file.
class file_info_type
{
public:
	char name[80];
	char *data;
	char tune;
	char pad_55;
	int16_t loop_measures;
	int16_t restart_measure;
};

// What each tune key is bound to.
class key_type
{
public:
	int16_t file;
	int16_t is_tune;
	int16_t used;
	int16_t pad_6[3];
};

class Music
{
public:
	static char skip_blanks(BaseFile *);
	static void skip_to_eol(BaseFile *);
	static int16_t read_to_white(BaseFile *, char *);
	static int16_t load_timbres(file_info_type *, int16_t, uint8_t *, uint8_t *);
	static int16_t first_section(file_info_type *);
	static char *second_section(void);
	static int16_t third_section(void);
	static int16_t do_data_line(BaseFile *, int16_t, int16_t (*)[100]);
	static int16_t fourth_section(file_info_type *, int16_t);
	static int16_t fifth_section(file_info_type *, int16_t);
	static char *do_files(file_info_type *, int16_t *, char *);
	static char *load_global_timbre(uint16_t, uint16_t);
	static void Music_halt(char *, int16_t, int16_t, char *);
	static int16_t init(char *, int16_t, int16_t, int16_t, int16_t);
	static int16_t play(int16_t);
	static int16_t resume(void);
	static int16_t pause(void);
	static void stop(void);
	static int16_t xwait_for_eom(current_type *);
	static int16_t end_measure(current_type);
	static int16_t do_normal(int16_t);
	static int16_t init_AIL(int32_t *);
	static void music_hook(void);
	static void slow_stop(void);
	static int16_t pushQueue(int16_t);
	static void Music_Timer(void);
	static void Music_Timer2(void);
	static int16_t update_music(void);
	static void deinit(void);
	static void reset(void);
	static int16_t status(void);
	static void volume(int16_t, int16_t);
	static char *Activate_Music_File(int16_t);
	static void SpeedHack(current_type *);
	static int16_t speedhack;
	static int16_t music_volume;
	static char *Music_Buffer[2];
	static Index *Music_Index_Ptr;
	static int16_t last_read[2];
	static uint8_t hdriver[2];
	static int32_t state_table_size;
	static int16_t old_measure;
	static int16_t new_measure;
	static int16_t (*trans_table)[100];
	static int16_t (*table_table)[256];
	static int16_t desired_tune;
	static int16_t current_tune;
	static int16_t initialized;
	static int16_t hooked;
	static int16_t num_ms;
	static int16_t music_status;
	static int16_t ref_meas;
	static int16_t music_play_state;
	static int16_t music_stop_state;
	static int16_t slow_stop_state;
	static int16_t mpu_401_device;
	static int16_t auto_detect;
	static int16_t music_dma;
	static int16_t music_irq;
	static int16_t music_drq;
	static int16_t music_port;
	static char music_branch[256];
	static FastFlex Music_Flex;
	static Index Music_Index;
	static char *trans_file_ptr;
	static int16_t play_queue;
	static file_info_type *ms_files;
	static key_type *key_array;
	static current_type current_ms;
	static current_type current_other;
	static current_type *current_temp;
	static current_type a_seq;
};

class MusicProcess : public ProcessRealTime
{
public:
	MusicProcess(int16_t);
	virtual void process(void);
	virtual void freeMemory(void);
	virtual void interruptHandler(uint32_t, uint32_t);
};

void musicVolume(int16_t, int16_t);
void musicPlay(int16_t);
void musicDeinit(void);
void musicStatus(void);
void musicPause(void);
void musicResume(void);
void musicStop(void);
void musicSlowStop(void);
void musicReset(void);
void stackPlay(int16_t);
void musicInit(char *, int16_t, int16_t, int16_t, int16_t);

extern void *TheMusicProcess;

#endif
