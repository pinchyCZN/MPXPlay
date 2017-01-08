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
//function: OGG file demuxing
//requires the decoders\ad_vorbi\vorbis.lib file (and include files)
//(ogg.lib (bitwise.c and framing.c) is part of the vorbis.lib)

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_OGG

#include "newfunc\newfunc.h"
#include "..\decoders\ad_vorbi\ogg.h"
#include <string.h>

#define INOGG_READHEAD_DETECTSIZE 32768
#define INOGG_READHEAD_CHECKSIZE  65536

#define INOGG_MAX_STREAMS 8
#define MPXPLAY_CODECID_THEORA 0x00028000

typedef struct ogg_parse_data_s;

/*typedef struct ogg_comment_s{
 unsigned int typelen;
 char *typestr;
 unsigned int datalen;
 char *datastr;
 int i3i_index;
}ogg_comment_s;*/

typedef struct ogg_stream_data_s {
	struct ogg_parse_data_s *parsedatas;
	mpxp_uint8_t *extradata;
	long extradata_size;
	long audio_channels;
	long audio_freq;
	long audio_bits;
	mpxp_int64_t pcmdatalen;
	ogg_stream_state oss;
	ogg_packet ops;
} ogg_stream_data_s;

typedef struct ogg_demuxer_data_s {
	unsigned int current_decoder_part;
	unsigned int nb_streams;
	struct ogg_stream_data_s *stream_audio;
	struct ogg_stream_data_s *stream_video;
	ogg_sync_state oys;
	ogg_page ogs;
	struct ogg_stream_data_s ogg_streams[INOGG_MAX_STREAMS];

	/*unsigned long metadata_size_cur;
	   unsigned long metadata_size_new;
	   unsigned long comments_totallen;
	   unsigned int comments_allocated;
	   unsigned int comments_loaded;
	   ogg_comment_s *comment_datas; */

} ogg_demuxer_data_s;

typedef struct ogg_parse_data_s {
	char *longname;
	unsigned int streamtype;
	unsigned long waveid;
	unsigned int (*parse_func) (struct ogg_stream_data_s * osds, unsigned char *packet, unsigned int bytes);
	unsigned int (*seekcomment_func) (struct mpxplay_bitstreambuf_s * bs);
} ogg_parse_data_s;

static struct ogg_stream_data_s *ogg_find_stream_by_serialno(struct ogg_demuxer_data_s *omip, unsigned long serialno);
static struct ogg_stream_data_s *ogg_find_stream_by_streamtype(struct ogg_demuxer_data_s *omip, unsigned int streamtype, struct mpxplay_streampacket_info_s *spi);
static void ogg_reset_streams(struct ogg_demuxer_data_s *omip);

static unsigned int ogg_parse_vorbis(struct ogg_stream_data_s *osds, unsigned char *packet, unsigned int bytes);
static unsigned int ogg_parse_flac(struct ogg_stream_data_s *osds, unsigned char *packet, unsigned int bytes);
static unsigned int ogg_parse_speex(struct ogg_stream_data_s *osds, unsigned char *packet, unsigned int bytes);
//static unsigned int ogg_parse_theora(struct ogg_stream_data_s *osds,unsigned char *packet,unsigned int bytes);

static unsigned int ogg_seekcomment_vorbis(struct mpxplay_bitstreambuf_s *bs);
static unsigned int ogg_seekcomment_flac(struct mpxplay_bitstreambuf_s *bs);
//static unsigned int ogg_seekcomment_theora(struct mpxplay_bitstreambuf_s *bs);

static ogg_parse_data_s ogg_parse_datas[] = {
	{"OgVorbis", MPXPLAY_SPI_STREAMTYPE_AUDIO, MPXPLAY_WAVEID_VORBIS, &ogg_parse_vorbis, &ogg_seekcomment_vorbis},
	{"Ogg-Flac", MPXPLAY_SPI_STREAMTYPE_AUDIO, MPXPLAY_WAVEID_FLAC, &ogg_parse_flac, &ogg_seekcomment_flac},
	{"OggSpeex", MPXPLAY_SPI_STREAMTYPE_AUDIO, MPXPLAY_WAVEID_SPEEX, &ogg_parse_speex, NULL},
	//{"OgTheora",MPXPLAY_SPI_STREAMTYPE_VIDEO,MPXPLAY_CODECID_THEORA,&ogg_parse_theora,&ogg_seekcomment_theora},
	{NULL, 0}
};

static int refill_sync_buff(struct ogg_demuxer_data_s *omip, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds);
static ogg_int64_t ogg_get_pcmlength(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds);

