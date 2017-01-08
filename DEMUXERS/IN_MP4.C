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
//function: MP4 demuxing
//requires the mp4ff\mp4ff.lib and mp4ff.h files

//#define MPXPLAY_USE_DEBUGF 1
//#define MPXPLAY_USE_DEBUGMSG
#define MPXPLAY_DEBUG_OUTPUT NULL

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_MP4

#include "mp4ff\mp4ff.h"

#define MP4_AUDIO_BSBUF_SIZE 32768	// have to be enough

typedef struct mp4_stream_data_s {
	int tracknum;
	int tracktype;
	long max_framesize;
	mpxp_uint8_t *extradata;
	long extradata_size;
} mp4_stream_data_s;

typedef struct mp4_demuxer_data_s {
	mp4ff_t *infile;
	int nb_tracks;
	int track_audio;
	long numSamples;
	long sampleId;

	struct mp4_stream_data_s *streams;
	struct mp4_stream_data_s *stream_audio;	// selected
	struct mp4_stream_data_s *stream_video;	//

	mp4ff_callback_t mp4cb;
} mp4_demuxer_data_s;

static int mp4_get_tracknum(mp4ff_t * infile, unsigned int track_type, struct mpxplay_streampacket_info_s *spi);
static unsigned int mp4_audiotype_to_waveid(unsigned int audiotype, struct mpxplay_audio_decoder_info_s *adi);

static int MP4_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct mp4_demuxer_data_s *mp4i = NULL;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	long pcmdatalen;

	if(!fbfs->fopen_read(fbds, filename, 8192))
		goto err_out_check;

	miis->filesize = fbfs->filelength(fbds);
	if(miis->filesize < 16)
		goto err_out_check;

	mp4i = (struct mp4_demuxer_data_s *)calloc(1, sizeof(struct mp4_demuxer_data_s));
	if(!mp4i)
		goto err_out_check;
	miis->private_data = mp4i;

	mp4i->mp4cb.read = fbfs->fread;
	mp4i->mp4cb.seek = fbfs->fseek;
	mp4i->mp4cb.user_data = fbds;

	mp4i->infile = mp4ff_open_read(&mp4i->mp4cb);
	if(!mp4i->infile)
		goto err_out_check;

	mp4i->nb_tracks = mp4ff_total_tracks(mp4i->infile);
	if(mp4i->nb_tracks < 1)
		goto err_out_check;

	mp4i->track_audio = mp4_get_tracknum(mp4i->infile, TRACK_AUDIO, spi);
	if(mp4i->track_audio < 0)
		goto err_out_check;

	spi->streamtype = MPXPLAY_SPI_STREAMTYPE_AUDIO;
	spi->wave_id = mp4_audiotype_to_waveid(mp4ff_get_audio_type(mp4i->infile, mp4i->track_audio), adi);
	if(!spi->wave_id)
		goto err_out_check;

	funcbit_smp_value_put(mp4i->numSamples, mp4ff_num_samples(mp4i->infile, mp4i->track_audio));
	if(!mp4i->numSamples)
		goto err_out_check;

	adi->freq = mp4ff_get_sample_rate(mp4i->infile, mp4i->track_audio);
	if(!adi->freq)
		goto err_out_check;
	adi->filechannels = adi->outchannels = mp4ff_get_channel_count(mp4i->infile, mp4i->track_audio);
	if(!adi->filechannels)
		goto err_out_check;
	adi->bits = mp4ff_get_sample_size(mp4i->infile, mp4i->track_audio);	// at lossless formats only

	pcmdatalen = mp4ff_get_track_duration(mp4i->infile, mp4i->track_audio);
	miis->timemsec = (long)(1000.0 * (float)pcmdatalen / (float)adi->freq);
	adi->bitrate = (mp4ff_get_avg_bitrate(mp4i->infile, mp4i->track_audio) + 500) / 1000;

	if(spi->wave_id == MPXPLAY_WAVEID_ALAC) {
		miis->longname = "MP4/ALAC";
		adi->bitratetext = malloc(MPXPLAY_ADITEXTSIZE_BITRATE + 8);
		if(adi->bitratetext) {
			sprintf(adi->bitratetext, "%2d/%2.1f%%", adi->bits, 100.0 * (float)miis->filesize / ((float)pcmdatalen * (adi->bits / 8) * adi->filechannels));
		}
	}

	if(adi->infobits & ADI_CNTRLBIT_BITSTREAMOUT) {
		miis->allframes = mp4i->numSamples;
		adi->infobits |= ADI_FLAG_BITSTREAMOUT | ADI_FLAG_BITSTREAMNOFRH;
	}

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_check:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int MP4_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct mpxplay_streampacket_info_s *spi;
	struct mp4_demuxer_data_s *mp4i;
	unsigned int i;
	int bytes, retcode;

	retcode = MP4_infile_check(fbfs, fbds, filename, miis);
	if(retcode != MPXPLAY_ERROR_INFILE_OK)
		return retcode;
	mp4i = miis->private_data;

	mp4i->streams = (struct mp4_stream_data_s *)calloc(mp4i->nb_tracks, sizeof(struct mp4_stream_data_s));
	if(!mp4i->streams)
		goto err_out_open;

	for(i = 0; i < mp4i->nb_tracks; i++) {
		struct mp4_stream_data_s *msd = &mp4i->streams[i];

		msd->tracknum = i;
		msd->max_framesize = mp4ff_read_sample_maxsize(mp4i->infile, i);

		bytes = mp4ff_get_decoder_config_size(mp4i->infile, i);
		if(bytes > 0) {
			msd->extradata = (mpxp_uint8_t *) malloc(bytes + MPXPLAY_SPI_EXTRADATA_PADDING);
			if(!msd->extradata)
				goto err_out_open;
			bytes = mp4ff_get_decoder_config_v2(mp4i->infile, i, msd->extradata, bytes);
			if(bytes < 0)
				goto err_out_open;
			pds_memset(msd->extradata + bytes, 0, MPXPLAY_SPI_EXTRADATA_PADDING);
			msd->extradata_size = bytes;
		}
	}

	mp4i->stream_audio = &mp4i->streams[mp4i->track_audio];
	spi = miis->audio_stream;
	spi->extradata = mp4i->stream_audio->extradata;
	spi->extradata_size = mp4i->stream_audio->extradata_size;
	spi->bs_framesize = mp4i->stream_audio->max_framesize;
	funcbit_enable(spi->flags, (MPXPLAY_SPI_FLAG_NEED_DECODER | MPXPLAY_SPI_FLAG_NEED_PARSING | MPXPLAY_SPI_FLAG_CONTAINER));

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_open:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static void MP4_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct mp4_demuxer_data_s *mp4i = (mp4_demuxer_data_s *) miis->private_data;
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	unsigned int i;

	if(mp4i) {
		if(mp4i->infile)
			mp4ff_close(mp4i->infile);
		if(mp4i->streams) {
			for(i = 0; i < mp4i->nb_tracks; i++)
				if(mp4i->streams[i].extradata)
					free(mp4i->streams[i].extradata);
			free(mp4i->streams);
		}
		if(adi->bitratetext)
			free(adi->bitratetext);
		free(mp4i);
	}
	fbfs->fclose(fbds);
}

