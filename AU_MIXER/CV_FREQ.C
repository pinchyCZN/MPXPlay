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
//function: speed/freq control

#include "au_mixer.h"
#include "newfunc\newfunc.h"

#ifdef MPXPLAY_LINK_FULL
#define MIXER_SPEED_LQ_INTERPOLATION 1	// better but slower
#endif

#define CVFREQ_USE_ASM 1

static int MIXER_var_speed;
one_mixerfunc_info MIXER_FUNCINFO_speed;

static PCM_CV_TYPE_F *cv_freq_buffer;
static unsigned int cv_freq_bufsize, mx_sp_begin;
static unsigned int mx_sp_100sw1000_tested, mx_sp_1000_laststatus;

//--------------------------------------------------------------------------
void asm_cv_freq_floor(void);
void asm_cv_freq_hq(void);

static void mixer_speed_hq(struct mpxplay_audioout_info_s *aui)
{
	static float inpos;
	unsigned long samplenum = aui->samplenum, channels = aui->chan_card;
	const float instep = (float)MIXER_var_speed / (float)MIXER_FUNCINFO_speed.var_center * (float)aui->freq_song / (float)aui->freq_card;
	const float inend = samplenum / channels;
	PCM_CV_TYPE_F *pcm = (PCM_CV_TYPE_F *) aui->pcm_sample, *intmp = cv_freq_buffer;
	unsigned long savesamplenum = channels, ipi, fpucontrolword_save;

	if(samplenum > cv_freq_bufsize)
		return;

	if(mx_sp_begin) {			// to avoid a click at start
		pds_qmemcpy(intmp, pcm, savesamplenum);
		mx_sp_begin = 0;
	}

	pds_qmemcpy((intmp + savesamplenum), pcm, samplenum);

#if defined(CVFREQ_USE_ASM) && defined(__WATCOMC__)
#pragma aux asm_cv_freq_hq=\
 "fstcw word ptr fpucontrolword_save"\
 "mov ax,word ptr fpucontrolword_save"\
 "or ax,0x0c00"\
 "mov word ptr ipi,ax"\
 "fldcw word ptr ipi"\
 "fld dword ptr inpos"\
 "mov ecx,dword ptr channels"\
 "mov esi,dword ptr pcm"\
 "back1:"\
  "fld st"\
  "frndint"\
  "fist dword ptr ipi"\
  "fsubr st,st(1)"\
  "mov eax,dword ptr ipi"\
  "imul ecx"\
  "fld1"\
  "fsub st,st(1)"\
  "shl eax,2"\
  "add eax,dword ptr intmp"\
  "lea ebx,dword ptr [eax+ecx*4]"\
  "mov edx,ecx"\
  "back2:"\
   "fld dword ptr [eax]"\
   "fmul st,st(1)"\
   "add eax,4"\
   "fld dword ptr [ebx]"\
   "fmul st,st(3)"\
   "add ebx,4"\
   "fadd"\
   "fstp dword ptr [esi]"\
   "add esi,4"\
   "dec edx"\
  "jnz back2"\
  "fstp st"\
  "fstp st"\
  "fadd dword ptr instep"\
  "fcom dword ptr inend"\
  "fnstsw ax"\
  "sahf"\
 "jb back1"\
 "fsub dword ptr inend"\
 "mov dword ptr pcm,esi"\
 "fstp dword ptr inpos"\
 "fldcw word ptr fpucontrolword_save"\
 modify[eax ebx ecx edx esi];
	asm_cv_freq_hq();

#else							// !CVFREQ_USE_ASM

#ifdef __WATCOMC__
	pds_fpu_setround_chop();	// to asm_cv_freq_floor() !
#endif
	do {
		float m1, m2;
		unsigned int ipi, ch;
		PCM_CV_TYPE_F *intmp1, *intmp2;
#ifdef __WATCOMC__
#pragma aux asm_cv_freq_floor=\
  "fld dword ptr inpos"\
  "fistp dword ptr ipi"\
  modify[];
		asm_cv_freq_floor();
#else
		ipi = (long)floor(inpos);
#endif
		m2 = inpos - (float)ipi;
		m1 = 1.0f - m2;
		ch = channels;
		ipi *= ch;
		intmp1 = intmp + ipi;
		intmp2 = intmp1 + ch;
		do {
			*pcm++ = (*intmp1++) * m1 + (*intmp2++) * m2;
		} while(--ch);
		inpos += instep;
	} while(inpos < inend);
	inpos -= inend;
#ifdef __WATCOMC__
	pds_fpu_setround_near();	// restore default
#endif

#endif							//CVFREQ_USE_ASM

	pds_qmemcpy(cv_freq_buffer, (cv_freq_buffer + aui->samplenum), savesamplenum);
	aui->samplenum = pcm - ((PCM_CV_TYPE_F *) aui->pcm_sample);
}

