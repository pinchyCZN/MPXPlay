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

 function: PCM data vector blocking, windowing and dis/reassembly
 last mod: $Id: block.c,v 1.70 2007/02/12 00:00:00 PDSoft Exp $

 Handle windowing, overlap-add, etc of the PCM vectors.  This is made
 more amusing by Vorbis' current two allowed block sizes.
 
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ogg.h"
#include "codec.h"
#include "codecint.h"
#include "window.h"
#include "mdct.h"
#include "registry.h"

int vorbis_block_init(vorbis_dsp_state *vd, vorbis_block *vb)
{
 vorbis_info      *vi=vd->vi;
 codec_setup_info *ci=vi->codec_setup;
 unsigned int i,s;

 _ogg_memset(vb,0,sizeof(*vb));
 vb->vd=vd;

 s=ci->blocksizes[1];

 vb->pcm=_ogg_malloc(sizeof(*vb->pcm)*vi->channels);
 if(!vb->pcm)
  return -1;
 for(i=0;i<vi->channels;i++){
  vb->pcm[i]=_ogg_malloc(s*sizeof(*(vb->pcm[i])));
  if(!vb->pcm[i])
   return -1;
 }

 return (0);
}

int vorbis_block_clear(vorbis_block *vb)
{
 if(vb){
  if(vb->pcm && vb->vd && vb->vd->vi){
   unsigned int i;
   vorbis_info *vi=vb->vd->vi;
   for(i=0;i<vi->channels;i++)
    if(vb->pcm[i])
     _ogg_free(vb->pcm[i]);
   _ogg_free(vb->pcm);
  }
  _ogg_memset(vb,0,sizeof(*vb));
 }
 return (0);
}

//------------------------------------------------------------------------
static int ilog2(unsigned int v)
{
 int ret=0;
 if(v)
  --v;
 while(v){
  ret++;
  v>>=1;
 }
 return(ret);
}

static int _vds_shared_init(vorbis_dsp_state *v,vorbis_info *vi)
{
 int i;
 codec_setup_info *ci=vi->codec_setup;
 backend_lookup_state *b=NULL;

 _ogg_memset(v,0,sizeof(*v));
 b=v->backend_state=_ogg_calloc(1,sizeof(*b));
 if(!b)
  return -1;

 v->vi=vi;
 b->modebits=ilog2(ci->modes);

 b->transform[0]=oggdec_mdct_init(ci->blocksizes[0]);
 b->transform[1]=oggdec_mdct_init(ci->blocksizes[1]);
 if(!b->transform[0] || !b->transform[1])
  return -1;

 b->window[0]=_vorbis_window_get(ci->blocksizes[0]/2);
 b->window[1]=_vorbis_window_get(ci->blocksizes[1]/2);

 if(!b->window[0] || !b->window[1])
  return -1;

 if(!ci->fullbooks){
  ci->fullbooks=_ogg_calloc(ci->books,sizeof(*ci->fullbooks));
  for(i=0;i<ci->books;i++){
   if(vorbis_book_init_decode(ci->fullbooks+i,ci->book_param[i])<0)
    return -1;
  }
  for(i=0;i<ci->books;i++){
   vorbis_staticbook_destroy(ci->book_param[i]);
   ci->book_param[i]=NULL;
  }
 }

 v->pcm_storage=ci->blocksizes[1];
 v->pcm=_ogg_malloc(vi->channels*sizeof(*v->pcm));
 v->pcmret=_ogg_malloc(vi->channels*sizeof(*v->pcmret));

 if(!v->pcm || !v->pcmret)
  return -1;

 for(i=0;i<vi->channels;i++){
  v->pcm[i]=_ogg_malloc(v->pcm_storage*sizeof(*v->pcm[i]));
  if(!v->pcm[i])
   return -1;
 }

 b->flr=_ogg_calloc(ci->floors,sizeof(*b->flr));
 b->residue=_ogg_calloc(ci->residues,sizeof(*b->residue));

 if(!b->flr || !b->residue)
  return -1;

 for(i=0;i<ci->floors;i++)
  b->flr[i]=_floor_P[ci->floor_type[i]]->look(v,ci->floor_param[i]);

 for(i=0;i<ci->residues;i++)
  b->residue[i]=_residue_P[ci->residue_type[i]]->look(v,ci->residue_param[i]);

 return(0);
}

