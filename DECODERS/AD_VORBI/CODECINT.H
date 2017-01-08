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

 function: libvorbis codec headers
 last mod: $Id: codec_internal.h,v 1.13 2002/11/05 00:00:00 PDSoft Exp $

 ********************************************************************/

#ifndef _V_CODECI_H_
#define _V_CODECI_H_

#include "codebook.h"

typedef void vorbis_look_mapping;
typedef void vorbis_look_floor;
typedef void vorbis_look_residue;
typedef void vorbis_look_transform;

typedef struct {
	int blockflag;
	int windowtype;
	int transformtype;
	int mapping;
} vorbis_info_mode;

typedef void vorbis_info_floor;
typedef void vorbis_info_residue;
typedef void vorbis_info_mapping;

#include "backends.h"

typedef struct backend_lookup_state {
	ogg_double_t *window[2];
	vorbis_look_transform *transform[2];
	//vorbis_look_transform **transform[2];
	int modebits;
	vorbis_look_floor **flr;
	vorbis_info_residue **residue;
} backend_lookup_state;

typedef struct codec_setup_info {
	unsigned long blocksizes[2];

	int modes;
	int maps;
	int floors;
	int residues;
	int books;

	codebook *fullbooks;

	vorbis_info_mode *mode_param[64];
	int map_type[64];
	vorbis_info_mapping *map_param[64];
	int floor_type[64];
	vorbis_info_floor *floor_param[64];
	int residue_type[64];
	vorbis_info_residue *residue_param[64];
	static_codebook *book_param[256];

} codec_setup_info;

#endif
