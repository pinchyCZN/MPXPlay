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
//function: set volume/limiter

#include "au_mixer.h"
#include "mix_func.h"
#include "newfunc\newfunc.h"

//#define MIXER_LIMITER_USE_DELAYBUF 1
#define MIXER_LIMITER_CORRECT_BOUNDARY 1

#define VOLUME_MUTE_VOLDIV_DEFAULT  8	// 1/8 volume reduction at mute
#define VOLUME_SCALE_DEFAULT 8192	// don't set it higher (13 + 3 + 16 = 32 bits)

static unsigned int mixer_volume_calcvol(struct mpxplay_audioout_info_s *aui);
static void mixer_volumes_calc(struct mpxplay_audioout_info_s *aui);

extern unsigned int MIXER_controlbits, MIXER_channels, crossfadepart;

int MIXER_var_limiter_overflow;
int MIXER_var_volume;
int MIXER_var_mutelen, MIXER_var_mute_voldiv = VOLUME_MUTE_VOLDIV_DEFAULT;
int MIXER_var_balance;

one_mixerfunc_info MIXER_FUNCINFO_volume;

static float mxvolum_limiter_overflow = 1.0;
static long mxvolum_volume_scale = VOLUME_SCALE_DEFAULT;	// controlled by limiter_overflow
static long mxvolum_mute_voldiv = 1, mxvolum_mute_keycounter;

//------------------------------------------------------------------------
// mute function

#define MUTE_SWITCH_SIGN 65535	// on/off switch instead of 'push'

//this is just a timer, not a transformation (volume does it)
static void mixer_volume_mute_lq(struct mpxplay_audioout_info_s *aui)
{
	if(MIXER_var_mutelen != MUTE_SWITCH_SIGN) {
		if(MIXER_var_mutelen)
			funcbit_smp_value_decrement(MIXER_var_mutelen);
		if(!MIXER_var_mutelen) {
			MIXER_checkfunc_setflags("MIX_MUTE");
			funcbit_smp_value_put(mxvolum_mute_voldiv, 1);
			funcbit_smp_value_put(mxvolum_mute_keycounter, 0);
		}
	}
}

static void mixer_volume_mute_setvar(struct mpxplay_audioout_info_s *aui, unsigned int setmode, int value)
{
	int muteval = MIXER_var_mutelen;
	switch (setmode) {
	case MIXER_SETMODE_ABSOLUTE:
		muteval = value;
		break;
	case MIXER_SETMODE_RELATIVE:
		if(value == MUTE_SWITCH_SIGN) {	// on/off switch
			if(muteval == MUTE_SWITCH_SIGN)
				muteval = 0;
			else
				muteval = MUTE_SWITCH_SIGN;
		} else {				// mute while push
#ifdef MPXPLAY_WIN32
			if(mxvolum_mute_keycounter < 15)
				muteval = 21 - mxvolum_mute_keycounter;
			else
				muteval = 6;
			mxvolum_mute_keycounter++;
#else
			if(muteval)
				muteval = 5;
			else
				muteval = 15;
#endif
		}
		break;
	case MIXER_SETMODE_RESET:
		mxvolum_mute_keycounter = 0;
		muteval = 0;
		break;
	}
	if(MIXER_var_mute_voldiv < 1)
		MIXER_var_mute_voldiv = 1;
	if(muteval)
		funcbit_smp_value_put(mxvolum_mute_voldiv, MIXER_var_mute_voldiv);
	else
		funcbit_smp_value_put(mxvolum_mute_voldiv, 1);

	funcbit_smp_value_put(MIXER_var_mutelen, muteval);
}

one_mixerfunc_info MIXER_FUNCINFO_mute = {
	"MIX_MUTE",
	NULL,
	&MIXER_var_mutelen,
	MIXER_INFOBIT_SWITCH,
	0, 0, 0, 0,
	NULL,
	&mixer_volume_mute_lq,
	&mixer_volume_mute_lq,
	NULL,
	&mixer_volume_mute_setvar
};

