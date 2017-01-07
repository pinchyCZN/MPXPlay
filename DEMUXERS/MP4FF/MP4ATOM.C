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
** $Id: mp4atom.c,v 1.22 2005/01/12 00:00:00 PDSoft Exp $
**/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mp4ff.h"

typedef struct atom_description_s{
 uint8_t atom_id;
 char atomname[4];
 int32_t (*readatom)(mp4ff_t *f, int32_t size, uint8_t atom_id);
}atom_description_s;

static struct atom_description_s atom_descriptions[]=
{
 {ATOM_MOOV,  "moov",NULL},
 {ATOM_TRAK,  "trak",NULL},
 {ATOM_EDTS,  "edts",NULL},
 {ATOM_MDIA,  "mdia",NULL},
 {ATOM_MINF,  "minf",NULL},
 {ATOM_STBL,  "stbl",NULL},
 {ATOM_UDTA,  "udta",NULL},
 {ATOM_ILST,  "ilst",NULL},
 {ATOM_TITLE, "©nam",NULL},
 {ATOM_ARTIST,"©ART",NULL},
 {ATOM_WRITER,"©wrt",NULL},
 {ATOM_ALBUM, "©alb",NULL},
 {ATOM_DATE,  "©day",NULL},
 {ATOM_TOOL,  "©too",NULL},
 {ATOM_COMMENT,"©cmt",NULL},
 {ATOM_GENRE1,"©gen",NULL},
 {ATOM_TRACK, "trkn",NULL},
 {ATOM_DISC,  "disk",NULL},
 {ATOM_COMPILATION,"cpil",NULL},
 {ATOM_GENRE2,"gnre",NULL},
 {ATOM_TEMPO, "tmpo",NULL},
 {ATOM_COVER, "covr",NULL},
 {ATOM_DRMS,  "drms",NULL},
 {ATOM_SINF,  "sinf",NULL},
 {ATOM_SCHI,  "schi",NULL},

 {ATOM_FTYP,"ftyp",NULL},
 {ATOM_MDAT,"mdat",NULL},
 {ATOM_MVHD,"mvhd",NULL},
 {ATOM_TKHD,"tkhd",NULL},
 {ATOM_TREF,"tref",NULL},
 {ATOM_MDHD,"mdhd",NULL},
 {ATOM_VMHD,"vmhd",NULL},
 {ATOM_SMHD,"smhd",NULL},
 {ATOM_HMHD,"hmhd",NULL},
 {ATOM_STSD,"stsd",NULL},
 {ATOM_STTS,"stts",NULL},
 {ATOM_STSZ,"stsz",NULL},
 {ATOM_STZ2,"stz2",NULL},
 {ATOM_STCO,"stco",NULL},
 {ATOM_STSC,"stsc",NULL},
 {ATOM_MP4A,"mp4a",NULL},
 {ATOM_MP4V,"mp4v",NULL},
 {ATOM_MP4S,"mp4s",NULL},
 {ATOM_ESDS,"esds",NULL},
 {ATOM_META,"meta",NULL},
 {ATOM_NAME,"name",NULL},
 {ATOM_DATA,"data",NULL},
 {ATOM_CTTS,"ctts",NULL},
 {ATOM_FRMA,"frma",NULL},
 {ATOM_IVIV,"iviv",NULL},
 {ATOM_PRIV,"priv",NULL},
 {ATOM_USER,"user",NULL},
 {ATOM_KEY ,"key ",NULL},
 {ATOM_ALAC,"alac",NULL},

 {ATOM_UNKNOWN,"",NULL}
};

static uint8_t mp4ff_atom_name_to_type(uint8_t *cname)
{
 struct atom_description_s *a=&atom_descriptions[0];
 const uint32_t iname=(*(uint32_t *)cname);
 do{
  if((*(uint32_t *)&a->atomname[0])==iname)
   break;
  a++;
 }while(a->atom_id<ATOM_UNKNOWN);
 return a->atom_id;
}

static uint32_t mp4ff_atom_get_size(uint8_t *data)
{
 return ((((uint32_t)data[0])<<24) | (((uint32_t)data[1])<<16) | (((uint32_t)data[2])<< 8) | ( (uint32_t)data[3]));
}

