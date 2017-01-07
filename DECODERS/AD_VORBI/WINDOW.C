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

 function: window functions
 last mod: $Id: window.c,v 1.21 2003/03/15 00:00:00 PDSoft Exp $

 ********************************************************************/

#include <stdlib.h>
#include <math.h>
#include "os_types.h"
#include "codec.h"
#include "os.h"

//#define OPTIMIZATIONS_MSVC 1

#ifdef OPTIMIZATIONS_MSVC

ogg_double_t *_vorbis_window_get(int left)
{
 static float half=0.5f;
 //double pi=atan(1.0)*4.0;
 //double pi2=pi/2;
 static float pi2=M_PI/2;
 static float pi=M_PI;
 static double qhalf=0.5;
#ifdef MPXPLAY
 static float scale=32768;
#endif
 int i;
 ogg_double_t *window=_ogg_malloc(left*sizeof(*window));
 if(window){
  for(i=0;i<left;i++){
   double x=(i+half)/left;
   x*=pi2;
   x=sin(x);
   x*=x;
   x*=pi;
   x*=qhalf;
   x=sin(x);

#ifdef MPXPLAY
   x*=scale;
#endif

   window[i]=x;
  }
 }
 return(window);
}

#else

#ifdef MPXPLAY
#define WINDOW_SCALE 32768
#endif

ogg_double_t *_vorbis_window_get(int left)
{
 int i;
 ogg_double_t *window=_ogg_malloc(left*sizeof(*window));
 if(window){
  for(i=0;i<left;i++){
   float x=(i+0.5f)/left*(M_PI/2.0f);
   x=sin(x);
   x*=x;
   x*=(M_PI/2.0f);
   x=sin(x);

#ifdef WINDOW_SCALE
   x*=WINDOW_SCALE;
#endif

   window[i]=x;
  }
 }
 return(window);
}

#endif
