//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2010 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function:sublist handling

#include <string.h>
#include "playlist.h"
#include "newfunc\newfunc.h"
#include "control\control.h"

unsigned int playlist_loadsub_check_extension(struct playlist_side_info *psi,char *filename)
{
 if(playlist_loadlist_check_extension(filename)) // m3u,mxu,pls,fpl,cue
  return 1;
 if(!(psi->editsidetype&PLT_DIRECTORY) || psi->sublistlevel)
  if(pds_filename_wildchar_chk(filename)) // dir-scan
   return 1;
 return 0;
}

void playlist_loadsub_setnewinputfile(struct playlist_side_info *psi,char *newinputfile,unsigned long loadtype)
{
 if(newinputfile){
  if(loadtype&PLL_DRIVESCAN)
   pds_strcpy(psi->sublistnames[0],newinputfile);
  else
   pds_filename_build_fullpath(psi->sublistnames[0],psi->currdir,newinputfile);
 }else
  psi->sublistnames[0][0]=0;
}

char *playlist_loadsub_getinputfile(struct playlist_side_info *psi)
{
 if(!psi->sublistnames[0][0] && freeopts[OPT_INPUTFILE])
  mpxplay_playlist_startfile_fullpath(psi->sublistnames[0],freeopts[OPT_INPUTFILE]);
 if(psi->sublistnames[0][0])
  return psi->sublistnames[0];
 return NULL;
}

void playlist_loadsub_addsubdots(struct playlist_side_info *psi)
{
 if(psi->sublistlevel){
  struct playlist_entry_info *pei=psi->firstentry;
  pei->filename=psi->filenameslastp;
  psi->filenameslastp+=pds_strcpy(psi->filenameslastp,psi->sublistnames[psi->sublistlevel]);
  psi->filenameslastp+=pds_strcpy(psi->filenameslastp,PDS_DIRECTORY_SEPARATOR_STR);
  psi->filenameslastp+=pds_strcpy(psi->filenameslastp,"..")+1;
  pei->entrytype=DFT_UPLIST;
  pei->id3info[I3I_DFT_STORE]="[up-list]";
  psi->lastentry=pei;
  pei++;
  psi->firstsong=pei;
  playlist_editorhighline_set(psi,psi->firstentry);
 }
}

static unsigned int load_uplist(struct playlist_side_info *psi)
{
 if(psi->sublistlevel){
  psi->sublistlevel--;
  playlist_savelist_clear(psi);
  if(playlist_buildlist_all(psi)){
   playlist_search_lastdir(psi,psi->sublistnames[psi->sublistlevel+1]);
   return 1;
  }
 }
 return 0;
}

static unsigned int load_sublist(struct playlist_side_info *psi,char *listname)
{
 if(psi->sublistlevel<MAX_SUBLIST_LEVELS){
  psi->sublistlevel++;
  playlist_savelist_clear(psi);
  pds_filename_build_fullpath(psi->sublistnames[psi->sublistlevel],psi->currdir,listname);
  if(playlist_buildlist_all(psi)){
   playlist_editorhighline_set(psi,psi->firstentry);
   return 1;
  }
 }
 return 0;
}

static void load_rootlist(struct playlist_side_info *psi)
{
 if(psi->sublistlevel){
  psi->sublistlevel=0;
  playlist_savelist_clear(psi);
  if(playlist_buildlist_all(psi))
   playlist_editorhighline_set(psi,psi->firstentry);
 }
}

static unsigned int load_newlist(struct playlist_side_info *psi,char *listname)
{
 playlist_disable_side_list(psi);
 funcbit_enable(psi->editloadtype,PLL_LOADLIST);
 playlist_loadsub_setnewinputfile(psi,listname,PLL_LOADLIST);
 if(playlist_buildlist_all(psi)){
  playlist_editorhighline_set(psi,psi->firstentry);
  return 1;
 }
 return 0;
}

struct playlist_side_info *playlist_loadsub_sublist_change(struct playlist_side_info *psi,unsigned long head)
{
 struct playlist_entry_info *pei=psi->editorhighline;
 unsigned int sideused;
 char filename[MAX_PATHNAMELEN];

 pds_strcpy(filename,pei->filename);
 if(head&DFTM_ROOT){
  load_rootlist(psi);
 }else{
  if(head&DFTM_UPLIST){
   load_uplist(psi);
  }else{
   if(head&DFTM_SUBLIST){
    load_sublist(psi,filename);
   }else{
    psi=psi->mvp->psil;
    sideused=psi->editsidetype&PLT_ENABLED;
    if(load_newlist(psi,filename))
     if(!sideused)
      playlist_start_sideplay(psi->mvp,psi);
   }
  }
 }

