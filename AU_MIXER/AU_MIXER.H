#ifndef au_mixer_h
#define au_mixer_h

#include "newfunc\newfunc.h"

typedef mpxp_int32_t PCM_CV_TYPE_I;	// max. 32 bit input/output pcm format (integer)
typedef mpxp_uint32_t PCM_CV_TYPE_UI;	//
typedef mpxp_float_t PCM_CV_TYPE_F;	// 32-bit float part of mixer
typedef mpxp_int16_t PCM_CV_TYPE_S;	// 16-bit integer part of mixer
typedef mpxp_int8_t PCM_CV_TYPE_C;	// 8-bit signed
typedef mpxp_uint8_t PCM_CV_TYPE_UC;	// 8-bit unsigned
typedef PCM_CV_TYPE_F PCM_CV_TYPE_MAX;	// largest datatype (currently)

#define MIXER_SCALE_BITS    16	// used bits in integer based (lq) functions and default scale bits in hq functions
#define MIXER_SCALE_VAL  65536
#define MIXER_SCALE_MIN -32768
#define MIXER_SCALE_MAX  32767

#define AUINFOS_MIXERINFOBIT_ACONV       1
#define AUINFOS_MIXERINFOBIT_FCONV       2
#define AUINFOS_MIXERINFOBIT_DECODERANALISER 32	// else enable pcm spectrum analiser
#define AUINFOS_MIXERCTRLBIT_SPEED1000  64	// (try to) enable 0.1% speed control precision (set in mixer-init)
#define AUINFOS_MIXERINFOBIT_SPEED1000 128	// 0.1% speed control precision is enabled (set in speed control init)

#define MIXER_SETMODE_RELATIVE 0
#define MIXER_SETMODE_ABSOLUTE 1
#define MIXER_SETMODE_RESET    2

// one_mixerfunc_info->function_init(inittype) values
#define MIXER_INITTYPE_INIT    0	// first initialization (alloc buffers,calculate constant tables)
#define MIXER_INITTYPE_REINIT  1	// re-init buffers (ie: at external dep (newfile))
#define MIXER_INITTYPE_START   2	// start flag
#define MIXER_INITTYPE_RESET   4	// reset flag (ie:at seeking)
#define MIXER_INITTYPE_STOP    8	// stop flag
#define MIXER_INITTYPE_CLOSE  16	// close/dealloc
#define MIXER_INITTYPE_LQHQSW 32	// switch between lq and hq routines (ie: at freq conversion)

// one_mixerfunc_info->infobits
#define MIXER_INFOBIT_RESETDONE            1	// mixer value is set to default (reset done)
#define MIXER_INFOBIT_ENABLED              2	// mixer function is enabled/used
#define MIXER_INFOBIT_BUSY                 4	// mixer function is under control/modification (to avoid short circuit (endless cycle) in the mixer)
#define MIXER_INFOBIT_SWITCH               8	// function variable a switch (flip-flop)(its value can be 0 or 1 only)
#define MIXER_INFOBIT_EXTERNAL_DEPENDENCY 16	// function has an external dependency (controlled by another/external variable too)(ie:freq conversion,swapchan,surround)
#define MIXER_INFOBIT_PARALLEL_DEPENDENCY 32	// dependant/enabled by another mixer function too
//#define MIXER_INFOBIT_USE_LIMITER       64 // mixer function modifies amp -> enable limiter
//#define MIXER_INFOBIT_RDT_OPTIONS      128 // value modification has desktop effect (RDT_OPTIONS)
//#define MIXER_INFOBIT_ANALISER16       256 //

// MIXER_controlbits
#define MIXER_CONTROLBIT_LIMITER   1	// enable limiter
#define MIXER_CONTROLBIT_SOFTTONE  2	// allways use software tone
#define MIXER_CONTROLBIT_SPEED1000 4	// (try to) enable 0.1% speed control

#include "mpxplay.h"

typedef struct one_mixerfunc_info {
	char *name;
	char *cmdlineopt;
	int *variablep;
	int infobits;
	int var_min;				// minimum value (must be set if variable is not a SWITCH)
	int var_max;				// maximum value (must be set if variable is not a SWITCH)
	int var_center;				// center (zero) value (must be set, if center value is not 0)
	int var_step;				// step
	int (*function_init) (struct mpxplay_audioout_info_s *, int inittype);	// see inittype flags
	void (*function_lq) (struct mpxplay_audioout_info_s *);	// integer (16 bit) based calculation
	void (*function_hq) (struct mpxplay_audioout_info_s *);	// float (32 bit) based calculation; if you set function_lq, then you must set function_hq too (and vica versa)
	int (*own_checkvar_routine) (struct mpxplay_audioout_info_s *);	// if(*variablep!=var_center && check_var())
	void (*own_setvar_routine) (struct mpxplay_audioout_info_s *, unsigned int setmode, int value);
} one_mixerfunc_info;

extern unsigned int MIXER_configure(struct mpxplay_audioout_info_s *aui, struct frame *frp);
extern unsigned int MIXER_conversion(struct mpxplay_audioout_info_s *aui, struct mpxplay_audio_decoder_info_s *adi, struct frame *frp, PCM_CV_TYPE_S * pcm_sample, unsigned int samplenum);
extern void MIXER_main(struct mpxplay_audioout_info_s *aui, struct mpxplay_audio_decoder_info_s *adi, struct frame *frp);

extern void MIXER_setfunction(char *func_name, unsigned int setmode, int value);
extern one_mixerfunc_info *MIXER_getfunction(char *func_name);
extern int MIXER_getvalue(char *func_name);
extern int MIXER_getstatus(char *func_name);
extern void MIXER_checkfunc_setflags(char *func_name);
extern void MIXER_checkallfunc_setflags(void);
extern void MIXER_checkallfunc_dependencies(int flag);
extern void MIXER_resetallfunc(void);

extern void MIXER_init(void);
extern void MIXER_allfuncinit_init(void);
extern void MIXER_allfuncinit_close(void);
extern void MIXER_allfuncinit_restart(void);
extern void MIXER_allfuncinit_reinit(void);

#endif
