//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: sofware-mixer routines - main

#include "newfunc\newfunc.h"
#include "mix_func.h"
#include "au_mixer.h"
#include "in_file.h"
#include "au_cards\au_cards.h"
#include "display\display.h"

#define MIXER_MAX_FUNCTIONS 31

extern struct mpxplay_audioout_info_s au_infos;
extern unsigned int displaymode;
extern unsigned int SOUNDLIMITvol, MIXER_var_autovolume;
extern mpxp_uint8_t *aucards_card_channelmap;	// later move it to au_infos (API break)

static one_mixerfunc_info MIXER_FUNCINFO_hqsw;
extern one_mixerfunc_info MIXER_FUNCINFO_crossfader;
extern one_mixerfunc_info MIXER_FUNCINFO_swapchan;
extern one_mixerfunc_info MIXER_FUNCINFO_limiter;
extern one_mixerfunc_info MIXER_FUNCINFO_mute;
extern one_mixerfunc_info MIXER_FUNCINFO_balance;
extern one_mixerfunc_info MIXER_FUNCINFO_surround;
extern one_mixerfunc_info MIXER_FUNCINFO_tone_bass;
extern one_mixerfunc_info MIXER_FUNCINFO_tone_treble;
extern one_mixerfunc_info MIXER_FUNCINFO_tone_loudness;
extern one_mixerfunc_info MIXER_FUNCINFO_seekspeed;
extern one_mixerfunc_info MIXER_FUNCINFO_speed;
extern one_mixerfunc_info MIXER_FUNCINFO_volume;

static one_mixerfunc_info *all_mixerfunc_info[MIXER_MAX_FUNCTIONS + 1] = {
	&MIXER_FUNCINFO_crossfader,	// must be the first
	&MIXER_FUNCINFO_swapchan,
	&MIXER_FUNCINFO_surround,
	&MIXER_FUNCINFO_hqsw,
	&MIXER_FUNCINFO_tone_loudness,
	&MIXER_FUNCINFO_tone_treble,	// parallel dep. : loudness
	&MIXER_FUNCINFO_tone_bass,	// parallel dep. : treble,loudness
	&MIXER_FUNCINFO_seekspeed,
	&MIXER_FUNCINFO_speed,
	&MIXER_FUNCINFO_mute,
	&MIXER_FUNCINFO_balance,
	&MIXER_FUNCINFO_limiter,	// last-1 (parallel dep.:surround,tone,(volume))
	&MIXER_FUNCINFO_volume,		// must be the last (parallel dep.:mute,balance,limiter)
	NULL
};

#define MIXER_BUILTIN_FUNCTIONS  13	// !!!
#define MIXER_DLLFUNC_INSERTPOINT 1	// after crossfade

int MIXER_var_usehq;
unsigned int MIXER_controlbits;
static PCM_CV_TYPE_S *MIXER_int16pcm_buffer;
static unsigned int MIXER_int16pcm_bufsize, MIXER_nb_mixerfuncs = MIXER_BUILTIN_FUNCTIONS;