//-------------------------------------------------------------------
//volume balance
static unsigned int mixer_volume_balance_calcvol(unsigned int ch, unsigned int vol)
{
	switch (ch) {
	case 0:
		if(MIXER_var_balance > 0)
			vol = vol * (100 - MIXER_var_balance) / 100;
		break;
	case 1:
		if(MIXER_var_balance < 0)
			vol = vol * (100 + MIXER_var_balance) / 100;
		break;
	}
	return vol;
}

static int mixer_volume_balance_checkvar(struct mpxplay_audioout_info_s *aui)
{
	if(MIXER_var_balance != 0)
		return 1;
	return 0;
}

one_mixerfunc_info MIXER_FUNCINFO_balance = {
	"MIX_BALANCE",
	"mxbl",
	&MIXER_var_balance,
	0,
	-99, +99, 0, 3,
	NULL,
	NULL,
	NULL,
	&mixer_volume_balance_checkvar,
	NULL
};

//--------------------------------------------------------------------------
//limiter
#define LIMITER_VOLUP_DELAY  15	// in frames

typedef struct wave_area_s {
	unsigned long begin;
	long maxsign;
	unsigned long value;
} wave_area_s;

static wave_area_s **wa;
static unsigned int wa_max_areas;

static unsigned int wac[PCM_MAX_CHANNELS];
static unsigned int VOL_volumes[PCM_MAX_CHANNELS];
static long savemul[PCM_MAX_CHANNELS];

#ifdef MIXER_LIMITER_USE_DELAYBUF
static PCM_CV_TYPE_F *pcmsave_buffer;
static unsigned int pcmsave_bufsize, pcmsave_storedsamples;
#endif
#ifdef MIXER_LIMITER_CORRECT_BOUNDARY
static long lastvalue[PCM_MAX_CHANNELS];
static long firstmaxsignpos[PCM_MAX_CHANNELS];
#endif

static void get_wave_areas_lq(struct mpxplay_audioout_info_s *aui)
{
	short *pcm_sample = aui->pcm16;
	unsigned int samplenum = aui->samplenum, channels = aui->chan_card;
	long i, ch, c1, begin, maxsign, sign, ac;

	for(ch = 0; ch < channels; ch++) {
		ac = 0;
		begin = ch;
		maxsign = wa[ch][wac[ch] - 1].maxsign;
		if(!maxsign)
			maxsign = (int)pcm_sample[ch];
		if(maxsign < 0)
			sign = 1;
		else
			sign = 0;
		for(i = ch; (i < samplenum) && (ac < wa_max_areas); i += channels) {
			c1 = (long)pcm_sample[i];
			if(c1 != 0) {
				if(sign) {
					if(c1 < 0) {
						if(c1 < maxsign)
							maxsign = c1;
					} else {
						sign = !sign;
						wa[ch][ac].begin = begin;
						wa[ch][ac].maxsign = maxsign;
						begin = i;
						maxsign = c1;
						ac++;
					}
				} else {
					if(c1 > 0) {
						if(c1 > maxsign)
							maxsign = c1;
					} else {
						sign = !sign;
						wa[ch][ac].begin = begin;
						wa[ch][ac].maxsign = maxsign;
						begin = i;
						maxsign = c1;
						ac++;
					}
				}
			}
		}
		wa[ch][ac].begin = begin;
		wa[ch][ac].maxsign = maxsign;
		ac++;
		wa[ch][ac].begin = 0x7fffffff;
		wa[ch][ac].maxsign = MIXER_SCALE_MAX;
		wac[ch] = ac;
	}
}