void vorbis_dsp_clear(vorbis_dsp_state *v)
{
 int i;
 if(v){
  vorbis_info *vi=v->vi;
  codec_setup_info *ci=(vi?vi->codec_setup:NULL);
  backend_lookup_state *b=v->backend_state;

  if(b){
   if(b->window[0])
    _ogg_free(b->window[0]);
   if(b->window[1])
    _ogg_free(b->window[1]);

   oggdec_mdct_clear(b->transform[0]);
   oggdec_mdct_clear(b->transform[1]);
   if(b->flr && ci){
    for(i=0;i<ci->floors;i++)
     _floor_P[ci->floor_type[i]]->free_look(b->flr[i]);
    _ogg_free(b->flr);
   }
   if(b->residue && ci){
    for(i=0;i<ci->residues;i++)
     _residue_P[ci->residue_type[i]]->free_look(b->residue[i]);
    _ogg_free(b->residue);
   }
   _ogg_free(b);
  }

  if(v->pcm && vi){
   for(i=0;i<vi->channels;i++)
    if(v->pcm[i])
     _ogg_free(v->pcm[i]);
   _ogg_free(v->pcm);
  }
  if(v->pcmret)
   _ogg_free(v->pcmret);

  _ogg_memset(v,0,sizeof(*v));
 }
}

void vorbis_synthesis_restart(vorbis_dsp_state *v,vorbis_info *vi)
{
 codec_setup_info *ci=vi->codec_setup;
 unsigned int i;

 v->lW=0;
 v->W=0;
 v->centerW=ci->blocksizes[1]/2;
 v->pcm_current=v->centerW;

 v->pcm_returned=-1;
 v->granulepos=-1;
 v->sequence=-1;
 v->eofflag=0;

 for(i=0;i<vi->channels;i++)
  _ogg_memset(v->pcm[i],0,v->pcm_storage*sizeof(*v->pcm[i]));
}

int vorbis_synthesis_init(vorbis_dsp_state *v,vorbis_info *vi)
{
 if(_vds_shared_init(v,vi)<0)
  return -1;

 vorbis_synthesis_restart(v,vi);

 return(0);
}

