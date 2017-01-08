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

  function: LSP (also called LSF) conversion routines
  last mod: $Id: lsp.h,v 1.11 2002/09/25 00:00:00 PDSoft Exp $

 ********************************************************************/


#ifndef _V_LSP_H_
#define _V_LSP_H_

#include "os_types.h"

extern void vorbis_lsp_to_curve(ogg_double_t * curve, ogg_double_t * lsp, ogg_double_t * map_cos, int n, int m, ogg_double_t amp, ogg_double_t ampoffset);

#endif
