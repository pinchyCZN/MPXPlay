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

 function: normalized modified discrete cosine transform
	   power of two length transform only [64 <= n ]
 last mod: $Id: mdct.c,v 1.32 2003/10/01 00:00:00 PDSoft Exp $

 Original algorithm adapted long ago from _The use of multirate filter
 banks for coding of high quality digital audio_, by T. Sporer,
 K. Brandenburg and B. Edler, collection of the European Signal
 Processing Conference (EUSIPCO), Amsterdam, June 1992, Vol.1, pp
 211-214

 The below code implements an algorithm that no longer looks much like
 that presented in the paper, but the basic structure remains if you
 dig deep enough to see it.

 This module DOES NOT INCLUDE code to generate/apply the window
 function.  Everybody has their own weird favorite including me... I
 happen to like the properties of y=sin(2PI*sin^2(x)), but others may
 vehemently disagree.

 ********************************************************************/

/* this can also be run as an integer transform by uncommenting a
   define in mdct.h; the integerization is a first pass and although
   it's likely stable for Vorbis, the dynamic range is constrained and
   roundoff isn't done (so it's noisy).  Consider it functional, but
   only a starting point.  There's no point on a machine with an FPU */

#include <string.h>
#include <math.h>
#include "os.h"
#include "mdct.h"

#ifndef USE_AAC_MDCT

#ifdef MDCT_ASM
 #ifdef MDCT_FPU32
  #include "newfunc.h"
 #endif
 //static DATA_TYPE cPI1_8,cPI2_8,cPI3_8;
 static DATA_TYPE cPI1_8=.92387953251128675613F;
 static DATA_TYPE cPI2_8=.70710678118654752441F;
 static DATA_TYPE cPI3_8=.38268343236508977175F;
 static DATA_TYPE half=0.5f;
#endif

void *oggdec_mdct_init(unsigned int n)
{
 int i,n1,n2,log2n,*bitrev;
 DATA_TYPE *T;
 double mpin1,mpin2;

 mdct_lookup *lookup=_ogg_calloc(1,sizeof(mdct_lookup));
 if(!lookup)
  return lookup;

 lookup->n=n;
 lookup->bitrev=bitrev=_ogg_malloc(sizeof(*bitrev)*(n/4));
 lookup->trig=T=_ogg_malloc(sizeof(*T)*(n+n/4));

 n1=n;
 n2=n>>1;
 log2n=lookup->log2n=rint(log((float)n1)/log(2.0F));
 mpin1=M_PI/n1;
 mpin2=M_PI/(n1<<1);

 for(i=0;i<n2;i+=2){
  T[i]     =FLOAT_CONV( cos(mpin1*(i<<1)) );
  T[i+1]   =FLOAT_CONV(-sin(mpin1*(i<<1)) );
  T[n2+i]  =FLOAT_CONV( cos(mpin2*(i+1) ) );
  T[n2+i+1]=FLOAT_CONV( sin(mpin2*(i+1) ) );
 }

 for(i=0;i<n/4;i+=2){
  T[n+i]  =FLOAT_CONV( cos(mpin1*((i<<1)+2))*0.5F);
  T[n+i+1]=FLOAT_CONV(-sin(mpin1*((i<<1)+2))*0.5F);
 }

 {
  int mask=(1<<(log2n-1))-1,i,j;
  int msb=1<<(log2n-2);
  for(i=0;i<n/8;i++){
   int acc=0;
   for(j=0;msb>>j;j++)
    if((msb>>j)&i)
     acc|=1<<j;
#ifdef MDCT_ASM
   bitrev[i*2]=(((~acc)&mask)-1)<<2;  // to avoid shl
   bitrev[i*2+1]=acc<<2;
#else
   bitrev[i*2]=((~acc)&mask)-1;
   bitrev[i*2+1]=acc;
#endif
  }
 }

//#ifdef MDCT_ASM
// cPI1_8=cos(pi*1.0f/8.0f);
// cPI2_8=cos(pi*2.0f/8.0f);
// cPI3_8=cos(pi*3.0f/8.0f);
//#endif

#ifdef MDCT_FORWARD
 lookup->scale=FLOAT_CONV(4.f/n);
#endif
 return lookup;
}

void oggdec_mdct_clear(mdct_lookup *l)
{
 if(l){
  if(l->trig) _ogg_free(l->trig);
  if(l->bitrev) _ogg_free(l->bitrev);
#ifdef MDCT_FORWARD
  if(l->forward_buffer) _ogg_free(l->forward_buffer);
#endif
  free(l);
 }
}

#if defined(MDCT_ASM) && defined(__WATCOMC__) && !defined(MDCT_INTEGERIZED) && !defined(DOUBLE_PRECISION)

void mb16_0(void);
void mb16_1(void);
void mb8_1(void);
void mb8_2(void);

