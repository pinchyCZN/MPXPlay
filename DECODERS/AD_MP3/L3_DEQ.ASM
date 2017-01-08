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
;function: Audio MPEG Layer III sub-routines (huffman decoding, dequantize-sample)
;based on the MPG123 decoder source

;set __WASM__=\WATCOM\binnt\wasm.exe
;%__WASM__% -q -w2 -fo=$(IntDir)\$(InputName).obj $(InputPath)
;EDITBIN.EXE $(IntDir)\$(InputName).obj
;$(OutDir)\$(InputName).obj

.387
.486p
.model flat
		PUBLIC	mpxdec_l3deq_init_
		PUBLIC	mpxdec_l3deq_dequantize_
		PUBLIC	mpxdec_l3deq_dequantize_ms_
		PUBLIC	mpxdec_l3deq_dequantize_bt2_
		PUBLIC	mpxdec_l3deq_dequantize_ms_bt2_
		PUBLIC	mpxdec_l3deq_dequantize_bt2_js_
		PUBLIC	mpxdec_l3deq_dequantize_ms_bt2_js_

        PUBLIC	mpxdec_l3deq_init
        PUBLIC	mpxdec_l3deq_dequantize
		PUBLIC	mpxdec_l3deq_dequantize_ms
		PUBLIC	mpxdec_l3deq_dequantize_bt2
		PUBLIC	mpxdec_l3deq_dequantize_ms_bt2
		PUBLIC	mpxdec_l3deq_dequantize_bt2_js
		PUBLIC	mpxdec_l3deq_dequantize_ms_bt2_js

        PUBLIC  _mpxdec_l3deq_gainpow2_2
        PUBLIC  _mpxdec_l3deq_gainpow2_4
        PUBLIC  _mpxdec_l3deq_ispow32_2
        PUBLIC  _mpxdec_l3deq_ispow32_4

        PUBLIC	_mpxdec_l3deq_init
        PUBLIC	_mpxdec_l3deq_dequantize
		PUBLIC	_mpxdec_l3deq_dequantize_ms
		PUBLIC	_mpxdec_l3deq_dequantize_bt2
		PUBLIC	_mpxdec_l3deq_dequantize_ms_bt2
		PUBLIC	_mpxdec_l3deq_dequantize_bt2_js
		PUBLIC	_mpxdec_l3deq_dequantize_ms_bt2_js

		EXTRN	_mpxdec_bitindex:BYTE
		EXTRN	_mpxdec_wordpointer:BYTE

DGROUP		GROUP	CONST,CONST2,_DATA,_BSS
_TEXT		SEGMENT	BYTE PUBLIC USE32 'CODE'
		ASSUME CS:_TEXT, DS:DGROUP, SS:DGROUP

;extern struct gr_info_s {
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
;}si[2][2];


mpxdec_l3deq_init:
mpxdec_l3deq_init_:
_mpxdec_l3deq_init:
push      eax
push      ecx
push      edx
push      edi
push      esi
push      ebp
mov       ebp,esp
sub       esp,4

mov  dword ptr -4[ebp],4            ;
fild dword ptr -4[ebp]              ;
mov  dword ptr -4[ebp],3            ;
fidiv dword ptr -4[ebp]             ; st(0)=4.0/3.0
				    ;
mov  edx,15                         ;
mov  ecx,offset _ispow              ;
L$1:                                ; for(i=15;i<(8192+15);i++){
                                     ;
; ispow[i]=pow((double)i,(double)4.0/3.0)
                                     ;
 fld  st                             ;
 mov  dword ptr -4[ebp],edx          ;
 fild dword ptr -4[ebp]              ;
 fyl2x                               ; v=log2(i)*4.0/3.0
				     ;
 fld   st                            ;
 frndint                             ;
 fxch  st(1)                         ;
 fsub  st,st(1)                      ;
 f2xm1                               ;
 fld1                                ;
 fadd                                ;
 fscale                              ;
 fstp  st(1)                         ; v=pow(2.0,v)
                                     ;
 fstp  dword ptr [ecx]               ; ispow[i]=v
				     ;
 inc  edx                            ;
 add  ecx,4                          ;
 cmp  edx,8207                       ;
jl   L$1                            ; }
fstp st(0)                          ;

fld1                                ;
mov dword ptr -4[ebp],4             ;
fidiv dword ptr -4[ebp]             ;
fchs                                ; tmp0=-0.25
fild dword ptr -4[ebp]              ;
mov dword ptr -4[ebp],3             ;
fidiv dword ptr -4[ebp]             ; tmp1=4.0/3.0
xor ecx,ecx                         ; i=0
back_gp_i:                          ; do{
 mov edi,ecx                         ;
 mov eax,edi                         ;
 and edi,1                           ;
 shl edi,8                           ;
 shr eax,1                           ;
 add edi,eax                         ; indx2=((i&1)<<8)+(i>>1);
 shl edi,2                           ; (indx2*=4)
 mov esi,ecx                         ;
 mov eax,esi                         ;
 and esi,3                           ;
 shl esi,7                           ;
 shr eax,2                           ;
 add esi,eax                         ; indx4=((i&3)<<7)+(i>>2);
 shl esi,2                           ; (indx4*=4)
                                     ; fg=pow(2.0,-0.25*(double)(i-46));
 mov dword ptr -4[ebp],ecx           ;
 fild dword ptr -4[ebp]              ;
 mov dword ptr -4[ebp],46            ;
 fisub dword ptr -4[ebp]             ; v=i-46
 fmul st,st(2)                       ; v*=-0.25
 fld st                              ;
 frndint                             ;
 fxch st(1)                          ;
 fsub st,st(1)                       ;
 f2xm1                               ;
 fld1                                ;
 fadd                                ;
 fscale                              ;
 fstp st(1)                          ; fg=pow(2.0,v)
 fst  dword ptr FLAT:_mpxdec_l3deq_gainpow2_2[edi]; gainpow2_2[indx2]=fg
 fst  dword ptr FLAT:_mpxdec_l3deq_gainpow2_4[esi]; gainpow2_4[indx4]=fg
 shl edi,5                           ; indx2*=32
 add edi,64                          ; indx2+=16
 shl esi,5                           ; indx4*=32
 add esi,64                          ; indx4+=16
 mov edx,1                           ; j=1
 mov eax,4                           ;
 back_gp_j:                          ; do{
                                      ; isg=pow((double)j,4.0/3.0)*fg
  fld st(1)                           ;
  mov dword ptr -4[ebp],edx           ;
  fild dword ptr -4[ebp]              ;
  fyl2x                               ; v=log2(j)*4.0/3.0
  fld st                              ;
  frndint                             ;
  fxch st(1)                          ;
  fsub st,st(1)                       ;
  f2xm1                               ;
  fld1                                ;
  fadd                                ;
  fscale                              ;
  fstp st(1)                          ; v=pow(2.0,v)
  fmul st,st(1)                       ; isg= v * fg
  fst  dword ptr FLAT:_mpxdec_l3deq_ispow32_2[edi+eax]; ispow32_2[indx2+j]=isg
  fst  dword ptr FLAT:_mpxdec_l3deq_ispow32_4[esi+eax]; ispow32_4[indx4+j]=isg
  fchs                                ;                 isg=-isg
  neg eax                             ;
  fst  dword ptr FLAT:_mpxdec_l3deq_ispow32_2[edi+eax]; ispow32_2[indx2-j]=isg
  fstp dword ptr FLAT:_mpxdec_l3deq_ispow32_4[esi+eax]; ispow32_4[indx4-j]=isg
  neg eax                             ;
  add eax,4                           ;
  inc edx                             ; j++
  cmp edx,16                          ;
 jl back_gp_j                        ; }while(j<16)
 fstp st(0)                          ; pop fg
 inc ecx                             ; i++
 cmp ecx,378                         ;
jl back_gp_i                        ; }while(i<(256+118+4))
fstp st(0)                          ; pop 4.0/3.0
fstp st(0)                          ; pop -0.25

mov       esp,ebp
pop       ebp
pop       esi
pop       edi
pop       edx
pop       ecx
pop       eax
ret

;-----------------------------------------------------------------------
mpxdec_l3deq_dequantize_:
push esi
push edi
push ebp
mov  ebp,esp
sub  esp,40H

; eax = locbits (24 bits)
; edx = loc_bitindex
; edi = xrpnt

;  -4H[ebp] : mc
;  -8H[ebp] : lp
; -0cH[ebp] : htable
; -10H[ebp] : locbits
; -14H[ebp] : sub32hlin
; -18H[ebp] : hlinbits
; -1cH[ebp] : i
; -20H[ebp] : part2end
; -24H[ebp] : lastxr
; -28H[ebp] : m
; -2cH[ebp] : gr_info
; -30H[ebp] : xr
; -34H[ebp] : scf
; -38H[ebp] : pow2index
; -3cH[ebp] : me
; -40H[ebp] : loc_wordpointer

