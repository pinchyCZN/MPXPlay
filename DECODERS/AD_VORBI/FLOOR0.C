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

 function: floor backend 0 implementation
 last mod: $Id: floor0.c,v 1.53 2002/09/24 00:00:00 PDSoft Exp $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "codec.h"
#include "codecint.h"
#include "registry.h"
#include "lsp.h"
#include "codebook.h"
#include "scales.h"
#include "os.h"

typedef struct {
 int ln;
 int  m;
 ogg_double_t **linearcosmap;
 int  n[2];

 vorbis_info_floor0 *vif0;
 ogg_double_t **lsp_data;

} vorbis_look_floor0;

static void floor0_free_info(vorbis_info_floor *i)
{
 vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
 if(info)
  _ogg_free(info);
}

static void floor0_free_look(vorbis_look_floor *vlf)
{
 vorbis_look_floor0 *look=(vorbis_look_floor0 *)vlf;
 unsigned int i;

 if(look){
  vorbis_info_floor0 *info=look->vif0;
  if(look->linearcosmap){
   if(look->linearcosmap[0]) _ogg_free(look->linearcosmap[0]);
   if(look->linearcosmap[1]) _ogg_free(look->linearcosmap[1]);
   _ogg_free(look->linearcosmap);
  }
  if(look->lsp_data && info && info->vi){
   vorbis_info *vi=info->vi;
   for(i=0;i<vi->channels;i++)
    _ogg_free(look->lsp_data[i]);
   _ogg_free(look->lsp_data);
  }
  _ogg_free(look);
 }
}

static vorbis_info_floor *floor0_unpack (vorbis_info *vi,oggpack_buffer *opb)
{
 codec_setup_info     *ci=vi->codec_setup;
 int j;

 vorbis_info_floor0 *info=_ogg_malloc(sizeof(*info));
 info->order=oggpack_read24(opb,8);
 info->rate=oggpack_read24(opb,16);
 info->barkmap=oggpack_read24(opb,16);
 info->ampbits=oggpack_read24(opb,6);
 info->ampdB=oggpack_read24(opb,8);
 info->numbooks=oggpack_read24(opb,4)+1;

 if(info->order<1)    goto err_out;
 if(info->rate<1)     goto err_out;
 if(info->barkmap<1)  goto err_out;
 if(info->numbooks<1) goto err_out;

 for(j=0;j<info->numbooks;j++){
  info->books[j]=oggpack_read24(opb,8);
  if(info->books[j]<0 || info->books[j]>=ci->books)
   goto err_out;
 }
 return(info);

err_out:
 floor0_free_info(info);
 return(NULL);
}

static void floor0_map_lazy_init(vorbis_dsp_state  *vd,
				 vorbis_info_floor *infoX,
				 vorbis_look_floor0 *look,
				 unsigned int W)
{
 if(!look->linearcosmap[W]){
  vorbis_info        *vi=vd->vi;
  codec_setup_info   *ci=vi->codec_setup;
  vorbis_info_floor0 *info=(vorbis_info_floor0 *)infoX;
  int n=ci->blocksizes[W]/2,j;

  ogg_double_t scale=look->ln/toBARK(info->rate/2.f);

  look->linearcosmap[W]=_ogg_malloc((n+1)*sizeof(**look->linearcosmap));
  for(j=0;j<n;j++){
   int val=floor( toBARK((info->rate/2.f)/n*j)*scale);
   if(val>=look->ln)
    val=look->ln-1;
   look->linearcosmap[W][j]=2.0f*cos(M_PI/(float)look->ln*(float)val);;
  }
  look->linearcosmap[W][j]=-999.0f;
  look->n[W]=n;
 }
}

static vorbis_look_floor *floor0_look(vorbis_dsp_state *vd,vorbis_info_floor *i)
{
 int j,dimmax;
 codec_setup_info     *ci=vd->vi->codec_setup;
 vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
 vorbis_look_floor0 *look=_ogg_calloc(1,sizeof(*look));
 vorbis_info        *vi=vd->vi;

 info->vi=vi;

 look->m=info->order;
 look->ln=info->barkmap;
 look->vif0=info;

 look->linearcosmap=_ogg_calloc(2,sizeof(*look->linearcosmap));

 floor0_map_lazy_init(vd,info,look,0);
 floor0_map_lazy_init(vd,info,look,1);

 dimmax=0;

 for(j=0;j<info->numbooks;j++){
  codebook *b=ci->fullbooks+info->books[j];
  dimmax=max(dimmax,b->dim);
 }

 look->lsp_data=_ogg_malloc(vi->channels*sizeof(*(look->lsp_data)));
 for(j=0;j<vi->channels;j++)
  look->lsp_data[j]=_ogg_malloc((look->m+dimmax+1)*sizeof(*(look->lsp_data[j])));

 return look;
}

static void *floor0_inverse1(vorbis_block *vb,vorbis_look_floor *i,unsigned int channel)
{
 vorbis_look_floor0 *look=(vorbis_look_floor0 *)i;
 vorbis_info_floor0 *info=look->vif0;
 oggpack_buffer *vbopb=&vb->opb;
 int j,k;

 int ampraw=oggpack_read32(vbopb,info->ampbits);
 if(ampraw>0){
  long maxval=(1<<info->ampbits)-1;
  ogg_double_t amp=(ogg_double_t)ampraw/maxval*info->ampdB;
  int booknum=oggpack_read32(vbopb,_ilog(info->numbooks));

  if((booknum>=0) && (booknum<info->numbooks)){
   codec_setup_info  *ci=vb->vd->vi->codec_setup;
   codebook *b=ci->fullbooks+info->books[booknum];
   int bookdim=b->dim;
   ogg_double_t *lsp=look->lsp_data[channel];

   if(vorbis_book_decodev_set(b,lsp,vbopb,bookdim)<0)
    goto eop;

   for(j=bookdim;j<look->m;){
    ogg_double_t last;
    if(vorbis_book_decodev_set(b,lsp+j,vbopb,bookdim)<0)
     goto eop;
    last=lsp[j-1];
    for(k=0;k<bookdim;k++,j++)
     lsp[j]+=last;
   }

   lsp[look->m]=amp;
   return(lsp);
  }
 }
eop:
 return(NULL);
}

static int floor0_inverse2(vorbis_block *vb,vorbis_look_floor *i,void *memo,ogg_double_t *out)
{
 vorbis_look_floor0 *look=(vorbis_look_floor0 *)i;
 vorbis_info_floor0 *info=look->vif0;

 if(memo){
  ogg_double_t *lsp=(ogg_double_t *)memo;
  ogg_double_t amp=lsp[look->m];

  vorbis_lsp_to_curve(out,lsp,look->linearcosmap[vb->W],
		      look->n[vb->W],look->m,
		      amp,(ogg_double_t)info->ampdB);
  return(1);
 }
 memset(out,0,sizeof(*out)*look->n[vb->W]);
 return(0);
}

vorbis_func_floor floor0_exportbundle={
  &floor0_unpack,
  &floor0_look,
  &floor0_free_info,
  &floor0_free_look,
  &floor0_inverse1,
  &floor0_inverse2
};