STIN void mdct_butterfly_16(DATA_TYPE *x)
{
#pragma aux mb16_0=\
 "fld  dword ptr cPI2_8"\
"fld  dword ptr  4[eax]"\
 "fld  dword ptr   [eax]"\
 "fld  dword ptr 36[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr  4[eax]"\
 "fld  dword ptr 32[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr   [eax]"\
 "fstp dword ptr 32[eax]"\
 "fstp dword ptr 36[eax]"\
"fld  st(1)"\
 "fadd st,st(1)"\
 "fmul st,st(3)"\
 "fstp dword ptr   [eax]"\
 "fsub"\
 "fmul st,st(1)"\
 "fstp dword ptr  4[eax]"\
"fld  dword ptr 12[eax]"\
 "fld  dword ptr 40[eax]"\
 "fld  dword ptr 44[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 12[eax]"\
 "fld  dword ptr  8[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 40[eax]"\
 "fstp dword ptr 40[eax]"\
 "fstp dword ptr 44[eax]"\
 "fstp dword ptr 12[eax]"\
 "fstp dword ptr  8[eax]"\
 modify[];

#pragma aux mb16_1=\
"fld  dword ptr 48[eax]"\
 "fld  dword ptr 52[eax]"\
 "fld  dword ptr 16[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 48[eax]"\
 "fld  dword ptr 20[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 52[eax]"\
 "fstp dword ptr 52[eax]"\
 "fstp dword ptr 48[eax]"\
"fld  st(1)"\
 "fadd st,st(1)"\
 "fmul st,st(3)"\
 "fstp dword ptr 20[eax]"\
 "fsub"\
 "fmul"\
 "fstp dword ptr 16[eax]"\
"fld  dword ptr 56[eax]"\
 "fld  dword ptr 60[eax]"\
 "fld  dword ptr 24[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 56[eax]"\
 "fld  dword ptr 28[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 60[eax]"\
 "fstp dword ptr 60[eax]"\
 "fstp dword ptr 56[eax]"\
 "fstp dword ptr 28[eax]"\
 "fstp dword ptr 24[eax]"\
 modify[];
 mb16_0();
 mb16_1();

#pragma aux mb8_1=\
 "fld  dword ptr 16[eax]"\
 "fld  dword ptr 24[eax]"\
 "fld  dword ptr  8[eax]"\
 "fsub st(1),st"\
 "fadd dword ptr 24[eax]"\
 "fld  dword ptr   [eax]"\
 "fsub st(3),st"\
 "fadd dword ptr 16[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr 24[eax]"\
 "fsubp st(1),st"\
 "fstp dword ptr 16[eax]"\
 "fld  dword ptr 20[eax]"\
 "fsub dword ptr  4[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr   [eax]"\
 "fsubp st(1),st"\
 "fstp dword ptr  8[eax]"\
 "fld  dword ptr 28[eax]"\
 "fld  dword ptr 12[eax]"\
 "fsub st(1),st"\
 "fadd dword ptr 28[eax]"\
 "fld  dword ptr 20[eax]"\
 "fadd dword ptr  4[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr 28[eax]"\
 "fsubp st(1),st"\
 "fstp dword ptr 20[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr 12[eax]"\
 "fsubrp st(1),st"\
 "fstp dword ptr  4[eax]"\
 modify[];

#pragma aux mb8_2=\
 "fld  dword ptr 48[eax]"\
 "fld  dword ptr 56[eax]"\
 "fld  dword ptr 40[eax]"\
 "fsub st(1),st"\
 "fadd dword ptr 56[eax]"\
 "fld  dword ptr 32[eax]"\
 "fsub st(3),st"\
 "fadd dword ptr 48[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr 56[eax]"\
 "fsubp st(1),st"\
 "fstp dword ptr 48[eax]"\
 "fld  dword ptr 52[eax]"\
 "fsub dword ptr 36[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr 32[eax]"\
 "fsubp st(1),st"\
 "fstp dword ptr 40[eax]"\
 "fld  dword ptr 60[eax]"\
 "fld  dword ptr 44[eax]"\
 "fsub st(1),st"\
 "fadd dword ptr 60[eax]"\
 "fld  dword ptr 52[eax]"\
 "fadd dword ptr 36[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr 60[eax]"\
 "fsubp st(1),st"\
 "fstp dword ptr 52[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fstp dword ptr 44[eax]"\
 "fsubrp st(1),st"\
 "fstp dword ptr 36[eax]"\
 modify[];

 mb8_1();
 mb8_2();
}

/*STIN void mdct_butterfly_16(DATA_TYPE *x)
{
 REG_TYPE r0     = x[1]  - x[9];
 REG_TYPE r1     = x[0]  - x[8];

 x[9]  += x[1];
 x[8]  += x[0];
 x[0]   = MULT_NORM((r0   + r1) * cPI2_8);
 x[1]   = MULT_NORM((r0   - r1) * cPI2_8);

 r0     = x[3]  - x[11];
 r1     = x[10] - x[2];
 x[11] += x[3];
 x[10] += x[2];
 x[3]   = r1;
 x[2]   = r0;

 r0     = x[12] - x[4];
 r1     = x[13] - x[5];
 x[12] += x[4];
 x[13] += x[5];
 x[5]   = MULT_NORM((r0   + r1) * cPI2_8);
 x[4]   = MULT_NORM((r0   - r1) * cPI2_8);

 r0     = x[14] - x[6];
 r1     = x[15] - x[7];
 x[14] += x[6];
 x[15] += x[7];
 x[7]  = r1;
 x[6]  = r0;

 mdct_butterfly_8(x);
 mdct_butterfly_8(x+8);
}*/

/*STIN void mdct_butterfly_8(DATA_TYPE *x)
{
  REG_TYPE r3   = x[4] - x[0];
  REG_TYPE r1   = x[6] - x[2];
  REG_TYPE r0   = x[6] + x[2];
  REG_TYPE r2   = x[4] + x[0];

  x[6] = r0   + r2;
  x[4] = r0   - r2;

  r0   = x[5] - x[1];

  x[0] = r1   + r0;
  x[2] = r1   - r0;

  r2   = x[7] - x[3];
  r1   = x[7] + x[3];
  r0   = x[5] + x[1];

  x[7] = r1   + r0;
  x[5] = r1   - r0;
  x[3] = r2   + r3;
  x[1] = r2   - r3;
}*/