static void get_wave_areas_hq_long(struct mpxplay_audioout_info_s *aui, PCM_CV_TYPE_F * pcm_sample)
{
	unsigned int samplenum = aui->samplenum, channels = aui->chan_card;
	long i, ch, ctmp, c1, begin, maxsign, sign, ac;

	for(ch = 0; ch < channels; ch++) {
#ifdef MIXER_LIMITER_USE_DELAYBUF
		ac = wac[ch] - 1;
		begin = wa[ch][ac].begin;
		maxsign = wa[ch][ac].maxsign;
#else
		ac = 0;
		begin = ch;
		maxsign = wa[ch][wac[ch] - 1].maxsign;
#endif
		if(!maxsign)
			pds_ftoi(pcm_sample[ch], &maxsign);
#ifdef MIXER_LIMITER_CORRECT_BOUNDARY
		firstmaxsignpos[ch] = ch;
#endif
		if(maxsign < 0)
			sign = 1;
		else
			sign = 0;
		for(i = begin; (i < samplenum) && (ac < wa_max_areas); i += channels) {
			pds_ftoi(pcm_sample[i], &ctmp);
			c1 = ctmp;
			if(c1 != 0) {
				if(sign) {
					if(c1 < 0) {
						if(c1 < maxsign) {
							maxsign = c1;
#ifdef MIXER_LIMITER_CORRECT_BOUNDARY
							if(!ac)
								firstmaxsignpos[ch] = i;
#endif
						}
					} else {
						sign = !sign;
						wa[ch][ac].begin = begin;
						wa[ch][ac].maxsign = maxsign;
						begin = i;
						maxsign = c1;
						ac++;
					}
				} else {
					if(c1 > 0) {
						if(c1 > maxsign) {
							maxsign = c1;
#ifdef MIXER_LIMITER_CORRECT_BOUNDARY
							if(!ac)
								firstmaxsignpos[ch] = i;
#endif
						}
					} else {
						sign = !sign;
						wa[ch][ac].begin = begin;
						wa[ch][ac].maxsign = maxsign;
						begin = i;
						maxsign = c1;
						ac++;
					}
				}
			}
		}
		wa[ch][ac].begin = begin;
		wa[ch][ac].maxsign = maxsign;
		ac++;
		wa[ch][ac].begin = 0x7fffffff;
		wa[ch][ac].maxsign = MIXER_SCALE_MAX;
		wac[ch] = ac;
	}
}

void cmwv1(void);

static void calculate_max_wave_values(struct mpxplay_audioout_info_s *aui, unsigned int samplenum)
{
	//struct crossfade_info *cfi=aui->mvp->cfi;
	unsigned int channels = aui->chan_card, volup_delay = LIMITER_VOLUP_DELAY;
	long ch, ca, amp, newamp, volmul, cmul, delaj;
	long volume_limit_max, volume_limit_min, volume_scale_max, volume_scale_min;
	//char sout[100];

	volume_limit_max = (long)((float)MIXER_SCALE_MAX * mxvolum_limiter_overflow);
	volume_limit_min = (long)((float)MIXER_SCALE_MIN * mxvolum_limiter_overflow);
	volume_scale_max = MIXER_SCALE_MAX * mxvolum_volume_scale;
	volume_scale_min = MIXER_SCALE_MIN * mxvolum_volume_scale;

	/*if(crossfadepart){
	   if((crossfadepart&CFT_FADEOUT) && (cfi->crossfadetype&CFT_FADEOUT))
	   volup_delay=1024;//0x7ffffff/wa_max_areas; // ???
	   if((crossfadepart&CFT_FADEIN) && (cfi->crossfadetype&CFT_FADEIN))
	   volup_delay=1024;//cfi->crossfade_in_len;
	   } */
	//sprintf(sout,"wac0: %5d wac1: %5d d0:%d",wac[0],wac[1],(wac[0]*volup_delay));
	//display_message(1,0,sout);

	for(ch = 0; ch < channels; ch++) {
		volmul = VOL_volumes[ch] * mxvolum_volume_scale / 100;
		if(savemul[ch] < volmul)
			cmul = savemul[ch];
		else
			cmul = volmul;

		delaj = wac[ch] * volup_delay;
		ca = 0;
		do {
			amp = wa[ch][ca].maxsign;
			pds_ftoi((float)amp * (float)cmul / (float)mxvolum_volume_scale, &newamp);	// hq-mode requires float calculation
			if(newamp > volume_limit_max)
				cmul = volume_scale_max / amp;	// new cmul (lower)
			else if(newamp < volume_limit_min)
				cmul = volume_scale_min / amp;	// new cmul (lower)
			wa[ch][ca].value = cmul;
			ca++;
			if(ca >= wac[ch])
				break;
			pds_ftoi(((float)cmul * (float)(delaj - 1) + (float)volmul) / (float)delaj, &cmul);	// large delaj -> overflow in 32-bit integer
		} while(1);
		if(cmul < volmul)
			savemul[ch] = cmul;
	}
}

