;**************************************************************************
;*                     This file is part of the                           *
;*                      Mpxplay - audio player.                           *
;*                  The source code of Mpxplay is                         *
;*        (C) copyright 1998-2006 by PDSoft (Attila Padar)                *
;*                    http://mpxplay.cjb.net                              *
;*                  email: mpxplay@freemail.hu                            *
;**************************************************************************
;*  This program is distributed in the hope that it will be useful,       *
;*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
;*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
;*  Please contact with the author (with me) if you want to use           *
;*  or modify this source.                                                *
;**************************************************************************
;function: Audio MPEG Layer II & III sub-routines (subband-synthesis)
;based on the MPG123 MPEG decoder source + ideas from some others
;
;16-bit integer output (rename it to synth.asm and enable MPXDEC_INTEGER_OUTPUT in mpxdec.h)
;

.387
.386p
.model flat
		PUBLIC	mpxdec_synth_granule_
		PUBLIC	mpxdec_synth_init_
DGROUP		GROUP	CONST,CONST2,_DATA,_BSS
_TEXT		SEGMENT	BYTE PUBLIC USE32 'CODE'
		ASSUME CS:_TEXT, DS:DGROUP, SS:DGROUP

;-------------------------------------------------------------------------
;eax=bandPtr edx=((bo<<8)|channels) ebx=pcmout ecx=synth_rollbuff
mpxdec_synth_granule_:
 push      ecx
 push      esi
 push      edi
 push      ebp
 mov       ebp,esp
 sub       esp,000000a4H
 sub       ebp,00000082H