void mb32_0(void);
void mb32_1(void);
void mb32_2(void);
void mb32_3(void);

STIN void mdct_butterfly_32(DATA_TYPE *x)
{
#pragma aux mb32_0=\
 "fld  dword ptr 120[eax]"\
 "fld  dword ptr 124[eax]"\
 "fld  dword ptr  56[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 120[eax]"\
 "fld  dword ptr  60[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 124[eax]"\
 "fstp dword ptr 124[eax]"\
 "fstp dword ptr 120[eax]"\
 "fstp dword ptr  60[eax]"\
 "fstp dword ptr  56[eax]"\
  "fld  dword ptr  88[eax]"\
 "fld  dword ptr  28[eax]"\
 "fld  dword ptr  24[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr  88[eax]"\
 "fld  dword ptr  92[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr  28[eax]"\
 "fstp dword ptr  92[eax]"\
 "fstp dword ptr  88[eax]"\
 "fstp dword ptr  24[eax]"\
 "fstp dword ptr  28[eax]"\
 modify[];
 mb32_0();
 /*r0 = x[30] - x[14];
 r1 = x[31] - x[15];
 x[30] += x[14];
 x[31] += x[15];
 x[15]  = r1;
 x[14]  = r0;

 r0 = x[22] - x[6];
 r1 = x[7]  - x[23];
 x[22] += x[6];
 x[23] += x[7];
 x[6]   = r1;
 x[7]   = r0;*/

#pragma aux mb32_1=\
 "fld  dword ptr cPI2_8"\
 "fld  dword ptr 104[eax]"\
 "fld  dword ptr 108[eax]"\
 "fld  dword ptr  40[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 104[eax]"\
 "fld  dword ptr  44[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 108[eax]"\
 "fstp dword ptr 108[eax]"\
 "fstp dword ptr 104[eax]"\
 "fld st(1)"\
 "fsub st,st(1)"\
 "fmul st,st(3)"\
 "fstp dword ptr  40[eax]"\
 "fadd"\
 "fmul st,st(1)"\
 "fstp dword ptr  44[eax]"\
 "fld  dword ptr   8[eax]"\
 "fld  dword ptr  12[eax]"\
 "fld  dword ptr  72[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr   8[eax]"\
 "fld  dword ptr  76[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr  12[eax]"\
 "fstp dword ptr  76[eax]"\
 "fstp dword ptr  72[eax]"\
 "fld  st(1)"\
 "fadd st,st(1)"\
 "fmul st,st(3)"\
 "fstp dword ptr   8[eax]"\
 "fsubr"\
 "fmul"\
 "fstp dword ptr  12[eax]"\
 modify[];
 mb32_1();
 /*r0 = x[26] - x[10];
 r1 = x[27] - x[11];
 x[26] += x[10];
 x[27] += x[11];
 x[10]  = MULT_NORM(( r0  - r1 ) * cPI2_8);
 x[11]  = MULT_NORM(( r0  + r1 ) * cPI2_8);

 r0 = x[2]  - x[18];
 r1 = x[3]  - x[19];
 x[18] += x[2];
 x[19] += x[3];
 x[2]   = MULT_NORM(( r1  + r0 ) * cPI2_8);
 x[3]   = MULT_NORM(( r1  - r0 ) * cPI2_8);*/

#pragma aux mb32_2=\
 "fld  dword ptr cPI1_8"\
 "fld  dword ptr cPI3_8"\
 "fld  dword ptr 112[eax]"\
 "fld  dword ptr 116[eax]"\
 "fld  dword ptr  48[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 112[eax]"\
 "fld  dword ptr  52[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 116[eax]"\
 "fstp dword ptr 116[eax]"\
 "fstp dword ptr 112[eax]"\
 "fld  st(1)"\
 "fmul st,st(4)"\
 "fld  st(1)"\
 "fmul st,st(4)"\
 "fsub"\
 "fstp dword ptr  48[eax]"\
 "fmul st,st(3)"\
 "fxch"\
 "fmul st,st(2)"\
 "fadd"\
 "fstp dword ptr  52[eax]"\
"fld  dword ptr  96[eax]"\
 "fld  dword ptr 100[eax]"\
 "fld  dword ptr  32[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr  96[eax]"\
 "fld  dword ptr  36[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr 100[eax]"\
 "fstp dword ptr 100[eax]"\
 "fstp dword ptr  96[eax]"\
 "fld  st(1)"\
 "fmul st,st(3)"\
 "fld  st(1)"\
 "fmul st,st(5)"\
 "fsub"\
 "fstp dword ptr  32[eax]"\
 "fmul st,st(2)"\
 "fxch"\
 "fmul st,st(3)"\
 "fadd"\
 "fstp dword ptr  36[eax]"\
 modify[];
 mb32_2();

 /*r0 = x[28] - x[12];
 r1 = x[29] - x[13];
 x[28] += x[12];
 x[29] += x[13];
 x[12]  = MULT_NORM( r0 * cPI1_8  -  r1 * cPI3_8 );
 x[13]  = MULT_NORM( r0 * cPI3_8  +  r1 * cPI1_8 );*/

 /*r0 = x[24] - x[8];
 r1 = x[25] - x[9];
 x[24] += x[8];
 x[25] += x[9];
 x[8]   = MULT_NORM( r0 * cPI3_8  -  r1 * cPI1_8 );
 x[9]   = MULT_NORM( r0 * cPI1_8  +  r1 * cPI3_8);*/

#pragma aux mb32_3=\
 "fld  dword ptr  16[eax]"\
 "fld  dword ptr  20[eax]"\
 "fld  dword ptr  80[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr  16[eax]"\
 "fld  dword ptr  84[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr  20[eax]"\
 "fstp dword ptr  84[eax]"\
 "fstp dword ptr  80[eax]"\
 "fld  st"\
 "fmul st,st(3)"\
 "fld  st(2)"\
 "fmul st,st(5)"\
 "fsub"\
 "fstp dword ptr  20[eax]"\
 "fmul st,st(3)"\
 "fxch"\
 "fmul st,st(2)"\
 "fadd"\
 "fstp dword ptr  16[eax]"\
"fld  dword ptr    [eax]"\
 "fld  dword ptr   4[eax]"\
 "fld  dword ptr  64[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr    [eax]"\
 "fld  dword ptr  68[eax]"\
 "fsub st(2),st"\
 "fadd dword ptr   4[eax]"\
 "fstp dword ptr  68[eax]"\
 "fstp dword ptr  64[eax]"\
 "fld  st"\
 "fmul st,st(4)"\
 "fld  st(2)"\
 "fmul st,st(4)"\
 "fsub"\
 "fstp dword ptr   4[eax]"\
 "fmulp st(2),st"\
 "fmulp st(2),st"\
 "fadd"\
 "fstp dword ptr    [eax]"\
 modify[];
 mb32_3();

/*
 r0 = x[4]  - x[20];
 r1 = x[5]  - x[21];
 x[20] += x[4];
 x[21] += x[5];
 x[5]   = MULT_NORM( r1 * cPI3_8  -  r0 * cPI1_8 );
 x[4]   = MULT_NORM( r1 * cPI1_8  +  r0 * cPI3_8 );

 r0 = x[0]  - x[16];
 r1 = x[1]  - x[17];
 x[16] += x[0];
 x[17] += x[1];
 x[1]   = MULT_NORM( r1 * cPI1_8  -  r0 * cPI3_8 );
 x[0]   = MULT_NORM( r1 * cPI3_8  +  r0 * cPI1_8 );*/

 mdct_butterfly_16(x);
 mdct_butterfly_16(x+16);
}