static int ogg_assign_values(struct ogg_stream_data_s *osds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	long filelen = fbfs->filelength(fbds), bytes_per_sample;

	if(!osds->audio_channels || !osds->audio_freq || !osds->audio_bits)
		return 0;

	if(osds->audio_channels)
		adi->filechannels = adi->outchannels = osds->audio_channels;
	if(osds->audio_freq)
		adi->freq = osds->audio_freq;
	if(osds->audio_bits)
		adi->bits = osds->audio_bits;

	if(miis->filesize != filelen || !miis->timemsec) {
		miis->filesize = filelen;
		if(!osds->pcmdatalen)
			osds->pcmdatalen = ogg_get_pcmlength(fbfs, fbds);
		if(!osds->pcmdatalen)
			return 0;
		if(osds->audio_freq)
			miis->timemsec = (float)osds->pcmdatalen * 1000.0 / (float)osds->audio_freq;
	}

	if(osds->parsedatas->waveid == MPXPLAY_WAVEID_FLAC) {
		adi->bitratetext = malloc(MPXPLAY_ADITEXTSIZE_BITRATE + 8);
		if(!adi->bitratetext)
			return 0;

		if(!osds->pcmdatalen)
			osds->pcmdatalen = (float)miis->timemsec / 1000.0 * (float)osds->audio_freq;
		if(!osds->pcmdatalen)
			return 0;

		bytes_per_sample = (adi->bits + 7) / 8;
		sprintf(adi->bitratetext, "%2d/%2.1f%%", adi->bits, 100.0 * (float)miis->filesize / (float)osds->pcmdatalen / (float)bytes_per_sample / (float)adi->filechannels);
		adi->bitratetext[MPXPLAY_ADITEXTSIZE_BITRATE] = 0;
	} else if(miis->timemsec)
		adi->bitrate = (long)((float)filelen * 8.0 / (float)miis->timemsec);

	return 1;
}

static int inogg_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis, unsigned long headchecksize)
{
	struct ogg_demuxer_data_s *omip;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct ogg_stream_data_s *osds;
	unsigned int sync_retry = headchecksize / OGG_SYNC_BUFFER_SIZE + 1;

	if(!fbfs->fopen_read(fbds, filename, 0))
		return MPXPLAY_ERROR_INFILE_FILEOPEN;
	if(fbfs->filelength(fbds) < 128)	// ???
		goto err_out_chkhead;

	omip = (struct ogg_demuxer_data_s *)calloc(1, sizeof(struct ogg_demuxer_data_s));
	if(!omip)
		goto err_out_chkhead;
	miis->private_data = omip;

	ogg_sync_init(&(omip->oys));
	refill_sync_buff(omip, fbfs, fbds);

	do {
		int result = ogg_sync_pageout(&(omip->oys), &(omip->ogs));
		if(result < 0)
			goto err_out_chkhead;
		if(result == 0) {
			if(!(--sync_retry))
				goto err_out_chkhead;
			if(!refill_sync_buff(omip, fbfs, fbds))
				goto err_out_chkhead;
		} else {
			struct ogg_parse_data_s *parsedatas;

			osds = ogg_find_stream_by_serialno(omip, ogg_page_serialno(&(omip->ogs)));
			if(osds)			// no new frame
				break;
			if(omip->nb_streams >= INOGG_MAX_STREAMS)
				goto err_out_chkhead;

			osds = &omip->ogg_streams[omip->nb_streams++];
			if(ogg_stream_init(&(osds->oss), ogg_page_serialno(&(omip->ogs))) < 0)
				goto err_out_chkhead;
			if(ogg_stream_pagein(&(osds->oss), &(omip->ogs)) < 0)
				goto err_out_chkhead;
			if(ogg_stream_packetout(&(osds->oss), &(osds->ops)) != 1)
				goto err_out_chkhead;

			parsedatas = &ogg_parse_datas[0];
			do {
				if(parsedatas->parse_func(osds, osds->ops.packet, osds->ops.bytes)) {
					osds->parsedatas = parsedatas;
					break;
				}
				parsedatas++;
			} while(parsedatas->longname);
		}
	} while(1);

	osds = ogg_find_stream_by_streamtype(omip, MPXPLAY_SPI_STREAMTYPE_AUDIO, spi);
	if(!osds || !osds->parsedatas)
		goto err_out_chkhead;

	omip->stream_audio = osds;
	spi->wave_id = osds->parsedatas->waveid;

	miis->longname = osds->parsedatas->longname;

	if(!ogg_assign_values(osds, fbfs, fbds, miis))
		goto err_out_chkhead;

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_chkhead:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int OGG_infile_detect(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	return inogg_infile_check(fbfs, fbds, filename, miis, INOGG_READHEAD_DETECTSIZE);
}

static int OGG_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	return inogg_infile_check(fbfs, fbds, filename, miis, INOGG_READHEAD_CHECKSIZE);
}

static int OGG_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct ogg_demuxer_data_s *omip;
	struct ogg_stream_data_s *osds;
	int retcode;

	retcode = inogg_infile_check(fbfs, fbds, filename, miis, INOGG_READHEAD_CHECKSIZE);
	if(retcode != MPXPLAY_ERROR_INFILE_OK)
		return retcode;

	omip = miis->private_data;
	osds = omip->stream_audio;
	spi->extradata = osds->extradata;
	spi->extradata_size = osds->extradata_size;
	spi->streamtype = MPXPLAY_SPI_STREAMTYPE_AUDIO;
	funcbit_enable(spi->flags, MPXPLAY_SPI_FLAG_NEED_DECODER | MPXPLAY_SPI_FLAG_NEED_PARSING | MPXPLAY_SPI_FLAG_CONTAINER);

	return retcode;
}