#ifdef MIXER_SPEED_LQ_INTERPOLATION

static void mixer_speed_lq(struct mpxplay_audioout_info_s *aui)
{
	static unsigned long inpos_save;
	unsigned int samplenum = aui->samplenum, channels = aui->chan_card;
	unsigned int inpos, instep, inend = (samplenum / channels) << 16;
	PCM_CV_TYPE_S *pcm = (PCM_CV_TYPE_S *) aui->pcm_sample;
	PCM_CV_TYPE_S *intmp = (PCM_CV_TYPE_S *) cv_freq_buffer;
	float fstep;

	if(samplenum > cv_freq_bufsize)
		return;

	fstep = ((float)MIXER_var_speed / (float)MIXER_FUNCINFO_speed.var_center * (float)aui->freq_song / (float)aui->freq_card * 65536.0);
	pds_ftoi(fstep, (long *)&instep);

	if(mx_sp_begin) {
		pds_memcpy(intmp, pcm, channels * sizeof(PCM_CV_TYPE_S));
		mx_sp_begin = 0;
		inpos_save = 0;
	}

	pds_memcpy((intmp + channels), pcm, samplenum * sizeof(PCM_CV_TYPE_S));

	inpos = inpos_save;

	if(channels == 2) {
		do {
			long m1, m2;
			unsigned int ip1;

			m2 = inpos & 0x0000ffff;
			m1 = 65536 - m2;
			ip1 = inpos >> 16;
			ip1 <<= 1;
			pcm[0] = ((long)intmp[ip1] * m1 + (long)intmp[ip1 + 2] * m2) >> 16;
			pcm[1] = ((long)intmp[ip1 + 1] * m1 + (long)intmp[ip1 + 3] * m2) >> 16;
			pcm += 2;
			inpos += instep;
		} while(inpos < inend);
	} else {
		do {
			long m1, m2;
			unsigned int ip1, ip2;
			unsigned int ch;

			m2 = inpos & 0x0000ffff;
			m1 = 65536 - m2;
			ip1 = inpos >> 16;
			ch = channels;
			ip1 *= ch;
			ip2 = ip1 + ch;
			do {
				*pcm++ = ((long)intmp[ip1] * m1 + (long)intmp[ip2] * m2) >> 16;
				ip1++;
				ip2++;
			} while(--ch);
			inpos += instep;
		} while(inpos < inend);
	}

	inpos -= inend;
	inpos_save = inpos;
	pds_memcpy(intmp, (intmp + aui->samplenum), channels * sizeof(PCM_CV_TYPE_S));
	aui->samplenum = pcm - ((PCM_CV_TYPE_S *) aui->pcm_sample);
}

#else

static void mixer_speed_lq(struct mpxplay_audioout_info_s *aui)
{
	unsigned int samplenum = aui->samplenum, channels = aui->chan_card;
	PCM_CV_TYPE_S *pcms = aui->pcm_sample;
	int istep;
	unsigned int ipos, ipos16;
	float fstep;

	if(samplenum > cv_freq_bufsize)
		return;

	fstep = (float)MIXER_var_speed / (float)MIXER_FUNCINFO_speed.var_center * (float)aui->freq_song / (float)aui->freq_card * 65536.0f;

	pds_ftoi(fstep, (long *)&istep);

	if(channels == 2) {
		long *pcm_sample = (long *)pcms;
		long *pcmc = (long *)cv_freq_buffer;
		samplenum >>= 1;
		ipos = 32768;
		ipos16 = 0;
		do {
			register long a1 = pcm_sample[ipos16];
			(*pcmc++) = a1;
			ipos += istep;
			ipos16 = ipos >> 16;
		} while(ipos16 < samplenum);
		aui->samplenum = (pcmc - ((long *)cv_freq_buffer)) << 1;
	} else {
		unsigned int ch;
		PCM_CV_TYPE_S *pcm_sample, *pcmc;
		samplenum /= channels;
		for(ch = 0; ch < channels; ch++) {
			pcm_sample = pcms + ch;
			pcmc = ((PCM_CV_TYPE_S *) cv_freq_buffer) + ch;
			ipos = 32768;
			ipos16 = 0;
			do {
				register PCM_CV_TYPE_S a1 = pcm_sample[ipos16];
				(*pcmc++) = a1;
				ipos += istep;
				ipos16 = ipos >> 16;
			} while(ipos16 < samplenum);
		}
		aui->samplenum = pcmc - ((PCM_CV_TYPE_S *) cv_freq_buffer);
		aui->samplenum /= channels;	// rounding to channels
		aui->samplenum *= channels;
	}
	pds_memcpy((char *)pcms, (char *)cv_freq_buffer, aui->samplenum * sizeof(PCM_CV_TYPE_S));
}