static void modify_volume_wa_lq(PCM_CV_TYPE_S * pcm_sample, unsigned int samplenum, unsigned int channels)
{
	unsigned int i, ch, ac, cmul;

	for(ch = 0; ch < channels; ch++) {
		ac = 0;					// wave are counter
		for(i = ch; i < samplenum; i += channels) {
			if(i >= wa[ch][ac].begin) {	// new wave
				cmul = wa[ch][ac].value / mxvolum_mute_voldiv;
				ac++;
			}
			pcm_sample[i] = (PCM_CV_TYPE_S) ((long)pcm_sample[i] * cmul / mxvolum_volume_scale);
		}
	}
}

static void modify_volume_wa_hq(PCM_CV_TYPE_F * pcm_sample, unsigned int samplenum, unsigned int channels)
{
	const float sdiv = (float)mxvolum_volume_scale * (float)mxvolum_mute_voldiv;
	float fmul;
	unsigned int i, ch, ac;

	//fprintf(stdout,"vol0:%1.4f vol1:%1.4f \n",(float)savemul[0]/sdiv,(float)savemul[1]/sdiv);
	for(ch = 0; ch < channels; ch++) {
		i = ch;
#ifdef MIXER_LIMITER_CORRECT_BOUNDARY
		if((firstmaxsignpos[ch] > (channels + ch)) && (wa[ch][0].value < lastvalue[ch])) {
			unsigned int len = firstmaxsignpos[ch];
			float begincorr = (lastvalue[ch] - wa[ch][0].value) / sdiv / (float)(len - ch);

			fmul = (float)wa[ch][0].value / sdiv;
			//fprintf(stdout,"ch:%d len:%3d bc:%1.4f fmul:%1.4f cf:%1.5f d:%1.5f e:%1.5f\n",
			//ch,len,begincorr*(float)(len-i),fmul,(fmul+begincorr*(float)(len-i)),(float)lastvalue[ch]/sdiv,((fmul+begincorr*(float)(len-i))-(float)lastvalue[ch]/sdiv)*100.0);
			for(; i < firstmaxsignpos[ch]; i += channels)
				pcm_sample[i] = pcm_sample[i] * (fmul + begincorr * (float)(len - i));
		}
#endif
		ac = 0;					// wave counter
		for(; i < samplenum; i += channels) {
			if(i >= wa[ch][ac].begin) {	// new wave
				fmul = (float)wa[ch][ac].value / sdiv;
				ac++;
			}
			pcm_sample[i] *= fmul;
		}
#ifdef MIXER_LIMITER_CORRECT_BOUNDARY
		lastvalue[ch] = wa[ch][wac[ch] - 1].value;
#endif
	}
}

#ifdef MIXER_LIMITER_USE_DELAYBUF

static unsigned int limiter_get_processed_samplenum(unsigned int samplenum, unsigned int channels)
{
	unsigned int ch;

	for(ch = 0; ch < channels; ch++) {
		if(wac[ch]) {			// ???
			unsigned int begin = wa[ch][wac[ch] - 1].begin;
			if(begin < samplenum)
				samplenum = begin;
		}
		wa[ch][0].begin = ch;
	}

	return samplenum;
}

static void limiter_shift_wa_pcmsave(unsigned int proc_boundary, unsigned int proc_samplenum, unsigned int channels)
{
	unsigned int ch, acs, act;

	for(ch = 0; ch < channels; ch++) {
		wa[ch][0].begin = ch;
		wa[ch][0].maxsign = 0;
		if(proc_samplenum) {
			unsigned int volmul = VOL_volumes[ch] * mxvolum_volume_scale / 100;
			act = 0;
			for(acs = 0; acs < wac[ch]; acs++) {
				unsigned int area_end = wa[ch][acs + 1].begin;
				if(area_end > proc_boundary) {
					wa[ch][act].maxsign = wa[ch][acs].maxsign;
					if(!act) {
						savemul[ch] = wa[ch][acs].value;
						if(savemul[ch] > volmul)
							savemul[ch] = volmul;
					} else
						wa[ch][act].begin = wa[ch][acs].begin - proc_samplenum;
					act++;
				}
			}
			if(!act)
				act++;
			wac[ch] = act;
		} else {
			if(wac[ch])			// ???
				wa[ch][0].maxsign = wa[ch][wac[ch] - 1].maxsign;
			wac[ch] = 1;
		}
	}
}

