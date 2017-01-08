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
//function: crossfade (mixer) routines

#include "au_mixer.h"
#include "newfunc\newfunc.h"

extern unsigned int crossfadepart;

static int MIXER_var_pcmcrossfade;

static void clear_samples(char *pcm, unsigned int begin, unsigned int end, unsigned int bytespersample)
{
	if(end > begin)
		pds_memset(pcm + begin * bytespersample, 0, (end - begin) * bytespersample);
}

static short *crossfade_config(struct mpxplay_audioout_info_s *aui, long *cf_v1, long *cf_v2)
{
	struct mainvars *mvp = aui->mvp;
	struct crossfade_info *cfi = mvp->cfi;
	struct frame *frp0 = mvp->frp0, *frp1 = frp0 + 1;
	long v, v1, v2;
	short *cf_pcm2;

	if(crossfadepart == CROSS_FADE) {
		aui->samplenum = max(frp0->pcmout_storedsamples, frp1->pcmout_storedsamples);
		cf_pcm2 = (short *)(frp1->pcmout_buffer);

		// if the length of last block (of the 2 files) do not match (ogg,wav,ape files)
		clear_samples((char *)aui->pcm_sample, frp0->pcmout_storedsamples, aui->samplenum, aui->bytespersample_mixer);	// (at end of file 1.)
		clear_samples((char *)cf_pcm2, frp1->pcmout_storedsamples, aui->samplenum, aui->bytespersample_mixer);	// (at end of file 2.)(rare)

	} else {
		cf_pcm2 = aui->pcm_sample;
	}

	if(crossfadepart & CFT_FADEOUT) {
		struct frame *frp = (crossfadepart & CROSS_IN) ? frp1 : frp0;
		v = cfi->crossfadeend - frp->frameNum;
		if(v > 0) {
			if(cfi->crossfadetype & CFT_FADEOUT) {
				v1 = (v << 7) / cfi->crossfade_out_len;
				if(v1 > 128)
					v1 = 128;
			} else
				v1 = 128;
		} else {
			v1 = 0;
		}
	} else
		v1 = 0;
	if(crossfadepart & CFT_FADEIN) {
		unsigned long index_pos = frp0->frameNum - frp0->index_start;
		v2 = v1;
		v1 = 128;
		if(index_pos < cfi->crossfade_in_len) {
			if(cfi->crossfadetype & CFT_FADEIN)
				v1 = (index_pos << 7) / cfi->crossfade_in_len;
		}
	} else
		v2 = 0;

	*cf_v1 = v1;
	*cf_v2 = v2;

	return cf_pcm2;
}

//---------------------------------------------------------------------------
static void mixer_crossmix_lq(struct mpxplay_audioout_info_s *aui)
{
	unsigned int i;
	short *pcm1, *pcm2;
	long cf_v1, cf_v2;

	pcm2 = crossfade_config(aui, &cf_v1, &cf_v2);
	pcm1 = aui->pcm_sample;
	i = aui->samplenum;
	do {
		register long c1 = (((long)pcm1[0] * cf_v1) / 128) + (((long)pcm2[0] * cf_v2) / 128);
		if(c1 > MIXER_SCALE_MAX)
			c1 = MIXER_SCALE_MAX;
		else if(c1 < MIXER_SCALE_MIN)
			c1 = MIXER_SCALE_MIN;
		pcm1[0] = (short)c1;
		pcm1++;
		pcm2++;
	} while(--i);
}

static void mixer_crossmix_hq(struct mpxplay_audioout_info_s *aui)
{
	unsigned int i;
	PCM_CV_TYPE_F *pcm1, *pcm2;
	float fv1, fv2;
	long cf_v1, cf_v2;

	pcm2 = (PCM_CV_TYPE_F *) crossfade_config(aui, &cf_v1, &cf_v2);
	pcm1 = (PCM_CV_TYPE_F *) aui->pcm_sample;
	fv1 = (float)cf_v1 / 128.0;
	fv2 = (float)cf_v2 / 128.0;
	i = aui->samplenum;
	do {
		pcm1[0] = pcm1[0] * fv1 + pcm2[0] * fv2;
		pcm1++;
		pcm2++;
	} while(--i);
}

one_mixerfunc_info MIXER_FUNCINFO_crossfader = {
	"MIX_CROSSFADER",
	NULL,
	&MIXER_var_pcmcrossfade,
	MIXER_INFOBIT_SWITCH,
	0, 1, 0, 0,
	NULL,
	&mixer_crossmix_lq,
	&mixer_crossmix_hq,
	NULL,
	NULL
};
