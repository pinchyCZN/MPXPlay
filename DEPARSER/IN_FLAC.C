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
//function: FLAC file handling (parsing/decoding with decoders\ffmpegac lib)

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_FLAC

#include "in_rawau.h"
#include "tagging.h"
#include "newfunc\newfunc.h"

#define FFLAC_BITSTREAM_BUFSIZE 65536

typedef struct flac_demuxer_data_s{
 struct mpxplay_bitstreambuf_s *bs;
 mpxp_filesize_t flacheaderbegin; // possible bullshit id3v2 header
 unsigned long max_blocksize,min_framesize,max_framesize;
 mpxp_uint8_t *extradata;
 unsigned long extradata_size;
 unsigned long pcmdatalen;
}flac_demuxer_data_s;

static int flac_parse_header(struct flac_demuxer_data_s *flaci,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis);

static int infflac_assign_values(struct flac_demuxer_data_s *flaci,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis)
{
 mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 struct mpxplay_streampacket_info_s *spi=miis->audio_stream;
 unsigned int bytes_per_sample;

 if((adi->outchannels<PCM_MIN_CHANNELS) || (adi->outchannels>PCM_MAX_CHANNELS))
  return 0;
 if((adi->bits<PCM_MIN_BITS) || (adi->bits>PCM_MAX_BITS))
  return 0;

 miis->timemsec=(float)flaci->pcmdatalen*1000.0/(float)adi->freq;

 miis->longname="  FLAC  ";

 adi->bitratetext=malloc(MPXPLAY_ADITEXTSIZE_BITRATE+8);
 if(!adi->bitratetext)
  return 0;

 bytes_per_sample=(adi->bits+7)/8;
 sprintf(adi->bitratetext,"%2d/%2.1f%%",adi->bits,100.0*(float)miis->filesize/(float)flaci->pcmdatalen/(float)bytes_per_sample/(float)adi->filechannels);
 adi->bitratetext[MPXPLAY_ADITEXTSIZE_BITRATE]=0;

 spi->bs_framesize=flaci->max_framesize;

 return 1;
}

static int infflac_check_header(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 struct flac_demuxer_data_s *flaci;
 unsigned long id3v2size;

 if(!fbfs->fopen_read(fbds,filename,0))
  return MPXPLAY_ERROR_INFILE_FILEOPEN;

 miis->filesize=fbfs->filelength(fbds);
 if(miis->filesize<16)  // ???
  goto err_out_chk;

 flaci=calloc(1,sizeof(struct flac_demuxer_data_s));
 if(!flaci)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 miis->private_data=flaci;

 flaci->bs=mpxplay_bitstream_alloc(FFLAC_BITSTREAM_BUFSIZE);
 if(!flaci->bs)
  goto err_out_chk;

 if(mpxplay_bitstream_fill(flaci->bs,fbfs,fbds,MPXPLAY_TAGGING_ID3V2_HEADSIZE)!=MPXPLAY_ERROR_MPXINBUF_OK)
  goto err_out_chk;

 id3v2size=mpxplay_tagging_id3v2_totalsize(mpxplay_bitstream_getbufpos(flaci->bs));
 if(id3v2size){
  if(fbfs->fseek(fbds,id3v2size,SEEK_SET)>0){
   flaci->flacheaderbegin=id3v2size;
   mpxplay_bitstream_reset(flaci->bs);
  }
 }

 if(!flac_parse_header(flaci,fbfs,fbds,miis))
  goto err_out_chk;

 if(!infflac_assign_values(flaci,fbfs,fbds,miis))
  goto err_out_chk;

 return MPXPLAY_ERROR_INFILE_OK;

err_out_chk:
 return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int INFLAC_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 return infflac_check_header(fbfs,fbds,filename,miis);
}

