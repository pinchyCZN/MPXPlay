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
//function: MP3 decoder handling
//requires the ad_mp3\mp3dec.lib and mp3dec.h files

//#define MPXPLAY_USE_DEBUGF 1
#define ADMP3_DEBUG_OUTPUT stdout

#include "in_file.h"

#ifdef MPXPLAY_LINK_DECODER_MPX

#include "newfunc\newfunc.h"
#include "ad_mp3\mp3dec.h"

#define MPXDEC_HEADERSIZE       4
#define MPXDEC_BLOCKSAMPLES_MAX 1152

#define MPXDEC_BITSTREAM_BUFSIZE (MPXDEC_FRAMESIZE_MAX*5)	// ???

#define MPXDEC_SYNC_RETRY_BYTE   (MPXDEC_FRAMESIZE_MAX*8)	// will not reach these
#define MPXDEC_SYNC_RETRY_FRAME  64	// in ad_mp3_sync_frame

#define ADMP3_FLAG_DECODING_BOF     (1<<0)
#define ADMP3_FLAG_DECODING_EOF     (1<<1)
#define ADMP3_FLAG_XING_LOADED      (1<<2)
#define ADMP3_FLAG_ENCPAD_BSF_ADDED (1<<3)

#define LAMEINFO_FLAG_NOGAP_NEXT (1<<6)
#define LAMEINFO_FLAG_NOGAP_PREV (1<<7)

typedef struct mp3_decoder_data_s {
	unsigned long flags;
	struct mpxplay_bitstreambuf_s *bs;
	struct mp3_decoder_data *mp3d;
	unsigned int lastframesize;
	unsigned int pcm_samplenum;
	unsigned int error;
	unsigned int decoded_framecount;
	unsigned int goodframe_count;
	unsigned int goodframe_limit;
	unsigned int gooddec_count;
	unsigned int gooddec_limit;
	unsigned int lameinfo_flags;
	unsigned int dctsave_filenum;
	int enc_delay;
	int enc_padding;
	unsigned int encdelay_framenum, encdelay_samplenum;
	unsigned int encpadding_framenum, encpadding_samplenum;
} mp3_decoder_data_s;

static struct mp3_decoder_dctsave_t {
	unsigned int filecount;		//
	unsigned int filenum;		// to avoid false dct restore at crossfade (note: a similar number should be in the Lame Info header to identify the sequential files...)
	unsigned int lay;
	unsigned int filechannels;
	unsigned int frequency_index;
	unsigned int granules;
	block_t *block_a;			// 1 + [2][MPXDEC_CHANNELS_MAX][SBLIMIT][SSLIMIT]
	synth_rollb_t *synth_rollbuff;	// [MPXDEC_CHANNELS_MAX][2][256+16]
	unsigned int synth_bo;
} admp3_dctsave;

static int ad_mp3_sync_head(struct mp3_decoder_data_s *mp3i, unsigned int framecount, unsigned int headsync);
static int ad_mp3_sync_frame(struct mp3_decoder_data_s *mp3i, unsigned int headsync);
static int ad_mp3_assign_values(struct mp3_decoder_data_s *mp3i, struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi);

static void ad_mp3_get_lame_info(struct mp3_decoder_data_s *mp3i);
static void ad_mp3_dctsave_alloc(void);
static void ad_mp3_dctsave_free(void);
static void ad_mp3_dct_save(struct mp3_decoder_data_s *mp3i);
static void ad_mp3_dct_restore(struct mp3_decoder_data_s *mp3i);

