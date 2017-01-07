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
//function: fastlist - load list/file with one key

#include "newfunc\newfunc.h"
#include "control.h"
#include "cntfuncs.h"
#include "playlist\playlist.h"
#include "display\display.h"

#define FASTLIST_MAX_FUNCS 16

typedef struct{
 unsigned int keycode;
 char filename[MAX_PATHNAMELEN];
}fastlist_func_t;

extern struct mainvars mvps;
extern unsigned int refdisp;
extern int playstartlist;

static fastlist_func_t fastlist_funcs[FASTLIST_MAX_FUNCS];
static unsigned int fastlistnum;

void mpxplay_control_fastlist_loadini(mpxini_line_t *mpxini_lines,struct mpxini_part_t *mpxini_partp)
{
 unsigned int i,funccount;
 mpxini_line_t *linep;
 fastlist_func_t *listfuncp;

 linep=mpxini_lines+mpxini_partp->partbegin_linenum;
 listfuncp=&fastlist_funcs[0];

 funccount=0;
 for(i=0;(i<mpxini_partp->partlinenum) && (funccount<FASTLIST_MAX_FUNCS);i++,linep++){
  if(linep->varnamep){
   if(pds_strlicmp(linep->varnamep,"fastlist")==0){
    char *fnamep=pds_strchr(linep->valuep,','); // begin of filename after ','
    if(!fnamep)
     continue;
    fnamep++;
    if(fnamep[0]=='\"')                         // filename begins with '"' (maybe)
     pds_strcpy(listfuncp->filename,fnamep+1);
    else
     pds_strcpy(listfuncp->filename,fnamep);
    fnamep=pds_strchr(listfuncp->filename,'\"');// filename ends with '"' (maybe)
    if(fnamep)
     *fnamep=0;
    listfuncp->keycode=pds_atol16(linep->valuep);
    listfuncp++;
    funccount++;
   }
  }
 }
 fastlistnum=funccount;
}

unsigned int mpxplay_control_fastlist_enabled(void)
{
 return fastlistnum;
}

void mpxplay_control_fastlist_searchfilename(struct playlist_side_info *psi,char *filename)
{
 if(fastlistnum){
  if(playstartlist<0 && filename){ // non-cmos search
   fastlist_func_t *listfuncp=&fastlist_funcs[0];
   do{
    if(pds_stricmp(filename,listfuncp->filename)==0){
     playstartlist=listfuncp-&fastlist_funcs[0];
     break;
    }
    listfuncp++;
   }while(listfuncp->filename[0] || listfuncp->keycode);
  }
  //verify CMOS and/or fastlist search result
  if(playstartlist<0)
   playstartlist=0;              // first
  if(playstartlist>=fastlistnum)
   playstartlist=fastlistnum-1;  // last
  playlist_loadsub_setnewinputfile(psi,fastlist_funcs[playstartlist].filename,PLL_FASTLIST);
  funcbit_disable(psi->editloadtype,PLL_TYPE_ALL);
  funcbit_enable(psi->editloadtype,PLL_FASTLIST);
 }
}

static void load_fastlist(struct playlist_side_info *psi,char *listname)
{
 char sout[128];
 playlist_disable_side_list(psi);
 playlist_clear_side(psi);
 playlist_loadsub_setnewinputfile(psi,listname,PLL_FASTLIST);
 playlist_buildlist_one(psi,listname,PLL_FASTLIST,NULL,NULL);
 if(psi->editsidetype&PLT_ENABLED){
  funcbit_disable(psi->editsidetype,PLT_DIRECTORY);
  funcbit_enable(psi->editloadtype,PLL_FASTLIST);
 }
 playlist_chkfile_start_norm(psi,0);
 refdisp|=RDT_EDITOR;
 snprintf(sout,sizeof(sout),"Selected %d. fastlist\nLoaded: %s",playstartlist+1,listname);
 display_timed_message(sout);
}

unsigned int mpxplay_control_fastlist_keycheck(unsigned int extkey,mainvars *mvp)
{
 if(fastlistnum){
  fastlist_func_t *listfuncp=&fastlist_funcs[0];
  do{
   if(extkey==listfuncp->keycode){
    playstartlist=listfuncp-&fastlist_funcs[0];
    load_fastlist(mvp->psil,listfuncp->filename);
    return 1;
   }
   listfuncp++;
  }while(listfuncp->filename[0] || listfuncp->keycode);
 }
 return 0;
}

unsigned int mpxplay_control_fastlist_step(mainvars *mvp,int direction)
{
 if(fastlistnum){
  playstartlist+=direction;
  if(playstartlist<0)
   playstartlist=fastlistnum-1;
  if(playstartlist>=fastlistnum)
   playstartlist=0;
  load_fastlist(mvp->psil,fastlist_funcs[playstartlist].filename);
  return 1;
 }
 return 0;
}