// read atom header, return atom size, atom size is with header included
uint64_t mp4ff_atom_read_header(mp4ff_t *f, uint8_t *atom_type, uint8_t *header_size)
{
 uint64_t size;
 uint8_t atom_header[8];

 if(mp4ff_read_data(f, (int8_t *)(&atom_header[0]), 8)!=8)
  return 0;

 size = mp4ff_atom_get_size(atom_header);
 *header_size = 8;

 // check for 64 bit atom size
 if(size == 1){
  *header_size = 16;
  size = mp4ff_read_int64(f);
 }

 *atom_type = mp4ff_atom_name_to_type(&atom_header[4]);

 return size;
}

static int32_t mp4ff_read_stsz(mp4ff_t *f)
{
 mp4ff_track_t *p_track = f->lasttrack;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 p_track->stsz_sample_size = mp4ff_read_int32(f);
 p_track->stsz_max_sample_size = p_track->stsz_sample_size;
 p_track->stsz_sample_count = mp4ff_read_int32(f);

 if(!p_track->stsz_sample_size && p_track->stsz_sample_count){
  int32_t i;

  p_track->stsz_table = (int32_t*)malloc(p_track->stsz_sample_count*sizeof(int32_t));
  if(!p_track->stsz_table)
   return 1;

  for(i=0; i<p_track->stsz_sample_count; i++){
   int32_t sample_size = mp4ff_read_int32(f);
   p_track->stsz_table[i] = sample_size;
   if(sample_size > p_track->stsz_max_sample_size)
    p_track->stsz_max_sample_size=sample_size;
  }
 }

 return 0;
}

static int32_t mp4ff_read_esds(mp4ff_t *f)
{
 mp4ff_track_t *p_track = f->lasttrack;
 uint8_t tag;
 uint32_t temp;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 // get and verify ES_DescrTag
 tag = mp4ff_read_char(f);
 if(tag==0x03){
  if(mp4ff_read_mp4_descr_length(f) < (5 + 15)) // ???
   return 1;
  mp4ff_read_int24(f); // skip 3 bytes
 }else
  mp4ff_read_int16(f); // skip 2 bytes

 // get and verify DecoderConfigDescrTab
 if(mp4ff_read_char(f) != 0x04)
  return 1;

 temp = mp4ff_read_mp4_descr_length(f);
 if(temp<13)
  return 1;

 p_track->audioType  = mp4ff_read_char(f);
 mp4ff_read_int32(f);//0x15000414 ????
 p_track->maxBitrate = mp4ff_read_int32(f);
 p_track->avgBitrate = mp4ff_read_int32(f);

 // get and verify DecSpecificInfoTag
 if(mp4ff_read_char(f) != 0x05)
  return 1;

 p_track->decoderConfigLen = mp4ff_read_mp4_descr_length(f);

 if(p_track->decoderConfig)
  free(p_track->decoderConfig);
 p_track->decoderConfig = malloc(p_track->decoderConfigLen);
 if(p_track->decoderConfig)
  mp4ff_read_data(f, (int8_t *)p_track->decoderConfig, p_track->decoderConfigLen);
 else
  p_track->decoderConfigLen = 0;

 return 0;
}

static int32_t mp4ff_read_mp4a(mp4ff_t *f)
{
 uint64_t size;
 int32_t i;
 mp4ff_track_t *p_track = f->lasttrack;
 uint8_t atom_type = 0;
 uint8_t header_size = 0;

 for (i = 0; i < 6; i++)
  mp4ff_read_char(f); // reserved

 mp4ff_read_int16(f); // data_reference_index
 mp4ff_read_int32(f); // reserved
 mp4ff_read_int32(f); // reserved

 p_track->channelCount = mp4ff_read_int16(f);
 p_track->sampleSize   = mp4ff_read_int16(f);
 mp4ff_read_int16(f);
 p_track->sampleRate   = mp4ff_read_int32(f);
 mp4ff_read_int16(f);

 size = mp4ff_atom_read_header(f, &atom_type, &header_size);
 if(atom_type == ATOM_ESDS)
  mp4ff_read_esds(f);

 return 0;
}