static int INFLAC_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 struct mpxplay_streampacket_info_s *spi=miis->audio_stream;
 struct flac_demuxer_data_s *flaci;
 int retcode;

 retcode=infflac_check_header(fbfs,fbds,filename,miis);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;

 flaci=miis->private_data;

 spi->streamtype=MPXPLAY_SPI_STREAMTYPE_AUDIO;
 spi->wave_id=MPXPLAY_WAVEID_FLAC;
 funcbit_enable(spi->flags,(MPXPLAY_SPI_FLAG_NEED_DECODER|MPXPLAY_SPI_FLAG_NEED_PARSING));

 spi->extradata=flaci->extradata;
 spi->extradata_size=flaci->extradata_size;

 return MPXPLAY_ERROR_INFILE_OK;
}

static void INFLAC_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis)
{
 struct flac_demuxer_data_s *flaci=(struct flac_demuxer_data_s *)miis->private_data;
 mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 if(flaci){
  mpxplay_bitstream_free(flaci->bs);
  if(flaci->extradata)
   free(flaci->extradata);
  if(adi->bitratetext)
   free(adi->bitratetext);
  free(flaci);
 }
 fbfs->fclose(fbds);
}

//--------------------------------------------------------------------------
#define FLAC_METADATA_TYPE_STREAMINFO    0
#define FLAC_METADATA_TYPE_VORBISCOMMENT 4

#define FLAC_METADATA_SIZE_STREAMINFO   34

static void flac_metadata_streaminfo(struct flac_demuxer_data_s *flaci,struct mpxplay_infile_info_s *miis)
{
 mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;

 if(!flaci->extradata){
  flaci->extradata=malloc(FLAC_METADATA_SIZE_STREAMINFO+MPXPLAY_SPI_EXTRADATA_PADDING);
  if(flaci->extradata){
   pds_memcpy(flaci->extradata,mpxplay_bitstream_getbufpos(flaci->bs),FLAC_METADATA_SIZE_STREAMINFO);
   pds_memset(flaci->extradata+FLAC_METADATA_SIZE_STREAMINFO,0,MPXPLAY_SPI_EXTRADATA_PADDING);
   flaci->extradata_size=FLAC_METADATA_SIZE_STREAMINFO;
  }
 }
                        mpxplay_bitstream_skipbits(flaci->bs,  16);
 flaci->max_blocksize = mpxplay_bitstream_getbits_be24(flaci->bs, 16);

 flaci->min_framesize = mpxplay_bitstream_getbits_be24(flaci->bs, 24);
 flaci->max_framesize = mpxplay_bitstream_getbits_be24(flaci->bs, 24);

 adi->freq         = mpxplay_bitstream_getbits_be24(flaci->bs, 20);
 adi->filechannels = mpxplay_bitstream_getbits_be24(flaci->bs,  3) + 1;
 adi->outchannels  = adi->filechannels;
 adi->bits         = mpxplay_bitstream_getbits_be24(flaci->bs,  5) + 1;

 flaci->pcmdatalen = mpxplay_bitstream_getbits_be64(flaci->bs, 36);

 mpxplay_bitstream_skipbits(flaci->bs,64); // md5 sum
 mpxplay_bitstream_skipbits(flaci->bs,64); //
}

static int flac_parse_header(struct flac_demuxer_data_s *flaci,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis)
{
 mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 struct mpxplay_bitstreambuf_s *bs=flaci->bs;
 long metadata_last,metadata_type,metadata_size;
 unsigned char *bufpos;

 mpxplay_bitstream_fill(bs,fbfs,fbds,4);

 bufpos=mpxplay_bitstream_getbufpos(bs);
 if(bufpos[0]=='f' && bufpos[1]=='L' && bufpos[2]=='a' && bufpos[3]=='C'){
  mpxplay_bitstream_skipbits(bs,32);
  do{
   mpxplay_bitstream_fill(bs,fbfs,fbds,8);
   metadata_last = mpxplay_bitstream_getbits_be24(bs, 1);
   metadata_type = mpxplay_bitstream_getbits_be24(bs, 7);
   metadata_size = mpxplay_bitstream_getbits_be24(bs, 24);
   if(metadata_size){
    mpxplay_bitstream_fill(bs,fbfs,fbds,metadata_size);
    switch(metadata_type){
     case FLAC_METADATA_TYPE_STREAMINFO:flac_metadata_streaminfo(flaci,miis);
      if(adi->freq && flaci->pcmdatalen && (adi->bits>=8) && adi->filechannels)
       return 1;
      return 0;
     default:mpxplay_bitstream_skipbits(bs,metadata_size*8);
    }
   }
  }while(!metadata_last);
 }
 return 0;
}