mov   dword ptr -30H[ebp],eax       ; xr=xr_p
mov   dword ptr -24H[ebp],eax       ; lastxr=xr_p
mov   edi,eax                       ; xrpnt=xr_p
mov   dword ptr -34H[ebp],edx       ; scf=scf_p
mov   dword ptr -2cH[ebp],ebx       ; gr_info=gr_info_p
mov   edx,dword ptr FLAT:_mpxdec_bitindex  ; loc_bitindex=mpxdec_bitindex
mov   eax,dword ptr FLAT:_mpxdec_wordpointer;
mov   dword ptr -40H[ebp],eax       ; loc_wordpointer=wordpointer
mov   eax,dword ptr 4H[ebx]         ;
add   eax,ecx                       ;
mov   dword ptr -20H[ebp],eax       ; part2end=part2begin+gr_info->part2_3_length
mov   dword ptr FLAT:_mpxdec_bitindex,eax  ; bitindex=part2end
mov   eax,dword ptr 70H[ebx]        ;
mov   dword ptr -28H[ebp],eax       ; m=gr_info->mapp
add   eax,00000058H                 ;
mov   dword ptr -3cH[ebp],eax       ; me=m+22
fldz                                ;
fst   dword ptr -38H[ebp]           ; pow2index=0
fst   dword ptr -1cH[ebp]           ; i=0
mov   dword ptr -4H[ebp],1          ; mc=1
L$2:                                ; do{
 mov   eax,dword ptr -1cH[ebp]       ;
 shl   eax,2                         ;
 add   eax,dword ptr -2cH[ebp]       ;
 mov   ebx,dword ptr 40H[eax]        ; ebx=gr_info->regions[i]
 test  ebx,ebx                       ;
 jz    L$27                          ; if(lp){
  mov   dword ptr -8H[ebp],ebx       ; lp=ebx
  mov   ebx,dword ptr 14H[eax]        ;
  shl   ebx,3                         ;
  add   ebx,offset FLAT:_ht           ; h=ht+gr_info->table_select[i]
  mov   eax,dword ptr 4H[ebx]         ;
  mov   dword ptr -0cH[ebp],eax       ; htable=h->table
  mov   eax,dword ptr [ebx]           ; hlinbits=h->linbits
  test  eax,eax                       ;
  jz    L$17                          ; if(hlinbits){
   mov   ebx,32                        ;
   sub   ebx,eax                       ;
   mov   dword ptr -14H[ebp],ebx       ; sub32hlin=32-hlinbits
   inc   eax                           ; hlinbits++
   mov   dword ptr -18H[ebp],eax       ; hlinbits=eax
L$3:                                   ; do{
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$4                           ; if(!mc){
     fstp  st                            ;
     mov   eax,dword ptr -28H[ebp]       ;
     mov   ecx,dword ptr [eax]           ;
     mov   dword ptr -4H[ebp],ecx        ; mc=m[0]
     add   eax,4                         ;
     mov   dword ptr -28H[ebp],eax       ; m++
     mov   eax,dword ptr -34H[ebp]       ;
     movzx esi,byte ptr [eax]            ;
     mov   dword ptr -38H[ebp],esi       ; pow2index=scf[0]
     inc   eax                           ;
     mov   dword ptr -34H[ebp],eax       ; scf++
     mov   eax,dword ptr -2cH[ebp]       ; // eax=gr_info
     mov   ecx,dword ptr 68H[eax]        ;
     shl   esi,2                         ;
     fld   dword ptr [ecx+esi]           ; v=gr_info->pow2gain[pow2index]
     shl   esi,5                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
L$4:                                    ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -40H[ebp]       ;
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; *val=htable
L$5:                                    ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$6                           ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++;
     shl   eax,1                         ; locbits<<=1
     jnc   L$5                           ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
    jmp   L$5                           ; }while(1)
L$6:                                    ;
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    jz    L$10                          ; if(x){
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     cmp   cl,0f0H                       ;
     jne   L$8                           ; if(x==15){
      mov   eax,edx                       ; locbits=get3bytes()
      shr   eax,3                         ;
      mov   ecx,edx                       ;
      add   eax,dword ptr -40H[ebp]       ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7			  ;
      shl   eax,cl                        ;
      mov   ecx,dword ptr -14H[ebp]       ;
      mov   dword ptr -10H[ebp],eax       ; locbits=eax
      shr   eax,cl                        ; x=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[eax*4]  ; a=ispow[x]*v
      mov   ecx,dword ptr -18H[ebp]       ; ecx=hlinbits
      fmul  st,st(1)                      ;
      mov   eax,dword ptr -10H[ebp]       ; eax=locbits
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits//+1
      jnc   L$7                           ; if(locbits&0x80000000)
       fchs                               ;  a=-a
L$7:                                      ;
      fstp  dword ptr [edi]               ; xrpnt[0]=a
     jmp   L$11                          ;
L$8:                                     ; }else{
      shr   ecx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$9                           ; if(locbits&0x80000000)
       neg   ecx                          ;  x=-x
L$9:                                      ;
      inc   edx                           ; loc_bitindex++
      mov   ecx,dword ptr [esi+ecx]       ; x=isp32[x]
L$10:                                   ; }else
    mov   dword ptr [edi],ecx           ;  xrpnt[0]=x
L$11:                                   ;
    add   edi,4                         ; xrpnt++
					;
    and   ebx,0000000fH                 ; y&=0x0f
    jz    L$15                          ; if(y){
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     cmp   ebx,0000000fH                 ;
     jne   L$13                          ; if(y==15){
      mov   eax,edx                       ;
      mov   ecx,eax                       ; locbits=get3bytes()
      shr   eax,3                         ;
      add   eax,dword ptr -40H[ebp];
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   ecx,dword ptr -14H[ebp]       ;
      mov   ebx,eax                       ; y=locbits>>sub32hlin
      shr   ebx,cl                        ;
      fld   dword ptr FLAT:_ispow[ebx*4]  ; a=ispow[y]*v
      mov   ecx,dword ptr -18H[ebp]       ; ecx=hlinbits
      fmul  st,st(1)                      ;
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits//+1
      jnc   L$12                          ; if(locbits&0x80000000)
       fchs                               ;  a=-a
L$12:                                     ;
      fstp  dword ptr [edi]               ; xrpnt[0]=a
     jmp   L$16                          ;
L$13:                                    ; }else{
      shl   ebx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$14                          ; if(locbits&0x80000000)
       neg   ebx                          ;  y=-y
L$14:                                     ;
      inc   edx                           ; loc_bitindex++
      mov   ebx,dword ptr [esi+ebx]       ; y=isp32[y]
L$15:                                   ; }else
    mov   dword ptr [edi],ebx           ;  xrpnt[0]=y
L$16:                                   ;
    add   edi,4                         ; xrpnt++
    dec   dword ptr -8H[ebp]           ; lp--
   jnz   L$3                           ; }while(lp)
  jmp   near ptr L$27                 ; }else{  // hlinbits
L$17:                                  ; do{   // lp
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$18                          ; if(!mc){
     mov   eax,dword ptr -28H[ebp]       ;
     mov   esi,dword ptr [eax]           ;
     mov   dword ptr -4H[ebp],esi        ; mc=m[0]
     add   eax,4                         ;
     mov   dword ptr -28H[ebp],eax       ; m++
     mov   eax,dword ptr -34H[ebp]       ;
     movzx esi,byte ptr [eax]            ;
     mov   dword ptr -38H[ebp],esi       ; pow2index=scf[0]
     inc   eax                           ;
     mov   dword ptr -34H[ebp],eax       ; scf++
     shl   esi,7                         ;
     mov   eax,dword ptr -2cH[ebp]       ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base+(pow2index<<5)
L$18:                                   ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -40H[ebp];
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; *val=htable
L$19:                                   ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$20                          ;  break
     add   ecx,2                         ; y++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$19                          ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
    jmp   L$19                          ; }while(1)
L$20:                                   ;
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    jz    L$22                          ; if(x){
     shr   ecx,2                         ; x>>=4
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$21                          ; if(locbits&0x80000000)
      neg   ecx                          ;  x=-x
L$21:                                    ;
     mov   ecx,dword ptr [esi+ecx]       ; x=isp32[x]
L$22:                                   ; }
    mov   dword ptr [edi],ecx           ; xrpnt[0]=x
    add   edi,4                         ; xrpnt++
    and   ebx,0000000fH                 ; y&=0x0f
    jz    L$25                          ; if(y){
     shl   ebx,2                         ;
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$24                          ; if(locbits&0x80000000)
      neg   ebx                          ;  y=-y
L$24:                                    ;
     mov   ebx,dword ptr [esi+ebx]       ; y=isp32[y]
L$25:                                   ; }
    mov   dword ptr [edi],ebx           ; xrpnt[0]=y
    add   edi,4                         ; xrpnt++
    dec   dword ptr -8H[ebp]           ; lp--
   jne   L$17                          ; }while(lp)
				      ;	} // endif hlinbits
L$27:                                ; }  // endif lp
 inc   dword ptr -1cH[ebp]           ; i++
 cmp   dword ptr -1cH[ebp],3         ;
jl        L$2                       ;}while(i<3)
				    ;--------------------------------------
mov   eax,dword ptr -30H[ebp]       ;
add   eax,2288                      ;
sub   eax,edi                       ; i=&xr[572]-xrpnt
js   L$62                           ; if(i<0) goto end
;add   eax,16                        ; i+=4   (i=&xr[576]-xrpnt)(at jnz)
sar   eax,4                         ; i>>=2(+2)
mov   ebx,dword ptr -2cH[ebp]       ; ebx=gr_info
;cmp   eax,dword ptr 4cH[ebx]        ;
;jle  L$28                           ; if(gr_info->regions[3]<i)
;mov   eax,dword ptr 4cH[ebx]        ;  i=gr_info->regions[3]
;L$28:                               ;
mov   dword ptr -1cH[ebp],eax       ; i=i
				    ;
mov   eax,dword ptr 58H[ebx]        ;
shl   eax,2                         ;
add   eax,offset FLAT:_htc2         ; h=htc2+gr_info->count1table_select
mov   eax,dword ptr [eax]           ;
mov   dword ptr -0cH[ebp],eax       ; htable=h->table
				    ;
mov   eax,dword ptr -38H[ebp]       ;
shl   eax,2                         ;
add   eax,dword ptr 68H[ebx]        ;
fstp  st                            ;
mov   esi,dword ptr [eax]           ; v=gr_info->pow2gain[pow2index]
fld   dword ptr [eax]               ;
fchs                                ; nv=-v
				    ;---------------------------------------
mov   eax,dword ptr -28H[ebp]       ;
cmp   eax,dword ptr -3cH[ebp]       ;
jne   L$45                          ; if(m==me){
L$30:                                ; do{
  cmp   edx,dword ptr -20H[ebp]       ;
  jge   L$62                          ; if(loc_bitindex>=part2end) goto end
  mov   eax,edx                       ;
  mov   ecx,eax                       ; locbits=get3bytes()
  shr   eax,3                         ;
  add   eax,dword ptr -40H[ebp];
  mov   eax,dword ptr [eax]           ;
  bswap eax                           ;
  and   ecx,7                         ;
  shl   eax,cl                        ;
  mov   ecx,dword ptr -0cH[ebp]       ; *val=htable
L$31:                                 ; do{
   movsx ebx,word ptr [ecx]            ; y=*val
   test  ebx,ebx                       ; if(y>=0)
   jge   L$32                          ;  break
   add   ecx,2                         ; val++
   inc   edx                           ; loc_bitindex++
   shl   eax,1                         ; locbits<<=1
   jnc   L$31                          ; if(locbits&0x80000000)
   sub   ecx,ebx                       ;  val-=y
  jmp   L$31                          ; }while(1)
L$32:                                 ;
  xor   ecx,ecx                       ;
  shr   ebx,1                         ;
  jnc   L$34                          ; if(y&1){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$33                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$35                         ;
L$33:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$35                         ;
L$34:                                 ; }else
   mov   dword ptr [edi],ecx          ;  xrpnt[0]=0
L$35:                                 ;
  add   edi,4                         ; xrpnt++
  shr   ebx,1                         ;
  jnc   L$37                          ; if(y&2){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$36                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$38                         ;
L$36:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$38                         ;
L$37:                                 ; }else
   mov   dword ptr [edi],ecx          ;  xrpnt[0]=0
L$38:                                 ;
  add   edi,4                         ; xrpnt++
  shr   ebx,1                         ;
  jnc   L$40                          ; if(y&4){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$39                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$41                         ;
L$39:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$41                         ;
L$40:                                 ; }else
   mov   dword ptr [edi],ecx          ;  xrpnt[0]=0
L$41:                                 ;
  add   edi,4                         ; xrpnt++
  shr   ebx,1                         ;
  jnc   L$43                          ; if(y&8){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$42                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$44                         ;
L$42:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$44                         ;
L$43:                                 ; }else
   mov   dword ptr [edi],ecx          ;  xrpnt[0]=0
L$44:                                 ;
  add   edi,4                         ;  xrpnt++
  dec   dword ptr -1cH[ebp]           ; i--
 jns   L$30                          ; }while(i>=0)
 jmp   L$62                         ; }else{   // (m==me)
L$45:                                ; do{
  cmp   edx,dword ptr -20H[ebp]       ;
  jge   L$62                          ; if(loc_bitindex>=part2end) goto end
  dec   dword ptr -4H[ebp]            ; mc--
  jne   L$46                          ; if(!mc){
   mov   ecx,dword ptr -28H[ebp]       ;
   mov   esi,dword ptr [ecx]           ;
   mov   dword ptr -4H[ebp],esi        ; mc=m[0]
   add   ecx,4                         ;
   mov   dword ptr -28H[ebp],ecx       ; m++
   mov   esi,dword ptr -34H[ebp]       ;
   movzx ecx,byte ptr [esi]            ; pow2index=*scf
   inc   esi                           ;
   mov   dword ptr -34H[ebp],esi       ; scf++
   shl   ecx,2                         ;
   mov   esi,dword ptr -2cH[ebp]       ;
   add   ecx,dword ptr 68H[esi]        ;
   fstp  st                            ;
   mov   esi,dword ptr [ecx]           ; v=gr_info->pow2gain[pow2index]
   fld   dword ptr [ecx]               ;
   fchs                                ; nv=-v
L$46:                                 ; }
  mov   eax,edx                       ;
  mov   ecx,eax                       ; locbits=get3bytes()
  shr   eax,3                         ;
  add   eax,dword ptr -40H[ebp];
  mov   eax,dword ptr [eax]           ;
  bswap eax                           ;
  and   ecx,7                         ;
  shl   eax,cl                        ;
  mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$47:                                 ; do{
   movsx ebx,word ptr [ecx]            ; y=*val
   test  ebx,ebx                       ; if(y>=0)
   jge   L$48                          ;  break
   add   ecx,2                         ; val++
   inc   edx                           ; loc_bitindex++
   shl   eax,1                         ; locbits<<=1
   jnc   L$47                          ; if(locbits&0x80000000)
   sub   ecx,ebx                       ;  val-=y
   jmp   L$47                          ;
L$48:                                 ; }while(1)
  shr   ebx,1                         ;
  jnc   L$50                          ; if(y&1){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$49                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$51                         ;
L$49:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$51                         ;
L$50:                                 ; }else
   mov   dword ptr [edi],0            ;  xrpnt[0]=0
L$51:                                 ;
  add   edi,4                         ; xrpnt++
  shr   ebx,1                         ;
  jnc   L$53                          ; if(y&2){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$52                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$54                         ;
L$52:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$54                         ;
L$53:                                 ; }else
   mov   dword ptr [edi],0            ;  xrpnt[0]=0
L$54:                                 ;
  add   edi,4                         ; xrpnt++
  dec   dword ptr -4H[ebp]            ; mc--
  jne   L$55                          ; if(!mc){
   mov   ecx,dword ptr -28H[ebp]       ;
   mov   esi,dword ptr [ecx]           ;
   mov   dword ptr -4H[ebp],esi        ; mc=m[0]
   add   ecx,4                         ;
   mov   dword ptr -28H[ebp],ecx       ; m++
   mov   esi,dword ptr -34H[ebp]       ;
   movzx ecx,byte ptr [esi]            ; pow2index=*scf
   inc   esi                           ;
   mov   dword ptr -34H[ebp],esi       ; scf++
   shl   ecx,2                         ;
   mov   esi,dword ptr -2cH[ebp]       ;
   add   ecx,dword ptr 68H[esi]        ;
   fstp  st                            ;
   mov   esi,dword ptr [ecx]           ; v=gr_info->pow2gain[pow2index]
   fld   dword ptr [ecx]               ;
   fchs                                ; nv=-v
L$55:                                 ; }
  shr   ebx,1                         ;
  jnc   L$57                          ; if(y&4){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$56                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$58                         ;
L$56:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$58                         ;
L$57:                                 ; }eles
   mov   dword ptr [edi],0            ;  xrpnt[0]=0
L$58:                                 ;
  add   edi,4                         ; xrpnt++
  shr   ebx,1                         ;
  jnc   L$60                          ; if(y&8){
   mov   dword ptr -24H[ebp],edi      ;  lastxr=xrpnt
   inc   edx                          ;  loc_bitindex++
   shl   eax,1                        ;  locbits<<=1
   jnc   L$59                         ;  if(locbits&0x80000000)
    fst   dword ptr [edi]             ;   xrpnt[0]=nv
   jmp   L$61                         ;
L$59:                                 ;  else
    mov   dword ptr [edi],esi         ;   xrpnt[0]=v
   jmp   L$61                         ;
L$60:                                 ; }else
   mov   dword ptr [edi],0            ;  xrpnt[0]=0
L$61:                                 ;
  add   edi,4                         ; xrpnt++
  dec   dword ptr -1cH[ebp]           ; i--
jns   L$45                           ; }while(i>=0)
;jnz   L$45                           ; }while(i)
L$62:                               ;} //endif (m==me)
fstp      st                        ;
mov   edx,edi                       ;
sub   edx,dword ptr -30H[ebp]       ; i=xrpnt-xr
cmp   edx,2300                      ;
jg L$64                             ; if(i<=575){
 sar   edx,2                        ;
 mov   ecx,00000240H                ;
 sub   ecx,edx                      ;
 cld                                ;
 xor   eax,eax                      ;
 rep   stosd                        ;  my4memreset(xrpnt,576-i)
L$64:
mov   ebx,dword ptr -2cH[ebp]       ; // ebx=gr_info
mov   eax,dword ptr -24H[ebp]       ;
sub   eax,dword ptr -30H[ebp]       ;
sar   eax,2                         ; i=(lastxr-xr)
mov   dword ptr 38H[ebx],eax        ; gr_info->maxbandl=i
add   eax,27                        ; i+=SSLIMIT+(SSLIMIT/2) // +(SSLIMIT/2) = maxb bugfix for Antialias (rounding)
mov   ecx,18                        ;
xor   edx,edx                       ;
idiv  ecx                           ; sb=i/18
cmp   eax,32                        ;
jle   L$66                          ; if(sb>32)
 mov   eax,32                       ;  sb=32
L$66:                               ;
mov   dword ptr 3cH[ebx],eax        ; gr_info->maxb=i

mov esp,ebp
pop ebp
pop edi
pop esi
ret
;-------------------------------------------------------------------------
mpxdec_l3deq_dequantize_ms_:
push esi
push edi
push ebp
mov  ebp,esp
sub  esp,3cH

;  -4H[ebp] : mc
;  -8H[ebp] : lp
; -0cH[ebp] : htable
; -10H[ebp] : locbits
; -14H[ebp] : sub32hlin
; -18H[ebp] : hlinbits
; -1cH[ebp] : i
; -20H[ebp] : part2end
; -24H[ebp] : lastxr
; -28H[ebp] : m
; -2cH[ebp] : gr_info
; -30H[ebp] : xr
; -34H[ebp] : scf
; -38H[ebp] : pow2index
; -3cH[ebp] : loc_wordpointer

mov   dword ptr -30H[ebp],eax       ; xr=xr_p
mov   dword ptr -24H[ebp],eax       ; lastxr=xr_p
mov   edi,eax                       ; xrpnt=xr_p
mov   dword ptr -34H[ebp],edx       ; scf=scf_p
mov   dword ptr -2cH[ebp],ebx       ; gr_info=gr_info_p
mov   edx,dword ptr FLAT:_mpxdec_bitindex  ; loc_bitindex=bitindex
mov   eax,dword ptr FLAT:_mpxdec_wordpointer;
mov   dword ptr -3cH[ebp],eax       ; loc_wordpointer=wordpointer
mov   eax,dword ptr 4H[ebx]         ;
add   eax,ecx                       ;
mov   dword ptr -20H[ebp],eax       ; part2end=part2begin+gr_info->part2_3_lenght
mov   dword ptr FLAT:_mpxdec_bitindex,eax  ; bitindex=part2end
mov   eax,dword ptr 70H[ebx]        ;
mov   dword ptr -28H[ebp],eax       ; m=gr_info->mapp
fldz                                ;
fst   dword ptr -38H[ebp]           ; pow2index=0
fst   dword ptr -1cH[ebp]           ; i=0
mov   dword ptr -4H[ebp],1          ; mc=1
L$68:                               ;
 mov   eax,dword ptr -1cH[ebp]       ;
 shl   eax,2                         ;
 add   eax,dword ptr -2cH[ebp]       ;
 mov   ebx,dword ptr 40H[eax]        ; ebx=gr_info->regions[i]
 test  ebx,ebx                       ;
 jz    L$93                          ; if(lp){
  mov   dword ptr -8H[ebp],ebx       ; lp=ebx
  mov   ebx,dword ptr 14H[eax]        ;
  shl   ebx,3                         ;
  add   ebx,offset FLAT:_ht           ; h=ht+gr_info->table_select[i]
  mov   eax,dword ptr 4H[ebx]         ;
  mov   dword ptr -0cH[ebp],eax       ; htable=h->table
  mov   eax,dword ptr [ebx]           ; hlinbits=h->linbits
  test  eax,eax                       ;
  jz    L$83                          ; if(hlinbits){
   mov   ebx,00000020H                 ;
   sub   ebx,eax                       ;
   mov   dword ptr -14H[ebp],ebx       ; sub32hlin=32-hlinbits
   inc   eax                           ; hlinbits++
   mov   dword ptr -18H[ebp],eax       ; hlinbits=eax
L$69:                                  ; do{
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$70                          ; if(!mc){
     fstp  st                            ;
     mov   eax,dword ptr -28H[ebp]       ;
     mov   ecx,dword ptr [eax]           ;
     mov   dword ptr -4H[ebp],ecx        ; mc=m[0]
     add   eax,4                         ;
     mov   dword ptr -28H[ebp],eax       ; m++
     mov   eax,dword ptr -34H[ebp]       ;
     movzx esi,byte ptr [eax]            ;
     mov   dword ptr -38H[ebp],esi       ; pow2index=scf[0]
     inc   eax                           ;
     mov   dword ptr -34H[ebp],eax       ; scf++
     mov   eax,dword ptr -2cH[ebp]       ; // eax=gr_info
     mov   ecx,dword ptr 68H[eax]        ;
     shl   esi,2                         ;
     fld   dword ptr [ecx+esi]           ; v=gr_info->pow2gain[pow2index]
     shl   esi,5                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
L$70:                                   ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -3cH[ebp];
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$71:                                   ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$72                          ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$71                          ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
    jmp   L$71                          ; }while(1)
L$72:                                   ;
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$76                          ; if(x){
     cmp   cl,0f0H                       ;
     jne   L$73                          ; if(x==15){
      mov   eax,edx                       ;
      mov   ecx,eax                       ; locbits=get3bytes()
      shr   eax,3                         ;
      add   eax,dword ptr -3cH[ebp];      ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   ecx,dword ptr -14H[ebp]       ;
      mov   dword ptr -10H[ebp],eax       ; locbits=eax
      shr   eax,cl                        ; x=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[eax*4]  ; a=ispow[x]*v
      mov   ecx,dword ptr -18H[ebp]       ;
      fmul  st,st(2)                      ;
      mov   eax,dword ptr -10H[ebp]       ; eax=locbits
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$75                          ; if(locbits&0x80000000)
       fchs                               ;  a=-a
     jmp   L$75                           ;
L$73:                                    ; }else{
      shr   ecx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$74                          ; if(locbits&0x80000000)
       neg   ecx                          ;  x=-x
L$74:                                     ;
      fld   dword ptr [esi+ecx]           ; a=isp32[x]
      inc   edx                           ; loc_bitindex++
L$75:                                    ; }
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
L$76:                                   ; }else
    add   edi,4                         ; xrpnt++
    fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$81                          ; if(y){
     cmp   ebx,0000000fH                 ;
     jne   L$78                          ; if(y==15){
      mov   eax,edx                       ;
      mov   ecx,eax                       ; locbits=get3bytes()
      shr   eax,3                         ;
      add   eax,dword ptr -3cH[ebp];
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   ebx,eax                       ;
      mov   ecx,dword ptr -14H[ebp]       ;
      shr   ebx,cl                        ; x=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[ebx*4]  ; a=ispow[x]*v
      mov   ecx,dword ptr -18H[ebp]       ;
      fmul  st,st(2)                      ;
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$80                          ; if(locbits&0x80000000)
       fchs                               ;  a=-a
     jmp   L$80                           ;
L$78:                                    ; }else{
      shl   ebx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$79                          ; if(locbits&0x80000000)
       neg   ebx                          ;  y=-y
L$79:                                     ;
      fld   dword ptr [esi+ebx]           ; a=isp32[y]
      inc   edx                           ; loc_bitindex++
L$80:                                    ; }
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
L$81:                                   ; }else
    add   edi,4                         ; xrpnt++
    fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
    dec   dword ptr -8H[ebp]           ; lp--
   jnz   L$69                          ; }while(lp)
  jmp   near ptr L$93                 ; } else { //hlinbits
L$83:                                  ; do{
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$84                          ; if(!mc){
     mov   eax,dword ptr -28H[ebp]       ;
     mov   ecx,dword ptr [eax]           ;
     mov   dword ptr -4H[ebp],ecx        ; mc=m[0]
     add   eax,4                         ;
     mov   dword ptr -28H[ebp],eax       ; m++
     mov   eax,dword ptr -34H[ebp]       ;
     movzx esi,byte ptr [eax]            ;
     mov   dword ptr -38H[ebp],esi       ; pow2index=*scf
     inc   eax                           ;
     mov   dword ptr -34H[ebp],eax       ; scf++
     shl   esi,7                         ;
     mov   eax,dword ptr -2cH[ebp]       ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->isp32_base[pow2index<<5]
L$84:                                   ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -3cH[ebp];
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$85:                                   ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$86                          ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$85                          ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$85                          ;
L$86:                                   ; }while(1)
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    jz    L$88                          ; if(x){
     shr   ecx,2                         ;
     shl   eax,1                         ; locbits<<=1
     jnc   L$87                          ; if(locbits&0x80000000)
      neg   ecx                          ;  x=-x
L$87:                                    ;
     fld   dword ptr [esi+ecx]           ; a=isp32[x]
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
     inc   edx                           ; loc_bitindex++
L$88:                                   ; }else
    add   edi,4                         ; xrpnt++
    fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$91                          ; if(y){
     shl   ebx,2                         ;
     shl   eax,1                         ; locbits<<=1
     jnc   L$90                          ; if(locbits&0x80000000)
      neg   ebx                          ;  y=-y
L$90:                                    ;
     fld   dword ptr [esi+ebx]           ; a=isp32[y]
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
     inc   edx                           ; loc_bitindex++
L$91:                                   ; }else
    add   edi,4                         ; xrpnt++
    fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
    dec   dword ptr -8H[ebp]           ; lp--
   jnz   L$83                          ; }while(lp)
L$93:                                 ; }//endif lp & hlinbits
 inc   dword ptr -1cH[ebp]           ; i++
 cmp   dword ptr -1cH[ebp],3         ;
jl    L$68
				    ;-----------------------------
mov   eax,dword ptr -30H[ebp]       ;
add   eax,2288                      ;
sub   eax,edi                       ; i=&xr[572]-xrpnt
js   L$113                          ; if(i<0) goto end
;add   eax,16                        ; i+=4   (i=&xr[576]-xrpnt)(at jnz)
sar   eax,4                         ; i>>=2(+2)
mov   ebx,dword ptr -2cH[ebp]       ; ebx=gr_info
;cmp   eax,dword ptr 4cH[ebx]        ;
;jle  L$94                           ; if(gr_info->regions[3]<i)
;mov   eax,dword ptr 4cH[ebx]        ;  i=gr_info->regions[3]
;L$94:                               ;
mov   dword ptr -1cH[ebp],eax       ; i=i
				    ;
mov   eax,dword ptr 58H[ebx]        ;
shl   eax,2                         ;
add   eax,offset FLAT:_htc2         ; h=htc2+gr_info->count1table_select
mov   eax,dword ptr [eax]           ;
mov   dword ptr -0cH[ebp],eax       ; htable=h->table
				    ;
mov   eax,dword ptr -38H[ebp]       ;
shl   eax,2                         ;
fstp  st                            ;
add   eax,dword ptr 68H[ebx]        ;
fld   dword ptr [eax]               ; v=gr_info->pow2gain[pow2index]
mov   esi,dword ptr -4H[ebp]        ; //esi=mc
L$96:                               ;------------------------------
 cmp   edx,dword ptr -20H[ebp]       ; if(loc_bitindex>=part2end)
 jge   L$113                         ;  goto end
 dec   esi                           ; mc--
 jne   L$97                          ; if(!mc){
  fstp st                             ;
  mov   esi,dword ptr -34H[ebp]       ;
  movzx ecx,byte ptr [esi]            ; pow2index=*scf
  inc   esi                           ;
  mov   dword ptr -34H[ebp],esi       ; scf++
  shl   ecx,2                         ;
  mov   esi,dword ptr -2cH[ebp]       ;
  add   ecx,dword ptr 68H[esi]        ;
  fld   dword ptr [ecx]               ; v=gr_info->pow2gain[pow2index]
  mov   ecx,dword ptr -28H[ebp]       ;
  mov   esi,dword ptr [ecx]           ; mc=m[0]
  add   ecx,4                         ;
  mov   dword ptr -28H[ebp],ecx       ; m++
L$97:                                ; }
 mov   eax,edx                       ;
 mov   ecx,eax                       ; locbits=get3bytes()
 shr   eax,3                         ;
 add   eax,dword ptr -3cH[ebp];
 mov   eax,dword ptr [eax]           ;
 bswap eax                           ;
 and   ecx,7                         ;
 shl   eax,cl                        ;
 mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$98:                                ; do{
  movsx ebx,word ptr [ecx]            ; y=*val
  test  ebx,ebx                       ; if(y>=0)
  jge   L$99                          ;  break
  add   ecx,2                         ; val++
  inc   edx                           ; loc_bitindex++
  shl   eax,1                         ; locbits<<=1
  jnc   L$98                          ; if(locbits&0x80000000)
  sub   ecx,ebx                       ;  val-=y
  jmp   L$98                          ;
L$99:                                ; }while(1)
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$101                         ; if(y&1){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$100                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$100:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$101:                               ; }else
 add   edi,4                         ; xrpnt++
 fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$104                         ; if(y&2){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$103                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$103:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$104:                               ; }else
 add   edi,4                         ; xrpnt++
 fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
 dec   esi                           ; mc--
 jnz   L$106                         ; if(!mc){
  fstp st                             ;
  mov   esi,dword ptr -34H[ebp]       ;
  movzx ecx,byte ptr [esi]            ; pow2index=*scf
  inc   esi                           ;
  mov   dword ptr -34H[ebp],esi       ; scf++
  shl   ecx,2                         ;
  mov   esi,dword ptr -2cH[ebp]       ;
  add   ecx,dword ptr 68H[esi]        ;
  fld   dword ptr [ecx]               ; v=gr_info->pow2gain[pow2index]
  mov   ecx,dword ptr -28H[ebp]       ;
  mov   esi,dword ptr [ecx]           ; mc=m[0]
  add   ecx,4                         ;
  mov   dword ptr -28H[ebp],ecx       ; m++
L$106:                               ; }
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$108                         ; if(y&4){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$107                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$107:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$108:                               ; }else
 add   edi,4                         ; xrpnt++
 fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$111                         ; if(y&8){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -24H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$110                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$110:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$111:                               ; }else
 add   edi,4                         ; xrpnt++
 fstp  dword ptr 8fcH[edi]           ; xrpnt[575]=x0
 dec   dword ptr -1cH[ebp]           ; i--
jns   L$96                          ; }while(i>=0)
;jnz   L$96                          ; }while(i)
L$113:                              ;
fstp      st                        ; // pop v
mov   eax,edi                       ;
sub   eax,dword ptr -30H[ebp]       ; i=xrpnt-xr
cmp   eax,2300                      ;
jg    L$114                         ; if(i<=575){
 sar   eax,2
 mov   ecx,576                       ;
 sub   ecx,eax                       ;
 mov   esi,edi                       ;
 add   edi,2304                      ;
 cld                                 ;
 rep   movsd                         ; my4memcpy(xrpnt+576,xrpnt,576-i);
L$114:                              ; }
mov   ebx,dword ptr -2cH[ebp]       ; //ebx=gr_info
mov   eax,dword ptr -24H[ebp]       ;
sub   eax,dword ptr -30H[ebp]       ;
sar   eax,2                         ; i=lastxr-xr
mov   dword ptr 38H[ebx],eax        ; gr_info->maxbandl=i
add   eax,27                        ; i+=SSLIMIT+(SSLIMIT/2) // maxb bugfix (rounding)
mov   ecx,18                        ;
xor   edx,edx                       ;
idiv  ecx                           ; sb=i/18
cmp   eax,32                        ;
jle   L$115                         ; if(sb>32)
 mov   eax,32                       ;  sb=32
L$115:                              ;
mov   dword ptr 3cH[ebx],eax        ; gr_info->maxb=sb

mov  esp,ebp
pop  ebp
pop  edi
pop  esi
ret

;-----------------------------------------------------------------------

mpxdec_l3deq_dequantize_bt2_:
push  esi
push  edi
push  ebp
mov   ebp,esp
sub   esp,00000048H

;  -4H[ebp] : mc
;  -8H[ebp] : locbits
; -0cH[ebp] : htable
; -10H[ebp] : step
; -14H[ebp] : lp
; -18H[ebp] : sub32hlin
; -1cH[ebp] : hlinbits
; -20H[ebp] : i
; -24H[ebp] : part2end
; -28H[ebp] : lastxr
; -2cH[ebp] : maxxr
; -30H[ebp] : m
; -34H[ebp] : gr_info
; -38H[ebp] : xr
; -3cH[ebp] : scf
; -40H[ebp] : pow2index
; -44H[ebp] : me
; -48H[ebp] : loc_wordpointer

mov   dword ptr -38H[ebp],eax       ; xr=xr_p
mov   edi,eax                       ; xrpnt=xr_p
mov   dword ptr -28H[ebp],eax       ; lastxr=xr_p
mov   dword ptr -2cH[ebp],eax       ; maxxr=xr_p
mov   dword ptr -3cH[ebp],edx       ; scf=scf_p
mov   dword ptr -34H[ebp],ebx       ; gr_info=gr_info_p
mov   edx,dword ptr FLAT:_mpxdec_bitindex  ; loc_bitindex=bitindex
mov   eax,dword ptr FLAT:_mpxdec_wordpointer;
mov   dword ptr -48H[ebp],eax       ; loc_wordpointer=wordpointer
mov   eax,dword ptr 4H[ebx]         ;
add   eax,ecx                       ;
mov   dword ptr -24H[ebp],eax       ; part2end=gr_info_p->part2_3_length+part2begin;
mov   dword ptr FLAT:_mpxdec_bitindex,eax  ; bitindex=part2end
mov   eax,dword ptr 70H[ebx]        ;
mov   dword ptr -30H[ebp],eax       ; m=gr_info->mapp
cmp   dword ptr 10H[ebx],0          ; if(gr_ino->mixed_block_flag)
je    L$116                         ;
 add   eax,00000260H                ;  me=m+152
jmp   L$117                         ; else
L$116:                              ;  me=m+156
 add   eax,00000270H                ;
L$117:                              ;
mov   dword ptr -44H[ebp],eax       ;
fldz                                ;
fst   dword ptr -20H[ebp]           ; i=0
fst   dword ptr -40H[ebp]           ; pow2index=0
mov   dword ptr -4H[ebp],1          ; mc=1
L$118:                              ; do{
 mov   eax,dword ptr -20H[ebp]       ;
 shl   eax,2                         ;
 add   eax,dword ptr -34H[ebp]       ;
 mov   ebx,dword ptr 40H[eax]        ;
 test  ebx,ebx                       ; lp=gr_info->regions[i]
 jz    L$147                         ; if(lp){
  mov   dword ptr -14H[ebp],ebx       ;
  mov   ebx,dword ptr 14H[eax]        ;
  shl   ebx,3                         ;
  add   ebx,offset FLAT:_ht           ; h=ht+gr_info->table_select[i]
  mov   eax,dword ptr 4H[ebx]         ;
  mov   dword ptr -0cH[ebp],eax       ; htable=h->table
  mov   eax,dword ptr [ebx]           ;
  test  eax,eax                       ; hlinbits=h->linbits
  jz    L$135                         ; if(hlinbits){
   mov   ebx,00000020H                 ;
   sub   ebx,eax                       ;
   mov   dword ptr -18H[ebp],ebx       ; sub32hlin=32-hlinbits
   inc   eax                           ;
   mov   dword ptr -1cH[ebp],eax       ; hlinbits=hlinbits+1
L$119:                                 ; do{
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$122                         ; if(mc){
     fstp  st                            ;
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
     mov   edi,dword ptr -28H[ebp]       ;
     cmp   edi,dword ptr -2cH[ebp]       ;
     jle   L$1193                        ; if(lastxr>maxxr)
      mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$1193:                                  ;
     mov   esi,dword ptr [ecx]           ;
     mov   dword ptr -4H[ebp],esi        ; mc=m[0]
     mov   edi,dword ptr -38H[ebp]       ;
     add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$120                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$121                         ;
L$120:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$121:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,2                         ;
     mov   ecx,dword ptr 68H[eax]        ;
     fld   dword ptr [esi+ecx]           ; v=gr_info->pow2gain[pow2index]
     shl   esi,5                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ; m+=4
L$122:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp];
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$123:                                  ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$124                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$123                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$123                         ;
L$124:                                  ; }while(1)
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$128                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   cl,0f0H                       ;
     jne   L$126                         ; if(x==15){
      mov   eax,edx                       ;
      mov   ecx,eax                       ; locbits=get3bytes()
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp];      ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   dword ptr -8H[ebp],eax        ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   eax,cl                        ; x=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[eax*4]  ; a=ispow[x]*v
      mov   ecx,dword ptr -1cH[ebp]       ; ecx=hlinbits
      fmul  st,st(1)                      ;
      mov   eax,dword ptr -8H[ebp]        ; eax=locbits
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits  // hlinbits+1
      jnc   L$125                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
L$125:                                    ;
      fstp  dword ptr [edi]               ; xrpnt[0]=a
     jmp   L$129                         ; }else{
L$126:                                    ;
      shr   ecx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$127                         ; if(locbits&0x80000000)
       neg   ecx                          ;  x=-x
L$127:                                    ;
      inc   edx                           ; loc_bitindex++
      mov   ecx,dword ptr [esi+ecx]       ; x=isp32[x]
L$128:                                  ;}else
     mov   dword ptr [edi],ecx           ; xrpnt[0]=x
L$129:
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$133                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   ebx,0000000fH                 ;
     jne   L$131                         ; if(y==15){
      mov   eax,edx                       ;
      mov   ecx,eax                       ; locbits=get3bytes()
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp];
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   ebx,eax                       ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   ebx,cl                        ; y=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[ebx*4]  ; a=ispow[x]*v
      mov   ecx,dword ptr -1cH[ebp]       ;
      fmul  st,st(1)                      ;
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$130                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
L$130:                                    ;
      fstp  dword ptr [edi]               ; xrpnt[0]=a
     jmp   L$134                         ; }else{
L$131:                                    ;
      shl   ebx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$132                         ; if(locbits&0x80000000)
       neg   ebx                          ;  y=-y
L$132:                                    ;
      inc   edx                           ; loc_bitindex++
      mov   ebx,dword ptr [esi+ebx]       ; y=isp32[y]
L$133:                                  ; }else
     mov   dword ptr [edi],ebx           ; xrpnt[0]=y
L$134:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    dec   dword ptr -14H[ebp]           ; lp--
   jnz   L$119                         ; }while(lp)
   jmp   near ptr L$147               ; }else{
L$135:                                 ; do{
    dec   dword ptr -4H[ebp]           ; mc--
    jne   L$138                         ; if(!mc){
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
     mov   edi,dword ptr -28H[ebp]       ;
     cmp   edi,dword ptr -2cH[ebp]       ;
     jle   L$1353                        ; if(lastxr>maxxr)
      mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$1353:                                  ;
     mov   esi,dword ptr [ecx]           ;
     mov   dword ptr -4H[ebp],esi        ; mc=m[0]
     mov   edi,dword ptr -38H[ebp]       ;
     add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$136                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$137                         ;
L$136:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$137:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,7                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ; m+=4
L$138:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp];
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$139:                                  ;do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$140                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$139                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$139                         ;
L$140:                                  ; }while(1)
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$142                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shr   ecx,2                         ;
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$141                         ; if(locbits&0x80000000)
      neg   ecx                          ;  x=-x
L$141:                                   ;
     mov   ecx,dword ptr [esi+ecx]       ; x=isp32[x]
L$142:                                  ; }else
    mov   dword ptr [edi],ecx           ;  xrpnt[0]=x
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$145                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shl   ebx,2                         ;
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$144                         ; if(locbits&0x80000000)
      neg   ebx                          ;  y=-y