static void OGG_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct ogg_demuxer_data_s *omip = miis->private_data;
	struct ogg_stream_data_s *osds;
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	unsigned int i;
	if(omip) {
		if(adi->bitratetext)
			free(adi->bitratetext);
		osds = &omip->ogg_streams[0];
		for(i = 0; i < omip->nb_streams; i++, osds++) {
			if(osds->extradata)
				free(osds->extradata);
			ogg_stream_clear(&(osds->oss));
		}
		ogg_sync_clear(&(omip->oys));
		/*if(omip->comment_datas){
		   for(i=0;i<omip->comments_allocated;i++){
		   if(omip->comment_datas[i].typestr)
		   free(omip->comment_datas[i].typestr);
		   if(omip->comment_datas[i].datastr)
		   free(omip->comment_datas[i].datastr);
		   }
		   free(omip->comment_datas);
		   } */
		free(omip);
	}
	fbfs->fclose(fbds);
}

static int refill_sync_buff(struct ogg_demuxer_data_s *omip, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	char *buffer;
	unsigned long bytes;

	buffer = ogg_sync_buffer(&(omip->oys), OGG_SYNC_BUFFER_SIZE);
	bytes = fbfs->fread(fbds, buffer, OGG_SYNC_BUFFER_SIZE);
	ogg_sync_wrote(&(omip->oys), bytes);

	return bytes;
}

static int read_ogg_frame(struct ogg_demuxer_data_s *omip, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	int result, retry = 16;
	do {
		result = ogg_sync_pageout(&(omip->oys), &(omip->ogs));
		if(result > 0) {
			struct ogg_stream_data_s *osds = ogg_find_stream_by_serialno(omip, ogg_page_serialno(&(omip->ogs)));
			if(osds == omip->stream_audio)
				ogg_stream_pagein(&(osds->oss), &(omip->ogs));
			else
				result = -1;	// new ogg_sync_pageout()
		}
		if(result == 0)
			if(!refill_sync_buff(omip, fbfs, fbds))
				return 0;
	} while((result <= 0) && (--retry));
	if(result <= 0)
		return 0;
	return 1;
}

static int decode_ogg_frame(struct ogg_demuxer_data_s *omip, struct mpxplay_infile_info_s *miis)
{
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct ogg_stream_data_s *osds = omip->stream_audio;
	if(ogg_stream_packetout(&(osds->oss), &(osds->ops)) > 0) {
		spi->bs_leftbytes = osds->ops.bytes;
		if(!spi->bs_leftbytes)
			return 0;
		pds_memcpy(spi->bitstreambuf, osds->ops.packet, osds->ops.bytes);
		return 1;
	}
	return 0;
}

/*static int decode_ogg_frame(struct ogg_demuxer_data_s *omip,struct mpxplay_infile_info_s *miis)
{
 struct mpxplay_streampacket_info_s *spi=miis->audio_stream;
 if(ogg_stream_packetout(&(omip->oss),&(omip->ops))>0){
  pds_memcpy(spi->bitstreambuf,&omip->ops,sizeof(omip->ops));
  spi->bs_leftbytes=sizeof(omip->ops);
  pds_memcpy(spi->bitstreambuf+spi->bs_leftbytes,omip->ops.packet,omip->ops.bytes);
  spi->bs_leftbytes+=omip->ops.bytes;
  return 1;
 }
 return 0;
}*/

static int OGG_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct ogg_demuxer_data_s *omip = miis->private_data;
	int eos;

	do {
		eos = 0;
		switch (omip->current_decoder_part) {
		case 0:
			if(read_ogg_frame(omip, fbfs, fbds)) {
				omip->current_decoder_part = 1;
			} else {
				eos = 1;
				break;
			}
		case 1:
			if(!decode_ogg_frame(omip, miis)) {
				omip->current_decoder_part = 0;
				break;
			} else
				eos = 2;
		}
	} while(!eos);

	if(eos == 1)
		return MPXPLAY_ERROR_INFILE_NODATA;

	return MPXPLAY_ERROR_INFILE_OK;
}

static void OGG_infile_clearbuffs(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, unsigned int seektype)
{
	struct ogg_demuxer_data_s *omip = miis->private_data;

	ogg_sync_reset(&(omip->oys));
	//ogg_stream_reset(&(omip->stream_audio->oss));
	ogg_reset_streams(omip);

	if(seektype & (MPX_SEEKTYPE_BOF | MPX_SEEKTYPE_PAUSE))
		omip->current_decoder_part = 0;
}

static long OGG_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newframenum)
{
	long newfilepos = (float)newframenum * (float)miis->filesize / (float)miis->allframes;
	if(fbfs->fseek(fbds, newfilepos, SEEK_SET) < 0)
		return MPXPLAY_ERROR_INFILE_EOF;
	return newframenum;
}

//-------------------------------------------------------------------------

#define OGG_CHUNKSIZE 8192
#define OGG_CHUNKNUM  8

