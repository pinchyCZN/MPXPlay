//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: channel conversions

#include "au_mixer.h"

int MIXER_var_swapchan;

unsigned int cv_channels_1_to_n(PCM_CV_TYPE_S * pcm_sample, unsigned int samplenum, unsigned int newchannels, unsigned int bytespersample)
{
	register unsigned int i, ch, b;
	PCM_CV_TYPE_C *pcms = ((PCM_CV_TYPE_C *) pcm_sample) + (samplenum * bytespersample);
	PCM_CV_TYPE_C *pcmt = ((PCM_CV_TYPE_C *) pcm_sample) + (samplenum * bytespersample * newchannels);

	for(i = samplenum; i; i--) {
		pcms -= bytespersample;
		for(ch = newchannels; ch; ch--) {
			pcmt -= bytespersample;
			for(b = 0; b < bytespersample; b++)
				pcmt[b] = pcms[b];
		}
	}
	return (samplenum * newchannels);
}

unsigned int cv_channels_remap(PCM_CV_TYPE_S * pcm_sample, unsigned int samplenum,
							   unsigned int channelnum_in, mpxp_uint8_t * chanmatrix_in, unsigned int channelnum_out, mpxp_uint8_t * chanmatrix_out, unsigned int bytespersample)
{
	unsigned int i, c, outsamplenum = 0;
	PCM_CV_TYPE_C *pcms_in = ((PCM_CV_TYPE_C *) pcm_sample);
	PCM_CV_TYPE_C *pcms_out = ((PCM_CV_TYPE_C *) pcm_sample);
	mpxp_uint8_t outchans[MPXPLAY_PCMOUTCHAN_MAX * sizeof(PCM_CV_TYPE_MAX)];
	//char sout[100];

	if((channelnum_in > MPXPLAY_PCMOUTCHAN_MAX) || (channelnum_out > MPXPLAY_PCMOUTCHAN_MAX))
		return samplenum;

	// !!! it might work incorrectly if no chanmatrix_in or chanmatrix_out

	if(channelnum_out <= channelnum_in) {	// shrink or same channelnum
		for(i = 0; i < samplenum; i += channelnum_in) {
			pds_memset(outchans, 0, sizeof(outchans));
			if(chanmatrix_in) {
				for(c = 0; c < channelnum_in; c++) {
					if(chanmatrix_in[c] < MPXPLAY_PCMOUTCHAN_MAX)
						pds_memcpy(&outchans[chanmatrix_in[c] * bytespersample], pcms_in, bytespersample);
					pcms_in += bytespersample;
				}
			} else {
				pds_memcpy(outchans, pcms_in, channelnum_in * bytespersample);
				pcms_in += channelnum_in * bytespersample;
			}
			if(chanmatrix_out) {
				for(c = 0; c < channelnum_out; c++) {
					if(chanmatrix_out[c] < MPXPLAY_PCMOUTCHAN_MAX)
						pds_memcpy(pcms_out, &outchans[chanmatrix_out[c] * bytespersample], bytespersample);
					else
						pds_memset(pcms_out, 0, bytespersample);
					pcms_out += bytespersample;
				}
			} else {
				pds_memcpy(pcms_out, outchans, channelnum_out * bytespersample);
				pcms_out += channelnum_out * bytespersample;
			}
			outsamplenum += channelnum_out;
		}
	} else {					// expand channelnum
		pcms_in += samplenum * bytespersample;
		pcms_out += (samplenum / channelnum_in) * channelnum_out * bytespersample;
		for(i = 0; i < samplenum; i += channelnum_in) {
			pds_memset(outchans, 0, sizeof(outchans));
			if(chanmatrix_in) {
				c = channelnum_in;
				do {
					c--;
					pcms_in -= bytespersample;
					if(chanmatrix_in[c] < MPXPLAY_PCMOUTCHAN_MAX)
						pds_memcpy(&outchans[chanmatrix_in[c] * bytespersample], pcms_in, bytespersample);
				} while(c > 0);
			} else {
				pcms_in -= channelnum_in * bytespersample;
				pds_memcpy(outchans, pcms_in, channelnum_in * bytespersample);
			}
			if(chanmatrix_out) {
				c = channelnum_out;
				do {
					c--;
					pcms_out -= bytespersample;
					if(chanmatrix_out[c] < MPXPLAY_PCMOUTCHAN_MAX)
						pds_memcpy(pcms_out, &outchans[chanmatrix_out[c] * bytespersample], bytespersample);
					else
						pds_memset(pcms_out, 0, bytespersample);
				} while(c > 0);
			} else {
				pcms_out -= channelnum_out * bytespersample;
				pds_memcpy(pcms_out, outchans, channelnum_out * bytespersample);
			}
			outsamplenum += channelnum_out;
		}
	}

	/*if(chanmatrix_in)
	   sprintf(sout,"n:%d i:%d o:%d %d %d %d %d %d %d %d %d",outsamplenum,channelnum_in,channelnum_out,chanmatrix_in[0],chanmatrix_in[1],chanmatrix_in[2],chanmatrix_in[3],chanmatrix_in[4],chanmatrix_in[5],chanmatrix_in[6],chanmatrix_in[7]);
	   else
	   sprintf(sout,"sn:%d chi:%d cho:%d %8.8X %8.8X %8.8X",outsamplenum,channelnum_in,channelnum_out,pcm_sample,pcms_in,pcms_out);
	   display_message(0,0,sout);
	   if(chanmatrix_out)
	   sprintf(sout,"n:%d i:%d o:%d %d %d %d %d %d %d %d %d",outsamplenum,channelnum_in,channelnum_out,chanmatrix_out[0],chanmatrix_out[1],chanmatrix_out[2],chanmatrix_out[3],chanmatrix_out[4],chanmatrix_out[5],chanmatrix_out[6],chanmatrix_out[7]);
	   else
	   sprintf(sout,"sn:%d chi:%d cho:%d %8.8X %8.8X %8.8X",outsamplenum,channelnum_in,channelnum_out,pcm_sample,pcms_in,pcms_out);
	   display_message(1,0,sout); */
	return outsamplenum;
}

