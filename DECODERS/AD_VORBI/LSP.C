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
  last mod: $Id: lsp.c,v 1.21 2003/03/14 00:00:00 PDSoft Exp $

  The LSP generation code is taken (with minimal modification and a
  few bugfixes) from "On the Computation of the LSP Frequencies" by
  Joseph Rothweiler <rothwlr@altavista.net>, available at:

  http://www2.xtdl.com/~rothwlr/lsfpaper/lsfpage.html

 ********************************************************************/

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "lsp.h"
#include "os.h"
#include "scales.h"

#ifdef OGG_USE_ASM
// #define LSP_ASM 1 // faster, but FPU rounding/precision different
#endif

static float half=0.5f,two=2.0f,four=4.0f;

#ifdef LSP_ASM

static float log2_10_d20=0.166096404744368f; // log2(10)/20 (log2(10)=log(10)/log(2))

void asm_lsp2curve(ogg_double_t *,ogg_double_t *,ogg_double_t *,int);

void vorbis_lsp_to_curve(ogg_double_t *curve,ogg_double_t *lsp,
			 ogg_double_t *map_cos,
			 int n,int m,
			 ogg_double_t amp,ogg_double_t ampoffset)
{
 int n_save;
 float *lsp_save;

#pragma aux asm_lsp2curve=\
 "mov dword ptr n_save,ecx"\
 "mov dword ptr lsp_save,edx"\
 "mov esi,dword ptr m"\
 "fld dword ptr two"\
 "mov edi,esi"\
 "shr edi,1"\
 "back0:"\
  "fld dword ptr [edx]"\
  "fcos"\
  "fmul st,st(1)"\
  "fstp dword ptr [edx]"\
  "add edx,4"\
  "dec esi"\
 "jnz back0"\
 "fstp st(0)"\
 "xor esi,esi"\
 "back3:"\
  "fld dword ptr half"\
  "fld st"\
  "fld dword ptr [ebx]"\
  "mov ecx,edi"\
  "mov edx,dword ptr lsp_save"\
  "back1:"\
   "fld dword ptr [edx]"\
   "fsubr st,st(1)"\
   "fmulp st(3),st"\
   "fld dword ptr 4[edx]"\
   "fsubr st,st(1)"\
   "add edx,8"\
   "fmulp st(2),st"\
   "dec ecx"\
  "jnz back1"\
  "test byte ptr m,1"\
  "jz lspodd"\
   "fld dword ptr [edx]"\
   "fsubr st,st(1)"\
   "fmul  st,st(3)"\
   "fmul  st,st"\
   "fstp  st(3)"\
   "fmul  st,st"\
   "fsubr dword ptr four"\
   "jmp lspjump1"\
  "lspodd:"\
   "fld   dword ptr two"\
   "fadd  st,st(1)"\
   "fmul  st,st(3)"\
   "fmulp st(3),st"\
   "fsubr dword ptr two"\
  "lspjump1:"\
   "fmul  st,st(1)"\
   "fmul"\
  "fadd"\
  "fsqrt"\
  "fdivr dword ptr amp"\
  "fsub  dword ptr ampoffset"\
  "fmul  dword ptr log2_10_d20"\
  "fld1"\
  "fld  st(1)"\
  "fprem"\
  "f2xm1"\
  "fadd"\
  "fscale"\
  "mov edx,dword ptr [ebx]"\
  "fstp st(1)"\
  "back2:"\
   "fld dword ptr [eax]"\
   "fmul st,st(1)"\
   "inc esi"\
   "add ebx,4"\
   "fstp dword ptr [eax]"\
   "add eax,4"\
   "cmp edx,dword ptr [ebx]"\
  "je back2"\
  "fstp st(0)"\
 "cmp esi,dword ptr n_save"\
 "jb back3"\
 parm[eax][edx][ebx][ecx] modify[eax ebx ecx edx edi esi];

 asm_lsp2curve(curve,lsp,map_cos,n);
}

#else

void vorbis_lsp_to_curve(ogg_double_t *curve,ogg_double_t *lsp,
			 ogg_double_t *map_cos,
			 int n,int m,
			 ogg_double_t amp,ogg_double_t ampoffset)
{
 int i;
 ogg_double_t *lspp=lsp;

 i=m;
 do{
  *lspp++=cos(*lspp)*two;
 }while(--i);

 i=0;
 do{
  unsigned long mci;
  unsigned int j=m>>1;
  ogg_double_t p=half;
  ogg_double_t q=half;

  lspp=lsp;
  do{
   q *= *map_cos-lspp[0];
   p *= *map_cos-lspp[1];
   lspp+=2;
  }while(--j);

  if(m&1){
   q*=(*map_cos-lspp[0]);
   q*=q;
   p*=p*(four - (*map_cos * *map_cos));
  }else{
   q*=q*(two + *map_cos);
   p*=p*(two - *map_cos);
  }

  q=fromdB(amp/sqrt(p+q)-ampoffset);

  mci=*((unsigned long *)map_cos);
  do{
   *curve++ *=q;
   map_cos++;
   i++;
  }while(*((unsigned long *)map_cos)==mci); // x86 specific code

 }while(i<n);
}

#endif