int vorbis_synthesis_blockin(vorbis_dsp_state *v,vorbis_block *vb)
{
 vorbis_info *vi=v->vi;
 codec_setup_info *ci=vi->codec_setup;
 backend_lookup_state *b=v->backend_state;
 unsigned int ch;
 unsigned int thisCenter;
 unsigned int prevCenter;
 unsigned int n,n0,n1;

 if(v->pcm_current>v->pcm_returned && v->pcm_returned!=-1){
  vorbis_synthesis_restart(v,vi);
  //return(OV_EINVAL);
 }

 v->lW=v->W;
 v->W=vb->W;
 v->nW=-1;

 if(v->sequence+1 != vb->sequence)
  v->granulepos=-1;

 v->sequence=vb->sequence;

 n=ci->blocksizes[v->W]/2;
 n0=ci->blocksizes[0]/2;
 n1=ci->blocksizes[1]/2;

 if(v->centerW){
  thisCenter=n1;
  prevCenter=0;
  v->centerW=0;
 }else{
  thisCenter=0;
  prevCenter=n1;
  v->centerW=n1;
 }

 for(ch=0;ch<vi->outchannels;ch++){
  ogg_double_t *vbpcmch=vb->pcm[ch];
  if(v->lW){
   if(v->W)
    vorbis_fmulwin_add_block(v->pcm[ch]+prevCenter,vbpcmch,b->window[1],n1);
   else{
#ifdef MPXPLAY
    vorbis_fscale_block(v->pcm[ch]+prevCenter,n1/2-n0/2,32768.0F);
#endif
    vorbis_fmulwin_add_block(v->pcm[ch]+prevCenter+n1/2-n0/2,vbpcmch,b->window[0],n0);
#ifdef MPXPLAY
    vorbis_fscale_block(v->pcm[ch]+prevCenter+n1/2+n0/2,n1/2-n0/2,32768.0F);
#endif
   }
  }else{
   if(v->W){
    ogg_double_t *pcm=v->pcm[ch]+prevCenter;
    const unsigned int i=n1/2-n0/2;
    ogg_double_t *p=vbpcmch+i;
    vorbis_fmulwin_add_block(pcm,p,b->window[0],n0);
#ifdef MPXPLAY
    vorbis_fcopy_and_scale_block(pcm+n0,p+n0,i,32768.0F);
#else
    vorbis_fcopy_block(pcm+n0,p+n0,i);
#endif
   }else
    vorbis_fmulwin_add_block(v->pcm[ch]+prevCenter,vbpcmch,b->window[0],n0);
  }
  vorbis_fcopy_block(v->pcm[ch]+thisCenter,vbpcmch+n,n);
 }

 if(v->pcm_returned==-1){
  v->pcm_returned=thisCenter;
  v->pcm_current=thisCenter;
 }else{
  v->pcm_returned=prevCenter;
  v->pcm_current=prevCenter+ci->blocksizes[v->lW]/4+ci->blocksizes[v->W]/4;
 }

 if(v->granulepos==-1){
  if(vb->granulepos!=-1){
   v->granulepos=vb->granulepos;
  }
 }else{
  v->granulepos+=ci->blocksizes[v->lW]/4+ci->blocksizes[v->W]/4;
  if(vb->granulepos!=-1 && v->granulepos!=vb->granulepos){
   if(v->granulepos>vb->granulepos){
    long extra=v->granulepos-vb->granulepos;

    if(vb->eofflag){
     v->pcm_current-=extra;
    }else
     if(vb->sequence == 1){
      v->pcm_returned+=extra;
      if(v->pcm_returned>v->pcm_current)
       v->pcm_returned=v->pcm_current;
     }
   }
   v->granulepos=vb->granulepos;
  }
 }

 if(vb->eofflag)
  v->eofflag=1;

 return(0);
}

int vorbis_synthesis_pcmout(vorbis_dsp_state *v,ogg_double_t ***pcm)
{
 vorbis_info *vi=v->vi;
 if(v->pcm_returned>-1 && v->pcm_returned<v->pcm_current){
  unsigned int i;
  for(i=0;i<vi->outchannels;i++)
   v->pcmret[i]=v->pcm[i]+v->pcm_returned;
  *pcm=v->pcmret;
  return(v->pcm_current-v->pcm_returned);
 }
 return(0);
}

int vorbis_synthesis_read(vorbis_dsp_state *v,int n)
{
 if(n && ((v->pcm_returned+n)>v->pcm_current)){
  v->pcm_returned=v->pcm_current;
  return(OV_EINVAL);
 }
 v->pcm_returned+=n;
 return(0);
}

//*************************************************************************

#ifdef MPXPLAY
void vorbis_fscale_block(ogg_double_t *a,unsigned int len,float scale)
{
 do{
  if(len>=32){
   a[ 0]*=scale; a[ 1]*=scale; a[ 2]*=scale; a[ 3]*=scale;
   a[ 4]*=scale; a[ 5]*=scale; a[ 6]*=scale; a[ 7]*=scale;
   a[ 8]*=scale; a[ 9]*=scale; a[10]*=scale; a[11]*=scale;
   a[12]*=scale; a[13]*=scale; a[14]*=scale; a[15]*=scale;
   a[16]*=scale; a[17]*=scale; a[18]*=scale; a[19]*=scale;
   a[20]*=scale; a[21]*=scale; a[22]*=scale; a[23]*=scale;
   a[24]*=scale; a[25]*=scale; a[26]*=scale; a[27]*=scale;
   a[28]*=scale; a[29]*=scale; a[30]*=scale; a[31]*=scale;
   a+=32;
   len-=32;
  }else{
   if(len)
    do{
     *a++ *= scale;
    }while(--len);
  }
 }while(len);
}
#endif

#ifdef OGGDEC_DOUBLE_PRECISION