void mbf_asm(void);

STIN void mdct_butterfly_first(DATA_TYPE *T,DATA_TYPE *x,int points)
{
#pragma aux mbf_asm=\
 "mov edi,ebx"\
 "shl edi,2"\
 "add edi,edx"\
 "shl ebx,1"\
 "add edx,ebx"\
 "shr ebx,5"\
 "mbfback1:"\
  "sub edi,32"\
  "fld  dword ptr 24[edi]"\
  "sub edx,32"\
  "fld  dword ptr 28[edi]"\
  "fld  dword ptr 24[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 24[edi]"\
  "fld  dword ptr 28[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 28[edi]"\
  "fstp dword ptr 28[edi]"\
  "fstp dword ptr 24[edi]"\
  "fld  dword ptr  4[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr   [eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr  4[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr 24[edx]"\
  "fsubr"\
  "fstp dword ptr 28[edx]"\
 "fld  dword ptr 16[edi]"\
  "fld  dword ptr 20[edi]"\
  "fld  dword ptr 16[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 16[edi]"\
  "fld  dword ptr 20[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 20[edi]"\
  "fstp dword ptr 20[edi]"\
  "fstp dword ptr 16[edi]"\
  "fld  dword ptr 20[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr 16[eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr 20[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr 16[edx]"\
  "fsubr"\
  "fstp dword ptr 20[edx]"\
 "fld  dword ptr  8[edi]"\
  "fld  dword ptr 12[edi]"\
  "fld  dword ptr  8[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr  8[edi]"\
  "fld  dword ptr 12[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 12[edi]"\
  "fstp dword ptr 12[edi]"\
  "fstp dword ptr  8[edi]"\
  "fld  dword ptr 36[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr 32[eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr 36[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr  8[edx]"\
  "fsubr"\
  "fstp dword ptr 12[edx]"\
 "fld  dword ptr   [edi]"\
  "fld  dword ptr  4[edi]"\
  "fld  dword ptr   [edx]"\
  "fsub st(2),st"\
  "fadd dword ptr   [edi]"\
  "fld  dword ptr  4[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr  4[edi]"\
  "fstp dword ptr  4[edi]"\
  "fstp dword ptr   [edi]"\
  "fld  dword ptr 52[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr 48[eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr 52[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr   [edx]"\
  "fsubr"\
  "add eax,64"\
  "dec ebx"\
  "fstp dword ptr  4[edx]"\
 "jnz mbfback1"\
 modify [eax ebx edx edi];
 mbf_asm();
}

void mbg_asm(void);

