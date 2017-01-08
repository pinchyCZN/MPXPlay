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

  function: packing variable sized words into an octet stream
  last mod: $Id: bitwise.c,v 1.20 2003/02/06 00:00:00 PDSoft Exp $

 ********************************************************************/

#include "ogg.h"

ogg_uint32_t mask[33] = { 0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f,
	0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff, 0x000001ff,
	0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff,
	0x00007fff, 0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
	0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
	0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 0x1fffffff,
	0x3fffffff, 0x7fffffff, 0xffffffff
};

void oggpack_readinit(oggpack_buffer * b, unsigned char *buf, int bytes)
{
	b->bitpos = 0;
	b->storedbits = bytes << 3;
	b->buffer = buf;
}

#if !defined(OGGPACK_ASM) || !defined(__WATCOMC__)

long oggpack_look32(oggpack_buffer * b, int bits)
{
#if defined(OGGPACK_ASM) && defined(__WATCOMC__)
	return oggpack_inline_look32(b, bits);
#else
	ogg_uint32_t ret, bitindex;
	unsigned char *bufpos;

	if((b->bitpos + bits) > b->storedbits)
		return (-1);

	bitindex = b->bitpos & 7;
	bufpos = b->buffer + (b->bitpos >> 3);

	ret = *((ogg_uint32_t *) bufpos);
	ret >>= bitindex;

	if((bits + bitindex) > 32 && bitindex)
		ret |= ((ogg_uint32_t) bufpos[4]) << (32 - bitindex);

	ret &= mask[bits];

	return (ret);
#endif
}

#endif

void oggpack_adv(oggpack_buffer * b, int bits)
{
	b->bitpos += bits;
}

#if defined(OGGPACK_ASM) && defined(__WATCOMC__)

long asm_oggpackread24(oggpack_buffer * b, int bits);

#pragma aux asm_oggpackread24=\
 "mov ebx,dword ptr [eax]"\
 "mov ecx,ebx"\
 "mov esi,ebx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "and ecx,7"\
 "add esi,edx"\
 "mov dword ptr [eax],esi"\
 "cmp esi,dword ptr 4[eax]"\
 "jbe readok"\
  "mov eax,0xffffffff"\
  "jmp readend"\
 "readok:"\
 "mov eax,dword ptr [ebx]"\
 "shr eax,cl"\
 "and eax,mask[edx*4]"\
 "readend:"\
 parm[eax][edx] value[eax] modify[ebx ecx esi];

#endif

long oggpack_read24(oggpack_buffer * b, int bits)
{
#if defined(OGGPACK_ASM) && defined(__WATCOMC__)
	return asm_oggpackread24(b, bits);
#else
	ogg_uint32_t ret, bitindex;
	unsigned char *bufpos;

	bufpos = b->buffer + (b->bitpos >> 3);
	bitindex = b->bitpos & 7;
	b->bitpos += bits;

	if(b->bitpos > b->storedbits)
		return (-1);

	ret = *((ogg_uint32_t *) bufpos);
	ret >>= bitindex;
	ret &= mask[bits];

	return (ret);
#endif
}

#if defined(OGGPACK_ASM) && defined(__WATCOMC__)

long asm_oggpackread32(oggpack_buffer * b, int bits);

#pragma aux asm_oggpackread32=\
 "mov ebx,dword ptr [eax]"\
 "mov ecx,ebx"\
 "mov esi,ebx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "and ecx,7"\
 "add esi,edx"\
 "mov dword ptr [eax],esi"\
 "cmp esi,dword ptr 4[eax]"\
 "jbe readok"\
  "mov eax,0xffffffff"\
  "jmp readend"\
 "readok:"\
 "mov eax,dword ptr [ebx]"\
 "shr eax,cl"\
 "lea esi,[edx+ecx]"\
 "cmp esi,32"\
 "jbe readmask"\
  "test edx,edx"\
  "je  readmask"\
   "mov esi,ecx"\
   "mov ecx,32"\
   "sub ecx,esi"\
   "movzx ebx,byte ptr 4[ebx]"\
   "shl ebx,cl"\
   "or eax,ebx"\
 "readmask:"\
 "and eax,mask[edx*4]"\
 "readend:"\
 parm[eax][edx] value[eax] modify[ebx ecx esi];

#endif

long oggpack_read32(oggpack_buffer * b, int bits)
{
#if defined(OGGPACK_ASM) && defined(__WATCOMC__)
	return asm_oggpackread32(b, bits);
#else
	ogg_uint32_t ret, bitindex;
	unsigned char *bufpos;

	bufpos = b->buffer + (b->bitpos >> 3);
	bitindex = b->bitpos & 7;
	b->bitpos += bits;

	if(b->bitpos > b->storedbits)
		return (-1);

	ret = *((ogg_uint32_t *) bufpos);
	ret >>= bitindex;

	if((bits + bitindex) > 32 && bitindex)
		ret |= bufpos[4] << (32 - bitindex);

	ret &= mask[bits];

	return (ret);
#endif
}

#if defined(OGGPACK_ASM) && defined(__WATCOMC__)

long asm_oggpackread1(oggpack_buffer * b);

#pragma aux asm_oggpackread1=\
 "mov ebx,dword ptr [eax]"\
 "mov edx,ebx"\
 "inc edx"\
 "mov dword ptr [eax],edx"\
 "cmp edx,dword ptr 4[eax]"\
 "jbe read1ok"\
  "mov eax,0xffffffff"\
  "jmp read1end"\
 "read1ok:"\
 "mov ecx,ebx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "and ecx,7"\
 "movzx eax,byte ptr [ebx]"\
 "shr eax,cl"\
 "and eax,1"\
 "read1end:"\
 parm[eax] value[eax] modify[ebx ecx edx];

#endif

long oggpack_read1(oggpack_buffer * b)
{
#if defined(OGGPACK_ASM) && defined(__WATCOMC__)
	return asm_oggpackread1(b);
#else
	ogg_uint32_t bitindex;
	unsigned char *bufpos;

	bitindex = b->bitpos & 7;
	bufpos = b->buffer + (b->bitpos >> 3);
	b->bitpos++;

	if(b->bitpos > b->storedbits)
		return (-1);

	return ((bufpos[0] >> bitindex) & 1);
#endif
}

long oggpack_leftbits(oggpack_buffer * b)
{
	return (b->storedbits - b->bitpos);
}

#if !defined(OGGPACK_ASM) || !defined(__WATCOMC__)
void oggpack_setleft(oggpack_buffer * b, int left)
{
	b->bitpos = b->storedbits - left;
}
#endif
