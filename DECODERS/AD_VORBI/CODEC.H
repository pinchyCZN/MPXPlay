/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2001             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *

 ********************************************************************

 function: libvorbis codec headers
 last mod: $Id: codec.h,v 1.39 2002/08/31 00:00:00 PDSoft Exp $

 ********************************************************************/

#ifndef _vorbis_codec_h_
#define _vorbis_codec_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include "ogg.h"

typedef struct vorbis_info{
  int channels;
  int outchannels;
  long rate;

  long bitrate_nominal;

  void *codec_setup;
} vorbis_info;

typedef struct vorbis_dsp_state
{
  vorbis_info *vi;

  ogg_double_t **pcm;
  ogg_double_t **pcmret;
  int      pcm_storage;
  int      pcm_current;
  int      pcm_returned;

  int  eofflag;

  long lW;
  long W;
  long nW;
  long centerW;

  ogg_int64_t granulepos;
  ogg_int64_t sequence;

  void       *backend_state;
} vorbis_dsp_state;

typedef struct vorbis_block{
  ogg_double_t  **pcm;       /* this is a pointer into local storage */
  oggpack_buffer opb;

  long  lW;
  long  W;
  long  nW;
  int   pcmend;
  int   mode;

  int         eofflag;
  ogg_int64_t granulepos;
  ogg_int64_t sequence;
  vorbis_dsp_state *vd; /* For read-only access of configuration */

} vorbis_block;

typedef struct vorbis_comment{
  char **user_comments;
  int   *comment_lengths;
  int    comments;
  char  *vendor;

} vorbis_comment;


/* Vorbis PRIMITIVES: general ***************************************/

extern void  vorbis_info_init(vorbis_info *vi);
extern void  vorbis_info_clear(vorbis_info *vi);
extern void  vorbis_comment_init(vorbis_comment *vc);
extern char *vorbis_comment_query(vorbis_comment *vc, char *tag, int count);
extern void  vorbis_comment_clear(vorbis_comment *vc);

extern int   vorbis_block_init(vorbis_dsp_state *v, vorbis_block *vb);
extern int   vorbis_block_clear(vorbis_block *vb);
extern void  vorbis_dsp_clear(vorbis_dsp_state *v);

extern void vorbis_fscale_block(ogg_double_t *a,unsigned int len,float scale);
extern void vorbis_fclear_block(ogg_double_t *a,unsigned int len);
extern void vorbis_fcopy_block(ogg_double_t *a,ogg_double_t *b,unsigned int len);
extern void vorbis_fmulwin_add_block(ogg_double_t *a,ogg_double_t *b,ogg_double_t *w,unsigned int len);
extern void vorbis_fcopy_and_scale_block(ogg_double_t *a,ogg_double_t *b,unsigned int len,float scale);

/* Vorbis PRIMITIVES: synthesis layer *******************************/
extern int      vorbis_synthesis_headerin(vorbis_info *vi,vorbis_comment *vc,ogg_packet *op);

extern void     vorbis_synthesis_restart(vorbis_dsp_state *v,vorbis_info *vi);
extern int      vorbis_synthesis_init(vorbis_dsp_state *v,vorbis_info *vi);
extern int      vorbis_synthesis(vorbis_block *vb,ogg_packet *op);
extern int      vorbis_synthesis_blockin(vorbis_dsp_state *v,vorbis_block *vb);
extern int      vorbis_synthesis_pcmout(vorbis_dsp_state *v,ogg_double_t ***pcm);
extern int      vorbis_synthesis_read(vorbis_dsp_state *v,int samples);

/* Vorbis ERRORS and return codes ***********************************/

#define OV_FALSE      -1
#define OV_EOF        -2
#define OV_HOLE       -3

#define OV_EREAD      -128
#define OV_EFAULT     -129
#define OV_EIMPL      -130
#define OV_EINVAL     -131
#define OV_ENOTVORBIS -132
#define OV_EBADHEADER -133
#define OV_EVERSION   -134
#define OV_ENOTAUDIO  -135
#define OV_EBADPACKET -136
#define OV_EBADLINK   -137
#define OV_ENOSEEK    -138

#ifdef __cplusplus
}
#endif

#endif
