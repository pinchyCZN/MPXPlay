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
//function: FFMPG lib handling (not used)
//requires ffmpegaf\avformat.lib and avformat.h, avcodec.h files

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_FFMPG

//#define INFFMPG_DEBUG 1

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "newfunc\newfunc.h"
#include "in_file.h"
#include "display\display.h"
#include "ffmpeg\libavfor\avformat.h"
#undef malloc
#undef free
#include <mem.h>

#define INFFMPG_INITIAL_PACKETBUF_SIZE 256000	// this have to be enough for a 25fps/50mbit video file to avoid inline malloc

typedef struct demuxer_info_s {
	char *ext;
	AVInputFormat *demuxer;
} demuxer_info_s;

typedef struct inffmpg_file_private_data_s {
	struct mpxplay_filehand_buffered_func_s *fbfs;
	void *fbds;
} inffmpg_file_private_data_s;

typedef struct ffmpg_decoder_data_s {
	URLContext *uctx;
	unsigned int audio_stream_index;
	AVPacket pkt;
	AVFormatContext fctx;
	AVFormatParameters ap;
	inffmpg_file_private_data_s file_priv_datas;
} ffmpg_decoder_data_s;

extern AVInputFormat matroska_demuxer;
extern AVInputFormat mpegts_demuxer;

static struct demuxer_info_s alldemuxers[] = {
	{"MKV", &matroska_demuxer},
	{"TS", &mpegts_demuxer},
	{"", NULL}
};

#define INFFMPG_ALLMUXER_NUM (sizeof(alldemuxers)/sizeof(struct demuxer_info_s)-1)

static URLProtocol inffmpg_file_protocol;

//---------------------------------------------------------------------

#ifdef INFFMPG_DEBUG
void inffmpg_debugf(const char *format, ...)
{
	va_list ap;
	char sout[500];

	va_start(ap, format);
	vsprintf(sout, format, ap);
	va_end(ap);

	//pds_textdisplay_printf(sout);
	//fprintf(stdout,"%s\n",sout);
	display_message(1, 0, sout);
}
#endif

static void INFFMPG_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis);

static void INFFMPG_preinit(void)
{
	av_log_set_level(AV_LOG_QUIET);
	//av_log_set_level(AV_LOG_DEBUG);
	//av_log_set_level(AV_LOG_ERROR);
}

//----------------------------------------------------------------------
static int64_t inffmpg_get_stream_duration_pts(struct ffmpg_decoder_data_s *ffmpi, struct mpxplay_infile_info_s *miis, AVStream * st)
{
	AVFormatContext *ic = &ffmpi->fctx;
	int64_t start_pts = 0, end_pts = 0, duration = 0, oldfilepos = (int64_t) - 1;
	//unsigned long readretry,readblocksize=INFFMPG_INITIAL_PACKETBUF_SIZE/2;

	if(st->duration && st->duration != AV_NOPTS_VALUE)
		goto err_out_durpts;

	oldfilepos = url_ftell(ic->pb);
	do {
		if(ic->iformat->read_packet(ic, &ffmpi->pkt) < 0)
			goto err_out_durpts;
		if(ffmpi->pkt.stream_index == ffmpi->audio_stream_index)
			break;
	} while(1);
	//if(st->duration && st->duration!=AV_NOPTS_VALUE) // ??? possible stream duration in 1st packet?
	// goto err_out_durpts;                            //
	if(ffmpi->pkt.pts == AV_NOPTS_VALUE)
		goto err_out_durpts;
	start_pts = ffmpi->pkt.pts;

	url_fseek(ic->pb, miis->filesize - INFFMPG_INITIAL_PACKETBUF_SIZE, SEEK_SET);
	do {
		if(ic->iformat->read_packet(ic, &ffmpi->pkt) < 0)
			break;
		if(ffmpi->pkt.stream_index == ffmpi->audio_stream_index)
			if(ffmpi->pkt.pts && ffmpi->pkt.pts != AV_NOPTS_VALUE)
				end_pts = ffmpi->pkt.pts;
	} while(1);
	if(start_pts > end_pts)
		goto err_out_durpts;
	st->duration = end_pts - start_pts;

/* for(readretry=1;readretry<4;readretry++){
  url_fseek(ic->pb,miis->filesize-readretry*readblocksize,SEEK_SET);
  do{
   if(ic->iformat->read_packet(ic,&ffmpi->pkt)<0)
    break;
   if(ffmpi->pkt.stream_index==ffmpi->audio_stream_index)
    if(ffmpi->pkt.pts && ffmpi->pkt.pts!=AV_NOPTS_VALUE)
     end_pts=ffmpi->pkt.pts;
  }while(1);
  if(end_pts>start_pts){
   st->duration=end_pts-start_pts;
   break;
  }
 }*/

/* readretry=3;
 do{
#ifdef INFFMPG_DEBUG
  inffmpg_debugf("retry:%d rbs:%d fp:%d",readretry,readblocksize,(long)url_fseek(ic->pb,(miis->filesize-readblocksize),SEEK_SET));
#else
  url_fseek(ic->pb,(miis->filesize-readblocksize),SEEK_SET);
#endif
  do{
   if(ic->iformat->read_packet(ic,&ffmpi->pkt)<0)
    break;
   if(ffmpi->pkt.stream_index==ffmpi->audio_stream_index)
    if(ffmpi->pkt.pts && ffmpi->pkt.pts!=AV_NOPTS_VALUE)
     end_pts=ffmpi->pkt.pts;
  }while(1);
  if(end_pts>start_pts){
   st->duration=end_pts-start_pts;
   break;
  }
  readblocksize*=2;
 }while(--readretry);*/

  err_out_durpts:
	if(st->duration && st->duration != AV_NOPTS_VALUE)
		duration = av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
	if(oldfilepos >= 0)
		url_fseek(ic->pb, oldfilepos, SEEK_SET);

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("s:%d e:%d tn:%d td:%d d:%d", (long)start_pts, (long)end_pts, (long)st->time_base.num, (long)st->time_base.den, (long)duration);
#endif
	return duration;
}

