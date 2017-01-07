/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: maintain the info structure, info <-> header packets
 last mod: $Id: info.c,v 1.60 2002/09/01 00:00:00 PDSoft Exp $

 ********************************************************************/

/* general handling of the header and the vorbis_info structure (and
   substructures) */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ogg.h"
#include "codec.h"
#include "codecint.h"
#include "codebook.h"
#include "registry.h"
#include "window.h"
#include "os.h"

static void _v_readstring(oggpack_buffer *o,char *buf,int bytes)
{
 while(bytes--)
  *buf++=oggpack_read24(o,8);
}

void vorbis_comment_init(vorbis_comment *vc)
{
 _ogg_memset(vc,0,sizeof(*vc));
}

void vorbis_comment_clear(vorbis_comment *vc)
{
#ifndef MPXPLAY
 if(vc){
  long i;
  for(i=0;i<vc->comments;i++)
   if(vc->user_comments[i])
    _ogg_free(vc->user_comments[i]);
  if(vc->user_comments)
   _ogg_free(vc->user_comments);
  if(vc->comment_lengths)
   _ogg_free(vc->comment_lengths);
  if(vc->vendor)
   _ogg_free(vc->vendor);
 }
 _ogg_memset(vc,0,sizeof(*vc));
#endif
}

void vorbis_info_init(vorbis_info *vi)
{
 _ogg_memset(vi,0,sizeof(*vi));
 vi->codec_setup=_ogg_calloc(1,sizeof(codec_setup_info));
}

void vorbis_info_clear(vorbis_info *vi)
{
 codec_setup_info     *ci=vi->codec_setup;
 int i;

 if(ci){

  for(i=0;i<ci->modes;i++)
   if(ci->mode_param[i])_ogg_free(ci->mode_param[i]);

  for(i=0;i<ci->maps;i++)
   _mapping_P[ci->map_type[i]]->free_info(ci->map_param[i]);

  for(i=0;i<ci->floors;i++)
   _floor_P[ci->floor_type[i]]->free_info(ci->floor_param[i]);

  for(i=0;i<ci->residues;i++)
   _residue_P[ci->residue_type[i]]->free_info(ci->residue_param[i]);

  for(i=0;i<ci->books;i++){
   if(ci->fullbooks)
    vorbis_book_clear(ci->fullbooks+i);
   if(ci->book_param[i])
    vorbis_staticbook_destroy(ci->book_param[i]);
  }
  if(ci->fullbooks)
   _ogg_free(ci->fullbooks);

  _ogg_free(ci);
 }

 _ogg_memset(vi,0,sizeof(*vi));
}

/* Header packing/unpacking ********************************************/

static int _vorbis_unpack_info(vorbis_info *vi,oggpack_buffer *opb)
{
 codec_setup_info     *ci=vi->codec_setup;
 if(!ci)
  return(OV_EFAULT);

 if(oggpack_read32(opb,32)!=0) // version
  return(OV_EVERSION);

 vi->channels=vi->outchannels=oggpack_read24(opb,8);
 vi->rate    =oggpack_read32(opb,32);

 oggpack_adv(opb,32);  // bitrate_upper
 vi->bitrate_nominal=oggpack_read32(opb,32);
 oggpack_adv(opb,32);  // bitrate_lower

 ci->blocksizes[0]=1<<oggpack_read24(opb,4);
 ci->blocksizes[1]=1<<oggpack_read24(opb,4);

 if(vi->rate<1)
  goto err_out;
 if(vi->channels<1)
  goto err_out;
 if(ci->blocksizes[0]<8)
  goto err_out;
 if(ci->blocksizes[1]<ci->blocksizes[0])
  goto err_out;

 if(oggpack_read1(opb)!=1)
  goto err_out; /* EOP check */

 return(0);
err_out:
 vorbis_info_clear(vi);
 return(OV_EBADHEADER);
}

#ifndef MPXPLAY
static int _vorbis_unpack_comment(vorbis_comment *vc,oggpack_buffer *opb)
{
 int i;
 int vendorlen=oggpack_read32(opb,32);
 if(vendorlen<0)
  goto err_out;
 vc->vendor=_ogg_calloc(vendorlen+1,1);
 _v_readstring(opb,vc->vendor,vendorlen);
 vc->comments=oggpack_read32(opb,32);
 if(vc->comments<0)
  goto err_out;
 vc->user_comments=_ogg_calloc(vc->comments+1,sizeof(*vc->user_comments));
 vc->comment_lengths=_ogg_calloc(vc->comments+1, sizeof(*vc->comment_lengths));

 for(i=0;i<vc->comments;i++){
  int len=oggpack_read32(opb,32);
  if(len<0)
   goto err_out;
  vc->comment_lengths[i]=len;
  vc->user_comments[i]=_ogg_calloc(len+1,1);
  _v_readstring(opb,vc->user_comments[i],len);
 }
 if(oggpack_read1(opb)!=1)
  goto err_out; /* EOP check */

 return(0);
err_out:
 vorbis_comment_clear(vc);
 return(OV_EBADHEADER);
}
#endif

