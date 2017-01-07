//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: auto volume correction (low sensitivy)

#include "au_mixer.h"
#include "newfunc\newfunc.h"
#include "display\display.h"

#define USE_ASM_MX_AVOL 1

extern unsigned int crossfadepart;
extern unsigned int refdisp;
extern int MIXER_var_volume;
extern mainvars mvps;

int MIXER_var_autovolume;

#define AVOL_MAX_VOLUME 400
#define AVOL_MIN_VOLUME 100
#define AVOL_UP_WAIT      8
#define AVOL_DOWN_SPEED  10
#define AVOL_CCHK_COMPRESS 5  // >>5 == /32
#define AVOL_CCHK_SIZE ((MIXER_SCALE_MAX+1)>>AVOL_CCHK_COMPRESS)

#ifdef USE_ASM_MX_AVOL
void get_clipcounts(void);
void get_signlimit(void);
#endif

void mixer_autovolume_set(short *pcm_sample,unsigned int samplenum)
{
 const unsigned int AVOL_CLIPS_LIMIT=5;
 static unsigned long clipcounts[AVOL_CCHK_SIZE+32],counter1,pluswait,signlimit;
 unsigned int currlimit,new_volume;

#ifdef USE_ASM_MX_AVOL

 #pragma aux get_clipcounts=\
 "push eax"\
 "mov edi,offset clipcounts"\
 "push edi"\
 "mov ecx,1039"\
 "cld"\
 "xor eax,eax"\
 "rep stosd"\
 "pop edi"\
 "pop esi"\
 "back1:"\
  "movsx eax,word ptr [esi]"\
  "test eax,eax"\
  "jge cpositiv"\
  "neg eax"\
  "cpositiv:"\
  "shr eax,3"\
  "and eax,0xfffffffc"\
  "add eax,edi"\
  "inc dword ptr [eax]"\
  "add esi,2"\
  "dec edx"\
 "jnz back1"\
 modify[edi esi];
 get_clipcounts();

 #pragma aux get_signlimit=\
 "mov eax,dword ptr AVOL_CLIPS_LIMIT"\
 "mov edi,offset clipcounts"\
 "add edi,4096"\
 "mov edx,1024"\
 "mov ebx,edx"\
 "xor ecx,ecx"\
 "back2:add ecx,dword ptr [edi]"\
  "cmp ecx,eax"\
  "ja ugr2"\
  "mov ebx,edx"\
  "sub edi,4"\
  "dec edx"\
 "jnz back2"\
 "ugr2:"\
 "mov eax,102400"\
 "xor edx,edx"\
 "div ebx"\
 "mov dword ptr currlimit,eax"\
 modify[eax ebx ecx edx edi];
 get_signlimit();

#else
 unsigned int k,clips;

 pds_qmemreset(clipcounts,AVOL_CCHK_SIZE+32);
 k=samplenum;
 do{
  register int sign=(int)pcm_sample[0];
  if(sign<0)
   sign=-sign;
  clipcounts[sign>>AVOL_CCHK_COMPRESS]++;
  pcm_sample++;
 }while(--k);

 signlimit=MIXER_SCALE_MAX+1;
 clips=0;
 k=AVOL_CCHK_SIZE;
 do{
  clips+=clipcounts[k];
  if(clips>AVOL_CLIPS_LIMIT)
   break;
  signlimit=k;
 }while(--k);
 currlimit=100*AVOL_CCHK_SIZE/k;

#endif

 new_volume=MIXER_var_volume;
 if((currlimit>MIXER_var_volume) && (MIXER_var_volume<AVOL_MAX_VOLUME)
     && (!crossfadepart || ((crossfadepart==CROSS_IN) && !(mvps.cfi->crossfadetype&CFT_FADEIN)))){
   signlimit=((7*signlimit)+currlimit)>>3;
   if(!counter1){
    new_volume++;
    counter1=signlimit-new_volume;
    if(counter1<100)
     new_volume--;
    if(counter1<40 && new_volume>AVOL_MIN_VOLUME)
     new_volume--;
    counter1>>=5;
    if(counter1>25)
     counter1=25;
    counter1=25-counter1;
    if(counter1>pluswait)
     pluswait=counter1;
    else
     if(pluswait)
      pluswait--;
    counter1=pluswait+AVOL_UP_WAIT;
   }else
    counter1--;
 }else{
  if(currlimit<MIXER_var_volume){
   if(currlimit>=AVOL_MIN_VOLUME){
    pluswait=30;
    new_volume--;
   }else
    new_volume=AVOL_MIN_VOLUME;
  }
 }
 if(new_volume!=MIXER_var_volume){
  MIXER_setfunction("MIX_VOLUME",MIXER_SETMODE_ABSOLUTE,new_volume);
  refdisp|=RDT_VOL;
 }
}