static ogg_int64_t ogg_get_pcmlength(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	static ogg_sync_state oy;
	static ogg_page og;
	char *ptr, *buffer;
	ogg_int64_t pcmlen = 0;
	mpxp_filesize_t newfilepos = 0, oldfilepos = fbfs->ftell(fbds);
	mpxp_filesize_t filelen = fbfs->filelength(fbds);
	long bytes, oyret;
	int retry1 = 20, retry2;

	if(!filelen)
		return 0;
	ogg_sync_init(&oy);
	ogg_sync_buffer(&oy, OGG_CHUNKSIZE * OGG_CHUNKNUM);
	if(oy.data == NULL)
		return 0;
	do {
		retry2 = 30;
		pds_memset((void *)(&og), 0, sizeof(ogg_page));
		oy.returned = oy.fill = OGG_CHUNKSIZE * OGG_CHUNKNUM;
		newfilepos = 0;
		do {
			if((newfilepos - OGG_CHUNKSIZE) > (-filelen)) {
				bytes = OGG_CHUNKSIZE;
				newfilepos -= OGG_CHUNKSIZE;
			} else {
				bytes = filelen + newfilepos;
				newfilepos = -(filelen);
			}
			if(fbfs->fseek(fbds, filelen + newfilepos, SEEK_SET) < 0)
				break;
			ptr = oy.data + oy.returned;
			buffer = ptr - bytes;
			bytes = fbfs->fread(fbds, buffer, bytes);
			if(bytes < 1)
				break;
			while((bytes > 0) && (pcmlen < 1)) {
				while((bytes > 0) && (PDS_GETB_LE32(ptr) != PDS_GET4C_LE32('O', 'g', 'g', 'S'))) {
					ptr--;
					bytes--;
					oy.returned--;
				}
				oyret = oy.returned;
				if(PDS_GETB_LE32(ptr) == PDS_GET4C_LE32('O', 'g', 'g', 'S')) {
					if(ogg_sync_pageout(&oy, &og) > 0)
						pcmlen = ogg_page_granulepos(&og);
					if(pcmlen < 1)
						if(bytes > 0) {
							ptr--;
							bytes--;
							oyret--;
						}
				}
				oy.headerbytes = 0;
				oy.bodybytes = 0;
				oy.returned = oyret;
			}
		} while((pcmlen < 1) && (oy.returned > 0) && (retry2--));
	} while((pcmlen < 1) && (--retry1));
	ogg_sync_clear(&oy);
	fbfs->fseek(fbds, oldfilepos, SEEK_SET);
	return pcmlen;
}

//--------------------------------------------------------------------------
static struct ogg_stream_data_s *ogg_find_stream_by_serialno(struct ogg_demuxer_data_s *omip, unsigned long serialno)
{
	ogg_stream_data_s *osds = &omip->ogg_streams;
	unsigned int i;

	for(i = 0; i < omip->nb_streams; i++) {
		if(osds->oss.serialno == serialno)
			return osds;
		osds++;
	}

	return NULL;
}

static struct ogg_stream_data_s *ogg_find_stream_by_streamtype(struct ogg_demuxer_data_s *omip, unsigned int streamtype, struct mpxplay_streampacket_info_s *spi)
{
	ogg_stream_data_s *osds = &omip->ogg_streams, *osd_found = NULL;
	unsigned int i, typecount = 0;

	for(i = 0; i < omip->nb_streams; i++) {
		if(osds->parsedatas && (osds->parsedatas->streamtype == streamtype)) {
			if(typecount <= spi->stream_select)
				osd_found = osds;
			typecount++;
		}
		osds++;
	}

	return osd_found;
}

static void ogg_reset_streams(struct ogg_demuxer_data_s *omip)
{
	ogg_stream_data_s *osds = &omip->ogg_streams;
	unsigned int i;

	for(i = 0; i < omip->nb_streams; i++, osds++)
		ogg_stream_reset(&(osds->oss));
}

static unsigned int ogg_parse_vorbis(struct ogg_stream_data_s *osds, unsigned char *packet, unsigned int bytes)
{
	struct mpxplay_bitstreambuf_s bs;

	mpxplay_bitstream_init(&bs, packet, bytes);

	// little endian bitstream in Vorbis!
	if(mpxplay_bitstream_get_byte(&bs) != 0x01)	// packtype
		return 0;
	if(memcmp(mpxplay_bitstream_getbufpos(&bs), "vorbis", 6) != 0)
		return 0;
	mpxplay_bitstream_skipbytes(&bs, 6);

	if(mpxplay_bitstream_get_le32(&bs) != 0)	// version
		return 0;

	osds->audio_channels = mpxplay_bitstream_get_byte(&bs);
	osds->audio_freq = mpxplay_bitstream_get_le32(&bs);
	osds->audio_bits = 16;

	return 1;
}

static unsigned int ogg_seekcomment_vorbis(struct mpxplay_bitstreambuf_s *bs)
{
	if(mpxplay_bitstream_get_byte(bs) != 0x03)	// packtype
		return 0;
	if(memcmp(mpxplay_bitstream_getbufpos(bs), "vorbis", 6) != 0)
		return 0;
	mpxplay_bitstream_skipbytes(bs, 6);
	return 1;
}

#define FLAC_METADATA_TYPE_VORBISCOMMENT 4
#define FLAC_METADATA_SIZE_STREAMINFO   34