static int inffmpg_assign_values_audio(struct ffmpg_decoder_data_s *ffmpi, struct mpxplay_infile_info_s *miis, unsigned int full_load)
{
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	AVFormatContext *ic = &ffmpi->fctx;
	unsigned int i;

	for(i = 0; i < ic->nb_streams; i++) {
		AVStream *st = ic->streams[i];
		AVCodecContext *codec = st->codec;
		if(codec && codec->codec_type == CODEC_TYPE_AUDIO) {
			switch (codec->codec_id) {
			case CODEC_ID_PCM_S8:
				spi->wave_id = MPXPLAY_WAVEID_PCM_SLE;
				adi->bits = 8;
				break;
			case CODEC_ID_PCM_S16LE:
				spi->wave_id = MPXPLAY_WAVEID_PCM_SLE;
				adi->bits = 16;
				break;
			case CODEC_ID_PCM_S24LE:
				spi->wave_id = MPXPLAY_WAVEID_PCM_SLE;
				adi->bits = 24;
				break;
			case CODEC_ID_PCM_S32LE:
				spi->wave_id = MPXPLAY_WAVEID_PCM_SLE;
				adi->bits = 32;
				break;
			case CODEC_ID_PCM_F32LE:
				spi->wave_id = MPXPLAY_WAVEID_PCM_FLOAT;
				adi->bits = 1;
				break;
			case CODEC_ID_AAC:
				spi->wave_id = MPXPLAY_WAVEID_AAC;
				break;
			case CODEC_ID_AC3:
				spi->wave_id = MPXPLAY_WAVEID_AC3;
				break;
			case CODEC_ID_DTS:
				spi->wave_id = MPXPLAY_WAVEID_DTS;
				break;
			case CODEC_ID_MP2:
				spi->wave_id = MPXPLAY_WAVEID_MP2;
				break;
			case CODEC_ID_MP3:
				spi->wave_id = MPXPLAY_WAVEID_MP3;
				break;
			case CODEC_ID_VORBIS:
				spi->wave_id = MPXPLAY_WAVEID_VORBIS;
				break;
			case CODEC_ID_WMAV1:
				spi->wave_id = MPXPLAY_WAVEID_WMAV1;
				break;
			case CODEC_ID_WMAV2:
				spi->wave_id = MPXPLAY_WAVEID_WMAV2;
				break;
			case CODEC_ID_FLAC:
				spi->wave_id = MPXPLAY_WAVEID_FLAC;
				break;
			default:
				break;
			}
			adi->freq = codec->sample_rate;
			adi->filechannels = adi->outchannels = codec->channels;
			spi->extradata = codec->extradata;
			spi->extradata_size = codec->extradata_size;
			ffmpi->audio_stream_index = i;
			if(!ic->duration && (ic->iformat == &mpegts_demuxer))
				ic->duration = inffmpg_get_stream_duration_pts(ffmpi, miis, st);
			if(ic->duration)
				miis->timemsec = ic->duration / 1000;	// /1000=*1000/AV_TIME_BASE
			else
				miis->timemsec = 180000;	// !!! hack
			spi->streamtype = MPXPLAY_SPI_STREAMTYPE_AUDIO;
			if(full_load) {
				funcbit_enable(spi->flags, (MPXPLAY_SPI_FLAG_NEED_DECODER | MPXPLAY_SPI_FLAG_NEED_PARSING | MPXPLAY_SPI_FLAG_CONTAINER));
				if(adi->infobits & ADI_CNTRLBIT_BITSTREAMOUT)
					funcbit_enable(adi->infobits, (ADI_FLAG_BITSTREAMOUT | ADI_FLAG_BITSTREAMNOFRH));
			}
			break;
		}
	}

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("freq:%d chans:%d bits:%d duration:%d br:%d wid:%8.8X", adi->freq, adi->outchannels, adi->bits, miis->timemsec, adi->bitrate, spi->wave_id);
#endif

	return 1;
}

