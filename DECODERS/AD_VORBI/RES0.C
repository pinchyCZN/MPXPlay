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

 function: residue backend 0, 1 and 2 implementation
 last mod: $Id: res0.c,v 1.50 2003/02/10 00:00:00 PDSoft Exp $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "ogg.h"
#include "codec.h"
#include "codecint.h"
#include "registry.h"
#include "codebook.h"
#include "os.h"

static void res012_free_info(vorbis_info_residue *i)
{
 vorbis_info_residue0 *info=(vorbis_info_residue0 *)i;
 int j;
 if(info){
  if(info->partwordp)
   _ogg_free(info->partwordp);

  if(info->partbooks){
   for(j=0;j<info->stages;j++)
    if(info->partbooks[j])
     _ogg_free(info->partbooks[j]);
   _ogg_free(info->partbooks);
  }

  if(info->decodemap){
   for(j=0;j<info->look_partvals;j++)
    _ogg_free(info->decodemap[j]);
   _ogg_free(info->decodemap);
  }

  _ogg_free(info);
 }
}

static void res012_free_look(vorbis_look_residue *i)
{
}

static int ilog(unsigned int v)
{
 int ret=0;
 while(v){
  ret++;
  v>>=1;
 }
 return(ret);
}

static int icount(unsigned int v)
{
 int ret=0;
 while(v){
  ret+=v&1;
  v>>=1;
 }
 return(ret);
}

static vorbis_info_residue0 *_012_unpack(vorbis_info *vi,oggpack_buffer *opb)
{
 int j,acc=0;
 vorbis_info_residue0 *info=_ogg_calloc(1,sizeof(*info));
 codec_setup_info     *ci=vi->codec_setup;

 info->begin=oggpack_read24(opb,24);
 info->len=oggpack_read24(opb,24)-info->begin;
 info->samples_per_partition=oggpack_read24(opb,24)+1;
 info->partitions=oggpack_read24(opb,6)+1;
 info->groupbook=oggpack_read24(opb,8);

 if(info->groupbook>=ci->books)
  goto errout;

 for(j=0;j<info->partitions;j++){
  int cascade=oggpack_read24(opb,3);
  if(oggpack_read1(opb))
   cascade|=(oggpack_read24(opb,5)<<3);
  info->secondstages[j]=cascade;

  acc+=icount(cascade);
 }

 for(j=0;j<acc;j++){
  int list=oggpack_read24(opb,8);
  if(list>=ci->books)
   goto errout;
  info->booklist[j]=list;
 }

 return(info);

errout:
 res012_free_info(info);
 return(NULL);
}

static vorbis_info_residue *res0_unpack(vorbis_info *vi,oggpack_buffer *opb)
{
 vorbis_info_residue0 *info=_012_unpack(vi,opb);
 if(info){
  info->decodepart=vorbis_book_decodevs_add;
  info->setpart=vorbis_book_decodevs_set;
  return (info);
 }
 return (NULL);
}

static vorbis_info_residue *res1_unpack(vorbis_info *vi,oggpack_buffer *opb)
{
 vorbis_info_residue0 *info=_012_unpack(vi,opb);
 if(info){
  info->decodepart=vorbis_book_decodev_add;
  info->setpart=vorbis_book_decodev_set;
  return (info);
 }
 return (NULL);
}

static vorbis_info_residue *res2_unpack(vorbis_info *vi,oggpack_buffer *opb)
{
 vorbis_info_residue0 *info=_012_unpack(vi,opb);
 if(info){
  info->decodepart=(res01_decodev_t *)&vorbis_book_decodevv_add;
  info->setpart   =(res01_decodev_t *)&vorbis_book_decodevv_set;
  return (info);
 }
 return (NULL);
}

static vorbis_look_residue *res012_look(vorbis_dsp_state *vd,vorbis_info_residue *vr)
{
 vorbis_info_residue0 *info=(vorbis_info_residue0 *)vr;
 vorbis_info          *vi=vd->vi;
 codec_setup_info     *ci=vi->codec_setup;

 int j,k,acc=0,partwordnum;
 int dim;
 int maxstage=0;

 // if there are submaps we cannot use 'setpart' (vorbis_book_decodexx_set)
 // (but it seems the existent Vorbis versions have no submaps (1 submap only))
 for(j=0;j<ci->maps;j++){
  if(ci->map_type[j]==0){
   vorbis_info_mapping0 *vim0=ci->map_param[j];
   if(vim0->submaps>1){
    info->setpart=info->decodepart;
    break;
   }
  }

 }

 info->phrasebook=ci->fullbooks+info->groupbook;
 dim=info->phrasebook->dim;

 for(j=0;j<info->partitions;j++){
  int stages=ilog(info->secondstages[j]);
  if(stages>maxstage)
   maxstage=stages;
 }

 // we allocate a little more memory, but this is the decoding order (->faster decoding (?))
 info->partbooks=_ogg_calloc(maxstage,sizeof(*info->partbooks));

 for(k=0;k<maxstage;k++)
  info->partbooks[k]=_ogg_calloc(info->partitions,sizeof(*info->partbooks[k]));

 for(j=0;j<info->partitions;j++){
  int stages=ilog(info->secondstages[j]);
  for(k=0;k<stages;k++)
   if(info->secondstages[j]&(1<<k))
    info->partbooks[k][j]=ci->fullbooks+info->booklist[acc++];
 }

 info->look_partvals=rint(pow((float)info->partitions,(float)dim));
 info->stages=maxstage;
 info->decodemap=_ogg_malloc(info->look_partvals*sizeof(*info->decodemap));
 for(j=0;j<info->look_partvals;j++){
  long val=j;
  long mult=info->look_partvals/info->partitions;
  info->decodemap[j]=_ogg_malloc(dim*sizeof(*info->decodemap[j]));
  for(k=0;k<dim;k++){
   long deco=val/mult;
   val-=deco*mult;
   mult/=info->partitions;
   info->decodemap[j][k]=deco;
  }
 }

 info->dim=dim;
 info->info_partvals=info->len/info->samples_per_partition;
 partwordnum=(info->info_partvals+info->dim-1)/info->dim;

 info->partwordp=_ogg_malloc(vi->channels*partwordnum*sizeof(*(info->partwordp)));

 return((vorbis_look_residue *)info);
}