static unsigned int ogg_parse_flac(struct ogg_stream_data_s *osds, unsigned char *packet, unsigned int bytes)
{
	struct mpxplay_bitstreambuf_s bs;

	mpxplay_bitstream_init(&bs, packet, bytes);

	if(mpxplay_bitstream_get_byte(&bs) != 0x7f)
		return 0;
	if(memcmp(mpxplay_bitstream_getbufpos(&bs), "FLAC", 4) != 0)
		return 0;
	mpxplay_bitstream_skipbytes(&bs, 4 + 1 + 1);	// skip FLAC and version numbers
	mpxplay_bitstream_get_be16(&bs);	// header_packets
	if(memcmp(mpxplay_bitstream_getbufpos(&bs), "fLaC", 4) != 0)
		return 0;
	mpxplay_bitstream_skipbytes(&bs, 4 + 4);	// skip fLaC and metadata last(1),type(7),size(24 bits)
	if(!osds->extradata) {
		osds->extradata = malloc(FLAC_METADATA_SIZE_STREAMINFO + MPXPLAY_SPI_EXTRADATA_PADDING);
		if(osds->extradata) {
			pds_memcpy(osds->extradata, mpxplay_bitstream_getbufpos(&bs), FLAC_METADATA_SIZE_STREAMINFO);
			pds_memset(osds->extradata + FLAC_METADATA_SIZE_STREAMINFO, 0, MPXPLAY_SPI_EXTRADATA_PADDING);
			osds->extradata_size = FLAC_METADATA_SIZE_STREAMINFO;
		}
	}
	mpxplay_bitstream_skipbits(&bs, 16 + 16 + 24 + 24);
	osds->audio_freq = mpxplay_bitstream_getbits_be24(&bs, 20);
	osds->audio_channels = mpxplay_bitstream_getbits_be24(&bs, 3) + 1;
	osds->audio_bits = mpxplay_bitstream_getbits_be24(&bs, 5) + 1;
	osds->pcmdatalen = mpxplay_bitstream_getbits_be64(&bs, 36);

	return 1;
}

static unsigned int ogg_seekcomment_flac(struct mpxplay_bitstreambuf_s *bs)
{
	mpxplay_bitstream_skipbits(bs, 1);	// metadata_last
	if(mpxplay_bitstream_getbits_be24(bs, 7) != FLAC_METADATA_TYPE_VORBISCOMMENT)
		return 0;
	mpxplay_bitstream_skipbits(bs, 24);	// metadata_size
	return 1;
}

#define SPEEX_HEADER_STRING_LENGTH   8
#define SPEEX_HEADER_VERSION_LENGTH 20

static unsigned int ogg_parse_speex(struct ogg_stream_data_s *osds, unsigned char *packet, unsigned int bytes)
{
	struct mpxplay_bitstreambuf_s bs;

	mpxplay_bitstream_init(&bs, packet, bytes);

	if(memcmp(mpxplay_bitstream_getbufpos(&bs), "Speex   ", SPEEX_HEADER_STRING_LENGTH) != 0)
		return 0;
	mpxplay_bitstream_skipbytes(&bs, SPEEX_HEADER_STRING_LENGTH + SPEEX_HEADER_VERSION_LENGTH);
	mpxplay_bitstream_skipbytes(&bs, 4 + 4);
	osds->audio_freq = mpxplay_bitstream_get_le32(&bs);
	mpxplay_bitstream_skipbytes(&bs, 4 + 4);
	osds->audio_channels = mpxplay_bitstream_get_le32(&bs);
	osds->audio_bits = 16;

	return 1;
}

#if 0
static unsigned int ogg_parse_theora(struct ogg_stream_data_s *osds, unsigned char *packet, unsigned int bytes)
{
	unsigned int version;
	struct mpxplay_bitstreambuf_s bs;

	mpxplay_bitstream_init(&bs, packet, bytes);

	if(mpxplay_bitstream_get_byte(&bs) != 0x80)	// packtype
		return 0;
	if(memcmp(mpxplay_bitstream_getbufpos(&bs), "theora", 6) != 0)
		return 0;
	mpxplay_bitstream_skipbytes(&bs, 6);

	version = mpxplay_bitstream_getbits_be24(&bs, 24);
	if(version < 0x030100)
		return 0;
/*
 video_width  = mpxplay_bitstream_getbits_be24(&bs, 16) << 4;
 video_height = mpxplay_bitstream_getbits_be24(&bs, 16) << 4;

 if(version >= 0x030400)
  mpxplay_bitstream_skipbits(&bs, 164);
 else if (version >= 0x030200)
  mpxplay_bitstream_skipbits(&bs, 64);

 time_base_den = mpxplay_bitstream_getbits_be24(&bs, 32);
 time_base_num = mpxplay_bitstream_getbits_be24(&bs, 32);

 sample_aspect_ratio_num = mpxplay_bitstream_getbits_be24(&bs, 24);
 sample_aspect_ratio_den = mpxplay_bitstream_getbits_be24(&bs, 24);

 if(version >= 0x030200)
  mpxplay_bitstream_skipbits(&bs, 38);
 if(version >= 0x304000)
  mpxplay_bitstream_skipbits(&bs, 2);

 gpshift = mpxplay_bitstream_getbits_be24(&bs, 5);
 gpmask = (1 << gpshift) - 1;*/

	return 1;
}