static void *inffmpg_check_header(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis, unsigned int full_load)
{
	struct ffmpg_decoder_data_s *ffmpi;
	AVFormatContext *ic;
	unsigned int i;
	char *ext;

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("check_header begin");
#endif

	ffmpi = calloc(1, sizeof(struct ffmpg_decoder_data_s));
	if(!ffmpi)
		return ffmpi;
	miis->private_data = ffmpi;

	ffmpi->file_priv_datas.fbfs = fbfs;
	ffmpi->file_priv_datas.fbds = fbds;

	ext = pds_strrchr(filename, '.');
	if(!ext)
		goto err_out_chk;
	ext++;

	ic = &ffmpi->fctx;

	for(i = 0; i < INFFMPG_ALLMUXER_NUM; i++)
		if(pds_stricmp(alldemuxers[i].ext, ext) == 0) {
			ic->iformat = alldemuxers[i].demuxer;
			break;
		}
	if(!ic->iformat)
		goto err_out_chk;

	if(ic->iformat->priv_data_size > 0) {
		ic->priv_data = calloc(1, ic->iformat->priv_data_size);
		if(!ic->priv_data)
			goto err_out_chk;
	}
	ic->probesize = 5000000;

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("url_fopen_protocol");
#endif

	if(url_open_protocol(&ffmpi->uctx, &inffmpg_file_protocol, filename, URL_RDONLY, &ffmpi->file_priv_datas) < 0)
		goto err_out_chk;

	miis->filesize = fbfs->filelength(fbds);

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("url_fdopen");
#endif

	if(url_fdopen(&ic->pb, ffmpi->uctx) < 0)
		goto err_out_chk;

	//if(url_setbufsize(ic->pb,128000)<0)
	// goto err_out_chk;

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("read header() ts:%8.8X pb:%8.8X", ic->priv_data, ic->pb);
#endif

	if(ic->iformat->read_header(ic, &ffmpi->ap) < 0)
		goto err_out_chk;

	if(av_new_packet(&ffmpi->pkt, INFFMPG_INITIAL_PACKETBUF_SIZE) != 0)
		goto err_out_chk;

	inffmpg_assign_values_audio(ffmpi, miis, full_load);

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("av_open ok");
#endif

	return ffmpi;

  err_out_chk:
#ifdef INFFMPG_DEBUG
	inffmpg_debugf("err_out_chk");
#endif
	INFFMPG_infile_close(fbfs, fbds, miis);
	return NULL;
}

static int INFFMPG_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	if(inffmpg_check_header(fbfs, fbds, filename, miis, 0) != NULL)
		return MPXPLAY_ERROR_INFILE_OK;
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int INFFMPG_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct ffmpg_decoder_data_s *ffmpi;

	ffmpi = inffmpg_check_header(fbfs, fbds, filename, miis, 1);
	if(!ffmpi)
		return MPXPLAY_ERROR_INFILE_CANTOPEN;

	return MPXPLAY_ERROR_INFILE_OK;
}

static void INFFMPG_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)miis->private_data;
	if(ffmpi) {
		ffmpi->file_priv_datas.fbds = fbds;
		av_free_packet(&ffmpi->pkt);
		av_close_input_file(&ffmpi->fctx);
		free(ffmpi);
	}
}

