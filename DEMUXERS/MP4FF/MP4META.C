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
** $Id: mp4meta.c,v 1.20 2005/01/12 00:00:00 PDSoft Exp $
**/

#ifdef USE_TAGGING

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mp4ff.h"

#define TAGS_INIT_STORAGE 16

static int32_t mp4ff_tag_add_field(mp4ff_metadata_t * tags, unsigned int atom, const char *value)
{
	mp4ff_tag_t *tagp;

	if(!atom || !value)
		return 0;

	if(tags->count >= tags->storage) {
		unsigned int newsize = (tags->storage) ? (tags->storage * 2) : TAGS_INIT_STORAGE;
		mp4ff_tag_t *newmem = calloc(newsize, sizeof(mp4ff_tag_t));
		if(!newmem)
			return 0;
		if(tags->tags) {
			if(tags->count)
				memcpy(newmem, tags->tags, tags->count * sizeof(mp4ff_tag_t));
			free(tags->tags);
		}
		tags->tags = newmem;
		tags->storage = newsize;
	}

	tagp = &tags->tags[tags->count];

	tagp->value = strdup(value);

	if(!tagp->value)
		return 0;

	tagp->atom = atom;

	tags->count++;
	return 1;
}

void mp4ff_tag_delete(mp4ff_metadata_t * tags)
{
	mp4ff_tag_t *tagp;

	tagp = tags->tags;
	if(tagp) {
		uint32_t i = tags->count;
		if(i) {
			do {
				if(tagp->value)
					free(tagp->value);
				tagp++;
			} while(--i);
		}
		free(tags->tags);
		tags->tags = NULL;
	}

	tags->count = 0;
}

extern char *mpxplay_tagging_id3v1_index_to_genre(unsigned int i);

static int32_t mp4ff_parse_tag(mp4ff_t * f, const uint8_t parent_atom_type, const int32_t size)
{
	uint8_t atom_type;
	uint8_t header_size = 0;
	uint64_t subsize, sumsize = 0;
	uint32_t done = 0;

	while(sumsize < size) {
		uint64_t destpos;
		subsize = mp4ff_atom_read_header(f, &atom_type, &header_size);
		destpos = mp4ff_position(f) + subsize - header_size;
		if(!done) {
			if(atom_type == ATOM_DATA) {
				mp4ff_read_char(f);	// version
				mp4ff_read_int24(f);	// flags
				mp4ff_read_int32(f);	// reserved

				// some need special attention
				if((parent_atom_type == ATOM_GENRE2) || (parent_atom_type == ATOM_TEMPO)) {
					if(subsize - header_size >= 8 + 2) {
						uint16_t val = mp4ff_read_int16(f);

						if(val) {
							if(parent_atom_type == ATOM_TEMPO) {
								char temp[16];
								sprintf(temp, "%.5u BPM", val);
								mp4ff_tag_add_field(&(f->tags), parent_atom_type, temp);
							} else {
								const char *temp = mpxplay_tagging_id3v1_index_to_genre(val - 1);
								if(temp)
									mp4ff_tag_add_field(&(f->tags), parent_atom_type, temp);
							}
						}
						done = 1;
					}
				} else if((parent_atom_type == ATOM_TRACK) || (parent_atom_type == ATOM_DISC)) {
					if(!done && subsize - header_size >= 8 + 8) {
						uint16_t index, total;
						char temp[32];
						mp4ff_read_int16(f);
						index = mp4ff_read_int16(f);
						total = mp4ff_read_int16(f);
						mp4ff_read_int16(f);

						if(total > 0)
							sprintf(temp, "%d/%d", index, total);
						else
							sprintf(temp, "%d", index);
						mp4ff_tag_add_field(&(f->tags), parent_atom_type, temp);
						done = 1;
					}
				} else {
					char *data = mp4ff_read_string(f, (uint32_t) (subsize - (header_size + 8)));
					mp4ff_tag_add_field(&(f->tags), parent_atom_type, data);
					free(data);
					done = 1;
				}
			}
			mp4ff_set_position(f, destpos);
			sumsize += subsize;
		}
	}

	return 1;
}

int32_t mp4ff_parse_metadata(mp4ff_t * f, const int32_t size)
{
	uint64_t subsize, sumsize = 0;
	uint8_t atom_type;
	uint8_t header_size = 0;

	while(sumsize < size) {
		subsize = mp4ff_atom_read_header(f, &atom_type, &header_size);
		mp4ff_parse_tag(f, atom_type, (uint32_t) (subsize - header_size));
		sumsize += subsize;
	}

	return 0;
}

char *mp4ff_meta_search_by_atom(mp4ff_t * f, uint32_t atom)
{
	mp4ff_metadata_t *tags = &f->tags;
	uint32_t i = tags->count;
	mp4ff_tag_t *tagp = tags->tags;

	if(tagp && i) {
		do {
			if(tagp->atom == atom)
				return tagp->value;
			tagp++;
		} while(--i);
	}
	return NULL;
}

#endif							// USE_TAGGING
