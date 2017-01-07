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
//function: cut silence (skip frames below SOUNDLIMITvol)

#include "au_mixer.h"
#include "newfunc\newfunc.h"

static void check_soundlimit_end(struct mpxplay_audioout_info_s *aui,int currvol);

extern unsigned int analtabnum;
extern unsigned int crossfadepart;
extern unsigned int playcontrol,SOUNDLIMITvol,SOUNDLIMITbegin,SOUNDLIMITlen;
extern int MIXER_var_volume;
extern unsigned long volnum[5][2];

void mixer_soundlimit_check(struct mpxplay_audioout_info_s *aui)
{
 unsigned int currvol=(volnum[analtabnum][0]+volnum[analtabnum][1])/64;
 if(playcontrol&PLAYC_BEGINOFSONG){
  if((currvol<(SOUNDLIMITvol>>1)) && MIXER_var_volume){
   pds_memset(aui->pcm_sample,0,aui->samplenum*aui->bytespersample_card);
   funcbit_enable(aui->card_controlbits,AUINFOS_CARDCNTRLBIT_DMADONTWAIT);
  }else
   funcbit_disable(playcontrol,PLAYC_BEGINOFSONG);
 }else
  check_soundlimit_end(aui,currvol);
}

static void check_soundlimit_end(struct mpxplay_audioout_info_s *aui,int currvol)
{
 struct mainvars *mvp=aui->mvp;
 struct crossfade_info *cfi=mvp->cfi;
 if(cfi->crossfadelimit || SOUNDLIMITvol){
  struct frame *frp0=mvp->frp0;
  if(crossfadepart==CROSS_OUT){
   if(currvol<cfi->crossfadelimit){
    mvp->sndempty++;
    if(mvp->sndempty>(SOUNDLIMITlen<<1))
     cfi->crossfadepoint=frp0->frameNum-cfi->crossfadebegin;
   }else
    mvp->sndempty=0;
  }else{
   if(currvol<SOUNDLIMITvol){
    unsigned int loc_limitbegin;
    if((frp0->allframes>>1)>SOUNDLIMITbegin)
     loc_limitbegin=SOUNDLIMITbegin;
    else
     loc_limitbegin=frp0->allframes>>1;
    mvp->sndempty++;
    if((mvp->sndempty>SOUNDLIMITlen) && (frp0->frameNum>(frp0->allframes-loc_limitbegin))){
     if(cfi->usecrossfade){
      cfi->crossfadebegin=frp0->frameNum-cfi->crossfadepoint;
     }else
      mvp->adone=ADONE_EOF;
     mvp->sndempty=0;
    }
   }else
    mvp->sndempty=0;
  }
 }
}
