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
** $Id: mp4ff.c,v 1.20 2005/01/12 00:00:00 PDSoft Exp $
**/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mp4ff.h"

static int32_t parse_atoms(mp4ff_t * f);

mp4ff_t *mp4ff_open_read(mp4ff_callback_t * f)
{
	mp4ff_t *ff = malloc(sizeof(mp4ff_t));
	if(!ff)
		return ff;

	memset(ff, 0, sizeof(mp4ff_t));
	ff->stream = f;

	if(parse_atoms(ff) != 0) {
		mp4ff_close(ff);
		return NULL;
	}

	return ff;
}

void mp4ff_close(mp4ff_t * ff)
{
	int32_t i;

	if(ff) {

		for(i = 0; i < ff->total_tracks; i++) {
			if(ff->track[i]) {
				if(ff->track[i]->stsz_table)
					free(ff->track[i]->stsz_table);
				if(ff->track[i]->stts_sample_count)
					free(ff->track[i]->stts_sample_count);
				if(ff->track[i]->stts_sample_delta)
					free(ff->track[i]->stts_sample_delta);
				if(ff->track[i]->stsc_first_chunk)
					free(ff->track[i]->stsc_first_chunk);
				if(ff->track[i]->stsc_samples_per_chunk)
					free(ff->track[i]->stsc_samples_per_chunk);
				if(ff->track[i]->stsc_sample_desc_index)
					free(ff->track[i]->stsc_sample_desc_index);
				if(ff->track[i]->stco_chunk_offset)
					free(ff->track[i]->stco_chunk_offset);
				if(ff->track[i]->decoderConfig)
					free(ff->track[i]->decoderConfig);
				if(ff->track[i]->ctts_sample_count)
					free(ff->track[i]->ctts_sample_count);
				if(ff->track[i]->ctts_sample_offset)
					free(ff->track[i]->ctts_sample_offset);
				free(ff->track[i]);
			}
		}

		mp4ff_tag_delete(&(ff->tags));

		free(ff);
	}
}

static int32_t mp4ff_track_add(mp4ff_t * f)
{
	f->total_tracks++;
	f->track[f->total_tracks - 1] = malloc(sizeof(mp4ff_track_t));
	if(!f->track[f->total_tracks - 1])
		return -1;
	f->lasttrack = f->track[f->total_tracks - 1];

	memset(f->lasttrack, 0, sizeof(mp4ff_track_t));
	return 0;
}

/* parse atoms that are sub atoms of other atoms */
static int32_t parse_sub_atoms(mp4ff_t * f, const uint64_t total_size)
{
	uint64_t size;
	uint8_t atom_type = 0;
	uint64_t counted_size = 0;
	uint8_t header_size = 0;

	while(counted_size < total_size) {
		size = mp4ff_atom_read_header(f, &atom_type, &header_size);
		if(size == 0)
			break;
		counted_size += size;

		if(atom_type == ATOM_TRAK)
			if(mp4ff_track_add(f) != 0)
				return -1;

		if(atom_type < SUBATOMIC)
			parse_sub_atoms(f, size - header_size);
		else
			mp4ff_atom_read(f, (uint32_t) size, atom_type);
	}
	return 0;
}

/* parse root atoms */
static int32_t parse_atoms(mp4ff_t * f)
{
	uint64_t size;
	uint8_t atom_type = 0;
	uint8_t header_size = 0;
	uint8_t firstatom = 1;

	f->file_size = 0;

	do {
		size = mp4ff_atom_read_header(f, &atom_type, &header_size);
		if(size < 0)
			return -1;
		if(firstatom) {
			if(size == 0)
				return -1;
			if(atom_type != ATOM_FTYP)
				return -1;
			firstatom = 0;
		}
		if(size == 0)
			break;

		f->file_size += size;
		f->last_atom = atom_type;

		if(atom_type == ATOM_MDAT && f->moov_read) {
			/* moov atom is before mdat, we can stop reading when mdat is encountered */
			/* file position will stay at beginning of mdat data */
			//break;
		}

		if((atom_type == ATOM_MOOV) && (size > header_size)) {
			f->moov_read = 1;
			f->moov_offset = mp4ff_position(f) - header_size;
			f->moov_size = size;
		}

		if(atom_type < SUBATOMIC)
			parse_sub_atoms(f, size - header_size);
		else
			mp4ff_set_position(f, mp4ff_position(f) + size - header_size);
	} while(1);

	return 0;
}

int32_t mp4ff_get_decoder_config_size(const mp4ff_t * f, const int32_t track)
{
	if(track >= f->total_tracks)
		return -1;

	if((f->track[track]->decoderConfig == NULL) || (f->track[track]->decoderConfigLen <= 0))
		return 0;

	return f->track[track]->decoderConfigLen;
}

int32_t mp4ff_get_decoder_config_v2(const mp4ff_t * f, const int32_t track, uint8_t * ppBuf, uint32_t pBufSize)
{
	if(track >= f->total_tracks)
		return -1;

	if(f->track[track]->decoderConfig == NULL || f->track[track]->decoderConfigLen == 0)
		return 0;

	if(pBufSize < f->track[track]->decoderConfigLen)
		return -2;

	memcpy(ppBuf, f->track[track]->decoderConfig, f->track[track]->decoderConfigLen);

	return f->track[track]->decoderConfigLen;
}

int32_t mp4ff_get_track_type(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->type;
}

int32_t mp4ff_total_tracks(const mp4ff_t * f)
{
	return f->total_tracks;
}

uint32_t mp4ff_get_avg_bitrate(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->avgBitrate;
}

int64_t mp4ff_get_track_duration(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->duration;
}

int32_t mp4ff_num_samples(const mp4ff_t * f, const int32_t track)
{
	int32_t i;
	int32_t total = 0;

	for(i = 0; i < f->track[track]->stts_entry_count; i++)
		total += f->track[track]->stts_sample_count[i];

	return total;
}

uint32_t mp4ff_get_sample_rate(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->sampleRate;
}

uint32_t mp4ff_get_channel_count(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->channelCount;
}

uint32_t mp4ff_get_audio_type(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->audioType;
}

uint32_t mp4ff_get_sample_size(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->sampleSize;
}

int32_t mp4ff_get_sample_duration(const mp4ff_t * f, const int32_t track, const int32_t sample)
{
	int32_t i, co = 0;

	for(i = 0; i < f->track[track]->stts_entry_count; i++) {
		int32_t delta = f->track[track]->stts_sample_count[i];
		if(sample < co + delta)
			return f->track[track]->stts_sample_delta[i];
		co += delta;
	}

	return (int32_t) (-1);
}

// among all samples
int32_t mp4ff_read_sample_maxsize(const mp4ff_t * f, const int32_t track)
{
	return f->track[track]->stsz_max_sample_size;
}

// specified sample
int32_t mp4ff_read_sample_getsize(mp4ff_t * f, const int32_t track, const int32_t sample)
{
	int32_t temp = mp4ff_audio_frame_size(f, track, sample);
	if(temp < 0)
		temp = 0;

	return temp;
}

int32_t mp4ff_read_sample_v2(mp4ff_t * f, const int32_t track, const int32_t sample, uint8_t * buffer)
{
	int32_t result = 0;
	int32_t size = mp4ff_audio_frame_size(f, track, sample);

	if(size <= 0)
		return 0;

	if(mp4ff_set_sample_position(f, track, sample) != 0)
		return 0;

	result = mp4ff_read_data(f, (int8_t *) buffer, size);

	return result;
}
