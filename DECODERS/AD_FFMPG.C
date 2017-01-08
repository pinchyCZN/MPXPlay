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
//function: FFMPEG codec handling

//#define MPXPLAY_USE_DEBUGF 1
//#define MPXPLAY_DEBUG_OUTPUT stdout

#include "in_file.h"

#ifdef MPXPLAY_LINK_DECODER_FFMPG

#include "newfunc\newfunc.h"
#include "ffmpegac\avcodec.h"
#undef malloc
#undef free
#include <mem.h>

#define FFMPG_BS_FRAMESIZE_MAX      16384	// if there's no other bufsize
#define FFMPG_SAMPLES_PER_FRAME_MAX (AVCODEC_MAX_AUDIO_FRAME_SIZE/4)

typedef struct ffmpg_decoder_s {
	unsigned int mpx_wave_id;
	unsigned int ffmpg_codec_id;
	AVCodec *avc_func;
} ffmpg_decoder_s;

typedef struct ffmpg_decoder_data_s {
	struct mpxplay_bitstreambuf_s *bs;
	AVCodec *avcodec_funcs;
	AVCodecContext *avcodec_datas;
	unsigned int avcodec_initialized;
	unsigned long min_framesize, max_framesize;
	unsigned int bytes_per_sample;
} ffmpg_decoder_data_s;

extern AVCodec flac_decoder;
extern AVCodec wmav1_decoder;
extern AVCodec wmav2_decoder;

struct ffmpg_decoder_s ffmpg_decoders[] = {
#ifdef MPXPLAY_LINK_DECODER_FLAC
	{MPXPLAY_WAVEID_FLAC, CODEC_ID_FLAC, &flac_decoder},
#endif
#ifdef MPXPLAY_LINK_DECODER_WMA
	{MPXPLAY_WAVEID_WMAV1, CODEC_ID_WMAV1, &wmav1_decoder},
	{MPXPLAY_WAVEID_WMAV2, CODEC_ID_WMAV2, &wmav2_decoder},
#endif
	{0, 0, NULL}
};

static unsigned int ad_ffmpg_assign_audio(struct mpxplay_audio_decoder_info_s *adi);
static void ffmpg_debugf(const char *format, ...);