static int res01_inverse(vorbis_block *vb,vorbis_info_residue *vl,ogg_double_t **in,unsigned int channels)
{
 vorbis_info_residue0 *info=(vorbis_info_residue0 *)vl;
 unsigned int i,ch,k,s,injoffset,stagebitpos;
 oggpack_buffer *vbopb=&vb->opb;
 int **wordpp;
 codebook ***partbookp;

 injoffset=info->begin;
 wordpp=info->partwordp;
 partbookp=&info->partbooks[0];

 i=info->info_partvals;
 do{
  int **wordpch=wordpp;
  ch=channels;
  do{
   int temp=vorbis_book_decode((info->phrasebook)->decode_tree,vbopb);
   if(temp<0)
    goto eopbreak;
   *wordpch=info->decodemap[temp];
   if(*wordpch==NULL)
    goto errout;
   wordpch++;
  }while(--ch);

  k=0;
  do{
   wordpch=wordpp;
   for(ch=0;ch<channels;ch++){
    int pwjlk=(*wordpch)[k];
    if(info->secondstages[pwjlk]&1){
     codebook *stagebook=(*partbookp)[pwjlk];
     if(stagebook)
      if(info->setpart(stagebook,in[ch]+injoffset,vbopb,info->samples_per_partition)<0)
       goto eopbreak;
    }
    wordpch++;
   }
   injoffset+=info->samples_per_partition;
  }while(--i && ++k<info->dim);
  wordpp+=channels;
 }while(i);

 partbookp++;

 s=info->stages;
 if(--s){
  stagebitpos=2;
  do{
   injoffset=info->begin;
   wordpp=info->partwordp;
   i=info->info_partvals;
   do{
    k=0;
    do{
     int **wordpch=wordpp;
     for(ch=0;ch<channels;ch++){
      int pwjlk=(*wordpch)[k];
      if(info->secondstages[pwjlk]&stagebitpos){
       codebook *stagebook=(*partbookp)[pwjlk];
       if(stagebook){
	if(info->decodepart(stagebook,in[ch]+injoffset,vbopb,info->samples_per_partition)<0)
	 goto eopbreak;
       }
      }
      wordpch++;
     }
     injoffset+=info->samples_per_partition;
    }while(--i && ++k<info->dim);
    wordpp+=channels;
   }while(i);
   partbookp++;
   stagebitpos<<=1;
  }while(--s);
 }

 errout:
 eopbreak:
 return(0);
}

static int res2_inverse(vorbis_block *vb,vorbis_info_residue *vl,ogg_double_t **in,unsigned int channels)
{
 vorbis_info_residue0 *info=(vorbis_info_residue0 *)vl;
 codebook ***partbookps=&info->partbooks[0];
 oggpack_buffer *vbopb=&vb->opb;
 unsigned int s;

 {
  int **wordpp=info->partwordp;
  codebook **partbookpk=*partbookps++;
  unsigned int injoffset=info->begin,i=info->info_partvals;
  do{
   int *wordpk;
   unsigned int k;
   int temp=vorbis_book_decode(info->phrasebook->decode_tree,vbopb);
   if(temp<0)
    goto eopbreak;
   *wordpp=info->decodemap[temp];
   if(*wordpp==NULL)
    goto errout;
   wordpk=*wordpp;
   k=info->dim;
   do{
    int pwlk=*wordpk++;
    if(info->secondstages[pwlk]&1){
     codebook *stagebook=partbookpk[pwlk];
     if(stagebook){
      if(((res2_decodev_t *)info->setpart)(stagebook,in,injoffset,channels,vbopb,info->samples_per_partition)<0)
       goto eopbreak;
     }
    }
    injoffset+=info->samples_per_partition;
   }while(--i && --k);
   wordpp++;
  }while(i);
 }

 s=info->stages;
 if(--s){
  unsigned int stagebitpos=2;
  do{
   int **wordpp=info->partwordp;
   codebook **partbookpk=*partbookps;
   unsigned int injoffset=info->begin,i=info->info_partvals;
   do{
    int *wordpk=*wordpp;
    unsigned int k=info->dim;
    do{
     int pwlk=*wordpk++;
     if(info->secondstages[pwlk]&stagebitpos){
      codebook *stagebook=partbookpk[pwlk];
      if(stagebook){
       if(((res2_decodev_t *)info->decodepart)(stagebook,in,injoffset,channels,vbopb,info->samples_per_partition)<0)
	goto eopbreak;
      }
     }
     injoffset+=info->samples_per_partition;
    }while(--i && --k);
    wordpp++;
   }while(i);
   partbookps++;
   stagebitpos<<=1;
  }while(--s);
 }

 errout:
 eopbreak:
 return(0);
}

vorbis_func_residue residue0_exportbundle={
  &res0_unpack,
  &res012_look,
  &res012_free_info,
  &res012_free_look,
  &res01_inverse
};

vorbis_func_residue residue1_exportbundle={
  &res1_unpack,
  &res012_look,
  &res012_free_info,
  &res012_free_look,
  &res01_inverse
};

vorbis_func_residue residue2_exportbundle={
  &res2_unpack,
  &res012_look,
  &res012_free_info,
  &res012_free_look,
  &res2_inverse
};