 if(psi->sublistlevel)
  funcbit_enable(psi->editloadtype,PLL_SUBLISTS);
 else
  funcbit_disable(psi->editloadtype,PLL_SUBLISTS);
 if(psi->editsidetype&PLT_DIRECTORY)
  funcbit_disable(psi->editloadtype,PLL_TYPE_LOAD);

 return psi;
}

//ctrl-gray-'/' and '*'
void playlist_loadsub_search_paralell_list(struct playlist_side_info *psi,int step)
{
 unsigned int found;
 struct playlist_entry_info *pei;
 char oldsublist[MAX_PATHNAMELEN],newsublist[MAX_PATHNAMELEN];

 pds_strcpy(oldsublist,psi->sublistnames[psi->sublistlevel]);  // save original sub-playlist
 if(load_uplist(psi)){
  found=0;
  pei=psi->editorhighline;
  pei+=step;
  while((step==1 && pei<=psi->lastentry) || (step==-1 && pei>=psi->firstentry)){
   if(playlist_loadsub_check_extension(psi,pei->filename)){
    pds_strcpy(newsublist,pei->filename);
    load_sublist(psi,newsublist);
    found=1;
    break;
   }
   pei+=step;
  }
  if(!found)
   load_sublist(psi,oldsublist); // restore original sub-playlist
  playlist_chkfile_start_norm(psi,0);
 }
}

#define SUBLIST_SEPARATOR_IN_MPXPLAYINI '|'

//slice sublist-names from one string (at loading of startup)
unsigned int playlist_loadsub_sublist_setlevels(struct playlist_side_info *psi,char *sublists)
{
 if(!sublists || !(*sublists))
  return 0;
 psi->sublistlevel=0; // it's 0 here
 do{
  char *next=pds_strchr(sublists,SUBLIST_SEPARATOR_IN_MPXPLAYINI);
  if(next)
   *next++=0;
  pds_strcpy(psi->sublistnames[psi->sublistlevel],sublists);
  if(!next)
   break;
  sublists=next;
  psi->sublistlevel++;
 }while(psi->sublistlevel<=MAX_SUBLIST_LEVELS);
 if(psi->sublistlevel)
  funcbit_enable(psi->editloadtype,PLL_SUBLISTS); // startup requires this setting
 return psi->sublistlevel;
}

//collect sublists in one string (at saving of startup)
unsigned int playlist_loadsub_sublist_getlevels(struct playlist_side_info *psi,char *destbuf,unsigned int buflen)
{
 unsigned int i=0;
 if(psi->editsidetype&PLT_DIRECTORY){               // ???
  pds_strcpy(psi->sublistnames[0],psi->currdir);    //
  mpxplay_diskdrive_drive_config(psi->mdds,MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_REALLYFULLPATH,psi->sublistnames[0],psi->sublistnames[0]); // !!!
 }
 do{
  unsigned int len=pds_strlen(psi->sublistnames[i]);
  if(buflen<=len)
   break;
  pds_strcpy(destbuf,psi->sublistnames[i]);
  destbuf+=len;
  buflen-=len;
  if(i>=psi->sublistlevel)
   break;
  *destbuf++=SUBLIST_SEPARATOR_IN_MPXPLAYINI;
  buflen--;
  i++;
 }while(1);
 *destbuf++=0;
 return (min(i,psi->sublistlevel));
}

void playlist_loadsub_sublist_clear(struct playlist_side_info *psi)
{
 psi->sublistlevel=0;
 psi->sublistnames[0][0]=0;
 funcbit_disable(psi->editloadtype,PLL_SUBLISTS);
}

char *playlist_loadsub_get_dftstr(unsigned int entrytype)
{
 char *s;
 switch(entrytype){
  case DFT_DRIVE   :s=DFTSTR_DRIVE;break;
  case DFT_UPDIR   :s=DFTSTR_UPDIR;break;
  case DFT_SUBDIR  :s=DFTSTR_SUBDIR;break;
  case DFT_PLAYLIST:s=DFTSTR_PLAYLIST;break;
  case DFT_UPLIST  :s=DFTSTR_UPLIST;break;
  case DFT_SUBLIST :s=DFTSTR_SUBLIST;break;
  default:s="";
 }
 return s;
}
