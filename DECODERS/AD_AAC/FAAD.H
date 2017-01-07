/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: faad.h,v 1.28 2006/12/04 00:00:00 PDSoft Exp $
**/

#ifndef __AACDEC_H__
#define __AACDEC_H__

#include "common.h"
#include "structs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*#ifdef _WIN32
  #pragma pack(push, 8)
  #ifndef FAADAPI
    #define FAADAPI __cdecl
  #endif
#else
  #ifndef FAADAPI
    #define FAADAPI
  #endif
#endif*/

#ifndef FAADAPI
 #define FAADAPI
#endif

#define FAAD2_VERSION "2.0 RC1 "

/* object types for AAC */
#define MAIN       0
#define LC         1
#define SSR        2
#define LTP        3
#define ER_LC     17
#define ER_LTP    19
#define LD        23
#define DRM_ER_LC 27 /* special object type for DRM */

/* header types */
#define RAW        0
#define ADIF       1
#define ADTS       2

/* library output formats */
#define FAAD_FMT_16BIT  1
#define FAAD_FMT_24BIT  2
#define FAAD_FMT_32BIT  3
#define FAAD_FMT_FLOAT  4
#define FAAD_FMT_DOUBLE 5
#define FAAD_FMT_16BIT_DITHER  6
#define FAAD_FMT_16BIT_L_SHAPE 7
#define FAAD_FMT_16BIT_M_SHAPE 8
#define FAAD_FMT_16BIT_H_SHAPE 9

/* Capabilities */
#define LC_DEC_CAP            (1<<0)
#define MAIN_DEC_CAP          (1<<1)
#define LTP_DEC_CAP           (1<<2)
#define LD_DEC_CAP            (1<<3)
#define ERROR_RESILIENCE_CAP  (1<<4)
#define FIXED_POINT_CAP       (1<<5)

/* Channel definitions */
#define FRONT_CHANNEL_CENTER (1)
#define FRONT_CHANNEL_LEFT   (2)
#define FRONT_CHANNEL_RIGHT  (3)
#define SIDE_CHANNEL_LEFT    (4)
#define SIDE_CHANNEL_RIGHT   (5)
#define BACK_CHANNEL_LEFT    (6)
#define BACK_CHANNEL_RIGHT   (7)
#define BACK_CHANNEL_CENTER  (8)
#define LFE_CHANNEL          (9)
#define UNKNOWN_CHANNEL      (0)


/* A decode call can eat up to FAAD_MIN_STREAMSIZE bytes per decoded channel,
   so at least so much bytes per channel should be available in this stream */
#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */

#define AAC_PCMOUT_T float  // must match with MPXPLAY_PCMOUT_FLOAT_T

unsigned long FAADAPI faacDecGetCapabilities(void);

faacDecHandle FAADAPI faacDecOpen(void);

faacDecConfigurationPtr FAADAPI faacDecGetCurrentConfiguration(faacDecHandle hDecoder);

// new init functions
int FAADAPI faacDecInit_frame(faacDecHandle hDecoder, uint8_t *buffer,
                              uint32_t buffer_size,
                              faacDecFrameInfo *frameInfo);

int FAADAPI faacDecInit_dsi(faacDecHandle hDecoder, unsigned char *pBuffer,
                            unsigned long SizeOfDecoderSpecificInfo,
                            faacDecFrameInfo *frameInfo);

/* Init the library based on info from the AAC file (ADTS/ADIF) */
long FAADAPI faacDecInit(faacDecHandle hDecoder,
                        unsigned char *buffer,
                        unsigned long buffer_size,
                        unsigned long *samplerate,
                        unsigned int  *channels);

/* Init the library using a DecoderSpecificInfo */
int FAADAPI faacDecInit2(faacDecHandle hDecoder, unsigned char *pBuffer,
                         unsigned long SizeOfDecoderSpecificInfo,
                         unsigned long *samplerate, unsigned int *channels);

/* Init the library for DRM */
int FAADAPI faacDecInitDRM(faacDecHandle hDecoder, unsigned long samplerate,
                            unsigned int channels);

long FAADAPI faacDecGetframeLength(faacDecHandle hDecoder);

//init dynamic memory fields
long FAADAPI faacDecInitFields(faacDecHandle hDecoder);
//bitstream related mallocs only
unsigned int FAADAPI faacDecInitField_bs(faacDecHandle hDecoder);

//close
void FAADAPI faacDecClose(faacDecHandle hDecoder);

//decoding
void* FAADAPI faacDecDecode(faacDecHandle hDecoder,
                            faacDecFrameInfo *hInfo,
                            unsigned char *buffer,
                            unsigned long buffer_size);

// seek helpers
void FAADAPI faacDecPostSeekReset(faacDecHandle hDecoder, long frame);

void FAADAPI faacDecReadframe(faacDecHandle hDecoder,
                            faacDecFrameInfo *hInfo,
                            unsigned char *buffer,
                            unsigned long buffer_size);

// MP4
char FAADAPI AudioSpecificConfig(unsigned char *pBuffer,
                                 unsigned long buffer_size,
                                 mp4AudioSpecificConfig *mp4ASC);

void aac_analiser_clear(void);

/*#ifdef _WIN32
  #pragma pack(pop)
#endif*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