static unsigned int ogg_seekcomment_theora(struct mpxplay_bitstreambuf_s *bs)
{
	if(mpxplay_bitstream_get_byte(bs) != 0x83)	// packtype
		return 0;
	if(memcmp(mpxplay_bitstream_getbufpos(bs), "theora", 6) != 0)
		return 0;
	mpxplay_bitstream_skipbytes(bs, 6);
	return 1;
}
#endif							// 0 (disable theora)

//--------------------------------------------------------------------------
#define VORBIS_COMMENT_TYPES 9

static char *OGG_infile_tag_get(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, char *id3p,
								struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	static char *vorbiscommenttypes[VORBIS_COMMENT_TYPES] = { "title", "artist", "author", "album", "date", "comment", "description", "genre", "tracknumber" };
	static unsigned int id3index[VORBIS_COMMENT_TYPES] = { I3I_TITLE, I3I_ARTIST, I3I_ARTIST, I3I_ALBUM, I3I_YEAR, I3I_COMMENT, I3I_COMMENT, I3I_GENRE, I3I_TRACKNUM };
	struct ogg_demuxer_data_s *omip = miis->private_data;
	struct ogg_stream_data_s *osds = NULL, *osd_search;
	long i, metadata_size, counted_size, comment_size, comments, allframecounter = 0, streamframecounter = 0;
	struct mpxplay_bitstreambuf_s bs;
	mpxp_filesize_t filepos;
	unsigned char *bufpos;

	if(!omip->nb_streams)
		return id3p;

	osd_search = omip->stream_audio;
	if(!osd_search)
		osd_search = omip->stream_video;
	if(!osd_search)
		return id3p;

	filepos = fbfs->ftell(fbds);

	fbfs->fseek(fbds, 0, SEEK_SET);
	ogg_sync_reset(&(omip->oys));
	ogg_reset_streams(omip);

	do {
		int result = ogg_sync_pageout(&(omip->oys), &(omip->ogs));
		if(result < 0)
			goto err_out_id3;
		if(result == 0) {
			if(!refill_sync_buff(omip, fbfs, fbds))
				goto err_out_id3;
			continue;
		}
		if(osds)
			ogg_stream_reset(&(osds->oss));

		osds = ogg_find_stream_by_serialno(omip, ogg_page_serialno(&(omip->ogs)));
		if(osds != osd_search)
			continue;

		streamframecounter++;
		if(streamframecounter < 2)	// the vorbiscomment is in the 2. frame
			continue;
		if(streamframecounter > 2)
			break;

		if(ogg_stream_pagein(&(osds->oss), &(omip->ogs)) < 0)
			goto err_out_id3;
		if(ogg_stream_packetout(&(osds->oss), &(osds->ops)) != 1)
			goto err_out_id3;

		mpxplay_bitstream_init(&bs, osds->ops.packet, osds->ops.bytes);

		if(osds->parsedatas->seekcomment_func)
			if(!osds->parsedatas->seekcomment_func(&bs))
				continue;

		bufpos = mpxplay_bitstream_getbufpos(&bs);
		metadata_size = mpxplay_bitstream_leftbytes(&bs);
		counted_size = 0;
		comment_size = PDS_GETB_LE32(bufpos);
		bufpos += 4;
		counted_size += 4;
		if(comment_size >= metadata_size)
			goto err_out_id3;
		bufpos += comment_size;
		counted_size += comment_size;	// reference lib
		comments = PDS_GETB_LE32(bufpos);
		bufpos += 4;
		counted_size += 4;
		if(comments > (metadata_size / 2))
			goto err_out_id3;
		while(comments-- && (counted_size < metadata_size)) {
			comment_size = PDS_GETB_LE32(bufpos);
			bufpos += 4;
			counted_size += 4;
			if(comment_size) {
				char *p;
				if(comment_size > (metadata_size - counted_size))
					break;
				p = pds_strchr(bufpos, '=');
				if(p) {
					*p++ = 0;
					for(i = 0; i < VORBIS_COMMENT_TYPES; i++) {
						if(pds_stricmp(bufpos, vorbiscommenttypes[i]) == 0) {
							unsigned int len = comment_size - (p - bufpos);
							pds_strncpy(id3p, p, len);
							if((*(mpxplay_textconv_funcs->control)) & ID3TEXTCONV_UTF_AUTO)
								len = mpxplay_textconv_funcs->utf8_to_char(id3p, len);	// ???
							len = mpxplay_textconv_funcs->all_to_char(id3p, len, ID3TEXTCONV_UTF8);
							if(len) {
								id3ip[id3index[i]] = id3p;
								id3p += len + 1;
							}
						}
					}
				}
				counted_size += comment_size;
				bufpos += comment_size;
			}
		}
		break;
	} while((++allframecounter) <= (omip->nb_streams * 3));

  err_out_id3:
	fbfs->fseek(fbds, filepos, SEEK_SET);
	ogg_sync_reset(&(omip->oys));
	ogg_reset_streams(omip);

	return id3p;
}

