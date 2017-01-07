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
** $Id: bits.c,v 1.23 2003/09/27 00:00:00 PDSoft Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>
#include "bits.h"

#ifndef FAAD_USE_BITS_ASM
static uint32_t bitmask[33]=
{0x00000000,0x00000001,0x00000003,0x00000007,0x0000000f,
 0x0000001f,0x0000003f,0x0000007f,0x000000ff,0x000001ff,
 0x000003ff,0x000007ff,0x00000fff,0x00001fff,0x00003fff,
 0x00007fff,0x0000ffff,0x0001ffff,0x0003ffff,0x0007ffff,
 0x000fffff,0x001fffff,0x003fffff,0x007fffff,0x00ffffff,
 0x01ffffff,0x03ffffff,0x07ffffff,0x0fffffff,0x1fffffff,
 0x3fffffff,0x7fffffff,0xffffffff };
#endif

void faad_initbits(bitfile *b,void *buf,uint32_t bytes)
{
 b->bitpos=0;
 b->storedbits=bytes<<3;
 b->buffer=buf;
 b->error=0;
}

uint32_t faad_byte_align(bitfile *b)
{
 uint32_t newbitpos=(b->bitpos+7)&(~7);
 uint32_t remainder=newbitpos-b->bitpos;
 b->bitpos=newbitpos;
 if(newbitpos>b->storedbits)
  b->error=1;
 return remainder;
}

uint32_t faad_get_processed_bits(bitfile *b)
{
 return b->bitpos;
}

void faad_rewindbits(bitfile *b)
{
 b->bitpos=0;
 b->error=0;
}

#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)

uint32_t asm_faad_bitsshow24(bitfile *b,uint32_t bits);

#pragma aux asm_faad_bitsshow24=\
 "mov ebx,dword ptr [eax]"\
 "mov ecx,ebx"\
 "add ebx,edx"\
 "cmp ebx,dword ptr 4[eax]"\
 "jbe showok"\
  "mov byte ptr 12[eax],1"\
  "xor eax,eax"\
  "jmp showend"\
 "showok:"\
 "mov ebx,ecx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "mov eax,dword ptr [ebx]"\
 "bswap eax"\
 "and ecx,7"\
 "shl eax,cl"\
 "mov cl,32"\
 "sub cl,dl"\
 "shr eax,cl"\
 "showend:"\
 parm[eax][edx] value[eax] modify[ebx ecx];

#endif

//show max. 24 bits
uint32_t faad_bits_show24(bitfile *b,uint32_t bits)
{
#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)
 return asm_faad_bitsshow24(b,bits);
#else
 uint32_t ret,bitindex;
 uint8_t *bufpos;

 if((b->bitpos+bits)>b->storedbits){
  b->error=1;
  return 0;
 }

 bufpos=b->buffer+(b->bitpos>>3);
 bitindex=b->bitpos&7;

 ret=*((uint32_t *)bufpos);
#ifndef ARCH_IS_BIG_ENDIAN
 BSWAP(ret);
#endif
 //ret<<=bitindex;
 //ret>>=32-bits;
 ret>>=32-bitindex-bits;
 ret&=bitmask[bits];

 return ret;
#endif
}

uint32_t faad_showbits(bitfile *b,uint32_t bits)
{
 uint32_t ret;

 if(bits<=24)
  ret=faad_bits_show24(b,bits);
 else{
  ret=faad_bits_show24(b,bits-16)<<16;
  b->bitpos+=bits-16;
  ret|=faad_bits_show24(b,16);
  b->bitpos-=bits-16;
 }

 return ret;
}

#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)

uint32_t asm_faad_bitsshow3b_h(bitfile *b);

#pragma aux asm_faad_bitsshow3b_h=\
 "mov ebx,dword ptr [eax]"\
 "mov ecx,ebx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "mov eax,dword ptr [ebx]"\
 "bswap eax"\
 "and ecx,7"\
 "shl eax,cl"\
 parm[eax] value[eax] modify[ebx ecx];

#endif

//show left (top) aligned min. 25 bits (lowest 7 bit is undefinied)
uint32_t faad_bits_show3b_h(bitfile *b)
{
#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)
 return asm_faad_bitsshow3b_h(b);
#else
 uint32_t ret,bitindex;
 uint8_t *bufpos;

 bufpos=b->buffer+(b->bitpos>>3);
 bitindex=b->bitpos&7;

 ret=*((uint32_t *)bufpos);
#ifndef ARCH_IS_BIG_ENDIAN
 BSWAP(ret);
#endif
 ret<<=bitindex;

 return ret;
#endif
}