//-------------------------------------------------------------------------
//configure mixer to the new audio file (decoding)
unsigned int MIXER_configure(struct mpxplay_audioout_info_s *aui, struct frame *frp)
{
	unsigned int blocksamples, newpcmoutbufsize;
	one_mixerfunc_info *infop_speed;
	float speed_expansion;

	if(aui->card_infobits & AUINFOS_CARDINFOBIT_BITSTREAMOUT) {
		frp->pcmout_blocksize = 1;
		MIXER_resetallfunc();
		return 1;
	}

	if(aui->freq_song < PCM_MIN_FREQ)
		return 0;

	infop_speed = MIXER_getfunction("MIX_SPEED");
	if(infop_speed)
		speed_expansion = (float)infop_speed->var_center / (float)infop_speed->var_min;	// 2 by default
	else
		speed_expansion = 1.0;

	blocksamples = mpxplay_infile_get_samplenum_per_frame(aui->freq_song);
	frp->pcmout_blocksize = blocksamples * aui->chan_song;
	newpcmoutbufsize = (long)(speed_expansion * (float)((blocksamples + 128) * max(aui->chan_song, aui->chan_card))	// +128 for safe
							  * (float)(max(aui->freq_card, aui->freq_song)) / (float)(aui->freq_song));
	newpcmoutbufsize *= sizeof(MPXPLAY_PCMOUT_FLOAT_T);
	if(frp->pcmout_bufsize < newpcmoutbufsize) {
		if(frp->pcmout_buffer)
			free(frp->pcmout_buffer);
		frp->pcmout_buffer = (mpxp_uint8_t *) malloc(newpcmoutbufsize);
		if(!frp->pcmout_buffer) {
			frp->pcmout_bufsize = 0;
			return 0;
		}
		frp->pcmout_bufsize = newpcmoutbufsize;
	}

	if((frp->pcmout_bufsize / sizeof(MPXPLAY_PCMOUT_FLOAT_T)) > (MIXER_int16pcm_bufsize / sizeof(PCM_CV_TYPE_S))) {
		if(MIXER_int16pcm_buffer)
			free(MIXER_int16pcm_buffer);
		MIXER_int16pcm_bufsize = frp->pcmout_bufsize / sizeof(MPXPLAY_PCMOUT_FLOAT_T) * sizeof(PCM_CV_TYPE_S);
		MIXER_int16pcm_buffer = (PCM_CV_TYPE_S *) pds_malloc(MIXER_int16pcm_bufsize);
		if(!MIXER_int16pcm_buffer) {
			MIXER_int16pcm_bufsize = 0;
			return 0;
		}
		aui->mvp->vds->pcm_data = MIXER_int16pcm_buffer;
		aui->mvp->vds->pcm_freq = aui->freq_card;
	}
	MIXER_checkallfunc_dependencies(MIXER_INFOBIT_EXTERNAL_DEPENDENCY);
	MIXER_checkallfunc_dependencies(MIXER_INFOBIT_PARALLEL_DEPENDENCY);
	return 1;
}