STIN void mdct_butterfly_generic(DATA_TYPE *T,DATA_TYPE *x,int points,int trigint)
{
#pragma aux mbg_asm=\
 "mov edi,ebx"\
 "shl edi,2"\
 "add edi,edx"\
 "shl ebx,1"\
 "add edx,ebx"\
 "shr ebx,5"\
 "shl ecx,2"\
 "mbgback1:"\
  "sub edi,32"\
  "fld  dword ptr 24[edi]"\
  "sub edx,32"\
  "fld  dword ptr 28[edi]"\
  "fld  dword ptr 24[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 24[edi]"\
  "fld  dword ptr 28[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 28[edi]"\
  "fstp dword ptr 28[edi]"\
  "fstp dword ptr 24[edi]"\
  "fld  dword ptr  4[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr   [eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr  4[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr 24[edx]"\
  "fsubr"\
  "add eax,ecx"\
  "fstp dword ptr 28[edx]"\
 "fld  dword ptr 16[edi]"\
  "fld  dword ptr 20[edi]"\
  "fld  dword ptr 16[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 16[edi]"\
  "fld  dword ptr 20[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 20[edi]"\
  "fstp dword ptr 20[edi]"\
  "fstp dword ptr 16[edi]"\
  "fld  dword ptr  4[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr   [eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr  4[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr 16[edx]"\
  "fsubr"\
  "add eax,ecx"\
  "fstp dword ptr 20[edx]"\
 "fld  dword ptr  8[edi]"\
  "fld  dword ptr 12[edi]"\
  "fld  dword ptr  8[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr  8[edi]"\
  "fld  dword ptr 12[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr 12[edi]"\
  "fstp dword ptr 12[edi]"\
  "fstp dword ptr  8[edi]"\
  "fld  dword ptr  4[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr   [eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr  4[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr  8[edx]"\
  "fsubr"\
  "add eax,ecx"\
  "fstp dword ptr 12[edx]"\
 "fld  dword ptr   [edi]"\
  "fld  dword ptr  4[edi]"\
  "fld  dword ptr   [edx]"\
  "fsub st(2),st"\
  "fadd dword ptr   [edi]"\
  "fld  dword ptr  4[edx]"\
  "fsub st(2),st"\
  "fadd dword ptr  4[edi]"\
  "fstp dword ptr  4[edi]"\
  "fstp dword ptr   [edi]"\
  "fld  dword ptr  4[eax]"\
  "fmul st,st(1)"\
  "fld  dword ptr   [eax]"\
  "fmul st(2),st"\
  "fmul st,st(3)"\
  "fadd"\
  "fld  dword ptr  4[eax]"\
  "fmulp st(3),st"\
  "fstp dword ptr   [edx]"\
  "fsubr"\
  "add eax,ecx"\
  "dec ebx"\
  "fstp dword ptr  4[edx]"\
 "jnz mbgback1"\
 modify [eax ebx ecx edx edi];
 mbg_asm();
}

void mbr_asm(void);

STIN void mdct_bitreverse(DATA_TYPE *x,int n,DATA_TYPE *T,int *bit)
{
 DATA_TYPE *x_save;

#pragma aux mbr_asm=\
 "shl edx,2"\
 "add ebx,edx"\
 "shr edx,1"\
 "add edx,eax"\
 "mov dword ptr x_save,edx"\
 "fld dword ptr half"\
 "mbrback1:"\
  "mov edi,dword ptr x_save"\
  "mov esi,edi"\
  "add edi,dword ptr  [ecx]"\
  "add esi,dword ptr 4[ecx]"\
  "fld  dword ptr  4[edi]"\
  "fsub dword ptr  4[esi]"\
  "fld  dword ptr   [edi]"\
  "fadd dword ptr   [esi]"\
  "fld  dword ptr   [ebx]"\
  "fmul st,st(1)"\
  "fld  dword ptr  4[ebx]"\
  "fmul st,st(3)"\
  "fadd"\
  "fxch st(2)"\
  "fmul dword ptr   [ebx]"\
  "fld  dword ptr  4[ebx]"\
  "fmulp st(2),st"\
  "sub edx,16"\
  "fsub"\
  "fld  dword ptr   [edi]"\
  "fsub dword ptr   [esi]"\
  "fmul st,st(3)"\
  "fld  dword ptr  4[edi]"\
  "fadd dword ptr  4[esi]"\
  "fmul st,st(4)"\
  "fld  st(3)"\
  "fadd st,st(1)"\
  "fstp dword ptr   [eax]"\
  "fsubrp st(3),st"\
  "fld  st"\
  "fadd st,st(2)"\
  "mov edi,dword ptr x_save"\
  "mov esi,edi"\
  "fstp dword ptr  4[eax]"\
  "fsub"\
  "add edi,dword ptr  8[ecx]"\
  "add esi,dword ptr 12[ecx]"\
  "fstp dword ptr 12[edx]"\
  "fstp dword ptr  8[edx]"\
 "fld  dword ptr  4[edi]"\
  "fsub dword ptr  4[esi]"\
  "fld  dword ptr   [edi]"\
  "fadd dword ptr   [esi]"\
  "fld  dword ptr  8[ebx]"\
  "fmul st,st(1)"\
  "fld  dword ptr 12[ebx]"\
  "fmul st,st(3)"\
  "fadd"\
  "fxch st(2)"\
  "fmul dword ptr  8[ebx]"\
  "fld  dword ptr 12[ebx]"\
  "fmulp st(2),st"\
  "fsub"\
  "fld  dword ptr   [edi]"\
  "fsub dword ptr   [esi]"\
  "fmul st,st(3)"\
  "fld  dword ptr  4[edi]"\
  "fadd dword ptr  4[esi]"\
  "fmul st,st(4)"\
  "fld  st(3)"\
  "fadd st,st(1)"\
  "fstp dword ptr  8[eax]"\
  "fsubrp st(3),st"\
  "fld  st"\
  "fadd st,st(2)"\
  "add ebx,16"\
  "add ecx,16"\
  "fstp dword ptr 12[eax]"\
  "fsub"\
  "add eax,16"\
  "cmp eax,edx"\
  "fstp dword ptr  4[edx]"\
  "fstp dword ptr   [edx]"\
 "jb mbrback1"\
 "fstp st"\
 modify [eax ebx ecx edx edi esi];
 mbr_asm();
}