L$144:                                   ;
     mov   ebx,dword ptr [esi+ebx]       ; y=isp32[y]
L$145:                                  ; }else
    mov   dword ptr [edi],ebx            ; xrpnt[0]=y
    add   edi,dword ptr -10H[ebp]       ;xrpnt+=step
    dec   dword ptr -14H[ebp]          ;lp--
   jnz   L$135                         ;}while(lp)
L$147:                                ;} //endif hlinbits, endif lp
 inc   dword ptr -20H[ebp]           ; i++
 cmp   dword ptr -20H[ebp],2         ;
jl    L$118                         ; }while(i<2)
mov   ebx,dword ptr -34H[ebp]       ;
mov   eax,dword ptr 58H[ebx]        ;
shl   eax,2                         ;
add   eax,offset FLAT:_htc2         ; h=htc2+gr_info->count1table_select
mov   eax,dword ptr [eax]           ;
mov   dword ptr -0cH[ebp],eax       ; htable=h->table
mov   eax,dword ptr 4cH[ebx]        ; i=gr_info->regions[3]
test  eax,eax                       ;
jz    L$169                         ; if(!i) goto end
mov   dword ptr -20H[ebp],eax       ;
mov   eax,dword ptr -40H[ebp]       ;
shl   eax,2                         ;
add   eax,dword ptr 68H[ebx]        ;
fstp  st                            ;
mov   esi,dword ptr [eax]           ; v=gr_info->pow2gain[pow2index]
fld   dword ptr [eax]               ;
fchs                                ; nv=-v
L$148:
 cmp   edx,dword ptr -24H[ebp]       ;
 jge   L$169                         ;if(loc_bitindex>=part2end) goto end
 dec   dword ptr -4H[ebp]            ;mc--
 jne   L$151                         ;if(!mc){
  fstp  st                            ;
  mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
  mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
  mov   edi,dword ptr -28H[ebp]       ;
  cmp   edi,dword ptr -2cH[ebp]       ;
  jle   L$1483                        ; if(lastxr>maxxr)
   mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$1483:                               ;
  mov   esi,dword ptr [ecx]           ;
  mov   dword ptr -4H[ebp],esi        ; mc=m[0]
  mov   edi,dword ptr -38H[ebp]       ;
  add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
  mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
  mov   esi,dword ptr -3cH[ebp]       ;
  movzx esi,byte ptr [esi]            ; esi=*scf
  cmp   ecx,0000000cH                 ;
  jne   L$149                         ; if(lwin==3)
   mov   dword ptr -10H[ebp],4        ;  step=1
  jmp   L$150                         ; else{
L$149:                                ;
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   add   esi,dword ptr 5cH[eax+ecx]   ;  pow2index+=gr_info->full_gain[lwin]
L$150:                                ; }
  shl   esi,2                         ;
  add   esi,dword ptr 68H[eax]        ;
  fld   dword ptr [esi]               ; nv=gr_info->pow2gain[pow2index]
  fchs                                ;
  mov   esi,dword ptr [esi]           ;
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$151:                               ; }
 mov   eax,edx                       ; locbits=get3bytes()
 mov   ecx,eax                       ;
 shr   eax,3                         ;
 add   eax,dword ptr -48H[ebp];
 mov   eax,dword ptr [eax]           ;
 bswap eax                           ;
 and   ecx,7                         ;
 shl   eax,cl                        ;
 mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$152:                               ; do{
  movsx ebx,word ptr [ecx]            ; y=*val
  test  ebx,ebx                       ; if(y>=0)
  jge   L$153                         ;  break
  add   ecx,2                         ; val++
  inc   edx                           ; loc_bitindex++
  shl   eax,1                         ; locbits<<=1
  jnc   L$152                         ; if(locbits&0x80000000)
  sub   ecx,ebx                       ;  val-=y
  jmp   L$152                         ;
L$153:                               ; }while(1)
 shr   ebx,1                         ;
 jnc   L$155                         ; if(y&1){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$154                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$156                        ;
L$154:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$156                        ;
L$155:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$156:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 shr   ebx,1                         ;
 jnc   L$158                         ; if(y&2){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$157                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$159                        ;
L$157:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$159                        ;
L$158:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$159:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -4H[ebp]            ; mc--
 jne   L$162                         ; if(!mc){
  fstp  st                            ;
  mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
  mov   edi,dword ptr -28H[ebp]       ;
  cmp   edi,dword ptr -2cH[ebp]       ;
  jle   L$1593                        ; if(lastxr>maxxr)
   mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$1593:                               ;
  mov   edi,dword ptr [ecx]           ;
  mov   dword ptr -4H[ebp],edi        ; mc=m[0]
  mov   edi,dword ptr 4H[ecx]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  mov   esi,dword ptr 8H[ecx]         ; lwin=m[2]
  mov   ecx,dword ptr -34H[ebp]       ; //ecx=gr_info
  cmp   esi,0000000cH                 ;
  jne   L$160                         ; if(lwin==3){
   mov   dword ptr -10H[ebp],4        ;  step=1
   xor   esi,esi                      ;  pow2index=0
  jmp   L$161                         ;
L$160:                                ; }else{
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   mov   esi,dword ptr 5cH[ecx+esi]   ;  pow2index=gr_info->full_gain[lwin]
   shl   esi,2                        ;
L$161:                                ; }
  add   esi,dword ptr 68H[ecx]        ; pow2index+=gr_info->pow2gain
  mov   ecx,dword ptr -3cH[ebp]       ;
  movzx ecx,byte ptr [ecx]            ;
  shl   ecx,2                         ;
  add   esi,ecx                       ; pow2index+=scf
  fld   dword ptr [esi]               ; nv=*pow2index
  mov   esi,dword ptr [esi]           ; v=*pow2index
  fchs                                ;
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$162:                               ;
 shr   ebx,1                         ;
 jnc   L$164                         ; if(y&4){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$163                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$165                        ;
L$163:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$165                        ;
L$164:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$165:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 shr   ebx,1                         ;
 jnc   L$167                         ; if(y&8){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$166                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$168                        ;
L$166:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$168                        ;
L$167:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$168:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -20H[ebp]          ; i--
jnz   L$148                         ; }while(i)
L$169:                              ;
fstp  st                            ;
xor   eax,eax                       ; eax=0
mov   ebx,dword ptr -44H[ebp]       ; ebx=me
mov   ecx,dword ptr -10H[ebp]       ; ecx=step
mov   edx,dword ptr -4H[ebp]        ; edx=mc
mov   esi,dword ptr -30H[ebp]       ; esi=m
L$170:                              ; do{
 dec   edx                           ; mc--
 jne   L$173                         ; if(!mc){
  cmp   esi,ebx                       ; if(m>=me)
  jae   L$174                         ;  break
  mov   edx,dword ptr [esi]           ; mc=m[0]
  mov   edi,dword ptr 4H[esi]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  cmp   dword ptr 8H[esi],0000000cH   ;
  jne   L$171                         ; if(lwin==3)
   mov   ecx,4                        ;  step=1
  jmp   L$172                         ;
L$171:                                ; else
   mov   ecx,0000000cH                ;  step=3
L$172:                                ;
  add   esi,00000010H                 ; m+=4
L$173:                               ; }
 mov   dword ptr [edi],eax           ; xrpnt[0]=0
 add   edi,ecx                       ; xrpnt+=step
 mov   dword ptr [edi],eax           ; xrpnt[0]=0
 add   edi,ecx                       ; xrpnt+=step
jmp   L$170                         ; }while(1)
L$174:

mov   eax,dword ptr -2cH[ebp]       ; eax=maxxr
mov   edi,dword ptr -28H[ebp]       ; edi=lastxr
cmp   edi,eax                       ;
jle   L$1741                        ; if(lastxr>maxxr)
 mov eax,edi                        ;  maxxr=lastxr
L$1741:                             ;
sub  eax,dword ptr -38H[ebp]        ; i=maxxr-xr
mov  ecx,72                         ;
xor  edx,edx                        ;
idiv ecx                            ; sb=i/18
inc  eax                            ; sb++
cmp  eax,32                         ;
jle  L$1742                         ; if(sb>32)
 mov  eax,32                        ;  sb=32
L$1742:                             ;
mov  ebx,dword ptr -34H[ebp]        ;
mov  dword ptr 3cH[ebx],eax         ; gr_info->maxb=sb

mov  esp,ebp
pop  ebp
pop  edi
pop  esi
ret
;--------------------------------------------------------------------------
mpxdec_l3deq_dequantize_ms_bt2_:
push esi
push edi
push ebp
mov  ebp,esp
sub  esp,00000048H

;  -4H[ebp] : mc
;  -8H[ebp] : locbits
; -0cH[ebp] : htable
; -10H[ebp] : step
; -14H[ebp] : lp
; -18H[ebp] : sub32hlin
; -1cH[ebp] : hlinbits
; -20H[ebp] : i
; -24H[ebp] : part2end
; -28H[ebp] : lastxr
; -2cH[ebp] : maxxr
; -30H[ebp] : m
; -34H[ebp] : gr_info
; -38H[ebp] : xr
; -3cH[ebp] : scf
; -40H[ebp] : pow2index
; -44H[ebp] : me
; -48H[ebp] : loc_wordpointer

mov   dword ptr -38H[ebp],eax       ; xr=xr_p
mov   edi,eax                       ; xrpnt=xr_p
mov   dword ptr -28H[ebp],eax       ; lastxr=xr_p
mov   dword ptr -2cH[ebp],eax       ; maxxr=xr_p
mov   dword ptr -3cH[ebp],edx       ; scf=scf_p
mov   dword ptr -34H[ebp],ebx       ; gr_info=gr_info_p
mov   edx,dword ptr FLAT:_mpxdec_bitindex  ;
mov   eax,dword ptr FLAT:_mpxdec_wordpointer;
mov   dword ptr -48H[ebp],eax       ; loc_wordpointer=wordpointer
mov   eax,dword ptr 4H[ebx]         ;
add   eax,ecx                       ;
mov   dword ptr -24H[ebp],eax       ; part2end=gr_info_p->part2_3_length+part2begin;
mov   dword ptr FLAT:_mpxdec_bitindex,eax  ; bitindex=part2end
mov   eax,dword ptr 70H[ebx]        ;
mov   dword ptr -30H[ebp],eax       ; m=gr_info_p->mapp
cmp   dword ptr 10H[ebx],0          ;
je    L$175                         ; if(gr_info_p->mixed_block_flag)
 add   eax,00000260H                ;  eax=m+152
jmp   L$176                         ;
L$175:                              ; else
 add   eax,00000270H                ;  eax=m+156
L$176:                              ;
mov   dword ptr -44H[ebp],eax       ; me=eax
fldz                                ;
fst   dword ptr -20H[ebp]           ; i=0
fst   dword ptr -40H[ebp]           ; pow2index=0
mov   dword ptr -4H[ebp],1          ; mc=1
L$177:                              ; do{ // i
 mov   eax,dword ptr -20H[ebp]       ;
 shl   eax,2                         ;
 add   eax,dword ptr -34H[ebp]       ;
 mov   ebx,dword ptr 40H[eax]        ; lp=gr_info->regions[i]
 test  ebx,ebx                       ;
 jz    L$206                         ; if(lp){
  mov   dword ptr -14H[ebp],ebx       ; //lp=ebx
  mov   ebx,dword ptr 14H[eax]        ;
  shl   ebx,3                         ;
  add   ebx,offset FLAT:_ht           ; h=ht+gr_info->table_select[i]
  mov   eax,dword ptr 4H[ebx]         ;
  mov   dword ptr -0cH[ebp],eax       ; htable=h->table
  mov   eax,dword ptr [ebx]           ; hlinbits=h->linbits
  test  eax,eax                       ;
  jz    L$194                         ; if(hlinbits){
   mov   ebx,00000020H                 ;
   sub   ebx,eax                       ;
   mov   dword ptr -18H[ebp],ebx       ; sub32hlin=32-hlinbits
   inc   eax                           ;
   mov   dword ptr -1cH[ebp],eax       ; hlinbits=hlinbits+1
L$178:                                 ; do{  //lp
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$181                         ; if(!mc){
     fstp  st                            ;
     mov   edi,dword ptr -28H[ebp]       ;
     cmp   edi,dword ptr -2cH[ebp]       ;
     jle   L$1782                        ; if(lastxr>maxxr)
      mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$1782:                                  ;
     mov   esi,dword ptr -30H[ebp]       ; //esi=m
     mov   eax,dword ptr [esi]           ;
     mov   dword ptr -4H[ebp],eax        ; mc=m[0]
     mov   edi,dword ptr 4H[esi]         ;
     add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[esi]         ; lwin=m[2]
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$179                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$180                         ;
L$179:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$180:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,2                         ;
     mov   ecx,dword ptr 68H[eax]        ;
     fld   dword ptr [esi+ecx]           ; v=gr_info->pow2gain[pow2index]
     shl   esi,5                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->isp32base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ; m+=4
L$181:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp];
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$182:                                  ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$183                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$182                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$182                         ;
L$183:                                  ; }while(1)
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$187                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   cl,0f0H                       ;
     jne   L$184                         ; if(x==15){
      mov   eax,edx                       ; locbits=get3bytes()
      mov   ecx,eax                       ;
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp];      ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   dword ptr -8H[ebp],eax        ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   eax,cl                        ; x=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[eax*4]  ;
      mov   ecx,dword ptr -1cH[ebp]       ; ecx=hlinbits //+1
      fmul  st,st(2)                      ; a=ispow[x]*v
      mov   eax,dword ptr -8H[ebp]        ;
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$186                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
     jmp   L$186                          ;
L$184:                                   ; }else{
      shr   ecx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$185                         ; if(locbits&0x80000000)
       neg   ecx                          ;  x=-x
L$185:                                    ;
      fld   dword ptr [esi+ecx]           ; a=isp32[x]
      inc   edx                           ; loc_bitindex++
L$186:                                   ; }
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
L$187:                                  ; }
    fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
L$188:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$192                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   ebx,0000000fH                 ;
     jne   L$189                         ; if(y==15){
      mov   eax,edx                       ;
      mov   ecx,eax                       ; locbits=get3bytes()
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp];
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   ebx,eax                       ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   ebx,cl                        ; y=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[ebx*4]  ;
      mov   ecx,dword ptr -1cH[ebp]       ; ecx=hlinbits //+1
      fmul  st,st(2)                      ; a=ispow[y]*v
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$191                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
     jmp    L$191                         ;
L$189:                                   ; }else{
      shl   ebx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$190                         ; if(locbits&0x80000000)
       neg   ebx                          ;  y=-y
L$190:                                    ;
      fld   dword ptr [esi+ebx]           ; a=isp32[y]
      inc   edx                           ; loc_bitindex++
L$191:                                   ; }
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
L$192:                                  ; }
     fstp  dword ptr 900H[edi]          ; xrpnt[576]=x0
L$193:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    dec   dword ptr -14H[ebp]          ; lp--
   jnz   L$178                         ; }while(lp)
  jmp   near ptr L$206                ; }else{ // hlinbits
L$194:                                 ; do{ //lp
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$197                         ; if(!mc){
     mov   edi,dword ptr -28H[ebp]       ;
     cmp   edi,dword ptr -2cH[ebp]       ;
     jle   L$1942                        ; if(lastxr>maxxr)
      mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$1942:                                  ;
     mov   esi,dword ptr -30H[ebp]       ; //esi=m
     mov   eax,dword ptr [esi]           ;
     mov   dword ptr -4H[ebp],eax        ; mc=m[0]
     mov   edi,dword ptr 4H[esi]         ;
     add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[esi]         ; lwin=m[2]
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$195                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$196                         ;
L$195:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$196:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,7                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ; m+=4
L$197:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp];
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$198:                                  ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$199                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$198                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$198                         ;
L$199:                                  ; }while(1)
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$201                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shr   ecx,2                         ;
     shl   eax,1                         ; locbits<<=1
     jnc   L$200                         ; if(locbits&0x80000000)
      neg   ecx                          ;  x=-x
L$200:                                   ;
     fld   dword ptr [esi+ecx]           ; a=isp32[x]
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
     inc   edx                           ; loc_bitindex++
L$201:                                  ; }
    fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
L$202:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$204                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shl   ebx,2                         ;
     shl   eax,1                         ; locbits<<=1
     jnc   L$203                         ; if(locbits&0x80000000)
      neg   ebx                          ;  y=-y
L$203:                                   ;
     fld   dword ptr [esi+ebx]           ; a=isp32[y]
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
     inc   edx                           ; loc_bitindex++
L$204:                                  ; }
    fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