//-----------------------------------------------------------------------
//decoding

static int INFFMPG_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)miis->private_data;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	int retcode;
	mpxp_uint64_t filepos;
	AVFormatContext *ic = &ffmpi->fctx;

	ffmpi->file_priv_datas.fbds = fbds;

	filepos = url_ftell(ic->pb);

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("read_packet1: buf:%8.8X %d fp:%d", spi->bitstreambuf, spi->bs_bufsize, (long)filepos);
#endif

	do {
		retcode = ic->iformat->read_packet(ic, &ffmpi->pkt);
#ifdef INFFMPG_DEBUG
		inffmpg_debugf("ic_read_packet: %d", retcode);
#endif
		if(retcode < 0) {
			if(fbfs->eof(fbds))
				retcode = MPXPLAY_ERROR_INFILE_EOF;
			else if(retcode == AVERROR(EIO) || retcode == AVERROR(EAGAIN)) {
				url_fseek(ic->pb, filepos, SEEK_SET);
				retcode = MPXPLAY_ERROR_INFILE_SYNC_IN;
			} else
				retcode = MPXPLAY_ERROR_INFILE_NODATA;
			goto err_out_demux;
		}
		if(ffmpi->pkt.stream_index == ffmpi->audio_stream_index)
			break;
		if(ic->iformat == &matroska_demuxer)	// !!! hack
			av_destruct_packet(&ffmpi->pkt);	//
	} while(1);

	if(!ffmpi->pkt.size || !ffmpi->pkt.data || (ffmpi->pkt.size > spi->bs_bufsize)) {
		retcode = MPXPLAY_ERROR_INFILE_NODATA;
		goto err_out_demux;
	}

	pds_memcpy(spi->bitstreambuf, ffmpi->pkt.data, ffmpi->pkt.size);
	spi->bs_leftbytes = ffmpi->pkt.size;

	retcode = MPXPLAY_ERROR_INFILE_OK;

  err_out_demux:
	if(ic->iformat == &matroska_demuxer)	// !!! hack
		av_destruct_packet(&ffmpi->pkt);	//

#ifdef INFFMPG_DEBUG
	inffmpg_debugf("rp2: r:%d %8.8X %d i:%d ai:%d fp:%d", retcode, ffmpi->pkt.data, ffmpi->pkt.size, ffmpi->pkt.stream_index, ffmpi->audio_stream_index, (long)filepos);
#endif
	return retcode;
}

//-------------------------------------------------------------------------
// seeking

static void INFFMPG_clearbuffs(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, unsigned int seektype)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)miis->private_data;
	ffmpi->file_priv_datas.fbds = fbds;
}

static long INFFMPG_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newmpxframenum)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)miis->private_data;
	AVFormatContext *ic = &ffmpi->fctx;
	AVRational *tb = &(ic->streams[ffmpi->audio_stream_index]->time_base);
	mpxp_int64_t newtimepos;
	//char sout[100];

	ffmpi->file_priv_datas.fbds = fbds;
	if(tb->den && tb->num && (ic->iformat != &mpegts_demuxer)) {	// !!! hack
		float tbd = av_q2d(*tb);
		newtimepos = (float)newmpxframenum *(float)miis->timemsec / 1000.0 / (float)miis->allframes / tbd;
		if(ic->iformat->read_seek(ic, ffmpi->audio_stream_index, newtimepos, AVSEEK_FLAG_ANY) < 0)
			return MPXPLAY_ERROR_INFILE_EOF;
	} else {
		newtimepos = (float)newmpxframenum *(float)miis->filesize / (float)miis->allframes;
		if(url_fseek(ffmpi->fctx.pb, newtimepos, SEEK_SET) < 0)
			return MPXPLAY_ERROR_INFILE_EOF;
	}
	//sprintf(sout,"si:%d td:%d tn:%d nf:%d",ffmpi->audio_stream_index,tb->num,tb->den,(long)newtimepos);
	//display_message(1,0,sout);
	//getch();
	return newmpxframenum;
}

//--------------------------------------------------------------------------
// read tag infos from AVFormatContext (need to call check_header before)

static char *inffmpg_tag_get_str(char *str, char **id3ip, char *id3p, struct mpxplay_textconv_func_s *mtfs)
{
	unsigned int len;

	if(str[0]) {
		len = pds_strncpy(id3p, str, 510);
		if(len) {
			id3p[len] = 0;
			if((*(mtfs->control)) & ID3TEXTCONV_UTF_AUTO)	// !!! FLAC only
				len = mtfs->utf8_to_char(id3p, len);
			len = mtfs->all_to_char(id3p, len, ID3TEXTCONV_UTF8);
			if(len) {
				*id3ip = id3p;
				id3p += len + 1;
			}
		}
	}
	return id3p;
}