//#define CVCHAN_USE_SURROUND 1

// channel downmix via adi->chanmatrix
unsigned int cv_channels_downmix(PCM_CV_TYPE_S * pcm_sample, unsigned int samplenum, unsigned int channelnum_in, mpxp_uint8_t * chanmatrix_in, unsigned int channelnum_out,
								 mpxp_uint8_t * chanmatrix_out, struct mpxplay_audioout_info_s *aui)
{
	register unsigned int i, c, outsamplenum = 0;
	PCM_CV_TYPE_S *pcms_in, *pcms_out;
	PCM_CV_TYPE_F *pcmf_in, *pcmf_out;
#ifdef CVCHAN_USE_SURROUND
	unsigned int surround;
	PCM_CV_TYPE_F sdata;
#endif
	PCM_CV_TYPE_F outchans_f[MPXPLAY_PCMOUTCHAN_MAX];

	if(!chanmatrix_in)
		return samplenum;

	pcms_in = pcms_out = pcm_sample;
	pcmf_in = pcmf_out = (PCM_CV_TYPE_F *) pcm_sample;

	if(channelnum_out <= 2) {	// multichannel to mono or stereo downmix
		for(i = 0; i < samplenum; i += channelnum_in) {
#ifdef CVCHAN_USE_SURROUND
			surround = 0;
			sdata = 0.0;
#endif
			pds_memset(outchans_f, 0, sizeof(outchans_f));
			for(c = 0; c < channelnum_in; c++) {
				PCM_CV_TYPE_F data;
				if(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_FCONV)
					data = *pcmf_in++;
				else
					data = *pcms_in++;
				switch (chanmatrix_in[c]) {
				case MPXPLAY_PCMOUTCHAN_FRONT_LEFT:
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] += data;
					break;
				case MPXPLAY_PCMOUTCHAN_FRONT_RIGHT:
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT] += data;
					break;
				case MPXPLAY_PCMOUTCHAN_FRONT_CENTER:
					data *= 0.5;
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] += data;
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT] += data;
					break;
#ifdef CVCHAN_USE_SURROUND
				case MPXPLAY_PCMOUTCHAN_REAR_LEFT:
					sdata += data;
					surround = 1;
					break;
				case MPXPLAY_PCMOUTCHAN_REAR_RIGHT:
					sdata += data;
					surround = 1;
					break;
				case MPXPLAY_PCMOUTCHAN_REAR_CENTER:
					data *= 0.5;
					sdata += data;
					surround = 1;
					break;
#else
				case MPXPLAY_PCMOUTCHAN_REAR_LEFT:
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] += data;
					break;
				case MPXPLAY_PCMOUTCHAN_REAR_RIGHT:
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT] += data;
					break;
				case MPXPLAY_PCMOUTCHAN_REAR_CENTER:
					data *= 0.5;
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] += data;
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT] += data;
					break;
