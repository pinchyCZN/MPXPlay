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
//function: WAV, W64 and AIF file reading/demuxing

//#define MPXPLAY_USE_DEBUGF 1
//#define MPXPLAY_DEBUG_OUTPUT stdout

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_WAV

#include "newfunc\newfunc.h"

#define WAVDEMUXER_FORMATTYPE_WAVE64   1
#define WAVDEMUXER_FORMATTYPE_AIFF     2
#define WAVDEMUXER_FORMATTYPE_AIFCVER1 0xA2805140

typedef struct wav_demuxer_data_s {
	unsigned int format_type;
	mpxp_uint64_t filedatabegin;
	mpxp_uint64_t filedatalen;
	mpxp_uint8_t *extradata;
	unsigned long extradata_size;
} wav_demuxer_data_s;

static mpxp_int64_t wav_chunk_search(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, mpxp_uint32_t tagname, struct wav_demuxer_data_s *wavi);

static int WAV_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct wav_demuxer_data_s *wavi;
	mpxp_uint32_t tagname;
	mpxp_int64_t taglen, indatalen, maxdatalen;

	if(!fbfs->fopen_read(fbds, filename, 0))
		return MPXPLAY_ERROR_INFILE_FILEOPEN;

	miis->filesize = fbfs->filelength(fbds);
	if(miis->filesize <= 44)	// smallest wav header (with no data)
		goto err_out_opn;

	wavi = calloc(1, sizeof(struct wav_demuxer_data_s));
	if(!wavi)
		goto err_out_opn;
	miis->private_data = wavi;

	tagname = fbfs->get_le32(fbds);
	if(tagname == PDS_GET4C_LE32('R', 'I', 'F', 'F')) {
		fbfs->get_le32(fbds);	// skip riff size
	} else if(tagname == PDS_GET4C_LE32('r', 'i', 'f', 'f')) {
		if(fbfs->fseek(fbds, 12 + 8, SEEK_CUR) < 0)	// skip riff guid + riff size
			goto err_out_opn;
		wavi->format_type = WAVDEMUXER_FORMATTYPE_WAVE64;
	} else
		goto err_out_opn;

	tagname = fbfs->get_le32(fbds);
	if(tagname == PDS_GET4C_LE32('R', 'M', 'P', '3')) {
		spi->streamtype = MPXPLAY_SPI_STREAMTYPE_AUDIO;
		spi->wave_id = MPXPLAY_WAVEID_MP3;
	} else {
		if((wavi->format_type == WAVDEMUXER_FORMATTYPE_WAVE64) && (tagname == PDS_GET4C_LE32('w', 'a', 'v', 'e'))) {
			if(fbfs->fseek(fbds, 12, SEEK_CUR) < 0)	// skip wave guid
				goto err_out_opn;
		} else if(tagname != PDS_GET4C_LE32('W', 'A', 'V', 'E'))
			goto err_out_opn;

		taglen = wav_chunk_search(fbfs, fbds, PDS_GET4C_LE32('f', 'm', 't', ' '), wavi);	// search for 'fmt' chunk
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "tl:%d", (long)taglen);
		if((taglen < 14) || (taglen > miis->filesize))	// invalid fmt chunk size
			goto err_out_opn;

		spi->streamtype = MPXPLAY_SPI_STREAMTYPE_AUDIO;
		spi->wave_id = fbfs->get_le16(fbds);
		adi->filechannels = fbfs->get_le16(fbds);
		adi->outchannels = adi->filechannels;
		adi->freq = fbfs->get_le32(fbds);
		adi->bitrate = (fbfs->get_le32(fbds) * 8 + 500) / 1000;
		spi->block_align = fbfs->get_le16(fbds);
		if(taglen == 14)		// plain vanilla WAVEFORMAT
			adi->bits = 8;
		else
			adi->bits = fbfs->get_le16(fbds);

		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "wi:%4.4X c:%d f:%d a:%d b:%d tl:%d", spi->wave_id, adi->filechannels, adi->freq, spi->block_align, adi->bits, (long)taglen);

		if(!spi->wave_id || !adi->filechannels || (adi->filechannels > PCM_MAX_CHANNELS))
			goto err_out_opn;
		if(!adi->freq || !spi->block_align)
			goto err_out_opn;

		if(taglen >= 18) {		// WAVEFORMATEX
			wavi->extradata_size = fbfs->get_le16(fbds);
			if(wavi->extradata_size) {
				if(wavi->extradata_size > (taglen - 18))
					wavi->extradata_size = taglen - 18;
				wavi->extradata = malloc(wavi->extradata_size + MPXPLAY_SPI_EXTRADATA_PADDING);
				if(!wavi->extradata)
					goto err_out_opn;
				if(fbfs->fread(fbds, wavi->extradata, wavi->extradata_size) != wavi->extradata_size)
					goto err_out_opn;
				pds_memset(wavi->extradata + wavi->extradata_size, 0, MPXPLAY_SPI_EXTRADATA_PADDING);
				spi->extradata = wavi->extradata;
				spi->extradata_size = wavi->extradata_size;
			}
			if((taglen - wavi->extradata_size - 18) > 0)	// skip garbage at the end
				fbfs->fseek(fbds, (taglen - wavi->extradata_size - 18), SEEK_CUR);
		} else if(taglen > 16)
			fbfs->fseek(fbds, (taglen - 16), SEEK_CUR);
	}

	indatalen = wav_chunk_search(fbfs, fbds, PDS_GET4C_LE32('d', 'a', 't', 'a'), wavi);
	if(indatalen < 0) {
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "data chunk not found!");
		goto err_out_opn;
	}

	wavi->filedatabegin = fbfs->ftell(fbds);
	maxdatalen = miis->filesize - wavi->filedatabegin;	// maxdatalen=filesize-headersize
	if(!indatalen || (indatalen > maxdatalen))	// indatalen have to be less than maxdatalen
		indatalen = maxdatalen;
	wavi->filedatalen = indatalen;

	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "databegin:%d datalen:%d", wavi->filedatabegin, wavi->filedatalen);

	funcbit_enable(spi->flags, (MPXPLAY_SPI_FLAG_NEED_DECODER | MPXPLAY_SPI_FLAG_CONTAINER));

	if((spi->wave_id == MPXPLAY_WAVEID_PCM_SLE) || (spi->wave_id == MPXPLAY_WAVEID_PCM_FLOAT)) {
		if(spi->wave_id == MPXPLAY_WAVEID_PCM_FLOAT) {
			if(adi->bits == 64)
				spi->wave_id = MPXPLAY_WAVEID_PCM_F64LE;
			//adi->bits=1;
			//funcbit_enable(adi->infobits,ADI_FLAG_FLOATOUT);
		} else if(!adi->bits || (adi->bits > PCM_MAX_BITS))
			goto err_out_opn;
		if(indatalen < spi->block_align)	// we need 1 sample at least
			goto err_out_opn;
		miis->timemsec = (float)indatalen *1000.0 / (float)spi->block_align / (float)adi->freq;
		spi->bs_framesize = mpxplay_infile_get_samplenum_per_frame(adi->freq) * spi->block_align;
		adi->bitrate = 0;
	} else
		funcbit_enable(spi->flags, MPXPLAY_SPI_FLAG_NEED_PARSING);

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_opn:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static mpxp_int64_t wav_chunk_search(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, mpxp_uint32_t search_id, struct wav_demuxer_data_s *wavi)
{
	unsigned int retry = 20;
	mpxp_int64_t taglen = (mpxp_int64_t) - 1;
	do {
		mpxp_uint32_t tagname = fbfs->get_le32(fbds);
		if(fbfs->eof(fbds))
			break;
		if(wavi->format_type == WAVDEMUXER_FORMATTYPE_WAVE64) {
			if(fbfs->fseek(fbds, 12, SEEK_CUR) < 0)
				return -1;
			taglen = fbfs->get_le64(fbds);
			if(taglen < 24)
				return -1;
			taglen -= 24;
		} else
			taglen = fbfs->get_le32(fbds);
		if(fbfs->eof(fbds))
			return -1;
		if(tagname == search_id)
			break;
		if(taglen != PDS_GET4C_LE32('f', 'a', 'c', 't'))	// ??? old Wavelab bug?
			if(fbfs->fseek(fbds, taglen, SEEK_CUR) < 0)
				break;
	} while(--retry);
	return taglen;
}