#endif

static void limiter_main_lq(struct mpxplay_audioout_info_s *aui)
{
	aui->pcm16 = aui->pcm_sample;

#ifdef MIXER_LIMITER_USE_DELAYBUF
	if(pcmsave_storedsamples) {
		if((aui->samplenum + pcmsave_storedsamples) > pcmsave_bufsize) {
			pcmsave_storedsamples = 0;
			return;
		}
		pds_qmemcpyr(((char *)aui->pcm_sample) + pcmsave_storedsamples * sizeof(PCM_CV_TYPE_S), aui->pcm_sample, (aui->samplenum * sizeof(PCM_CV_TYPE_S) + 3) / 4);
		pds_memcpy(aui->pcm_sample, pcmsave_buffer, pcmsave_storedsamples * sizeof(PCM_CV_TYPE_S));
		aui->samplenum += pcmsave_storedsamples;
		pcmsave_storedsamples = 0;
	}
#endif

	get_wave_areas_lq(aui);
	calculate_max_wave_values(aui, aui->samplenum);
	modify_volume_wa_lq((PCM_CV_TYPE_S *) aui->pcm_sample, aui->samplenum, aui->chan_card);
}

static void limiter_main_hq(struct mpxplay_audioout_info_s *aui)
{
#ifdef MIXER_LIMITER_USE_DELAYBUF
	unsigned int proc_boundary, proc_samplenum;

	//fprintf(stdout,"-----------------------------------------------------------------\n");

	//fprintf(stdout,"-ps:%5d  beg0:%5d beg1:%5d  wac0:%4d wac1:%4d",pcmsave_storedsamples,wa[0][wac[0]-1].begin,wa[1][wac[1]-1].begin,wac[0],wac[1]);
	//if(pcmsave_storedsamples<wa[0][wac[0]-1].begin || pcmsave_storedsamples<(wa[1][wac[1]-1].begin-1))
	// fprintf(stdout," !!!!!");
	//fprintf(stdout,"\n");

	if(pcmsave_storedsamples) {
		if((aui->samplenum + pcmsave_storedsamples) > pcmsave_bufsize) {
			pcmsave_storedsamples = 0;
			return;
		}
		pds_qmemcpyr(((char *)aui->pcm_sample) + pcmsave_storedsamples * sizeof(PCM_CV_TYPE_F), aui->pcm_sample, aui->samplenum * sizeof(PCM_CV_TYPE_F) / 4);
		pds_qmemcpy(aui->pcm_sample, pcmsave_buffer, pcmsave_storedsamples * sizeof(PCM_CV_TYPE_F) / 4);
		aui->samplenum += pcmsave_storedsamples;
		pcmsave_storedsamples = 0;
	}
#endif

	get_wave_areas_hq_long(aui, (PCM_CV_TYPE_F *) aui->pcm_sample);
	calculate_max_wave_values(aui, aui->samplenum);

#ifdef MIXER_LIMITER_USE_DELAYBUF
	proc_boundary = limiter_get_processed_samplenum(aui->samplenum, aui->chan_card);
	proc_samplenum = proc_boundary - (proc_boundary % aui->chan_card);	// round down to channels

	//fprintf(stdout,"-----------------------------------------------------------------\n");
	//fprintf(stdout,"as:%5d ps:%5d ac0:%3d ac1:%3d l0:%4d l1:%4d\n",aui->samplenum,proc_samplenum,wac[0],wac[1],wa[0][wac[0]-1].begin,wa[1][wac[1]-1].begin);

	if(proc_samplenum && proc_samplenum <= aui->samplenum) {
		pcmsave_storedsamples = aui->samplenum - proc_samplenum;
		if(proc_samplenum < aui->samplenum)
			pds_qmemcpy(pcmsave_buffer, ((char *)aui->pcm_sample) + proc_samplenum * sizeof(PCM_CV_TYPE_F), pcmsave_storedsamples * sizeof(PCM_CV_TYPE_F) / 4);
		aui->samplenum = proc_samplenum;
	} else
		pcmsave_storedsamples = 0;
#endif

	modify_volume_wa_hq((PCM_CV_TYPE_F *) aui->pcm_sample, aui->samplenum, aui->chan_card);

#ifdef MIXER_LIMITER_USE_DELAYBUF
	limiter_shift_wa_pcmsave(proc_boundary, proc_samplenum, aui->chan_card);
#endif
}