void faad_flushbits(bitfile *b,uint32_t bits)
{
 b->bitpos+=bits;
 if(b->bitpos>b->storedbits)
  b->error=1;
}

#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)

uint32_t asm_faad_bitsread24(bitfile *b,uint32_t bits);

#pragma aux asm_faad_bitsread24=\
 "mov ebx,dword ptr [eax]"\
 "mov ecx,ebx"\
 "add ebx,edx"\
 "mov dword ptr [eax],ebx"\
 "cmp ebx,dword ptr 4[eax]"\
 "jbe readok"\
  "mov byte ptr 12[eax],1"\
  "xor eax,eax"\
  "jmp readend"\
 "readok:"\
 "mov ebx,ecx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "mov eax,dword ptr [ebx]"\
 "bswap eax"\
 "and ecx,7"\
 "shl eax,cl"\
 "mov cl,32"\
 "sub cl,dl"\
 "shr eax,cl"\
 "readend:"\
 parm[eax][edx] value[eax] modify[ebx ecx];

#endif

//read max. 24 bits
uint32_t faad_bits_read24(bitfile *b,uint32_t bits DEBUGDEC)
{
#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)
 uint32_t ret=asm_faad_bitsread24(b,bits);
#ifdef ANALYSIS
 if(print){
  fprintf(stdout, "%4d %2d bits, val: %4d, variable: %d %s \n", dbg_count++, bits, ret, var, dbg);
  fflush(stdout);
 }
#endif
 return ret;
#else
 uint32_t ret,bitindex;
 uint8_t *bufpos;

 bufpos=b->buffer+(b->bitpos>>3);
 bitindex=b->bitpos&7;
 b->bitpos+=bits;

 if(b->bitpos>b->storedbits){
  b->error=1;
  return 0;
 }

 ret=*((uint32_t *)bufpos);
#ifndef ARCH_IS_BIG_ENDIAN
 BSWAP(ret);
#endif
 //ret<<=bitindex;
 //ret>>=32-bits;
 ret>>=32-bitindex-bits;
 ret&=bitmask[bits];

 return ret;
#endif
}

uint32_t faad_getbits(bitfile *b,uint32_t bits  DEBUGDEC)
{
 uint32_t ret;

 if(bits<=24)
  ret=faad_bits_read24(b,bits DEBUGVAR(print,var,dbg));
 else{
  ret=faad_bits_read24(b,bits-16 DEBUGVAR(print,var,dbg))<<16;
  ret|=faad_bits_read24(b,16 DEBUGVAR(print,var,dbg));
 }

 return ret;
}

#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)

long asm_faad_bitsread1(bitfile *b);

#pragma aux asm_faad_bitsread1=\
 "mov ecx,dword ptr [eax]"\
 "mov edx,ecx"\
 "inc edx"\
 "mov dword ptr [eax],edx"\
 "cmp edx,dword ptr 4[eax]"\
 "jbe read1ok"\
  "mov byte ptr 12[eax],1"\
  "xor eax,eax"\
  "jmp read1end"\
 "read1ok:"\
 "mov edx,ecx"\
 "shr edx,3"\
 "add edx,dword ptr 8[eax]"\
 "and ecx,7"\
 "inc ecx"\
 "movzx eax,byte ptr [edx]"\
 "shl al,cl"\
 "setc al"\
 "read1end:"\
 parm[eax] value[eax] modify[ecx edx];

#endif

uint32_t faad_get1bit(bitfile *b  DEBUGDEC)
{
#if defined(FAAD_USE_BITS_ASM) && defined(__WATCOMC__)
 return asm_faad_bitsread1(b);
#else
 uint32_t bitindex;
 uint8_t *bufpos;

 bitindex=b->bitpos&7;
 bufpos=b->buffer+(b->bitpos>>3);
 b->bitpos++;

 if(b->bitpos>b->storedbits){
  b->error=1;
  return 0;
 }

 return ((bufpos[0]>>(7-bitindex))&1);
#endif
}

//set the number of left bits
uint32_t faad_bits_setleft(bitfile *b,uint32_t left)
{
 uint32_t newsb=b->bitpos+left;
 if(newsb<b->storedbits){
  b->storedbits=newsb;
  return 1;
 }
 b->error=1;
 return 0;
}