L$205:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    dec   dword ptr -14H[ebp]          ; lp--
   jnz   L$194                         ; }while(lp)
L$206:                                ; // endif hlinbits & lp
 inc   dword ptr -20H[ebp]           ;
 cmp   dword ptr -20H[ebp],2         ;
jl    L$177                         ; }while(i<2)
mov   ebx,dword ptr -34H[ebp]       ;
mov   eax,dword ptr 58H[ebx]        ;
shl   eax,2                         ;
add   eax,offset FLAT:_htc2         ; h=htc2+gr_info->count1table_select
mov   eax,dword ptr [eax]           ;
mov   dword ptr -0cH[ebp],eax       ; htable=h->table
mov   eax,dword ptr 4cH[ebx]        ; i=gr_info->regions[3]
test  eax,eax                       ; if(!i)
jz    L$228                         ;  goto end
mov   dword ptr -20H[ebp],eax       ;
fstp  st                            ;
mov   eax,dword ptr -40H[ebp]       ;
shl   eax,2                         ;
add   eax,dword ptr 68H[ebx]        ;
fld   dword ptr [eax]               ; v=gr_info->pow2gain[pow2index]
L$207:                              ;
 cmp   edx,dword ptr -24H[ebp]      ; if(loc_bitindex>=part2end)
 jge   L$228                        ;  goto end
 dec   dword ptr -4H[ebp]            ; mc--
 jne   L$210                         ; if(!mc)
  fstp  st                            ;
  mov   edi,dword ptr -28H[ebp]       ;
  cmp   edi,dword ptr -2cH[ebp]       ;
  jle   L$2072                        ; if(lastxr>maxxr)
   mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$2072:                               ;
  mov   esi,dword ptr -30H[ebp]       ; //esi=m
  mov   eax,dword ptr [esi]           ;
  mov   dword ptr -4H[ebp],eax        ; mc=m[0]
  mov   edi,dword ptr 4H[esi]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  mov   ecx,dword ptr 8H[esi]         ; lwin=m[2]
  mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
  mov   esi,dword ptr -3cH[ebp]       ;
  movzx esi,byte ptr [esi]            ; pow2index=*sfc
  cmp   ecx,0000000cH                 ;
  jne   L$208                         ; if(lwin==3)
   mov   dword ptr -10H[ebp],4        ;  step=1
  jmp   L$209                         ;
L$208:                                ; else{
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   add   esi,dword ptr 5cH[eax+ecx]   ;  pow2index+=gr_info->full_gain[lwin]
L$209:                                ; }
  shl   esi,2                         ;
  add   esi,dword ptr 68H[eax]        ;
  fld   dword ptr [esi]               ; v=gr_info->pow2gain[pow2index]
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$210:                               ; }
 mov   eax,edx                       ; locbits=get3bytes()
 mov   ecx,eax                       ;
 shr   eax,3                         ;
 add   eax,dword ptr -48H[ebp];
 mov   eax,dword ptr [eax]           ;
 bswap eax                           ;
 and   ecx,7                         ;
 shl   eax,cl                        ;
 mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$211:                               ; do{
  movsx ebx,word ptr [ecx]            ; y=*val
  test  ebx,ebx                       ; if(y>=0)
  jge   L$212                         ;  break
  add   ecx,2                         ; val++
  inc   edx                           ; loc_bitindex++
  shl   eax,1                         ; locbits<<=1
  jnc   L$211                         ; if(locbits&0x80000000)
  sub   ecx,ebx                       ;  val-=y
  jmp   L$211                         ;
L$212:                               ; }while(1)
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$214                         ; if(y&1){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$213                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$213:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$214:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$217                         ; if(y&2){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$216                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$216:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$217:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -4H[ebp]            ; mc--
 jne   L$221                         ; if(!mc){
  fstp  st                            ;
  mov   edi,dword ptr -28H[ebp]       ;
  cmp   edi,dword ptr -2cH[ebp]       ;
  jle   L$2172                        ; if(lastxr>maxxr)
   mov   dword ptr -2cH[ebp],edi      ;  maxxr=lastxr
L$2172:                               ;
  mov   ecx,dword ptr -30H[ebp]       ; //edx=m
  mov   edi,dword ptr [ecx]           ;
  mov   dword ptr -4H[ebp],edi        ; mc=m[0]
  mov   edi,dword ptr 4H[ecx]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  mov   esi,dword ptr 8H[ecx]         ; lwin=m[2]
  mov   ecx,dword ptr -34H[ebp]       ; //edx=gr_info
  cmp   esi,0000000cH                 ;
  jne   L$219                         ; if(lwin==3){
   mov   dword ptr -10H[ebp],4        ;  step=1
   xor   esi,esi                      ;  pow2index=0
  jmp   L$220                         ;
L$219:                                ; }else{
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   mov   esi,dword ptr 5cH[ecx+esi]   ;  pow2index=gr_info->full_gain[lwin]
   shl   esi,2                        ;
L$220:                                ; }
  add   esi,dword ptr 68H[ecx]        ; pow2index+=gr_info->pow2gain
  mov   ecx,dword ptr -3cH[ebp]       ;
  movzx ecx,byte ptr [ecx]            ;
  shl   ecx,2                         ;
  add   esi,ecx                       ; pow2index+=*scf
  fld   dword ptr [esi]               ; v=*pow2index
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$221:                               ; }
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$223                         ; if(y&4){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x1+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$222                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$222:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$223:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$226                         ; if(y&8){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x1+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$225                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$225:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$226:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -20H[ebp]          ; i--
jnz L$207                           ; }while(i)
L$228:                              ;
fstp  st                            ;
mov   ebx,dword ptr -44H[ebp]       ; ebx=me
mov   ecx,dword ptr -10H[ebp]       ; ecx=step
mov   edx,dword ptr -4H[ebp]        ; edx=mc
mov   esi,dword ptr -30H[ebp]       ; esi=m
L$229:                              ; do{
 dec   edx                           ; mc--
 jne   L$232                         ; if(!mc){
  cmp   esi,ebx                       ; if(m>=me)
  jae   L$233                         ;  break
  mov   edx,dword ptr [esi]           ; mc=m[0]
  mov   edi,dword ptr 4H[esi]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  cmp   dword ptr 8H[esi],0000000cH   ;
  jne   L$230                         ; if(lwin==3)
   mov   ecx,4                        ;  step=1
  jmp   L$231                         ;
L$230:                                ; else
   mov   ecx,0000000cH                ;  step=3
L$231:                                ;
  add   esi,00000010H                 ; m+=4
L$232:                               ; }
 mov   eax,dword ptr [edi]           ; x0=xrpnt[0]
 mov   dword ptr 900H[edi],eax       ; xrpnt[576]=x0
 add   edi,ecx                       ; xrpnt+=step
 mov   eax,dword ptr [edi]           ; x0=xrpnt[0]
 mov   dword ptr 900H[edi],eax       ; xrpnt[576]=x0
 add   edi,ecx                       ; xrpnt+=step
jmp   L$229                         ; }while(1)
L$233:                              ;

mov   eax,dword ptr -2cH[ebp]       ; eax=maxxr
mov   edi,dword ptr -28H[ebp]       ; edi=lastxr
cmp   edi,eax                       ;
jle   L$2331                        ; if(lastxr>maxxr)
 mov eax,edi                        ;  maxxr=lastxr
L$2331:                             ;
sub  eax,dword ptr -38H[ebp]        ; i=maxxr-xr
mov  ecx,72                         ;
xor  edx,edx                        ;
idiv ecx                            ; sb=i/18
inc  eax                            ; sb++
cmp  eax,32                         ;
jle  L$2332                         ; if(sb>32)
 mov  eax,32                        ;  sb=32
L$2332:                             ;
mov  ebx,dword ptr -34H[ebp]        ;
mov  dword ptr 3cH[ebx],eax         ; gr_info->maxb=sb

mov  esp,ebp
pop  ebp
pop  edi
pop  esi
ret

;-----------------------------------------------------------------------

mpxdec_l3deq_dequantize_bt2_js_:
push  esi
push  edi
push  ebp
mov   ebp,esp
sub   esp,00000048H

;  -4H[ebp] : mc
;  -8H[ebp] : locbits
; -0cH[ebp] : htable
; -10H[ebp] : step
; -14H[ebp] : lp
; -18H[ebp] : sub32hlin
; -1cH[ebp] : hlinbits
; -20H[ebp] : i
; -24H[ebp] : part2end
; -28H[ebp] : lastxr
; -2cH[ebp] : maxxr
; -30H[ebp] : m
; -34H[ebp] : gr_info
; -38H[ebp] : xr
; -3cH[ebp] : scf
; -40H[ebp] : pow2index
; -44H[ebp] : me
; -48H[ebp] : loc_wordpointer

mov   dword ptr -38H[ebp],eax       ; xr=xr_p
mov   edi,eax                       ; xrpnt=xr_p
mov   dword ptr -2cH[ebp],eax       ; maxxr=xr_p
mov   dword ptr -3cH[ebp],edx       ; scf=scf_p
mov   dword ptr -34H[ebp],ebx       ; gr_info=gr_info_p
mov   edx,dword ptr FLAT:_mpxdec_bitindex  ; loc_bitindex=bitindex
mov   eax,dword ptr FLAT:_mpxdec_wordpointer;
mov   dword ptr -48H[ebp],eax       ; loc_wordpointer=wordpointer
mov   eax,dword ptr 4H[ebx]         ;
add   eax,ecx                       ;
mov   dword ptr -24H[ebp],eax       ; part2end=gr_info_p->part2_3_length+part2begin;
mov   dword ptr FLAT:_mpxdec_bitindex,eax  ; bitindex=part2end
mov   eax,dword ptr 70H[ebx]        ;
mov   dword ptr -30H[ebp],eax       ; m=gr_info->mapp

xor   esi,esi                       ; mbl=0
cmp   dword ptr 10H[ebx],esi        ; if(gr_ino->mixed_block_flag){
je    L$316                         ;
 add   eax,00000260H                ;  me=m+152
 mov   ecx,3                        ;  mb0=3
jmp   L$317                         ;
L$316:                              ; }else{
 add   eax,00000270H                ;  me=m+156
 mov   ecx,esi                      ;  mb0=mbl
L$317:                              ; }
mov   dword ptr -44H[ebp],eax       ; //me=eax
mov   dword ptr 2cH[ebx],ecx        ; gr_info->maxband[0]=mb0
mov   dword ptr 30H[ebx],ecx        ; gr_info->maxband[1]=mb0
mov   dword ptr 34H[ebx],ecx        ; gr_info->maxband[2]=mb0
mov   dword ptr 38H[ebx],esi        ; gr_info->maxbandl=mbl

fldz                                ;
fst   dword ptr -20H[ebp]           ; i=0
fst   dword ptr -28H[ebp]           ; lastxr=0
fst   dword ptr -40H[ebp]           ; pow2index=0
mov   dword ptr -4H[ebp],1          ; mc=1
L$318:                              ; do{
 mov   eax,dword ptr -20H[ebp]       ;
 shl   eax,2                         ;
 add   eax,dword ptr -34H[ebp]       ;
 mov   ebx,dword ptr 40H[eax]        ;
 test  ebx,ebx                       ; lp=gr_info->regions[i]
 jz    L$347                         ; if(lp){
  mov   dword ptr -14H[ebp],ebx       ;
  mov   ebx,dword ptr 14H[eax]        ;
  shl   ebx,3                         ;
  add   ebx,offset FLAT:_ht           ; h=ht+gr_info->table_select[i]
  mov   eax,dword ptr 4H[ebx]         ;
  mov   dword ptr -0cH[ebp],eax       ; htable=h->table
  mov   eax,dword ptr [ebx]           ;
  test  eax,eax                       ; hlinbits=h->linbits
  jz    L$335                         ; if(hlinbits){
   mov   ebx,00000020H                 ;
   sub   ebx,eax                       ;
   mov   dword ptr -18H[ebp],ebx       ; sub32hlin=32-hlinbits
   inc   eax                           ;
   mov   dword ptr -1cH[ebp],eax       ; hlinbits=hlinbits+1
L$319:                                 ; do{
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$322                         ; if(mc){
     fstp  st                            ;
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
     mov   edi,dword ptr -28H[ebp]       ;
     test  edi,edi                       ;
     jz    L$3192                        ; if(lastxr){
      cmp   edi,dword ptr -2cH[ebp]      ;
      jle   L$3193                       ;  if(lastxr>maxxr)
       mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$3193:
      mov   dword ptr -28H[ebp],0        ;  lastxr=0
      mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
      lea   esi,dword ptr 2cH[eax]       ;  esi=&(gr_info->maxband[0])
      add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
      mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$3192:                                  ; }
     mov   esi,dword ptr [ecx]           ;
     mov   dword ptr -4H[ebp],esi        ; mc=m[0]
     mov   edi,dword ptr -38H[ebp]       ;
     add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$320                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$321                         ;
L$320:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$321:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,2                         ;
     mov   ecx,dword ptr 68H[eax]        ;
     fld   dword ptr [esi+ecx]           ; v=gr_info->pow2gain[pow2index]
     shl   esi,5                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ;  m+=4
L$322:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp]       ;
    mov   eax,dword ptr [eax]           ;
    and   ecx,7                         ;
    bswap eax                           ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; *val=htable
L$323:                                  ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$324                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$323                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$323                         ;
L$324:                                  ; }while(1)
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$328                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   cl,0f0H                       ;
     jne   L$326                         ; if(x==15){  // (x==0xf0)
      mov   eax,edx                       ; locbits=get3bytes()
      mov   ecx,eax                       ;
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp]       ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   dword ptr -8H[ebp],eax        ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   eax,cl                        ; x=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[eax*4]  ; a=ispow[x]*v
      fmul  st,st(1)                      ;
      mov   eax,dword ptr -8H[ebp]        ;
      mov   ecx,dword ptr -1cH[ebp]       ; ecx=hlinbits
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits  // hlinbits+1
      jnc   L$325                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
L$325:                                    ;
      fstp  dword ptr [edi]               ; xrpnt[0]=a
     jmp   L$329                         ; }else{
L$326:                                    ;
      shr   ecx,2                         ; // x>>=4
      shl   eax,1                         ; locbits<<=1
      jnc   L$327                         ; if(locbits&0x80000000)
       neg   ecx                          ;  x=-x
L$327:                                    ;
      inc   edx                           ; loc_bitindex++
      mov   ecx,dword ptr [esi+ecx]       ; x=isp32[x]
L$328:                                  ;}else
     mov   dword ptr [edi],ecx           ; xrpnt[0]=x
L$329:
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$333                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   ebx,0000000fH                 ;
     jne   L$331                         ; if(y==15){
      mov   eax,edx                       ; locbits=get3bytes()
      mov   ecx,eax                       ;
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp]       ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   ebx,eax                       ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   ebx,cl                        ; y=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[ebx*4]  ; a=ispow[x]*v
      fmul  st,st(1)                      ;
      mov   ecx,dword ptr -1cH[ebp]       ;
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$330                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
L$330:                                    ;
      fstp  dword ptr [edi]               ; xrpnt[0]=a
     jmp   L$334                         ; }else{
L$331:                                    ;
      shl   ebx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$332                         ; if(locbits&0x80000000)
       neg   ebx                          ;  y=-y
L$332:                                    ;
      inc   edx                           ; loc_bitindex++
      mov   ebx,dword ptr [esi+ebx]       ; y=isp32[y]
L$333:                                  ; }else
     mov   dword ptr [edi],ebx           ; xrpnt[0]=y
L$334:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    dec   dword ptr -14H[ebp]          ; lp--
    jnz   L$319                        ; }while(lp)
    jmp   near ptr L$347              ; }else{
L$335:                                 ; do{
    dec   dword ptr -4H[ebp]           ; mc--
    jne   L$338                         ; if(!mc){
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
     mov   edi,dword ptr -28H[ebp]       ;
     test  edi,edi                       ;
     jz    L$3352                        ; if(lastxr){
      cmp   edi,dword ptr -2cH[ebp]      ;
      jle   L$3353                       ;  if(lastxr>maxxr)
       mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$3353:
      mov   dword ptr -28H[ebp],0        ;  lastxr=0
      mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
      lea   esi,dword ptr 2cH[eax]       ;  esi=&(gr_info->maxband[0])
      add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
      mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$3352:                                  ; }
     mov   esi,dword ptr [ecx]           ;
     mov   dword ptr -4H[ebp],esi        ; mc=m[0]
     mov   edi,dword ptr -38H[ebp]       ;
     add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$336                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$337                         ;
L$336:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$337:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,7                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ; m+=4
L$338:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp]       ;
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$339:                                  ;do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$340                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$339                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$339                         ;
L$340:                                  ; }while(1)
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$342                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shr   ecx,2                         ;
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$341                         ; if(locbits&0x80000000)
      neg   ecx                          ;  x=-x
L$341:                                   ;
     mov   ecx,dword ptr [esi+ecx]       ; x=isp32[x]
L$342:                                  ; }else
    mov   dword ptr [edi],ecx           ;  xrpnt[0]=x
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$345                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shl   ebx,2                         ;
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$344                         ; if(locbits&0x80000000)
      neg   ebx                          ;  y=-y
L$344:                                   ;
     mov   ebx,dword ptr [esi+ebx]       ; y=isp32[y]
L$345:                                  ; }else
    mov   dword ptr [edi],ebx            ; xrpnt[0]=y
    add   edi,dword ptr -10H[ebp]       ;xrpnt+=step
    dec   dword ptr -14H[ebp]          ;lp--
   jnz   L$335                         ;}while(lp)
L$347:                                ;} //endif hlinbits, endif lp
 inc   dword ptr -20H[ebp]           ; i++
 cmp   dword ptr -20H[ebp],2         ;
