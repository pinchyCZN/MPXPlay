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
;function: Audio MPEG Layer III sub-routines (antialias,mdct12/36)
;based on the MPG123 MPEG decoder source

.387
.386p
.model flat
		PUBLIC	mpxdec_l3mdct_init_
		PUBLIC	mpxdec_l3mdct_hybrid_

                PUBLIC	mpxdec_l3mdct_init
		PUBLIC	mpxdec_l3mdct_hybrid

DGROUP		GROUP	CONST,CONST2,_DATA,_BSS
_TEXT		SEGMENT	BYTE PUBLIC USE32 'CODE'
		ASSUME CS:_TEXT, DS:DGROUP, SS:DGROUP

;-------------------------------------------------------------------------
mpxdec_l3mdct_init:
mpxdec_l3mdct_init_:
push  eax
push  ebx
push  ecx
push  edx
push  edi
push  ebp
mov   ebp,esp
sub   esp,4

mov   ebx,4                         ;
xor   edx,edx                       ;
fld1                                ;
L$1:                                ; for(i=0;i<9;i++){
 fld   qword ptr FLAT:L$24[edx*2]    ; Ci[i]->st0
 fld   st                            ;
 fmul  st,st(1)                      ;
 fadd  st,st(2)                      ;
 fsqrt                               ; sq=sqrt(1.0+Ci[i]*Ci[i]);
 fdivr st,st(2)                      ;
 fst   dword ptr FLAT:_aa_cs[edx]    ; aa_cs[i] = 1.0/sq;
 fmul                                ;
 fstp  dword ptr FLAT:_aa_cs+32[edx] ; aa_ca[i] = Ci[i]/sq;
 add   edx,ebx                       ;
 cmp   edx,32                        ;
jne   L$1                           ; }
fstp  st                            ;

fld1
mov   dword ptr -4[ebp],2           ;
fidiv dword ptr -4[ebp]             ; 0.5=1/2
fldpi                               ;
mov   dword ptr -4[ebp],72          ;
fidiv dword ptr -4[ebp]             ; PIp12=PI/72
mov   edi,offset FLAT:_win          ; winp=&win[0][0][0]
mov   edx,1                         ;
mov   ecx,18                        ;
L$2:                                ; for(i=1;i<37;i+=2){
 mov   eax,edx                       ;
 add   eax,ecx                       ;
 mov   dword ptr -4H[ebp],eax        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(1)                      ;
 fcos                                ; a=cos((double)(i+18)*PIp72)
 mov   dword ptr -4H[ebp],edx        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fsin                                ; b=sin((double)(i)*PIp72)
 fdivrp st(1),st                     ; a=a/b
 fmul  st,st(2)                      ; a*=0.5
 fst   dword ptr [edi]               ; winp[0] =a
 fstp  dword ptr 90H[edi]            ; winp[36]=a
 add   edi,ebx                       ; winp++
 add   edx,2                         ;
 cmp   edx,37                        ;
jb    L$2                           ; }

L$3:                                ; for(;i<73;i+=2){
 mov   eax,edx                       ;
 add   eax,ecx                       ;
 mov   dword ptr -4H[ebp],eax        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(1)                      ;
 fcos                                ; a=cos((double)(i+18)*PIp72)
 fld   st                            ;
 fdivr st,st(3)                      ;
 fstp  dword ptr 90H[edi]            ; winp[36]=0.5/a
 mov   dword ptr -4H[ebp],edx        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fsin                                ; b=sin(double)(i)*PIp72)
 fdivrp st(1),st                     ; a=a/b
 fmul  st,st(2)                      ; a*=0.5
 fst   dword ptr [edi]               ; winp[0]   =a
 fstp  dword ptr 1b0H[edi]           ; winp[3*36]=a
 add   edi,ebx                       ; winp++
 add   edx,2                         ;
 cmp   edx,73                        ;
jb    L$3                           ; }