// audio-data to mixer-data conversion (lq/hq)
unsigned int MIXER_conversion(struct mpxplay_audioout_info_s *aui, struct mpxplay_audio_decoder_info_s *adi, struct frame *frp, PCM_CV_TYPE_S * pcm_sample, unsigned int samplenum)
{
	unsigned int used_samplenum;

	frp->pcmout_storedsamples = samplenum;

	if(aui->card_infobits & AUINFOS_CARDINFOBIT_BITSTREAMOUT)
		return samplenum;

	if(!samplenum)
		return samplenum;

	if(samplenum > frp->pcmout_blocksize)
		samplenum = frp->pcmout_blocksize;
	used_samplenum = samplenum;

	//if(pcm_sample!=(PCM_CV_TYPE_S *)frp->pcmout_buffer){ // decoder wrote data into pcmdec_buffer
	pds_memcpy(frp->pcmout_buffer, pcm_sample, samplenum * adi->bytespersample);
	pcm_sample = (PCM_CV_TYPE_S *) frp->pcmout_buffer;
	//}// else decoder wrote data into pcmout_buffer

	if(aui->mixer_function_flags || ((adi->outchannels != aui->chan_card) && (adi->outchannels > 2) && adi->chanmatrix && (aui->chan_card <= 2))) {
		funcbit_enable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_ACONV);
		if(MIXER_var_usehq)
			funcbit_enable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_FCONV);
		else
			funcbit_disable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_FCONV);
	} else {
		funcbit_disable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_ACONV);
		funcbit_disable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_FCONV);
	}

	if(adi->infobits & ADI_FLAG_FLOATOUT) {	// decoder makes a 32-bit float output (aac,ac3,dts,mp2,mp3,mpc,ogg,wav-float)
		if(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_FCONV) {
			cv_scale_float(pcm_sample, samplenum, adi->bits, MIXER_SCALE_BITS, 0);
			aui->bytespersample_mixer = sizeof(float);
		} else {
			if((aui->mixer_infobits & AUINFOS_MIXERINFOBIT_ACONV)) {
				aui->bytespersample_mixer = MIXER_SCALE_BITS >> 3;
				cv_float_to_n_bits(pcm_sample, samplenum, adi->bits, aui->bytespersample_mixer, adi->infobits & ADI_FLAG_FPUROUND_CHOP);
			} else {
				aui->bytespersample_mixer = sizeof(float);
			}
		}
	} else {					// pcm (8,16,24,32 bit) output (ape,cdw,flac,wav)
		if(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_FCONV) {
			cv_n_bits_to_float(pcm_sample, samplenum, adi->bytespersample, MIXER_SCALE_BITS);
			aui->bytespersample_mixer = sizeof(float);
		} else {
			if((aui->mixer_infobits & AUINFOS_MIXERINFOBIT_ACONV) && adi->bytespersample != 2) {
				aui->bytespersample_mixer = MIXER_SCALE_BITS >> 3;
				cv_bits_n_to_m(pcm_sample, samplenum, adi->bytespersample, aui->bytespersample_mixer);
			} else {
				aui->bytespersample_mixer = adi->bytespersample;
			}
		}
	}

	if(adi->chanmatrix && (aui->chan_card <= 2))
		samplenum = cv_channels_downmix(pcm_sample, samplenum, adi->outchannels, adi->chanmatrix, aui->chan_card, aucards_card_channelmap, aui);
	else if((adi->outchannels != aui->chan_card) || adi->chanmatrix) {
		if(adi->outchannels == 1)
			samplenum = cv_channels_1_to_n(pcm_sample, samplenum, aui->chan_card, aui->bytespersample_mixer);
		else
			samplenum = cv_channels_remap(pcm_sample, samplenum, adi->outchannels, adi->chanmatrix, aui->chan_card, aucards_card_channelmap, aui->bytespersample_mixer);
	}

	frp->pcmout_storedsamples = samplenum;	// samplenum of mixer

	if((adi->infobits & ADI_FLAG_OWN_SPECTANAL) || (frp->infile_infos->audio_decoder_funcs && frp->infile_infos->audio_decoder_funcs->get_analiser_bands))
		funcbit_enable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_DECODERANALISER);
	else
		funcbit_disable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_DECODERANALISER);

	return used_samplenum;
}