static char *inffmpg_tag_get_num(unsigned int num, char **id3ip, char *id3p)
{
	unsigned int len;
	if(num) {
		len = snprintf(id3p, 16, "%d", num);
		id3p[len] = 0;
		*id3ip = id3p;
		id3p += len + 1;
	}
	return id3p;
}

static char *INFFMPG_tag_get(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, char *id3p, struct mpxplay_textconv_func_s *mtfs)
{
	struct ffmpg_decoder_data_s *ffmpi = (struct ffmpg_decoder_data_s *)miis->private_data;
	AVFormatContext *fctx = &ffmpi->fctx;

	id3p = inffmpg_tag_get_str(fctx->title, &id3ip[I3I_TITLE], id3p, mtfs);
	id3p = inffmpg_tag_get_str(fctx->author, &id3ip[I3I_ARTIST], id3p, mtfs);
	id3p = inffmpg_tag_get_str(fctx->comment, &id3ip[I3I_COMMENT], id3p, mtfs);
	id3p = inffmpg_tag_get_str(fctx->album, &id3ip[I3I_ALBUM], id3p, mtfs);
	id3p = inffmpg_tag_get_str(fctx->genre, &id3ip[I3I_GENRE], id3p, mtfs);

	id3p = inffmpg_tag_get_num(fctx->year, &id3ip[I3I_YEAR], id3p);
	id3p = inffmpg_tag_get_num(fctx->track, &id3ip[I3I_TRACKNUM], id3p);

	return id3p;
}

//--------------------------------------------------------------------------
// file protocol (connected to mpxinbuf)

static int inffmpg_file_open(URLContext * h, const char *filename, int flags)
{
	inffmpg_file_private_data_s *pd = (inffmpg_file_private_data_s *) h->priv_data;
	if((flags & URL_RDWR) || (flags & URL_WRONLY)) {
		if(!pd->fbfs->fopen_write(pd->fbds, (char *)filename))
			return -ENOENT;
	} else {
		if(!pd->fbfs->fopen_read(pd->fbds, (char *)filename, 0))
			return -ENOENT;
	}
	return 0;
}

static int inffmpg_file_read(URLContext * h, unsigned char *buf, int size)
{
	inffmpg_file_private_data_s *pd = (inffmpg_file_private_data_s *) h->priv_data;
	return pd->fbfs->fread(pd->fbds, buf, size);
}

static int inffmpg_file_write(URLContext * h, unsigned char *buf, int size)
{
	inffmpg_file_private_data_s *pd = (inffmpg_file_private_data_s *) h->priv_data;
	return pd->fbfs->fwrite(pd->fbds, buf, size);
}

static int64_t inffmpg_file_seek(URLContext * h, int64_t pos, int whence)
{
	inffmpg_file_private_data_s *pd = (inffmpg_file_private_data_s *) h->priv_data;
	return pd->fbfs->fseek(pd->fbds, pos, whence);
}

static int inffmpg_file_close(URLContext * h)
{
	inffmpg_file_private_data_s *pd = (inffmpg_file_private_data_s *) h->priv_data;
	pd->fbfs->fclose(pd->fbds);
	return 0;
}

static URLProtocol inffmpg_file_protocol = {
	"file",
	inffmpg_file_open,
	inffmpg_file_read,
	inffmpg_file_write,
	inffmpg_file_seek,
	inffmpg_file_close,
	NULL,
	NULL,
	NULL,
	NULL
};

//--------------------------------------------------------------------------

struct mpxplay_infile_func_s IN_FFMPG_funcs = {
	0,
	&INFFMPG_preinit,
	NULL,
	&INFFMPG_infile_check,
	&INFFMPG_infile_check,
	&INFFMPG_infile_open,
	&INFFMPG_infile_close,
	&INFFMPG_infile_decode,
	&INFFMPG_fseek,
	&INFFMPG_clearbuffs,
	&INFFMPG_tag_get,
	NULL,
	NULL,
	{							// (usually) no such information in the FFMPG lib, we must to set this here manually
	 "MKV", "TS",
	 NULL}
};

#endif							// MPXPLAY_LINK_INFILE_FFMPG