static void WAV_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct wav_demuxer_data_s *wavi = miis->private_data;
	if(wavi) {
		if(wavi->extradata)
			free(wavi->extradata);
		free(wavi);
	}
	fbfs->fclose(fbds);
}

static int WAV_infile_demux(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct wav_demuxer_data_s *wavi = miis->private_data;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	long filepos, fileend, readsize;

	filepos = fbfs->ftell(fbds);
	fileend = wavi->filedatabegin + wavi->filedatalen;
	readsize = spi->bs_readsize;

	if((filepos + readsize) > fileend)
		readsize = fileend - filepos;

	if(readsize <= 0)
		return MPXPLAY_ERROR_INFILE_EOF;

	spi->bs_leftbytes = fbfs->fread(fbds, spi->bitstreambuf, readsize);
	if(!spi->bs_leftbytes)
		return MPXPLAY_ERROR_INFILE_NODATA;

	//mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"demux fp:%8d fpe:%8d db:%d dl:%d fe:%d brs:%d rs:%d",
	// filepos,filepos+readsize,wavi->filedatabegin,wavi->filedatalen,fileend,spi->bs_readsize,readsize);

	return MPXPLAY_ERROR_INFILE_OK;
}

static long WAV_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newmpxframenum)
{
	struct wav_demuxer_data_s *wavi = miis->private_data;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	long newfilepos;

	newfilepos = newmpxframenum * spi->bs_readsize;	// ??? if not pcm, decoder should set bs_framesize/bs_readsize

	if(newfilepos >= wavi->filedatalen)
		return MPXPLAY_ERROR_INFILE_EOF;

	newfilepos += wavi->filedatabegin;

	if(fbfs->fseek(fbds, newfilepos, SEEK_SET) < 0)
		return MPXPLAY_ERROR_INFILE_EOF;

	//mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"fseek fp:%8d",newfilepos);

	return newmpxframenum;
}