; 62H[ebp] : bo1
; 66H[ebp] : loc_bandptr
; 6eH[ebp] : bo
; 72H[ebp] : loc_rollbuff
; 76H[ebp] : b0
; 7aH[ebp] : loc_pcmout
; 7eH[ebp] : ss

 mov  dword ptr 66H[ebp],eax         ;loc_bandptr=bandPtr_p
 mov  dword ptr 72H[ebp],ecx         ;loc_rollbuff=synth_rollbuff
 mov  dword ptr 7aH[ebp],ebx         ;loc_pcmout=pcmout
 mov  eax,edx                        ;
 shr  eax,8                          ;
 mov  dword ptr 6eH[ebp],eax         ;bo=(edx>>8)
 and  edx,0x7f                       ;channels=edx&0x7f

 mov  dword ptr 7eH[ebp],18          ;ss=18
 cmp  edx,2                          ;
 jne  L$19                           ;if(channels==2){
L$1:                                  ;do{ // while(--ss)
  mov  ebx,dword ptr 6eH[ebp]          ;
  sub  ebx,4                           ;bo--
  and  ebx,60                          ;bo&=0xf
  mov  dword ptr 6eH[ebp],ebx          ;
  mov  edi,ebx                         ;bo1=bo
  mov  eax,dword ptr 72H[ebp]          ;bufp0 = loc_rollbuff
  mov  ecx,eax                         ;b0=bufp0
  test bl,4                            ;
  jz   L$2                             ;if(bo&1){
   lea  edx,[eax+ebx]                  ; bufp1=bufp0+bo
   add  ebx,4                          ;
   and  ebx,60                         ;
   lea  eax,[eax+ebx+440H]             ; bufp0+=((bo+1)&0xf)+SYNTHBUF_SIZE
  jmp  L$3                             ;}else{
L$2:                                   ;
   add  eax,ebx                        ; bufp0+=bo
   lea  edx,[eax+444H]                 ; bufp1=bufp0+SYNTHBUF_SIZE+1
   add  ecx,440H                       ; b0+=SYNTHBUF_SIZE
   add  edi,4                          ; bo1++
L$3:                                   ;}
  mov  dword ptr 76H[ebp],ecx          ;->b0
  mov  dword ptr 62H[ebp],edi          ;->bo1
  lea  ebx,-22H[ebp]                   ;tmpbuf
  lea  ecx,1eH[ebp]                    ;tmpbuf+0x10
  mov  edi,dword ptr 66H[ebp]          ;bandPtr=loc_bandptr;
  mov  esi,offset FLAT:_dct64costab    ;dct64costab
  call near ptr FLAT:dct64_1_          ;dct64_1(bufp0,bufp1,b1,b2,bandPtr,dct64costab)
  add  eax,880H                        ;bufp0+=2*SYNTHBUF_SIZE;
  add  edx,880H                        ;bufp1+=2*SYNTHBUF_SIZE;
  add  edi,2304                        ;bandPtr+=SBLIMIT*SSLIMIT;
  call near ptr FLAT:dct64_1_          ;dct64_1(bufp0,bufp1,b1,b2,bandPtr,dct64costab)
  sub  edi,2176                        ;bandPtr-=(SSLIMIT-1)*SBLIMIT;
  mov  dword ptr 66H[ebp],edi          ;loc_bandptr=bandPtr;

  mov  ebx,dword ptr 7aH[ebp]          ;pcm_out=loc_pcmout
  mov  edx,dword ptr FLAT:_decwin16    ;
  sub  edx,dword ptr 62H[ebp]          ;window = decwin16 - bo1;
  mov  edi,dword ptr 76H[ebp]          ;b0
  lea  esi,880H[edi]                   ;b1=b0+2*SYNTHBUF_SIZE
  mov  ecx,16                          ;j=16
L$4:                                   ;do{
   fld    dword ptr  4[edx]             ;
   fld    dword ptr  4[edi]             ;
   fmul   st,st(1)                      ;suml  = window[0x1] * b0[0x1]
   fld    dword ptr  4[esi]             ;
   fmulp  st(2),st                      ;sumr  = window[0x1] * b1[0x1]
   fld    dword ptr  0[edx]             ;
   fld    dword ptr  0[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0x0] * b0[0x0]
   fmul   dword ptr  0[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0x0] * b1[0x0]
   fld    dword ptr  8[edx]             ;
   fld    dword ptr  8[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0x2] * b0[0x2]
   fmul   dword ptr  8[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0x2] * b1[0x2]
   fld    dword ptr 12[edx]             ;
   fld    dword ptr 12[edi]             ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[0x3] * b0[0x3]
   fmul   dword ptr 12[esi]             ;
   faddp  st(2),st                      ;sumr += window[0x3] * b1[0x3]
   fld    dword ptr 16[edx]             ;
   fld    dword ptr 16[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0x4] * b0[0x4]
   fmul   dword ptr 16[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0x4] * b1[0x4]
   fld    dword ptr 20[edx]             ;
   fld    dword ptr 20[edi]             ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[0x5] * b0[0x5]
   fmul   dword ptr 20[esi]             ;
   faddp  st(2),st                      ;sumr += window[0x5] * b1[0x5]
   fld    dword ptr 24[edx]             ;
   fld    dword ptr 24[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0x6] * b0[0x6]
   fmul   dword ptr 24[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0x6] * b1[0x6]
   fld    dword ptr 28[edx]             ;
   fld    dword ptr 28[edi]             ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[0x7] * b0[0x7]
   fmul   dword ptr 28[esi]             ;
   faddp  st(2),st                      ;sumr += window[0x7] * b1[0x7]
   fld    dword ptr 32[edx]             ;
   fld    dword ptr 32[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0x8] * b0[0x8]
   fmul   dword ptr 32[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0x8] * b1[0x8]
   fld    dword ptr 36[edx]             ;
   fld    dword ptr 36[edi]             ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[0x9] * b0[0x9]
   fmul   dword ptr 36[esi]             ;
   faddp  st(2),st                      ;sumr += window[0x9] * b1[0x9]
   fld    dword ptr 40[edx]             ;
   fld    dword ptr 40[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0xA] * b0[0xA]
   fmul   dword ptr 40[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0xA] * b1[0xA]
   fld    dword ptr 44[edx]             ;
   fld    dword ptr 44[edi]             ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[0xB] * b0[0xB]
   fmul   dword ptr 44[esi]             ;
   faddp  st(2),st                      ;sumr += window[0xB] * b1[0xB]
   fld    dword ptr 48[edx]             ;
   fld    dword ptr 48[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0xC] * b0[0xC]
   fmul   dword ptr 48[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0xC] * b1[0xC]
   fld    dword ptr 52[edx]             ;
   fld    dword ptr 52[edi]             ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[0xD] * b0[0xD]
   fmul   dword ptr 52[esi]             ;
   faddp  st(2),st                      ;sumr += window[0xD] * b1[0xD]
   fld    dword ptr 56[edx]             ;
   fld    dword ptr 56[edi]             ;
   fmul   st,st(1)                      ;
   fsubp  st(2),st                      ;suml -= window[0xE] * b0[0xE]
   fmul   dword ptr 56[esi]             ;
   fsubp  st(2),st                      ;sumr -= window[0xE] * b1[0xE]
   fld    dword ptr 60[edx]             ;
   fld    dword ptr 60[edi]             ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[0xF] * b0[0xF]
   fmul   dword ptr 60[esi]             ;
   faddp  st(2),st                      ;sumr += window[0xF] * b1[0xF]
   add    edx,128                       ;window+=32
   add    edi,64                        ;b0+=16
   add    esi,64                        ;b1+=16
   fistp  dword ptr [ebx]               ;
   movsx  eax,word ptr [ebx]            ;
   cmp    eax,dword ptr [ebx]           ;
   je     L$6                           ;
   test    dword ptr [ebx],80000000H    ;
   je      L$5                          ;
   mov      word ptr [ebx],8000H        ;
   jmp     L$6                          ;
L$5:                                    ;
   mov      word ptr [ebx],7fffH        ;
L$6:                                    ;
   add    ebx,2                         ;
   fistp  dword ptr [ebx]               ;
   movsx  eax,word ptr [ebx]            ;
   cmp    eax,dword ptr [ebx]           ;
   je     L$8                           ;
   test    dword ptr [ebx],80000000H    ;
   je      L$7                          ;
   mov      word ptr [ebx],8000H        ;
   jmp     L$8                          ;
L$7:                                    ;
   mov      word ptr [ebx],7fffH        ;
L$8:                                    ;
   add    ebx,2                         ;
   dec   ecx                            ;j--
  jne    L$4                           ;}while(j)

  fld    dword ptr   [edx]             ;
  fchs                                 ;
  fld    st                            ;
  fmul   dword ptr   [edi]             ;suml  =-window[0x0] * b0[0x0]
  fld    dword ptr   [esi]             ;
  fmulp  st(2),st                      ;sumr  =-window[0x0] * b1[0x0]
  fld    dword ptr  8[edx]             ;
  fld    dword ptr  8[edi]             ;
  fmul   st,st(1)                      ;
  fsubp  st(2),st                      ;suml -= window[0x2] * b0[0x2]
  fmul   dword ptr  8[esi]             ;
  fsubp  st(2),st                      ;sumr -= window[0x2] * b1[0x2]
  fld    dword ptr 16[edx]             ;
  fld    dword ptr 16[edi]             ;
  fmul   st,st(1)                      ;
  fsubp  st(2),st                      ;suml -= window[0x4] * b0[0x4]
  fmul   dword ptr 16[esi]             ;
  fsubp  st(2),st                      ;sumr -= window[0x4] * b1[0x4]
  fld    dword ptr 24[edx]             ;
  fld    dword ptr 24[edi]             ;
  fmul   st,st(1)                      ;
  fsubp  st(2),st                      ;suml -= window[0x6] * b0[0x6]
  fmul   dword ptr 24[esi]             ;
  fsubp  st(2),st                      ;sumr -= window[0x6] * b1[0x6]
  fld    dword ptr 32[edx]             ;
  fld    dword ptr 32[edi]             ;
  fmul   st,st(1)                      ;
  fsubp  st(2),st                      ;suml -= window[0x8] * b0[0x8]
  fmul   dword ptr 32[esi]             ;
  fsubp  st(2),st                      ;sumr -= window[0x8] * b1[0x8]
  fld    dword ptr 40[edx]             ;
  fld    dword ptr 40[edi]             ;
  fmul   st,st(1)                      ;
  fsubp  st(2),st                      ;suml -= window[0xA] * b0[0xA]
  fmul   dword ptr 40[esi]             ;
  fsubp  st(2),st                      ;sumr -= window[0xA] * b1[0xA]
  fld    dword ptr 48[edx]             ;
  fld    dword ptr 48[edi]             ;
  fmul   st,st(1)                      ;
  fsubp  st(2),st                      ;suml -= window[0xC] * b0[0xC]
  fmul   dword ptr 48[esi]             ;
  fsubp  st(2),st                      ;sumr -= window[0xC] * b1[0xC]
  fld    dword ptr 56[edx]             ;
  fld    dword ptr 56[edi]             ;
  fmul   st,st(1)                      ;
  fsubp  st(2),st                      ;suml -= window[0xE] * b0[0xE]
  fmul   dword ptr 56[esi]             ;
  fsubp  st(2),st                      ;sumr -= window[0xE] * b1[0xE]
  sub    edi,64                        ;b0-=16
  sub    esi,64                        ;b1-=16
  mov    ecx,dword ptr 62H[ebp]        ;
  shl    ecx,1                         ;
  sub    ecx,132                       ;
  add    edx,ecx                       ;window+=(bo1<<1)-33
  fistp  dword ptr [ebx]               ;
  movsx  eax,word ptr [ebx]            ;
  cmp    eax,dword ptr [ebx]           ;
  je     L$10                          ;
   test   dword ptr [ebx],80000000H    ;
   je     L$9                          ;
    mov    word ptr [ebx],8000H        ;
   jmp    L$10                         ;
L$9:                                   ;
    mov    word ptr [ebx],7fffH        ;
L$10:                                  ;
  add    ebx,2                         ;
  fistp  dword ptr [ebx]               ;
  movsx  eax,word ptr [ebx]            ;
  cmp    eax,dword ptr [ebx]           ;
  je     L$12                          ;
   test   dword ptr [ebx],80000000H    ;
   je     L$11                         ;
    mov    word ptr [ebx],8000H        ;
   jmp    L$12                         ;
L$11:                                  ;
    mov    word ptr [ebx],7fffH        ;
L$12:                                  ;
  add    ebx,2                         ;

  mov    ecx,15                        ;j=15
L$13:                                  ;do{
   fld    dword ptr   0[edx]            ;
   fld    dword ptr   0[edi]            ;
   fmul   st,st(1)                      ;suml  = window[ 0x0] * b0[0x0]
   fld    dword ptr   0[esi]            ;
   fmulp  st(2),st                      ;sumr  = window[ 0x0] * b1[0x0]
   fld    dword ptr  -4[edx]            ;
   fld    dword ptr   4[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x1] * b0[0x1]
   fmul   dword ptr   4[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x1] * b1[0x1]
   fld    dword ptr  -8[edx]            ;
   fld    dword ptr   8[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x2] * b0[0x2]
   fmul   dword ptr   8[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x2] * b1[0x2]
   fld    dword ptr -12[edx]            ;
   fld    dword ptr  12[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x3] * b0[0x3]
   fmul   dword ptr  12[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x3] * b1[0x3]
   fld    dword ptr -16[edx]            ;
   fld    dword ptr  16[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x4] * b0[0x4]
   fmul   dword ptr  16[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x4] * b1[0x4]
   fld    dword ptr -20[edx]            ;
   fld    dword ptr  20[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x5] * b0[0x5]
   fmul   dword ptr  20[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x5] * b1[0x5]
   fld    dword ptr -24[edx]            ;
   fld    dword ptr  24[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x6] * b0[0x6]
   fmul   dword ptr  24[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x6] * b1[0x6]
   fld    dword ptr -28[edx]            ;
   fld    dword ptr  28[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x7] * b0[0x7]
   fmul   dword ptr  28[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x7] * b1[0x7]
   fld    dword ptr -32[edx]            ;
   fld    dword ptr  32[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x8] * b0[0x8]
   fmul   dword ptr  32[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x8] * b1[0x8]
   fld    dword ptr -36[edx]            ;
   fld    dword ptr  36[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0x9] * b0[0x9]
   fmul   dword ptr  36[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0x9] * b1[0x9]
   fld    dword ptr -40[edx]            ;
   fld    dword ptr  40[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0xA] * b0[0xA]
   fmul   dword ptr  40[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0xA] * b1[0xA]
   fld    dword ptr -44[edx]            ;
   fld    dword ptr  44[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0xB] * b0[0xB]
   fmul   dword ptr  44[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0xB] * b1[0xB]
   fld    dword ptr -48[edx]            ;
   fld    dword ptr  48[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0xC] * b0[0xC]
   fmul   dword ptr  48[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0xC] * b1[0xC]
   fld    dword ptr -52[edx]            ;
   fld    dword ptr  52[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0xD] * b0[0xD]
   fmul   dword ptr  52[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0xD] * b1[0xD]
   fld    dword ptr -56[edx]            ;
   fld    dword ptr  56[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0xE] * b0[0xE]
   fmul   dword ptr  56[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0xE] * b1[0xE]
   fld    dword ptr -60[edx]            ;
   fld    dword ptr  60[edi]            ;
   fmul   st,st(1)                      ;
   faddp  st(2),st                      ;suml += window[-0xF] * b0[0xF]
   fmul   dword ptr  60[esi]            ;
   faddp  st(2),st                      ;sumr += window[-0xF] * b1[0xF]
   sub    edx,128                       ;window-=32
   sub    edi,64                        ;b0-=16
   sub    esi,64                        ;b1-=16
   fistp  dword ptr [ebx]               ;
   movsx  eax,word ptr [ebx]            ;
   cmp    eax,dword ptr [ebx]           ;
   je     L$15                          ;
    test   dword ptr [ebx],80000000H    ;
    je     L$14                         ;
     mov    word ptr [ebx],8000H        ;
    jmp    L$15                         ;
L$14:                                   ;
     mov    word ptr [ebx],7fffH        ;
L$15:                                   ;
   add    ebx,2                         ;
   fistp  dword ptr [ebx]               ;
   movsx  eax,word ptr [ebx]            ;
   cmp    eax,dword ptr [ebx]           ;
   je     L$17                          ;
    test   dword ptr [ebx],80000000H    ;
    je     L$16                         ;
     mov    word ptr [ebx],8000H        ;
    jmp    L$17                         ;
L$16:                                   ;
     mov    word ptr [ebx],7fffH        ;
L$17:                                   ;
   add    ebx,2                         ;
   dec   ecx                            ;j--
  jne    L$13                          ;}while(j)
  mov   dword ptr 7aH[ebp],ebx         ;loc_pcmout=pcm_out
  dec   dword ptr 7eH[ebp]             ;ss--
 jnz   L$1                             ;}while(ss)
jmp  near ptr L$30                   ;}else{ // channels==1
L$19:                                  ;do{ // while(--ss)
  mov  ebx,dword ptr 6eH[ebp]          ;
  sub  ebx,4                           ;bo--
  and  ebx,60                          ;bo&=0xf
  mov  dword ptr 6eH[ebp],ebx          ;
  mov  edi,ebx                         ;bo1=bo
  mov  eax,dword ptr 72H[ebp]          ;bufp0 = loc_rollbuf
  mov  ecx,eax                         ;b0=bufp0
  test bl,4                            ;
  jz   L$20                            ;if(bo&1){
   lea  edx,[eax+ebx]                  ; bufp1=bufp0+bo
   add  ebx,4                          ;
   and  ebx,60                         ;
   lea  eax,[eax+ebx+440H]             ; bufp0+=((bo+1)&0xf)+SYNTHBUF_SIZE
  jmp  L$21                            ;}else{
L$20:                                  ;
   add  eax,ebx                        ; bufp0+=bo
   lea  edx,[eax+444H]                 ; bufp1=bufp0+SYNTHBUF_SIZE+1
   add  ecx,440H                       ; b0+=SYNTHBUF_SIZE
   add  edi,4                          ; bo1++
L$21:                                  ;}
  mov  dword ptr 76H[ebp],ecx          ;->b0
  mov  dword ptr 62H[ebp],edi          ;->bo1
  lea  ebx,-22H[ebp]                   ;tmpbuf
  lea  ecx,1eH[ebp]                    ;tmpbuf+0x10
  mov  edi,dword ptr 66H[ebp]          ;bandPtr=loc_bandptr;
  mov  esi,offset FLAT:_dct64costab    ;dct64costab
  call near ptr FLAT:dct64_1_          ;dct64_1(bufp0,bufp1,b1,b2,bandPtr,dct64costab)
  add  edi,128                         ;bandPtr+=SBLIMIT;
  mov  dword ptr 66H[ebp],edi          ;loc_bandptr=bandPtr;

  mov  ebx,dword ptr 7aH[ebp]          ;pcm_out=loc_pcmout
  mov  edx,dword ptr FLAT:_decwin16    ;
  sub  edx,dword ptr 62H[ebp]          ;window = decwin16 - bo1;
  mov  edi,dword ptr 76H[ebp]          ;b0
  mov  ecx,16                          ;j=16
L$22:
    fld       dword ptr 4H[edx]
    fmul      dword ptr 4H[edi]
    fld       dword ptr [edx]
    fmul      dword ptr [edi]
    fsubp     st(1),st
    fld       dword ptr 8H[edx]
    fmul      dword ptr 8H[edi]
    fsubp     st(1),st
    fld       dword ptr 0cH[edx]
    fmul      dword ptr 0cH[edi]
    faddp     st(1),st
    fld       dword ptr 10H[edx]
    fmul      dword ptr 10H[edi]
    fsubp     st(1),st
    fld       dword ptr 14H[edx]
    fmul      dword ptr 14H[edi]
    faddp     st(1),st
    fld       dword ptr 18H[edx]
    fmul      dword ptr 18H[edi]
    fsubp     st(1),st
    fld       dword ptr 1cH[edx]
    fmul      dword ptr 1cH[edi]
    faddp     st(1),st
    fld       dword ptr 20H[edx]
    fmul      dword ptr 20H[edi]
    fsubp     st(1),st
    fld       dword ptr 24H[edx]
    fmul      dword ptr 24H[edi]
    faddp     st(1),st
    fld       dword ptr 28H[edx]
    fmul      dword ptr 28H[edi]
    fsubp     st(1),st
    fld       dword ptr 2cH[edx]
    fmul      dword ptr 2cH[edi]
    faddp     st(1),st
    fld       dword ptr 30H[edx]
    fmul      dword ptr 30H[edi]
    fsubp     st(1),st
    fld       dword ptr 34H[edx]
    fmul      dword ptr 34H[edi]
    faddp     st(1),st
    fld       dword ptr 38H[edx]
    fmul      dword ptr 38H[edi]
    fsubp     st(1),st
    fld       dword ptr 3cH[edx]
    fmul      dword ptr 3cH[edi]
    faddp     st(1),st
    add       edx,00000080H
    add       edi,00000040H
    fistp     dword ptr [ebx]
    movsx     eax,word ptr [ebx]
    cmp       eax,dword ptr [ebx]
    je        L$24
    test      dword ptr [ebx],80000000H
    je        L$23
    mov       word ptr [ebx],8000H
    jmp       L$24
L$23:
    mov       word ptr [ebx],7fffH
L$24:
    add       ebx,00000002H
    dec       ecx
    jne       L$22

    fld       dword ptr [edx]
    fchs      
    fmul      dword ptr [edi]
    fld       dword ptr 8H[edx]
    fmul      dword ptr 8H[edi]
    fsubp     st(1),st
    fld       dword ptr 10H[edx]
    fmul      dword ptr 10H[edi]
    fsubp     st(1),st
    fld       dword ptr 18H[edx]
    fmul      dword ptr 18H[edi]
    fsubp     st(1),st
    fld       dword ptr 20H[edx]
    fmul      dword ptr 20H[edi]
    fsubp     st(1),st
    fld       dword ptr 28H[edx]
    fmul      dword ptr 28H[edi]
    fsubp     st(1),st
    fld       dword ptr 30H[edx]
    fmul      dword ptr 30H[edi]
    fsubp     st(1),st
    fld       dword ptr 38H[edx]
    fmul      dword ptr 38H[edi]
    fsubp     st(1),st
    sub       edi,00000040H
    mov       ecx,dword ptr 62H[ebp]
    shl       ecx,1
    sub       ecx,00000084H
    add       edx,ecx
    fistp     dword ptr [ebx]
    movsx     eax,word ptr [ebx]
    cmp       eax,dword ptr [ebx]
    je        L$26
    test      dword ptr [ebx],80000000H
    je        L$25
    mov       word ptr [ebx],8000H
    jmp       L$26
L$25:
    mov       word ptr [ebx],7fffH
L$26:
    add       ebx,00000002H

    mov       ecx,0000000fH
L$27:
    fld       dword ptr [edx]
    fmul      dword ptr [edi]
    fld       dword ptr -4H[edx]
    fmul      dword ptr 4H[edi]
    faddp     st(1),st
    fld       dword ptr -8H[edx]
    fmul      dword ptr 8H[edi]
    faddp     st(1),st
    fld       dword ptr -0cH[edx]
    fmul      dword ptr 0cH[edi]
    faddp     st(1),st
    fld       dword ptr -10H[edx]
    fmul      dword ptr 10H[edi]
    faddp     st(1),st
    fld       dword ptr -14H[edx]
    fmul      dword ptr 14H[edi]
    faddp     st(1),st
    fld       dword ptr -18H[edx]
    fmul      dword ptr 18H[edi]
    faddp     st(1),st
    fld       dword ptr -1cH[edx]
    fmul      dword ptr 1cH[edi]
    faddp     st(1),st
    fld       dword ptr -20H[edx]
    fmul      dword ptr 20H[edi]
    faddp     st(1),st
    fld       dword ptr -24H[edx]
    fmul      dword ptr 24H[edi]
    faddp     st(1),st
    fld       dword ptr -28H[edx]
    fmul      dword ptr 28H[edi]
    faddp     st(1),st
    fld       dword ptr -2cH[edx]
    fmul      dword ptr 2cH[edi]
    faddp     st(1),st
    fld       dword ptr -30H[edx]
    fmul      dword ptr 30H[edi]
    faddp     st(1),st
    fld       dword ptr -34H[edx]
    fmul      dword ptr 34H[edi]
    faddp     st(1),st
    fld       dword ptr -38H[edx]
    fmul      dword ptr 38H[edi]
    faddp     st(1),st
    fld       dword ptr -3cH[edx]
    fmul      dword ptr 3cH[edi]
    faddp     st(1),st
    sub       edx,00000080H
    sub       edi,00000040H
    fistp     dword ptr [ebx]
    movsx     eax,word ptr [ebx]
    cmp       eax,dword ptr [ebx]
    je        L$29
    test      dword ptr [ebx],80000000H
    je        L$28
    mov       word ptr [ebx],8000H
    jmp       L$29
L$28:
    mov       word ptr [ebx],7fffH
L$29:
    add       ebx,00000002H
    dec       ecx
   jne       L$27
   mov   dword ptr 7aH[ebp],ebx      ;loc_pcmout=pcm_out
   dec   dword ptr 7eH[ebp]          ;ss--
  jnz   L$19                        ;}while(ss)

L$30:
mov       eax,dword ptr 7aH[ebp]
lea       esp,82H[ebp]
pop       ebp
pop       edi
pop       esi
pop       ecx
ret

;---------------------------------------------------------------------------
; eax=out0, edx=out1, ebx=b1, ecx=b2, edi=bandPtr, esi=dct64costab
dct64_1_:

fld   dword ptr   0[edi]     ;
 fadd  dword ptr 124[edi]    ; tmp0=samples[0x00]+samples[0x1f]
 fld   st                    ;
 fld   dword ptr  60[edi]    ;
 fadd  dword ptr  64[edi]    ; tmp1=samples[0x0f]+samples[0x10]
 fadd  st(2),st              ; tmp2=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  64[esi]    ; tmp3=(tmp0-tmp1)*costab[16]
fld   dword ptr  28[edi]     ;
 fadd  dword ptr  96[edi]    ; tmp0=samples[0x07]+samples[0x18]
 fld   dword ptr  32[edi]    ;
 fadd  dword ptr  92[edi]    ; tmp1=samples[0x08]+samples[0x17]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   0[ebx]    ; b1[0x00]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  92[esi]    ; tmp4=(tmp0-tmp1)*costab[23]
fld   dword ptr  12[edi]     ;
 fadd  dword ptr 112[edi]    ; tmp0=samples[0x03]+samples[0x1c]
 fld   dword ptr  48[edi]    ;
 fadd  dword ptr  76[edi]    ; tmp1=samples[0x0c]+samples[0x13]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   4[ebx]    ; b1[0x01]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  76[esi]    ; tmp5=(tmp0-tmp1)*costab[19]
fld   dword ptr  16[edi]     ;
 fadd  dword ptr 108[edi]    ; tmp0=samples[0x04]+samples[0x1b]
 fld   dword ptr  44[edi]    ;
 fadd  dword ptr  80[edi]    ; tmp1=samples[0x0b]+samples[0x14]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   8[ebx]    ; b1[0x02]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  80[esi]    ; tmp6=(tmp0-tmp1)*costab[20]
fld   st(3)                  ;
 fadd  st,st(3)              ; tmp0=tmp3+tmp4
 fld   st(2)                 ;
 fadd  st,st(2)              ; tmp1=tmp5+tmp6
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  32[ecx]    ; b2[0x08]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  44[ecx]    ; b2[0x0b]=(tmp0-tmp1)*costab[28]
fsubp st(1),st               ;
 fmul  dword ptr 108[esi]    ; tmp0=(tmp3-tmp4)*costab[27]
 fxch  st(2)                 ;
 fsubrp st(1),st             ;
 fmul  dword ptr  96[esi]    ; tmp1=(tmp5-tmp6)*costab[24]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  48[ecx]    ; b2[0x0c]=tmp0+tmp1
 fsubrp st(1),st             ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  60[ecx]    ; b2[0x0f]=(tmp0-tmp1)*costab[28]
fld   st                     ;
 fadd  dword ptr   0[ebx]    ; tmp0=tmp2+b1[0x00]
 fld   dword ptr   4[ebx]    ;
 fadd  dword ptr   8[ebx]    ; tmp1=b1[0x01]+b1[0x02]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   0[ecx]    ; b2[0x00]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  12[ecx]    ; b2[0x03]=(tmp0-tmp1)*costab[28]
fsub  dword ptr   0[ebx]     ;
 fmul  dword ptr  96[esi]    ; tmp0=(tmp2-b1[0x00])*costab[24]
 fld   dword ptr   4[ebx]    ;
 fsub  dword ptr   8[ebx]    ;
 fmul  dword ptr 108[esi]    ; tmp1=(b1[0x01]-b1[0x02])*costab[27]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  16[ecx]    ; b2[0x04]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  28[ecx]    ; b2[0x07]=(tmp0-tmp1)*costab[28]
;-----------------------------------------------------------
fld   dword ptr   4[edi]     ;
 fadd  dword ptr 120[edi]    ; tmp0=samples[0x01]+samples[0x1e]
 fld   st                    ;
 fld   dword ptr  56[edi]    ;
 fadd  dword ptr  68[edi]    ; tmp1=samples[0x0e]+samples[0x11]
 fadd  st(2),st              ; tmp2=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  68[esi]    ; tmp3=(tmp0-tmp1)*costab[17]
fld   dword ptr  24[edi]     ;
 fadd  dword ptr 100[edi]    ; tmp0=samples[0x06]+samples[0x19]
 fld   dword ptr  36[edi]    ;
 fadd  dword ptr  88[edi]    ; tmp1=samples[0x09]+samples[0x16]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   0[ebx]    ; b1[0x00]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  88[esi]    ; tmp4=(tmp0-tmp1)*costab[22]
fld   dword ptr   8[edi]     ;
 fadd  dword ptr 116[edi]    ; tmp0=samples[0x02]+samples[0x1d]
 fld   dword ptr  52[edi]    ;
 fadd  dword ptr  72[edi]    ; tmp1=samples[0x0d]+samples[0x12]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   4[ebx]    ; b1[0x01]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  72[esi]    ; tmp5=(tmp0-tmp1)*costab[18]
fld   dword ptr  20[edi]     ;
 fadd  dword ptr 104[edi]    ; tmp0=samples[0x05]+samples[0x1a]
 fld   dword ptr  40[edi]    ;
 fadd  dword ptr  84[edi]    ; tmp1=samples[0x0a]+samples[0x15]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   8[ebx]    ; b1[0x02]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  84[esi]    ; tmp6=(tmp0-tmp1)*costab[21]
fld   st(3)                  ;
 fadd  st,st(3)              ; tmp0=tmp3+tmp4
 fld   st(2)                 ;
 fadd  st,st(2)              ; tmp1=tmp5+tmp6
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  36[ecx]    ; b2[0x09]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr  40[ecx]    ; b2[0x0a]=(tmp0-tmp1)*costab[29]
fsubp st(1),st               ;
 fmul  dword ptr 104[esi]    ; tmp0=(tmp3-tmp4)*costab[26]
 fxch  st(2)                 ;
 fsubrp st(1),st             ;
 fmul  dword ptr 100[esi]    ; tmp1=(tmp5-tmp6)*costab[25]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  52[ecx]    ; b2[0x0d]=tmp0+tmp1
 fsubrp st(1),st             ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr  56[ecx]    ; b2[0x0e]=(tmp0-tmp1)*costab[29]
fld   st                     ;
 fadd  dword ptr   0[ebx]    ; tmp0=tmp2+b1[0x00]
 fld   dword ptr   4[ebx]    ;
 fadd  dword ptr   8[ebx]    ; tmp1=b1[0x01]+b1[0x02]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   4[ecx]    ; b2[0x01]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr   8[ecx]    ; b2[0x02]=(tmp0-tmp1)*costab[29]
fsub  dword ptr   0[ebx]     ;
 fmul  dword ptr 100[esi]    ; tmp0=(tmp2-b1[0x00])*costab[25]
 fld   dword ptr   4[ebx]    ;
 fsub  dword ptr   8[ebx]    ;
 fmul  dword ptr 104[esi]    ; tmp1=(b1[0x01]-b1[0x02])*costab[26]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  20[ecx]    ; b2[0x05]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr  24[ecx]    ; b2[0x06]=(tmp0-tmp1)*costab[29]
;-----------------------------------------------------------

 fld   dword ptr 120[esi]    ; cos0 = costab[30];
 fld   dword ptr    [ecx]    ;
 fld   st                    ;
 fadd  dword ptr   4[ecx]    ;
 fstp  dword ptr 1024[eax]   ; out0[0x10*16]= b2[0x00]+ b2[0x01]
 fsub  dword ptr   4[ecx]    ;
 fmul  st,st(1)              ;
 fst   dword ptr     [eax]   ; out0[0x10* 0]=
 fstp  dword ptr     [edx]   ; out1[0x10* 0]= (b2[0x00] - b2[0x01]) * cos0
 fld   dword ptr  12[ecx]    ;
 fsub  dword ptr   8[ecx]    ;
 fmul  st,st(1)              ; tmp0         = (b2[0x03] - b2[0x02]) * cos0
 fst   dword ptr  512[edx]   ; out1[0x10* 8]= tmp0
 fadd  dword ptr   8[ecx]    ;
 fadd  dword ptr  12[ecx]    ;
 fstp  dword ptr  512[eax]   ; out0[0x10* 8]= tmp0 + b2[0x02] + b2[0x03]
 fld   dword ptr  28[ecx]    ;
 fsub  dword ptr  24[ecx]    ;
 fmul  st,st(1)              ; tmp2         = (b2[0x07] - b2[0x06]) * cos0
 fst   dword ptr  768[edx]   ; out1[0x10*12]= tmp2
 fld   st                    ;
 fadd  dword ptr  24[ecx]    ;
 fadd  dword ptr  28[ecx]    ; tmp0         =  b2[0x06] + b2[0x07] + tmp2
 fld   dword ptr  16[ecx]    ;
 fadd  dword ptr  20[ecx]    ;
 fadd  st,st(1)              ;
 fstp  dword ptr  768[eax]   ; out0[0x10*12]=  b2[0x04] + b2[0x05] + tmp0
 fld   dword ptr  16[ecx]    ;
 fsub  dword ptr  20[ecx]    ;
 fmul  st,st(3)              ; tmp1         = (b2[0x04] - b2[0x05])*cos0
 fadd  st(1),st              ; tmp0+=tmp1
 faddp st(2),st              ; tmp2+=tmp1
 fstp  dword ptr  256[eax]   ; out0[0x10* 4]=  tmp0
 fstp  dword ptr  256[edx]   ; out1[0x10* 4]=  tmp2
;-----------------------------------------------------------
 fld   dword ptr  60[ecx]    ;
 fsub  dword ptr  56[ecx]    ;
 fmul  st,st(1)              ; tmp4         = (b2[0x0F]-b2[0x0E])*cos0
 fst   dword ptr  896[edx]   ; out1[0x10*14]= tmp4
 fld   st                    ;
 fadd  dword ptr  56[ecx]    ;
 fadd  dword ptr  60[ecx]    ; tmp1         =  b2[0x0E] + b2[0x0F] + tmp4
 fld   dword ptr  48[ecx]    ;
 fadd  dword ptr  52[ecx]    ;
 fadd  st,st(1)              ; tmp2         =  b2[0x0C] + b2[0x0D] + tmp1
 fld   dword ptr  32[ecx]    ;
 fadd  dword ptr  36[ecx]    ;
 fadd  st,st(1)              ;
 fstp  dword ptr  896[eax]   ; out0[0x10*14]=  b2[0x08] + b2[0x09] + tmp2
 fld   dword ptr  48[ecx]    ;
 fsub  dword ptr  52[ecx]    ;
 fmul  st,st(4)              ; tmp3         = (b2[0x0C] - b2[0x0D])*cos0
 fadd  st(2),st              ; tmp1        += tmp3
 fadd  st,st(3)              ; tmp3        += tmp4
 fld   dword ptr  32[ecx]    ;
 fsub  dword ptr  36[ecx]    ;
 fmul  st,st(5)              ; tmp0         = (b2[0x08] - b2[0x09])*cos0
 fld   st(3)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  128[eax]   ; out0[0x10* 2]= tmp1 + tmp0
 fadd  st,st(1)              ;
 fstp  dword ptr  128[edx]   ; out1[0x10* 2]= tmp0 + tmp3
 fld   dword ptr  44[ecx]    ;
 fsub  dword ptr  40[ecx]    ;
 fmul  st,st(5)              ; tmp0         = (b2[0x0B] - b2[0x0A])*cos0
 fadd  st(1),st              ; tmp3        += tmp0
 fadd  st(4),st              ; tmp4        += tmp0
 fadd  dword ptr  40[ecx]    ;
 fadd  dword ptr  44[ecx]    ; tmp0        +=  b2[0x0A] + b2[0x0B]
 fadd  st(3),st              ; tmp1        += tmp0
 faddp st(2),st              ; tmp2        += tmp0
 fstp  dword ptr  384[edx]   ; out1[0x10* 6]= tmp3
 fstp  dword ptr  640[eax]   ; out0[0x10*10]= tmp2
 fstp  dword ptr  384[eax]   ; out0[0x10* 6]= tmp1
 fstp  dword ptr  640[edx]   ; out1[0x10*10]= tmp4
 fstp  st                    ; pop cos0

;*************************************************************************

fld   dword ptr   0[edi]     ;
 fsub  dword ptr 124[edi]    ;
 fmul  dword ptr   0[esi]    ; tmp0=(samples[0x00]+samples[0x1f])*costab[0]
 fld   st                    ;
 fld   dword ptr  60[edi]    ;
 fsub  dword ptr  64[edi]    ;
 fmul  dword ptr  60[esi]    ; tmp1=(samples[0x0f]+samples[0x10])*costab[15]
 fadd  st(2),st              ; tmp2=tmp0+tmp1
 fsubp st(1),st              ; 
 fmul  dword ptr  64[esi]    ; tmp3=(tmp0-tmp1)*costab[16]
fld   dword ptr  28[edi]     ;
 fsub  dword ptr  96[edi]    ;
 fmul  dword ptr  28[esi]    ; tmp0=(samples[0x07]+samples[0x18])*costab[7]
 fld   dword ptr  32[edi]    ;
 fsub  dword ptr  92[edi]    ;
 fmul  dword ptr  32[esi]    ; tmp1=(samples[0x08]+samples[0x17])*costab[8]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   0[ebx]    ; b1[0x00]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  92[esi]    ; tmp4=(tmp0-tmp1)*costab[23]
fld   dword ptr  12[edi]     ;
 fsub  dword ptr 112[edi]    ;
 fmul  dword ptr  12[esi]    ; tmp0=(samples[0x03]+samples[0x1c])*costab[3]
 fld   dword ptr  48[edi]    ;
 fsub  dword ptr  76[edi]    ;
 fmul  dword ptr  48[esi]    ; tmp1=(samples[0x0c]+samples[0x13])*costab[12]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   4[ebx]    ; b1[0x01]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  76[esi]    ; tmp5=(tmp0-tmp1)*costab[19]
fld   dword ptr  16[edi]     ;
 fsub  dword ptr 108[edi]    ;
 fmul  dword ptr  16[esi]    ; tmp0=(samples[0x04]+samples[0x1b])*costab[4]
 fld   dword ptr  44[edi]    ;
 fsub  dword ptr  80[edi]    ;
 fmul  dword ptr  44[esi]    ; tmp1=(samples[0x0b]+samples[0x14])*costab[11]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   8[ebx]    ; b1[0x02]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  80[esi]    ; tmp6=(tmp0-tmp1)*costab[20]
fld   st(3)                  ;
 fadd  st,st(3)              ; tmp0=tmp3+tmp4
 fld   st(2)                 ;
 fadd  st,st(2)              ; tmp1=tmp5+tmp6
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  32[ecx]    ; b2[0x08]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  44[ecx]    ; b2[0x0b]=(tmp0-tmp1)*costab[28]
fsubp st(1),st               ;
 fmul  dword ptr 108[esi]    ; tmp0=(tmp3-tmp4)*costab[27]
 fxch  st(2)                 ;
 fsubrp st(1),st             ;
 fmul  dword ptr  96[esi]    ; tmp1=(tmp5-tmp6)*costab[24]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  48[ecx]    ; b2[0x0c]=tmp0+tmp1
 fsubrp st(1),st             ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  60[ecx]    ; b2[0x0f]=(tmp0-tmp1)*costab[28]
fld   st                     ;
 fadd  dword ptr   0[ebx]    ; tmp0=tmp2+b1[0x00]
 fld   dword ptr   4[ebx]    ;
 fadd  dword ptr   8[ebx]    ; tmp1=b1[0x01]+b1[0x02]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   0[ecx]    ; b2[0x00]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  12[ecx]    ; b2[0x03]=(tmp0-tmp1)*costab[28]
fsub  dword ptr   0[ebx]     ;
 fmul  dword ptr  96[esi]    ; tmp0=(tmp2-b1[0x00])*costab[24]
 fld   dword ptr   4[ebx]    ;
 fsub  dword ptr   8[ebx]    ;
 fmul  dword ptr 108[esi]    ; tmp1=(b1[0x01]-b1[0x02])*costab[27]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  16[ecx]    ; b2[0x04]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 112[esi]    ;
 fstp  dword ptr  28[ecx]    ; b2[0x07]=(tmp0-tmp1)*costab[28]
;-----------------------------------------------------------
fld   dword ptr   4[edi]     ;
 fsub  dword ptr 120[edi]    ;
 fmul  dword ptr   4[esi]    ; tmp0=(samples[0x01]+samples[0x1e])*costab[1]
 fld   st                    ;
 fld   dword ptr  56[edi]    ;
 fsub  dword ptr  68[edi]    ;
 fmul  dword ptr  56[esi]    ; tmp1=(samples[0x0e]+samples[0x11])*costab[14]
 fadd  st(2),st              ; tmp2=tmp0+tmp1
 fsubp st(1),st              ; 
 fmul  dword ptr  68[esi]    ; tmp3=(tmp0-tmp1)*costab[17]
fld   dword ptr  24[edi]     ;
 fsub  dword ptr 100[edi]    ;
 fmul  dword ptr  24[esi]    ; tmp0=(samples[0x06]+samples[0x19])*costab[6]
 fld   dword ptr  36[edi]    ;
 fsub  dword ptr  88[edi]    ;
 fmul  dword ptr  36[esi]    ; tmp1=(samples[0x09]+samples[0x16])*costab[9]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   0[ebx]    ; b1[0x00]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  88[esi]    ; tmp4=(tmp0-tmp1)*costab[22]
fld   dword ptr   8[edi]     ;
 fsub  dword ptr 116[edi]    ;
 fmul  dword ptr   8[esi]    ; tmp0=(samples[0x02]+samples[0x1d])*costab[2]
 fld   dword ptr  52[edi]    ;
 fsub  dword ptr  72[edi]    ;
 fmul  dword ptr  52[esi]    ; tmp1=(samples[0x0d]+samples[0x12])*costab[13]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   4[ebx]    ; b1[0x01]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  72[esi]    ; tmp5=(tmp0-tmp1)*costab[18]
fld   dword ptr  20[edi]     ;
 fsub  dword ptr 104[edi]    ;
 fmul  dword ptr  20[esi]    ; tmp0=(samples[0x05]+samples[0x1a])*costab[5]
 fld   dword ptr  40[edi]    ;
 fsub  dword ptr  84[edi]    ;
 fmul  dword ptr  40[esi]    ; tmp1=(samples[0x0a]+samples[0x15])*costab[10]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   8[ebx]    ; b1[0x02]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr  84[esi]    ; tmp6=(tmp0-tmp1)*costab[21]
fld   st(3)                  ;
 fadd  st,st(3)              ; tmp0=tmp3+tmp4
 fld   st(2)                 ;
 fadd  st,st(2)              ; tmp1=tmp5+tmp6
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  36[ecx]    ; b2[0x09]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr  40[ecx]    ; b2[0x0a]=(tmp0-tmp1)*costab[29]
fsubp st(1),st               ;
 fmul  dword ptr 104[esi]    ; tmp0=(tmp3-tmp4)*costab[26]
 fxch  st(2)                 ;
 fsubrp st(1),st             ;
 fmul  dword ptr 100[esi]    ; tmp1=(tmp5-tmp6)*costab[25]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  52[ecx]    ; b2[0x0d]=tmp0+tmp1
 fsubrp st(1),st             ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr  56[ecx]    ; b2[0x0e]=(tmp0-tmp1)*costab[29]
fld   st                     ;
 fadd  dword ptr   0[ebx]    ; tmp0=tmp2+b1[0x00]
 fld   dword ptr   4[ebx]    ;
 fadd  dword ptr   8[ebx]    ; tmp1=b1[0x01]+b1[0x02]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr   4[ecx]    ; b2[0x01]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr   8[ecx]    ; b2[0x02]=(tmp0-tmp1)*costab[29]
fsub  dword ptr   0[ebx]     ;
 fmul  dword ptr 100[esi]    ; tmp0=(tmp2-b1[0x00])*costab[25]
 fld   dword ptr   4[ebx]    ;
 fsub  dword ptr   8[ebx]    ;
 fmul  dword ptr 104[esi]    ; tmp1=(b1[0x01]-b1[0x02])*costab[26]
 fld   st(1)                 ;
 fadd  st,st(1)              ;
 fstp  dword ptr  20[ecx]    ; b2[0x05]=tmp0+tmp1
 fsubp st(1),st              ;
 fmul  dword ptr 116[esi]    ;
 fstp  dword ptr  24[ecx]    ; b2[0x06]=(tmp0-tmp1)*costab[29]

;-----------------------------------------------------------
 fld   dword ptr 120[esi]    ; cos0=costab[30]
 fld   dword ptr  60[ecx]    ;
 fsub  dword ptr  56[ecx]    ;
 fmul  st,st(1)              ; tmp6         =(b2[0x0F]-b2[0x0E])*cos0
 fst   dword ptr  960[edx]   ; out1[0x10*15]= tmp6
 fld   dword ptr  56[ecx]    ;
 fadd  dword ptr  60[ecx]    ;
 fadd  st,st(1)              ;
 fst   dword ptr  16[ebx]    ; b1[0x04]     = b2[0x0E] + b2[0x0F] + tmp6
 fadd  dword ptr  48[ecx]    ;
 fadd  dword ptr  52[ecx]    ; tmp3         = b2[0x0C] + b2[0x0D] + b1[0x04]
 fld   dword ptr  32[ecx]    ;
 fadd  dword ptr  36[ecx]    ;
 fadd  st,st(1)              ; tmp2         = b2[0x08] + b2[0x09] + tmp3
 fld   dword ptr    [ecx]    ;
 fadd  dword ptr   4[ecx]    ;
 fadd  st,st(1)              ;
 fstp  dword ptr  960[eax]   ; out0[0x10*15]= b2[0x00] + b2[0x01] + tmp2
 fld   dword ptr  28[ecx]    ;
 fsub  dword ptr  24[ecx]    ;
 fmul  st,st(4)              ;
 fst   dword ptr  28[ebx]    ; b1[0x07]     =(b2[0x07]-b2[0x06])*cos0
 fld   st                    ;
 fadd  st,st(4)              ;
 fstp  dword ptr  832[edx]   ; out1[0x10*13]= b1[0x07] + tmp6
 fadd  dword ptr  24[ecx]    ;
 fadd  dword ptr  28[ecx]    ;
 fst   dword ptr  24[ebx]    ; b1[0x06]     = b2[0x06] + b2[0x07] + b1[0x07]
 fadd  dword ptr  16[ecx]    ;
 fadd  dword ptr  20[ecx]    ; tmp1         = b2[0x04] + b2[0x05] + b1[0x06]
 fadd  st(1),st              ; tmp2        += tmp1
 fld   dword ptr  44[ecx]    ;
 fsub  dword ptr  40[ecx]    ;
 fmul  st,st(5)              ;
 fst   dword ptr    [ebx]    ; b1[0x00]     =(b2[0x0B]-b2[0x0A])*cos0
 fadd  dword ptr  40[ecx]    ;
 fadd  dword ptr  44[ecx]    ;
 fst   dword ptr   4[ebx]    ; b1[0x01]     = b2[0x0A] + b2[0x0B] + b1[0x00]
 faddp st(3),st              ; tmp3        += b1[0x01]
 fadd  st,st(2)              ;
 fstp  dword ptr  704[eax]   ; out0[0x10*11]= tmp1 + tmp3
 fstp  dword ptr  832[eax]   ; out0[0x10*13]= tmp2
 fld   dword ptr  12[ecx]    ;
 fsub  dword ptr   8[ecx]    ;
 fmul  st,st(3)              ;
 fst   dword ptr  12[ebx]    ; b1[0x03]     =(b2[0x03]-b2[0x02])*cos0
 fadd  dword ptr   8[ecx]    ;
 fadd  dword ptr  12[ecx]    ;
 fst   dword ptr   8[ebx]    ; b1[0x02]     = b2[0x02] + b2[0x03] + b1[0x03]
 faddp st(1),st              ;
 fstp  dword ptr  576[eax]   ; out0[0x10* 9]= tmp3 + b1[0x02]
;-----------------------------------------------------------
 fld   dword ptr  32[ecx]    ;
 fsub  dword ptr  36[ecx]    ;
 fmul  st,st(2)              ; tmp5         =(b2[0x08]-b2[0x09])*cos0
 fld   dword ptr  48[ecx]    ;
 fsub  dword ptr  52[ecx]    ;
 fmul  st,st(3)              ; tmp4         =(b2[0x0C]-b2[0x0D])*cos0
 fld   dword ptr  16[ebx]    ;
 fadd  st,st(1)              ; tmp3         = b1[0x04] + tmp4
 fld   dword ptr  16[ecx]    ;
 fsub  dword ptr  20[ecx]    ;
 fmul  st,st(5)              ;
 fst   dword ptr  20[ebx]    ; b1[0x05]     =(b2[0x04]-b2[0x05])*cos0
 fadd  dword ptr  24[ebx]    ; tmp2         = b1[0x06] + b1[0x05]
 fld   dword ptr   4[ebx]    ;
 fadd  st,st(2)              ; tmp1         = b1[0x01] + tmp3
 fld   dword ptr   8[ebx]    ;
 fadd  st,st(1)              ;
 fstp  dword ptr  448[eax]   ; out0[0x10* 7]= b1[0x02] + tmp1
 fadd  st,st(1)              ;
 fstp  dword ptr  320[eax]   ; out0[0x10* 5]= tmp1 + tmp2
 fld   st(3)                 ;
 faddp st(2),st              ; tmp3        += tmp5
 fadd  st,st(1)              ;
 fstp  dword ptr  192[eax]   ; out0[0x10* 3]= tmp2 + tmp3
 fld   dword ptr     [ecx]   ;
 fsub  dword ptr    4[ecx]   ;
 fmulp st(5),st              ; cos0        *=(b2[0x00]-b2[0x01])
 fld   st(3)                 ;
 faddp st(2),st              ; tmp4        += tmp6
 fld   st(1)                 ;
 faddp st(3),st              ; tmp5        += tmp4
 fld   st(4)                 ;
 fadd  st,st(3)              ;
 fstp  dword ptr   64[edx]   ; out1[0x10* 1]= tmp5 + cos0
 faddp st(4),st              ; cos0        += tmp3
 fadd  dword ptr     [ebx]   ; tmp1         = tmp4     + b1[0x00]
 fld   dword ptr  20[ebx]    ;
 fadd  dword ptr  28[ebx]    ; tmp2         = b1[0x05] + b1[0x07]
 fadd  st(2),st              ; tmp5        += tmp2
 fadd  st,st(1)              ;
 fstp  dword ptr  320[edx]   ; out1[0x10* 5]= tmp2     + tmp1
 fadd  dword ptr  12[ebx]    ;
 fstp  dword ptr  448[edx]   ; out1[0x10* 7]= tmp1     + b1[0x03]
 fstp  dword ptr  192[edx]   ; out1[0x10* 3]= tmp5
 fadd  dword ptr     [ebx]   ; tmp6        += b1[0x00]
 fld   dword ptr  12[ebx]    ;
 fadd  st,st(1)              ;
 fstp  dword ptr  576[edx]   ; out1[0x10* 9]= b1[0x03] + tmp6
 fadd  dword ptr  28[ebx]    ;
 fstp  dword ptr  704[edx]   ; out1[0x10*11]= tmp6     + b1[0x07]
 fstp  dword ptr   64[eax]   ; out0[0x10* 1]= cos0

 ret

;--------------------------------------------------------------------------
; eax=scale
mpxdec_synth_init_:
 push      ecx
 push      edi
 push      ebp
 mov       ebp,esp
 sub       esp,00000004H

;initialize/calculate window table

 shr       eax,1                         ; scale>>=1
 mov       dword ptr -4H[ebp],eax        ;
 fild      dword ptr -4H[ebp]            ;
 mov       dword ptr -4H[ebp],65536      ;
 fidiv     dword ptr -4H[ebp]            ; scale/=65536
 mov       ecx,offset FLAT:_intwinbase   ; intwinp=&intwinbase[0];
 mov       edi,offset FLAT:_decwin       ; table=decwin
 mov       eax,edi                       ;
 add       eax,64                        ;
 mov       dword ptr FLAT:_decwin16,eax  ; decwin16=decwin+16
 mov       ah,17                         ; i=17
L$31:                                    ; do{
  mov       al,16                        ;  j=16
L$32:                                    ;  do{
   fild      dword ptr [ecx]             ;   float w=intwinp[0]*scale
   fmul      st,st(1)                    ;
   add       ecx,4                       ;   intwinp++
   fst       dword ptr [edi]             ;   table[0]=table[16]=w
   fstp      dword ptr 64[edi]           ;
   add       edi,4                       ;   table++
   dec       al                          ;   j--
  jne       L$32                         ;  }while(j)
  add       edi,64                       ;  table+=16
  dec       ah                           ;  i--;
 jne       L$31                          ; }while(i)
 fstp      st                            ;

; calculate cos table for dct64

 mov  edi,offset FLAT:_dct64costab+78H   ; costabp=&dct64costab[30]
 mov  ecx,4                              ; i=4
 fldpi                                   ;
 fld1                                    ;
 mov       dword ptr -4H[ebp],2          ;
 fild      dword ptr -4H[ebp]            ;
L$33:                                    ; do{
  mov       eax,64                       ;
  shr       eax,cl                       ;  divv=64>>i
  mov       dword ptr -4H[ebp],eax       ;
  fild      dword ptr -4H[ebp]           ;
  mov       eax,16                       ;
  shr       eax,cl                       ;
  dec       eax                          ;  k=(16>>i)-1
L$34:                                    ;
   mov       dword ptr -4H[ebp],eax      ;
   fild      dword ptr -4H[ebp]          ;
   fmul      st,st(2)                    ;
   fadd      st,st(3)                    ;
   fmul      st,st(4)                    ;
   fdiv      st,st(1)                    ;
   fcos                                  ;
   fmul      st,st(2)                    ;
   fdivr     st,st(3)                    ;
   fstp      dword ptr [edi]             ;   *costab=1.0 / (2.0*cos(M_PI * ((float)k*2.0+1.0) / (float)divv));
   sub       edi,4                       ;   costabp--
   dec       eax                         ;   j--
  jns       L$34                         ;  }while(j>=0)
  fstp      st                           ;
  dec       ecx                          ;  i--
 jns       L$33                          ; }while(i>=0)
 fstp      st                            ;
 fstp      st                            ;
 fstp      st                            ;

 mov       esp,ebp
 pop       ebp
 pop       edi
 pop       ecx
 ret

