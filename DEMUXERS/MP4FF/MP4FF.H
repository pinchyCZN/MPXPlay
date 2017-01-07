/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: mp4ff.h,v 1.xx 2005/01/12 00:00:00 PDSoft Exp $
**/

#ifndef MP4FF_H
#define MP4FF_H

#include <inttypes.h>

// track types
#define MAX_TRACKS 1024
#define TRACK_UNKNOWN 0
#define TRACK_AUDIO   1
#define TRACK_VIDEO   2
#define TRACK_SYSTEM  3

// MP4 Audio track types
#define MP4_INVALID_AUDIO_TYPE		   0x00
#define MP4_MPEG1_AUDIO_TYPE		   0x6B
#define MP4_MPEG2_AUDIO_TYPE		   0x69
#define MP4_MP3_AUDIO_TYPE		   MP4_MPEG2_AUDIO_TYPE
#define MP4_MPEG2_AAC_MAIN_AUDIO_TYPE	   0x66
#define MP4_MPEG2_AAC_LC_AUDIO_TYPE	   0x67
#define MP4_MPEG2_AAC_SSR_AUDIO_TYPE	   0x68
#define MP4_MPEG2_AAC_AUDIO_TYPE	   MP4_MPEG2_AAC_MAIN_AUDIO_TYPE
#define MP4_MPEG4_AUDIO_TYPE		   0x40
#define MP4_PRIVATE_AUDIO_TYPE		   0xC0
#define MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE 0xE0	/* a private definition */
#define MP4_VORBIS_AUDIO_TYPE		   0xE1	/* a private definition */
#define MP4_AC3_AUDIO_TYPE		   0xE2	/* a private definition */
#define MP4_PCM16_BIG_ENDIAN_AUDIO_TYPE    0xE6 /* a private definition */
#define MP4_ALAC_AUDIO_TYPE                0xFE

#define MP4_IS_MPEG2_AAC_AUDIO_TYPE(type) \
	(((type) >= MP4_MPEG2_AAC_MAIN_AUDIO_TYPE \
		&& (type) <= MP4_MPEG2_AAC_SSR_AUDIO_TYPE))

#define MP4_IS_AAC_AUDIO_TYPE(type) \
	(MP4_IS_MPEG2_AAC_AUDIO_TYPE(type) \
	|| (type) == MP4_MPEG4_AUDIO_TYPE)

#define MP4_IS_MP3_AUDIO_TYPE(type) \
	((type) == MP4_MPEG1_AUDIO_TYPE || (type) == MP4_MPEG2_AUDIO_TYPE)

//--------- atoms ------------------------------------

#define SUBATOMIC 128

/* atoms without subatoms */
#define ATOM_FTYP 129
#define ATOM_MDAT 130
#define ATOM_MVHD 131
#define ATOM_TKHD 132
#define ATOM_TREF 133
#define ATOM_MDHD 134
#define ATOM_VMHD 135
#define ATOM_SMHD 136
#define ATOM_HMHD 137
#define ATOM_STSD 138
#define ATOM_STTS 139
#define ATOM_STSZ 140
#define ATOM_STZ2 141
#define ATOM_STCO 142
#define ATOM_STSC 143
#define ATOM_MP4A 144
#define ATOM_MP4V 145
#define ATOM_MP4S 146
#define ATOM_ESDS 147
#define ATOM_META 148 /* iTunes Metadata box */
#define ATOM_NAME 149 /* iTunes Metadata name box */
#define ATOM_DATA 150 /* iTunes Metadata data box */
#define ATOM_CTTS 151
#define ATOM_FRMA 152
#define ATOM_IVIV 153
#define ATOM_PRIV 154
#define ATOM_USER 155
#define ATOM_KEY  156

#define ATOM_ALAC 200

#define ATOM_UNKNOWN 255
#define ATOM_FREE ATOM_UNKNOWN
#define ATOM_SKIP ATOM_UNKNOWN

/* atoms with subatoms */
#define ATOM_MOOV 1
#define ATOM_TRAK 2
#define ATOM_EDTS 3
#define ATOM_MDIA 4
#define ATOM_MINF 5
#define ATOM_STBL 6
#define ATOM_UDTA 7
#define ATOM_ILST 8 /* iTunes Metadata list */
#define ATOM_TITLE 9
#define ATOM_ARTIST 10
#define ATOM_WRITER 11
#define ATOM_ALBUM 12
#define ATOM_DATE 13
#define ATOM_TOOL 14
#define ATOM_COMMENT 15
#define ATOM_GENRE1 16
#define ATOM_TRACK 17
#define ATOM_DISC 18
#define ATOM_COMPILATION 19
#define ATOM_GENRE2 20
#define ATOM_TEMPO 21
#define ATOM_COVER 22
#define ATOM_DRMS 23
#define ATOM_SINF 24
#define ATOM_SCHI 25

typedef struct
{
 unsigned long (*read)(void *user_data, void *buffer, unsigned long length);
 int32_t (*write)(void *udata, void *buffer, uint32_t length);
 long (*seek)(void *user_data, long position, int whence);
 uint32_t (*truncate)(void *user_data);
 void *user_data;
}mp4ff_callback_t;

/* metadata tag structure */
typedef struct
{
 unsigned int atom;
 char *value;
}mp4ff_tag_t;

/* metadata list structure */
typedef struct
{
 mp4ff_tag_t *tags;
 uint32_t storage;
 uint32_t count;
}mp4ff_metadata_t;