struct mpxplay_infile_func_s IN_WAV_funcs = {
	0,
	NULL,
	NULL,
	&WAV_infile_open,
	&WAV_infile_open,
	&WAV_infile_open,
	&WAV_infile_close,
	&WAV_infile_demux,
	&WAV_infile_fseek,
	NULL,
	NULL,
	NULL,
	NULL,
	{"WAV", "W64", "CDW", NULL}
};

//------------------------------------------------------------------------


static int AIF_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct wav_demuxer_data_s *wavi;
	unsigned long size, num_frames, tag;
	mpxp_filesize_t filesize, offset, indatalen, maxdatalen;
	mpxp_float80_t sample_rate;

	if(!fbfs->fopen_read(fbds, filename, 0))
		return MPXPLAY_ERROR_INFILE_FILEOPEN;

	if(fbfs->get_le32(fbds) != PDS_GET4C_LE32('F', 'O', 'R', 'M'))
		goto err_out_opn;
	fbfs->get_be32(fbds);		// form size

	wavi = calloc(1, sizeof(struct wav_demuxer_data_s));
	if(!wavi)
		goto err_out_opn;
	miis->private_data = wavi;

	filesize = miis->filesize = fbfs->filelength(fbds);
	if(filesize <= 12)
		goto err_out_opn;

	switch (fbfs->get_le32(fbds)) {
	case PDS_GET4C_LE32('A', 'I', 'F', 'F'):
		wavi->format_type = WAVDEMUXER_FORMATTYPE_AIFF;
		break;
	case PDS_GET4C_LE32('A', 'I', 'F', 'C'):
		wavi->format_type = WAVDEMUXER_FORMATTYPE_AIFCVER1;
		break;
	default:
		goto err_out_opn;
	}

	do {
		tag = fbfs->get_le32(fbds);
		size = fbfs->get_be32(fbds);
		if(size > 0x7fffffff)
			goto err_out_opn;
		filesize -= size + 8;
		if(filesize <= 0)
			goto err_out_opn;
		offset = fbfs->ftell(fbds);

		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "tag:%4.4s size:%d offset:%d", (char *)&tag, size, offset);

		switch (tag) {
		case PDS_GET4C_LE32('C', 'O', 'M', 'M'):
			adi->filechannels = adi->outchannels = fbfs->get_be16(fbds);
			if(adi->filechannels > PCM_MAX_CHANNELS)
				goto err_out_opn;
			num_frames = fbfs->get_be32(fbds);
			adi->bits = fbfs->get_be16(fbds);
			fbfs->fread(fbds, (char *)&sample_rate, sizeof(sample_rate));
			pds_mem_reverse((char *)&sample_rate, sizeof(sample_rate));
			adi->freq = (long)pds_float80_get(&sample_rate);
			if(wavi->format_type == WAVDEMUXER_FORMATTYPE_AIFCVER1) {
				switch (fbfs->get_le32(fbds)) {
				case PDS_GET4C_LE32('s', 'o', 'w', 't'):
					if(adi->bits > PCM_MAX_BITS)
						goto err_out_opn;
					spi->wave_id = MPXPLAY_WAVEID_PCM_SLE;
					adi->bytespersample = (adi->bits + 7) / 8;
					break;
				case PDS_GET4C_LE32('f', 'l', '3', '2'):
					spi->wave_id = MPXPLAY_WAVEID_PCM_F32BE;
					adi->bytespersample = sizeof(mpxp_float_t);
					break;
				case PDS_GET4C_LE32('f', 'l', '6', '4'):
					spi->wave_id = MPXPLAY_WAVEID_PCM_F64BE;
					adi->bytespersample = sizeof(mpxp_double_t);
					break;
				default:
					goto err_out_opn;
				}
			} else {
				if(adi->bits > PCM_MAX_BITS)
					goto err_out_opn;
				spi->wave_id = MPXPLAY_WAVEID_PCM_SBE;
				adi->bytespersample = (adi->bits + 7) / 8;
			}
			if(wavi->filedatabegin)
				goto got_sound;
			break;
		case PDS_GET4C_LE32('F', 'V', 'E', 'R'):
			wavi->format_type = fbfs->get_be32(fbds);
			break;
		case PDS_GET4C_LE32('S', 'S', 'N', 'D'):
			wavi->filedatabegin = offset + fbfs->get_be32(fbds) + 8;
			if(adi->freq)
				goto got_sound;
			break;
		}
		if(size & 1)
			size++;
		if(fbfs->fseek(fbds, offset + size, SEEK_SET) < 0)
			goto err_out_opn;
	} while(1);

  got_sound:
	spi->block_align = adi->bytespersample * adi->filechannels;
	indatalen = num_frames * spi->block_align;
	maxdatalen = miis->filesize - wavi->filedatabegin;
	if(!indatalen || (indatalen > maxdatalen))
		indatalen = maxdatalen;
	wavi->filedatalen = indatalen;
	miis->timemsec = (float)indatalen *1000.0 / (float)spi->block_align / (float)adi->freq;
	spi->bs_framesize = mpxplay_infile_get_samplenum_per_frame(adi->freq) * spi->block_align;

	funcbit_enable(spi->flags, (MPXPLAY_SPI_FLAG_NEED_DECODER | MPXPLAY_SPI_FLAG_CONTAINER));

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_opn:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

struct mpxplay_infile_func_s IN_AIF_funcs = {
	0,
	NULL,
	NULL,
	&AIF_infile_open,
	&AIF_infile_open,
	&AIF_infile_open,
	&WAV_infile_close,
	&WAV_infile_demux,
	&WAV_infile_fseek,
	NULL,
	NULL,
	NULL,
	NULL,
	{"AIF", "AIFF", "AFC", "AIFC", NULL}
};

#endif							//MPXPLAY_LINK_INFILE_WAV