#endif

static unsigned int mixer_speed_alloc(unsigned int samplenum)
{
	if(cv_freq_bufsize < samplenum) {
		cv_freq_bufsize = samplenum;
		if(cv_freq_buffer)
			pds_free(cv_freq_buffer);
		cv_freq_buffer = (PCM_CV_TYPE_F *) pds_malloc(cv_freq_bufsize * sizeof(PCM_CV_TYPE_F));
		if(!cv_freq_buffer) {
			cv_freq_bufsize = 0;
			return 0;
		}
	}
	return 1;
}

static void mixer_speed_dealloc(void)
{
	if(cv_freq_buffer) {
		pds_free(cv_freq_buffer);
		cv_freq_buffer = NULL;
		cv_freq_bufsize = 0;
	}
}

static void mixer_speed_chk_var_speed(struct mpxplay_audioout_info_s *aui)
{
	if(mx_sp_1000_laststatus != (aui->mixer_infobits & AUINFOS_MIXERCTRLBIT_SPEED1000)) {
		mx_sp_1000_laststatus = aui->mixer_infobits & AUINFOS_MIXERCTRLBIT_SPEED1000;
		mx_sp_100sw1000_tested = 0;
	}
	if(aui->mixer_infobits & AUINFOS_MIXERCTRLBIT_SPEED1000) {
		MIXER_FUNCINFO_speed.var_min = 500;
		MIXER_FUNCINFO_speed.var_max = 9999;
		MIXER_FUNCINFO_speed.var_center = 1000;
		if(MIXER_var_speed < MIXER_FUNCINFO_speed.var_min)
			if(!mx_sp_100sw1000_tested)
				MIXER_var_speed *= 10;
			else
				MIXER_var_speed = MIXER_FUNCINFO_speed.var_min;
		if(MIXER_var_speed > MIXER_FUNCINFO_speed.var_max)
			MIXER_var_speed = MIXER_FUNCINFO_speed.var_max;
		funcbit_enable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_SPEED1000);
	} else {
		MIXER_FUNCINFO_speed.var_min = 50;
		MIXER_FUNCINFO_speed.var_max = 999;
		MIXER_FUNCINFO_speed.var_center = 100;
		if(MIXER_var_speed > MIXER_FUNCINFO_speed.var_max)
			if(!mx_sp_100sw1000_tested)
				MIXER_var_speed /= 10;
			else
				MIXER_var_speed = MIXER_FUNCINFO_speed.var_max;
		if(MIXER_var_speed < MIXER_FUNCINFO_speed.var_min)
			MIXER_var_speed = MIXER_FUNCINFO_speed.var_min;
		funcbit_disable(aui->mixer_infobits, AUINFOS_MIXERINFOBIT_SPEED1000);
	}
	mx_sp_100sw1000_tested = 1;
}

static int mixer_speed_init(struct mpxplay_audioout_info_s *aui, int inittype)
{
	switch (inittype) {
	case MIXER_INITTYPE_INIT:
		if(!mixer_speed_alloc(PCM_BUFFER_SIZE / (PCM_MAX_BITS / 8) + PCM_MAX_CHANNELS))
			return 0;
		break;
	case MIXER_INITTYPE_START:
	case MIXER_INITTYPE_REINIT:
		if(aui->freq_song >= PCM_MIN_FREQ) {
			one_mixerfunc_info *infop_speed = &MIXER_FUNCINFO_speed;
			float speed_expansion;
			long samplenum;

			if(infop_speed)
				speed_expansion = (float)infop_speed->var_center / (float)infop_speed->var_min;	// *2
			else
				speed_expansion = 1.0;

			samplenum = mpxplay_infile_get_samplenum_per_frame(aui->freq_song);
			samplenum++;
			samplenum = speed_expansion * (float)((samplenum + 128) * max(aui->chan_song, aui->chan_card))	// *2 : speed_control max expansion
				* (float)(max(aui->freq_card, aui->freq_song)) / (float)aui->freq_song;
			if(cv_freq_bufsize >= samplenum)
				return 1;
			if(!mixer_speed_alloc(samplenum))
				return 0;
		}
	case MIXER_INITTYPE_RESET:
	case MIXER_INITTYPE_LQHQSW:
		mx_sp_begin = 1;
		break;

	case MIXER_INITTYPE_CLOSE:
		mixer_speed_dealloc();
		break;
	}
	return 1;
}