/*static char *vorbiscommenttypes[VORBIS_COMMENT_TYPES]={"title","artist","author","album","date","comment","description","genre","tracknumber"};
static unsigned int id3index[VORBIS_COMMENT_TYPES]={I3I_TITLE,I3I_ARTIST,I3I_ARTIST,I3I_ALBUM,I3I_YEAR,I3I_COMMENT,I3I_COMMENT,I3I_GENRE,I3I_TRACKNUM};

static char *OGG_infile_tag_get(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,char *id3p,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
 struct ogg_demuxer_data_s *omip=miis->private_data;
 struct ogg_comment_s *oggcomments;
 struct mpxplay_bitstreambuf_s bs;
 long i,counted_size,comment_size,nb_comments;
 mpxp_filesize_t filepos;
 mpxp_uint8_t *bufpos,*reflib;

 if(!omip->o2w)
  return id3p;

 filepos=fbfs->ftell(fbds);

 fbfs->fseek(fbds,0,SEEK_SET);
 ogg_sync_reset(&(omip->oys));
 ogg_stream_reset(&(omip->oss));

 if(!read_ogg_frame(omip,fbfs,fbds))
  goto err_out_id3;
 if(ogg_stream_packetout(&(omip->oss),&(omip->ops))!=1)
  goto err_out_id3;
 if(!read_ogg_frame(omip,fbfs,fbds))
  goto err_out_id3;
 if(ogg_stream_packetout(&(omip->oss),&(omip->ops))!=1)
  goto err_out_id3;

 mpxplay_bitstream_init(&bs,omip->ops.packet,omip->ops.bytes);
 if(omip->o2w->seekcomment_func)
  if(!omip->o2w->seekcomment_func(&bs))
   goto err_out_id3;

 bufpos=mpxplay_bitstream_getbufpos(&bs);
 omip->metadata_size_cur=mpxplay_bitstream_leftbytes(&bs);

 counted_size=0;
 comment_size=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
 if(comment_size>=omip->metadata_size_cur)
  goto err_out_id3;
 reflib=bufpos;
 bufpos+=comment_size;counted_size+=comment_size; // reference lib

 nb_comments=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
 if(!nb_comments || (nb_comments>(omip->metadata_size_cur/2)))
  goto err_out_id3;

 omip->comments_allocated=nb_comments+1+I3I_MAX+1;

 oggcomments=omip->comment_datas=calloc(omip->comments_allocated,sizeof(*omip->comment_datas));
 if(!oggcomments)
  goto err_out_id3;

 oggcomments->datastr=malloc(comment_size*sizeof(*oggcomments->datastr));
 if(oggcomments->datastr){
  oggcomments->datalen=comment_size;
  pds_memcpy(oggcomments->datastr,reflib,oggcomments->datalen);
  oggcomments++;
 }

 while(nb_comments-- && (counted_size<omip->metadata_size_cur)){
  comment_size=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
  if(comment_size){
   char *p;
   if(comment_size>(omip->metadata_size_cur-counted_size))
    break;
   oggcomments->i3i_index=-1;
   p=pds_strchr(bufpos,'=');
   if(p){
    oggcomments->typelen=p-bufpos;
    *p++=0;
    oggcomments->typestr=malloc((oggcomments->typelen+1)*sizeof(*oggcomments->typestr));
    if(oggcomments->typestr)
     pds_strcpy(oggcomments->typestr,bufpos);
    oggcomments->datalen=comment_size-oggcomments->typelen-1;
    oggcomments->datastr=malloc(oggcomments->datalen*sizeof(*oggcomments->datastr));
    if(oggcomments->datastr)
     pds_memcpy(oggcomments->datastr,p,oggcomments->datalen);

    for(i=0;i<VORBIS_COMMENT_TYPES;i++){
     if(pds_stricmp(bufpos,vorbiscommenttypes[i])==0){
      unsigned int len=oggcomments->datalen;
      pds_strncpy(id3p,p,len);
      if((*(mpxplay_textconv_funcs->control))&ID3TEXTCONV_UTF_AUTO)
       len=mpxplay_textconv_funcs->utf8_to_char(id3p,len);  // ???
      len=mpxplay_textconv_funcs->all_to_char(id3p,len,ID3TEXTCONV_UTF8);
      if(len){
       oggcomments->i3i_index=id3index[i];
       id3ip[id3index[i]]=id3p;
       id3p+=len+1;
      }
     }
    }
   }else{
    oggcomments->datalen=comment_size;
    oggcomments->datastr=malloc(comment_size*sizeof(*oggcomments->datastr));
    if(oggcomments->datastr)
     pds_memcpy(oggcomments->datastr,bufpos,oggcomments->datalen);
   }
   counted_size+=comment_size;
   bufpos+=comment_size;
   oggcomments++;
   omip->comments_loaded++;
  }
 }
 omip->comments_totallen=counted_size;

err_out_id3:
 fbfs->fseek(fbds,filepos,SEEK_SET);
 ogg_sync_reset(&(omip->oys));
 ogg_stream_reset(&(omip->oss));

 return id3p;
}*/