static unsigned int limiter_alloc_wa(unsigned int samplenum)
{
	unsigned int ch;
	//samplenum=samplenum*2/2;
	if(samplenum < wa_max_areas)
		return 1;
	wa_max_areas = 0;
	if(!wa)
		wa = pds_calloc(PCM_MAX_CHANNELS, sizeof(*wa));
	if(!wa)
		return 0;
	for(ch = 0; ch < PCM_MAX_CHANNELS; ch++) {
		if(wa[ch])
			pds_free(wa[ch]);
		wa[ch] = pds_calloc(samplenum + 2, sizeof(struct wave_area_s));
		if(!wa[ch])
			return 0;
	}
	wa_max_areas = samplenum;
	return 1;
}

#ifdef MIXER_LIMITER_USE_DELAYBUF

static unsigned int limiter_alloc_hq(unsigned int samplenum, struct mpxplay_audioout_info_s *aui)
{
	if(MIXER_getstatus("MIX_HQ")) {
		pcmsave_storedsamples = 0;
		samplenum *= 2;			// we have to store max. 2 frames at once
		if(pcmsave_bufsize < samplenum) {
			pcmsave_bufsize = 0;
			if(pcmsave_buffer)
				pds_free(pcmsave_buffer);
			pcmsave_buffer = pds_malloc(samplenum * sizeof(PCM_CV_TYPE_F));
			if(!pcmsave_buffer)
				return 0;
			pcmsave_bufsize = samplenum;
		}
	} else {					// at LQHQSW
		if(pcmsave_storedsamples)
			cv_float_to_int16((short *)pcmsave_buffer, pcmsave_storedsamples, aui->mvp->frp0->infile_infos->audio_decoder_infos->infobits & ADI_FLAG_FPUROUND_CHOP);
	}
	return 1;
}

#endif

static void limiter_dealloc(void)
{
	unsigned int ch;
	if(wa) {
		for(ch = 0; ch < PCM_MAX_CHANNELS; ch++)
			if(wa[ch])
				pds_free(wa[ch]);
		pds_free(wa);
		wa = NULL;
	}
	wa_max_areas = 0;
#ifdef MIXER_LIMITER_USE_DELAYBUF
	if(pcmsave_buffer) {
		pds_free(pcmsave_buffer);
		pcmsave_buffer = NULL;
	}
	pcmsave_bufsize = 0;
#endif
}

