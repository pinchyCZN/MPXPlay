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
** $Id: huffman.h,v 1.15 2003/09/27 00:00:00 PDSoft Exp $
**/

#include <stdlib.h>
#ifdef ANALYSIS
#include <stdio.h>
#endif
#include "huffman.h"
#include "hcb.h"
#include "hcb_tab.h"

#ifdef FAAD_USE_ASM
#define USE_HUFFMAN_ASM 1
#endif

huff_cbtable_s huff_cbtables[12] = {
//hcb_tab,quad_tab,q_size,pair_tab,p_size,bin_tab,b_size,N, u_cb
	{NULL, NULL, 0, NULL, 0, NULL, 0, 0, 0}
	,							// 0
	{hcb1_1, hcb1_2, 114, NULL, 0, NULL, 0, 5, 0}
	,							// 1
	{hcb2_1, hcb2_2, 86, NULL, 0, NULL, 0, 5, 0}
	,							// 2
	{NULL, NULL, 0, NULL, 0, NULL, 161, 0, 1}
	,							// 3
	{hcb4_1, hcb4_2, 185, NULL, 0, NULL, 0, 5, 1}
	,							// 4
	{NULL, NULL, 0, NULL, 0, hcb5, 161, 0, 0}
	,							// 5
	{hcb6_1, NULL, 0, hcb6_2, 126, NULL, 0, 5, 0}
	,							// 6
	{NULL, NULL, 0, NULL, 0, hcb7, 127, 0, 1}
	,							// 7
	{hcb8_1, NULL, 0, hcb8_2, 83, NULL, 0, 5, 1}
	,							// 8
	{NULL, NULL, 0, NULL, 0, hcb9, 337, 0, 1}
	,							// 9
	{hcb10_1, NULL, 0, hcb10_2, 210, NULL, 0, 6, 1}
	,							// 10
	{hcb11_1, NULL, 0, hcb11_2, 373, NULL, 0, 5, 1}	// 11
};


uint32_t asm_hsf(bitfile * ld);

int32_t huffman_scale_factor(bitfile * ld)
{
#if defined(USE_HUFFMAN_ASM) && defined(__WATCOMC__)

#pragma aux asm_hsf=\
 "mov ebx,dword ptr [eax]"\
 "mov ecx,ebx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "mov ebx,dword ptr [ebx]"\
 "bswap ebx"\
 "and ecx,7"\
 "shl ebx,cl"\
 "xor edx,edx"\
 "mov edi,offset hcb_sf"\
 "mov esi,eax"\
 "xor eax,eax"\
 "back1:"\
  "movzx ecx,byte ptr [edi+eax*2+1]"\
  "test ecx,ecx"\
  "jz end_ok"\
  "inc edx"\
  "shl ebx,1"\
  "jc jump1"\
   "movzx ecx,byte ptr [edi+eax*2]"\
  "jump1:"\
  "add eax,ecx"\
  "cmp eax,240"\
  "jbe back1"\
  "mov eax,-1"\
  "jmp end_hsf"\
 "end_ok:"\
 "movzx eax,byte ptr [edi+eax*2]"\
 "end_hsf:"\
 "add edx,dword ptr [esi]"\
 "mov dword ptr [esi],edx"\
 "cmp edx,dword ptr 4[esi]"\
 "seta byte ptr 12[esi]"\
 parm[eax] value[eax] modify[ebx ecx edx edi esi];

	return asm_hsf(ld);

#else

	int32_t retcode;
	uint32_t offset = 0, bitcount = 0;
	uint32_t store = faad_bits_inline_show3b_h(ld);
	uint8_t *hcbsf = &hcb_sf[0][0];

	do {
		uint32_t tmp = (uint32_t) hcbsf[offset * 2 + 1];
		if(!tmp)
			goto good_out;
		if(!(store & 0x80000000))
			tmp = (uint32_t) hcbsf[offset * 2];
		offset += tmp;
		store <<= 1;
		bitcount++;
	} while(offset <= 240);

	retcode = -1;
	goto hsf_end;
  good_out:
	retcode = (uint32_t) hcbsf[offset * 2];
  hsf_end:
	faad_bits_inline_flush(ld, bitcount);
	return retcode;
#endif
}

