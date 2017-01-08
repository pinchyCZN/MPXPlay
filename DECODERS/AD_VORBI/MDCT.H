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

 function: modified discrete cosine transform prototypes
 last mod: $Id: mdct.h,v 1.20 2003/10/01 00:00:00 PDSoft Exp $

 ********************************************************************/

#ifndef _OGG_mdct_H_
#define _OGG_mdct_H_

//#define USE_AAC_MDCT 1

#ifdef USE_AAC_MDCT

#include "../dec_aac/structs.h"
#include "../dec_aac/mdct.h"

#define oggdec_mdct_init(n)       faad_mdct_init(n)
#define oggdec_mdct_clear(lookup) faad_mdct_end((mdct_info *)lookup)
#define oggdec_mdct_backward(lookup,in,out) \
         {                                  \
          unsigned int i=n;                 \
          double n_f=(double)n/2;           \
          for(i=0;i<n/2;i++)                \
           in[i] *=(float)n_f;              \
         }                                  \
         faad_imdct((mdct_info *)lookup,in,out)

#else

#include "os_types.h"

//with the asm routines the (full) decoding is faster with 6-8%,
//but we get too much high sounds (a false tone) (due to the optimized FPU calculations)

#ifdef OGG_USE_ASM
 //#define MDCT_ASM 1
#endif

#if defined(MDCT_ASM) && defined(MPXPLAY)
 //#define MDCT_FPUC 1
//#define MDCT_FPU32 1 // for asm
#endif

//#define MDCT_INTEGERIZED  // <- be warned there could be some hurt left here

//#define MDCT_FORWARD 1

#ifdef MDCT_INTEGERIZED

#define DATA_TYPE int
#define REG_TYPE  register int
#define TRIGBITS 14
#define cPI3_8 6270
#define cPI2_8 11585
#define cPI1_8 15137

#define FLOAT_CONV(x) ((int)((x)*(1<<TRIGBITS)+.5))
#define MULT_NORM(x) ((x)>>TRIGBITS)
#define HALVE(x) ((x)>>1)

#else

#define DATA_TYPE ogg_double_t
#define REG_TYPE  ogg_double_t
#ifndef MDCT_ASM
#define cPI3_8 .38268343236508977175F
#define cPI2_8 .70710678118654752441F
#define cPI1_8 .92387953251128675613F
#endif

#define FLOAT_CONV(x) (x)
#define MULT_NORM(x) (x)
#define HALVE(x) ((x)*.5F)

#endif

typedef struct {
	int n;
	int log2n;
	DATA_TYPE *trig;
	int *bitrev;

#ifdef MDCT_FORWARD
	DATA_TYPE scale;
	DATA_TYPE *forward_buffer;
#endif
} mdct_lookup;

extern void *oggdec_mdct_init(unsigned int n);
extern void oggdec_mdct_clear(mdct_lookup * l);
extern void oggdec_mdct_backward(mdct_lookup * init, DATA_TYPE * in, DATA_TYPE * out);
#ifdef MDCT_FORWARD
extern void oggdec_mdct_forward(mdct_lookup * init, DATA_TYPE * in, DATA_TYPE * out);
#endif

#endif							// USE_AAC_MDCT

#endif							// _OGG_mdct_H