static int mixer_speed_checkvar(struct mpxplay_audioout_info_s *aui)
{
	float freq = (aui->freq_song < 22050) ? 22050.0 : (float)aui->freq_song;
	long speed_center, speed_cur;

	speed_center = MIXER_FUNCINFO_speed.var_center;
	speed_cur = MIXER_var_speed;
	if(speed_cur < speed_center)
		speed_cur = speed_center;

	aui->int08_decoder_cycles =
		(int)(((float)speed_cur + (float)(speed_center / 2)) / (float)speed_center * freq / (float)PCM_OUTSAMPLES) * (float)INT08_DIVISOR_NEW / (float)(INT08_CYCLES_DEFAULT * INT08_DIVISOR_DEFAULT) +
		1;

	if((MIXER_var_speed != speed_center) || (aui->freq_song != aui->freq_card))
		return 1;
	return 0;
}

static void mixer_speed_setvar(struct mpxplay_audioout_info_s *aui, unsigned int setmode, int value)
{
	switch (setmode) {
	case MIXER_SETMODE_ABSOLUTE:
		MIXER_var_speed = value;
		break;
	case MIXER_SETMODE_RELATIVE:
		MIXER_var_speed += value;
		break;
	case MIXER_SETMODE_RESET:
		MIXER_var_speed = MIXER_FUNCINFO_speed.var_center;
		break;
	}
	mixer_speed_chk_var_speed(aui);
}

one_mixerfunc_info MIXER_FUNCINFO_speed = {
	"MIX_SPEED",
	"mxsp",
	&MIXER_var_speed,
	MIXER_INFOBIT_EXTERNAL_DEPENDENCY,	// aui->freq_song
	50, 999, 100, 1,
	&mixer_speed_init,
	&mixer_speed_lq,
	&mixer_speed_hq,
	&mixer_speed_checkvar,
	&mixer_speed_setvar
};

//---------------------------------------------------------------

one_mixerfunc_info MIXER_FUNCINFO_seekspeed;
static int seekspeed_base, seekspeed_extra, seekspeed_counter;
extern unsigned int refdisp;
#include "display\display.h"

static void mixer_speed_seekspeed_lq(struct mpxplay_audioout_info_s *aui)
{
	if(seekspeed_counter)
		seekspeed_counter--;
	else {
		//MIXER_setfunction("MIX_SPEEDSEEK",MIXER_SETMODE_RESET,0);
		MIXER_setfunction("MIX_SPEEDSEEK", MIXER_SETMODE_RELATIVE, -15);
		refdisp |= RDT_OPTIONS;
	}
}

static void mixer_speed_seekspeed_setvar(struct mpxplay_audioout_info_s *aui, unsigned int setmode, int value)
{
	if(!seekspeed_extra) {
		seekspeed_base = MIXER_getvalue("MIX_SPEED");
		seekspeed_counter = 20;
	} else if(value > 0) {
		if(!funcbit_smp_test(aui->card_infobits, AUINFOS_CARDINFOBIT_DMAFULL)) {
			if(funcbit_smp_test(aui->card_infobits, AUINFOS_CARDINFOBIT_DMAUNDERRUN)) {
				value = -5;
				seekspeed_counter = 1;
			} else
				seekspeed_counter = 6;
		} else
			seekspeed_counter = 12;
	} else
		seekspeed_counter = 0;
	if(value <= 0 || funcbit_smp_test(aui->card_infobits, AUINFOS_CARDINFOBIT_DMAFULL)) {
		int spde = seekspeed_extra;
		switch (setmode) {
		case MIXER_SETMODE_ABSOLUTE:
			spde = value;
			break;
		case MIXER_SETMODE_RELATIVE:
			spde += value;
			break;
		case MIXER_SETMODE_RESET:
			spde = 0;
			break;
		}
		if(spde < 0)
			spde = 0;
		if(spde > MIXER_FUNCINFO_seekspeed.var_max)
			spde = MIXER_FUNCINFO_seekspeed.var_max;
		seekspeed_extra = spde;
		if(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_SPEED1000)
			spde *= 10;
		MIXER_setfunction("MIX_SPEED", MIXER_SETMODE_ABSOLUTE, seekspeed_base + spde);
	}
	MIXER_setfunction("MIX_MUTE", MIXER_SETMODE_ABSOLUTE, (seekspeed_extra) ? 48 : 0);
}

one_mixerfunc_info MIXER_FUNCINFO_seekspeed = {
	"MIX_SPEEDSEEK",
	NULL,
	&seekspeed_extra,
	0,
	0, 890, 0, 10,
	NULL,
	&mixer_speed_seekspeed_lq,
	&mixer_speed_seekspeed_lq,
	NULL,
	&mixer_speed_seekspeed_setvar
};