//----------------------------------------------------------------------
void asm_hsb(bitfile * ld, int16_t * sp, uint8_t len);

static void huffman_sign_bits(bitfile * ld, int16_t * sp, uint8_t len)
{
#if defined(USE_HUFFMAN_ASM) && defined(__WATCOMC__)

#pragma aux asm_hsb=\
 "mov edi,dword ptr [eax]"\
 "mov ecx,edi"\
 "shr edi,3"\
 "add edi,dword ptr 8[eax]"\
 "mov edi,dword ptr [edi]"\
 "bswap edi"\
 "and ecx,7"\
 "shl edi,cl"\
 "xor esi,esi"\
 "back1:"\
  "mov cx,word ptr [edx]"\
  "test cx,cx"\
  "jz sp_nochg"\
   "inc esi"\
   "shl edi,1"\
   "jnc sp_nochg"\
    "neg cx"\
    "mov word ptr [edx],cx"\
  "sp_nochg:"\
  "add edx,2"\
  "dec bl"\
 "jnz back1"\
 "add esi,dword ptr [eax]"\
 "mov dword ptr [eax],esi"\
 "cmp esi,dword ptr 4[eax]"\
 "seta byte ptr 12[eax]"\
 parm[eax][edx][bl] modify[ebx ecx edx edi esi];
	asm_hsb(ld, sp, len);

#else
	uint32_t store = faad_bits_inline_show3b_h(ld), bitcount = 0;
	do {
		if(sp[0]) {
			if(store & 0x80000000)
				sp[0] = -sp[0];
			store <<= 1;
			bitcount++;
		}
		sp++;
	} while(--len);
	faad_bits_inline_flush(ld, bitcount);

#endif
}

static int16_t huffman_getescape(bitfile * ld, int16_t sp)
{
	uint8_t neg;
	int16_t off;
	uint32_t store, bitcount;

	if(sp < 0) {
		if(sp != -16)
			return sp;
		neg = 1;
	} else {
		if(sp != 16)
			return sp;
		neg = 0;
	}

	store = faad_bits_inline_show3b_h(ld);
	bitcount = 0;

	do {
		bitcount++;
		if(!(store & 0x80000000))
			break;
		store <<= 1;
	} while(1);

	faad_bits_inline_flush(ld, bitcount);
	bitcount += 3;

	off = faad_bits_read24(ld, bitcount DEBUGVAR(1, 9, "huffman_getescape(): escape"));
	off += (1 << bitcount);
	if(neg)
		off = -off;
	return off;
}

//------------------------------------------------------------------------

//cb 1,2
static uint8_t huffman_2step_quad(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint32_t cw;
	uint32_t offset;
	uint32_t extra_bits;
	hcb_2_quad *quadtab;

	cw = faad_bits_inline_show24nc(ld, cb->N);
	offset = cb->hcb_table[cw].offset;
	extra_bits = cb->hcb_table[cw].extra_bits;
	quadtab = cb->quad_table;

	if(extra_bits) {
		faad_bits_inline_flush(ld, cb->N);
		offset += faad_bits_inline_show24nc(ld, extra_bits);
		faad_bits_inline_flush(ld, quadtab[offset].bits - cb->N);
	} else {
		faad_bits_inline_flush(ld, quadtab[offset].bits);
	}

	if(offset > cb->quad_tab_size)
		return 10;

	sp[0] = quadtab[offset].x;
	sp[1] = quadtab[offset].y;
	sp[2] = quadtab[offset].v;
	sp[3] = quadtab[offset].w;

	return 0;
}

// cb 4
static uint8_t huffman_2step_quad_sign(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint8_t err = huffman_2step_quad(ld, sp, cb);
	huffman_sign_bits(ld, sp, QUAD_LEN);
	return err;
}

uint8_t asm_hbqs(bitfile * ld, int16_t * sp);