jl    L$318                         ; }while(i<2)
mov   ebx,dword ptr -34H[ebp]       ;
mov   eax,dword ptr 58H[ebx]        ;
shl   eax,2                         ;
add   eax,offset FLAT:_htc2         ; h=htc2+gr_info->count1table_select
mov   eax,dword ptr [eax]           ;
mov   dword ptr -0cH[ebp],eax       ; htable=h->table
mov   eax,dword ptr 4cH[ebx]        ; i=gr_info->regions[3]
test  eax,eax                       ;
jz    L$369                         ; if(!i) goto end
mov   dword ptr -20H[ebp],eax       ;
mov   eax,dword ptr -40H[ebp]       ;
shl   eax,2                         ;
add   eax,dword ptr 68H[ebx]        ;
fstp  st                            ;
mov   esi,dword ptr [eax]           ; v=gr_info->pow2gain[pow2index]
fld   dword ptr [eax]               ;
fchs                                ; nv=-v
L$348:
 cmp   edx,dword ptr -24H[ebp]       ;
 jge   L$369                         ;if(loc_bitindex>=part2end) goto end
 dec   dword ptr -4H[ebp]            ;mc--
 jne   L$351                         ;if(!mc){
  fstp  st                            ;
  mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
  mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
  mov   edi,dword ptr -28H[ebp]       ;
  test  edi,edi                       ;
  jz    L$3482                        ; if(lastxr){
   cmp   edi,dword ptr -2cH[ebp]      ;
   jle   L$3483                       ;  if(lastxr>maxxr)
    mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$3483:                               ;
   mov   dword ptr -28H[ebp],0        ;  lastxr=0
   mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
   lea   esi,dword ptr 2cH[eax]       ;  esi=&(gr_info->maxband[0])
   add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
   mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$3482:                               ; }
  mov   esi,dword ptr [ecx]           ;
  mov   dword ptr -4H[ebp],esi        ; mc=m[0]
  mov   edi,dword ptr -38H[ebp]       ;
  add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
  mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
  mov   esi,dword ptr -3cH[ebp]       ;
  movzx esi,byte ptr [esi]            ; esi=*scf
  cmp   ecx,0000000cH                 ;
  jne   L$349                         ; if(lwin==3)
   mov   dword ptr -10H[ebp],4        ;  step=1
  jmp   L$350                         ; else{
L$349:                                ;
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   add   esi,dword ptr 5cH[eax+ecx]   ;  pow2index+=gr_info->full_gain[lwin]
L$350:                                ; }
  shl   esi,2                         ;
  add   esi,dword ptr 68H[eax]        ;
  fld   dword ptr [esi]               ; nv=gr_info->pow2gain[pow2index]
  fchs                                ;
  mov   esi,dword ptr [esi]           ;
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$351:                               ; }
 mov   eax,edx                       ; locbits=get3bytes()
 mov   ecx,eax                       ;
 shr   eax,3                         ;
 add   eax,dword ptr -48H[ebp]       ;
 mov   eax,dword ptr [eax]           ;
 bswap eax                           ;
 and   ecx,7                         ;
 shl   eax,cl                        ;
 mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$352:                               ; do{
  movsx ebx,word ptr [ecx]            ; y=*val
  test  ebx,ebx                       ; if(y>=0)
  jge   L$353                         ;  break
  add   ecx,2                         ; val++
  inc   edx                           ; loc_bitindex++
  shl   eax,1                         ; locbits<<=1
  jnc   L$352                         ; if(locbits&0x80000000)
  sub   ecx,ebx                       ;  val-=y
  jmp   L$352                         ;
L$353:                               ; }while(1)
 shr   ebx,1                         ;
 jnc   L$355                         ; if(y&1){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$354                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$356                        ;
L$354:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$356                        ;
L$355:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$356:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 shr   ebx,1                         ;
 jnc   L$358                         ; if(y&2){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$357                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$359                        ;
L$357:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$359                        ;
L$358:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$359:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -4H[ebp]            ; mc--
 jne   L$362                         ; if(!mc){
  fstp  st                            ;
  mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
  mov   edi,dword ptr -28H[ebp]       ;
  test  edi,edi                       ;
  jz    L$3592                        ; if(lastxr){
   cmp   edi,dword ptr -2cH[ebp]      ;
   jle   L$3593                       ;  if(lastxr>maxxr)
    mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$3593:                               ;
   mov   dword ptr -28H[ebp],0        ;  lastxr=0
   mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
   mov   esi,dword ptr -34H[ebp]      ;  //esi=gr_info
   lea   esi,dword ptr 2cH[esi]       ;  //esi=&(gr_info->maxband[0])
   add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
   mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$3592:                               ; }
  mov   edi,dword ptr [ecx]           ;
  mov   dword ptr -4H[ebp],edi        ; mc=m[0]
  mov   edi,dword ptr 4H[ecx]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  mov   esi,dword ptr 8H[ecx]         ; lwin=m[2]
  mov   ecx,dword ptr -34H[ebp]       ; //ecx=gr_info
  cmp   esi,0000000cH                 ;
  jne   L$360                         ; if(lwin==3){
   mov   dword ptr -10H[ebp],4        ;  step=1
   xor   esi,esi                      ;  pow2index=0
  jmp   L$361                         ;
L$360:                                ; }else{
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   mov   esi,dword ptr 5cH[ecx+esi]   ;  pow2index=gr_info->full_gain[lwin]
   shl   esi,2                        ;
L$361:                                ; }
  add   esi,dword ptr 68H[ecx]        ; pow2index+=gr_info->pow2gain
  mov   ecx,dword ptr -3cH[ebp]       ;
  movzx ecx,byte ptr [ecx]            ;
  shl   ecx,2                         ;
  add   esi,ecx                       ; pow2index+=scf
  fld   dword ptr [esi]               ; nv=*pow2index
  mov   esi,dword ptr [esi]           ; v=*pow2index
  fchs                                ;
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$362:                               ;
 shr   ebx,1                         ;
 jnc   L$364                         ; if(y&4){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$363                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$365                        ;
L$363:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$365                        ;
L$364:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$365:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 shr   ebx,1                         ;
 jnc   L$367                         ; if(y&8){
  mov   dword ptr -28H[ebp],edi      ;  lastxr=xrpnt
  inc   edx                          ;  loc_bitindex++
  shl   eax,1                        ;  locbits<<=1
  jnc   L$366                        ;  if(locbits&0x80000000)
   fst   dword ptr [edi]             ;   xrpnt[0]=nv
  jmp   L$368                        ;
L$366:                               ;  else
   mov   dword ptr [edi],esi         ;   xrpnt[0]=v
  jmp   L$368                        ;
L$367:                               ; }else
   mov   dword ptr [edi],0           ;  xrpnt[0]=0
L$368:                               ;
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -20H[ebp]          ; i--
jnz   L$348                         ; }while(i)
L$369:                              ;
fstp  st                            ;

mov  eax,dword ptr -2cH[ebp]        ; eax=maxxr
mov  ecx,dword ptr -28H[ebp]        ; ecx=lastxr
mov  ebx,dword ptr -34H[ebp]        ; ebx=gr_info
mov  esi,dword ptr -30H[ebp]        ; esi=m
test ecx,ecx                        ;
jz   L$3741                         ; if(lastxr){
 cmp  ecx,eax                       ;
 jle  L$3742                        ;  if(lastxr>maxxr)
  mov eax,ecx                       ;   maxxr=lastxr
L$3742:                             ;
 mov  ecx,dword ptr -4H[esi]        ;  cb=m[-1]
 lea  edx,dword ptr 2cH[ebx]        ;  //edx=&(gr_info->maxband[0])
 add  edx,dword ptr -8H[esi]        ;  lwin=m[-2]
 mov  dword ptr [edx],ecx           ;  gr_info->maxband[lwin]=cb
L$3741:                             ; }
sub  eax,dword ptr -38H[ebp]        ; i=maxxr-xr
mov  ecx,72                         ;
xor  edx,edx                        ;
idiv ecx                            ; sb=i/18
inc  eax                            ; sb++
cmp  eax,32                         ;
jle  L$3743                         ; if(sb>32)
 mov  eax,32                        ;  sb=32
L$3743:                             ;
mov  dword ptr 3cH[ebx],eax         ; gr_info->maxb=sb

xor   eax,eax                       ; eax=0
mov   ebx,dword ptr -44H[ebp]       ; ebx=me
mov   ecx,dword ptr -10H[ebp]       ; ecx=step
mov   edx,dword ptr -4H[ebp]        ; edx=mc
L$370:                              ; do{
 dec   edx                           ; mc--
 jne   L$373                         ; if(!mc){
  cmp   esi,ebx                       ; if(m>=me)
  jae   L$374                         ;  break
  mov   edx,dword ptr [esi]           ; mc=m[0]
  mov   edi,dword ptr 4H[esi]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  cmp   dword ptr 8H[esi],0000000cH   ;
  jne   L$371                         ; if(lwin==3)
   mov   ecx,4                        ;  step=1
  jmp   L$372                         ;
L$371:                                ; else
   mov   ecx,0000000cH                ;  step=3
L$372:                                ;
  add   esi,00000010H                 ; m+=4
L$373:                               ; }
 mov   dword ptr [edi],eax           ; xrpnt[0]=0
 add   edi,ecx                       ; xrpnt+=step
 mov   dword ptr [edi],eax           ; xrpnt[0]=0
 add   edi,ecx                       ; xrpnt+=step
jmp   L$370                         ; }while(1)
L$374:

mov  esp,ebp
pop  ebp
pop  edi
pop  esi
ret
;--------------------------------------------------------------------------
mpxdec_l3deq_dequantize_ms_bt2_js_:
push esi
push edi
push ebp
mov  ebp,esp
sub  esp,00000048H

;  -4H[ebp] : mc
;  -8H[ebp] : locbits
; -0cH[ebp] : htable
; -10H[ebp] : step
; -14H[ebp] : lp
; -18H[ebp] : sub32hlin
; -1cH[ebp] : hlinbits
; -20H[ebp] : i
; -24H[ebp] : part2end
; -28H[ebp] : lastxr
; -2cH[ebp] : maxxr
; -30H[ebp] : m
; -34H[ebp] : gr_info
; -38H[ebp] : xr
; -3cH[ebp] : scf
; -40H[ebp] : pow2index
; -44H[ebp] : me
; -48H[ebp] : loc_wordpointer

mov   dword ptr -38H[ebp],eax       ; xr=xr_p
mov   edi,eax                       ; xrpnt=xr_p
mov   dword ptr -2cH[ebp],eax       ; maxxr=xr_p
mov   dword ptr -3cH[ebp],edx       ; scf=scf_p
mov   dword ptr -34H[ebp],ebx       ; gr_info=gr_info_p
mov   edx,dword ptr FLAT:_mpxdec_bitindex  ; loc_bitindex=bitindex
mov   eax,dword ptr FLAT:_mpxdec_wordpointer;
mov   dword ptr -48H[ebp],eax       ; loc_wordpointer=wordpointer
mov   eax,dword ptr 4H[ebx]         ;
add   eax,ecx                       ;
mov   dword ptr -24H[ebp],eax       ; part2end=gr_info_p->part2_3_length+part2begin;
mov   dword ptr FLAT:_mpxdec_bitindex,eax  ; bitindex=part2end
mov   eax,dword ptr 70H[ebx]        ;
mov   dword ptr -30H[ebp],eax       ; m=gr_info_p->mapp
xor   esi,esi                       ; mbl=0
cmp   dword ptr 10H[ebx],esi        ; if(gr_info->mixed_block_flag){
je    L$475                         ;
 add   eax,00000260H                ;  me=m+152
 mov   ecx,3                        ;  mb0=3
jmp   L$476                         ;
L$475:                              ; }else{
 add   eax,00000270H                ;  me=m+156
 mov   ecx,esi                      ;  mb0=mbl
L$476:                              ; }
mov   dword ptr -44H[ebp],eax       ; //me=eax
mov   dword ptr 2cH[ebx],ecx        ; gr_info->maxband[0]=mb0
mov   dword ptr 30H[ebx],ecx        ; gr_info->maxband[1]=mb0
mov   dword ptr 34H[ebx],ecx        ; gr_info->maxband[2]=mb0
mov   dword ptr 38H[ebx],esi        ; gr_info->maxbandl=mbl

fldz                                ;
fst   dword ptr -20H[ebp]           ; i=0
fst   dword ptr -28H[ebp]           ; lastxr=0
fst   dword ptr -40H[ebp]           ; pow2index=0
mov   dword ptr -4H[ebp],1          ; mc=1
L$477:                              ; do{ // i
 mov   eax,dword ptr -20H[ebp]       ;
 shl   eax,2                         ;
 add   eax,dword ptr -34H[ebp]       ;
 mov   ebx,dword ptr 40H[eax]        ; lp=gr_info->regions[i]
 test  ebx,ebx                       ;
 jz    L$506                         ; if(lp){
  mov   dword ptr -14H[ebp],ebx       ; //lp=ebx
  mov   ebx,dword ptr 14H[eax]        ;
  shl   ebx,3                         ;
  add   ebx,offset FLAT:_ht           ; h=ht+gr_info->table_select[i]
  mov   eax,dword ptr 4H[ebx]         ;
  mov   dword ptr -0cH[ebp],eax       ; htable=h->table
  mov   eax,dword ptr [ebx]           ; hlinbits=h->linbits
  test  eax,eax                       ;
  jz    L$494                         ; if(hlinbits){
   mov   ebx,00000020H                 ;
   sub   ebx,eax                       ;
   mov   dword ptr -18H[ebp],ebx       ; sub32hlin=32-hlinbits
   inc   eax                           ;
   mov   dword ptr -1cH[ebp],eax       ; hlinbits=hlinbits+1
L$478:                                 ; do{  //lp
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$481                         ; if(!mc){
     fstp  st                            ;
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
     mov   edi,dword ptr -28H[ebp]       ;
     test  edi,edi                       ;
     jz    L$4782                        ; if(lastxr){
      cmp   edi,dword ptr -2cH[ebp]      ;
      jle   L$4783                       ;  if(lastxr>maxxr)
       mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$4783:                                  ;
      mov   dword ptr -28H[ebp],0        ;  lastxr=0
      mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
      lea   esi,dword ptr 2cH[eax]       ;  esi=&(gr_info->maxband[0])
      add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
      mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$4782:                                  ; }
     mov   esi,dword ptr [ecx]           ;
     mov   dword ptr -4H[ebp],esi        ; mc=m[0]
     mov   edi,dword ptr -38H[ebp]       ;
     add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$479                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$480                         ;
L$479:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$480:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,2                         ;
     mov   ecx,dword ptr 68H[eax]        ;
     fld   dword ptr [esi+ecx]           ; v=gr_info->pow2gain[pow2index]
     shl   esi,5                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->isp32base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ; m+=4
L$481:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp]       ;
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$482:                                  ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$483                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$482                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$482                         ;
L$483:                                  ; }while(1)
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$487                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   cl,0f0H                       ;
     jne   L$484                         ; if(x==15){  // (x==0xf0)
      mov   eax,edx                       ; locbits=get3bytes()
      mov   ecx,eax                       ;
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp]       ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   dword ptr -8H[ebp],eax        ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   eax,cl                        ; x=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[eax*4]  ;
      fmul  st,st(2)                      ; a=ispow[x]*v
      mov   eax,dword ptr -8H[ebp]        ;
      mov   ecx,dword ptr -1cH[ebp]       ; ecx=hlinbits //+1
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$486                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
     jmp   L$486                          ;
L$484:                                   ; }else{
      shr   ecx,2                         ; // x>>=4
      shl   eax,1                         ; locbits<<=1
      jnc   L$485                         ; if(locbits&0x80000000)
       neg   ecx                          ;  x=-x
L$485:                                    ;
      fld   dword ptr [esi+ecx]           ; a=isp32[x]
      inc   edx                           ; loc_bitindex++
L$486:                                   ; }
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
L$487:                                  ; }
    fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
L$488:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$492                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     cmp   ebx,0000000fH                 ;
     jne   L$489                         ; if(y==15){
      mov   eax,edx                       ; locbits=get3bytes()
      mov   ecx,eax                       ;
      shr   eax,3                         ;
      add   eax,dword ptr -48H[ebp]       ;
      mov   eax,dword ptr [eax]           ;
      bswap eax                           ;
      and   ecx,7                         ;
      shl   eax,cl                        ;
      mov   ebx,eax                       ;
      mov   ecx,dword ptr -18H[ebp]       ;
      shr   ebx,cl                        ; y=locbits>>sub32hlin
      fld   dword ptr FLAT:_ispow[ebx*4]  ;
      fmul  st,st(2)                      ; a=ispow[y]*v
      mov   ecx,dword ptr -1cH[ebp]       ; ecx=hlinbits //+1
      add   edx,ecx                       ; loc_bitindex+=hlinbits
      shl   eax,cl                        ; locbits<<=hlinbits
      jnc   L$491                         ; if(locbits&0x80000000)
       fchs                               ;  a=-a
     jmp    L$491                         ;
L$489:                                   ; }else{
      shl   ebx,2                         ;
      shl   eax,1                         ; locbits<<=1
      jnc   L$490                         ; if(locbits&0x80000000)
       neg   ebx                          ;  y=-y
L$490:                                    ;
      fld   dword ptr [esi+ebx]           ; a=isp32[y]
      inc   edx                           ; loc_bitindex++
L$491:                                   ; }
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
L$492:                                  ; }
     fstp  dword ptr 900H[edi]          ; xrpnt[576]=x0
L$493:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    dec   dword ptr -14H[ebp]          ; lp--
   jnz   L$478                         ; }while(lp)
  jmp   near ptr L$506                ; }else{ // hlinbits
L$494:                                 ; do{ //lp
    dec   dword ptr -4H[ebp]            ; mc--
    jne   L$497                         ; if(!mc){
     mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
     mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
     mov   edi,dword ptr -28H[ebp]       ;
     test  edi,edi                       ;
     jz    L$4942                        ; if(lastxr){
      cmp   edi,dword ptr -2cH[ebp]      ;
      jle   L$4943                       ;  if(lastxr>maxxr)
       mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$4943:                                  ;
      mov   dword ptr -28H[ebp],0        ;  lastxr=0
      mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
      lea   esi,dword ptr 2cH[eax]       ;  esi=&(gr_info->maxband[0])
      add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
      mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$4942:                                  ; }
     mov   esi,dword ptr [ecx]           ;
     mov   dword ptr -4H[ebp],esi        ; mc=m[0]
     mov   edi,dword ptr -38H[ebp]       ;
     add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
     mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
     mov   esi,dword ptr -3cH[ebp]       ;
     movzx esi,byte ptr [esi]            ; esi=*scf
     cmp   ecx,0000000cH                 ;
     jne   L$495                         ; if(lwin==3)
      mov   dword ptr -10H[ebp],4        ;  step=1
     jmp   L$496                         ;
L$495:                                   ; else{
      mov   dword ptr -10H[ebp],0000000cH;  step=3
      add   esi,dword ptr 5cH[eax+ecx]   ;  esi+=gr_info->full_gain[lwin]
L$496:                                   ; }
     mov   dword ptr -40H[ebp],esi       ; pow2index=esi
     shl   esi,7                         ;
     add   esi,dword ptr 6cH[eax]        ; isp32=gr_info->ispow32_base[pow2index<<5]
     inc   dword ptr -3cH[ebp]           ; scf++
     add   dword ptr -30H[ebp],00000010H ; m+=4
L$497:                                  ; }
    mov   eax,edx                       ; locbits=get3bytes()
    mov   ecx,eax                       ;
    shr   eax,3                         ;
    add   eax,dword ptr -48H[ebp]       ;
    mov   eax,dword ptr [eax]           ;
    bswap eax                           ;
    and   ecx,7                         ;
    shl   eax,cl                        ;
    mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$498:                                  ; do{
     movsx ebx,word ptr [ecx]            ; y=*val
     test  ebx,ebx                       ; if(y>=0)
     jge   L$499                         ;  break
     add   ecx,2                         ; val++
     inc   edx                           ; loc_bitindex++
     shl   eax,1                         ; locbits<<=1
     jnc   L$498                         ; if(locbits&0x80000000)
     sub   ecx,ebx                       ;  val-=y
     jmp   L$498                         ;
L$499:                                  ; }while(1)
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    mov   ecx,ebx                       ; x=y
    and   ecx,0f0H                      ; x&=0xf0
    je    L$501                         ; if(x){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shr   ecx,2                         ;
     shl   eax,1                         ; locbits<<=1
     jnc   L$500                         ; if(locbits&0x80000000)
      neg   ecx                          ;  x=-x
L$500:                                   ;
     fld   dword ptr [esi+ecx]           ; a=isp32[x]
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
     inc   edx                           ; loc_bitindex++
L$501:                                  ; }
    fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
L$502:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    fld   dword ptr [edi]               ; x0=xrpnt[0]
    and   ebx,0000000fH                 ; y&=0x0f
    je    L$504                         ; if(y){
     mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
     shl   ebx,2                         ;
     shl   eax,1                         ; locbits<<=1
     jnc   L$503                         ; if(locbits&0x80000000)
      neg   ebx                          ;  y=-y
L$503:                                   ;
     fld   dword ptr [esi+ebx]           ; a=isp32[y]
     fld   st(1)                         ;
     fadd  st,st(1)                      ;
     fstp  dword ptr [edi]               ; xrpnt[0]  =x0+a
     fsub                                ;         x0=x0-a
     inc   edx                           ; loc_bitindex++
L$504:                                  ; }
    fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
L$505:                                  ;
    add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
    dec   dword ptr -14H[ebp]          ; lp--
   jnz   L$494                         ; }while(lp)
L$506:                                ; // endif hlinbits & lp
 inc   dword ptr -20H[ebp]           ;
 cmp   dword ptr -20H[ebp],2         ;
jl    L$477                         ; }while(i<2)
mov   ebx,dword ptr -34H[ebp]       ;
mov   eax,dword ptr 58H[ebx]        ;
shl   eax,2                         ;
add   eax,offset FLAT:_htc2         ; h=htc2+gr_info->count1table_select
mov   eax,dword ptr [eax]           ;
mov   dword ptr -0cH[ebp],eax       ; htable=h->table
mov   eax,dword ptr 4cH[ebx]        ; i=gr_info->regions[3]
test  eax,eax                       ; if(!i)
jz    L$528                         ;  goto end
mov   dword ptr -20H[ebp],eax       ;
fstp  st                            ;
mov   eax,dword ptr -40H[ebp]       ;
shl   eax,2                         ;
add   eax,dword ptr 68H[ebx]        ;
fld   dword ptr [eax]               ; v=gr_info->pow2gain[pow2index]
L$507:                              ;
 cmp   edx,dword ptr -24H[ebp]      ; if(loc_bitindex>=part2end)
 jge   L$528                        ;  goto end
 dec   dword ptr -4H[ebp]            ; mc--
 jne   L$510                         ; if(!mc)
  fstp  st                            ;
  mov   eax,dword ptr -34H[ebp]       ; //eax=gr_info
  mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
  mov   edi,dword ptr -28H[ebp]       ;
  test  edi,edi                       ;
  jz    L$5072                        ; if(lastxr){
   cmp   edi,dword ptr -2cH[ebp]      ;
   jle   L$5073                       ;  if(lastxr>maxxr)
    mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$5073:                               ;
   mov   dword ptr -28H[ebp],0        ;  lasxr=0
   mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
   lea   esi,dword ptr 2cH[eax]       ;  esi=&(gr_info->maxband[0])
   add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
   mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$5072:                               ; }
  mov   esi,dword ptr [ecx]           ;
  mov   dword ptr -4H[ebp],esi        ; mc=m[0]
  mov   edi,dword ptr -38H[ebp]       ;
  add   edi,dword ptr 4H[ecx]         ; xrpnt=xr+m[1]
  mov   ecx,dword ptr 8H[ecx]         ; lwin=m[2]
  mov   esi,dword ptr -3cH[ebp]       ;
  movzx esi,byte ptr [esi]            ; pow2index=*sfc
  cmp   ecx,0000000cH                 ;
  jne   L$508                         ; if(lwin==3)
   mov   dword ptr -10H[ebp],4        ;  step=1
  jmp   L$509                         ;
L$508:                                ; else{
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   add   esi,dword ptr 5cH[eax+ecx]   ;  pow2index+=gr_info->full_gain[lwin]
L$509:                                ; }
  shl   esi,2                         ;
  add   esi,dword ptr 68H[eax]        ;
  fld   dword ptr [esi]               ; v=gr_info->pow2gain[pow2index]
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$510:                               ; }
 mov   eax,edx                       ; locbits=get3bytes()
 mov   ecx,eax                       ;
 shr   eax,3                         ;
 add   eax,dword ptr -48H[ebp]       ;
 mov   eax,dword ptr [eax]           ;
 bswap eax                           ;
 and   ecx,7                         ;
 shl   eax,cl                        ;
 mov   ecx,dword ptr -0cH[ebp]       ; val=htable