#endif
				case MPXPLAY_PCMOUTCHAN_SIDE_LEFT:
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] += data;
					break;
				case MPXPLAY_PCMOUTCHAN_SIDE_RIGHT:
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT] += data;
					break;
				case MPXPLAY_PCMOUTCHAN_LFE:
					data *= 0.5;
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] += data;
					outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT] += data;
					break;
				}
			}
#ifdef CVCHAN_USE_SURROUND
			if(surround) {		// dolby surround
				sdata *= 0.5;
				outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] -= sdata;
				outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT] += sdata;
			}
#endif
			if(channelnum_out == 1)
				outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_LEFT] += outchans_f[MPXPLAY_PCMOUTCHAN_FRONT_RIGHT];
			for(c = 0; c < channelnum_out; c++) {
				if(aui->mixer_infobits & AUINFOS_MIXERINFOBIT_FCONV)
					*pcmf_out++ = outchans_f[c];
				else
					pds_ftoi(outchans_f[c], pcms_out++);
			}
			outsamplenum += channelnum_out;
		}

	} else {					// channel remapping only (pcmdec to pcmout)

		outsamplenum = cv_channels_remap(pcm_sample, samplenum, channelnum_in, chanmatrix_in, channelnum_out, chanmatrix_out, aui->bytespersample_mixer);

	}
	return outsamplenum;
}

//-------------------------------------------------------------------------
static void mixer_swapchan_lqhq(struct mpxplay_audioout_info_s *aui)
{
	unsigned int samplenum = aui->samplenum, bytespersample = aui->bytespersample_mixer;
	unsigned int b, chskip = (aui->chan_card - 1) * bytespersample;
	PCM_CV_TYPE_C *left = (PCM_CV_TYPE_C *) aui->pcm_sample;
	PCM_CV_TYPE_C *right = left + bytespersample;

	samplenum /= aui->chan_card;
	do {
		for(b = bytespersample; b; b--) {
			char cl = left[0];
			left[0] = right[0];
			right[0] = cl;
			left++;
			right++;
		}
		left += chskip;
		right += chskip;
	} while(--samplenum);
}

static int mixer_swapchan_checkvar(struct mpxplay_audioout_info_s *aui)
{
	if(MIXER_var_swapchan && (aui->chan_card >= 2))
		return 1;
	return 0;
}

one_mixerfunc_info MIXER_FUNCINFO_swapchan = {
	"MIX_SWAPCHAN",
	"mxsw",
	&MIXER_var_swapchan,
	MIXER_INFOBIT_SWITCH | MIXER_INFOBIT_EXTERNAL_DEPENDENCY,	// aui->chan_card
	0, 1, 0, 0,
	NULL,
	&mixer_swapchan_lqhq,
	&mixer_swapchan_lqhq,
	&mixer_swapchan_checkvar,
	NULL
};