// cb 3
static uint8_t huffman_binary_quad_sign(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
#if defined(USE_HUFFMAN_ASM) && defined(__WATCOMC__)
#pragma aux asm_hbqs=\
 "push edx"\
 "mov edi,dword ptr [eax]"\
 "mov ecx,edi"\
 "shr edi,3"\
 "add edi,dword ptr 8[eax]"\
 "mov edi,dword ptr [edi]"\
 "bswap edi"\
 "and ecx,7"\
 "shl edi,cl"\
 "xor esi,esi"\
 "xor ecx,ecx"\
 "mov ebx,offset hcb3"\
 "back1:"\
  "inc esi"\
  "shl edi,1"\
  "jnc data0"\
   "movsx edx,byte ptr 2[ebx]"\
   "jmp jump1"\
  "data0:"\
   "movsx edx,byte ptr 1[ebx]"\
  "jump1:"\
  "add ecx,edx"\
  "add ebx,edx"\
  "shl edx,2"\
  "add ebx,edx"\
  "cmp byte ptr[ebx],0"\
 "je back1"\
 "pop edx"\
 "cmp ecx,161"\
 "jbe jump_ok"\
  "mov eax,10"\
  "jmp end_hbq"\
 "jump_ok:"\
 "movsx cx,byte ptr 1[ebx]"\
 "test cx,cx"\
 "jz jump2"\
  "inc esi"\
  "shl edi,1"\
  "jnc jump2"\
   "neg cx"\
 "jump2:"\
 "mov word ptr [edx],cx"\
 "movsx cx,byte ptr 2[ebx]"\
 "test cx,cx"\
 "jz jump3"\
  "inc esi"\
  "shl edi,1"\
  "jnc jump3"\
   "neg cx"\
 "jump3:"\
 "mov word ptr 2[edx],cx"\
 "movsx cx,byte ptr 3[ebx]"\
 "test cx,cx"\
 "jz jump4"\
  "inc esi"\
  "shl edi,1"\
  "jnc jump4"\
   "neg cx"\
 "jump4:"\
 "mov word ptr 4[edx],cx"\
 "movsx cx,byte ptr 4[ebx]"\
 "test cx,cx"\
 "jz jump5"\
  "inc esi"\
  "shl edi,1"\
  "jnc jump5"\
   "neg cx"\
 "jump5:"\
 "mov word ptr 6[edx],cx"\
 "add esi,dword ptr [eax]"\
 "mov dword ptr [eax],esi"\
 "cmp esi,dword ptr 4[eax]"\
 "seta byte ptr 12[eax]"\
 "xor eax,eax"\
 "end_hbq:"\
 parm[eax][edx] value[al] modify[ebx ecx edx edi esi];
	return asm_hbqs(ld, sp);

#else

	uint32_t store = faad_bits_inline_show3b_h(ld), bitcount = 0;
	uint32_t offset = 0;
	hcb_bin_quad *hcbtab = &hcb3[0];

	while(!hcbtab[offset].is_leaf) {
		if(store & 0x80000000)
			offset += hcbtab[offset].data[1];
		else
			offset += hcbtab[offset].data[0];
		store <<= 1;
		bitcount++;
	}

	faad_bits_inline_flush(ld, bitcount);

	if(offset > cb->bin_tab_size)
		return 10;

	sp[0] = hcbtab[offset].data[0];
	sp[1] = hcbtab[offset].data[1];
	sp[2] = hcbtab[offset].data[2];
	sp[3] = hcbtab[offset].data[3];

	huffman_sign_bits(ld, sp, QUAD_LEN);

	return 0;
#endif
}

// cb 6
static uint8_t huffman_2step_pair(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint32_t cw;
	uint32_t offset;
	uint32_t extra_bits;
	hcb_2_pair *pairtab;

	cw = faad_bits_inline_show24nc(ld, cb->N);
	offset = cb->hcb_table[cw].offset;
	extra_bits = cb->hcb_table[cw].extra_bits;
	pairtab = cb->pair_table;

	if(extra_bits) {
		faad_bits_inline_flush(ld, cb->N);
		offset += faad_bits_inline_show24nc(ld, extra_bits);
		faad_bits_inline_flush(ld, pairtab[offset].bits - cb->N);
	} else {
		faad_bits_inline_flush(ld, pairtab[offset].bits);
	}

	if(offset > cb->pair_tab_size)
		return 10;

	sp[0] = pairtab[offset].x;
	sp[1] = pairtab[offset].y;

	return 0;
}