L$511:                               ; do{
  movsx ebx,word ptr [ecx]            ; y=*val
  test  ebx,ebx                       ; if(y>=0)
  jge   L$512                         ;  break
  add   ecx,2                         ; val++
  inc   edx                           ; loc_bitindex++
  shl   eax,1                         ; locbits<<=1
  jnc   L$511                         ; if(locbits&0x80000000)
  sub   ecx,ebx                       ;  val-=y
  jmp   L$511                         ;
L$512:                               ; }while(1)
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$514                         ; if(y&1){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$513                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$513:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$514:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$517                         ; if(y&2){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x0+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$516                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$516:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$517:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -4H[ebp]            ; mc--
 jne   L$521                         ; if(!mc){
  fstp  st                            ;
  mov   ecx,dword ptr -30H[ebp]       ; //ecx=m
  mov   edi,dword ptr -28H[ebp]       ;
  test  edi,edi                       ;
  jz    L$5172                        ; if(lastxr){
   cmp   edi,dword ptr -2cH[ebp]      ;
   jle   L$5173                       ;  if(lastxr>maxxr)
    mov   dword ptr -2cH[ebp],edi     ;   maxxr=lastxr
L$5173:                               ;
   mov   dword ptr -28H[ebp],0        ;  lasxr=0
   mov   edi,dword ptr -4H[ecx]       ;  cb=m[-1]
   mov   esi,dword ptr -34H[ebp]      ;  //esi=gr_info
   lea   esi,dword ptr 2cH[esi]       ;  //esi=&(gr_info->maxband[0])
   add   esi,dword ptr -8H[ecx]       ;  lwin=m[-2]
   mov   dword ptr [esi],edi          ;  gr_info->maxband[lwin]=cb
L$5172:                               ; }
  mov   edi,dword ptr [ecx]           ;
  mov   dword ptr -4H[ebp],edi        ; mc=m[0]
  mov   edi,dword ptr 4H[ecx]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  mov   esi,dword ptr 8H[ecx]         ; lwin=m[2]
  mov   ecx,dword ptr -34H[ebp]       ; //ecx=gr_info
  cmp   esi,0000000cH                 ;
  jne   L$519                         ; if(lwin==3){
   mov   dword ptr -10H[ebp],4        ;  step=1
   xor   esi,esi                      ;  pow2index=0
  jmp   L$520                         ;
L$519:                                ; }else{
   mov   dword ptr -10H[ebp],0000000cH;  step=3
   mov   esi,dword ptr 5cH[ecx+esi]   ;  pow2index=gr_info->full_gain[lwin]
   shl   esi,2                        ;
L$520:                                ; }
  add   esi,dword ptr 68H[ecx]        ; pow2index+=gr_info->pow2gain
  mov   ecx,dword ptr -3cH[ebp]       ;
  movzx ecx,byte ptr [ecx]            ;
  shl   ecx,2                         ;
  add   esi,ecx                       ; pow2index+=*scf
  fld   dword ptr [esi]               ; v=*pow2index
  inc   dword ptr -3cH[ebp]           ; scf++
  add   dword ptr -30H[ebp],00000010H ; m+=4
L$521:                               ; }
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$523                         ; if(y&4){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x1+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$522                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$522:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$523:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 fld   dword ptr [edi]               ; x0=xrpnt[0]
 shr   ebx,1                         ;
 jnc   L$526                         ; if(y&8){
  fld   st                            ;
  fsub  st,st(2)                      ; x0=x0-v
  inc   edx                           ; loc_bitindex++
  mov   dword ptr -28H[ebp],edi       ; lastxr=xrpnt
  fxch  st(1)                         ;
  fadd  st,st(2)                      ; x1=x1+v
  shl   eax,1                         ; locbits<<=1
  jnc   L$525                         ; if(locbits&0x80000000)
   fxch  st(1)                        ;  x1=x0;x0=x1
L$525:                                ;
  fstp  dword ptr [edi]               ; xrpnt[0]=x1
L$526:                               ; }
 fstp  dword ptr 900H[edi]           ; xrpnt[576]=x0
 add   edi,dword ptr -10H[ebp]       ; xrpnt+=step
 dec   dword ptr -20H[ebp]          ; i--
jnz L$507                           ; }while(i)
L$528:                              ;
fstp  st                            ;

mov  eax,dword ptr -2cH[ebp]        ; eax=maxxr
mov  ecx,dword ptr -28H[ebp]        ; ecx=lastxr
mov  ebx,dword ptr -34H[ebp]        ; ebx=gr_info
mov  esi,dword ptr -30H[ebp]        ; esi=m
test ecx,ecx                        ;
jz   L$5291                         ; if(lastxr){
 cmp  ecx,eax                       ;
 jle  L$5292                        ;  if(lastxr>maxxr)
  mov eax,ecx                       ;   maxxr=lastxr
L$5292:                             ;
 mov  ecx,dword ptr -4H[esi]        ;  cb=m[-1]
 lea  edx,dword ptr 2cH[ebx]        ;  //edx=&(gr_info->maxband[0])
 add  edx,dword ptr -8H[esi]        ;  lwin=m[-2]
 mov  dword ptr [edx],ecx           ;  gr_info->maxband[lwin]=cb
L$5291:                             ; }
sub  eax,dword ptr -38H[ebp]        ; i=maxxr-xr
mov  ecx,72                         ;
xor  edx,edx                        ;
idiv ecx                            ; sb=i/18
inc  eax                            ; sb++
cmp  eax,32                         ;
jle  L$5293                         ; if(sb>32)
 mov  eax,32                        ;  sb=32
L$5293:                             ;
mov  dword ptr 3cH[ebx],eax         ; gr_info->maxb=sb

mov   ebx,dword ptr -44H[ebp]       ; ebx=me
mov   ecx,dword ptr -10H[ebp]       ; ecx=step
mov   edx,dword ptr -4H[ebp]        ; edx=mc
L$529:                              ; do{
 dec   edx                           ; mc--
 jne   L$532                         ; if(!mc){
  cmp   esi,ebx                       ; if(m>=me)
  jae   L$533                         ;  break
  mov   edx,dword ptr [esi]           ; mc=m[0]
  mov   edi,dword ptr 4H[esi]         ;
  add   edi,dword ptr -38H[ebp]       ; xrpnt=xr+m[1]
  cmp   dword ptr 8H[esi],0000000cH   ;
  jne   L$530                         ; if(lwin==3)
   mov   ecx,4                        ;  step=1
  jmp   L$531                         ;
L$530:                                ; else
   mov   ecx,0000000cH                ;  step=3
L$531:                                ;
  add   esi,00000010H                 ; m+=4
L$532:                               ; }
 mov   eax,dword ptr [edi]           ; x0=xrpnt[0]
 mov   dword ptr 900H[edi],eax       ; xrpnt[576]=x0
 add   edi,ecx                       ; xrpnt+=step
 mov   eax,dword ptr [edi]           ; x0=xrpnt[0]
 mov   dword ptr 900H[edi],eax       ; xrpnt[576]=x0
 add   edi,ecx                       ; xrpnt+=step
jmp   L$529                         ; }while(1)
L$533:                              ;

mov  esp,ebp
pop  ebp
pop  edi
pop  esi
ret

;---------------------------------------------------------------
III_deq_nop:                        ; do not remove!
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

mpxdec_l3deq_dequantize:
_mpxdec_l3deq_dequantize:
push eax
push edx
push ebx
push ecx
mov eax,dword ptr [esp+ 4+16];
mov edx,dword ptr [esp+ 8+16];
mov ebx,dword ptr [esp+12+16];
mov ecx,dword ptr [esp+16+16];
call mpxdec_l3deq_dequantize_
pop ecx
pop ebx
pop edx
pop eax
ret

mpxdec_l3deq_dequantize_ms:
_mpxdec_l3deq_dequantize_ms:
push eax
push edx
push ebx
push ecx
mov eax,dword ptr [esp+ 4+16];
mov edx,dword ptr [esp+ 8+16];
mov ebx,dword ptr [esp+12+16];
mov ecx,dword ptr [esp+16+16];
call mpxdec_l3deq_dequantize_ms_
pop ecx
pop ebx
pop edx
pop eax
ret

mpxdec_l3deq_dequantize_bt2:
_mpxdec_l3deq_dequantize_bt2:
push eax
push edx
push ebx
push ecx
mov eax,dword ptr [esp+ 4+16];
mov edx,dword ptr [esp+ 8+16];
mov ebx,dword ptr [esp+12+16];
mov ecx,dword ptr [esp+16+16];
call mpxdec_l3deq_dequantize_bt2_
pop ecx
pop ebx
pop edx
pop eax
ret

mpxdec_l3deq_dequantize_ms_bt2:
_mpxdec_l3deq_dequantize_ms_bt2:
push eax
push edx
push ebx
push ecx
mov eax,dword ptr [esp+ 4+16];
mov edx,dword ptr [esp+ 8+16];
mov ebx,dword ptr [esp+12+16];
mov ecx,dword ptr [esp+16+16];
call mpxdec_l3deq_dequantize_ms_bt2_
pop ecx
pop ebx
pop edx
pop eax
ret

mpxdec_l3deq_dequantize_bt2_js:
_mpxdec_l3deq_dequantize_bt2_js:
push eax
push edx
push ebx
push ecx
mov eax,dword ptr [esp+ 4+16];
mov edx,dword ptr [esp+ 8+16];
mov ebx,dword ptr [esp+12+16];
mov ecx,dword ptr [esp+16+16];
call mpxdec_l3deq_dequantize_bt2_js_
pop ecx
pop ebx
pop edx
pop eax
ret

mpxdec_l3deq_dequantize_ms_bt2_js:
_mpxdec_l3deq_dequantize_ms_bt2_js:
push eax
push edx
push ebx
push ecx
mov eax,dword ptr [esp+ 4+16];
mov edx,dword ptr [esp+ 8+16];
mov ebx,dword ptr [esp+12+16];
mov ecx,dword ptr [esp+16+16];
call mpxdec_l3deq_dequantize_ms_bt2_js_
pop ecx
pop ebx
pop edx
pop eax
ret

_TEXT		ENDS
CONST		SEGMENT	DWORD PUBLIC USE32 'DATA'
CONST		ENDS
CONST2		SEGMENT	DWORD PUBLIC USE32 'DATA'
CONST2		ENDS
_DATA		SEGMENT	DWORD PUBLIC USE32 'DATA'
_htab0:
    DB	00H, 00H
_htab1:
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 11H, 00H
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab2:
    DB	0e2H, 0ffH, 0eaH, 0ffH, 0eeH, 0ffH, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 22H, 00H, 02H, 00H
    DB	12H, 00H, 0feH, 0ffH, 21H, 00H, 20H, 00H
    DB	11H, 00H, 0feH, 0ffH, 01H, 00H, 10H, 00H
    DB	00H, 00H
_htab3:
    DB	0e6H, 0ffH, 0eaH, 0ffH, 0eeH, 0ffH, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 22H, 00H, 02H, 00H
    DB	12H, 00H, 0feH, 0ffH, 21H, 00H, 20H, 00H
    DB	10H, 00H, 11H, 00H, 0feH, 0ffH, 01H, 00H
    DB	00H, 00H
_htab5:
    DB	0c6H, 0ffH, 0ceH, 0ffH, 0d2H, 0ffH, 0e2H, 0ffH
    DB	0f2H, 0ffH, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	33H, 00H, 23H, 00H, 32H, 00H, 31H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 13H, 00H, 03H, 00H
    DB	0feH, 0ffH, 30H, 00H, 22H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 12H, 00H, 21H, 00H, 0feH, 0ffH
    DB	02H, 00H, 20H, 00H, 11H, 00H, 0feH, 0ffH
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab6:
    DB	0ceH, 0ffH, 0daH, 0ffH, 0e6H, 0ffH, 0eeH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 33H, 00H
    DB	03H, 00H, 23H, 00H, 0feH, 0ffH, 32H, 00H
    DB	30H, 00H, 0feH, 0ffH, 13H, 00H, 31H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 22H, 00H, 02H, 00H
    DB	12H, 00H, 0faH, 0ffH, 0feH, 0ffH, 21H, 00H
    DB	20H, 00H, 01H, 00H, 0feH, 0ffH, 11H, 00H
    DB	0feH, 0ffH, 10H, 00H, 00H, 00H
_htab7:
    DB	76H, 0ffH, 7eH, 0ffH, 8eH, 0ffH, 0b2H, 0ffH
    DB	0c6H, 0ffH, 0deH, 0ffH, 0eaH, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 55H, 00H, 45H, 00H
    DB	0feH, 0ffH, 54H, 00H, 53H, 00H, 0feH, 0ffH
    DB	35H, 00H, 44H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	25H, 00H, 52H, 00H, 15H, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 51H, 00H, 0feH, 0ffH, 05H, 00H
    DB	34H, 00H, 0feH, 0ffH, 50H, 00H, 0feH, 0ffH
    DB	43H, 00H, 33H, 00H, 0f6H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 24H, 00H, 42H, 00H, 14H, 00H
    DB	0feH, 0ffH, 41H, 00H, 40H, 00H, 0eaH, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 04H, 00H
    DB	23H, 00H, 0feH, 0ffH, 32H, 00H, 03H, 00H
    DB	0feH, 0ffH, 13H, 00H, 31H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 30H, 00H, 22H, 00H, 12H, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 21H, 00H, 0feH, 0ffH
    DB	02H, 00H, 20H, 00H, 11H, 00H, 0feH, 0ffH
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab8:
    DB	7eH, 0ffH, 82H, 0ffH, 8aH, 0ffH, 0a6H, 0ffH
    DB	0c2H, 0ffH, 0daH, 0ffH, 0e6H, 0ffH, 0f2H, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 55H, 00H
    DB	54H, 00H, 45H, 00H, 53H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 35H, 00H, 44H, 00H, 25H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 52H, 00H, 05H, 00H
    DB	15H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 51H, 00H
    DB	0feH, 0ffH, 34H, 00H, 43H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 50H, 00H, 33H, 00H, 24H, 00H
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 42H, 00H
    DB	14H, 00H, 41H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	04H, 00H, 40H, 00H, 0feH, 0ffH, 23H, 00H
    DB	32H, 00H, 0eeH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 13H, 00H, 31H, 00H, 0feH, 0ffH
    DB	03H, 00H, 30H, 00H, 22H, 00H, 0feH, 0ffH
    DB	02H, 00H, 20H, 00H, 0feH, 0ffH, 12H, 00H
    DB	21H, 00H, 11H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab9:
    DB	82H, 0ffH, 96H, 0ffH, 0aeH, 0ffH, 0c6H, 0ffH
    DB	0daH, 0ffH, 0eaH, 0ffH, 0f6H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 55H, 00H, 45H, 00H, 35H, 00H
    DB	0feH, 0ffH, 53H, 00H, 0feH, 0ffH, 54H, 00H
    DB	05H, 00H, 0faH, 0ffH, 0feH, 0ffH, 44H, 00H
    DB	25H, 00H, 0feH, 0ffH, 52H, 00H, 15H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 51H, 00H, 34H, 00H
    DB	0feH, 0ffH, 43H, 00H, 0feH, 0ffH, 50H, 00H
    DB	04H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	24H, 00H, 42H, 00H, 0feH, 0ffH, 33H, 00H
    DB	40H, 00H, 0feH, 0ffH, 14H, 00H, 41H, 00H
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 23H, 00H
    DB	32H, 00H, 13H, 00H, 0feH, 0ffH, 31H, 00H
    DB	0feH, 0ffH, 03H, 00H, 30H, 00H, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 22H, 00H, 02H, 00H
    DB	12H, 00H, 0feH, 0ffH, 21H, 00H, 20H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 11H, 00H, 01H, 00H
    DB	0feH, 0ffH, 10H, 00H, 00H, 00H
_htab10:
    DB	06H, 0ffH, 0eH, 0ffH, 22H, 0ffH, 5aH, 0ffH
    DB	92H, 0ffH, 0baH, 0ffH, 0d6H, 0ffH, 0e6H, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 77H, 00H
    DB	67H, 00H, 0feH, 0ffH, 76H, 00H, 57H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 75H, 00H, 66H, 00H
    DB	47H, 00H, 0faH, 0ffH, 0feH, 0ffH, 74H, 00H
    DB	56H, 00H, 0feH, 0ffH, 65H, 00H, 37H, 00H
    DB	0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 73H, 00H
    DB	46H, 00H, 0faH, 0ffH, 0feH, 0ffH, 55H, 00H
    DB	54H, 00H, 63H, 00H, 0feH, 0ffH, 27H, 00H
    DB	72H, 00H, 0eaH, 0ffH, 0f6H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 64H, 00H, 07H, 00H, 70H, 00H
    DB	0feH, 0ffH, 62H, 00H, 0feH, 0ffH, 45H, 00H
    DB	35H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 06H, 00H
    DB	0feH, 0ffH, 53H, 00H, 44H, 00H, 17H, 00H
    DB	0deH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH, 71H, 00H
    DB	0feH, 0ffH, 36H, 00H, 26H, 00H, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 25H, 00H, 52H, 00H
    DB	15H, 00H, 0feH, 0ffH, 51H, 00H, 0feH, 0ffH
    DB	34H, 00H, 43H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	16H, 00H, 61H, 00H, 0feH, 0ffH, 60H, 00H
    DB	0feH, 0ffH, 05H, 00H, 50H, 00H, 0daH, 0ffH
    DB	0eaH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	24H, 00H, 42H, 00H, 0feH, 0ffH, 33H, 00H
    DB	04H, 00H, 0feH, 0ffH, 14H, 00H, 41H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 40H, 00H, 23H, 00H
    DB	0feH, 0ffH, 32H, 00H, 03H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 13H, 00H, 31H, 00H, 0feH, 0ffH
    DB	30H, 00H, 22H, 00H, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 12H, 00H, 21H, 00H, 0feH, 0ffH
    DB	02H, 00H, 20H, 00H, 11H, 00H, 0feH, 0ffH
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab11:
    DB	0eH, 0ffH, 1eH, 0ffH, 4eH, 0ffH, 8aH, 0ffH
    DB	0aaH, 0ffH, 0caH, 0ffH, 0deH, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 77H, 00H, 67H, 00H
    DB	0feH, 0ffH, 76H, 00H, 75H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 66H, 00H, 47H, 00H, 0feH, 0ffH
    DB	74H, 00H, 0feH, 0ffH, 57H, 00H, 55H, 00H
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 56H, 00H
    DB	65H, 00H, 37H, 00H, 0feH, 0ffH, 73H, 00H
    DB	46H, 00H, 0eeH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 45H, 00H, 54H, 00H, 0feH, 0ffH
    DB	35H, 00H, 53H, 00H, 27H, 00H, 0feH, 0ffH
    DB	72H, 00H, 0feH, 0ffH, 64H, 00H, 07H, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 71H, 00H, 0feH, 0ffH
    DB	17H, 00H, 70H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	36H, 00H, 63H, 00H, 0feH, 0ffH, 60H, 00H
    DB	0feH, 0ffH, 44H, 00H, 25H, 00H, 0e6H, 0ffH
    DB	0f2H, 0ffH, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	52H, 00H, 05H, 00H, 15H, 00H, 62H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 26H, 00H, 06H, 00H
    DB	16H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 61H, 00H
    DB	0feH, 0ffH, 51H, 00H, 34H, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 50H, 00H, 0feH, 0ffH, 43H, 00H
    DB	33H, 00H, 0feH, 0ffH, 24H, 00H, 42H, 00H
    DB	0e2H, 0ffH, 0eaH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 14H, 00H, 41H, 00H, 0feH, 0ffH
    DB	04H, 00H, 40H, 00H, 0feH, 0ffH, 23H, 00H
    DB	32H, 00H, 0feH, 0ffH, 13H, 00H, 31H, 00H
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 03H, 00H
    DB	30H, 00H, 22H, 00H, 21H, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 12H, 00H, 0feH, 0ffH, 02H, 00H
    DB	20H, 00H, 11H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab12:
    DB	1aH, 0ffH, 3aH, 0ffH, 6eH, 0ffH, 0a6H, 0ffH
    DB	0caH, 0ffH, 0deH, 0ffH, 0eeH, 0ffH, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 77H, 00H, 67H, 00H
    DB	76H, 00H, 0feH, 0ffH, 57H, 00H, 75H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 66H, 00H, 47H, 00H
    DB	0feH, 0ffH, 74H, 00H, 65H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 56H, 00H, 37H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 73H, 00H, 55H, 00H, 27H, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 72H, 00H
    DB	46H, 00H, 0feH, 0ffH, 64H, 00H, 17H, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 71H, 00H, 0feH, 0ffH
    DB	07H, 00H, 70H, 00H, 0feH, 0ffH, 36H, 00H
    DB	63H, 00H, 0e6H, 0ffH, 0eeH, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 45H, 00H, 54H, 00H, 0feH, 0ffH
    DB	44H, 00H, 0feH, 0ffH, 06H, 00H, 05H, 00H
    DB	0feH, 0ffH, 26H, 00H, 62H, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 61H, 00H, 0feH, 0ffH, 16H, 00H
    DB	60H, 00H, 0faH, 0ffH, 0feH, 0ffH, 35H, 00H
    DB	53H, 00H, 0feH, 0ffH, 25H, 00H, 52H, 00H
    DB	0deH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	15H, 00H, 51H, 00H, 0feH, 0ffH, 34H, 00H
    DB	43H, 00H, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	50H, 00H, 04H, 00H, 24H, 00H, 0feH, 0ffH
    DB	42H, 00H, 14H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	33H, 00H, 41H, 00H, 0feH, 0ffH, 23H, 00H
    DB	32H, 00H, 0eaH, 0ffH, 0f2H, 0ffH, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 40H, 00H, 03H, 00H
    DB	30H, 00H, 13H, 00H, 0feH, 0ffH, 31H, 00H
    DB	22H, 00H, 0feH, 0ffH, 12H, 00H, 21H, 00H
    DB	0f2H, 0ffH, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	02H, 00H, 20H, 00H, 00H, 00H, 11H, 00H
    DB	0feH, 0ffH, 01H, 00H, 10H, 00H
_htab13:
    DB	06H, 0fcH, 12H, 0fcH, 4aH, 0fcH, 0d6H, 0fcH
    DB	66H, 0fdH, 0eeH, 0fdH, 66H, 0feH, 0ceH, 0feH
    DB	1aH, 0ffH, 5aH, 0ffH, 96H, 0ffH, 0baH, 0ffH
    DB	0d6H, 0ffH, 0e6H, 0ffH, 0eeH, 0ffH, 0f2H, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0feH, 00H
    DB	0fcH, 00H, 0fdH, 00H, 0edH, 00H, 0ffH, 00H
    DB	0feH, 0ffH, 0efH, 00H, 0dfH, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 0eeH, 00H, 0cfH, 00H, 0feH, 0ffH
    DB	0deH, 00H, 0bfH, 00H, 0eeH, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 0fbH, 00H, 0ceH, 00H, 0feH, 0ffH
    DB	0dcH, 00H, 0feH, 0ffH, 0afH, 00H, 0e9H, 00H
    DB	0feH, 0ffH, 0ecH, 00H, 0ddH, 00H, 0eeH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0faH, 00H
    DB	0cdH, 00H, 0beH, 00H, 0feH, 0ffH, 0ebH, 00H
    DB	9fH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0f9H, 00H
    DB	0eaH, 00H, 0feH, 0ffH, 0bdH, 00H, 0dbH, 00H
    DB	0deH, 0ffH, 0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	8fH, 00H, 0f8H, 00H, 0feH, 0ffH, 0ccH, 00H
    DB	0feH, 0ffH, 0aeH, 00H, 9eH, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 8eH, 00H, 0feH, 0ffH, 7fH, 00H
    DB	7eH, 00H, 0f7H, 00H, 0f6H, 0ffH, 0feH, 0ffH
    DB	0daH, 00H, 0feH, 0ffH, 0adH, 00H, 0bcH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0cbH, 00H, 0f6H, 00H
    DB	6fH, 00H, 0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 0e8H, 00H, 5fH, 00H, 0feH, 0ffH
    DB	9dH, 00H, 0d9H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0f5H, 00H, 0e7H, 00H, 0feH, 0ffH, 0acH, 00H
    DB	0bbH, 00H, 0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	4fH, 00H, 0f4H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0caH, 00H, 0e6H, 00H, 0f3H, 00H, 0feH, 0ffH
    DB	3fH, 00H, 0feH, 0ffH, 8dH, 00H, 0d8H, 00H
    DB	0d6H, 0ffH, 0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	2fH, 00H, 0f2H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	6eH, 00H, 9cH, 00H, 0fH, 00H, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0c9H, 00H, 5eH, 00H
    DB	0abH, 00H, 0faH, 0ffH, 0feH, 0ffH, 7dH, 00H
    DB	0d7H, 00H, 4eH, 00H, 0eaH, 0ffH, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0c8H, 00H, 0d6H, 00H
    DB	3eH, 00H, 0feH, 0ffH, 0b9H, 00H, 0feH, 0ffH
    DB	9bH, 00H, 0aaH, 00H, 0feH, 0ffH, 1fH, 00H
    DB	0f1H, 00H, 0d2H, 0ffH, 0e6H, 0ffH, 0f6H, 0ffH
    DB	0feH, 0ffH, 0f0H, 00H, 0feH, 0ffH, 0baH, 00H
    DB	0e5H, 00H, 0faH, 0ffH, 0feH, 0ffH, 0e4H, 00H
    DB	8cH, 00H, 0feH, 0ffH, 6dH, 00H, 0e3H, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 0e2H, 00H, 0feH, 0ffH
    DB	2eH, 00H, 0eH, 00H, 0feH, 0ffH, 1eH, 00H
    DB	0e1H, 00H, 0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 0e0H, 00H, 5dH, 00H, 0feH, 0ffH
    DB	0d5H, 00H, 7cH, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0c7H, 00H, 4dH, 00H, 0feH, 0ffH, 8bH, 00H
    DB	0b8H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0d4H, 00H, 9aH, 00H, 0feH, 0ffH, 0a9H, 00H
    DB	6cH, 00H, 0feH, 0ffH, 0c6H, 00H, 3dH, 00H
    DB	0b6H, 0ffH, 0d6H, 0ffH, 0eeH, 0ffH, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0d3H, 00H, 7bH, 00H
    DB	2dH, 00H, 0feH, 0ffH, 0d2H, 00H, 1dH, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 0b7H, 00H, 0feH, 0ffH
    DB	5cH, 00H, 0c5H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	99H, 00H, 7aH, 00H, 0c3H, 00H, 0f2H, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0a7H, 00H
    DB	97H, 00H, 4bH, 00H, 0d1H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 0dH, 00H, 0d0H, 00H, 0feH, 0ffH
    DB	8aH, 00H, 0a8H, 00H, 0eaH, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 4cH, 00H, 0c4H, 00H
    DB	0feH, 0ffH, 6bH, 00H, 0b6H, 00H, 0feH, 0ffH
    DB	3cH, 00H, 2cH, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0c2H, 00H, 5bH, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0b5H, 00H, 89H, 00H, 1cH, 00H, 0aaH, 0ffH
    DB	0d2H, 0ffH, 0eaH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH
    DB	0c1H, 00H, 0feH, 0ffH, 98H, 00H, 0cH, 00H
    DB	0feH, 0ffH, 0c0H, 00H, 0feH, 0ffH, 0b4H, 00H
    DB	6aH, 00H, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0a6H, 00H, 79H, 00H, 3bH, 00H, 0feH, 0ffH
    DB	0b3H, 00H, 0feH, 0ffH, 88H, 00H, 5aH, 00H
    DB	0eaH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH, 2bH, 00H
    DB	0feH, 0ffH, 0a5H, 00H, 69H, 00H, 0feH, 0ffH
    DB	0a4H, 00H, 0feH, 0ffH, 78H, 00H, 87H, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 94H, 00H, 0feH, 0ffH
    DB	77H, 00H, 76H, 00H, 0b2H, 00H, 0eaH, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 1bH, 00H, 0b1H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0bH, 00H, 0b0H, 00H
    DB	0feH, 0ffH, 96H, 00H, 4aH, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 3aH, 00H, 0a3H, 00H
    DB	0feH, 0ffH, 59H, 00H, 95H, 00H, 0feH, 0ffH
    DB	2aH, 00H, 0a2H, 00H, 0a2H, 0ffH, 0d2H, 0ffH
    DB	0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 1aH, 00H
    DB	0a1H, 00H, 0faH, 0ffH, 0feH, 0ffH, 0aH, 00H
    DB	68H, 00H, 0a0H, 00H, 0f6H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 86H, 00H, 49H, 00H, 93H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 39H, 00H, 58H, 00H
    DB	0feH, 0ffH, 85H, 00H, 67H, 00H, 0eeH, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 29H, 00H, 92H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 57H, 00H, 75H, 00H
    DB	38H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 83H, 00H
    DB	0feH, 0ffH, 66H, 00H, 47H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 74H, 00H, 56H, 00H, 0feH, 0ffH
    DB	65H, 00H, 73H, 00H, 0eaH, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 19H, 00H, 91H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 09H, 00H, 90H, 00H, 0feH, 0ffH
    DB	48H, 00H, 84H, 00H, 0f2H, 0ffH, 0f6H, 0ffH
    DB	0feH, 0ffH, 72H, 00H, 0feH, 0ffH, 46H, 00H
    DB	64H, 00H, 28H, 00H, 0feH, 0ffH, 82H, 00H
    DB	18H, 00H, 0aeH, 0ffH, 0caH, 0ffH, 0eaH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 37H, 00H
    DB	27H, 00H, 17H, 00H, 0feH, 0ffH, 71H, 00H
    DB	0feH, 0ffH, 55H, 00H, 07H, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 70H, 00H, 36H, 00H
    DB	0feH, 0ffH, 63H, 00H, 45H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 54H, 00H, 26H, 00H, 0feH, 0ffH
    DB	62H, 00H, 35H, 00H, 0f6H, 0ffH, 0feH, 0ffH
    DB	81H, 00H, 0feH, 0ffH, 08H, 00H, 80H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 16H, 00H, 61H, 00H
    DB	0feH, 0ffH, 06H, 00H, 60H, 00H, 0e6H, 0ffH
    DB	0eeH, 0ffH, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	53H, 00H, 44H, 00H, 25H, 00H, 0feH, 0ffH
    DB	52H, 00H, 05H, 00H, 0feH, 0ffH, 15H, 00H
    DB	51H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	34H, 00H, 43H, 00H, 0feH, 0ffH, 50H, 00H
    DB	24H, 00H, 0faH, 0ffH, 0feH, 0ffH, 42H, 00H
    DB	33H, 00H, 14H, 00H, 0daH, 0ffH, 0eaH, 0ffH
    DB	0f6H, 0ffH, 0feH, 0ffH, 41H, 00H, 0feH, 0ffH
    DB	04H, 00H, 40H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	23H, 00H, 32H, 00H, 13H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 31H, 00H, 03H, 00H, 0feH, 0ffH
    DB	30H, 00H, 22H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	12H, 00H, 21H, 00H, 0feH, 0ffH, 02H, 00H
    DB	20H, 00H, 0faH, 0ffH, 0feH, 0ffH, 11H, 00H
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab15:
    DB	22H, 0fcH, 86H, 0fcH, 3aH, 0fdH, 0f2H, 0fdH
    DB	92H, 0feH, 1aH, 0ffH, 66H, 0ffH, 0aaH, 0ffH
    DB	0caH, 0ffH, 0e6H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 0ffH, 00H, 0efH, 00H, 0feH, 0ffH
    DB	0feH, 00H, 0dfH, 00H, 0feH, 0ffH, 0eeH, 00H
    DB	0feH, 0ffH, 0fdH, 00H, 0cfH, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0fcH, 00H, 0deH, 00H
    DB	0feH, 0ffH, 0edH, 00H, 0bfH, 00H, 0feH, 0ffH
    DB	0fbH, 00H, 0feH, 0ffH, 0ceH, 00H, 0ecH, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0ddH, 00H
    DB	0afH, 00H, 0feH, 0ffH, 0faH, 00H, 0beH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0ebH, 00H, 0cdH, 00H
    DB	0feH, 0ffH, 0dcH, 00H, 9fH, 00H, 0e2H, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0f9H, 00H
    DB	0eaH, 00H, 0feH, 0ffH, 0bdH, 00H, 0dbH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 8fH, 00H, 0f8H, 00H
    DB	0feH, 0ffH, 0ccH, 00H, 9eH, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0e9H, 00H, 7fH, 00H
    DB	0feH, 0ffH, 0f7H, 00H, 0adH, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 0daH, 00H, 0bcH, 00H, 0feH, 0ffH
    DB	6fH, 00H, 0feH, 0ffH, 0aeH, 00H, 0fH, 00H
    DB	0daH, 0ffH, 0eaH, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0cbH, 00H, 0f6H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	8eH, 00H, 0e8H, 00H, 0feH, 0ffH, 5fH, 00H
    DB	9dH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0f5H, 00H
    DB	7eH, 00H, 0feH, 0ffH, 0e7H, 00H, 0acH, 00H
    DB	0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0caH, 00H
    DB	0bbH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0d9H, 00H
    DB	8dH, 00H, 4fH, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0f4H, 00H, 3fH, 00H, 0feH, 0ffH, 0f3H, 00H
    DB	0d8H, 00H, 0beH, 0ffH, 0deH, 0ffH, 0eeH, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0e6H, 00H, 2fH, 00H
    DB	0feH, 0ffH, 0f2H, 00H, 0feH, 0ffH, 6eH, 00H
    DB	0f0H, 00H, 0faH, 0ffH, 0feH, 0ffH, 1fH, 00H
    DB	0f1H, 00H, 0feH, 0ffH, 9cH, 00H, 0c9H, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 5eH, 00H
    DB	0abH, 00H, 0feH, 0ffH, 0baH, 00H, 0e5H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 7dH, 00H, 0d7H, 00H
    DB	0feH, 0ffH, 4eH, 00H, 0e4H, 00H, 0e2H, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 8cH, 00H
    DB	0c8H, 00H, 0feH, 0ffH, 3eH, 00H, 6dH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0d6H, 00H, 0e3H, 00H
    DB	0feH, 0ffH, 9bH, 00H, 0b9H, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 2eH, 00H, 0aaH, 00H
    DB	0feH, 0ffH, 0e2H, 00H, 1eH, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 0e1H, 00H, 0feH, 0ffH, 0eH, 00H
    DB	0e0H, 00H, 0feH, 0ffH, 5dH, 00H, 0d5H, 00H
    DB	0a6H, 0ffH, 0ceH, 0ffH, 0e6H, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 7cH, 00H, 0c7H, 00H
    DB	0feH, 0ffH, 4dH, 00H, 8bH, 00H, 0feH, 0ffH
    DB	0d4H, 00H, 0feH, 0ffH, 0b8H, 00H, 9aH, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0a9H, 00H
    DB	6cH, 00H, 0feH, 0ffH, 0c6H, 00H, 3dH, 00H
    DB	0feH, 0ffH, 0d3H, 00H, 0d2H, 00H, 0eeH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 2dH, 00H
    DB	0dH, 00H, 1dH, 00H, 0feH, 0ffH, 7bH, 00H
    DB	0b7H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 0d1H, 00H
    DB	0feH, 0ffH, 5cH, 00H, 0d0H, 00H, 0feH, 0ffH
    DB	0c5H, 00H, 8aH, 00H, 0deH, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0a8H, 00H, 4cH, 00H
    DB	0feH, 0ffH, 0c4H, 00H, 6bH, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 0b6H, 00H, 0feH, 0ffH, 99H, 00H
    DB	0cH, 00H, 0feH, 0ffH, 3cH, 00H, 0c3H, 00H
    DB	0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 7aH, 00H
    DB	0a7H, 00H, 0feH, 0ffH, 0a6H, 00H, 0feH, 0ffH
    DB	0c0H, 00H, 0bH, 00H, 0feH, 0ffH, 0c2H, 00H
    DB	0feH, 0ffH, 2cH, 00H, 5bH, 00H, 92H, 0ffH
    DB	0c6H, 0ffH, 0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 0b5H, 00H, 1cH, 00H, 0feH, 0ffH
    DB	89H, 00H, 98H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0c1H, 00H, 4bH, 00H, 0feH, 0ffH, 0b4H, 00H
    DB	6aH, 00H, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	3bH, 00H, 79H, 00H, 0b3H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 97H, 00H, 88H, 00H, 0feH, 0ffH
    DB	2bH, 00H, 5aH, 00H, 0eaH, 0ffH, 0f6H, 0ffH
    DB	0feH, 0ffH, 0b2H, 00H, 0feH, 0ffH, 0a5H, 00H
    DB	1bH, 00H, 0feH, 0ffH, 0b1H, 00H, 0feH, 0ffH
    DB	0b0H, 00H, 69H, 00H, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 96H, 00H, 4aH, 00H, 0feH, 0ffH
    DB	0a4H, 00H, 78H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	87H, 00H, 3aH, 00H, 0a3H, 00H, 0deH, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 59H, 00H
    DB	95H, 00H, 0feH, 0ffH, 2aH, 00H, 0a2H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 1aH, 00H, 0a1H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0aH, 00H, 0a0H, 00H
    DB	68H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	86H, 00H, 49H, 00H, 0feH, 0ffH, 94H, 00H
    DB	39H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 93H, 00H
    DB	0feH, 0ffH, 77H, 00H, 09H, 00H, 0feH, 0ffH
    DB	58H, 00H, 85H, 00H, 96H, 0ffH, 0c6H, 0ffH
    DB	0e6H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	29H, 00H, 67H, 00H, 0feH, 0ffH, 76H, 00H
    DB	92H, 00H, 0feH, 0ffH, 91H, 00H, 0feH, 0ffH
    DB	19H, 00H, 90H, 00H, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 48H, 00H, 84H, 00H, 0feH, 0ffH
    DB	57H, 00H, 75H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	38H, 00H, 83H, 00H, 0feH, 0ffH, 66H, 00H
    DB	47H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	28H, 00H, 82H, 00H, 0feH, 0ffH, 18H, 00H
    DB	81H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	74H, 00H, 08H, 00H, 0feH, 0ffH, 80H, 00H
    DB	56H, 00H, 0faH, 0ffH, 0feH, 0ffH, 65H, 00H
    DB	37H, 00H, 0feH, 0ffH, 73H, 00H, 46H, 00H
    DB	0deH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	27H, 00H, 72H, 00H, 0feH, 0ffH, 64H, 00H
    DB	17H, 00H, 0faH, 0ffH, 0feH, 0ffH, 55H, 00H
    DB	71H, 00H, 0faH, 0ffH, 0feH, 0ffH, 07H, 00H
    DB	70H, 00H, 36H, 00H, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 63H, 00H, 45H, 00H, 0feH, 0ffH
    DB	54H, 00H, 26H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	62H, 00H, 16H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	06H, 00H, 60H, 00H, 35H, 00H, 0beH, 0ffH
    DB	0daH, 0ffH, 0eeH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH
    DB	61H, 00H, 0feH, 0ffH, 53H, 00H, 44H, 00H
    DB	0feH, 0ffH, 25H, 00H, 52H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 15H, 00H, 51H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 05H, 00H, 50H, 00H, 34H, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 43H, 00H
    DB	24H, 00H, 0feH, 0ffH, 42H, 00H, 33H, 00H
    DB	0feH, 0ffH, 41H, 00H, 0feH, 0ffH, 14H, 00H
    DB	04H, 00H, 0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	23H, 00H, 32H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	40H, 00H, 03H, 00H, 13H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 31H, 00H, 30H, 00H, 22H, 00H
    DB	0eeH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	12H, 00H, 21H, 00H, 0feH, 0ffH, 02H, 00H
    DB	20H, 00H, 11H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab16:
    DB	06H, 0fcH, 12H, 0fcH, 66H, 0fcH, 7aH, 0fdH
    DB	32H, 0ffH, 0b6H, 0ffH, 0caH, 0ffH, 0e2H, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0efH, 00H
    DB	0feH, 00H, 0feH, 0ffH, 0dfH, 00H, 0fdH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0cfH, 00H, 0fcH, 00H
    DB	0feH, 0ffH, 0bfH, 00H, 0fbH, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 0afH, 00H, 0feH, 0ffH, 0faH, 00H
    DB	9fH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0f9H, 00H
    DB	0f8H, 00H, 8fH, 00H, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 7fH, 00H, 0f7H, 00H, 0feH, 0ffH
    DB	6fH, 00H, 0f6H, 00H, 0ffH, 00H, 0eeH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 5fH, 00H
    DB	0f5H, 00H, 4fH, 00H, 0feH, 0ffH, 0f4H, 00H
    DB	0f3H, 00H, 96H, 0ffH, 0feH, 0ffH, 0f0H, 00H
    DB	0feH, 0ffH, 3fH, 00H, 0c6H, 0ffH, 0daH, 0ffH
    DB	0e6H, 0ffH, 0f2H, 0ffH, 0f6H, 0ffH, 0feH, 0ffH
    DB	0ceH, 00H, 0feH, 0ffH, 0ecH, 00H, 0ddH, 00H
    DB	0deH, 00H, 0feH, 0ffH, 0e9H, 00H, 0feH, 0ffH
    DB	0eaH, 00H, 0d9H, 00H, 0feH, 0ffH, 0eeH, 00H
    DB	0feH, 0ffH, 0edH, 00H, 0ebH, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 0beH, 00H, 0cdH, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 0dcH, 00H, 0dbH, 00H, 0aeH, 00H
    DB	0eaH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH, 0ccH, 00H
    DB	0feH, 0ffH, 0adH, 00H, 0daH, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 7eH, 00H, 0acH, 00H, 0caH, 00H
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0c9H, 00H
    DB	7dH, 00H, 5eH, 00H, 0bdH, 00H, 0f2H, 00H
    DB	46H, 0ffH, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	2fH, 00H, 0fH, 00H, 1fH, 00H, 0feH, 0ffH
    DB	0f1H, 00H, 9eH, 0ffH, 0ceH, 0ffH, 0e6H, 0ffH
    DB	0f6H, 0ffH, 0feH, 0ffH, 9eH, 00H, 0feH, 0ffH
    DB	0bcH, 00H, 0cbH, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	8eH, 00H, 0e8H, 00H, 0feH, 0ffH, 9dH, 00H
    DB	0e7H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0bbH, 00H, 8dH, 00H, 0feH, 0ffH, 0d8H, 00H
    DB	6eH, 00H, 0feH, 0ffH, 0e6H, 00H, 9cH, 00H
    DB	0e6H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0abH, 00H, 0baH, 00H, 0feH, 0ffH, 0e5H, 00H
    DB	0d7H, 00H, 0feH, 0ffH, 4eH, 00H, 0feH, 0ffH
    DB	0e4H, 00H, 8cH, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	0c8H, 00H, 3eH, 00H, 0feH, 0ffH, 6dH, 00H
    DB	0feH, 0ffH, 0d6H, 00H, 9bH, 00H, 0daH, 0ffH
    DB	0eaH, 0ffH, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0b9H, 00H, 0aaH, 00H, 0e1H, 00H, 0feH, 0ffH
    DB	0d4H, 00H, 0feH, 0ffH, 0b8H, 00H, 0a9H, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 7bH, 00H, 0feH, 0ffH
    DB	0b7H, 00H, 0d0H, 00H, 0e3H, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0eH, 00H, 0e0H, 00H
    DB	0feH, 0ffH, 5dH, 00H, 0d5H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 7cH, 00H, 0c7H, 00H, 0feH, 0ffH
    DB	4dH, 00H, 8bH, 00H, 6aH, 0ffH, 0a6H, 0ffH
    DB	0caH, 0ffH, 0e6H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 9aH, 00H, 6cH, 00H, 0feH, 0ffH
    DB	0c6H, 00H, 3dH, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	5cH, 00H, 0c5H, 00H, 0dH, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 8aH, 00H, 0a8H, 00H
    DB	0feH, 0ffH, 99H, 00H, 4cH, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 0b6H, 00H, 7aH, 00H, 3cH, 00H
    DB	0eaH, 0ffH, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	5bH, 00H, 89H, 00H, 1cH, 00H, 0feH, 0ffH
    DB	0c0H, 00H, 0feH, 0ffH, 98H, 00H, 79H, 00H
    DB	0feH, 0ffH, 0e2H, 00H, 0feH, 0ffH, 2eH, 00H
    DB	1eH, 00H, 0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 0d3H, 00H, 2dH, 00H, 0feH, 0ffH
    DB	0d2H, 00H, 0d1H, 00H, 0f6H, 0ffH, 0feH, 0ffH
    DB	3bH, 00H, 0feH, 0ffH, 97H, 00H, 88H, 00H
    DB	1dH, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0c4H, 00H, 6bH, 00H, 0feH, 0ffH, 0c3H, 00H
    DB	0a7H, 00H, 0feH, 0ffH, 2cH, 00H, 0feH, 0ffH
    DB	0c2H, 00H, 0b5H, 00H, 0d2H, 0ffH, 0e6H, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0c1H, 00H
    DB	0cH, 00H, 0feH, 0ffH, 4bH, 00H, 0b4H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 6aH, 00H, 0a6H, 00H
    DB	0b3H, 00H, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	5aH, 00H, 0a5H, 00H, 2bH, 00H, 0feH, 0ffH
    DB	0b2H, 00H, 1bH, 00H, 0e6H, 0ffH, 0f6H, 0ffH
    DB	0feH, 0ffH, 0b1H, 00H, 0feH, 0ffH, 0bH, 00H
    DB	0b0H, 00H, 0faH, 0ffH, 0feH, 0ffH, 69H, 00H
    DB	96H, 00H, 0feH, 0ffH, 4aH, 00H, 0a4H, 00H
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 78H, 00H
    DB	87H, 00H, 0a3H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	3aH, 00H, 59H, 00H, 2aH, 00H, 3eH, 0ffH
    DB	8eH, 0ffH, 0beH, 0ffH, 0daH, 0ffH, 0eaH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 95H, 00H
    DB	68H, 00H, 0a1H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	86H, 00H, 77H, 00H, 94H, 00H, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 49H, 00H, 57H, 00H
    DB	67H, 00H, 0a2H, 00H, 0f6H, 0ffH, 0feH, 0ffH
    DB	1aH, 00H, 0feH, 0ffH, 0aH, 00H, 0a0H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 39H, 00H, 93H, 00H
    DB	0feH, 0ffH, 58H, 00H, 85H, 00H, 0eeH, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 29H, 00H, 92H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 76H, 00H, 09H, 00H
    DB	19H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 91H, 00H
    DB	0feH, 0ffH, 90H, 00H, 48H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 84H, 00H, 75H, 00H, 0feH, 0ffH
    DB	38H, 00H, 83H, 00H, 0d6H, 0ffH, 0eaH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 66H, 00H
    DB	28H, 00H, 82H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	47H, 00H, 74H, 00H, 18H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 81H, 00H, 80H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 08H, 00H, 56H, 00H, 37H, 00H
    DB	0eeH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH, 73H, 00H
    DB	0feH, 0ffH, 65H, 00H, 46H, 00H, 0feH, 0ffH
    DB	27H, 00H, 72H, 00H, 0f6H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 64H, 00H, 55H, 00H, 07H, 00H
    DB	17H, 00H, 0d2H, 0ffH, 0e6H, 0ffH, 0f6H, 0ffH
    DB	0feH, 0ffH, 71H, 00H, 0feH, 0ffH, 70H, 00H
    DB	36H, 00H, 0faH, 0ffH, 0feH, 0ffH, 63H, 00H
    DB	45H, 00H, 0feH, 0ffH, 54H, 00H, 26H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 62H, 00H, 16H, 00H
    DB	0feH, 0ffH, 61H, 00H, 0feH, 0ffH, 06H, 00H
    DB	60H, 00H, 0eeH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH
    DB	53H, 00H, 0feH, 0ffH, 35H, 00H, 44H, 00H
    DB	0feH, 0ffH, 25H, 00H, 52H, 00H, 0feH, 0ffH
    DB	51H, 00H, 0feH, 0ffH, 15H, 00H, 05H, 00H
    DB	0beH, 0ffH, 0d2H, 0ffH, 0e6H, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 34H, 00H, 43H, 00H
    DB	0feH, 0ffH, 50H, 00H, 24H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 42H, 00H, 33H, 00H, 14H, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 41H, 00H, 0feH, 0ffH
    DB	04H, 00H, 40H, 00H, 0feH, 0ffH, 23H, 00H
    DB	32H, 00H, 0faH, 0ffH, 0feH, 0ffH, 13H, 00H
    DB	31H, 00H, 0faH, 0ffH, 0feH, 0ffH, 03H, 00H
    DB	30H, 00H, 22H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	12H, 00H, 21H, 00H, 0feH, 0ffH, 02H, 00H
    DB	20H, 00H, 0faH, 0ffH, 0feH, 0ffH, 11H, 00H
    DB	01H, 00H, 10H, 00H, 00H, 00H
_htab24:
    DB	7aH, 0fcH, 16H, 0ffH, 0aaH, 0ffH, 0ceH, 0ffH
    DB	0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0efH, 00H, 0feH, 00H, 0feH, 0ffH, 0dfH, 00H
    DB	0fdH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0cfH, 00H
    DB	0fcH, 00H, 0feH, 0ffH, 0bfH, 00H, 0fbH, 00H
    DB	0f6H, 0ffH, 0feH, 0ffH, 0faH, 00H, 0feH, 0ffH
    DB	0afH, 00H, 9fH, 00H, 0feH, 0ffH, 0f9H, 00H
    DB	0f8H, 00H, 0eeH, 0ffH, 0f6H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 8fH, 00H, 7fH, 00H, 0f7H, 00H
    DB	0feH, 0ffH, 6fH, 00H, 0f6H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 5fH, 00H, 0f5H, 00H, 0feH, 0ffH
    DB	4fH, 00H, 0f4H, 00H, 72H, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 3fH, 00H, 0f3H, 00H
    DB	0feH, 0ffH, 2fH, 00H, 0f2H, 00H, 0f6H, 0ffH
    DB	0feH, 0ffH, 0f1H, 00H, 0feH, 0ffH, 1fH, 00H
    DB	0f0H, 00H, 0ceH, 0ffH, 0eeH, 0ffH, 0feH, 0ffH
    DB	0fH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0eeH, 00H
    DB	0deH, 00H, 0feH, 0ffH, 0edH, 00H, 0ceH, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0ecH, 00H
    DB	0ddH, 00H, 0feH, 0ffH, 0beH, 00H, 0ebH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0cdH, 00H, 0dcH, 00H
    DB	0feH, 0ffH, 0aeH, 00H, 0eaH, 00H, 0e2H, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0bdH, 00H
    DB	0dbH, 00H, 0feH, 0ffH, 0ccH, 00H, 9eH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0e9H, 00H, 0adH, 00H
    DB	0feH, 0ffH, 0daH, 00H, 0bcH, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0cbH, 00H, 8eH, 00H
    DB	0feH, 0ffH, 0e8H, 00H, 9dH, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 0d9H, 00H, 7eH, 00H, 0feH, 0ffH
    DB	0e7H, 00H, 0acH, 00H, 0ffH, 00H, 2aH, 0feH
    DB	0e2H, 0feH, 66H, 0ffH, 0a6H, 0ffH, 0ceH, 0ffH
    DB	0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0caH, 00H, 0bbH, 00H, 0feH, 0ffH, 8dH, 00H
    DB	0d8H, 00H, 0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0eH, 00H, 0e0H, 00H, 0dH, 00H, 0e6H, 00H
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 6eH, 00H
    DB	9cH, 00H, 0c9H, 00H, 0feH, 0ffH, 5eH, 00H
    DB	0baH, 00H, 0eeH, 0ffH, 0f6H, 0ffH, 0feH, 0ffH
    DB	0e5H, 00H, 0feH, 0ffH, 0abH, 00H, 7dH, 00H
    DB	0feH, 0ffH, 0d7H, 00H, 0e4H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 8cH, 00H, 0c8H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 4eH, 00H, 2eH, 00H, 3eH, 00H
    DB	0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	6dH, 00H, 0d6H, 00H, 0feH, 0ffH, 0e3H, 00H
    DB	9bH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0b9H, 00H
    DB	0aaH, 00H, 0feH, 0ffH, 0e2H, 00H, 1eH, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0e1H, 00H
    DB	5dH, 00H, 0feH, 0ffH, 0d5H, 00H, 7cH, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0c7H, 00H, 4dH, 00H
    DB	0feH, 0ffH, 8bH, 00H, 0b8H, 00H, 0c2H, 0ffH
    DB	0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0d4H, 00H, 9aH, 00H, 0feH, 0ffH, 0a9H, 00H
    DB	6cH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0c6H, 00H
    DB	3dH, 00H, 0feH, 0ffH, 0d3H, 00H, 2dH, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0d2H, 00H
    DB	1dH, 00H, 0feH, 0ffH, 7bH, 00H, 0b7H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0d1H, 00H, 5cH, 00H
    DB	0feH, 0ffH, 0c5H, 00H, 8aH, 00H, 0deH, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0a8H, 00H
    DB	99H, 00H, 0feH, 0ffH, 4cH, 00H, 0c4H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 6bH, 00H, 0b6H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0d0H, 00H, 0cH, 00H
    DB	3cH, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0c3H, 00H, 7aH, 00H, 0feH, 0ffH, 0a7H, 00H
    DB	2cH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0c2H, 00H
    DB	5bH, 00H, 0feH, 0ffH, 0b5H, 00H, 1cH, 00H
    DB	8eH, 0ffH, 0baH, 0ffH, 0daH, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 89H, 00H, 98H, 00H
    DB	0feH, 0ffH, 0c1H, 00H, 4bH, 00H, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0c0H, 00H, 0bH, 00H
    DB	3bH, 00H, 0faH, 0ffH, 0feH, 0ffH, 0b0H, 00H
    DB	0aH, 00H, 1aH, 00H, 0f6H, 0ffH, 0feH, 0ffH
    DB	0b4H, 00H, 0feH, 0ffH, 6aH, 00H, 0a6H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 79H, 00H, 97H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 0a0H, 00H, 09H, 00H
    DB	90H, 00H, 0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	0b3H, 00H, 88H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	2bH, 00H, 5aH, 00H, 0b2H, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 0a5H, 00H, 1bH, 00H
    DB	0feH, 0ffH, 0b1H, 00H, 69H, 00H, 0feH, 0ffH
    DB	96H, 00H, 0a4H, 00H, 0deH, 0ffH, 0eeH, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 4aH, 00H
    DB	78H, 00H, 87H, 00H, 0feH, 0ffH, 3aH, 00H
    DB	0a3H, 00H, 0faH, 0ffH, 0feH, 0ffH, 59H, 00H
    DB	95H, 00H, 0feH, 0ffH, 2aH, 00H, 0a2H, 00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 0a1H, 00H
    DB	68H, 00H, 0feH, 0ffH, 86H, 00H, 77H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 49H, 00H, 94H, 00H
    DB	0feH, 0ffH, 39H, 00H, 93H, 00H, 82H, 0ffH
    DB	0c2H, 0ffH, 0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH
    DB	0feH, 0ffH, 58H, 00H, 85H, 00H, 0feH, 0ffH
    DB	29H, 00H, 67H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	76H, 00H, 92H, 00H, 0feH, 0ffH, 19H, 00H
    DB	91H, 00H, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	48H, 00H, 84H, 00H, 0feH, 0ffH, 57H, 00H
    DB	75H, 00H, 0faH, 0ffH, 0feH, 0ffH, 38H, 00H
    DB	83H, 00H, 0feH, 0ffH, 66H, 00H, 28H, 00H
    DB	0deH, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	82H, 00H, 18H, 00H, 0feH, 0ffH, 47H, 00H
    DB	74H, 00H, 0f6H, 0ffH, 0feH, 0ffH, 81H, 00H
    DB	0feH, 0ffH, 08H, 00H, 80H, 00H, 0feH, 0ffH
    DB	56H, 00H, 65H, 00H, 0f2H, 0ffH, 0f6H, 0ffH
    DB	0feH, 0ffH, 17H, 00H, 0feH, 0ffH, 07H, 00H
    DB	70H, 00H, 73H, 00H, 0faH, 0ffH, 0feH, 0ffH
    DB	37H, 00H, 27H, 00H, 72H, 00H, 0e2H, 0ffH
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 46H, 00H
    DB	64H, 00H, 0feH, 0ffH, 55H, 00H, 71H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 36H, 00H, 63H, 00H
    DB	0feH, 0ffH, 45H, 00H, 54H, 00H, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 26H, 00H, 62H, 00H
    DB	0feH, 0ffH, 16H, 00H, 61H, 00H, 0f6H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH, 06H, 00H, 60H, 00H
    DB	35H, 00H, 0feH, 0ffH, 53H, 00H, 44H, 00H
    DB	9aH, 0ffH, 0b6H, 0ffH, 0d2H, 0ffH, 0e2H, 0ffH
    DB	0eeH, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 25H, 00H
    DB	52H, 00H, 0feH, 0ffH, 15H, 00H, 0feH, 0ffH
    DB	05H, 00H, 50H, 00H, 0feH, 0ffH, 51H, 00H
    DB	0feH, 0ffH, 34H, 00H, 43H, 00H, 0faH, 0ffH
    DB	0feH, 0ffH, 24H, 00H, 42H, 00H, 0feH, 0ffH
    DB	33H, 00H, 14H, 00H, 0eeH, 0ffH, 0f6H, 0ffH
    DB	0feH, 0ffH, 41H, 00H, 0feH, 0ffH, 04H, 00H
    DB	40H, 00H, 0feH, 0ffH, 23H, 00H, 32H, 00H
    DB	0feH, 0ffH, 13H, 00H, 31H, 00H, 0f2H, 0ffH
    DB	0f6H, 0ffH, 0faH, 0ffH, 0feH, 0ffH, 03H, 00H
    DB	30H, 00H, 22H, 00H, 12H, 00H, 0feH, 0ffH
    DB	21H, 00H, 0feH, 0ffH, 02H, 00H, 20H, 00H
    DB	0faH, 0ffH, 0feH, 0ffH, 11H, 00H, 01H, 00H
    DB	0feH, 0ffH, 10H, 00H, 00H, 00H
_ht:
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab0
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab1
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab2
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab3
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab0
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab5
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab6
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab7
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab8
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab9
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab10
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab11
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab12
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab13
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab0
    DB	00H, 00H, 00H, 00H
    DD	FLAT:_htab15
    DB	01H, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	02H, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	03H, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	04H, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	06H, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	08H, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	0aH, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	0dH, 00H, 00H, 00H
    DD	FLAT:_htab16
    DB	04H, 00H, 00H, 00H
    DD	FLAT:_htab24
    DB	05H, 00H, 00H, 00H
    DD	FLAT:_htab24
    DB	06H, 00H, 00H, 00H
    DD	FLAT:_htab24
    DB	07H, 00H, 00H, 00H
    DD	FLAT:_htab24
    DB	08H, 00H, 00H, 00H
    DD	FLAT:_htab24
    DB	09H, 00H, 00H, 00H
    DD	FLAT:_htab24
    DB	0bH, 00H, 00H, 00H
    DD	FLAT:_htab24
    DB	0dH, 00H, 00H, 00H
    DD	FLAT:_htab24
_htab_c0_2:
    DB	0c6H, 0ffH, 0d6H, 0ffH, 0e6H, 0ffH, 0f2H, 0ffH
    DB	0faH, 0ffH, 0feH, 0ffH,  0dH,  00H,  0fH,  00H
    DB	0feH, 0ffH,  0bH,  00H,  07H,  00H, 0faH, 0ffH
    DB	0feH, 0ffH,  0eH,  00H,  0aH,  00H,  09H,  00H
    DB	0faH, 0ffH, 0feH, 0ffH,  06H,  00H,  0cH,  00H
    DB	0feH, 0ffH,  05H,  00H,  03H,  00H, 0faH, 0ffH
    DB	0feH, 0ffH,  04H,  00H,  08H,  00H, 0feH, 0ffH
    DB	 02H,  00H,  01H,  00H,  00H,  00H
;_htab_c0_2:
;    DB	0c6H, 0ffH, 0d6H, 0ffH, 0e6H, 0ffH, 0f2H, 0ffH
;    DB	0faH, 0ffH, 0feH, 0ffH,  0bH,  00H,  0fH,  00H
;    DB	0feH, 0ffH,  0dH,  00H,  0eH,  00H, 0faH, 0ffH
;    DB	0feH, 0ffH,  07H,  00H,  05H,  00H,  09H,  00H
;    DB	0faH, 0ffH, 0feH, 0ffH,  06H,  00H,  03H,  00H
;    DB	0feH, 0ffH,  0aH,  00H,  0cH,  00H, 0faH, 0ffH
;    DB	0feH, 0ffH,  02H,  00H,  01H,  00H, 0feH, 0ffH
;    DB	 04H,  00H,  08H,  00H,  00H,  00H
_htab_c1_2:
    DB	0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
    DB	 0fH,  00H,  07H,  00H, 0feH, 0ffH,  0bH,  00H
    DB	 03H,  00H, 0faH, 0ffH, 0feH, 0ffH,  0dH,  00H
    DB	 05H,  00H, 0feH, 0ffH,  09H,  00H,  01H,  00H
    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH,  0eH,  00H
    DB	 06H,  00H, 0feH, 0ffH,  0aH,  00H,  02H,  00H
    DB	0faH, 0ffH, 0feH, 0ffH,  0cH,  00H,  04H,  00H
    DB	0feH, 0ffH,  08H,  00H,  00H,  00H
;_htab_c1_2:
;    DB	0e2H, 0ffH, 0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH
;    DB	 0fH,  00H,  0eH,  00H, 0feH, 0ffH,  0dH,  00H
;    DB	 0cH,  00H, 0faH, 0ffH, 0feH, 0ffH,  0bH,  00H
;    DB	 0aH,  00H, 0feH, 0ffH,  09H,  00H,  08H,  00H
;    DB	0f2H, 0ffH, 0faH, 0ffH, 0feH, 0ffH,  07H,  00H
;    DB	 06H,  00H, 0feH, 0ffH,  05H,  00H,  04H,  00H
;    DB	0faH, 0ffH, 0feH, 0ffH,  03H,  00H,  02H,  00H
;    DB	0feH, 0ffH,  01H,  00H,  00H,  00H
_htc2:
    DD	FLAT:_htab_c0_2
    DD	FLAT:_htab_c1_2

_DATA		ENDS
_BSS		SEGMENT	DWORD PUBLIC USE32 'BSS'
    ORG 00000000H
_ispow    LABEL	BYTE
    ORG  32768         ; 8192*4 bytes (dword) precision
_mpxdec_l3deq_gainpow2_2 LABEL BYTE
    ORG  34816         ; 512*4 bytes
_mpxdec_l3deq_gainpow2_4 LABEL BYTE
    ORG  36864         ; 512*4
_mpxdec_l3deq_ispow32_2 LABEL BYTE
    ORG  102400        ; 512*32*4
_mpxdec_l3deq_ispow32_4 LABEL BYTE
    ORG  167936        ; 512*32*4
_BSS		ENDS

		END
