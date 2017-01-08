//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2010 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: WavPack file handling
//requires the decoders\ad_wavpa\wavpack.lib and wavpack.h files

#include <in_file.h>

#ifdef MPXPLAY_LINK_INFILE_WAVPACK

#include "tagging.h"
#include "newfunc\newfunc.h"
#include "decoders\ad_wavpa\wavpack.h"

typedef struct wavpack_decoder_data {
	WavpackContext *wpc;
	unsigned int bytes_per_sample;
} wavpack_decoder_data;

static int wavpack_assign_values(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct wavpack_decoder_data *wpdi, struct mpxplay_infile_info_s *miis)
{
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	unsigned int encmode;
	unsigned long pcmdatalen;

	adi->filechannels = adi->outchannels = WavpackGetReducedChannels(wpdi->wpc);	//WavpackGetNumChannels(wpdi->wpc);
	if((adi->outchannels < PCM_MIN_CHANNELS) || (adi->outchannels > PCM_MAX_CHANNELS))
		return 0;

	adi->bits = WavpackGetBitsPerSample(wpdi->wpc);
	if((adi->bits < PCM_MIN_BITS) || (adi->bits > PCM_MAX_BITS))
		return 0;

	adi->freq = WavpackGetSampleRate(wpdi->wpc);
	wpdi->bytes_per_sample = WavpackGetBytesPerSample(wpdi->wpc);
	if(!adi->freq || !wpdi->bytes_per_sample)
		return 0;

	pcmdatalen = WavpackGetNumSamples(wpdi->wpc);

	miis->timemsec = (float)pcmdatalen *1000.0 / adi->freq;

	encmode = WavpackGetMode(wpdi->wpc);
	if(encmode & MODE_FLOAT) {
		adi->infobits |= ADI_FLAG_FLOATOUT;
		adi->bits = 1;
		wpdi->bytes_per_sample = sizeof(MPXPLAY_PCMOUT_FLOAT_T);
	}

	miis->longname = "WavPack ";

	if(encmode & MODE_HYBRID) {
		adi->bitrate = (long)((float)miis->filesize * 8.0 / 1000.0 * (float)adi->freq / (float)pcmdatalen);
	} else {
		long compr_ratio;
		adi->bitratetext = malloc(MPXPLAY_ADITEXTSIZE_BITRATE + 8);
		if(!adi->bitratetext)
			return 0;
		compr_ratio = (long)(1000.0 * (float)miis->filesize / (float)pcmdatalen / (float)wpdi->bytes_per_sample / (float)adi->filechannels);
		sprintf(adi->bitratetext, "%2d/%2.2d.%1.1d%%", adi->bits, compr_ratio / 10, compr_ratio % 10);
	}

	return 1;
}

static int WAVPACK_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct wavpack_decoder_data *wpdi = NULL;
	char errstr[128];

	if(!fbfs->fopen_read(fbds, filename, 0))
		return MPXPLAY_ERROR_INFILE_FILEOPEN;

	miis->filesize = fbfs->filelength(fbds);
	if(miis->filesize < 16)		// ???
		goto err_out_check;

	wpdi = (struct wavpack_decoder_data *)calloc(1, sizeof(struct wavpack_decoder_data));
	if(!wpdi)
		goto err_out_check;
	miis->private_data = wpdi;

	wpdi->wpc = (WavpackContext *) calloc(1, sizeof(WavpackContext));
	if(!wpdi->wpc)
		goto err_out_check;

	if(!WavpackOpenFileInput(wpdi->wpc, fbfs->fread, fbds, errstr))
		goto err_out_check;

	if(!wavpack_assign_values(fbfs, fbds, wpdi, miis))
		goto err_out_check;

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_check:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static void WAVPACK_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct wavpack_decoder_data *wpdi = miis->private_data;
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	if(wpdi) {
		if(wpdi->wpc)
			free(wpdi->wpc);
		free(wpdi);
	}
	if(adi->bitratetext)
		free(adi->bitratetext);
	fbfs->fclose(fbds);
}

//-------------------------------------------------------------------------

static void format_samples(int bps, unsigned char *dst, long *src, unsigned long samcnt)
{
	if(bps == 1) {
		do {
			*dst++ = *src++ + 128;
		} while(--samcnt);
	} else {
		do {
			*((long *)dst) = *src++;
			dst += bps;
		} while(--samcnt);
	}
}

static int WAVPACK_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	struct wavpack_decoder_data *wpdi = miis->private_data;

	WavpackUpdateStreamhand(wpdi->wpc, fbds);

	adi->pcm_samplenum = WavpackUnpackSamples(wpdi->wpc, (long *)adi->pcm_bufptr, adi->pcm_framelen) * adi->outchannels;
	if(!adi->pcm_samplenum)
		return MPXPLAY_ERROR_INFILE_NODATA;

	if(wpdi->bytes_per_sample < 4)
		format_samples(wpdi->bytes_per_sample, (unsigned char *)adi->pcm_bufptr, (long *)adi->pcm_bufptr, adi->pcm_samplenum);

	return MPXPLAY_ERROR_INFILE_OK;
}

//-------------------------------------------------------------------------

static void WAVPACK_infile_clearbuffs(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, unsigned int seektype)
{
	struct wavpack_decoder_data *wpdi = miis->private_data;
	WavpackResetDecoding(wpdi->wpc);
}

static long WAVPACK_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newmpxframenum)
{
	struct wavpack_decoder_data *wpdi = miis->private_data;
	long newfilepos;

	WavpackUpdateStreamhand(wpdi->wpc, fbds);

	newfilepos = (long)((float)newmpxframenum * (float)miis->filesize / (float)miis->allframes);
	if(fbfs->fseek(fbds, newfilepos, SEEK_SET) < 0)
		return MPXPLAY_ERROR_INFILE_EOF;
	return newmpxframenum;
}

struct mpxplay_infile_func_s IN_WAVPACK_funcs = {
	(MPXPLAY_TAGTYPE_PUT_SUPPORT(MPXPLAY_TAGTYPE_ID3V1 | MPXPLAY_TAGTYPE_APETAG)
	 | MPXPLAY_TAGTYPE_PUT_PRIMARY(MPXPLAY_TAGTYPE_APETAG)),
	NULL,
	NULL,
	&WAVPACK_infile_open,
	&WAVPACK_infile_open,
	&WAVPACK_infile_open,
	&WAVPACK_infile_close,
	&WAVPACK_infile_decode,
	&WAVPACK_infile_fseek,
	&WAVPACK_infile_clearbuffs,
	NULL,
	NULL,
	NULL,
	{"WV", NULL}
};

#endif							// MPXPLAY_LINK_INFILE_WAVPACK