// cb 8,10
static uint8_t huffman_2step_pair_sign(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint8_t err = huffman_2step_pair(ld, sp, cb);
	huffman_sign_bits(ld, sp, PAIR_LEN);
	return err;
}

// cb 5
static uint8_t huffman_binary_pair(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint32_t store = faad_bits_inline_show3b_h(ld), bitcount = 0;
	uint32_t offset = 0;
	hcb_bin_pair *bintab = cb->bin_table;

	while(!bintab[offset].is_leaf) {
		if(store & 0x80000000)
			offset += bintab[offset].data[1];
		else
			offset += bintab[offset].data[0];
		store <<= 1;
		bitcount++;
	}

	faad_bits_inline_flush(ld, bitcount);

	if(offset > cb->bin_tab_size)
		return 10;

	sp[0] = bintab[offset].data[0];
	sp[1] = bintab[offset].data[1];

	return 0;
}

// cb 7,9
static uint8_t huffman_binary_pair_sign(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint8_t err = huffman_binary_pair(ld, sp, cb);
	huffman_sign_bits(ld, sp, PAIR_LEN);
	return err;
}

static __inline int16_t huffman_codebook(uint8_t i)
{
	const uint32_t data = 16428320;
	if(i == 0)
		return (int16_t) (data >> 16) & 0xFFFF;
	return (int16_t) data & 0xFFFF;
}

static uint8_t huff_bookhand_11(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint8_t err = huffman_2step_pair_sign(ld, sp, &huff_cbtables[11]);
	sp[0] = huffman_getescape(ld, sp[0]);
	sp[1] = huffman_getescape(ld, sp[1]);
	return err;
}

static uint8_t huff_bookhand_12(bitfile * ld, int16_t * sp, huff_cbtable_s * cb)
{
	uint8_t err = huffman_2step_quad(ld, sp, &huff_cbtables[1]);
	sp[0] = huffman_codebook(0);
	sp[1] = huffman_codebook(1);
	return err;
}

typedef uint8_t(*hsd_case_t) (bitfile * ld, int16_t * sp, huff_cbtable_s * cb);

static hsd_case_t hsd_cases[] = {
	NULL,						// 0
	&huffman_2step_quad,		// 1
	&huffman_2step_quad,		// 2
	&huffman_binary_quad_sign,	// 3
	&huffman_2step_quad_sign,	// 4
	&huffman_binary_pair,		// 5
	&huffman_2step_pair,		// 6
	&huffman_binary_pair_sign,	// 7
	&huffman_2step_pair_sign,	// 8
	&huffman_binary_pair_sign,	// 9
	&huffman_2step_pair_sign,	// 10
	&huff_bookhand_11,			// 11
	&huff_bookhand_12,			// 12
	NULL,						// 13
	NULL,						// 14
	NULL						// 15
};

#include "syntax.h"

uint8_t huffman_spectral_data_block(bitfile * ld, int16_t * sp, uint32_t cb, uint32_t len)
{
	hsd_case_t hsd_case;
	huff_cbtable_s *cbtab;
	unsigned int step = PAIR_LEN;

	len >>= 1;
	if(cb < FIRST_PAIR_HCB) {
		len >>= 1;
		step <<= 1;
	}
#ifdef AAC_DEBUGINFO
	fprintf(stdout, "huffman_spectral_data_block() cb:%d len:%d step:%d\n", cb, len, step);
#endif

	if(!len)					// ???
		return 0;

	if(cb >= 16)
		cb = 11;

	hsd_case = hsd_cases[cb];
	if(hsd_case) {
		cbtab = &huff_cbtables[cb];
		do {
			uint8_t result;
			if((result = hsd_case(ld, sp, cbtab)))
				return result;
			sp += step;
		} while(--len);
		return 0;
	}

	return 11;
}

/*uint8_t huffman_spectral_data(bitfile *ld, int16_t *sp, uint32_t cb)
{
 hsd_case_t hsd_case;

 if(cb>=16)
  cb=11;

 hsd_case=hsd_cases[cb];
 if(hsd_case)
  return hsd_case(ld,sp,&huff_cbtables[cb]);

 return 11;
}
*/