static int mxvolum_limiter_init(struct mpxplay_audioout_info_s *aui, int inittype)
{
	long ch, samplenum, save_mutelen;

	if(!(MIXER_controlbits & MIXER_CONTROLBIT_LIMITER))
		return 0;

	switch (inittype) {
	case MIXER_INITTYPE_INIT:
		samplenum = PCM_BUFFER_SIZE / (PCM_MAX_BITS / 8);
		if(!limiter_alloc_wa(samplenum / PCM_MAX_CHANNELS))
			return 0;
#ifdef MIXER_LIMITER_USE_DELAYBUF
		if(!limiter_alloc_hq(samplenum, aui))
			return 0;
#endif
		break;
	case MIXER_INITTYPE_LQHQSW:
	case MIXER_INITTYPE_START:
	case MIXER_INITTYPE_REINIT:
		if((aui->freq_song >= PCM_MIN_FREQ) && aui->freq_card) {
			samplenum = mpxplay_infile_get_samplenum_per_frame(aui->freq_song);
			pds_ftoi((float)(2 * samplenum) * (float)aui->freq_card / (float)aui->freq_song, &samplenum);	// *2 : speed_control max expansion

			if(samplenum >= wa_max_areas)
				return 1;
			if(!limiter_alloc_wa(samplenum))
				return 0;
#ifdef MIXER_LIMITER_USE_DELAYBUF
			if(!limiter_alloc_hq(samplenum * aui->chan_card, aui))
				return 0;
#endif
		}

	case MIXER_INITTYPE_RESET:
		if(!wa_max_areas)
			return 0;

#ifdef MIXER_LIMITER_CORRECT_BOUNDARY
		for(ch = 0; ch < aui->chan_card; ch++)
			lastvalue[ch] = 0;
#endif

		if(inittype == MIXER_INITTYPE_LQHQSW)
			break;

#ifdef MIXER_LIMITER_USE_DELAYBUF
		pcmsave_storedsamples = 0;
#endif

		save_mutelen = MIXER_var_mutelen;
		MIXER_var_mutelen = 0;	// to avoid the effect of mute
		mixer_volumes_calc(aui);
		MIXER_var_mutelen = save_mutelen;
		for(ch = 0; ch < aui->chan_card; ch++) {
			if(!crossfadepart)
				savemul[ch] = VOL_volumes[ch] * mxvolum_volume_scale / 100;
			wac[ch] = 1;
			wa[ch][0].begin = ch;
			wa[ch][0].maxsign = 0;
		}
		break;

	case MIXER_INITTYPE_CLOSE:
		limiter_dealloc();
		break;
	}
	return 1;
}

static int mxvolum_limiter_checkvar(struct mpxplay_audioout_info_s *aui)
{
	if(MIXER_controlbits & MIXER_CONTROLBIT_LIMITER) {
		unsigned int hq_flag = MIXER_getstatus("MIX_HQ");
		if((mixer_volume_calcvol(aui) > 100)
		   || (hq_flag && (MIXER_getstatus("MIX_CROSSFADER")	// ???
						   || MIXER_getstatus("MIX_SURROUND")
						   || MIXER_getstatus("MIX_TONE_BASS")
			   )
		   )
#ifdef MIXER_LIMITER_USE_DELAYBUF
		   && (!hq_flag || pcmsave_buffer)
#endif
		   && wa_max_areas) {
			mxvolum_limiter_overflow = pow(10.0, (double)MIXER_var_limiter_overflow / 20.0);
			mxvolum_volume_scale = VOLUME_SCALE_DEFAULT / mxvolum_limiter_overflow;
			return 1;
		}
	}
	mxvolum_volume_scale = VOLUME_SCALE_DEFAULT;
	return 0;
}

one_mixerfunc_info MIXER_FUNCINFO_limiter = {
	"MIX_LIMITER",
	"mxlo",
	&MIXER_var_limiter_overflow,
	MIXER_INFOBIT_PARALLEL_DEPENDENCY,
	0, 18, 0, 1,
	&mxvolum_limiter_init,
	NULL,
	NULL,
	&mxvolum_limiter_checkvar,
	NULL
};

//--------------------------------------------------------------------------
//volume
static unsigned int mixer_volume_calcvol(struct mpxplay_audioout_info_s *aui)
{
	unsigned int loc_volume = MIXER_var_volume;
	/*if(aui->mvp && aui->mvp->frp0 && aui->mvp->frp0->infile_infos && aui->mvp->frp0->infile_infos->audio_decoder_infos){
	   float rg=(aui->mvp->frp0)->infile_infos->audio_decoder_infos->replaygain;
	   //fprintf(stdout,"rg:%2.2f \n",rg);
	   if(rg!=1.0)
	   loc_volume*=rg;
	   } */
	return loc_volume;
}

static void mixer_volumes_calc(struct mpxplay_audioout_info_s *aui)
{
	unsigned int ch, loc_volume = mixer_volume_calcvol(aui);
	for(ch = 0; ch < aui->chan_card; ch++)
		VOL_volumes[ch] = mixer_volume_balance_calcvol(ch, loc_volume);
}

