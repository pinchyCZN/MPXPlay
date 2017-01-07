/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2001             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: basic shared codebook operations
 last mod: $Id: sharedbook.c,v 1.23 2003/02/10 00:00:00 PDSoft Exp $

 ********************************************************************/

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ogg.h"
#include "os.h"
#include "codec.h"
#include "codebook.h"
#include "scales.h"

int _ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

#define VQ_FEXP 10
#define VQ_FMAN 21
#define VQ_FEXP_BIAS 768

static ogg_float_t _float32_unpack(long val)
{
 double mant=val&0x1fffff;
 int    sign=val&0x80000000;
 long   exp =(val&0x7fe00000L)>>VQ_FMAN;
 if(sign)
  mant= -mant;
 return(ldexp(mant,exp-(VQ_FMAN-1)-VQ_FEXP_BIAS));
}

int vorbis_staticbook_unpack(oggpack_buffer *opb,static_codebook *s)
{
 long i,j;

 if(!s)
  return(-1);

 memset(s,0,sizeof(*s));
 s->allocedp=1;

 if(oggpack_read24(opb,24)!=0x564342)
  goto _eofout;

 s->dim=oggpack_read24(opb,16);
 s->entries=oggpack_read24(opb,24);

 if(s->entries==-1)
  goto _eofout;

 switch((int)oggpack_read1(opb)){
  case 0:
    s->lengthlist=_ogg_malloc(sizeof(*s->lengthlist)*s->entries);
    if(oggpack_read1(opb)){
     for(i=0;i<s->entries;i++){
      if(oggpack_read1(opb)){
       long num=oggpack_read24(opb,5);
       if(num==-1)
	goto _eofout;
       s->lengthlist[i]=num+1;
      }else
       s->lengthlist[i]=0;
     }
    }else{
     for(i=0;i<s->entries;i++){
      long num=oggpack_read24(opb,5);
      if(num==-1)
       goto _eofout;
      s->lengthlist[i]=num+1;
     }
    }

    break;
  case 1:
    {
     long length=oggpack_read24(opb,5)+1;
     s->lengthlist=_ogg_malloc(sizeof(*s->lengthlist)*s->entries);

     for(i=0;i<s->entries;){
      long num=oggpack_read32(opb,_ilog(s->entries-i));
      if(num==-1)
       goto _eofout;
      for(j=0;j<num && i<s->entries;j++,i++)
       s->lengthlist[i]=length;
      length++;
     }
    }
    break;
  default:
    return(-1);
 }

 switch((s->maptype=oggpack_read24(opb,4))){
  case 0:break;
  case 1:
  case 2:

    s->q_min=oggpack_read32(opb,32);
    s->q_delta=oggpack_read32(opb,32);
    s->q_quant=oggpack_read24(opb,4)+1;
    s->q_sequencep=oggpack_read1(opb);

    {
      int quantvals=0;
      switch(s->maptype){
      case 1:
	quantvals=_book_maptype1_quantvals(s);
	break;
      case 2:
	quantvals=s->entries*s->dim;
	break;
      }

      s->quantlist=_ogg_malloc(sizeof(*s->quantlist)*quantvals);
      for(i=0;i<quantvals;i++)
	s->quantlist[i]=oggpack_read32(opb,s->q_quant);

      if(quantvals && (s->quantlist[quantvals-1]==-1))
       goto _eofout;
    }
    break;
  default:
    goto _errout;
 }

 return(0);

 _errout:
 _eofout:
  vorbis_staticbook_clear(s);
  return(-1);
}

static ogg_uint32_t *_make_words(long *l,long n,long used_entries)
{
 unsigned long i,j,ecounter=0;
 ogg_uint32_t marker[33];
 ogg_uint32_t *r=_ogg_malloc(used_entries*sizeof(*r));

 if(!r)
  return r;

 memset(marker,0,sizeof(marker));

 for(i=0;i<n;i++){
  long length=l[i];
  if(length>0){
   long entry=marker[length],temp;

   if(length<32 && (entry>>length)){
    _ogg_free(r);
    return(NULL);
   }

   temp=0;
   for(j=0;j<length;j++){
    temp<<=1;
    temp|=(entry>>j)&1;
   }
   r[ecounter]=temp;

   for(j=length;j>0;j--){
    if(marker[j]&1){
     if(j==1)
      marker[1]++;
     else
      marker[j]=marker[j-1]<<1;
     break;
    }
    marker[j]++;
   }

   for(j=length+1;j<33;j++){
    if((marker[j]>>1) == entry){
     entry=marker[j];
     marker[j]=marker[j-1]<<1;
    }else
     break;
   }

   ecounter++;
  }
 }

 return(r);
}

//#define SHARBOOK_DEBUG 1

#ifdef SHARBOOK_DEBUG
 #include <stdio.h>
 static unsigned int taberror,allerror,atn;