static int AD_FFMPG_open(struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	struct ffmpg_decoder_data_s *ffmpi;
	AVCodecContext *avci;
	AVCodec *avc_funcs = NULL;
	struct ffmpg_decoder_s *fds = &ffmpg_decoders[0];
	unsigned long privdata_size;

	do {
		if(fds->mpx_wave_id == spi->wave_id) {
			avc_funcs = fds->avc_func;
			break;
		}
		fds++;
	} while(fds->mpx_wave_id);

	if(!avc_funcs || !avc_funcs->decode)
		return MPXPLAY_ERROR_INFILE_CANTOPEN;

	ffmpi = (struct ffmpg_decoder_data_s *)calloc(1, sizeof(struct ffmpg_decoder_data_s));
	if(!ffmpi)
		return MPXPLAY_ERROR_INFILE_MEMORY;

	adi->private_data = ffmpi;

	ffmpi->avcodec_funcs = avc_funcs;

	avci = (AVCodecContext *) calloc(1, sizeof(AVCodecContext));
	if(!avci)
		return 0;
	ffmpi->avcodec_datas = avci;

	if(avc_funcs->priv_data_size)
		privdata_size = avc_funcs->priv_data_size;
	else
		privdata_size = 65536;	// ???
	avci->priv_data = (void *)calloc(1, privdata_size);
	if(!avci->priv_data)
		return 0;

	//av_log_set_level(AV_LOG_DEBUG);
	av_log_set_level(AV_LOG_QUIET);

	avci->sample_rate = adi->freq;
	avci->channels = adi->filechannels;
	avci->bit_rate = adi->bitrate * 1000;
	avci->block_align = spi->block_align;
	avci->extradata = spi->extradata;
	avci->extradata_size = spi->extradata_size;
	avci->codec = avc_funcs;
	avci->codec_id = fds->ffmpg_codec_id;
	switch (spi->streamtype) {
	case MPXPLAY_SPI_STREAMTYPE_AUDIO:
		avci->codec_type = CODEC_TYPE_AUDIO;
		break;
	case MPXPLAY_SPI_STREAMTYPE_VIDEO:
		avci->codec_type = CODEC_TYPE_VIDEO;
		break;
	case MPXPLAY_SPI_STREAMTYPE_DATA:
		avci->codec_type = CODEC_TYPE_DATA;
		break;
	case MPXPLAY_SPI_STREAMTYPE_SUBTITLE:
		avci->codec_type = CODEC_TYPE_SUBTITLE;
		break;
	default:
		return MPXPLAY_ERROR_INFILE_CANTOPEN;
	}

	if(avc_funcs->init) {
		if(avc_funcs->init(avci) < 0)	// !!! extradata parsing is done here
			return MPXPLAY_ERROR_INFILE_CANTOPEN;
		ffmpi->avcodec_initialized = 1;
	}

	if(!avci->block_align)
		avci->block_align = 1;

	if(!spi->bs_framesize)
		if(avci->block_align >= MPXPLAY_SPI_MINBSREADSIZE)
			spi->bs_framesize = avci->block_align;
		else
			spi->bs_framesize = FFMPG_BS_FRAMESIZE_MAX;

	ffmpi->bs = mpxplay_bitstream_alloc(spi->bs_framesize * 2);
	if(!ffmpi->bs)
		return MPXPLAY_ERROR_INFILE_MEMORY;

	ad_ffmpg_assign_audio(adi);

	return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_FFMPG_close(struct mpxplay_audio_decoder_info_s *adi)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)adi->private_data;
	if(ffmpi) {
		AVCodecContext *avci = ffmpi->avcodec_datas;
		if(avci) {
			if(ffmpi->avcodec_funcs && ffmpi->avcodec_funcs->close && ffmpi->avcodec_initialized)
				ffmpi->avcodec_funcs->close(avci);
			if(avci->priv_data)
				free(avci->priv_data);
			free(avci);
		}
		mpxplay_bitstream_free(ffmpi->bs);
		free(ffmpi);
	}
}

static int AD_FFMPG_parse_extra(struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	// extradata parsing is done in av_funcs->_init
	return MPXPLAY_ERROR_INFILE_OK;
}

static int AD_FFMPG_parse_frame(struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)adi->private_data;
	int bytes_consumed, bsbufbytes, pcmdata_size;

	spi->bs_usedbytes = mpxplay_bitstream_putbytes(ffmpi->bs, spi->bitstreambuf, spi->bs_leftbytes);
	bsbufbytes = mpxplay_bitstream_leftbytes(ffmpi->bs);

	pcmdata_size = 0;
	bytes_consumed = ffmpi->avcodec_funcs->decode(ffmpi->avcodec_datas, adi->pcm_bufptr, &pcmdata_size, mpxplay_bitstream_getbufpos(ffmpi->bs), bsbufbytes);

	if(bytes_consumed < 0) {
		mpxplay_bitstream_skipbytes(ffmpi->bs, ffmpi->avcodec_datas->block_align);	// !!!
		return MPXPLAY_ERROR_INFILE_RESYNC;
	}

	mpxplay_bitstream_skipbytes(ffmpi->bs, bytes_consumed);

	if(pcmdata_size) {
		ad_ffmpg_assign_audio(adi);
		return MPXPLAY_ERROR_INFILE_OK;
	}

	return MPXPLAY_ERROR_INFILE_RESYNC;
}