void vorbis_fclear_block(ogg_double_t *a,unsigned int len)
{
 const ogg_double_t zero=0.0;
 do{
  if(len>=32){
   a[ 0]=zero; a[ 1]=zero; a[ 2]=zero; a[ 3]=zero;
   a[ 4]=zero; a[ 5]=zero; a[ 6]=zero; a[ 7]=zero;
   a[ 8]=zero; a[ 9]=zero; a[10]=zero; a[11]=zero;
   a[12]=zero; a[13]=zero; a[14]=zero; a[15]=zero;
   a[16]=zero; a[17]=zero; a[18]=zero; a[19]=zero;
   a[20]=zero; a[21]=zero; a[22]=zero; a[23]=zero;
   a[24]=zero; a[25]=zero; a[26]=zero; a[27]=zero;
   a[28]=zero; a[29]=zero; a[30]=zero; a[31]=zero;
   a+=32;
   len-=32;
  }else{
   if(len)
    do{
     *a++ = zero;
    }while(--len);
  }
 }while(len);
}

#else

void vorbis_fclear_block(ogg_double_t *a,unsigned int len)
{
 const long zero=0;
 do{
  if(len>=32){
   *((long *)&a[ 0])=zero; *((long *)&a[ 1])=zero; *((long *)&a[ 2])=zero;
   *((long *)&a[ 3])=zero; *((long *)&a[ 4])=zero; *((long *)&a[ 5])=zero;
   *((long *)&a[ 6])=zero; *((long *)&a[ 7])=zero; *((long *)&a[ 8])=zero;
   *((long *)&a[ 9])=zero; *((long *)&a[10])=zero; *((long *)&a[11])=zero;
   *((long *)&a[12])=zero; *((long *)&a[13])=zero; *((long *)&a[14])=zero;
   *((long *)&a[15])=zero; *((long *)&a[16])=zero; *((long *)&a[17])=zero;
   *((long *)&a[18])=zero; *((long *)&a[19])=zero; *((long *)&a[20])=zero;
   *((long *)&a[21])=zero; *((long *)&a[22])=zero; *((long *)&a[23])=zero;
   *((long *)&a[24])=zero; *((long *)&a[25])=zero; *((long *)&a[26])=zero;
   *((long *)&a[27])=zero; *((long *)&a[28])=zero; *((long *)&a[29])=zero;
   *((long *)&a[30])=zero; *((long *)&a[31])=zero;
   a+=32;
   len-=32;
  }else{
   if(len)
    do{
     *((long *)a) = zero;
     a++;
    }while(--len);
  }
 }while(len);
}

#endif

void vorbis_fcopy_block(ogg_double_t *a,ogg_double_t *b,unsigned int len)
{
 do{
  if(len>=32){
   a[ 0]=b[ 0]; a[ 1]=b[ 1]; a[ 2]=b[ 2]; a[ 3]=b[ 3];
   a[ 4]=b[ 4]; a[ 5]=b[ 5]; a[ 6]=b[ 6]; a[ 7]=b[ 7];
   a[ 8]=b[ 8]; a[ 9]=b[ 9]; a[10]=b[10]; a[11]=b[11];
   a[12]=b[12]; a[13]=b[13]; a[14]=b[14]; a[15]=b[15];
   a[16]=b[16]; a[17]=b[17]; a[18]=b[18]; a[19]=b[19];
   a[20]=b[20]; a[21]=b[21]; a[22]=b[22]; a[23]=b[23];
   a[24]=b[24]; a[25]=b[25]; a[26]=b[26]; a[27]=b[27];
   a[28]=b[28]; a[29]=b[29]; a[30]=b[30]; a[31]=b[31];
   a+=32;
   b+=32;
   len-=32;
  }else{
   if(len)
    do{
     *a++ = *b++;
    }while(--len);
  }
 }while(len);
}