// do mixer functions
void MIXER_main(struct mpxplay_audioout_info_s *aui, struct mpxplay_audio_decoder_info_s *adi, struct frame *frp)
{
	unsigned int do_pcm16;

	aui->samplenum = frp->pcmout_storedsamples;
	aui->mvp->vds->pcm_samplenum = frp->pcmout_storedsamples;

	if(aui->card_infobits & AUINFOS_CARDINFOBIT_BITSTREAMOUT) {
		aui->pcm_sample = (PCM_CV_TYPE_S *) frp->pcmdec_buffer;
		return;
	}

	aui->pcm_sample = (PCM_CV_TYPE_S *) frp->pcmout_buffer;

	if(!aui->samplenum)
		return;

	//mixer functions (volume,surround,speed,swpachan ...)
	if(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_ACONV) {
		unsigned long i = 0, funcflags = aui->mixer_function_flags;
		do {
			if(funcflags & 1) {
				one_mixerfunc_info *infop = all_mixerfunc_info[i];
				if(infop) {
					if(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_FCONV)
						infop->function_hq(aui);
					else
						infop->function_lq(aui);
					if(!aui->samplenum)
						return;
				}
			}
			i++;
			funcflags >>= 1;
		} while(funcflags);
	}

	if((displaymode & DISP_ANALISER) && (displaymode & DISP_NOFULLEDIT) && !(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_DECODERANALISER)) {
		if(aui->samplenum < 2304)
			do_pcm16 = 2;
		else
			do_pcm16 = 1;
	} else if((displaymode & DISP_TIMEPOS) || SOUNDLIMITvol || aui->mvp->cfi->crossfadelimit)
		do_pcm16 = 1;
	else if(MIXER_var_autovolume)
		do_pcm16 = 1;
	else
		do_pcm16 = 0;

	if(do_pcm16) {
		if((aui->card_wave_id == MPXPLAY_WAVEID_PCM_SLE) && (aui->bytespersample_card == 2) && (do_pcm16 < 2)) {
			aui->pcm16 = aui->pcm_sample;
			do_pcm16 = 0;
		} else {
			aui->pcm16 = MIXER_int16pcm_buffer;
			pds_memcpy((char *)aui->pcm16, (char *)aui->pcm_sample, aui->samplenum * aui->bytespersample_mixer);
		}
	}

	if((aui->mixer_infobits & AUINFOS_MIXERINFOBIT_FCONV) || (!(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_ACONV) && (adi->infobits & ADI_FLAG_FLOATOUT))) {
		unsigned int mixer_scalebits = (aui->mixer_infobits & AUINFOS_MIXERINFOBIT_FCONV) ? MIXER_SCALE_BITS : aui->bits_song;

		if(do_pcm16)
			cv_float_to_n_bits(aui->pcm16, aui->samplenum, mixer_scalebits, (16 >> 3), 0);

		if(aui->card_wave_id == MPXPLAY_WAVEID_PCM_FLOAT)
			cv_scale_float(aui->pcm_sample, aui->samplenum, mixer_scalebits, aui->bits_card, 1);
		else
			cv_float_to_n_bits(aui->pcm_sample, aui->samplenum, mixer_scalebits, aui->bytespersample_card, adi->infobits & ADI_FLAG_FPUROUND_CHOP);

	} else {
		if(do_pcm16)
			if(aui->bytespersample_mixer != 2)
				cv_bits_n_to_m(aui->pcm16, aui->samplenum, aui->bytespersample_mixer, 2);

		if(aui->card_wave_id == MPXPLAY_WAVEID_PCM_FLOAT)
			cv_n_bits_to_float(aui->pcm_sample, aui->samplenum, aui->bytespersample_mixer, aui->bits_card);
		else
			cv_bits_n_to_m(aui->pcm_sample, aui->samplenum, aui->bytespersample_mixer, aui->bytespersample_card);
	}

	//get volume & spectrum analiser level
	if((displaymode & DISP_TIMEPOS) || SOUNDLIMITvol || aui->mvp->cfi->crossfadelimit)
		mixer_get_volumelevel(aui->pcm16, aui->samplenum, aui->chan_card);

	if((displaymode & DISP_ANALISER) && (displaymode & DISP_NOFULLEDIT))
		if(!(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_DECODERANALISER))
			mixer_pcm_spectrum_analiser(aui->pcm16, aui->samplenum, aui->chan_card);

	//auto volume
	if(MIXER_var_autovolume)
		mixer_autovolume_set(aui->pcm16, aui->samplenum);

	//cut silence
	mixer_soundlimit_check(aui);
}

//--------------------------------------------------------------------------
//search function-name in all_mixerfunc_info
static int mixer_common_search_functionname(char *func_name)
{
	unsigned int i;

	for(i = 0; i < MIXER_nb_mixerfuncs; i++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[i];
		if(infop && infop->name) {
			if(pds_stricmp(infop->name, func_name) == 0)
				return i;
		}
	}
	return -1;
}