static int AD_FFMPG_decode(struct mpxplay_audio_decoder_info_s *adi, struct mpxplay_streampacket_info_s *spi)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)adi->private_data;
	int bytes_consumed, bsbufbytes, pcmdata_size;

	spi->bs_usedbytes = mpxplay_bitstream_putbytes(ffmpi->bs, spi->bitstreambuf, spi->bs_leftbytes);
	bsbufbytes = mpxplay_bitstream_leftbytes(ffmpi->bs);

	pcmdata_size = 0;
	bytes_consumed = ffmpi->avcodec_funcs->decode(ffmpi->avcodec_datas, adi->pcm_bufptr, &pcmdata_size, mpxplay_bitstream_getbufpos(ffmpi->bs), bsbufbytes);

	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "bc:%4d ds:%5d", bytes_consumed, pcmdata_size);

	if(bytes_consumed < 0) {
		mpxplay_bitstream_skipbytes(ffmpi->bs, ffmpi->avcodec_datas->block_align);	// !!!
		return MPXPLAY_ERROR_INFILE_NODATA;
	}

	if(!bytes_consumed && !bsbufbytes && !pcmdata_size)
		return MPXPLAY_ERROR_INFILE_NODATA;

	mpxplay_bitstream_skipbytes(ffmpi->bs, bytes_consumed);

	adi->pcm_samplenum = pcmdata_size / ffmpi->bytes_per_sample;

	//mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"lb:%4d ub:%4d bc:%4d po:%5d ba:%d",spi->bs_leftbytes,spi->bs_usedbytes,bytes_consumed,adi->pcm_samplenum,spi->block_align);

	return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_FFMPG_clearbuff(struct mpxplay_audio_decoder_info_s *adi, unsigned int seektype)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)adi->private_data;
	mpxplay_bitstream_reset(ffmpi->bs);
	if(ffmpi->avcodec_funcs->flush)
		ffmpi->avcodec_funcs->flush(ffmpi->avcodec_datas, (seektype & (MPX_SEEKTYPE_BOF | MPX_SEEKTYPE_PAUSE)));
}

static unsigned int ad_ffmpg_assign_audio(struct mpxplay_audio_decoder_info_s *adi)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)adi->private_data;
	AVCodecContext *avci = ffmpi->avcodec_datas;

	switch (avci->sample_fmt) {
	case SAMPLE_FMT_S16:
		ffmpi->bytes_per_sample = 2;
		adi->bits = 16;
		break;
	case SAMPLE_FMT_S24:
		ffmpi->bytes_per_sample = 3;
		adi->bits = 24;
		break;
	case SAMPLE_FMT_S32:
		ffmpi->bytes_per_sample = 4;
		adi->bits = 32;
		break;
	case SAMPLE_FMT_FLT:
		ffmpi->bytes_per_sample = 4;
		adi->bits = 1;
		funcbit_enable(adi->infobits, ADI_FLAG_FLOATOUT);
		break;					// ???
	case SAMPLE_FMT_F16:
		ffmpi->bytes_per_sample = 4;
		adi->bits = 16;
		funcbit_enable(adi->infobits, ADI_FLAG_FLOATOUT);
		break;
	default:
		return 0;
	}

	adi->freq = avci->sample_rate;
	adi->filechannels = avci->channels;
	adi->bitrate = avci->bit_rate / 1000;

	return 1;
}

struct mpxplay_audio_decoder_func_s AD_FFMPG_funcs = {
	0,
	NULL,
	NULL,
	NULL,
	&AD_FFMPG_open,
	&AD_FFMPG_close,
	&AD_FFMPG_parse_extra,
	&AD_FFMPG_parse_frame,
	&AD_FFMPG_decode,
	&AD_FFMPG_clearbuff,
	NULL,
	NULL,
	FFMPG_BS_FRAMESIZE_MAX,
	FFMPG_SAMPLES_PER_FRAME_MAX,
	{
	 {MPXPLAY_WAVEID_FLAC, "FLA"},
	 {MPXPLAY_WAVEID_WMAV1, "WMA"},
	 {MPXPLAY_WAVEID_WMAV2, "WMA"},
	 {0, NULL}
	 }
};

#endif							// MPXPLAY_LINK_DECODER_FFMPG