void vorbis_fmulwin_add_block(ogg_double_t *a,ogg_double_t *b,ogg_double_t *w,unsigned int len)
{
 ogg_double_t *w2=w+len-1;
 do{
  if(len>=32){
   a[ 0]= a[ 0]*w2[ -0] + b[ 0]*w[ 0];
   a[ 1]= a[ 1]*w2[ -1] + b[ 1]*w[ 1];
   a[ 2]= a[ 2]*w2[ -2] + b[ 2]*w[ 2];
   a[ 3]= a[ 3]*w2[ -3] + b[ 3]*w[ 3];
   a[ 4]= a[ 4]*w2[ -4] + b[ 4]*w[ 4];
   a[ 5]= a[ 5]*w2[ -5] + b[ 5]*w[ 5];
   a[ 6]= a[ 6]*w2[ -6] + b[ 6]*w[ 6];
   a[ 7]= a[ 7]*w2[ -7] + b[ 7]*w[ 7];
   a[ 8]= a[ 8]*w2[ -8] + b[ 8]*w[ 8];
   a[ 9]= a[ 9]*w2[ -9] + b[ 9]*w[ 9];
   a[10]= a[10]*w2[-10] + b[10]*w[10];
   a[11]= a[11]*w2[-11] + b[11]*w[11];
   a[12]= a[12]*w2[-12] + b[12]*w[12];
   a[13]= a[13]*w2[-13] + b[13]*w[13];
   a[14]= a[14]*w2[-14] + b[14]*w[14];
   a[15]= a[15]*w2[-15] + b[15]*w[15];
   a[16]= a[16]*w2[-16] + b[16]*w[16];
   a[17]= a[17]*w2[-17] + b[17]*w[17];
   a[18]= a[18]*w2[-18] + b[18]*w[18];
   a[19]= a[19]*w2[-19] + b[19]*w[19];
   a[20]= a[20]*w2[-20] + b[20]*w[20];
   a[21]= a[21]*w2[-21] + b[21]*w[21];
   a[22]= a[22]*w2[-22] + b[22]*w[22];
   a[23]= a[23]*w2[-23] + b[23]*w[23];
   a[24]= a[24]*w2[-24] + b[24]*w[24];
   a[25]= a[25]*w2[-25] + b[25]*w[25];
   a[26]= a[26]*w2[-26] + b[26]*w[26];
   a[27]= a[27]*w2[-27] + b[27]*w[27];
   a[28]= a[28]*w2[-28] + b[28]*w[28];
   a[29]= a[29]*w2[-29] + b[29]*w[29];
   a[30]= a[30]*w2[-30] + b[30]*w[30];
   a[31]= a[31]*w2[-31] + b[31]*w[31];
   a+=32;b+=32;w+=32;w2-=32;len-=32;
  }else{
   if(len)
    do{
     *a++= *a * *w2-- + *b++ * *w++;
    }while(--len);
  }
 }while(len);
}

#ifdef MPXPLAY
void vorbis_fcopy_and_scale_block(ogg_double_t *a,ogg_double_t *b,unsigned int len,float scale)
{
 do{
  if(len>=32){
   a[ 0]=b[ 0] * scale;
   a[ 1]=b[ 1] * scale;
   a[ 2]=b[ 2] * scale;
   a[ 3]=b[ 3] * scale;
   a[ 4]=b[ 4] * scale;
   a[ 5]=b[ 5] * scale;
   a[ 6]=b[ 6] * scale;
   a[ 7]=b[ 7] * scale;
   a[ 8]=b[ 8] * scale;
   a[ 9]=b[ 9] * scale;
   a[10]=b[10] * scale;
   a[11]=b[11] * scale;
   a[12]=b[12] * scale;
   a[13]=b[13] * scale;
   a[14]=b[14] * scale;
   a[15]=b[15] * scale;
   a[16]=b[16] * scale;
   a[17]=b[17] * scale;
   a[18]=b[18] * scale;
   a[19]=b[19] * scale;
   a[20]=b[20] * scale;
   a[21]=b[21] * scale;
   a[22]=b[22] * scale;
   a[23]=b[23] * scale;
   a[24]=b[24] * scale;
   a[25]=b[25] * scale;
   a[26]=b[26] * scale;
   a[27]=b[27] * scale;
   a[28]=b[28] * scale;
   a[29]=b[29] * scale;
   a[30]=b[30] * scale;
   a[31]=b[31] * scale;
   a+=32;
   b+=32;
   len-=32;
  }else{
   if(len)
    do{
     *a++ = *b++ * scale;
    }while(--len);
  }
 }while(len);
}
#endif