static uint32_t mp4ff_read_alac(mp4ff_t *f,int32_t size)
{
 mp4ff_track_t *p_track = f->lasttrack;
 uint16_t version,i;

 size-=8;

 for (i = 0; i < 6; i++)
  mp4ff_read_char(f); // reserved
 size-=6;

 version=mp4ff_read_int16(f); size-=2; // version have to be 1
 mp4ff_read_int16(f); // revision level
 mp4ff_read_int32(f); // vendor
 mp4ff_read_int16(f); // something
 size-=8;

 p_track->channelCount = mp4ff_read_int16(f); size-=2;
 p_track->sampleSize   = mp4ff_read_int16(f); size-=2;
 mp4ff_read_int16(f); // compression id
 mp4ff_read_int16(f); // packet_size
 size-=4;
 p_track->sampleRate   = mp4ff_read_int16(f); size-=2;
 mp4ff_read_int16(f); size-=2;

 p_track->decoderConfigLen = size+12+8;
 if(p_track->decoderConfig)
  free(p_track->decoderConfig);
 p_track->decoderConfig = malloc(p_track->decoderConfigLen);
 if(p_track->decoderConfig){
  ((uint32_t *)p_track->decoderConfig)[0]=0x0c000000;
  memcpy(p_track->decoderConfig+4,"frma",4);
  memcpy(p_track->decoderConfig+8,"alac",4);
  mp4ff_read_data(f,(int8_t *)p_track->decoderConfig+12,size);
  memset(p_track->decoderConfig+12+size,0,8);
 }else
  p_track->decoderConfigLen = 0;

 p_track->audioType=MP4_ALAC_AUDIO_TYPE;

 return 0;
}

static int32_t mp4ff_read_stsd(mp4ff_t *f)
{
 int32_t i;
 mp4ff_track_t *p_track = f->lasttrack;
 uint8_t header_size = 0;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 p_track->stsd_entry_count = mp4ff_read_int32(f);

 for(i=0; i<p_track->stsd_entry_count; i++){
  uint64_t skip = mp4ff_position(f);
  uint64_t size;
  uint8_t atom_type = 0;
  size = mp4ff_atom_read_header(f, &atom_type, &header_size);
  skip += size;

  switch(atom_type){
   case ATOM_MP4A:p_track->type=TRACK_AUDIO;mp4ff_read_mp4a(f);break;
   case ATOM_MP4V:p_track->type=TRACK_VIDEO;break;
   case ATOM_MP4S:p_track->type=TRACK_SYSTEM;break;
   case ATOM_ALAC:p_track->type=TRACK_AUDIO;mp4ff_read_alac(f,size);break;
          default:p_track->type=TRACK_UNKNOWN;
  }

  mp4ff_set_position(f, skip);
 }

 return 0;
}

static int32_t mp4ff_read_stsc(mp4ff_t *f)
{
 int32_t i;
 mp4ff_track_t *p_track = f->lasttrack;

 if(p_track->stsc_entry_count)
  return 0;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 p_track->stsc_entry_count = mp4ff_read_int32(f);
 if(!p_track->stsc_entry_count)
  return 1;

 p_track->stsc_first_chunk       = (int32_t*)malloc(p_track->stsc_entry_count*sizeof(int32_t));
 p_track->stsc_samples_per_chunk = (int32_t*)malloc(p_track->stsc_entry_count*sizeof(int32_t));
 p_track->stsc_sample_desc_index = (int32_t*)malloc(p_track->stsc_entry_count*sizeof(int32_t));

 if(!p_track->stsc_first_chunk || !p_track->stsc_samples_per_chunk || !p_track->stsc_sample_desc_index)
  return 1;

 for(i=0; i<p_track->stsc_entry_count; i++){
  p_track->stsc_first_chunk[i]       = mp4ff_read_int32(f);
  p_track->stsc_samples_per_chunk[i] = mp4ff_read_int32(f);
  p_track->stsc_sample_desc_index[i] = mp4ff_read_int32(f);
 }

 return 0;
}

static int32_t mp4ff_read_stco(mp4ff_t *f)
{
 int32_t i;
 mp4ff_track_t *p_track = f->lasttrack;

 if(p_track->stco_entry_count)
  return 0;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 p_track->stco_entry_count = mp4ff_read_int32(f);
 if(!p_track->stco_entry_count)
  return 1;

 p_track->stco_chunk_offset =(int32_t*)malloc(p_track->stco_entry_count*sizeof(int32_t));
 if(!p_track->stco_chunk_offset)
  return 1;

 for(i=0; i<p_track->stco_entry_count; i++)
  p_track->stco_chunk_offset[i] = mp4ff_read_int32(f);

 return 0;
}

