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
 last mod: $Id: codebook.h,v 1.10 2003/02/10 00:00:00 PDSoft Exp $

 ********************************************************************/

#ifndef _V_CODEBOOK_H_
#define _V_CODEBOOK_H_

#include "ogg.h"

#ifdef OGG_USE_ASM
 #define CODEBOOK_ASM 1
#endif

#define CODEBOOK_MAX_ENTRIES 32767  // 'short' storage capacity (I hope we'll never need more)
#define CODEBOOK_LOOKUP_BITS_MAX 8
#ifdef CODEBOOK_ASM                 // max allowed length of huffman bits (in the decoder)
 #define CODEBOOK_MAX_BITS    (CODEBOOK_LOOKUP_BITS_MAX+25)
#else
 #define CODEBOOK_MAX_BITS    (CODEBOOK_LOOKUP_BITS_MAX+32)
#endif

typedef struct static_codebook{
  long   dim;            /* codebook dimensions (elements per vector) */
  long   entries;        /* codebook entries */
  long  *lengthlist;     /* codeword lengths in bits */

  /* mapping ***************************************************************/
  int    maptype;        /* 0=none
			    1=implicitly populated values from map column
			    2=listed arbitrary values */

  /* The below does a linear, single monotonic sequence mapping. */
  long     q_min;       /* packed 32 bit float; quant value 0 maps to minval */
  long     q_delta;     /* packed 32 bit float; val 1 - val 0 == delta */
  int      q_quant;     /* bits: 0 < quant <= 16 */
  int      q_sequencep; /* bitflag */

  long     *quantlist;  /* map == 1: (int)(entries^(1/dim)) element column map
			   map == 2: list of dim*entries quantized entry vals
			*/
  int allocedp;
} static_codebook;

typedef struct decode_aux{
  int    tab_maxlen;
  short  *tab_ptr;
  unsigned char *tab_codelen;
  short  *ptr0;
  short  *ptr1;
} decode_aux;

typedef struct codebook{
  long dim;                /* codebook dimensions (elements per vector) */
  struct decode_aux *decode_tree;
  ogg_float_t *valuelist;  /* list of dim*entries actual entry values */

  long entries;       /* codebook entries */
  int  maxbits;
} codebook;

extern void vorbis_staticbook_clear(static_codebook *b);
extern void vorbis_staticbook_destroy(static_codebook *b);
extern int  vorbis_book_init_decode(codebook *dest,static_codebook *source);
extern void vorbis_book_clear(codebook *b);

extern int  _ilog(unsigned int v);
extern long _book_maptype1_quantvals(const static_codebook *b);

extern int  vorbis_staticbook_unpack(oggpack_buffer *b,static_codebook *c);

extern long vorbis_book_decode(const decode_aux *t, oggpack_buffer *b);

// positive value if ok, negative value if bad (not only -1 !)
extern long vorbis_book_decodevs_add(codebook *book, ogg_double_t *a,oggpack_buffer *b,int n);
extern long vorbis_book_decodevs_set(codebook *book, ogg_double_t *a,oggpack_buffer *b,int n);
extern long vorbis_book_decodev_add(codebook *book, ogg_double_t *a,oggpack_buffer *b,int n);
extern long vorbis_book_decodev_set(codebook *book, ogg_double_t *a,oggpack_buffer *b,int n);
extern long vorbis_book_decodevv_add(codebook *book, ogg_double_t **a,long off,int ch,oggpack_buffer *b,int n);
extern long vorbis_book_decodevv_set(codebook *book, ogg_double_t **a,long off,int ch,oggpack_buffer *b,int n);

#endif
