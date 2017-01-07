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
** $Id: bits.h,v 1.23 2006/12/04 00:00:00 PDSoft Exp $
**/

#ifndef __BITS_H__
#define __BITS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "analysis.h"
#ifdef ANALYSIS
#include <stdio.h>
#endif
#include "common.h"

#ifdef FAAD_USE_ASM
 #define FAAD_USE_BITS_ASM 1
#endif

#define BYTE_NUMBIT 8
#define bit2byte(a) ((a+7)/BYTE_NUMBIT)

typedef struct _bitfile
{
 uint32_t bitpos;
 uint32_t storedbits;
 uint8_t *buffer;
 uint8_t error;
}bitfile;


#if defined(__WATCOMC__)
 uint32_t asm_BSWAP(uint32_t);
 #pragma aux asm_BSWAP parm [eax] value [eax] = "bswap eax"
 #define BSWAP(a) a=asm_BSWAP(a);
#else

 #if defined (_WIN32) && !defined(_WIN32_WCE)
  #define BSWAP(a) __asm mov eax,a __asm bswap eax __asm mov a, eax
 #elif defined(LINUX) || defined(DJGPP)
  #define BSWAP(a) __asm__ ( "bswapl %0\n" : "=r" (a) : "0" (a) )
 #else
  #define BSWAP(a) \
    ((a) = ( ((a)&0xff)<<24) | (((a)&0xff00)<<8) | (((a)>>8)&0xff00) | (((a)>>24)&0xff))
 #endif
#endif

void faad_initbits(bitfile *ld, void *buffer, uint32_t buffer_size);
uint32_t faad_byte_align(bitfile *ld);
uint32_t faad_get_processed_bits(bitfile *ld);
void faad_rewindbits(bitfile *ld);
uint32_t faad_showbits(bitfile *ld, uint32_t bits);
void faad_flushbits(bitfile *ld, uint32_t bits);
uint32_t faad_getbits(bitfile *ld, uint32_t n DEBUGDEC);
uint32_t faad_get1bit(bitfile *ld DEBUGDEC);

uint32_t faad_bits_show24(bitfile *b,uint32_t bits);
uint32_t faad_bits_show3b_h(bitfile *b);
uint32_t faad_bits_read24(bitfile *b,uint32_t bits DEBUGDEC);
uint32_t faad_bits_setleft(bitfile *b,uint32_t left);

#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)

 //show min. 25 bits left (top) aligned (the lowest 7 bit is undefinied)
 uint32_t faad_bits_inline_show3b_h(bitfile *b);
 #pragma aux faad_bits_inline_show3b_h=\
 "mov ecx,dword ptr [eax]"\
 "mov esi,ecx"\
 "shr esi,3"\
 "add esi,dword ptr 8[eax]"\
 "mov esi,dword ptr [esi]"\
 "bswap esi"\
 "and ecx,7"\
 "shl esi,cl"\
 parm[eax] value[esi] modify[ecx];

 //show 24 bits without overflow check
 uint32_t faad_bits_inline_show24nc(bitfile *b,uint32_t bits);
 #pragma aux faad_bits_inline_show24nc=\
 "mov ecx,dword ptr [eax]"\
 "mov esi,ecx"\
 "shr esi,3"\
 "add esi,dword ptr 8[eax]"\
 "mov esi,dword ptr [esi]"\
 "bswap esi"\
 "and ecx,7"\
 "shl esi,cl"\
 "mov cl,32"\
 "sub cl,dl"\
 "shr esi,cl"\
 parm[eax][edx] value[esi] modify[ecx];

 void faad_bits_inline_flush(bitfile *ld, uint32_t bits);
 #pragma aux faad_bits_inline_flush=\
 "add edx,dword ptr [eax]"\
 "mov dword ptr [eax],edx"\
 "cmp edx,dword ptr 4[eax]"\
 "seta byte ptr 12[eax]"\
 parm[eax][edx] modify[edx];

#else

 #define faad_bits_inline_show3b_h(ld)      faad_bits_show3b_h(ld)
 #define faad_bits_inline_show24nc(ld,bits) faad_bits_show24(ld,bits)
 #define faad_bits_inline_flush(ld,bits) ld->bitpos+=bits; \
                                         if(ld->bitpos>ld->storedbits) \
                                          ld->error=1
#endif


#ifdef __cplusplus
}
#endif
#endif