static void mixer_volume_manual_lq(struct mpxplay_audioout_info_s *aui)
{
	short *pcm_s = aui->pcm_sample;
	unsigned int samplenum = aui->samplenum, channels = aui->chan_card;
	unsigned int ch;
	samplenum /= channels;

	for(ch = 0; ch < channels; ch++) {
		const long cmul = VOL_volumes[ch] * mxvolum_volume_scale / 100 / mxvolum_mute_voldiv;
		short *pcm = pcm_s + ch;
		unsigned int sn = samplenum;
		do {
			long c1 = ((long)pcm[0] * cmul) / mxvolum_volume_scale;
			if(c1 > MIXER_SCALE_MAX)
				pcm[0] = MIXER_SCALE_MAX;
			else if(c1 < MIXER_SCALE_MIN)
				pcm[0] = MIXER_SCALE_MIN;
			else
				pcm[0] = (short)c1;
			pcm += channels;
		} while(--sn);
	}
}

static void mixer_volume_manual_hq(struct mpxplay_audioout_info_s *aui)
{
	PCM_CV_TYPE_F *pcm_s = (PCM_CV_TYPE_F *) aui->pcm_sample;
	unsigned int samplenum = aui->samplenum, channels = aui->chan_card;
	unsigned int ch;
	samplenum /= channels;

	for(ch = 0; ch < channels; ch++) {
		const float cfvol = (float)VOL_volumes[ch] / 100.0f / (float)mxvolum_mute_voldiv;
		PCM_CV_TYPE_F *pcm = pcm_s + ch;
		unsigned int sn = samplenum;
		do {
			pcm[0] *= cfvol;
			pcm += channels;
		} while(--sn);
	}
}

static void mixer_volume_lq(struct mpxplay_audioout_info_s *aui)
{
	if(MIXER_FUNCINFO_limiter.infobits & MIXER_INFOBIT_ENABLED)
		limiter_main_lq(aui);
	else
		mixer_volume_manual_lq(aui);
}

static void mixer_volume_hq(struct mpxplay_audioout_info_s *aui)
{
	if(MIXER_FUNCINFO_limiter.infobits & MIXER_INFOBIT_ENABLED)
		limiter_main_hq(aui);
	else
		mixer_volume_manual_hq(aui);
}

static int mixer_volume_checkvar(struct mpxplay_audioout_info_s *aui)
{
	mixer_volumes_calc(aui);
	if(MIXER_var_mutelen || mixer_volume_calcvol(aui) != MIXER_FUNCINFO_volume.var_center || mixer_volume_balance_checkvar(aui)
	   || mxvolum_limiter_checkvar(aui)) {
		return 1;
	}
	return 0;
}

static void mixer_volume_manual_setvar(struct mpxplay_audioout_info_s *aui, unsigned int setmode, int value)
{
	one_mixerfunc_info *infop = &MIXER_FUNCINFO_volume;
	int step = value * infop->var_step;
	int newvol = infop->var_center;
	int currvol = *(infop->variablep);

	switch (setmode) {
	case MIXER_SETMODE_RELATIVE:
		if((currvol < 14) || (currvol + step) < 14)
			newvol = currvol + value;
		else if(currvol <= 100)
			newvol = currvol + step;
		else
			newvol = (value > 0) ? ((currvol * 1059 + 500) / 1000) : ((currvol * 944 + 500) / 1000);	// +0.5:-0.5 dB
		if((currvol < infop->var_center && newvol > infop->var_center) || (currvol > infop->var_center && newvol < infop->var_center))
			newvol = infop->var_center;
		break;
	case MIXER_SETMODE_ABSOLUTE:
		newvol = value;
		break;
	case MIXER_SETMODE_RESET:
		newvol = infop->var_center;
		break;
	}
	if(newvol < infop->var_min)
		newvol = infop->var_min;
	else if(newvol > infop->var_max)
		newvol = infop->var_max;
	MIXER_var_volume = newvol;
}

one_mixerfunc_info MIXER_FUNCINFO_volume = {
	"MIX_VOLUME",
	"mxv",
	&MIXER_var_volume,
	MIXER_INFOBIT_PARALLEL_DEPENDENCY,	//|MIXER_INFOBIT_EXTERNAL_DEPENDENCY, // mute,balance,limiter
	0, 700, 100, 6,
	NULL,
	&mixer_volume_lq,
	&mixer_volume_hq,
	&mixer_volume_checkvar,
	&mixer_volume_manual_setvar
};