static int32_t mp4ff_read_ctts(mp4ff_t *f)
{
 int32_t i;
 mp4ff_track_t *p_track = f->lasttrack;

 if(p_track->ctts_entry_count)
  return 0;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 p_track->ctts_entry_count = mp4ff_read_int32(f);
 if(!p_track->ctts_entry_count)
  return 1;

 p_track->ctts_sample_count  = (int32_t*)malloc(p_track->ctts_entry_count * sizeof(int32_t));
 p_track->ctts_sample_offset = (int32_t*)malloc(p_track->ctts_entry_count * sizeof(int32_t));
 if(!p_track->ctts_sample_count || !p_track->ctts_sample_offset){
  p_track->ctts_entry_count = 0;
  return 1;
 }

 for(i=0; i<f->lasttrack->ctts_entry_count; i++){
  p_track->ctts_sample_count[i]  = mp4ff_read_int32(f);
  p_track->ctts_sample_offset[i] = mp4ff_read_int32(f);
 }

 return 0;
}

static int32_t mp4ff_read_stts(mp4ff_t *f)
{
 int32_t i;
 mp4ff_track_t * p_track = f->lasttrack;

 if(p_track->stts_entry_count)
  return 0;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 p_track->stts_entry_count = mp4ff_read_int32(f);
 if(!p_track->stts_entry_count)
  return 1;

 p_track->stts_sample_count = (int32_t*)malloc(p_track->stts_entry_count * sizeof(int32_t));
 p_track->stts_sample_delta = (int32_t*)malloc(p_track->stts_entry_count * sizeof(int32_t));
 if(!p_track->stts_sample_count || !p_track->stts_sample_delta){
  p_track->stts_entry_count = 0;
  return 1;
 }

 for(i=0; i<p_track->stts_entry_count; i++){
  p_track->stts_sample_count[i] = mp4ff_read_int32(f);
  p_track->stts_sample_delta[i] = mp4ff_read_int32(f);
 }

 return 0;
}

static int32_t mp4ff_read_mdhd(mp4ff_t *f)
{
 mp4ff_track_t * p_track = f->lasttrack;
 uint32_t version;

 version = mp4ff_read_int32(f);
 if(version==1){
  mp4ff_read_int64(f);//creation-time
  mp4ff_read_int64(f);//modification-time
  p_track->timeScale = mp4ff_read_int32(f);
  p_track->duration  = mp4ff_read_int64(f);
 }else{ //version == 0
  uint32_t temp;

  mp4ff_read_int32(f);//creation-time
  mp4ff_read_int32(f);//modification-time

  p_track->timeScale = mp4ff_read_int32(f);
  temp = mp4ff_read_int32(f);
  p_track->duration = (temp == (uint32_t)(-1)) ? (uint64_t)(-1) : (uint64_t)(temp);
 }

 mp4ff_read_int16(f);
 mp4ff_read_int16(f);
 return 1;
}

static int32_t mp4ff_read_meta(mp4ff_t *f, const uint64_t size)
{
 uint64_t subsize, sumsize = 0;
 uint8_t atom_type;
 uint8_t header_size = 0;

 mp4ff_read_char(f);  // version
 mp4ff_read_int24(f); // flags

 while(sumsize < (size-(header_size+4))){
  subsize = mp4ff_atom_read_header(f, &atom_type, &header_size);
  if(subsize <= header_size+4)
   return 1;

  if(atom_type == ATOM_ILST)
   mp4ff_parse_metadata(f, (uint32_t)(subsize-(header_size+4)));
  else
   mp4ff_set_position(f, mp4ff_position(f)+subsize-header_size);

  sumsize += subsize;
 }

 return 0;
}

int32_t mp4ff_atom_read(mp4ff_t *f, const int32_t size, const uint8_t atom_type)
{
 uint64_t dest_position = mp4ff_position(f)+size-8;

 switch(atom_type){
  case ATOM_STSZ: mp4ff_read_stsz(f);break; // sample size box
  case ATOM_STTS: mp4ff_read_stts(f);break; // time to sample box
  case ATOM_CTTS: mp4ff_read_ctts(f);break; // composition offset box
  case ATOM_STSC: mp4ff_read_stsc(f);break; // sample to chunk box
  case ATOM_STCO: mp4ff_read_stco(f);break; // chunk offset box
  case ATOM_STSD: mp4ff_read_stsd(f);break; // sample description box
  case ATOM_MDHD: mp4ff_read_mdhd(f);break; // track header
  case ATOM_META: mp4ff_read_meta(f,size);break;// iTunes Metadata box
 }

 mp4ff_set_position(f, dest_position);

 return 0;
}