;-------------------------------------------------------------------------

_TEXT		ENDS
CONST		SEGMENT	DWORD PUBLIC USE32 'DATA'
CONST		ENDS
CONST2		SEGMENT	DWORD PUBLIC USE32 'DATA'
CONST2		ENDS
_DATA		SEGMENT	DWORD PUBLIC USE32 'DATA'
_intwinbase:
    DB	00H, 00H, 00H, 00H, 0e3H, 0ffH, 0ffH, 0ffH
    DB	0d5H, 00H, 00H, 00H, 35H, 0feH, 0ffH, 0ffH
    DB	0f5H, 07H, 00H, 00H, 0dfH, 0ebH, 0ffH, 0ffH
    DB	0aeH, 19H, 00H, 00H, 8fH, 6dH, 0ffH, 0ffH
    DB	1eH, 25H, 01H, 00H, 71H, 92H, 00H, 00H
    DB	0aeH, 19H, 00H, 00H, 21H, 14H, 00H, 00H
    DB	0f5H, 07H, 00H, 00H, 0cbH, 01H, 00H, 00H
    DB	0d5H, 00H, 00H, 00H, 1dH, 00H, 00H, 00H
    DB	0ffH, 0ffH, 0ffH, 0ffH, 0e1H, 0ffH, 0ffH, 0ffH
    DB	0daH, 00H, 00H, 00H, 0f9H, 0fdH, 0ffH, 0ffH
    DB	0d0H, 07H, 00H, 00H, 73H, 0eaH, 0ffH, 0ffH
    DB	47H, 17H, 00H, 00H, 58H, 66H, 0ffH, 0ffH
    DB	0f0H, 24H, 01H, 00H, 38H, 8bH, 00H, 00H
    DB	0deH, 1bH, 00H, 00H, 0b4H, 12H, 00H, 00H
    DB	0fH, 08H, 00H, 00H, 91H, 01H, 00H, 00H
    DB	0d0H, 00H, 00H, 00H, 1aH, 00H, 00H, 00H
    DB	0ffH, 0ffH, 0ffH, 0ffH, 0ddH, 0ffH, 0ffH, 0ffH
    DB	0deH, 00H, 00H, 00H, 0bbH, 0fdH, 0ffH, 0ffH
    DB	0a0H, 07H, 00H, 00H, 09H, 0e9H, 0ffH, 0ffH
    DB	0a8H, 14H, 00H, 00H, 28H, 5fH, 0ffH, 0ffH
    DB	68H, 24H, 01H, 00H, 0ffH, 83H, 00H, 00H
    DB	0d8H, 1dH, 00H, 00H, 49H, 11H, 00H, 00H
    DB	20H, 08H, 00H, 00H, 5bH, 01H, 00H, 00H
    DB	0caH, 00H, 00H, 00H, 18H, 00H, 00H, 00H
    DB	0ffH, 0ffH, 0ffH, 0ffH, 0daH, 0ffH, 0ffH, 0ffH
    DB	0e1H, 00H, 00H, 00H, 7bH, 0fdH, 0ffH, 0ffH
    DB	65H, 07H, 00H, 00H, 0a3H, 0e7H, 0ffH, 0ffH
    DB	0d1H, 11H, 00H, 00H, 02H, 58H, 0ffH, 0ffH
    DB	86H, 23H, 01H, 00H, 0cbH, 7cH, 00H, 00H
    DB	9cH, 1fH, 00H, 00H, 0dfH, 0fH, 00H, 00H
    DB	27H, 08H, 00H, 00H, 26H, 01H, 00H, 00H
    DB	0c4H, 00H, 00H, 00H, 15H, 00H, 00H, 00H
    DB	0ffH, 0ffH, 0ffH, 0ffH, 0d7H, 0ffH, 0ffH, 0ffH
    DB	0e3H, 00H, 00H, 00H, 39H, 0fdH, 0ffH, 0ffH
    DB	1eH, 07H, 00H, 00H, 43H, 0e6H, 0ffH, 0ffH
    DB	0c0H, 0eH, 00H, 00H, 0ebH, 50H, 0ffH, 0ffH
    DB	49H, 22H, 01H, 00H, 0a0H, 75H, 00H, 00H
    DB	2cH, 21H, 00H, 00H, 79H, 0eH, 00H, 00H
    DB	25H, 08H, 00H, 00H, 0f4H, 00H, 00H, 00H
    DB	0beH, 00H, 00H, 00H, 13H, 00H, 00H, 00H
    DB	0ffH, 0ffH, 0ffH, 0ffH, 0d3H, 0ffH, 0ffH, 0ffH
    DB	0e4H, 00H, 00H, 00H, 0f5H, 0fcH, 0ffH, 0ffH
    DB	0cbH, 06H, 00H, 00H, 0e9H, 0e4H, 0ffH, 0ffH
    DB	77H, 0bH, 00H, 00H, 0e7H, 49H, 0ffH, 0ffH
    DB	0b4H, 20H, 01H, 00H, 81H, 6eH, 00H, 00H
    DB	88H, 22H, 00H, 00H, 17H, 0dH, 00H, 00H
    DB	1bH, 08H, 00H, 00H, 0c5H, 00H, 00H, 00H
    DB	0b7H, 00H, 00H, 00H, 11H, 00H, 00H, 00H
    DB	0ffH, 0ffH, 0ffH, 0ffH, 0cfH, 0ffH, 0ffH, 0ffH
    DB	0e4H, 00H, 00H, 00H, 0b0H, 0fcH, 0ffH, 0ffH
    DB	6cH, 06H, 00H, 00H, 99H, 0e3H, 0ffH, 0ffH
    DB	0f5H, 07H, 00H, 00H, 0faH, 42H, 0ffH, 0ffH
    DB	0c7H, 1eH, 01H, 00H, 72H, 67H, 00H, 00H
    DB	0b3H, 23H, 00H, 00H, 0bcH, 0bH, 00H, 00H
    DB	09H, 08H, 00H, 00H, 99H, 00H, 00H, 00H
    DB	0b0H, 00H, 00H, 00H, 10H, 00H, 00H, 00H
    DB	0feH, 0ffH, 0ffH, 0ffH, 0cbH, 0ffH, 0ffH, 0ffH
    DB	0e3H, 00H, 00H, 00H, 69H, 0fcH, 0ffH, 0ffH
    DB	0ffH, 05H, 00H, 00H, 53H, 0e2H, 0ffH, 0ffH
    DB	3aH, 04H, 00H, 00H, 27H, 3cH, 0ffH, 0ffH
    DB	83H, 1cH, 01H, 00H, 76H, 60H, 00H, 00H
    DB	0adH, 24H, 00H, 00H, 67H, 0aH, 00H, 00H
    DB	0f0H, 07H, 00H, 00H, 6fH, 00H, 00H, 00H
    DB	0a9H, 00H, 00H, 00H, 0eH, 00H, 00H, 00H
    DB	0feH, 0ffH, 0ffH, 0ffH, 0c6H, 0ffH, 0ffH, 0ffH
    DB	0e0H, 00H, 00H, 00H, 21H, 0fcH, 0ffH, 0ffH
    DB	86H, 05H, 00H, 00H, 1aH, 0e1H, 0ffH, 0ffH
    DB	46H, 00H, 00H, 00H, 73H, 35H, 0ffH, 0ffH
    DB	0e9H, 19H, 01H, 00H, 91H, 59H, 00H, 00H
    DB	78H, 25H, 00H, 00H, 1aH, 09H, 00H, 00H
    DB	0d1H, 07H, 00H, 00H, 48H, 00H, 00H, 00H
    DB	0a1H, 00H, 00H, 00H, 0dH, 00H, 00H, 00H
    DB	0feH, 0ffH, 0ffH, 0ffH, 0c1H, 0ffH, 0ffH, 0ffH
    DB	0ddH, 00H, 00H, 00H, 0d8H, 0fbH, 0ffH, 0ffH
    DB	00H, 05H, 00H, 00H, 0efH, 0dfH, 0ffH, 0ffH
    DB	1aH, 0fcH, 0ffH, 0ffH, 0e2H, 2eH, 0ffH, 0ffH
    DB	0fcH, 16H, 01H, 00H, 0c5H, 52H, 00H, 00H
    DB	16H, 26H, 00H, 00H, 0d6H, 07H, 00H, 00H
    DB	0aaH, 07H, 00H, 00H, 24H, 00H, 00H, 00H
    DB	9aH, 00H, 00H, 00H, 0bH, 00H, 00H, 00H
    DB	0feH, 0ffH, 0ffH, 0ffH, 0bcH, 0ffH, 0ffH, 0ffH
    DB	0d7H, 00H, 00H, 00H, 8fH, 0fbH, 0ffH, 0ffH
    DB	6bH, 04H, 00H, 00H, 0d5H, 0deH, 0ffH, 0ffH
    DB	0b6H, 0f7H, 0ffH, 0ffH, 76H, 28H, 0ffH, 0ffH
    DB	0beH, 13H, 01H, 00H, 16H, 4cH, 00H, 00H
    DB	87H, 26H, 00H, 00H, 9cH, 06H, 00H, 00H
    DB	7fH, 07H, 00H, 00H, 02H, 00H, 00H, 00H
    DB	93H, 00H, 00H, 00H, 0aH, 00H, 00H, 00H
    DB	0fdH, 0ffH, 0ffH, 0ffH, 0b7H, 0ffH, 0ffH, 0ffH
    DB	0d0H, 00H, 00H, 00H, 46H, 0fbH, 0ffH, 0ffH
    DB	0caH, 03H, 00H, 00H, 0cdH, 0ddH, 0ffH, 0ffH
    DB	1cH, 0f3H, 0ffH, 0ffH, 36H, 22H, 0ffH, 0ffH
    DB	2fH, 10H, 01H, 00H, 87H, 45H, 00H, 00H
    DB	0cfH, 26H, 00H, 00H, 6cH, 05H, 00H, 00H
    DB	4eH, 07H, 00H, 00H, 0e3H, 0ffH, 0ffH, 0ffH
    DB	8bH, 00H, 00H, 00H, 09H, 00H, 00H, 00H
    DB	0fdH, 0ffH, 0ffH, 0ffH, 0b1H, 0ffH, 0ffH, 0ffH
    DB	0c8H, 00H, 00H, 00H, 0fdH, 0faH, 0ffH, 0ffH
    DB	1aH, 03H, 00H, 00H, 0daH, 0dcH, 0ffH, 0ffH
    DB	4bH, 0eeH, 0ffH, 0ffH, 23H, 1cH, 0ffH, 0ffH
    DB	54H, 0cH, 01H, 00H, 1bH, 3fH, 00H, 00H
    DB	0eeH, 26H, 00H, 00H, 47H, 04H, 00H, 00H
    DB	19H, 07H, 00H, 00H, 0c7H, 0ffH, 0ffH, 0ffH
    DB	84H, 00H, 00H, 00H, 08H, 00H, 00H, 00H
    DB	0fcH, 0ffH, 0ffH, 0ffH, 0abH, 0ffH, 0ffH, 0ffH
    DB	0bdH, 00H, 00H, 00H, 0b4H, 0faH, 0ffH, 0ffH
    DB	5dH, 02H, 00H, 00H, 0fdH, 0dbH, 0ffH, 0ffH
    DB	46H, 0e9H, 0ffH, 0ffH, 42H, 16H, 0ffH, 0ffH
    DB	2dH, 08H, 01H, 00H, 0d4H, 38H, 00H, 00H
    DB	0e7H, 26H, 00H, 00H, 2eH, 03H, 00H, 00H
    DB	0dfH, 06H, 00H, 00H, 0adH, 0ffH, 0ffH, 0ffH
    DB	7dH, 00H, 00H, 00H, 07H, 00H, 00H, 00H
    DB	0fcH, 0ffH, 0ffH, 0ffH, 0a5H, 0ffH, 0ffH, 0ffH
    DB	0b1H, 00H, 00H, 00H, 6cH, 0faH, 0ffH, 0ffH
    DB	92H, 01H, 00H, 00H, 38H, 0dbH, 0ffH, 0ffH
    DB	0eH, 0e4H, 0ffH, 0ffH, 97H, 10H, 0ffH, 0ffH
    DB	0beH, 03H, 01H, 00H, 0b4H, 32H, 00H, 00H
    DB	0bcH, 26H, 00H, 00H, 21H, 02H, 00H, 00H
    DB	0a2H, 06H, 00H, 00H, 96H, 0ffH, 0ffH, 0ffH
    DB	75H, 00H, 00H, 00H, 07H, 00H, 00H, 00H
    DB	0fbH, 0ffH, 0ffH, 0ffH, 9fH, 0ffH, 0ffH, 0ffH
    DB	0a3H, 00H, 00H, 00H, 26H, 0faH, 0ffH, 0ffH
    DB	0b9H, 00H, 00H, 00H, 8fH, 0daH, 0ffH, 0ffH
    DB	0a4H, 0deH, 0ffH, 0ffH, 24H, 0bH, 0ffH, 0ffH
    DB	0aH, 0ffH, 00H, 00H, 0bfH, 2cH, 00H, 00H
    DB	6eH, 26H, 00H, 00H, 20H, 01H, 00H, 00H
    DB	62H, 06H, 00H, 00H, 81H, 0ffH, 0ffH, 0ffH
    DB	6fH, 00H, 00H, 00H, 06H, 00H, 00H, 00H
    DB	0fbH, 0ffH, 0ffH, 0ffH, 98H, 0ffH, 0ffH, 0ffH
    DB	92H, 00H, 00H, 00H, 0e1H, 0f9H, 0ffH, 0ffH
    DB	0d3H, 0ffH, 0ffH, 0ffH, 01H, 0daH, 0ffH, 0ffH
    DB	09H, 0d9H, 0ffH, 0ffH, 0edH, 05H, 0ffH, 0ffH
    DB	13H, 0faH, 00H, 00H, 0f7H, 26H, 00H, 00H
    DB	0ffH, 25H, 00H, 00H, 2dH, 00H, 00H, 00H
    DB	1fH, 06H, 00H, 00H, 6eH, 0ffH, 0ffH, 0ffH
    DB	68H, 00H, 00H, 00H, 05H, 00H, 00H, 00H

_DATA		ENDS
_BSS		SEGMENT	DWORD PUBLIC USE32 'BSS'

    ORG 0
_dct64costab LABEL	BYTE ; 32*4
    ORG 128
_decwin      LABEL	BYTE ; (512+32)*4
    ORG 2304
_decwin16    LABEL	BYTE ; 4
    ORG 2308

_BSS		ENDS

		END