#define FLAC_COMMENT_TYPES 7

static char *flac_tag_get(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,char *id3p,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
 static char *flaccommenttypes[FLAC_COMMENT_TYPES]={"title","artist","album","date","comment","genre","tracknumber"};
 static unsigned int id3index[FLAC_COMMENT_TYPES]={I3I_TITLE,I3I_ARTIST,I3I_ALBUM,I3I_YEAR,I3I_COMMENT,I3I_GENRE,I3I_TRACKNUM};
 struct flac_demuxer_data_s *flaci=(struct flac_demuxer_data_s *)miis->private_data;
 struct mpxplay_bitstreambuf_s *bs;
 long i,metadata_last,metadata_type,metadata_size,counted_size,comment_size,comments;
 unsigned char *bufpos;

 if(!flaci)
  return id3p;

 bs=mpxplay_bitstream_alloc(FFLAC_BITSTREAM_BUFSIZE);
 if(!bs)
  return id3p;

 fbfs->fseek(fbds,flaci->flacheaderbegin,SEEK_SET);

 mpxplay_bitstream_fill(bs,fbfs,fbds,4);
 bufpos=mpxplay_bitstream_getbufpos(bs);
 if(bufpos[0]=='f' && bufpos[1]=='L' && bufpos[2]=='a' && bufpos[3]=='C'){
  mpxplay_bitstream_skipbits(bs,32);
  do{
   mpxplay_bitstream_fill(bs,fbfs,fbds,8);
   metadata_last = mpxplay_bitstream_getbits_be24(bs, 1);
   metadata_type = mpxplay_bitstream_getbits_be24(bs, 7);
   metadata_size = mpxplay_bitstream_getbits_be24(bs, 24);
   if(metadata_size){
    mpxplay_bitstream_fill(bs,fbfs,fbds,metadata_size);
    switch(metadata_type){
     case FLAC_METADATA_TYPE_VORBISCOMMENT:
          counted_size=0;
          bufpos=mpxplay_bitstream_getbufpos(bs);
          comment_size=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
          bufpos+=comment_size;counted_size+=comment_size; // reference lib
          comments=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
          while(comments-- && (counted_size<metadata_size)){
           comment_size=PDS_GETB_LE32(bufpos);bufpos+=4;counted_size+=4;
           if(comment_size){
            char *p=pds_strchr(bufpos,'=');
            if(p){
             *p++=0;
             for(i=0;i<FLAC_COMMENT_TYPES;i++){
              if(pds_stricmp(bufpos,flaccommenttypes[i])==0){
               unsigned int len=comment_size-(p-bufpos);
               pds_strncpy(id3p,p,len);
               if((*(mpxplay_textconv_funcs->control))&ID3TEXTCONV_UTF_AUTO)
                len=mpxplay_textconv_funcs->utf8_to_char(id3p,len);  // ???
               len=mpxplay_textconv_funcs->all_to_char(id3p,len,ID3TEXTCONV_UTF8);
               if(len){
                id3ip[id3index[i]]=id3p;
                id3p+=len+1;
               }
              }
             }
            }
            counted_size+=comment_size;
            bufpos+=comment_size;
           }
          }
          goto err_out_ftg;

     default:mpxplay_bitstream_skipbits(bs,metadata_size*8);
    }
   }
  }while(!metadata_last);
 }

err_out_ftg:

 mpxplay_bitstream_free(bs);

 return id3p;
}

//--------------------------------------------------------------------------

struct mpxplay_infile_func_s IN_FLAC_funcs={
 0,
 NULL,
 NULL,
 &INFLAC_infile_check,
 &INFLAC_infile_check,
 &INFLAC_infile_open,
 &INFLAC_infile_close,
 &INRAWAU_infile_decode,
 &INRAWAU_fseek,
 NULL,
 &flac_tag_get,
 NULL,
 NULL,
 {"FLAC","FLA","FLC",NULL}
};

#endif // MPXPLAY_LINK_INFILE_FLAC