#else

/* 8 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_8(DATA_TYPE *x)
{
 REG_TYPE r0   = x[6] + x[2];
 REG_TYPE r1   = x[6] - x[2];
 REG_TYPE r2   = x[4] + x[0];
 REG_TYPE r3   = x[4] - x[0];

 x[6] = r0   + r2;
 x[4] = r0   - r2;

 r0   = x[5] - x[1];
 r2   = x[7] - x[3];
 x[0] = r1   + r0;
 x[2] = r1   - r0;

 r0   = x[5] + x[1];
 r1   = x[7] + x[3];
 x[3] = r2   + r3;
 x[1] = r2   - r3;
 x[7] = r1   + r0;
 x[5] = r1   - r0;
}

/* 16 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_16(DATA_TYPE *x)
{
 REG_TYPE r0     = x[1]  - x[9];
 REG_TYPE r1     = x[0]  - x[8];

 x[8]  += x[0];
 x[9]  += x[1];
 x[0]   = MULT_NORM((r0   + r1) * cPI2_8);
 x[1]   = MULT_NORM((r0   - r1) * cPI2_8);

 r0     = x[3]  - x[11];
 r1     = x[10] - x[2];
 x[10] += x[2];
 x[11] += x[3];
 x[2]   = r0;
 x[3]   = r1;

 r0     = x[12] - x[4];
 r1     = x[13] - x[5];
 x[12] += x[4];
 x[13] += x[5];
 x[4]   = MULT_NORM((r0   - r1) * cPI2_8);
 x[5]   = MULT_NORM((r0   + r1) * cPI2_8);

 r0     = x[14] - x[6];
 r1     = x[15] - x[7];
 x[14] += x[6];
 x[15] += x[7];
 x[6]  = r0;
 x[7]  = r1;

 mdct_butterfly_8(x);
 mdct_butterfly_8(x+8);
}

/* 32 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_32(DATA_TYPE *x)
{
 REG_TYPE r0     = x[30] - x[14];
 REG_TYPE r1     = x[31] - x[15];

 x[30] += x[14];
 x[31] += x[15];
 x[14]  = r0;
 x[15]  = r1;

 r0 = x[28] - x[12];
 r1 = x[29] - x[13];
 x[28] += x[12];
 x[29] += x[13];
 x[12]  = MULT_NORM( r0 * cPI1_8  -  r1 * cPI3_8 );
 x[13]  = MULT_NORM( r0 * cPI3_8  +  r1 * cPI1_8 );

 r0 = x[26] - x[10];
 r1 = x[27] - x[11];
 x[26] += x[10];
 x[27] += x[11];
 x[10]  = MULT_NORM(( r0  - r1 ) * cPI2_8);
 x[11]  = MULT_NORM(( r0  + r1 ) * cPI2_8);

 r0 = x[24] - x[8];
 r1 = x[25] - x[9];
 x[24] += x[8];
 x[25] += x[9];
 x[8]   = MULT_NORM( r0 * cPI3_8  -  r1 * cPI1_8 );
 x[9]   = MULT_NORM( r1 * cPI3_8  +  r0 * cPI1_8 );

 r0 = x[22] - x[6];
 r1 = x[7]  - x[23];
 x[22] += x[6];
 x[23] += x[7];
 x[6]   = r1;
 x[7]   = r0;

 r0 = x[4]  - x[20];
 r1 = x[5]  - x[21];
 x[20] += x[4];
 x[21] += x[5];
 x[4]   = MULT_NORM( r1 * cPI1_8  +  r0 * cPI3_8 );
 x[5]   = MULT_NORM( r1 * cPI3_8  -  r0 * cPI1_8 );

 r0 = x[2]  - x[18];
 r1 = x[3]  - x[19];
 x[18] += x[2];
 x[19] += x[3];
 x[2]   = MULT_NORM(( r1  + r0 ) * cPI2_8);
 x[3]   = MULT_NORM(( r1  - r0 ) * cPI2_8);

 r0 = x[0]  - x[16];
 r1 = x[1]  - x[17];
 x[16] += x[0];
 x[17] += x[1];
 x[0]   = MULT_NORM( r1 * cPI3_8  +  r0 * cPI1_8 );
 x[1]   = MULT_NORM( r1 * cPI1_8  -  r0 * cPI3_8 );

 mdct_butterfly_16(x);
 mdct_butterfly_16(x+16);
}

/* N point first stage butterfly (in place, 2 register) */
STIN void mdct_butterfly_first(DATA_TYPE *T,DATA_TYPE *x,int points)
{
 DATA_TYPE *x1        = x          + points      - 8;
 DATA_TYPE *x2        = x          + (points>>1) - 8;
 REG_TYPE   r0;
 REG_TYPE   r1;

 do{
  r0 = x1[6] - x2[6];
  r1 = x1[7] - x2[7];
  x1[6]  += x2[6];
  x1[7]  += x2[7];
  x2[6]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
  x2[7]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

  r0 = x1[4] - x2[4];
  r1 = x1[5] - x2[5];
  x1[4]  += x2[4];
  x1[5]  += x2[5];
  x2[4]   = MULT_NORM(r1 * T[5]  +  r0 * T[4]);
  x2[5]   = MULT_NORM(r1 * T[4]  -  r0 * T[5]);

  r0 = x1[2] - x2[2];
  r1 = x1[3] - x2[3];
  x1[2]  += x2[2];
  x1[3]  += x2[3];
  x2[2]   = MULT_NORM(r1 * T[9]  +  r0 * T[8]);
  x2[3]   = MULT_NORM(r1 * T[8]  -  r0 * T[9]);

  r0 = x1[0] - x2[0];
  r1 = x1[1] - x2[1];
  x1[0]  += x2[0];
  x1[1]  += x2[1];
  x2[0]   = MULT_NORM(r1 * T[13] +  r0 * T[12]);
  x2[1]   = MULT_NORM(r1 * T[12] -  r0 * T[13]);

  x1-=8;
  x2-=8;
  T+=16;
 }while(x2>=x);
}