static int _vorbis_unpack_books(vorbis_info *vi,oggpack_buffer *opb)
{
 codec_setup_info     *ci=vi->codec_setup;
 int i;

 if(!ci)
  return(OV_EFAULT);

 ci->books=oggpack_read24(opb,8)+1;
 for(i=0;i<ci->books;i++){
  ci->book_param[i]=_ogg_calloc(1,sizeof(*ci->book_param[i]));
  if(vorbis_staticbook_unpack(opb,ci->book_param[i]))
   goto err_out;
 }

 {
  int times=oggpack_read24(opb,6)+1;
  for(i=0;i<times;i++){
   int test=oggpack_read24(opb,16);
   if(test<0 || test>=VI_TIMEB)
    goto err_out;
  }
 }

 ci->floors=oggpack_read24(opb,6)+1;
 for(i=0;i<ci->floors;i++){
  ci->floor_type[i]=oggpack_read24(opb,16);
  if(ci->floor_type[i]<0 || ci->floor_type[i]>=VI_FLOORB)
   goto err_out;
  ci->floor_param[i]=_floor_P[ci->floor_type[i]]->unpack(vi,opb);
  if(!ci->floor_param[i])
   goto err_out;
 }

 ci->residues=oggpack_read24(opb,6)+1;
 for(i=0;i<ci->residues;i++){
  ci->residue_type[i]=oggpack_read24(opb,16);
  if(ci->residue_type[i]<0 || ci->residue_type[i]>=VI_RESB)
   goto err_out;
  ci->residue_param[i]=_residue_P[ci->residue_type[i]]->unpack(vi,opb);
  if(!ci->residue_param[i])
   goto err_out;
 }

 ci->maps=oggpack_read24(opb,6)+1;
 for(i=0;i<ci->maps;i++){
  ci->map_type[i]=oggpack_read24(opb,16);
  if(ci->map_type[i]<0 || ci->map_type[i]>=VI_MAPB)
   goto err_out;
  ci->map_param[i]=_mapping_P[ci->map_type[i]]->unpack(vi,opb);
  if(!ci->map_param[i])
   goto err_out;
 }

 ci->modes=oggpack_read24(opb,6)+1;
 for(i=0;i<ci->modes;i++){
  ci->mode_param[i]=_ogg_calloc(1,sizeof(*ci->mode_param[i]));
  ci->mode_param[i]->blockflag=oggpack_read1(opb);
  ci->mode_param[i]->windowtype=oggpack_read24(opb,16);
  ci->mode_param[i]->transformtype=oggpack_read24(opb,16);
  ci->mode_param[i]->mapping=oggpack_read24(opb,8);

  if(ci->mode_param[i]->windowtype>=VI_WINDOWB)goto err_out;
  if(ci->mode_param[i]->transformtype>=VI_WINDOWB)goto err_out;
  if(ci->mode_param[i]->mapping>=ci->maps)goto err_out;
 }

 if(oggpack_read1(opb)!=1)
  goto err_out; /* top level EOP check */

 return(0);
err_out:
 vorbis_info_clear(vi);
 return(OV_EBADHEADER);
}

int vorbis_synthesis_headerin(vorbis_info *vi,vorbis_comment *vc,ogg_packet *op)
{
 int packtype;
 oggpack_buffer opb;
 char buffer[6];

 if(op){
  oggpack_readinit(&opb,op->packet,op->bytes);

  packtype=oggpack_read24(&opb,8);
  _v_readstring(&opb,buffer,6);
  if(memcmp(buffer,"vorbis",6)!=0)
   return(OV_ENOTVORBIS);

  switch(packtype){
   case 0x01:
	if(!op->b_o_s)
	  return(OV_EBADHEADER);
	if(vi->rate!=0)
	  return(OV_EBADHEADER);

	return(_vorbis_unpack_info(vi,&opb));

   case 0x03:
	if(vi->rate==0)
	  return(OV_EBADHEADER);

#ifndef MPXPLAY
	return(_vorbis_unpack_comment(vc,&opb));
#else
        return(0); // comments are readed at Ogg demuxing in Mpxplay
#endif

   case 0x05:
	if(vi->rate==0)
	  return(OV_EBADHEADER);
#ifndef MPXPLAY
        if(vc->vendor==NULL)
	  return(OV_EBADHEADER);
#endif

	return(_vorbis_unpack_books(vi,&opb));

   default:
	return(OV_EBADHEADER);
	break;
  }
 }
 return(OV_EBADHEADER);
}