#endif

static decode_aux *_make_decode_tree(codebook *c,static_codebook *s,unsigned int bit_usage[CODEBOOK_MAX_BITS])
{
 unsigned long top,i,j,n,ccount;
 ogg_uint32_t *codelist;
 short *ptr0,*ptr1;
 decode_aux *t=_ogg_calloc(1,sizeof(*t));

 if(!t)
  return(t);

#ifdef SHARBOOK_DEBUG
 top=0;
#endif

 ptr0=t->ptr0=_ogg_calloc(c->entries,sizeof(*(t->ptr0)));
 ptr1=t->ptr1=_ogg_calloc(c->entries,sizeof(*(t->ptr1)));
 if(!ptr0 || !ptr1){
  codelist=NULL;
  goto tree_err_out;
 }

 codelist=_make_words(s->lengthlist,s->entries,c->entries);
 if(!codelist)
  goto tree_err_out;

 ccount=0;
 top=0;
 for(i=0;i<s->entries;i++){
  if(s->lengthlist[i]>0){
   long ptr=0;
   for(j=0;j<s->lengthlist[i]-1;j++){
    int bit=(codelist[ccount]>>j)&1;
    if(!bit){
     if(!ptr0[ptr])
      ptr0[ptr]= ++top;
     ptr=ptr0[ptr];
    }else{
     if(!ptr1[ptr])
      ptr1[ptr]= ++top;
     ptr=ptr1[ptr];
    }
    if(top>=c->entries)  // I hope this never happens
     goto tree_err_out;
   }
   if(!((codelist[ccount]>>j)&1))
    ptr0[ptr]=-i;
   else
    ptr1[ptr]=-i;
   ccount++;
  }
 }
 _ogg_free(codelist);
 codelist=NULL;

 t->tab_maxlen=0;

 for(i=1;i<=c->maxbits;i++)   // first bit-len
  if(bit_usage[i])
   break;
 j=i;

 for(i=j+1;i<=c->maxbits;i++) // next bit-len
  if(bit_usage[i])
   break;

 if((i-j)>=5)
  t->tab_maxlen=j;

 if(!t->tab_maxlen){
  n=0;
  for(i=1;i<=c->maxbits;i++){
   if(bit_usage[i]){
    n+=bit_usage[i];
    j=c->maxbits*c->entries/(i*n);
    if(j<c->maxbits){
     //fprintf(stdout,"j:%2d n:%d",j,n);
     break;
    }
   }
  }
  if(i>CODEBOOK_LOOKUP_BITS_MAX)
   i=CODEBOOK_LOOKUP_BITS_MAX;
  t->tab_maxlen=i;

  // original with minor modifications
  /*t->tab_maxlen=_ilog(c->entries);
  if(t->tab_maxlen>4)
   t->tab_maxlen-=4;
  if(t->tab_maxlen<6)
   t->tab_maxlen=6;
  if(t->tab_maxlen>9)
   t->tab_maxlen=9;
  if(t->tab_maxlen>c->maxbits)
   t->tab_maxlen=c->maxbits;*/

  // fastest, but uses too much memory
  /*t->tab_maxlen = _ilog(s->entries);
  if(t->tab_maxlen<c->maxbits){
   if(c->maxbits<=(CODEBOOK_LOOKUP_BITS_MAX/2))
    t->tab_maxlen=c->maxbits;
  }else{
   t->tab_maxlen=c->maxbits;

   if(t->tab_maxlen>CODEBOOK_LOOKUP_BITS_MAX)
    t->tab_maxlen=CODEBOOK_LOOKUP_BITS_MAX;
  }*/
 }

 n = 1<<t->tab_maxlen;
 t->tab_ptr    = _ogg_calloc(n,sizeof(*t->tab_ptr));
 t->tab_codelen= _ogg_calloc(n,sizeof(*t->tab_codelen));

 if(!t->tab_ptr || !t->tab_codelen)
  goto tree_err_out;

#ifdef SHARBOOK_DEBUG
  atn+=n;
  taberror=0;
#endif

 for (i = 0; i < n; i++) {
  long p=0;
  for (j=0; (j<t->tab_maxlen) && (p>0 || j==0); j++){
   if(i & (1 << j))
    p = ptr1[p];
   else
    p = ptr0[p];
  }

  t->tab_ptr[i] = p;
  t->tab_codelen[i] = j;

#ifdef SHARBOOK_DEBUG
  if(p>0)
   taberror++;
#endif
 }

#ifdef SHARBOOK_DEBUG
 allerror+=taberror;
 fprintf(stdout,"se:%4d ue:%4d d:%2d tn:%2d mb:%2d buf:%5d err:%4d allerr:%5d \n",
   s->entries,c->entries,s->dim,t->tab_maxlen,c->maxbits,atn,taberror,allerror);
 //fprintf(stdout,"se:%4d ue:%4d d:%2d tn:%2d mb:%2d buf:%5d err:%4d allerr:%5d top:%d\n",
 //  s->entries,c->entries,s->dim,t->tab_maxlen,c->maxbits,atn,taberror,allerror,top);
 taberror=0;
#endif

 return(t);

tree_err_out:
 if(codelist) _ogg_free(codelist);
 if(t->ptr0)  _ogg_free(t->ptr0);
 if(t->ptr1)  _ogg_free(t->ptr1);
 if(t->tab_ptr)    _ogg_free(t->tab_ptr);
 if(t->tab_codelen) _ogg_free(t->tab_codelen);
 _ogg_free(t);
 return (NULL);
}