STIN void mdct_butterfly_generic(DATA_TYPE *T,DATA_TYPE *x,int points,int trigint)
{
 DATA_TYPE *x1        = x          + points      - 8;
 DATA_TYPE *x2        = x          + (points>>1) - 8;
 REG_TYPE   r0;
 REG_TYPE   r1;

 do{
  r0      = x1[6] - x2[6];
  r1      = x1[7] - x2[7];
  x1[6]  += x2[6];
  x1[7]  += x2[7];
  x2[6]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
  x2[7]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

  T+=trigint;

  r0      = x1[4] - x2[4];
  r1      = x1[5] - x2[5];
  x1[4]  += x2[4];
  x1[5]  += x2[5];
  x2[4]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
  x2[5]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

  T+=trigint;

  r0      = x1[2] - x2[2];
  r1      = x1[3] - x2[3];
  x1[2]  += x2[2];
  x1[3]  += x2[3];
  x2[2]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
  x2[3]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

  T+=trigint;

  r0      = x1[0] - x2[0];
  r1      = x1[1] - x2[1];
  x1[0]  += x2[0];
  x1[1]  += x2[1];
  x2[0]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
  x2[1]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

  T+=trigint;
  x1-=8;
  x2-=8;

 }while(x2>=x);
}

STIN void mdct_bitreverse(DATA_TYPE *x,int n,DATA_TYPE *T,int *bit)
{
 DATA_TYPE *w0,*w1;

 T+=n;
 w0=x;
 w1=x+(n>>1);
 x=w1;

 do{
  DATA_TYPE *x0    = x+bit[0];
  DATA_TYPE *x1    = x+bit[1];

  REG_TYPE  r0     = x0[1]  - x1[1];
  REG_TYPE  r1     = x0[0]  + x1[0];
  REG_TYPE  r2     = MULT_NORM(r1     * T[0]   + r0 * T[1]);
  REG_TYPE  r3     = MULT_NORM(r1     * T[1]   - r0 * T[0]);

  w1    -= 4;

  r0     = HALVE(x0[1] + x1[1]);
  r1     = HALVE(x0[0] - x1[0]);

  w0[0]  = r0     + r2;
  w1[2]  = r0     - r2;
  w0[1]  = r1     + r3;
  w1[3]  = r3     - r1;

  x0     = x+bit[2];
  x1     = x+bit[3];

  r0     = x0[1]  - x1[1];
  r1     = x0[0]  + x1[0];
  r2     = MULT_NORM(r1     * T[2]   + r0 * T[3]);
  r3     = MULT_NORM(r1     * T[3]   - r0 * T[2]);

  r0     = HALVE(x0[1] + x1[1]);
  r1     = HALVE(x0[0] - x1[0]);

  w0[2]  = r0     + r2;
  w1[0]  = r0     - r2;
  w0[3]  = r1     + r3;
  w1[1]  = r3     - r1;

  T     += 4;
  bit   += 4;
  w0    += 4;
 }while(w0<w1);
}

#endif

STIN void mdct_butterflies(mdct_lookup *init,DATA_TYPE *x,int points)
{
 DATA_TYPE *T=init->trig;
 int stages=init->log2n-5;
 int i,j;

 if(--stages>0)
  mdct_butterfly_first(T,x,points);

 for(i=1;--stages>0;i++)
  for(j=0;j<(1<<i);j++)
   mdct_butterfly_generic(T,x+(points>>i)*j,points>>i,4<<i);

 for(j=0;j<points;j+=32)
  mdct_butterfly_32(x+j);
}

#ifdef MDCT_FPUC
void asm_fpusetround_chop(void);
void asm_fpusetround_near(void);
#endif