static int MP4_infile_demux(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct mp4_demuxer_data_s *mp4i = (mp4_demuxer_data_s *) miis->private_data;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	long bsbytes, readbytes;

	if(!mp4i)
		return MPXPLAY_ERROR_INFILE_EOF;

	mp4i->mp4cb.user_data = fbds;	// we allways update it here

	if(mp4i->sampleId >= mp4i->numSamples)
		return MPXPLAY_ERROR_INFILE_EOF;

	bsbytes = mp4ff_set_sample_position(mp4i->infile, mp4i->track_audio, mp4i->sampleId);
	if(bsbytes < 0) {			// failed seek
		if(bsbytes == MPXPLAY_ERROR_MPXINBUF_SEEK_EOF) {	// seekpoint is out of file
			funcbit_smp_value_increment(mp4i->sampleId);	// seek to next frame
		}						// else seekpoint is out of mpxbuffer
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mp4 seek error");
		return MPXPLAY_ERROR_INFILE_RESYNC;	//
	}

	bsbytes = mp4ff_read_sample_getsize(mp4i->infile, mp4i->track_audio, mp4i->sampleId);
	if((bsbytes < 1) || (bsbytes > mp4i->stream_audio->max_framesize)) {
		funcbit_smp_value_increment(mp4i->sampleId);
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mp4 sync error");
		return MPXPLAY_ERROR_INFILE_SYNC_IN;
	}

	readbytes = mp4ff_read_sample_v2(mp4i->infile, mp4i->track_audio, mp4i->sampleId, spi->bitstreambuf);
	if(readbytes < bsbytes) {
		mp4ff_set_sample_position(mp4i->infile, mp4i->track_audio, mp4i->sampleId);	// rewind datapos in buffer if we cannot read enough bytes
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mp4 read error");
		return MPXPLAY_ERROR_INFILE_NODATA;
	}

	spi->bs_leftbytes = readbytes;

	funcbit_smp_value_increment(mp4i->sampleId);

	return MPXPLAY_ERROR_INFILE_OK;
}