static int AD_MP3_open(struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	struct mp3_decoder_data_s *mp3i;
	struct mp3_decoder_data *mp3d;

	mp3i = calloc(1, sizeof(struct mp3_decoder_data_s));
	if(!mp3i)
		return MPXPLAY_ERROR_INFILE_MEMORY;

	adi->private_data = mp3i;

	mp3i->mp3d = mpxdec_init();
	if(!mp3i->mp3d)
		return MPXPLAY_ERROR_INFILE_MEMORY;
	mp3d = mp3i->mp3d;

	mp3i->bs = mpxplay_bitstream_alloc(MPXDEC_BITSTREAM_BUFSIZE);
	if(!mp3i->bs)
		return MPXPLAY_ERROR_INFILE_MEMORY;

	// decoder can work without parsing too, the following settings are for this
	// it doesn't work at single MP3 files, only at containers (where we get freq and filechannels values)
	switch (adi->wave_id) {
	case MPXPLAY_WAVEID_MP2:
		mp3d->lay = 2;
		break;
	case MPXPLAY_WAVEID_MP3:
	default:
		mp3d->lay = 3;
		break;
	}
	if((adi->channelcfg == CHM_LEFT) || (adi->channelcfg == CHM_DOWNMIX))
		mp3d->outchannels = 1;
	else
		mp3d->outchannels = adi->filechannels;	// !!!
	mp3d->synthdata->outchannels = mp3d->outchannels;
	adi->outchannels = mp3d->outchannels;

	if(adi->freq && (adi->freq <= 24000))	// !!!
		mp3d->lsf = 1;
	mp3i->pcm_samplenum = (MPXDEC_BLOCKSAMPLES_MAX >> mp3d->lsf) * mp3d->outchannels;
	adi->pcm_framelen = (MPXDEC_BLOCKSAMPLES_MAX >> mp3d->lsf);

	if(mp3d->lay == 3) {
		mp3i->goodframe_limit = 2;	// dynamic bit allocation
		mp3i->gooddec_limit = 2;	// mdct12/36 overlap
	} else {					// lay 2
		mp3i->goodframe_limit = 1;
		mp3i->gooddec_limit = 0;
	}

	funcbit_enable(mp3i->flags, ADMP3_FLAG_DECODING_BOF);
	mp3i->dctsave_filenum = admp3_dctsave.filecount++;
	mp3i->enc_delay = mp3i->enc_padding = -1;

	adi->bits = 16;
#ifndef MPXDEC_INTEGER_OUTPUT
	adi->infobits |= ADI_FLAG_FLOATOUT;
	if(mp3d->lay == 3)
		adi->infobits |= ADI_FLAG_FPUROUND_CHOP;
#endif
	if(adi->infobits & ADI_CNTRLBIT_BITSTREAMOUT)
		adi->infobits |= ADI_FLAG_BITSTREAMOUT;

	mpxplay_debugf(ADMP3_DEBUG_OUTPUT, "admp3 decoder open");

	return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_MP3_close(struct mpxplay_audio_decoder_info_s *adi)
{
	struct mp3_decoder_data_s *mp3i = (struct mp3_decoder_data_s *)adi->private_data;
	if(mp3i) {
		if((mp3i->lameinfo_flags & LAMEINFO_FLAG_NOGAP_NEXT) && (mp3i->flags & ADMP3_FLAG_DECODING_EOF))
			ad_mp3_dct_save(mp3i);
		mpxplay_bitstream_free(mp3i->bs);
		mpxdec_close(mp3i->mp3d);
		free(mp3i);
	}
}

static int AD_MP3_parse_frame(struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	struct mp3_decoder_data_s *mp3i = (struct mp3_decoder_data_s *)adi->private_data;
	int retcode;

	spi->bs_usedbytes = mpxplay_bitstream_putbytes(mp3i->bs, spi->bitstreambuf, spi->bs_leftbytes);

	mpxplay_debugf(ADMP3_DEBUG_OUTPUT, "parse bsfs:%d lb:%4d ub:%4d gfc:%d", spi->bs_framesize, spi->bs_leftbytes, spi->bs_usedbytes, mp3i->goodframe_count);

	ad_mp3_get_lame_info(mp3i);

	mp3i->goodframe_limit = 3;	// for head sync

	retcode = ad_mp3_sync_frame(mp3i, 1);
	if(retcode != MPXPLAY_ERROR_INFILE_OK)
		return retcode;

	ad_mp3_assign_values(mp3i, adi, spi);

	if(!spi->bs_framesize)
		spi->bs_framesize = mp3i->lastframesize;

	if((mp3i->enc_padding > 0) && !(mp3i->flags & ADMP3_FLAG_ENCPAD_BSF_ADDED)) {
		spi->bs_framesize += spi->bs_framesize * (mp3i->encpadding_framenum + 1);
		funcbit_enable(mp3i->flags, ADMP3_FLAG_ENCPAD_BSF_ADDED);
	}

	if(mp3i->lameinfo_flags & LAMEINFO_FLAG_NOGAP_NEXT)
		ad_mp3_dctsave_alloc();

	return MPXPLAY_ERROR_INFILE_OK;
}

static int AD_MP3_decode(struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	struct mp3_decoder_data_s *mp3i = (struct mp3_decoder_data_s *)adi->private_data;
	int retcode;

	if(adi->infobits & ADI_CNTRLBIT_BITSTREAMOUT) {
		spi->bs_usedbytes = mpxplay_bitstream_putbytes(mp3i->bs, spi->bitstreambuf, spi->bs_leftbytes);
		retcode = ad_mp3_sync_frame(mp3i, 0);
		if(retcode != MPXPLAY_ERROR_INFILE_OK)
			return retcode;
		mpxplay_bitstream_readbytes(mp3i->bs, adi->pcm_bufptr, mp3i->lastframesize);
		adi->pcm_samplenum = mp3i->lastframesize;
		return MPXPLAY_ERROR_INFILE_OK;
	}

	if(mp3i->lastframesize) {
		mpxplay_bitstream_skipbytes(mp3i->bs, mp3i->lastframesize);
		mp3i->lastframesize = 0;
	}
	spi->bs_usedbytes = mpxplay_bitstream_putbytes(mp3i->bs, spi->bitstreambuf, spi->bs_leftbytes);

	if((mp3i->enc_padding > 0) && spi->bs_leftbytes && mpxplay_bitstream_leftbytes(mp3i->bs) < spi->bs_framesize)	// need more data to check padding
		return MPXPLAY_ERROR_INFILE_OK;

	mp3i->decoded_framecount++;

	retcode = ad_mp3_sync_frame(mp3i, 0);
	mpxplay_debugf(ADMP3_DEBUG_OUTPUT, "decode ub:%4d lb:%4d gfc:%4d gdc:%4d gdl:%d rc:%d", spi->bs_usedbytes, spi->bs_leftbytes, mp3i->goodframe_count, mp3i->gooddec_count, mp3i->gooddec_limit,
				   retcode);
	if(retcode != MPXPLAY_ERROR_INFILE_OK) {
		funcbit_enable(mp3i->flags, ADMP3_FLAG_DECODING_EOF);
		return retcode;
	}

	if(mp3i->error) {
		mp3i->gooddec_count = 0;
		mp3i->error = 0;
	}

	if(mp3i->flags & ADMP3_FLAG_DECODING_BOF) {
		if(mp3i->lameinfo_flags & LAMEINFO_FLAG_NOGAP_PREV)
			ad_mp3_dct_restore(mp3i);
		funcbit_disable(mp3i->flags, ADMP3_FLAG_DECODING_BOF);
	}

	if(mp3i->decoded_framecount <= mp3i->encdelay_framenum)
		return MPXPLAY_ERROR_INFILE_OK;

	if(mpxdec_decode_part1(mp3i->mp3d, adi->channelcfg) == MPXDEC_ERROR_OK)
		mp3i->gooddec_count++;
	else
		mp3i->gooddec_count = 0;

	if(mp3i->gooddec_count >= mp3i->gooddec_limit) {
		mpxdec_decode_part2(mp3i->mp3d->synthdata, adi->pcm_bufptr);
		adi->pcm_samplenum = mp3i->pcm_samplenum;

		if((mp3i->enc_delay > 0) && (mp3i->decoded_framecount == (mp3i->encdelay_framenum + 1)) && (mp3i->encdelay_samplenum < adi->pcm_samplenum)) {
			adi->pcm_samplenum -= mp3i->encdelay_samplenum;
#ifdef MPXDEC_INTEGER_OUTPUT
			pds_memcpy(adi->pcm_bufptr, adi->pcm_bufptr + mp3i->encdelay_samplenum * sizeof(short), adi->pcm_samplenum * sizeof(short));
#else
			pds_memcpy(adi->pcm_bufptr, adi->pcm_bufptr + mp3i->encdelay_samplenum * sizeof(float), adi->pcm_samplenum * sizeof(float));
#endif
			mpxplay_debugf(ADMP3_DEBUG_OUTPUT, "enc_delay:%d sn:%d", mp3i->enc_delay, adi->pcm_samplenum);
		}

		if(mp3i->enc_padding > 0) {
			int framesleft = ad_mp3_sync_head(mp3i, mp3i->encpadding_framenum + 1, 0);
			mpxplay_debugf(ADMP3_DEBUG_OUTPUT, "enc_padding:%d lb:%d fs:%d rc:%d", mp3i->enc_padding, mpxplay_bitstream_leftbytes(mp3i->bs), spi->bs_framesize, framesleft);
			if(framesleft <= (int)(mp3i->encpadding_framenum)) {
				if(framesleft == (mp3i->encpadding_framenum))
					adi->pcm_samplenum = mp3i->pcm_samplenum - mp3i->encpadding_samplenum;
				else
					adi->pcm_samplenum = 0;
			}
		}
	}

	return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_MP3_clearbuff(struct mpxplay_audio_decoder_info_s *adi, unsigned int seektype)
{
	struct mp3_decoder_data_s *mp3i = (struct mp3_decoder_data_s *)adi->private_data;
	mpxplay_bitstream_reset(mp3i->bs);
	mp3i->lastframesize = 0;
	mp3i->goodframe_count = 0;
	mp3i->gooddec_count = 0;
	if(seektype & MPX_SEEKTYPE_BOF) {
		mpxdec_reset_bitstream(mp3i->mp3d);
		mp3i->goodframe_count = mp3i->goodframe_limit;
		mp3i->gooddec_count = mp3i->gooddec_limit;
		mp3i->decoded_framecount = 0;
	} else {
		mp3i->goodframe_count = 0;
		mp3i->gooddec_count = 0;
		mp3i->decoded_framecount = mp3i->encdelay_framenum + 2;	// to sign that it's not the BOF
		funcbit_disable(mp3i->flags, ADMP3_FLAG_DECODING_BOF);	//
	}

	mpxdec_reset_decoding(mp3i->mp3d);
	if(seektype & (MPX_SEEKTYPE_BOF | MPX_SEEKTYPE_PAUSE))
		mpxdec_reset_synth(mp3i->mp3d);
}

//------------------------------------------------------------------------
static int ad_mp3_sync_head(struct mp3_decoder_data_s *mp3i, unsigned int framecount, unsigned int headsync)
{
	unsigned int retry_byte = MPXDEC_SYNC_RETRY_BYTE, minframesleft = 0;
	struct mpxplay_bitstreambuf_s bs;
	struct mp3_decoder_data mp3d;

	pds_memcpy((void *)&bs, (void *)mp3i->bs, sizeof(struct mpxplay_bitstreambuf_s));
	pds_memcpy((void *)&mp3d, (void *)mp3i->mp3d, sizeof(struct mp3_decoder_data));

	if(mp3i->lastframesize)
		mpxplay_bitstream_skipbytes(&bs, mp3i->lastframesize);

	do {
		int framesize;
		if(mpxplay_bitstream_leftbytes(&bs) <= MPXDEC_HEADERSIZE)
			return minframesleft;

		framesize = mpxdec_syncinfo(&mp3d, mpxplay_bitstream_getbufpos(&bs));
		if(framesize <= 0) {
			if(headsync)
				mp3d.firsthead = 0;
			mpxplay_bitstream_skipbytes(&bs, 1);
			if(!(--retry_byte))
				break;
			continue;
		}
		if(mpxplay_bitstream_leftbytes(&bs) < framesize)
			return minframesleft;
		mpxplay_bitstream_skipbytes(&bs, framesize);
		minframesleft++;
		if(minframesleft >= framecount)
			return minframesleft;
	} while(1);

	return MPXPLAY_ERROR_INFILE_EOF;
}

static int ad_mp3_sync_frame(struct mp3_decoder_data_s *mp3i, unsigned int headsync)
{
	unsigned int retry_frame = MPXDEC_SYNC_RETRY_FRAME;
	unsigned int retry_byte = MPXDEC_SYNC_RETRY_BYTE;
	do {
		int framesize;
		if(mpxplay_bitstream_leftbytes(mp3i->bs) <= MPXDEC_HEADERSIZE)
			return MPXPLAY_ERROR_INFILE_NODATA;

		framesize = mpxdec_syncinfo(mp3i->mp3d, mpxplay_bitstream_getbufpos(mp3i->bs));
		if(framesize <= 0) {
			//mpxplay_debugf(ADMP3_DEBUG_OUTPUT,"sync fail %8.8X %8.8X",PDS_GETB_BE32(mpxplay_bitstream_getbufpos(mp3i->bs)),mp3i->mp3d->firsthead);
			if(headsync)
				mp3i->mp3d->firsthead = 0;
			mp3i->goodframe_count = 0;
			mp3i->error = MPXDEC_ERROR_BITSTREAM;
			mpxplay_bitstream_skipbytes(mp3i->bs, 1);
			if(!(--retry_byte))
				break;
			continue;
		}
#ifdef MPXPLAY_USE_DEBUGF
		//else
		// mpxplay_debugf(ADMP3_DEBUG_OUTPUT,"sync OK   %8.8X %8.8X",PDS_GETB_BE32(mpxplay_bitstream_getbufpos(mp3i->bs)),mp3i->mp3d->firsthead);
#endif

		if(mpxplay_bitstream_leftbytes(mp3i->bs) < framesize) {
			if(!mp3i->goodframe_count && headsync)
				mp3i->mp3d->firsthead = 0;
			return MPXPLAY_ERROR_INFILE_NODATA;
		}

		if(mpxdec_read_frame(mp3i->mp3d, mpxplay_bitstream_getbufpos(mp3i->bs)) == MPXDEC_ERROR_OK) {
			mp3i->goodframe_count++;
			if(mp3i->goodframe_count >= mp3i->goodframe_limit) {
				mp3i->lastframesize = framesize;
				return MPXPLAY_ERROR_INFILE_OK;
			} else {
				mpxplay_bitstream_skipbytes(mp3i->bs, framesize);
				continue;
			}
		}

		if(headsync)
			mp3i->mp3d->firsthead = 0;
		mp3i->goodframe_count = 0;
		mp3i->error = MPXDEC_ERROR_BITSTREAM;
		mpxplay_bitstream_skipbytes(mp3i->bs, framesize);	// skips frame on error
		if(!(--retry_frame))
			break;
	} while(1);

	return MPXPLAY_ERROR_INFILE_EOF;
}

static int ad_mp3_assign_values(struct mp3_decoder_data_s *mp3i, struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	struct mp3_decoder_data *mp3d = mp3i->mp3d;

	switch (mp3d->lay) {
	case 2:
		spi->wave_id = MPXPLAY_WAVEID_MP2;
		break;
	case 3:
		spi->wave_id = MPXPLAY_WAVEID_MP3;
		break;
	}

	switch (mp3d->mpg_chmode) {
	case MPG_MD_DUALCHAN:
		adi->channeltext = "DualChan";
		break;
	case MPG_MD_JOINT_STEREO:
		if(mp3d->mpg_chmode_ext & 0x1)
			adi->channeltext = "i-Stereo";
		else
			adi->channeltext = "msStereo";
		break;
	}

	adi->freq = mp3d->freq;
	adi->filechannels = mp3d->filechannels;
	if((adi->channelcfg == CHM_LEFT) || (adi->channelcfg == CHM_DOWNMIX))
		mp3d->outchannels = 1;
	else
		mp3d->outchannels = mp3d->filechannels;
	adi->outchannels = mp3d->outchannels;
	mp3d->synthdata->outchannels = mp3d->outchannels;

	adi->pcm_framelen = (MPXDEC_BLOCKSAMPLES_MAX >> mp3d->lsf);
	mp3i->pcm_samplenum = (MPXDEC_BLOCKSAMPLES_MAX >> mp3d->lsf) * mp3d->outchannels;

#ifdef MPXDEC_ENABLE_FREEFORMAT
	if(mp3d->infobits & MP3DI_INFOBIT_FREEFORMAT) {
		if(!adi->bitrate)
			adi->bitrate = mp3d->bitrate = (mp3d->fsize_curr * adi->freq + (576 >> mp3d->lsf) * (1000 / 8)) / ((MPXDEC_BLOCKSAMPLES_MAX >> mp3d->lsf) * (1000 / 8));	// rounding
		mp3d->firsthead &= MPXDEC_HEADMASK_FREEFORMAT;
	} else
#else
	if(!adi->bitrate)			// not XING-VBR (didn't get bitrate in in_mp3.c)
		adi->bitrate = mp3d->bitrate;
#endif

	if(mp3d->lay == 3) {
		mp3i->goodframe_limit = 2;
		if(mp3d->bitrate <= 96)
			mp3i->gooddec_limit = 3;
		else if(mp3d->bitrate < 256)
			mp3i->gooddec_limit = 2;
		else
			mp3i->gooddec_limit = 1;
	} else {					// lay 2
		mp3i->goodframe_limit = 1;
		mp3i->gooddec_limit = 0;
	}

	if(mp3i->enc_delay > 0) {
		unsigned int encdelay_framenum = mp3i->enc_delay / adi->pcm_framelen;
		mp3i->encdelay_samplenum = mp3i->enc_delay - encdelay_framenum * adi->pcm_framelen;
		mp3i->encdelay_samplenum *= mp3d->outchannels;
		mp3i->encdelay_framenum = encdelay_framenum + 1;	// +1 : skip Info frame
	}
	if(mp3i->enc_padding > 0) {
		mp3i->encpadding_framenum = mp3i->enc_padding / adi->pcm_framelen;
		mp3i->encpadding_samplenum = mp3i->enc_padding - mp3i->encpadding_framenum * adi->pcm_framelen;
		mp3i->encpadding_samplenum *= mp3d->outchannels;
	}

	mpxplay_debugf(ADMP3_DEBUG_OUTPUT, "encds:%d encps:%d encpf:%d", mp3i->encdelay_samplenum, mp3i->encpadding_samplenum, mp3i->encpadding_framenum);

	return MPXPLAY_ERROR_INFILE_OK;
}

//--------------------------------------------------------------------------
// for gapless playing

#define XING_FRAMES_FLAG    0x0001
#define XING_BYTES_FLAG     0x0002
#define XING_TOC_FLAG       0x0004
#define XING_VBR_SCALE_FLAG 0x0008
#define XING_NUMTOCENTRIES  100

static void ad_mp3_get_lame_info(struct mp3_decoder_data_s *mp3i)
{
	unsigned int xing_flags, offs;
	struct mp3_decoder_data *mp3d = mp3i->mp3d;
	unsigned char *headbufp = mpxplay_bitstream_getbufpos(mp3i->bs), *endbufp;

	if(mp3i->flags & ADMP3_FLAG_XING_LOADED)
		return;

	if(mpxdec_syncinfo(mp3d, headbufp) < 0)
		return;

	endbufp = headbufp + mpxplay_bitstream_leftbytes(mp3i->bs);

	if(!mp3d->lsf) {			// mpeg1
		if(mp3d->mpg_chmode != MPG_MD_MONO)
			offs = (32 + 4);
		else
			offs = (17 + 4);
	} else {					// mpeg2
		if(mp3d->mpg_chmode != MPG_MD_MONO)
			offs = (17 + 4);
		else
			offs = (9 + 4);
	}

	headbufp += offs;
	if((headbufp + 32) >= endbufp)
		return;

	if(PDS_GETB_LE32(headbufp) != PDS_GET4C_LE32('X', 'i', 'n', 'g')
	   && PDS_GETB_LE32(headbufp) != PDS_GET4C_LE32('I', 'n', 'f', 'o'))
		return;

	funcbit_enable(mp3i->flags, ADMP3_FLAG_XING_LOADED);
	mp3i->encdelay_framenum = 1;	// to skip this Info frame

	headbufp += 4;

	xing_flags = PDS_GETB_BE32(headbufp);
	headbufp += 4;
	if(xing_flags & XING_FRAMES_FLAG)
		headbufp += 4;
	if(xing_flags & XING_BYTES_FLAG)
		headbufp += 4;
	if(xing_flags & XING_TOC_FLAG)
		headbufp += XING_NUMTOCENTRIES;
	if(xing_flags & XING_VBR_SCALE_FLAG)
		headbufp += 4;

	if((headbufp + 32) > endbufp)
		return;

	headbufp += 19;
	mp3i->lameinfo_flags = headbufp[0];
	headbufp += 2;

	mp3i->enc_delay = (unsigned int)headbufp[0] << 4;
	mp3i->enc_delay += (unsigned int)headbufp[1] >> 4;
	mp3i->enc_padding = ((unsigned int)headbufp[1] & 0x0F) << 8;
	mp3i->enc_padding += (unsigned int)headbufp[2];
	if(mp3i->enc_delay > 3000)
		mp3i->enc_delay = -1;
	if(mp3i->enc_padding > 3000)
		mp3i->enc_padding = 0;

	mpxplay_debugf(ADMP3_DEBUG_OUTPUT, "get_lameinfo flags:%2.2X delay:%d pad:%d", mp3i->lameinfo_flags, mp3i->enc_delay, mp3i->enc_padding);
}

static void ad_mp3_dctsave_alloc(void)
{
	struct mp3_decoder_dctsave_t *ds = &admp3_dctsave;

	if(!ds->block_a)
		ds->block_a = malloc((1 + 2 * MPXDEC_CHANNELS_MAX * SBLIMIT * SSLIMIT) * sizeof(block_t));
	if(!ds->synth_rollbuff)
		ds->synth_rollbuff = malloc(MPXDEC_CHANNELS_MAX * 2 * 0x110 * sizeof(synth_rollb_t));
}

static void ad_mp3_dctsave_free(void)
{
	struct mp3_decoder_dctsave_t *ds = &admp3_dctsave;

	if(ds->block_a) {
		free(ds->block_a);
		ds->block_a = NULL;
	}
	if(ds->synth_rollbuff) {
		free(ds->synth_rollbuff);
		ds->synth_rollbuff = NULL;
	}
}

static void ad_mp3_dct_save(struct mp3_decoder_data_s *mp3i)
{
	struct mp3_decoder_dctsave_t *ds = &admp3_dctsave;
	struct mp3_decoder_data *mp3d = mp3i->mp3d;
	struct mpxsynth_data_s *sd = mp3d->synthdata;

	if(ds->block_a && ds->synth_rollbuff) {
		ds->filenum = mp3i->dctsave_filenum;
		ds->lay = mp3d->lay;
		ds->filechannels = mp3d->filechannels;
		ds->frequency_index = mp3d->frequency_index;
		ds->granules = mp3d->granules;
		pds_memcpy(ds->block_a, mp3d->block_a, (1 + 2 * MPXDEC_CHANNELS_MAX * SBLIMIT * SSLIMIT) * sizeof(block_t));
		pds_memcpy(ds->synth_rollbuff, sd->synth_rollbuff, MPXDEC_CHANNELS_MAX * 2 * 0x110 * sizeof(synth_rollb_t));
		ds->synth_bo = sd->synth_bo;
	}
}

static void ad_mp3_dct_restore(struct mp3_decoder_data_s *mp3i)
{
	struct mp3_decoder_dctsave_t *ds = &admp3_dctsave;
	struct mp3_decoder_data *mp3d = mp3i->mp3d;
	struct mpxsynth_data_s *sd = mp3d->synthdata;

	if((ds->filenum == (ds->filecount - 1)) && (ds->lay == mp3d->lay) && (ds->filechannels == mp3d->filechannels) && (ds->frequency_index == mp3d->frequency_index) && (ds->granules == mp3d->granules)
	   && ds->block_a && ds->synth_rollbuff) {
		pds_memcpy(mp3d->block_a, ds->block_a, (1 + 2 * MPXDEC_CHANNELS_MAX * SBLIMIT * SSLIMIT) * sizeof(block_t));
		pds_memcpy(sd->synth_rollbuff, ds->synth_rollbuff, MPXDEC_CHANNELS_MAX * 2 * 0x110 * sizeof(synth_rollb_t));
		sd->synth_bo = ds->synth_bo;
	}
}

//--------------------------------------------------------------------------

static void AD_MP3_get_analiser_bands(struct mpxplay_audio_decoder_info_s *adi, unsigned int bandnum, unsigned long *banddataptr)
{
	struct mp3_decoder_data_s *mp3i = (struct mp3_decoder_data_s *)adi->private_data;
	struct mpxsynth_data_s *sd = mp3i->mp3d->synthdata;
	sd->analiser_bandnum = bandnum;
	sd->analiser_banddata = banddataptr;
}

static void AD_MP3_set_eq(struct mpxplay_audio_decoder_info_s *adi, unsigned int bandnum, unsigned long *band_freqs, float *band_powers)
{
	struct mp3_decoder_data_s *mp3i = (struct mp3_decoder_data_s *)adi->private_data;
	struct mpxsynth_data_s *sd = mp3i->mp3d->synthdata;
	sd->eq_bandnum = bandnum;
	sd->eq_freqs = band_freqs;
	sd->eq_powers = band_powers;
	mpxdec_layer3_eq_config(sd);
}

struct mpxplay_audio_decoder_func_s AD_MP3_funcs = {
	0,
	&mpxdec_preinit,
	&ad_mp3_dctsave_free,
	NULL,
	&AD_MP3_open,
	&AD_MP3_close,
	NULL,
	&AD_MP3_parse_frame,
	&AD_MP3_decode,
	&AD_MP3_clearbuff,
	&AD_MP3_get_analiser_bands,
	&AD_MP3_set_eq,
	MPXDEC_FRAMESIZE_MAX,
	MPXDEC_BLOCKSAMPLES_MAX,
	{{MPXPLAY_WAVEID_MP2, "MP2"}, {MPXPLAY_WAVEID_MP3, "MP3"}, {0, NULL}}
};

#endif							// MPXPLAY_LINK_DECODER_MPX