typedef struct
{
 int32_t type;
 int32_t channelCount;
 int32_t sampleSize;
 uint16_t sampleRate;
 int32_t audioType;

 /* stsd */
 int32_t stsd_entry_count;

 /* stsz */
 int32_t stsz_max_sample_size; // stsz_sample_size or max in stsz_table[]
 int32_t stsz_sample_size;
 int32_t stsz_sample_count;
 int32_t *stsz_table;

 /* stts */
 int32_t stts_entry_count;
 int32_t *stts_sample_count;
 int32_t *stts_sample_delta;

 /* stsc */
 int32_t stsc_entry_count;
 int32_t *stsc_first_chunk;
 int32_t *stsc_samples_per_chunk;
 int32_t *stsc_sample_desc_index;

 /* stsc */
 int32_t stco_entry_count;
 int32_t *stco_chunk_offset;

 /* ctts */
 int32_t ctts_entry_count;
 int32_t *ctts_sample_count;
 int32_t *ctts_sample_offset;

 /* esde */
 uint8_t *decoderConfig;
 int32_t decoderConfigLen;

 uint32_t maxBitrate;
 uint32_t avgBitrate;

 uint32_t timeScale;
 uint64_t duration;

}mp4ff_track_t;

/* mp4 main file structure */
typedef struct
{
 mp4ff_callback_t *stream;
 int64_t current_position;

 int32_t  moov_read;
 uint64_t moov_offset;
 uint64_t moov_size;
 uint8_t  last_atom;
 uint64_t file_size;

 /* incremental track index while reading the file */
 int32_t total_tracks;

 /* track data */
 mp4ff_track_t *lasttrack;
 mp4ff_track_t *track[MAX_TRACKS];

 /* metadata */
 mp4ff_metadata_t tags;
}mp4ff_t;

// API
mp4ff_t *mp4ff_open_read(mp4ff_callback_t *f);
   void mp4ff_close(mp4ff_t *f);
int32_t mp4ff_get_sample_duration(const mp4ff_t *f, const int32_t track, const int32_t sample);

int32_t mp4ff_read_sample_maxsize(const mp4ff_t *f, const int32_t track); // max size of samples
int32_t mp4ff_read_sample_getsize(mp4ff_t *f, const int32_t track, const int32_t sample);//returns 0 on error, buffer size needed for mp4ff_read_sample_v2() on success
int32_t mp4ff_read_sample_v2(mp4ff_t *f, const int32_t track, const int32_t sample,uint8_t *buffer);//returns 0 on error, number of bytes read on success, use mp4ff_read_sample_getsize() to check buffer size needed

int32_t mp4ff_set_sample_position(mp4ff_t *f, const int32_t track, const int32_t sample);

int32_t mp4ff_get_decoder_config_size(const mp4ff_t *f, const int32_t track);
int32_t mp4ff_get_decoder_config_v2(const mp4ff_t *f, const int32_t track,uint8_t *ppBuf, uint32_t pBufSize);
int32_t mp4ff_get_track_type(const mp4ff_t *f, const int32_t track);
int32_t mp4ff_total_tracks(const mp4ff_t *f);
int32_t mp4ff_num_samples(const mp4ff_t *f, const int32_t track);

uint32_t mp4ff_get_avg_bitrate(const mp4ff_t *f, const int32_t track);
int64_t  mp4ff_get_track_duration(const mp4ff_t *f, const int32_t track); //returns (-1) if unknown
uint32_t mp4ff_get_sample_rate(const mp4ff_t *f, const int32_t track);
uint32_t mp4ff_get_channel_count(const mp4ff_t * f,const int32_t track);
uint32_t mp4ff_get_audio_type(const mp4ff_t * f,const int32_t track);
uint32_t mp4ff_get_sample_size(const mp4ff_t * f,const int32_t track);

char *mp4ff_meta_search_by_atom(mp4ff_t *f,uint32_t atom);

// internal
// mp4util.c
int32_t  mp4ff_read_data(mp4ff_t *f, int8_t *data, uint32_t size);
uint64_t mp4ff_read_int64(mp4ff_t *f);
uint32_t mp4ff_read_int32(mp4ff_t *f);
uint32_t mp4ff_read_int24(mp4ff_t *f);
uint16_t mp4ff_read_int16(mp4ff_t *f);
uint8_t  mp4ff_read_char(mp4ff_t *f);
uint32_t mp4ff_read_mp4_descr_length(mp4ff_t *f);
int64_t  mp4ff_position(const mp4ff_t *f);
int32_t  mp4ff_set_position(mp4ff_t *f, const int64_t position);
char    *mp4ff_read_string(mp4ff_t * f,uint32_t length);

// mp4atom.c
uint64_t mp4ff_atom_read_header(mp4ff_t *f, uint8_t *atom_type, uint8_t *header_size);
int32_t  mp4ff_atom_read(mp4ff_t *f, const int32_t size, const uint8_t atom_type);

// mp4sample.c
int32_t  mp4ff_sample_to_offset(const mp4ff_t *f, const int32_t track, const int32_t sample);
int32_t  mp4ff_audio_frame_size(const mp4ff_t *f, const int32_t track, const int32_t sample);

//metadata.c
int32_t  mp4ff_parse_metadata(mp4ff_t *f, const int32_t size);
void     mp4ff_tag_delete(mp4ff_metadata_t *tags);

#endif