static long MP4_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newmpxframenum)
{
	struct mp4_demuxer_data_s *mp4i = (mp4_demuxer_data_s *) miis->private_data;

	if(mp4i) {
		mp4i->mp4cb.user_data = fbds;	// we allways update it here
		funcbit_smp_value_put(mp4i->sampleId, (long)((float)mp4i->numSamples * (float)newmpxframenum / (float)miis->allframes));

		if(mp4i->sampleId < mp4i->numSamples) {
			if(mp4ff_set_sample_position(mp4i->infile, mp4i->track_audio, mp4i->sampleId) >= 0)
				return newmpxframenum;
		}
	}

	return MPXPLAY_ERROR_INFILE_EOF;
}

#define MP4_COMMENT_TYPES 8

static char *MP4_infile_tag_get(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, char *id3p,
								struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	static unsigned int mp4atoms[MP4_COMMENT_TYPES] = { ATOM_TITLE, ATOM_ARTIST, ATOM_ALBUM, ATOM_DATE, ATOM_COMMENT, ATOM_GENRE2, ATOM_GENRE1, ATOM_TRACK };
	static unsigned int id3index[MP4_COMMENT_TYPES] = { I3I_TITLE, I3I_ARTIST, I3I_ALBUM, I3I_YEAR, I3I_COMMENT, I3I_GENRE, I3I_GENRE, I3I_TRACKNUM };
	struct mp4_demuxer_data_s *mp4i = (mp4_demuxer_data_s *) miis->private_data;

	if(mp4i) {
		unsigned int i;
		for(i = 0; i < MP4_COMMENT_TYPES; i++) {
			if(!id3ip[id3index[i]]) {	// genre1 OR genre2
				char *tag = mp4ff_meta_search_by_atom(mp4i->infile, mp4atoms[i]);
				if(tag) {
					unsigned int len;
					len = pds_strcpy(id3p, tag);
					if((*(mpxplay_textconv_funcs->control)) & ID3TEXTCONV_UTF_AUTO)
						len = mpxplay_textconv_funcs->utf8_to_char(id3p, len);
					len = mpxplay_textconv_funcs->all_to_char(id3p, len, ID3TEXTCONV_UTF8);
					if(len) {
						id3ip[id3index[i]] = id3p;
						id3p += len + 1;
					}
				}
			}
		}
	}

	return id3p;
}

static int mp4_get_tracknum(mp4ff_t * infile, unsigned int track_type, struct mpxplay_streampacket_info_s *spi)
{
	int numTracks, tracknum = -1;
	unsigned int i, tracktypecount = 0;

	numTracks = mp4ff_total_tracks(infile);
	if(numTracks <= 0)
		return tracknum;

	for(i = 0; i < numTracks; i++) {
		if(mp4ff_get_track_type(infile, i) == track_type) {
			if(tracktypecount <= spi->stream_select)
				tracknum = i;
			tracktypecount++;
		}
	}

	spi->nb_streams = tracktypecount;

	return tracknum;
}

static unsigned int mp4_audiotype_to_waveid(unsigned int audiotype, struct mpxplay_audio_decoder_info_s *adi)
{
	unsigned int waveid = MPXPLAY_WAVEID_UNKNOWN;

	if(MP4_IS_AAC_AUDIO_TYPE(audiotype))
		waveid = MPXPLAY_WAVEID_AAC;
	else if(MP4_IS_MP3_AUDIO_TYPE(audiotype))
		waveid = MPXPLAY_WAVEID_MP3;
	else {
		switch (audiotype) {	// private, unofficial types
		case MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE:
			waveid = MPXPLAY_WAVEID_PCM_SLE;
			adi->bits = 16;
			break;
		case MP4_VORBIS_AUDIO_TYPE:
			waveid = MPXPLAY_WAVEID_VORBIS;
			break;
		case MP4_AC3_AUDIO_TYPE:
			waveid = MPXPLAY_WAVEID_AC3;
			break;
		case MP4_ALAC_AUDIO_TYPE:
			waveid = MPXPLAY_WAVEID_ALAC;
			break;
		}
	}

	return waveid;
}

struct mpxplay_infile_func_s IN_MP4_funcs = {
	0,
	NULL,
	NULL,
	&MP4_infile_check,
	&MP4_infile_check,
	&MP4_infile_open,
	&MP4_infile_close,
	&MP4_infile_demux,
	&MP4_infile_fseek,
	NULL,
	&MP4_infile_tag_get,
	NULL,
	NULL,
	{"MP4", "M4A", "M4B", "M4V", NULL}
};

#endif							// MPXPLAY_LINK_INFILE_MP4
