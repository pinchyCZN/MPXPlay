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

 function: libvorbis backend and mapping structures; needed for
	   static mode headers
 last mod: $Id: backends.h,v 1.13 2002/11/05 00:00:00 PDSoft Exp $

 ********************************************************************/

#ifndef _vorbis_backend_h_
#define _vorbis_backend_h_

#include "codecint.h"

typedef struct{
 vorbis_info_floor *(*unpack)(vorbis_info *,oggpack_buffer *);
 vorbis_info_floor *(*look)  (vorbis_dsp_state *,vorbis_info_floor *);
 void (*free_info)  (vorbis_info_floor *);
 void (*free_look)  (vorbis_look_floor *);
 void *(*inverse1)  (struct vorbis_block *,vorbis_look_floor *,unsigned int channel);
 int   (*inverse2)  (struct vorbis_block *,vorbis_look_floor *,void *buffer,ogg_double_t *);
} vorbis_func_floor;

typedef struct{
 int   order;
 long  rate;
 long  barkmap;

 int   ampbits;
 int   ampdB;

 int   numbooks;
 int   books[16];
 vorbis_info *vi;
} vorbis_info_floor0;

#define VIF_POSIT 64 // 63 !!!
#define VIF_CLASS 16
#define VIF_PARTS 32 // 31 !!!

typedef struct{
 int dim;                 /* 1 to 8 */
 int subs;                /* 0,1,2,3 (bits: 1<<n poss) */
 int book;                /* subs ^ dim entries */
 int subbook[8];          /* [VIF_CLASS][subs] */
}vif_class_s;

typedef struct{
 int   partitions;                /* 0 to 31 */
 int   mult;                      /* 1 2 3 or 4 */
 int   *partitionclass;           /* 0 to 15 */
 vif_class_s *classes;
 int   *postlist;             /* first two implicit */
 vorbis_info *vi;
} vorbis_info_floor1;

typedef struct{
 vorbis_info_residue *(*unpack)(vorbis_info *,oggpack_buffer *);
 vorbis_look_residue *(*look)  (vorbis_dsp_state *,vorbis_info_residue *);
 void (*free_info)    (vorbis_info_residue *);
 void (*free_look)    (vorbis_look_residue *);
 int  (*inverse)      (struct vorbis_block *,vorbis_info_residue *,ogg_double_t **,unsigned int);
} vorbis_func_residue;

typedef long res01_decodev_t(codebook *,ogg_double_t *,oggpack_buffer *,int);
typedef long res2_decodev_t(codebook *,ogg_double_t **,long,int,oggpack_buffer *,int);

typedef struct vorbis_info_residue0{
 unsigned int  samples_per_partition; /* group n vectors per partition */
 unsigned long begin;

 unsigned int dim;                 // look->phrasebook->dim;
 unsigned int info_partvals;       // len/samples_per_partition
 int       **partwordp;

 // from look
 int         stages;
 codebook   *phrasebook;
 codebook ***partbooks;
 int       **decodemap;
 int         look_partvals;
 // ----

 res01_decodev_t *setpart;
 res01_decodev_t *decodepart;

 unsigned int len;              // end-begin
 unsigned int partitions;       /* possible codebooks for a partition */
 unsigned int groupbook;        /* huffbook for partitioning */

 unsigned int secondstages[64]; /* expanded out to pointers in lookup */
 unsigned int booklist[256];    /* list of second stage books */

} vorbis_info_residue0;

typedef struct{
 vorbis_info_mapping *(*unpack)(vorbis_info *,oggpack_buffer *);
 void (*free_info)    (vorbis_info_mapping *);
 int  (*inverse)      (struct vorbis_block *vb,vorbis_info_mapping *);
} vorbis_func_mapping;

typedef struct vorbis_info_mapping0
{
 int   submaps;           /* <= 16 */
 int   coupling_steps;

 ogg_double_t **pcmbundle;
 int   *nonzero;
 void  **floormemo;

 int   *floorsubmap;   /* [mux] submap to floors */
 int   *residuesubmap; /* [mux] submap to residue */

 int   *chmuxlist;
 int   *coupling_mag;
 int   *coupling_ang;

} vorbis_info_mapping0;

#endif