/*static int ogg_load_vorbiscomment(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,char *id3p,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
 struct ogg_demuxer_data_s *omip=miis->private_data;
 struct ogg_comment_s *oggcomments;
 struct mpxplay_bitstreambuf_s bs;
 long i,counted_size,comment_size,nb_comments;
 mpxp_filesize_t filepos;
 mpxp_uint8_t *bufpos,*reflib;

 if(!omip->o2w)
  return id3p;

 filepos=fbfs->ftell(fbds);

 fbfs->fseek(fbds,0,SEEK_SET);
 ogg_sync_reset(&(omip->oys));
 ogg_stream_reset(&(omip->oss));

 if(!read_ogg_frame(omip,fbfs,fbds))
  goto err_out_id3;
 if(ogg_stream_packetout(&(omip->oss),&(omip->ops))!=1)
  goto err_out_id3;
 if(!read_ogg_frame(omip,fbfs,fbds))
  goto err_out_id3;
 if(ogg_stream_packetout(&(omip->oss),&(omip->ops))!=1)
  goto err_out_id3;

 mpxplay_bitstream_init(&bs,omip->ops.packet,omip->ops.bytes);
 if(omip->o2w->seekcomment_func)
  if(!omip->o2w->seekcomment_func(&bs))
   goto err_out_id3;

 bufpos=mpxplay_bitstream_getbufpos(&bs);
 omip->metadata_size_cur=mpxplay_bitstream_leftbytes(&bs);

 counted_size=0;
 comment_size=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
 if(comment_size>=omip->metadata_size_cur)
  goto err_out_id3;
 reflib=bufpos;
 bufpos+=comment_size;counted_size+=comment_size; // reference lib

 nb_comments=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
 if(!nb_comments || (nb_comments>(omip->metadata_size_cur/2)))
  goto err_out_id3;

 omip->comments_allocated=nb_comments+1+I3I_MAX+1;

 oggcomments=omip->comment_datas=calloc(omip->comments_allocated,sizeof(*omip->comment_datas));
 if(!oggcomments)
  goto err_out_id3;

 oggcomments->datastr=malloc(comment_size*sizeof(*oggcomments->datastr));
 if(oggcomments->datastr){
  oggcomments->datalen=comment_size;
  pds_memcpy(oggcomments->datastr,reflib,oggcomments->datalen);
  oggcomments++;
 }

 while(nb_comments-- && (counted_size<omip->metadata_size_cur)){
  comment_size=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
  if(comment_size){
   char *p;
   if(comment_size>(omip->metadata_size_cur-counted_size))
    break;
   oggcomments->i3i_index=-1;
   p=pds_strchr(bufpos,'=');
   if(p){
    oggcomments->typelen=p-bufpos;
    *p++=0;
    oggcomments->typestr=malloc((oggcomments->typelen+1)*sizeof(*oggcomments->typestr));
    if(oggcomments->typestr)
     pds_strcpy(oggcomments->typestr,bufpos);
    oggcomments->datalen=comment_size-oggcomments->typelen-1;
    oggcomments->datastr=malloc(oggcomments->datalen*sizeof(*oggcomments->datastr));
    if(oggcomments->datastr)
     pds_memcpy(oggcomments->datastr,p,oggcomments->datalen);

   }else{
    oggcomments->datalen=comment_size;
    oggcomments->datastr=malloc(comment_size*sizeof(*oggcomments->datastr));
    if(oggcomments->datastr)
     pds_memcpy(oggcomments->datastr,bufpos,oggcomments->datalen);
   }
   counted_size+=comment_size;
   bufpos+=comment_size;
   oggcomments++;
   omip->comments_loaded++;
  }
 }
 omip->comments_totallen=counted_size;

err_out_id3:
 fbfs->fseek(fbds,filepos,SEEK_SET);
 ogg_sync_reset(&(omip->oys));
 ogg_stream_reset(&(omip->oss));

 return id3p;
}*/

/*static int OGG_infile_tag_put(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
 struct ogg_demuxer_data_s *omip=miis->private_data;
 struct ogg_comment_s *oggcomments;
 unsigned int i,c;
 //struct mpxplay_bitstreambuf_s bs;
 mpxp_filesize_t filepos;

 if(!omip->o2w || !omip->comment_datas)
  return MPXPLAY_ERROR_INFILE_WRITETAG_UNKNOWN;

 for(i=0;i<I3I_MAX;i++){
  oggcomments=omip->comment_datas;
  for(c=0;c<omip->comments_loaded;c++){
   for(i=0;i<VORBIS_COMMENT_TYPES;i++){
    if(pds_stricmp(oggcomments->typestr,vorbiscommenttypes[i])==0){


    }
   }
  }

 filepos=fbfs->ftell(fbds);

 fbfs->fseek(fbds,0,SEEK_SET);
 ogg_sync_reset(&(omip->oys));
 ogg_stream_reset(&(omip->oss));

err_out_id3:
 fbfs->fseek(fbds,filepos,SEEK_SET);
 ogg_sync_reset(&(omip->oys));
 ogg_stream_reset(&(omip->oss));

 return MPXPLAY_ERROR_INFILE_OK;
}*/

struct mpxplay_infile_func_s IN_OGG_funcs = {
	0,
	NULL,
	NULL,
	&OGG_infile_detect,
	&OGG_infile_check,
	&OGG_infile_open,
	&OGG_infile_close,
	&OGG_infile_decode,
	&OGG_infile_fseek,
	&OGG_infile_clearbuffs,
	&OGG_infile_tag_get,
	NULL,						//&OGG_infile_tag_put,
	NULL,
	{"OGG", "OGA", "OGV", "SPX", NULL}
};

#endif							// MPXPLAY_LINK_INFILE_OGG
