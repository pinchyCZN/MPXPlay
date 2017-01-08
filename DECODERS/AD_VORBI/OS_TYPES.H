/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: #ifdef jail to whip a few platforms into the UNIX ideal.
 last mod: $Id: os_types.h,v 1.20 2003/02/05 00:00:00 PDSoft Exp $

 ********************************************************************/
#ifndef _OS_TYPES_H
#define _OS_TYPES_H

#define _ogg_malloc  malloc
#define _ogg_calloc  calloc
#define _ogg_realloc realloc
#define _ogg_free    free

#define _ogg_memcpy  memcpy
#define _ogg_memset  memset
#define _ogg_memmove memmove

//#define OGGDEC_DOUBLE_PRECISION 1 // currently doesn't work at all (some x86-32bit specific C code)

#if defined(__WATCOMC__) && !defined(OGGDEC_DOUBLE_PRECISION)
#define OGG_USE_ASM 1			// currently only for Watcom-asm with 32-bit floats
#endif

#ifdef MPXPLAY
#define OGG_SPECTRUM_ANALISER 1
#endif

#ifdef OGG_SPECTRUM_ANALISER
extern void ogg_vorbis_analiser_config(unsigned int bn, unsigned long *bd);
extern void ogg_vorbis_analiser_clear(void);
#endif

#ifdef OGGDEC_DOUBLE_PRECISION
typedef double ogg_double_t;
#else
typedef float ogg_double_t;
#endif
typedef float ogg_float_t;

#if defined(_WIN32) && !defined(__WATCOMC__)

#  ifndef __GNUC__
   /* MSVC/Borland */
typedef __int64 ogg_int64_t;
typedef __int32 ogg_int32_t;
typedef unsigned __int32 ogg_uint32_t;
typedef __int16 ogg_int16_t;
#  else
   /* Cygwin */
#include <_G_config.h>
typedef _G_int64_t ogg_int64_t;
typedef _G_int32_t ogg_int32_t;
typedef _G_uint32_t ogg_uint32_t;
typedef _G_int16_t ogg_int16_t;
#  endif

#elif defined(__MACOS__)

#  include <sys/types.h>
typedef SInt16 ogg_int16_t;
typedef SInt32 ogg_int32_t;
typedef UInt32 ogg_uint32_t;
typedef SInt64 ogg_int64_t;

#elif defined(__MACOSX__)		/* MacOS X Framework build */

#  include <sys/types.h>
typedef int16_t ogg_int16_t;
typedef int32_t ogg_int32_t;
typedef u_int32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;

#elif defined(__BEOS__)

   /* Be */
#  include <inttypes.h>

#elif defined (__EMX__)

   /* OS/2 GCC */
typedef short ogg_int16_t;
typedef int ogg_int32_t;
typedef unsigned int ogg_uint32_t;
typedef long long ogg_int64_t;

#else

#ifdef __WATCOMC__
typedef long long ogg_int64_t;	// Watcom C v11c can handle this (http://www.openwatcom.org)
typedef long ogg_int32_t;
typedef unsigned long ogg_uint32_t;
typedef short ogg_int16_t;
#else
#include <sys/types.h>
#include <ogg/config_types.h>
#endif

#endif

#endif							/* _OS_TYPES_H */