//set function's variable by a new value
static unsigned int mixer_common_setvar(one_mixerfunc_info * infop, unsigned int setmode, int value)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;

	if(!infop)
		return 0;
	if(infop->infobits & MIXER_INFOBIT_BUSY)
		return 0;
	funcbit_smp_enable(infop->infobits, MIXER_INFOBIT_BUSY);

	if(infop->own_setvar_routine) {
		infop->own_setvar_routine(aui, setmode, value);
	} else {
		if(infop->variablep) {
			int currmixval = *(infop->variablep), newmixval = currmixval;

			if(infop->infobits & MIXER_INFOBIT_SWITCH) {
				switch (setmode) {
				case MIXER_SETMODE_RELATIVE:
					funcbit_inverse(newmixval, infop->var_max);
					break;
				case MIXER_SETMODE_ABSOLUTE:
					if(value > 0)
						funcbit_enable(newmixval, infop->var_max);
					else
						funcbit_disable(newmixval, infop->var_max);
					break;
				case MIXER_SETMODE_RESET:
					funcbit_disable(newmixval, infop->var_max);
					newmixval |= infop->var_center;
					break;
				}
			} else {
				switch (setmode) {
				case MIXER_SETMODE_RELATIVE:
					newmixval = currmixval + (value * infop->var_step);
					if((currmixval < infop->var_center && newmixval > infop->var_center) || (currmixval > infop->var_center && newmixval < infop->var_center))
						newmixval = infop->var_center;
					break;
				case MIXER_SETMODE_ABSOLUTE:
					newmixval = value;
					break;
				case MIXER_SETMODE_RESET:
					newmixval = infop->var_center;
					break;
				}
				if(newmixval < infop->var_min)
					newmixval = infop->var_min;
				else if(newmixval > infop->var_max)
					newmixval = infop->var_max;
			}

			funcbit_smp_value_put(*(infop->variablep), newmixval);

		} else {
			funcbit_smp_disable(infop->infobits, MIXER_INFOBIT_BUSY);
			return 0;
		}
	}
	funcbit_smp_disable(infop->infobits, MIXER_INFOBIT_BUSY);
	return 1;
}

//check function's variable and give back the (new) status of function (0/1)
static unsigned int mixer_common_checkvar(one_mixerfunc_info * infop)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	unsigned int enable = 0;

	if(!infop)
		return enable;

	if(infop->variablep) {
		if(infop->infobits & MIXER_INFOBIT_SWITCH) {
			if(*(infop->variablep))
				enable = 1;
		} else {
			// ??? an extra range check
			if(*(infop->variablep) > infop->var_max)
				*(infop->variablep) = infop->var_max;
			else if(*(infop->variablep) < infop->var_min)
				*(infop->variablep) = infop->var_min;
			if(*(infop->variablep) != infop->var_center)
				enable = 1;
		}
	}

	if(infop->own_checkvar_routine)
		enable = infop->own_checkvar_routine(aui);

	return enable;
}

static void mixer_common_setflags(one_mixerfunc_info * infop, int functionnum, unsigned int enable)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	unsigned int bit = 1 << functionnum;

	if(!infop)
		return;

	if(enable) {
		funcbit_smp_enable(infop->infobits, MIXER_INFOBIT_ENABLED);
		if(infop->function_lq && infop->function_hq)
			funcbit_smp_enable(aui->mixer_function_flags, bit);
	} else {
		funcbit_smp_disable(infop->infobits, MIXER_INFOBIT_ENABLED);
		funcbit_smp_disable(aui->mixer_function_flags, bit);
	}
}

static unsigned int mixfunc_checkvar_setflags(one_mixerfunc_info * infop, int functionnum)
{
	if(infop && (infop->infobits & MIXER_INFOBIT_RESETDONE) && !(infop->infobits & MIXER_INFOBIT_BUSY)) {
		struct mpxplay_audioout_info_s *aui = &au_infos;
		unsigned int enable, oldstatus;

		funcbit_smp_enable(infop->infobits, MIXER_INFOBIT_BUSY);

		oldstatus = infop->infobits & MIXER_INFOBIT_ENABLED;
		enable = mixer_common_checkvar(infop);
		mixer_common_setflags(infop, functionnum, enable);

		if(infop->function_init) {
			if(!oldstatus && enable) {
				if(!infop->function_init(aui, MIXER_INITTYPE_START)) {
					mixer_common_setflags(infop, functionnum, 0);
					infop->function_init(aui, MIXER_INITTYPE_STOP);
				}
			} else {
				if(oldstatus && !enable)
					infop->function_init(aui, MIXER_INITTYPE_STOP);
			}
		}

		funcbit_smp_disable(infop->infobits, MIXER_INFOBIT_BUSY);
		return 1;
	}
	return 0;
}