long _book_maptype1_quantvals(const static_codebook *s)
{
 long vals=floor(pow((float)s->entries,1.f/(float)s->dim));

 while(1){
  long acc=1;
  long acc1=1;
  int i;
  for(i=0;i<s->dim;i++){
   acc*=vals;
   acc1*=vals+1;
  }
  if(acc<=s->entries && acc1>s->entries){
   return(vals);
  }else{
   if(acc>s->entries){
    vals--;
   }else{
    vals++;
   }
  }
 }
}

static unsigned int _book_unquantize(static_codebook *s,codebook *c)
{
 unsigned long j,k;
 if(s->maptype==1 || s->maptype==2){
  int quantvals;
  ogg_float_t mindel=_float32_unpack(s->q_min);
  ogg_float_t delta=_float32_unpack(s->q_delta);
  ogg_float_t *r=_ogg_calloc(s->entries*s->dim,sizeof(*r));

  if(!r)
   return 0;
  c->valuelist=r;

  switch(s->maptype){
    case 1:
      quantvals=_book_maptype1_quantvals(s);
      for(j=0;j<s->entries;j++){
	ogg_float_t last=0.f;
	int indexdiv=1;
	for(k=0;k<s->dim;k++){
	 int index= (j/indexdiv)%quantvals;
	 ogg_float_t val=s->quantlist[index];
	 val=fabs(val)*delta+mindel+last;
	 if(s->q_sequencep)
	  last=val;
	 *r++=val;
	 indexdiv*=quantvals;
	}
      }
      break;
    case 2:
      for(j=0;j<s->entries;j++){
	long *sqlp=&s->quantlist[0];
	ogg_float_t last=0.f;
	for(k=0;k<s->dim;k++){
	 ogg_float_t val=*sqlp++;//s->quantlist[j*s->dim+k];
	 val=fabs(val)*delta+mindel+last;
	 if(s->q_sequencep)
	  last=val;
	 *r++=val;
	}
      }
      break;
  }
 }
 return 1;
}

void vorbis_staticbook_clear(static_codebook *s)
{
 if(s->allocedp){
  if(s->quantlist)  _ogg_free(s->quantlist);
  if(s->lengthlist) _ogg_free(s->lengthlist);
  memset(s,0,sizeof(*s));
 }
}

void vorbis_staticbook_destroy(static_codebook *s)
{
 if(s->allocedp){
  vorbis_staticbook_clear(s);
  _ogg_free(s);
 }
}

void vorbis_book_clear(codebook *c)
{
 if(c->decode_tree){
  struct decode_aux *t=c->decode_tree;
  if(t->tab_ptr)     _ogg_free(t->tab_ptr);
  if(t->tab_codelen) _ogg_free(t->tab_codelen);
  if(t->ptr0) _ogg_free(t->ptr0);
  if(t->ptr1) _ogg_free(t->ptr1);
  _ogg_free(c->decode_tree);
 }
 if(c->valuelist)
  _ogg_free(c->valuelist);
 memset(c,0,sizeof(*c));

#ifdef SHARBOOK_DEBUG
 allerror=atn=0;
#endif
}

int vorbis_book_init_decode(codebook *c,static_codebook *s)
{
 unsigned int i;
 unsigned int bit_usage[CODEBOOK_MAX_BITS];

 memset(&bit_usage[0],0,CODEBOOK_MAX_BITS*sizeof(unsigned int));
 memset(c,0,sizeof(*c));
 c->dim=s->dim;

 for(i=0;i<s->entries;i++){
  int b=s->lengthlist[i];
  if(b){
   if(b>c->maxbits)
    c->maxbits=b;
   bit_usage[b]++;
   c->entries++;
  }
 }

 if(c->entries){
  if(c->entries>CODEBOOK_MAX_ENTRIES)
   goto err_out;
  if(c->maxbits>CODEBOOK_MAX_BITS)
   goto err_out;

  c->decode_tree=_make_decode_tree(c,s,bit_usage);

  if(c->decode_tree==NULL)
   goto err_out;
  if(!_book_unquantize(s,c))
   goto err_out;
 }

 return(0);
err_out:
 vorbis_book_clear(c);
 return(-1);
}
