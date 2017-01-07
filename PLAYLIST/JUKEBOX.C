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
//function:jukebox

#include "newfunc\newfunc.h"
#include "playlist.h"
#include "display\display.h"

extern unsigned int refdisp,playcontrol,playlistload,playrand;

void playlist_jukebox_add_entry(struct mainvars *mvp,struct playlist_side_info *psi_src)
{
 if((mvp->newsong>=psi_src->firstsong) && (mvp->newsong<=psi_src->lastentry)){ // if the doomlist-side and the newsong-side match (they don't match at album-skip)
  struct playlist_side_info *psi_dest=mvp->psip;
  unsigned int sideused=psi_dest->editsidetype&PLT_ENABLED;

  playlist_editlist_addfile_any(psi_src,mvp->newsong,NULL);
  if(!sideused && !(playcontrol&PLAYC_RUNNING)){
   /*if(playcontrol&PLAYC_RUNNING){
    if(pds_strcmp(mvp->pei0->filename,mvp->newsong->filename)==0) // hack
     mvp->aktfilenum--;            // (else it's deleted at jukebox_skip)
    mvp->adone=ADONE_RESTART;
   }else*/
    playlist_start_sideplay(mvp,psi_dest);
  }else
   refdisp|=RDT_BROWSER|RDT_EDITOR;
  mvp->newsong=NULL;
 }
}

unsigned int playlist_jukebox_skip(struct mainvars *mvp)
{
 struct playlist_side_info *psi=mvp->psip;
 struct playlist_entry_info *peiakt,*peinew;
 unsigned int retcode=1;

 if(mvp->direction==0)
  mvp->direction=1;
 if(psi->lastentry>=psi->firstentry){

  if(mvp->aktfilenum>=psi->firstentry){
   if(mvp->aktfilenum>psi->lastentry)
    mvp->aktfilenum=psi->lastentry;
   peinew=mvp->newfilenum;
   peiakt=mvp->aktfilenum;
   if(peiakt!=peinew){ // do not delete if newfilenum==aktfilenum
    if(peiakt->entrytype<DFT_AUDIOFILE)
     peiakt->entrytype=DFT_UNKNOWN;              // else delfile will not delete HFT_DFT
    playlist_editlist_delfile_one(psi,peiakt,EDITLIST_MODE_ALL);
    if(playrand && !peinew)
     mvp->newfilenum=NULL;
   }
  }

  playlist_skip(mvp);

  if(mvp->aktfilenum>psi->lastentry)
   mvp->aktfilenum=psi->lastentry;
  if(mvp->aktfilenum<psi->firstentry)
   mvp->aktfilenum=psi->firstentry;
 }
 if(psi->lastentry<psi->firstentry){
  if(playcontrol&PLAYC_RUNNING)
   retcode=0;
  else
   mpxplay_stop_and_clear(mvp,0);
 }

 return retcode;
}

void playlist_jukebox_switch(struct mainvars *mvp)
{
 struct playlist_side_info *psi0=mvp->psi0,*psi1=psi0+1,*newpsil;

 funcbit_inverse(playlistload,PLL_DOOMBOX);

 if(playlistload&PLL_DOOMBOX)
  newpsil=psi0;
 else
  newpsil=psi1;

 if(newpsil!=mvp->psil){
  if(mvp->psil->editsidetype&PLT_ENABLED)
   playlist_copy_side_infos(newpsil,mvp->psil);
  mvp->psil=newpsil;
 }

 playlist_disable_side_partial(psi1);
 playlist_reset_side(psi0);
 playlist_reset_side(psi1);

 if(playlistload&PLL_DOOMBOX){
  funcbit_enable(psi0->editloadtype,PLL_DOOMBOX);
  playlist_loadsub_sublist_clear(psi1);
  playlist_savelist_clear(psi1);
  psi1->editsidetype=PLT_DOOMQUEUE;
  psi1->editloadtype=PLL_CHG_ENTRY;
 }else{
  funcbit_disable(psi0->editloadtype,PLL_DOOMBOX);
  funcbit_disable(psi1->editloadtype,PLL_DOOMBOX);
 }

 if(!(psi0->editloadtype&PLL_TYPE_LOAD) || !(playlistload&PLL_DOOMBOX))
  funcbit_enable(psi0->editsidetype,PLT_DIRECTORY);

 playlist_init_pointers(mvp);

 playlist_get_allfilenames(mvp);

 if((playlistload&PLL_DOOMBOX) || (psi1->editsidetype&PLT_ENABLED))
  mvp->psip=psi1;
 else
  mvp->psip=psi0;

 if(!(playlistload&PLL_DOOMBOX) && (mvp->psie==mvp->psi0) && (mvp->psip!=mvp->psie)){
  struct playlist_side_info *psie;
  playlist_change_editorside(mvp);
  psie=mvp->psie;
  playlist_editorhighline_set(psie,psie->firstentry);
 }
 mvp->aktfilenum=mvp->psip->firstsong-1;
 playlist_chkfile_start_norm(psi0,0);
 playlist_chkfile_start_norm(psi1,0);
}

void playlist_jukebox_set(struct mainvars *mvp,unsigned int loadtype_priority)
{
 struct playlist_side_info *psi0=mvp->psi0;
 struct playlist_side_info *psi1=psi0+1;

 if(loadtype_priority){
  if(psi0->editloadtype&PLL_DOOMBOX)
   psi1->editsidetype=PLT_DOOMQUEUE;
  if(psi1->editloadtype&PLL_DOOMBOX)
   psi0->editsidetype=PLT_DOOMQUEUE;
 }

 if(psi0->editsidetype&PLT_DOOMQUEUE){
  funcbit_enable(psi1->editloadtype,PLL_DOOMBOX);
  mvp->psip=psi0;
 }else
  funcbit_disable(psi1->editloadtype,PLL_DOOMBOX);

 if(psi1->editsidetype&PLT_DOOMQUEUE){
  funcbit_enable(psi0->editloadtype,PLL_DOOMBOX);
  funcbit_disable(psi0->editsidetype,PLT_DOOMQUEUE); // corrections (should not happen)
  funcbit_disable(psi1->editloadtype,PLL_DOOMBOX);   //
  mvp->psip=psi1;
 }else
  funcbit_disable(psi0->editloadtype,PLL_DOOMBOX);

 if((psi0->editsidetype&PLT_DOOMQUEUE) || (psi1->editsidetype&PLT_DOOMQUEUE))
  funcbit_enable(playlistload,PLL_DOOMBOX);
 else
  funcbit_disable(playlistload,PLL_DOOMBOX);

 if(((playlistload&PLL_DOOMBOX) || (psi1->editsidetype&(PLT_DIRECTORY|PLT_DOOMQUEUE))) && !(psi0->editsidetype&PLT_DIRECTORY))
  mvp->psil=psi0;
 else
  mvp->psil=psi1;
}