//----------------------------------------------------------------------
//set one mixer function (modify,enable/disable)
void MIXER_setfunction(char *func_name, unsigned int setmode, int value)
{
	one_mixerfunc_info *infop;
	int functionnum;

	functionnum = mixer_common_search_functionname(func_name);
	if(functionnum < 0)
		return;

	infop = all_mixerfunc_info[functionnum];
	if(!mixer_common_setvar(infop, setmode, value))
		return;

	if(mixfunc_checkvar_setflags(infop, functionnum))
		MIXER_checkallfunc_dependencies(MIXER_INFOBIT_PARALLEL_DEPENDENCY);
}

// return mixerfunc structure (to read a value from it)
one_mixerfunc_info *MIXER_getfunction(char *func_name)
{
	int functionnum = mixer_common_search_functionname(func_name);

	if(functionnum < 0)
		return NULL;

	return (all_mixerfunc_info[functionnum]);
}

int MIXER_getvalue(char *func_name)
{
	one_mixerfunc_info *infop = MIXER_getfunction(func_name);

	if(infop && infop->variablep)
		return (*infop->variablep);

	return 0;
}

// disabled(0)/enabled(1)
int MIXER_getstatus(char *func_name)
{
	one_mixerfunc_info *infop = MIXER_getfunction(func_name);

	if(infop)
		return (infop->infobits & MIXER_INFOBIT_ENABLED);

	return 0;
}

//check one function and set its flag (enable/disable)
void MIXER_checkfunc_setflags(char *func_name)
{
	one_mixerfunc_info *infop;
	int functionnum;

	functionnum = mixer_common_search_functionname(func_name);
	if(functionnum < 0)
		return;

	infop = all_mixerfunc_info[functionnum];

	if(mixfunc_checkvar_setflags(infop, functionnum))
		MIXER_checkallfunc_dependencies(MIXER_INFOBIT_PARALLEL_DEPENDENCY);
}

void MIXER_checkallfunc_setflags(void)
{
	unsigned int functionnum;
	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		mixfunc_checkvar_setflags(infop, functionnum);
	}
	//MIXER_checkallfunc_dependencies(MIXER_INFOBIT_PARALLEL_DEPENDENCY); // ??? called from MIXER_configure
}

// set (enable/disable) functions witch have external or parallel dependency
void MIXER_checkallfunc_dependencies(int flag)
{
	unsigned int functionnum;
	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		if(infop && infop->infobits & flag)
			mixfunc_checkvar_setflags(infop, functionnum);
	}
}

//set all mixer variables/functions to default (zero) value
void MIXER_resetallfunc(void)
{
	unsigned int functionnum;

	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		if(infop) {
			mixer_common_setvar(infop, MIXER_SETMODE_RESET, 0);
			funcbit_smp_enable(infop->infobits, MIXER_INFOBIT_RESETDONE);
		}
	}
}

//--------------------------------------------------------------------
//init all functions (if it has function_init)
#ifdef MPXPLAY_LINK_DLLLOAD

#ifndef WIN32
#include <mem.h>
#endif

static void MIXER_load_plugins(void)
{
	int functionnum;
	mpxplay_module_entry_s *dll_found = NULL;

	do {
		dll_found = newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_AUMIXER, 0, NULL, dll_found);	// get next
		//fprintf(stdout,"dll:%8.8X sv:%4.4X\n",dll_found,dll_found->module_structure_version);
		if(dll_found && (dll_found->module_structure_version == MPXPLAY_DLLMODULEVER_AUMIXER)) {	// !!!
			one_mixerfunc_info *infop = (one_mixerfunc_info *) dll_found->module_callpoint;
			if(infop) {
				functionnum = mixer_common_search_functionname(infop->name);
				if(functionnum >= 0)	// overwrite built-in dsp routine
					all_mixerfunc_info[functionnum] = infop;
				else {			// insert the new dsp routine
					if(MIXER_nb_mixerfuncs < MIXER_MAX_FUNCTIONS) {
						memmove((void *)&all_mixerfunc_info[MIXER_DLLFUNC_INSERTPOINT], (void *)&all_mixerfunc_info[MIXER_DLLFUNC_INSERTPOINT + 1],
								(MIXER_nb_mixerfuncs - MIXER_DLLFUNC_INSERTPOINT) * sizeof(one_mixerfunc_info *));
						all_mixerfunc_info[MIXER_DLLFUNC_INSERTPOINT] = infop;
						MIXER_nb_mixerfuncs++;
					}
				}
			}
		}
	} while(dll_found);
}
#endif