void oggdec_mdct_backward(mdct_lookup *init, DATA_TYPE *in, DATA_TYPE *out)
{
 int n=init->n;
 int n2=n>>1;
 int n4=n>>2;

 /* rotate */

 DATA_TYPE *iX = in+n2-7;
 DATA_TYPE *oX = out+n2+n4;
 DATA_TYPE *T  = init->trig+n4;

#ifdef MDCT_FPUC
 int tmp;
 #pragma aux asm_fpusetround_chop=\
  "fstcw word ptr tmp"\
  "or word ptr tmp,0x0c00"\
  "fldcw word ptr tmp"\
  modify[];
 asm_fpusetround_chop();
#endif

 do{
  oX         -= 4;
  oX[0]       = MULT_NORM(-iX[2] * T[3] - iX[0]  * T[2]);
  oX[1]       = MULT_NORM (iX[0] * T[3] - iX[2]  * T[2]);
  oX[2]       = MULT_NORM(-iX[6] * T[1] - iX[4]  * T[0]);
  oX[3]       = MULT_NORM (iX[4] * T[1] - iX[6]  * T[0]);
  iX         -= 8;
  T          += 4;
 }while(iX>=in);

 iX            = in+n2-8;
 oX            = out+n2+n4;
 T             = init->trig+n4;

 do{
  T          -= 4;
  oX[0]       =  MULT_NORM (iX[4] * T[3] + iX[6] * T[2]);
  oX[1]       =  MULT_NORM (iX[4] * T[2] - iX[6] * T[3]);
  oX[2]       =  MULT_NORM (iX[0] * T[1] + iX[2] * T[0]);
  oX[3]       =  MULT_NORM (iX[0] * T[0] - iX[2] * T[1]);
  iX         -= 8;
  oX         += 4;
 }while(iX>=in);

#ifdef MDCT_FPU32
 pds_fpu_set32bit();
#endif

 mdct_butterflies(init,out+n2,n2);
 mdct_bitreverse(out,init->n,init->trig,init->bitrev);

#ifdef MDCT_FPU32
 pds_fpu_set80bit();
#endif


 /* rotate + window */
 {
  DATA_TYPE *oX1=out+n2+n4;
  DATA_TYPE *oX2=out+n2+n4;
  DATA_TYPE *iX =out;
  T             =init->trig+n2;

  do{
   oX1-=4;

   oX1[3]  =  MULT_NORM (iX[0] * T[1] - iX[1] * T[0]);
   oX2[0]  = -MULT_NORM (iX[0] * T[0] + iX[1] * T[1]);

   oX1[2]  =  MULT_NORM (iX[2] * T[3] - iX[3] * T[2]);
   oX2[1]  = -MULT_NORM (iX[2] * T[2] + iX[3] * T[3]);

   oX1[1]  =  MULT_NORM (iX[4] * T[5] - iX[5] * T[4]);
   oX2[2]  = -MULT_NORM (iX[4] * T[4] + iX[5] * T[5]);

   oX1[0]  =  MULT_NORM (iX[6] * T[7] - iX[7] * T[6]);
   oX2[3]  = -MULT_NORM (iX[6] * T[6] + iX[7] * T[7]);

   oX2+=4;
   iX    +=   8;
   T     +=   8;
  }while(iX<oX1);

  iX=out+n2+n4;
  oX1=out+n4;
  oX2=oX1;

  do{
   oX1-=4;
   iX-=4;

   oX2[0] = -(oX1[3] = iX[3]);
   oX2[1] = -(oX1[2] = iX[2]);
   oX2[2] = -(oX1[1] = iX[1]);
   oX2[3] = -(oX1[0] = iX[0]);

   oX2+=4;
  }while(oX2<iX);

  iX=out+n2+n4;
  oX1=out+n2+n4;
  oX2=out+n2;

  do{
   oX1-=4;
   oX1[0]= iX[3];
   oX1[1]= iX[2];
   oX1[2]= iX[1];
   oX1[3]= iX[0];
   iX+=4;
  }while(oX1>oX2);
 }
#ifdef MDCT_FPUC
 #pragma aux asm_fpusetround_near=\
  "fstcw word ptr tmp"\
  "and word ptr tmp,0xf3ff"\
  "fldcw word ptr tmp"\
  modify[];
 asm_fpusetround_near();
#endif
}

#ifdef MDCT_FORWARD

void oggdec_mdct_forward(mdct_lookup *init, DATA_TYPE *in, DATA_TYPE *out)
{
  int n=init->n;
  int n2=n>>1;
  int n4=n>>2;
  int n8=n>>3;
  DATA_TYPE *w=init->forward_buffer;
  DATA_TYPE *w2=w+n2;

  REG_TYPE r0;
  REG_TYPE r1;
  DATA_TYPE *x0=in+n2+n4;
  DATA_TYPE *x1=x0+1;
  DATA_TYPE *T=init->trig+n2;

  int i=0;

  for(i=0;i<n8;i+=2){
    x0 -=4;
    T-=2;
    r0= x0[2] + x1[0];
    r1= x0[0] + x1[2];
    w2[i]=   MULT_NORM(r1*T[1] + r0*T[0]);
    w2[i+1]= MULT_NORM(r1*T[0] - r0*T[1]);
    x1 +=4;
  }

  x1=in+1;

  for(;i<n2-n8;i+=2){
    T-=2;
    x0 -=4;
    r0= x0[2] - x1[0];
    r1= x0[0] - x1[2];
    w2[i]=   MULT_NORM(r1*T[1] + r0*T[0]);
    w2[i+1]= MULT_NORM(r1*T[0] - r0*T[1]);
    x1 +=4;
  }

  x0=in+n;

  for(;i<n2;i+=2){
    T-=2;
    x0 -=4;
    r0= -x0[2] - x1[0];
    r1= -x0[0] - x1[2];
    w2[i]=   MULT_NORM(r1*T[1] + r0*T[0]);
    w2[i+1]= MULT_NORM(r1*T[0] - r0*T[1]);
    x1 +=4;
  }


  mdct_butterflies(init,w+n2,n2);
  mdct_bitreverse(w,init->n,init->trig,init->bitrev);

  T=init->trig+n2;
  x0=out+n2;

  for(i=0;i<n4;i++){
    x0--;
    out[i] =MULT_NORM((w[0]*T[0]+w[1]*T[1])*init->scale);
    x0[0]  =MULT_NORM((w[0]*T[1]-w[1]*T[0])*init->scale);
    w+=2;
    T+=2;
  }
}

#endif

#endif // !USE_AAC_MDCT