fldpi                               ;
mov   dword ptr -4[ebp],24          ;
fidiv dword ptr -4[ebp]             ; PIp12=PI/24
mov   edx,1                         ;
L$4:                                ; for(i=1;i<13;i+=2){
 mov   eax,edx                       ;
 add   eax,42                        ;
 mov   dword ptr -4H[ebp],eax        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fcos                                ; a=cos((double)(i+42)*PIp72)
 fdivr st,st(3)                      ;
 fstp  dword ptr 336[edi]            ; winp[2*36+12]=0.5/a
 mov   eax,edx                       ;
 add   eax,66                        ;
 mov   dword ptr -4H[ebp],eax        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fcos                                ; a=cos((double)(i+66)*PIp72
 mov   eax,edx                       ;
 add   eax,12                        ;
 mov   dword ptr -4H[ebp],eax        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fsin                                ; b=sin((double)(i+12)*PIp24
 fdivrp st(1),st                     ; a=a/b
 fmul  st,st(3)                      ; a*=0.5
 fstp  dword ptr 96[edi]             ; winp[24]=a
 mov   eax,edx                       ;
 add   eax,30                        ;
 mov   dword ptr -4H[ebp],eax        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fcos                                ; a=cos((double)(i+30)*PIp72
 mov   dword ptr -4H[ebp],edx        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fsin                                ; b=sin((double)(i)*PIp24)
 fdivrp st(1),st                     ; a=a/b
 fmul  st,st(3)                      ; a*=0.5
 fstp  dword ptr 312[edi]            ; winp[2*36+6]=a
 fldz                                ;
 fst   dword ptr 120[edi]            ; winp[  30]=0
 fstp  dword ptr 288[edi]            ; winp[2*36]=0
 add   edi,ebx                       ; winp++
 add   edx,2                         ;
 cmp   edx,13                        ;
jb    L$4                           ; }

fxch  st(1)                         ;
fstp  st                            ; push PIp72

;lea   edi,FLAT:_win+288             ;
mov   edi,offset FLAT:_win          ; winp=&win[0][2][0]
add   edi,288                       ;
mov   edx,1                         ;
mov   ecx,6                         ;
L$5:                                ; for(i=1;i<25;i+=2){
 mov   eax,edx                       ;
 add   eax,ecx                       ;
 mov   dword ptr -4H[ebp],eax        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(1)                      ;
 fcos                                ; a=cos((double)(i+6)*PIp24)
 mov   dword ptr -4H[ebp],edx        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(2)                      ;
 fsin                                ; a=sin((double)(i)*PIp24)
 fdivrp st(1),st                     ; a=a/b
 fmul  st,st(2)                      ; a*=0.5
 fstp  dword ptr [edi]               ; winp[0]=a
 add   edi,ebx                       ; winp++
 add   edx,2                         ;
 cmp   edx,25                        ;
jb    L$5                           ; }
fstp  st                            ; push PIp24

mov   edi,offset FLAT:_win          ; winp=&win[0][0][0]
mov   edx,72                        ;
L$6:                                ; for(i=72;i;i--){
 fld   dword ptr [edi]               ;
 fstp  dword ptr 576[edi]            ; winp[144]= winp[0]
 add   edi,ebx                       ; winp++
 fld   dword ptr [edi]               ;
 fchs                                ;
 fstp  dword ptr 576[edi]            ; winp[144]=-winp[0]
 add   edi,ebx                       ; winp++
 dec   edx                           ;
jne   L$6                           ; }

fldpi                               ;
mov   dword ptr -4[ebp],18          ;
fidiv dword ptr -4[ebp]             ; PIp12=PI/18
xor   edx,edx                       ;
mov   edi,offset FLAT:_COS9         ; cosp=&COS9[0]
L$7:                                ; for(i=0;i<9;i++){
 mov   dword ptr -4H[ebp],edx        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(1)                      ;
 fcos                                ;
 fstp  dword ptr [edi]               ; cosp[0]=cos((double)(i)*PIp18)
 add   edi,ebx                       ; cosp++
 inc   edx                           ;
 cmp   edx,9                         ;
jb    L$7                           ; }

fstp  st                            ; push PIp18
fldpi                               ;
mov   dword ptr -4[ebp],36          ;
fidiv dword ptr -4[ebp]             ; PIp12=PI/36
mov   edx,1                         ;
L$8:                                ; for(i=1;i<19;i+=2){
 mov   dword ptr -4H[ebp],edx        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(1)                      ;
 fcos                                ;
 fdivr st,st(2)                      ;
 fstp  dword ptr [edi]               ; cosp[0]=0.5/cos((double)(i)*PIp36)
 add   edi,ebx                       ; cosp++
 add   edx,2                         ;
 cmp   edx,19                        ;
jb    L$8                           ; }

fstp  st                            ; push PIp36
fldpi                               ;
mov   dword ptr -4[ebp],12          ;
fidiv dword ptr -4[ebp]             ; PIp12=PI/12
mov   edi,offset FLAT:_tfcos12      ; cosp=&tfcos[0]
mov   edx,1                         ;
L$9:                                ; for(i=1;i<7;i+=2){
 mov   dword ptr -4H[ebp],edx        ;
 fild  dword ptr -4H[ebp]            ;
 fmul  st,st(1)                      ;
 fcos                                ;
 fdivr st,st(2)                      ; cosp[0]=0.5/cos((double)(i)*PIp12)
 fstp  dword ptr [edi]               ; cosp++
 add   edi,ebx                       ;
 add   edx,2                         ;
 cmp   edx,7                         ;
jb    L$9                           ; }

fstp  st                            ; push PIp12
fstp  st                            ; push 0.5

fldpi                               ;
mov   dword ptr -4[ebp],6           ;
fidiv dword ptr -4[ebp]             ;
fcos                                ;
fstp  dword ptr [edi]               ; cosp[0]=cos(PI/6)
add   edi,ebx                       ; cosp++
fldpi                               ;
mov   dword ptr -4[ebp],3           ;
fidiv dword ptr -4[ebp]             ;
fcos                                ;
fstp  dword ptr [edi]               ; cosp[0]=cos(PI/3)

mov  esp,ebp
pop  ebp
pop  edi
pop  edx
pop  ecx
pop  ebx
pop  eax
ret

;--------------------------------------------------------------------------
III_antialias_32bit_:
push  eax
push  edx
push  ebx
push  edi

fld   dword ptr 28[eax]             ; in7=in[ 7]
fst   dword ptr -10H[ebp]           ; in7_old=in7
fadd  dword ptr 24[eax]             ; in76_old=in7+in[ 6]
fld   st			    ; in76=in76_old

fld   dword ptr 20[eax]             ; <- in[ 5]
fld   st                            ;
fadd  dword ptr 24[eax]             ;
fstp  dword ptr 24[eax]             ; in[ 6]+=in[ 5]

fadd  dword ptr 16[eax]             ; in54=in[ 5]+in[ 4]
fadd  st(1),st                      ; in76+=in54

fld   dword ptr 12[eax]             ;
fld   st                            ;
fadd  dword ptr 16[eax]             ;
fstp  dword ptr 16[eax]             ; in[ 4]+=in[ 3]

fadd  dword ptr 8[eax]              ; in32=in[ 3]+in[ 2]
fadd  st(1),st                      ; in54+=in32

fld   dword ptr 4[eax]              ;
fld   st                            ;
fadd  dword ptr 8[eax]              ;
mov   edx,esi                       ; sb=esi
fstp  dword ptr 8[eax]              ; in[ 2]+=in[ 1]

fadd  dword ptr  [eax]              ;
mov   edi,4                         ;
fst   dword ptr 4[eax]              ; in[ 1]+=in[ 0]

fadd                                ; in32+=in[ 1]
fstp  dword ptr 12[eax]             ; in[ 3]=in32
fstp  dword ptr 20[eax]             ; in[ 5]=in54
fstp  dword ptr 28[eax]             ; in[ 7]=in76
				    ;
dec edx                             ; sb_a--
jz noanti                           ; if(!sb_a) goto noanti
add  eax,72                         ;
antisb:                             ;
 mov   esi,eax                      ;
 mov   ebx,offset FLAT:_aa_cs       ;
				    ;
 fld   dword ptr [eax]              ;
 fld   st                           ;
 fmul  dword ptr [ebx]              ;
 sub   esi,edi                      ;
 fld   dword ptr [esi]              ;
 fmul  dword ptr 32[ebx]            ;
 fadd                               ;
 fst   dword ptr [eax]              ; in7=in[ 0]=(in[0]*aa_cs[0])+(in[17]*aa_cs[8])
				    ;
 fxch st(1)                         ;
 fmul  dword ptr 32[ebx]            ;
 add   eax,edi                      ;
 fld   dword ptr [esi]              ;
 fmul  dword ptr [ebx]              ;
 add   ebx,edi                      ;
 fsubr                              ; in10=in[17]=(in[17]*aa_cs[0])+(in[0]*aa_cs[8])

 sub   esi,edi                      ;
 fld   dword ptr [esi]              ;
 fmul  dword ptr [ebx]              ;
 fld   dword ptr [eax]
 fmul  dword ptr 32[ebx]            ;
 fsub                               ; in[16]
 fxch  st(1)                        ;
 fadd  st,st(1)                     ; in1110=(in[17]+=in[16])
				    ;
 fld   dword ptr [eax]              ;
 fmul  dword ptr [ebx]              ;
 fld   dword ptr [esi]              ;
 fmul  dword ptr 32[ebx]            ;
 add   ebx,edi                      ;
 fadd                               ; in[ 1]
 fxch  st(3)                        ;
 fadd  st,st(3)                     ;
 fst   dword ptr [eax]              ; in76=(in[ 1]+=in[0])
 add   eax,edi                      ;
 fxch  st(2)                        ; in76<->in10
				    ;
 mov   dh,3                         ;
 antiss:                            ;
  fld   dword ptr [eax]             ;
  fld   st                          ;
  fmul  dword ptr [ebx]             ;
  sub   esi,edi                     ;
  fld   dword ptr [esi]             ;
  fmul  dword ptr 32[ebx]           ;
  fadd                              ; in[2],in[4],in[6]
  fxch  st(5)                       ;
  fadd  st,st(5)                    ;
  fstp  dword ptr [eax]             ; in[2]+=in[1],in[4]+=in[3],in[6]+=in[5]

  fmul  dword ptr 32[ebx]           ;
  add   eax,edi                     ;
  fld   dword ptr [esi]             ;
  fmul  dword ptr [ebx]             ;
  add   ebx,edi                     ;
  fsubr                             ; in[15],in[13],in[11]
  fxch  st(1)                       ;
  fadd  st,st(1)                    ;
  fstp  dword ptr 4[esi]            ; in[16]+=in[15],in[14],in[12]

  fld   dword ptr [eax]             ;
  fld   st                          ;
  fmul  dword ptr [ebx]             ;
  sub   esi,edi                     ;
  fld   dword ptr [esi]             ;
  fmul  dword ptr 32[ebx]           ;
  fadd                              ; in[3],in[5],in[7]
  fxch  st(5)                       ;
  fadd  st,st(5)                    ; in[3]+=in[2],in[5]+=in[4],in[7]+=in[6]
  fxch  st(4)                       ;
  fadd  st,st(4)                    ; in[3]+=in[1],in[5]+=in[3],in[7]+=in[5]
  fstp  dword ptr [eax]             ;

  fmul  dword ptr 32[ebx]           ;
  add   eax,edi                     ;
  fld   dword ptr [esi]             ;
  fmul  dword ptr [ebx]             ;
  add   ebx,edi                     ;
  fsubr                             ; in[14],in[12],in[10]
  fxch  st(1)                       ;
  fadd  st,st(1)                    ;
  dec   dh                          ;
  fst   dword ptr 4[esi]            ; in[15]+=in[14],in[13]+=in[12],in[11]+=in[10]
  fxch  st(2)                       ; in[15] <-> in[17]
  fadd  st,st(2)                    ;
  fstp  dword ptr 12[esi]           ; in[17]+=in[15],in[15]+=in[13],in[13]+=in[11]
 jnz antiss                         ;

;st0=in10
;st1=in1110
;st2=in76
;st3=in7
;st4=in76_old

 fadd  dword ptr -4[esi]            ; in10+=in[9]
 fstp  dword ptr   [esi]            ; in[10]=in10

 fld   dword ptr -4[esi]            ; in[9]
 fadd  dword ptr -8[esi]            ; in98=in[9]+in[8]
 add  eax,40
 fadd  st(1),st                     ; in1110+=in98

;st0=in98
;st1=in1110
;st2=in76
;st3=in7
;st4=in76_old

 faddp st(4),st                     ; in76_old+=in98
 fstp  dword ptr 4[esi]             ; in[11]=in1110
 fxch  st(2)                        ; in76<->in76_old
 fstp  dword ptr -4[esi]            ; in[9]=in76_old

 fld   dword ptr -8[esi]            ;
 fadd  dword ptr -10H[ebp]          ;
 dec  dl
 fstp  dword ptr -8[esi]            ; in[8]+=in7_old
 fstp  dword ptr -10H[ebp]          ; in7_old=in7
				    ;
jnz antisb                          ;

sub  eax,72                         ;

noanti:
fld   dword ptr 64[eax]             ;
fld   st                            ;
fadd  dword ptr 60[eax]             ;
fstp  dword ptr 64[eax]             ; in[16]+=in[15]
fadd  dword ptr 68[eax]             ; in1716=in[17]+in[16]

fld   dword ptr 60[eax]             ;
fadd  dword ptr 56[eax]             ; in1514=in[15]+in[14]
fadd  st(1),st                      ; in1716+=in1514

fld   dword ptr 52[eax]             ;
fld   st                            ;
fadd  dword ptr 56[eax]             ;
fstp  dword ptr 56[eax]             ; in[14]+=in[13]
fadd  dword ptr 48[eax]             ; in1312=in[13]+in[12]
fadd  st(1),st                      ; in1514+=in1312

fld   dword ptr 44[eax]             ;
fld   st                            ;
fadd  dword ptr 48[eax]             ;
fstp  dword ptr 48[eax]             ; in[12]+=in[11]
fadd  dword ptr 40[eax]             ; in1110=in[11]+in[10]
fadd  st(1),st                      ; in1312+=in1110

fld   dword ptr 36[eax]             ;
fld   st                            ;
fadd  dword ptr 40[eax]             ;
fstp  dword ptr 40[eax]             ; in[10]+=in[ 9]
fadd  dword ptr 32[eax]             ; in98=in[ 9]+in[ 8]
fadd  st(1),st                      ; in1110+=in98

faddp st(5),st                      ; in76_old+=in98

fstp  dword ptr 44[eax]             ; in[11]=in1110
fstp  dword ptr 52[eax]             ; in[13]=in1312
fstp  dword ptr 60[eax]             ; in[15]=in1514
fstp  dword ptr 68[eax]             ; in[17]=in1716

fstp  dword ptr 36[eax]             ; in[ 9]=in76_old

fld   dword ptr 32[eax]             ;
fadd  dword ptr -10H[ebp]           ;
fstp  dword ptr 32[eax]             ; in[ 8]+=in7_old

pop  edi
pop  ebx
pop  edx
pop  eax
ret
;---------------------------------------------------------------------------
;struct gr_info_s {
; int scfsi;                         // 0x00
; unsigned int part2_3_length;       // 0x04
; unsigned int scalefac_compress;    // 0x08
; unsigned int block_type;           // 0x0c
; unsigned int mixed_block_flag;     // 0x10
; unsigned int table_select[3];      // 0x14
; unsigned int subblock_gain[3];     // 0x20
; unsigned int maxband[3];           // 0x2c
; unsigned int maxbandl;             // 0x38
; unsigned int maxb;                 // 0x3c
; unsigned int regions[4];           // 0x40
; unsigned int preflag;              // 0x50
; unsigned int scalefac_scale;       // 0x54
; unsigned int count1table_select;   // 0x58
; unsigned int full_gain[3];         // 0x5c
; FLOAT  *pow2gain;                  // 0x68
; FLOAT  *ispow32_base;              // 0x6c
; int *mapp;                         // 0x70
;};
;---------------------------------------------------------------------------
mpxdec_l3mdct_hybrid_:
; in:  eax = fsinp (deqp), edx = tspnt (hout), ebx = rawoutp (blockp), ecx = gr_info_s *gr_info

push  esi
push  edi
push  ebp
mov   ebp,esp
sub   esp,00000020H

;  eax : fsinp
;  edx : tspnt
;  ebx : rawout1
;  ecx : rawout2
;  edi : winp
;  esi : cosp= COS9 or tfcos12

;  -4H[ebp] : sb
;  -8H[ebp] : empty
; -0cH[ebp] : mixed_block_flag
; -10H[ebp] : in7_old in antialias
; -14H[ebp] : ta33 in dct36()
; -18H[ebp] : ta66
; -1cH[ebp] : tb33
; -20H[ebp] : tb66

mov   esi,dword ptr 3cH[ecx]   ;
mov   dword ptr -4H[ebp],esi   ; sb=gr_info->maxb
mov   edi,32                   ;
sub   edi,esi                  ;
mov   dword ptr -8H[ebp],edi   ; empty=32-sb
mov   esi,dword ptr 10H[ecx]   ;
mov   dword ptr -0cH[ebp],esi  ; mixed_block_flag=gr_info->mixed_block_flag
mov   esi,dword ptr 0cH[ecx]   ; blocktype=gr_info->blocktype
			       ;
mov   ecx,ebx                  ; rawout1=rawout2=rawoutp
xor   byte ptr -4[ebx],1       ; blc=!blc   (blc stored in the rawoutp[-1])
jz   noblc                     ; if(blc){
 add   ebx,4616                ;  rawout1+=2*SBLIMIT*SSLIMIT+2
jmp endblc                     ;
noblc:                         ; }else{
 add   ecx,4616                ;  rawout2+=2*SBLIMIT*SSLIMIT+2
endblc:                        ; }
			       ;
cmp   byte ptr -4H[ebp],0      ; if(!sb)
jz    sbnull                   ;  goto sbnull
			       ;
mov   edi,esi                  ;
shl   esi,3                    ;
add   edi,esi                  ;
shl   edi,4                    ;
add   edi,offset _win          ; winp=&win[0][blocktype][0]
			       ;
cmp   esi,16                   ;
jne   nobt2                    ; if(blocktype==2){
 cmp   byte ptr -0cH[ebp],0    ;
 je    nombf                   ;  if(mixed_block_flag){
  mov   esi,2                  ;   sb_a=2
  call  III_antialias_32bit_   ;   III_antialias(fsinp,sb_a)
  mov   esi,offset _COS9       ;   cosp=COS9
  call  dct36_                 ;   dct36()
  add   edi,576                ;   winp+=144
  dec   byte ptr -4H[ebp]      ;   sb--
  jz    sbnull                 ;   if(!sb) goto sbnull
  call  dct36_                 ;   dct36()
  sub   edi,576                ;   winp-=144
  dec   byte ptr -4H[ebp]      ;   sb--
  jz    sbnull                 ;   if(!sb) goto sbnull
 nombf:                        ;  }
 mov   esi,offset _tfcos12     ;  cosp=tfcos12
 backbt2:                      ;  do{
  call  dct12_                 ;   dct12()
  add   edi,576                ;   winp+=144
  dec   byte ptr -4H[ebp]      ;   sb--
  jz    sbnull                 ;   if(!sb) goto sbnull
  call  dct12_                 ;   dct12()
  sub   edi,576                ;   winp-=144
  dec   byte ptr -4H[ebp]      ;   sb--
 jnz   backbt2                 ;  }while(sb)
 jmp   sbnull                  ;
nobt2:                         ; }else{
 mov   esi,dword ptr -4H[ebp]  ;  sb_a=sb
 call  III_antialias_32bit_    ;  III_antialias_32bit(fsinp,sb_a)
 mov   esi,offset _COS9        ;  cosp=COS9
 backnobt2:                    ;  do{
  call  dct36_                 ;   dct36()
  add   edi,576                ;   winp+=144
  dec   byte ptr -4H[ebp]      ;   sb--
  jz    sbnull                 ;   if(!sb) goto sbnull
  call  dct36_                 ;   dct36()
  sub   edi,576                ;   winp-=144
  dec   byte ptr -4H[ebp]      ;   sb--
 jnz   backnobt2               ;  }while(sb)
sbnull:                        ; }
mov   eax,dword ptr -8H[ebp]   ;
test  eax,eax                  ;
jz    endhybrid                ; if(!empty) goto endhybrid
mov   esi,4                    ;
backemptysb:                   ; do{
 mov   ah,18                   ;  ss=18
 backemptyss:                  ;  do{
  mov   edi,dword ptr [ebx]    ;   *tspnt=*rawout1
  add   ebx,esi                ;   rawout1++
  mov   dword ptr [edx],edi    ;
  add   edx,128                ;   tspnt+=32
  mov   dword ptr [ecx],0      ;   *rawout2=0
  add   ecx,esi                ;   rawout2++
  dec   ah                     ;   ss--
 jnz   backemptyss             ;  }while(ss)
 sub   edx,2300                ;  tspnt-=575
 dec   al                      ;  empty--
jnz   backemptysb              ; }while(empty)
			       ;
endhybrid:                     ;
mov   esp,ebp                  ;
pop   ebp                      ;
pop   edi                      ;
pop   esi                      ;
ret                            ;

mpxdec_l3mdct_hybrid:
push eax
push edx
push ebx
push ecx
mov eax,dword ptr [esp+ 4+16];
mov edx,dword ptr [esp+ 8+16];
mov ebx,dword ptr [esp+12+16];
mov ecx,dword ptr [esp+16+16];
call mpxdec_l3mdct_hybrid_
pop ecx
pop ebx
pop edx
pop eax
ret

;--------------------------------------------------------------------------
dct36_:                     ; c=COS9
fld    dword ptr  8[eax]    ;
fmul   dword ptr  4[esi]    ;
fld    dword ptr 24[eax]    ;
fmul   dword ptr 12[esi]    ;
fst    dword ptr -14H[ebp]  ; ta33=in[6]*c[3]
fadd                        ;
fld    dword ptr 40[eax]    ;
fmul   dword ptr 20[esi]    ;
fadd                        ;
fld    dword ptr 56[eax]    ;
fmul   dword ptr 28[esi]    ;
fadd                        ; tmp1a=in[2]*c[1]+ta33+in[10]*c[5]+in[14]*c[7]
fld    dword ptr 16[eax]    ;
fmul   dword ptr  8[esi]    ;
fadd   dword ptr [eax]      ;
fld    dword ptr 32[eax]    ;
fmul   dword ptr 16[esi]    ;
fadd                        ;
fld    dword ptr 48[eax]    ;
fmul   dword ptr 24[esi]    ;
fst    dword ptr -18H[ebp]  ; ta66=in[12]*c[6]
fadd                        ;
fld    dword ptr 64[eax]    ;
fmul   dword ptr 32[esi]    ;
fadd                        ; tmp2a=in[4]*c[2]+in[0]+in[8]*in[4]+ta66+in[16]*c[8]
fld    dword ptr 12[eax]    ;
fmul   dword ptr  4[esi]    ;
fld    dword ptr 28[eax]    ;
fmul   dword ptr 12[esi]    ;
fst    dword ptr -1cH[ebp]  ; tb33=in[7]*c[3]
fadd                        ;
fld    dword ptr 44[eax]    ;
fmul   dword ptr 20[esi]    ;
fadd                        ;
fld    dword ptr 60[eax]    ;
fmul   dword ptr 28[esi]    ;
fadd                        ; tmp1b=in[3]*c[1]+tb33+in[11]*c[5]+in[15]*c[7]
fld    dword ptr 20[eax]    ;
fmul   dword ptr  8[esi]    ;
fadd   dword ptr  4[eax]    ;
fld    dword ptr 36[eax]    ;
fmul   dword ptr 16[esi]    ;
fadd                        ;
fld    dword ptr 52[eax]    ;
fmul   dword ptr 24[esi]    ;
fst    dword ptr -20H[ebp]  ; tb66=in[13]*c[6]
fadd                        ;
fld    dword ptr 68[eax]    ;
fmul   dword ptr 32[esi]    ;
fadd                        ; tmp2b=in[5]*c[2]+in[1]+in[9]*c[4]+tb66+in[17]*c[8]

fld    st(3)                ;
fadd   st,st(3)             ; sum0=tmp1a+tmp2a
fld    st(2)                ;
fadd   st,st(2)             ;
fmul   dword ptr 36[esi]    ; sum1=(tmp1b+tmp2b)*COS9[9]
fld    st(1)                ;
fadd   st,st(1)             ; tmp=sum0+sum1
fld    dword ptr 108[edi]   ;
fmul   st,st(1)             ;
fstp   dword ptr 36[ecx]    ; out2[9]=tmp*winp[27]
fmul   dword ptr 104[edi]   ;
fstp   dword ptr 32[ecx]    ; out2[8]=tmp*winp[26]
fsub                        ; sum0-=sum1
fld    dword ptr 32[edi]    ;
fmul   st,st(1)             ;
fadd   dword ptr 32[ebx]    ;
fstp   dword ptr 1024[edx]  ; ts[256]=sum0*winp[8]+out1[8]
fmul   dword ptr 36[edi]    ;
fadd   dword ptr 36[ebx]    ;
fstp   dword ptr 1152[edx]  ; ts[288]=sum0*winp[9]+out1[9]

fsubr                       ;
fmul   dword ptr 68[esi]    ; sum1=(tmp2b-tmp1b)*COS9[17]
fxch   st(2)                ;
fsub                        ; sum0=tmp2a-tmp1a
fld    st(1)                ;
fadd   st,st(1)             ; tmp=sum0+sum1
fld    dword ptr 72[edi]    ;
fmul   st,st(1)             ;
fstp   dword ptr [ecx]      ; out2[0]=tmp*winp[18]
fmul   dword ptr 140[edi]   ;
fstp   dword ptr 68[ecx]    ; out2[17]=tmp*winp[35]
fsubr                       ; sum0-=sum1
fld    dword ptr [edi]      ;
fmul   st,st(1)             ;
fadd   dword ptr [ebx]      ;
fstp   dword ptr [edx]      ; ts[0]=sum0*winp[0]+out1[0]
fmul   dword ptr 68[edi]    ;
fadd   dword ptr 68[ebx]    ;
fstp   dword ptr 2176[edx]  ; ts[544]=sum0*winp[17]+out1[17]

fld    dword ptr  8[eax]    ;
fsub   dword ptr 40[eax]    ;
fsub   dword ptr 56[eax]    ;
fmul   dword ptr 12[esi]    ;
fld    dword ptr 16[eax]    ;
fsub   dword ptr 32[eax]    ;
fsub   dword ptr 64[eax]    ;
fmul   dword ptr 24[esi]    ;
fsub   dword ptr 48[eax]    ;
fadd   dword ptr [eax]      ;
fld    dword ptr 12[eax]    ;
fsub   dword ptr 44[eax]    ;
fsub   dword ptr 60[eax]    ;
fmul   dword ptr 12[esi]    ;
fld    dword ptr 20[eax]    ;
fsub   dword ptr 36[eax]    ;
fsub   dword ptr 68[eax]    ;
fmul   dword ptr 24[esi]    ;
fsub   dword ptr 52[eax]    ;
fadd   dword ptr  4[eax]    ;

fld    st(3)                ;
fadd   st,st(3)             ;
fld    st(2)                ;
fadd   st,st(2)             ;
fmul   dword ptr 40[esi]    ;
fld    st(1)                ;
fadd   st,st(1)             ;
fld    dword ptr 112[edi]   ;
fmul   st,st(1)             ;
fstp   dword ptr 40[ecx]    ;
fmul   dword ptr 100[edi]   ;
fstp   dword ptr 28[ecx]    ;
fsub                        ;
fld    dword ptr 28[edi]    ;
fmul   st,st(1)             ;
fadd   dword ptr 28[ebx]    ;
fstp   dword ptr 896[edx]   ;
fmul   dword ptr 40[edi]    ;
fadd   dword ptr 40[ebx]    ;
fstp   dword ptr 1280[edx]  ;

fsubr                       ;
fmul   dword ptr 64[esi]    ;
fxch   st(2)                ;
fsub                        ;
fld    st(1)                ;
fadd   st,st(1)             ;
fld    dword ptr 76[edi]    ;
fmul   st,st(1)             ;
fstp   dword ptr  4[ecx]    ;
fmul   dword ptr 136[edi]   ;
fstp   dword ptr 64[ecx]    ;
fsubr                       ;
fld    dword ptr  4[edi]    ;
fmul   st,st(1)             ;
fadd   dword ptr  4[ebx]    ;
fstp   dword ptr 128[edx]   ;
fmul   dword ptr 64[edi]    ;
fadd   dword ptr 64[ebx]    ;
fstp   dword ptr 2048[edx]  ;

fld    dword ptr  8[eax]    ;
fmul   dword ptr 20[esi]    ;
fsub   dword ptr -14H[ebp]  ;
fld    dword ptr 40[eax]    ;
fmul   dword ptr 28[esi]    ;
fsub                        ;
fld    dword ptr 56[eax]    ;
fmul   dword ptr  4[esi]    ;
fadd                        ;
fld    dword ptr 16[eax]    ;
fmul   dword ptr 32[esi]    ;
fsubr  dword ptr [eax]      ;
fld    dword ptr 32[eax]    ;
fmul   dword ptr  8[esi]    ;
fsub                        ;
fadd   dword ptr -18H[ebp]  ;
fld    dword ptr 64[eax]    ;
fmul   dword ptr 16[esi]    ;
fadd                        ;
fld    dword ptr 12[eax]    ;
fmul   dword ptr 20[esi]    ;
fsub   dword ptr -1cH[ebp]  ;
fld    dword ptr 44[eax]    ;
fmul   dword ptr 28[esi]    ;
fsub                        ;
fld    dword ptr 60[eax]    ;
fmul   dword ptr  4[esi]    ;
fadd                        ;
fld    dword ptr 20[eax]    ;
fmul   dword ptr 32[esi]    ;
fsubr  dword ptr  4[eax]    ;
fld    dword ptr 36[eax]    ;
fmul   dword ptr  8[esi]    ;
fsub                        ;
fadd   dword ptr -20H[ebp]  ;
fld    dword ptr 68[eax]    ;
fmul   dword ptr 16[esi]    ;
fadd                        ;

fld    st(3)                ;
fadd   st,st(3)             ;
fld    st(2)                ;
fadd   st,st(2)             ;
fmul   dword ptr 44[esi]    ;
fld    st(1)                ;
fadd   st,st(1)             ;
fld    dword ptr 116[edi]   ;
fmul   st,st(1)             ;
fstp   dword ptr 44[ecx]    ;
fmul   dword ptr 96[edi]    ;
fstp   dword ptr 24[ecx]    ;
fsub                        ;
fld    dword ptr 24[edi]    ;
fmul   st,st(1)             ;
fadd   dword ptr 24[ebx]    ;
fstp   dword ptr 768[edx]   ;
fmul   dword ptr 44[edi]    ;
fadd   dword ptr 44[ebx]    ;
fstp   dword ptr 1408[edx]  ;

fsubr                       ;
fmul   dword ptr 60[esi]    ;
fxch   st(2)                ;
fsub                        ;
fld    st(1)                ;
fadd   st,st(1)             ;
fld    dword ptr 80[edi]    ;
fmul   st,st(1)             ;
fstp   dword ptr  8[ecx]    ;
fmul   dword ptr 132[edi]   ;
fstp   dword ptr 60[ecx]    ;
fsubr                       ;
fld    dword ptr  8[edi]    ;
fmul   st,st(1)             ;
fadd   dword ptr  8[ebx]    ;
fstp   dword ptr 256[edx]   ;
fmul   dword ptr 60[edi]    ;
fadd   dword ptr 60[ebx]    ;
fstp   dword ptr 1920[edx]  ;

fld    dword ptr  8[eax]    ;
fmul   dword ptr 28[esi]    ;
fsub   dword ptr -14H[ebp]  ;
fld    dword ptr 40[eax]    ;
fmul   dword ptr  4[esi]    ;
fadd                        ;
fld    dword ptr 56[eax]    ;
fmul   dword ptr 20[esi]    ;
fsub                        ;
fld    dword ptr 16[eax]    ;
fmul   dword ptr 16[esi]    ;
fsubr  dword ptr [eax]      ;
fld    dword ptr 32[eax]    ;
fmul   dword ptr 32[esi]    ;
fadd                        ;
fadd   dword ptr -18H[ebp]  ;
fld    dword ptr 64[eax]    ;
fmul   dword ptr  8[esi]    ;
fsub                        ;
fld    dword ptr 12[eax]    ;
fmul   dword ptr 28[esi]    ;
fsub   dword ptr -1cH[ebp]  ;
fld    dword ptr 44[eax]    ;
fmul   dword ptr  4[esi]    ;
fadd                        ;
fld    dword ptr 60[eax]    ;
fmul   dword ptr 20[esi]    ;
fsub                        ;
fld    dword ptr 20[eax]    ;
fmul   dword ptr 16[esi]    ;
fsubr  dword ptr  4[eax]    ;
fld    dword ptr 36[eax]    ;
fmul   dword ptr 32[esi]    ;
fadd                        ;
fadd   dword ptr -20H[ebp]  ;
fld    dword ptr 68[eax]    ;
fmul   dword ptr  8[esi]    ;
fsub                        ;

fld    st(3)                ;
fadd   st,st(3)             ;
fld    st(2)                ;
fadd   st,st(2)             ;
fmul   dword ptr 48[esi]    ;
fld    st(1)                ;
fadd   st,st(1)             ;
fld    dword ptr 120[edi]   ;
fmul   st,st(1)             ;
fstp   dword ptr 48[ecx]    ;
fmul   dword ptr 92[edi]    ;
fstp   dword ptr 20[ecx]    ;
fsub                        ;
fld    dword ptr 20[edi]    ;
fmul   st,st(1)             ;
fadd   dword ptr 20[ebx]    ;
fstp   dword ptr 640[edx]   ;
fmul   dword ptr 48[edi]    ;
fadd   dword ptr 48[ebx]    ;
fstp   dword ptr 1536[edx]  ;

fsubr                       ;
fmul   dword ptr 56[esi]    ;
fxch   st(2)                ;
fsub                        ;
fld    st(1)                ;
fadd   st,st(1)             ;
fld    dword ptr 84[edi]    ;
fmul   st,st(1)             ;
fstp   dword ptr 12[ecx]    ;
fmul   dword ptr 128[edi]   ;
fstp   dword ptr 56[ecx]    ;
fsubr                       ;
fld    dword ptr 12[edi]    ;
fmul   st,st(1)             ;
fadd   dword ptr 12[ebx]    ;
fstp   dword ptr 384[edx]   ;
fmul   dword ptr 56[edi]    ;
fadd   dword ptr 56[ebx]    ;
fstp   dword ptr 1792[edx]  ;

fld    dword ptr [eax]      ;
fsub   dword ptr 16[eax]    ;
fadd   dword ptr 32[eax]    ;
fsub   dword ptr 48[eax]    ;
fadd   dword ptr 64[eax]    ;
fld    dword ptr  4[eax]    ;
fsub   dword ptr 20[eax]    ;
fadd   dword ptr 36[eax]    ;
fsub   dword ptr 52[eax]    ;
fadd   dword ptr 68[eax]    ;
fmul   dword ptr 52[esi]    ;

fld    st(1)                ;
fadd   st,st(1)             ;
fld    dword ptr 124[edi]   ;
fmul   st,st(1)             ;
fstp   dword ptr 52[ecx]    ;
fmul   dword ptr 88[edi]    ;
fstp   dword ptr 16[ecx]    ;
fsub                        ;
fld    dword ptr 16[edi]    ;
fmul   st,st(1)             ;
add  eax,72                 ;
fadd   dword ptr 16[ebx]    ;
add  ecx,72                 ;
fstp   dword ptr 512[edx]   ;
fmul   dword ptr 52[edi]    ;
add  edx,4                  ;
fadd   dword ptr 52[ebx]    ;
add  ebx,72                 ;
fstp   dword ptr 1660[edx]  ;

ret
;---------------------------------------------------------------------------
dct12_:
fld    dword ptr [ebx]         ;
fld    dword ptr  4[ebx]       ;
fld    dword ptr  8[ebx]       ;
fld    dword ptr 12[ebx]       ;
fld    dword ptr 16[ebx]       ;
fld    dword ptr 20[ebx]       ;
fstp   dword ptr 640[edx]      ;
fstp   dword ptr 512[edx]      ;
fstp   dword ptr 384[edx]      ;
fstp   dword ptr 256[edx]      ;
fstp   dword ptr 128[edx]      ;
fstp   dword ptr [edx]         ;
fldz                           ;
fst    dword ptr 48[ecx]       ;
fst    dword ptr 52[ecx]       ;
fst    dword ptr 56[ecx]       ;
fst    dword ptr 60[ecx]       ;
fst    dword ptr 64[ecx]       ;
fstp   dword ptr 68[ecx]       ;
fld    dword ptr 48[eax]       ;
fadd   dword ptr 36[eax]       ;
fld    dword ptr 36[eax]       ;
fadd   dword ptr 24[eax]       ;
fld    dword ptr 12[eax]       ;
fadd   dword ptr [eax]         ;
fld    dword ptr 60[eax]       ;
fadd   dword ptr 48[eax]       ;
fadd   st,st(2)                ;
fld    dword ptr [eax]         ;
fsub   st,st(4)                ;
fld    st(2)                   ;
fsub   st,st(2)                ;
fmul   dword ptr  4[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 40[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr 64[ebx]       ;
fstp   dword ptr 2048[edx]     ;
fmul   dword ptr 28[edi]       ;
fadd   dword ptr 52[ebx]       ;
fstp   dword ptr 1664[edx]     ;
fsub                           ;
fld    dword ptr  4[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr 28[ebx]       ;
fstp   dword ptr 896[edx]      ;
fmul   dword ptr 16[edi]       ;
fadd   dword ptr 40[ebx]       ;
fstp   dword ptr 1280[edx]     ;
fmul   dword ptr 16[esi]       ;
fadd   st,st(1)                ;
fxch   st(2)                   ;
fadd                           ;
fmul   dword ptr 12[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fmul   dword ptr [esi]         ;
fxch   st(2)                   ;
fsubr                          ;
fmul   dword ptr  8[esi]       ;
fxch   st(2)                   ;
fmul   dword ptr 16[esi]       ;
fadd   dword ptr [eax]         ;
fld    dword ptr 24[eax]       ;
fadd   dword ptr 12[eax]       ;
fmul   dword ptr 12[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fxch   st(2)                   ;
fsubr                          ;
fxch   st(2)                   ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 32[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr 56[ebx]       ;
fstp   dword ptr 1792[edx]     ;
fmul   dword ptr 36[edi]       ;
fadd   dword ptr 60[ebx]       ;
fstp   dword ptr 1920[edx]     ;
fsub                           ;
fld    dword ptr  8[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr 32[ebx]       ;
fstp   dword ptr 1024[edx]     ;
fmul   dword ptr 12[edi]       ;
fadd   dword ptr 36[ebx]       ;
fstp   dword ptr 1152[edx]     ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 44[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr 68[ebx]       ;
fstp   dword ptr 2176[edx]     ;
fmul   dword ptr 24[edi]       ;
fadd   dword ptr 48[ebx]       ;
fstp   dword ptr 1536[edx]     ;
fsubr                          ;
fld    dword ptr [edi]         ;
fmul   st,st(1)                ;
fadd   dword ptr 24[ebx]       ;
fstp   dword ptr 768[edx]      ;
fmul   dword ptr 20[edi]       ;
fadd   dword ptr 44[ebx]       ;
fstp   dword ptr 1408[edx]     ;
fld    dword ptr 52[eax]       ;
fadd   dword ptr 40[eax]       ;
fld    dword ptr 40[eax]       ;
fadd   dword ptr 28[eax]       ;
fld    dword ptr 16[eax]       ;
fadd   dword ptr  4[eax]       ;
fld    dword ptr 64[eax]       ;
fadd   dword ptr 52[eax]       ;
fadd   st,st(2)                ;
fld    dword ptr  4[eax]       ;
fsub   st,st(4)                ;
fld    st(2)                   ;
fsub   st,st(2)                ;
fmul   dword ptr  4[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 40[edi]       ;
fmul   st,st(1)                ;
fstp   dword ptr 16[ecx]       ;
fmul   dword ptr 28[edi]       ;
fstp   dword ptr  4[ecx]       ;
fsub                           ;
fld    dword ptr  4[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr 1664[edx]     ;
fstp   dword ptr 1664[edx]     ;
fmul   dword ptr 16[edi]       ;
fadd   dword ptr 2048[edx]     ;
fstp   dword ptr 2048[edx]     ;
fmul   dword ptr 16[esi]       ;
fadd   st,st(1)                ;
fxch   st(2)                   ;
fadd                           ;
fmul   dword ptr 12[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fmul   dword ptr [esi]         ;
fxch   st(2)                   ;
fsubr                          ;
fmul   dword ptr  8[esi]       ;
fxch   st(2)                   ;
fmul   dword ptr 16[esi]       ;
fadd   dword ptr  4[eax]       ;
fld    dword ptr 28[eax]       ;
fadd   dword ptr 16[eax]       ;
fmul   dword ptr 12[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fxch   st(2)                   ;
fsubr                          ;
fxch   st(2)                   ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 32[edi]       ;
fmul   st,st(1)                ;
fstp   dword ptr  8[ecx]       ;
fmul   dword ptr 36[edi]       ;
fstp   dword ptr 12[ecx]       ;
fsub                           ;
fld    dword ptr  8[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr 1792[edx]     ;
fstp   dword ptr 1792[edx]     ;
fmul   dword ptr 12[edi]       ;
fadd   dword ptr 1920[edx]     ;
fstp   dword ptr 1920[edx]     ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 44[edi]       ;
fmul   st,st(1)                ;
fstp   dword ptr 20[ecx]       ;
fmul   dword ptr 24[edi]       ;
fstp   dword ptr [ecx]         ;
fsubr                          ;
fld    dword ptr [edi]         ;
fmul   st,st(1)                ;
fadd   dword ptr 1536[edx]     ;
fstp   dword ptr 1536[edx]     ;
fmul   dword ptr 20[edi]       ;
fadd   dword ptr 2176[edx]     ;
fstp   dword ptr 2176[edx]     ;
fld    dword ptr 56[eax]       ;
fadd   dword ptr 44[eax]       ;
fld    dword ptr 44[eax]       ;
fadd   dword ptr 32[eax]       ;
fld    dword ptr 20[eax]       ;
fadd   dword ptr  8[eax]       ;
fld    dword ptr 68[eax]       ;
fadd   dword ptr 56[eax]       ;
fadd   st,st(2)                ;
fld    dword ptr  8[eax]       ;
fsub   st,st(4)                ;
fld    st(2)                   ;
fsub   st,st(2)                ;
fmul   dword ptr  4[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 40[edi]       ;
fmul   st,st(1)                ;
fstp   dword ptr 40[ecx]       ;
fmul   dword ptr 28[edi]       ;
fstp   dword ptr 28[ecx]       ;
fsub                           ;
fld    dword ptr  4[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr  4[ecx]       ;
fstp   dword ptr  4[ecx]       ;
fmul   dword ptr 16[edi]       ;
fadd   dword ptr 16[ecx]       ;
fstp   dword ptr 16[ecx]       ;
fmul   dword ptr 16[esi]       ;
fadd   st,st(1)                ;
fxch   st(2)                   ;
fadd                           ;
fmul   dword ptr 12[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fmul   dword ptr [esi]         ;
fxch   st(2)                   ;
fsubr                          ;
fmul   dword ptr  8[esi]       ;
fxch   st(2)                   ;
fmul   dword ptr 16[esi]       ;
fadd   dword ptr  8[eax]       ;
fld    dword ptr 32[eax]       ;
fadd   dword ptr 20[eax]       ;
fmul   dword ptr 12[esi]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fxch   st(2)                   ;
fsubr                          ;
fxch   st(2)                   ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 32[edi]       ;
fmul   st,st(1)                ;
fstp   dword ptr 32[ecx]       ;
fmul   dword ptr 36[edi]       ;
fstp   dword ptr 36[ecx]       ;
fsub                           ;
fld    dword ptr  8[edi]       ;
fmul   st,st(1)                ;
fadd   dword ptr  8[ecx]       ;
fstp   dword ptr  8[ecx]       ;
fmul   dword ptr 12[edi]       ;
fadd   dword ptr 12[ecx]       ;
fstp   dword ptr 12[ecx]       ;
fld    st(1)                   ;
fadd   st,st(1)                ;
fld    dword ptr 44[edi]       ;
fmul   st,st(1)                ;
fstp   dword ptr 44[ecx]       ;
fmul   dword ptr 24[edi]       ;
fstp   dword ptr 24[ecx]       ;
fsubr                          ;
fld    dword ptr [edi]         ;
fmul   st,st(1)                ;
fadd   dword ptr [ecx]         ;
add   eax,72                   ;
fstp   dword ptr [ecx]         ;
fmul   dword ptr 20[edi]       ;
add   edx,4                    ;
fadd   dword ptr 20[ecx]       ;
add   ebx,72                   ;
fstp   dword ptr 20[ecx]       ;
add   ecx,72                   ;
ret                            ;

;-------------------------------------------------------------------------

_TEXT		ENDS
CONST		SEGMENT	DWORD PUBLIC USE32 'DATA'
CONST		ENDS
CONST2		SEGMENT	DWORD PUBLIC USE32 'DATA'
CONST2		ENDS
_DATA		SEGMENT	DWORD PUBLIC USE32 'DATA'
L$24:
    DB	33H, 33H, 33H, 33H, 33H, 33H, 0e3H, 0bfH
    DB	1fH, 85H, 0ebH, 51H, 0b8H, 1eH, 0e1H, 0bfH
    DB	1fH, 85H, 0ebH, 51H, 0b8H, 1eH, 0d5H, 0bfH
    DB	0aeH, 47H, 0e1H, 7aH, 14H, 0aeH, 0c7H, 0bfH
    DB	52H, 0b8H, 1eH, 85H, 0ebH, 51H, 0b8H, 0bfH
    DB	0cbH, 0a1H, 45H, 0b6H, 0f3H, 0fdH, 0a4H, 0bfH
    DB	86H, 5aH, 0d3H, 0bcH, 0e3H, 14H, 8dH, 0bfH
    DB	0acH, 0adH, 0d8H, 5fH, 76H, 4fH, 6eH, 0bfH

_DATA		ENDS
_BSS		SEGMENT	DWORD PUBLIC USE32 'BSS'

    ORG 0
_aa_cs   LABEL	BYTE ; 32*4=128
    ORG 128
_win     LABEL	BYTE ; 288*4=1152
    ORG 1280
_COS9    LABEL	BYTE ; 18*4=72
    ORG 1352
_tfcos12 LABEL	BYTE ; 5*4=20
    ORG 1372

_BSS		ENDS

		END