void MIXER_init(void)
{
	funcbit_enable(MIXER_controlbits, MIXER_CONTROLBIT_LIMITER);
#ifdef MPXPLAY_LINK_DLLLOAD
	MIXER_load_plugins();
#endif
	MIXER_resetallfunc();
}

//first init (malloc)
void MIXER_allfuncinit_init(void)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	unsigned int functionnum;

	MIXER_int16pcm_buffer = (PCM_CV_TYPE_S *) pds_malloc(2 * PCM_BUFFER_SIZE);
	if(MIXER_int16pcm_buffer)
		MIXER_int16pcm_bufsize = PCM_BUFFER_SIZE;

	if(MIXER_controlbits & MIXER_CONTROLBIT_SOFTTONE)
		funcbit_disable(aui->card_infobits, AUINFOS_CARDINFOBIT_HWTONE);

	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		if(infop && infop->function_init)
			infop->function_init(aui, MIXER_INITTYPE_INIT);
	}
}

//func close (free)
void MIXER_allfuncinit_close(void)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	unsigned int functionnum;

	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		if(infop && infop->function_init)
			infop->function_init(aui, MIXER_INITTYPE_CLOSE);
	}
	if(MIXER_int16pcm_buffer) {
		free(MIXER_int16pcm_buffer);
		MIXER_int16pcm_buffer = NULL;
		MIXER_int16pcm_bufsize = 0;
	}
}

// re-init buffers (ie: at new file (external dep))
void MIXER_allfuncinit_reinit(void)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	unsigned int functionnum;

	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		if(infop && infop->function_init && (infop->infobits & MIXER_INFOBIT_ENABLED))
			infop->function_init(aui, MIXER_INITTYPE_REINIT);
	}
}

//clear buffers (ie: at seeking)
void MIXER_allfuncinit_restart(void)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	unsigned int functionnum;

	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		if(infop && infop->function_init && (infop->infobits & MIXER_INFOBIT_ENABLED))
			infop->function_init(aui, MIXER_INITTYPE_RESET);
	}
}

//switch between lq and hq routines
static void MIXER_allfuncinit_lqhqsw(void)
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	unsigned int functionnum;

	for(functionnum = 0; functionnum < MIXER_nb_mixerfuncs; functionnum++) {
		one_mixerfunc_info *infop = all_mixerfunc_info[functionnum];
		if(infop && infop->function_init && (infop->infobits & MIXER_INFOBIT_ENABLED))
			infop->function_init(aui, MIXER_INITTYPE_LQHQSW);
	}
}

//----------------------------------------------------------------------

static int mixer_hqsw_init(struct mpxplay_audioout_info_s *aui, int inittype)
{
	switch (inittype) {
	case MIXER_INITTYPE_START:
	case MIXER_INITTYPE_STOP:
		MIXER_allfuncinit_lqhqsw();	// re-init functions after hq/lq change
	}
	return 1;
}

static one_mixerfunc_info MIXER_FUNCINFO_hqsw = {
	"MIX_HQ",
	"mxhq",
	&MIXER_var_usehq,
	MIXER_INFOBIT_SWITCH,
	0, 1, 0, 0,
	mixer_hqsw_init,
	NULL,
	NULL,
	NULL,
	NULL,
};
