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
//function: startup (restore the playing at the last position)

#include "newfunc\newfunc.h"
#include "control.h"
#include "cntfuncs.h"
#include "playlist\playlist.h"

#define STARTUP_FLAG_LOAD    1
#define STARTUP_FLAG_SAVE    2
#define STARTUP_FLAG_CMOS    4
#define STARTUP_FLAG_RESDIR  8 // restore directory and song-pos at non-playlist mode
#define STARTUP_FLAG_2SIDES 16 // restore both editorsides

#define CMOS_SAVE_REFRESH 2 // in secs

//control.c
extern struct mainvars mvps;
extern char *drivescanletters;
extern unsigned int playlistload;
extern unsigned int intsoundconfig,intsoundcontrol;
extern unsigned int playcontrol,playstartsong;
extern int playstartlist,playstartpercent,playstartframe,control_startup_type_override;
extern char *playstarttime;

static unsigned int su_startupenabled,su_startuptype;
static unsigned int su_editorside,su_playside;
static unsigned long su_oldsongnum,su_oldframenum;

static unsigned int su_oldlisttype[PLAYLIST_MAX_SIDES],su_editorhighline[PLAYLIST_MAX_SIDES];
static mpxp_uint32_t su_oldorderkeys[PLAYLIST_MAX_SIDES];
static char su_oldlistname[PLAYLIST_MAX_SIDES][MPXINI_MAX_CHARSPERLINE],*su_oldsongname;

static mpxini_var_s startup_vars_selector[]={
 {"StartupEnabled",&su_startupenabled,ARG_NUM},
 {NULL,NULL,0}
};

static mpxini_var_s startup_vars_singleside[]={
 {"StartupEnabled",&su_startupenabled,ARG_NUM},
 {"OldListType"   ,&su_oldlisttype[0],   ARG_NUM|ARG_SAVE},
 {"OldListname"   ,&su_oldlistname[0][0],ARG_CHAR|ARG_SAVE},
 {"OldSongname"   ,&su_oldsongname,      ARG_CHAR|ARG_POINTER|ARG_SAVE},
 {"OldSongNum"    ,&su_oldsongnum,       ARG_NUM|ARG_SAVE},
 {"OldFrameNum"   ,&su_oldframenum,      ARG_NUM|ARG_SAVE},
 {NULL,NULL,0}
};

static mpxini_var_s startup_vars_doubleside[]={
 {"StartupEnabled" ,&su_startupenabled,ARG_NUM},
 {"EditorSide"     ,&su_editorside,    ARG_NUM|ARG_SAVE},
 {"PlaySide"       ,&su_playside,      ARG_NUM|ARG_SAVE},
 {"OldSongname"    ,&su_oldsongname,   ARG_CHAR|ARG_POINTER|ARG_SAVE},
 {"OldSongNum"     ,&su_oldsongnum,    ARG_NUM|ARG_SAVE},
 {"OldFrameNum"    ,&su_oldframenum,   ARG_NUM|ARG_SAVE},
 {"OldListType1"   ,&su_oldlisttype[0],   ARG_NUM|ARG_SAVE},
 {"EditorHighline1",&su_editorhighline[0],ARG_NUM|ARG_SAVE},
 {"OrderKeys1"     ,&su_oldorderkeys[0],  ARG_NUM|ARG_HEX|ARG_SAVE},
 {"OldListname1"   ,&su_oldlistname[0][0],ARG_CHAR|ARG_SAVE},
 {"OldListType2"   ,&su_oldlisttype[1],   ARG_NUM|ARG_SAVE},
 {"EditorHighline2",&su_editorhighline[1],ARG_NUM|ARG_SAVE},
 {"OrderKeys2"     ,&su_oldorderkeys[1],  ARG_NUM|ARG_HEX|ARG_SAVE},
 {"OldListname2"   ,&su_oldlistname[1][0],ARG_CHAR|ARG_SAVE},
 {NULL,NULL,0}
};

#ifdef __DOS__
static void shortdelay(val)
{
 while(--val){}
}
#endif

static void mpxplay_control_startup_savetocmos(struct mainvars *mvp)
{
#ifdef __DOS__
 struct frame *frp0=mvp->frp0;
 struct playlist_side_info *psip=mvp->psip;
 long index_pos=frp0->frameNum-frp0->index_start;
 unsigned long framenum=((mvp->adone==ADONE_EOF) || (index_pos<100))? 0:(index_pos-10);
 unsigned long percent=framenum*100/frp0->index_len;      // 7 bits (song position in %)
 unsigned long filenum=(((mvp->adone==ADONE_EOF) && (mvp->aktfilenum==psip->lastentry)) || (mvp->aktfilenum<psip->firstsong))? 0:(mvp->aktfilenum-psip->firstsong+1);
 unsigned long listnum=(playstartlist>=0)? (playstartlist&0x7):0; // 3 bits
 unsigned int intsoundcntrl_save;

 MPXPLAY_INTSOUNDDECODER_DISALLOW;
 outp(0x70,1);
 shortdelay(1); // ???
 outp(0x71,filenum&0xff);                           // 8 bits filenum
 outp(0x70,3);
 shortdelay(1); // ???
 if(mpxplay_control_fastlist_enabled()){ // filenum: 8+6=14 bits
  outp(0x71,((filenum>>8)&0x3f)|((listnum&0x03)<<6));// 6 (higher) bits filenum & 2 (lower) bits listnum
  outp(0x70,5);
  shortdelay(1); // ???
  outp(0x71,(percent&0x7f)|((listnum&0x04)<<(7-2))); // 7 (lower) bits percent & 1 (higher) bit listnum
 }else{                                  // filenum: 8+8+1=17 bits
  outp(0x71,((filenum>>8)&0xff));                    // 8 bits filenum
  outp(0x70,5);
  shortdelay(1); // ???
  outp(0x71,(percent&0x7f)|((filenum>>(16-7))&0x80)); // 7 (lower) bits percent & 1 (higher) bit filenum
 }
 MPXPLAY_INTSOUNDDECODER_ALLOW;
#endif
}

static void mpxplay_control_startup_loadfromcmos_listnum(void)
{
#ifdef __DOS__
 unsigned int c2,c3;

 if((playstartlist<0) && mpxplay_control_fastlist_enabled()){
  outp(0x70,3);
  shortdelay(3);
  c2=inp(0x71);
  outp(0x70,5);
  shortdelay(3);
  c3=inp(0x71);
  playstartlist=((c2>>6)&0x03)|((c3>>(7-2))&0x04);
 }
#endif
}

static void mpxplay_control_startup_loadfromcmos_songpos(struct mainvars *mvp)
{
#ifdef __DOS__
 struct playlist_side_info *psip=mvp->psip;
 unsigned int c1,c2,c3;

 if(!playstartsong){
  outp(0x70,1);
  shortdelay(3);
  c1=inp(0x71);
  outp(0x70,3);
  shortdelay(3);
  c2=inp(0x71);
  outp(0x70,5);
  shortdelay(3);
  c3=inp(0x71);
  if(mpxplay_control_fastlist_enabled())
   playstartsong=c1|((c2&0x3f)<<8);
  else
   playstartsong=c1|(c2<<8)|((c3&0x80)<<(16-7));
  if((psip->editloadtype&PLL_TYPE_LOAD) && playstartsong<=((psip->lastentry-psip->firstsong)+1)){
   if(!playstartframe && !playstartpercent && !playstarttime)
    playstartpercent=c3&0x7f;
   playcontrol|=PLAYC_STARTNEXT;
  }
 }
#endif
}

//-------------------------------------------------------------------------
void mpxplay_control_startup_loadini(mpxini_line_t *mpxini_lines,struct mpxini_part_t *mpxini_partp)
{
 unsigned int i;
 for(i=0;i<PLAYLIST_MAX_SIDES;i++)
  su_oldorderkeys[i]=0xffffffff;
 mpxplay_control_general_loadini(mpxini_lines,mpxini_partp,startup_vars_selector);
 if(control_startup_type_override>=0)
  su_startuptype=control_startup_type_override;
 else
  su_startuptype=su_startupenabled;
 if(su_startuptype&STARTUP_FLAG_2SIDES)
  mpxplay_control_general_loadini(mpxini_lines,mpxini_partp,startup_vars_doubleside);
 else
  mpxplay_control_general_loadini(mpxini_lines,mpxini_partp,startup_vars_singleside);
#ifndef __DOS__
 funcbit_disable(su_startuptype,STARTUP_FLAG_CMOS);
#endif
}

void mpxplay_control_startup_loadlastlist(void)
{
 struct mainvars *mvp=&mvps;
 struct playlist_side_info *psi;
 unsigned int i,loadside_begin,loadside_end;
 char fullname[300];

 if(su_startuptype&STARTUP_FLAG_LOAD){
  psi=mvp->psi0;
  loadside_begin=0;
  loadside_end=PLAYLIST_MAX_SIDES-1;
  if(!mvp->psi0->editsidetype)
   loadside_begin=1;
  if(!(su_startuptype&STARTUP_FLAG_2SIDES) || (su_startuptype&STARTUP_FLAG_CMOS)
     || (playlistload&PLL_TYPE_LOAD) || drivescanletters || playlist_loadsub_getinputfile(mvp->psil))
   loadside_end=0;
  if(loadside_end>loadside_begin){
   mvp->psie=psi+su_editorside;
   mvp->psip=psi+su_playside;
   funcbit_enable(playlistload,PLL_RESTORED);
  }

  psi=mvp->psi0+loadside_begin;

  for(i=loadside_begin;i<=loadside_end;i++,psi++){
   if(!su_oldlistname[i][0])
    continue;
   if((su_startuptype&(STARTUP_FLAG_RESDIR|STARTUP_FLAG_2SIDES)) && ((psi!=mvp->psil) || (!(su_startuptype&STARTUP_FLAG_CMOS) && !(playlistload&PLL_TYPE_LOAD) && !playlist_loadsub_getinputfile(psi) && !drivescanletters))){
    psi->editloadtype=su_oldlisttype[i]; // ??? !!!
    if(psi->editloadtype&PLL_TYPE_LOAD){ // reload last playlist
     funcbit_disable(psi->editsidetype,PLT_DIRECTORY);
     if(psi->editloadtype&PLL_SUBLISTS)
      playlist_loadsub_sublist_setlevels(psi,&su_oldlistname[i][0]);
     else if(psi->editloadtype&PLL_FASTLIST)
      mpxplay_control_fastlist_searchfilename(psi,&su_oldlistname[i][0]);
     else
      playlist_loadsub_setnewinputfile(psi,&su_oldlistname[i][0],psi->editloadtype);
    }else if(su_oldlistname[i][0]){ // restore (change to) last directory
     struct playlist_side_info *psid=(su_startuptype&STARTUP_FLAG_2SIDES)? psi:mvp->psi0;
     funcbit_enable(psid->editsidetype,PLT_DIRECTORY);
     if(psi->editloadtype&PLL_SUBLISTS)
      playlist_loadsub_sublist_setlevels(psid,&su_oldlistname[i][0]);
     playlist_loaddir_initdirside(psid,&su_oldlistname[i][0]);
    }
   }else if((playlistload&PLL_FASTLIST) && (i || !(su_startuptype&STARTUP_FLAG_2SIDES))){
    char *filename;
    if(su_startuptype&STARTUP_FLAG_CMOS){
     mpxplay_control_startup_loadfromcmos_listnum();
     filename=NULL;
    }else
     filename=&su_oldlistname[i][0];
    mpxplay_control_fastlist_searchfilename(psi,filename);
   }else if(su_oldlisttype[i]&PLL_SUBLISTS){
    if(playlist_loadsub_sublist_setlevels(psi,&su_oldlistname[i][0])){ // overwrites su_oldlistname (cuts the string at the end of the first listname), but this is good/required
     mpxplay_playlist_startfile_fullpath(fullname,freeopts[OPT_INPUTFILE]);
     if(pds_stricmp(&su_oldlistname[i][0],fullname)!=0) // compare rootlistname with inputfilename
      playlist_loadsub_sublist_clear(psi);
    }else
     playlist_loadsub_sublist_clear(psi);
   }
   if(loadside_end>loadside_begin){
    playlist_editorhighline_seek(psi,su_editorhighline[i],SEEK_SET);
    playlist_sortlist_set_orderkeys_from_hexa(psi,su_oldorderkeys[i]);
   }
  }
  if(loadside_end>loadside_begin)
   playlist_jukebox_set(mvp,1);
 }

 if((su_startuptype&STARTUP_FLAG_CMOS) && (su_startuptype&STARTUP_FLAG_SAVE) && !(mvp->aui->card_handler->infobits&SNDCARD_IGNORE_STARTUP)){
  mpxplay_timer_addfunc(&mpxplay_control_startup_savetocmos,NULL,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_MVPDATA,mpxplay_timer_secs_to_counternum(CMOS_SAVE_REFRESH));
  mpxplay_timer_addfunc(&mpxplay_control_startup_savetocmos,NULL,MPXPLAY_TIMERTYPE_SIGNAL|MPXPLAY_TIMERFLAG_MULTIPLY|MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_MVPDATA,MPXPLAY_SIGNALTYPE_NEWFILE);
 }
}

void mpxplay_control_startup_getstartpos(mainvars *mvp)
{
 char *s,fullname[300];

 if((su_startuptype&STARTUP_FLAG_LOAD) && !(mvp->aui->card_handler->infobits&SNDCARD_IGNORE_STARTUP)){
  if(su_startuptype&STARTUP_FLAG_CMOS){
   mpxplay_control_startup_loadfromcmos_songpos(mvp);
  }else{
   if(freeopts[OPT_INPUTFILE]){
    mpxplay_playlist_startfile_fullpath(fullname,freeopts[OPT_INPUTFILE]);
    s=&fullname[0];
   }else{
    if(drivescanletters){
     pds_strcpy(fullname,drivescanletters);
     s=&fullname[0];
    }else{
     fullname[0]=0;
     s=NULL;
    }
   }

   if(!playstartsong && (((su_oldlisttype[su_playside]==mvp->psip->editloadtype) && (!s || pds_stricmp(&su_oldlistname[su_playside][0],fullname)==0)) || (pds_stricmp(su_oldsongname,fullname)==0))){
    struct playlist_side_info *psi=mvp->psip;
    struct playlist_entry_info *pei=NULL;
    unsigned long samplenum=PCM_OUTSAMPLES,freq=44100;
    long timempos;

    if(su_oldsongnum){
     pei=psi->firstsong+su_oldsongnum-1;
     if(pds_stricmp(pei->filename,su_oldsongname)!=0)
      pei=NULL;
    }
    if(!pei)
     pei=playlist_search_filename(psi,su_oldsongname,-1,NULL); // !!! searching for the first, not for the closest
    if(!pei)
     return;
    playlist_randlist_delete(pei); // because it's pushed back again at song-start

    if(pei->infobits&PEIF_INDEXED){ // then get freq for the correct indexed songpos-restoring
     if(mpxplay_infile_get_header_by_ext(mvp->frp0+2,pei->mdds,su_oldsongname)){
      freq=(mvp->frp0+2)->infile_infos->audio_decoder_infos->freq;
      if(freq){
       samplenum=mpxplay_infile_get_samplenum_per_frame(freq);
       timempos=(long)(1000.0*((float)su_oldframenum*(float)samplenum+((float)freq/2))/(float)freq);
       mpxplay_infile_close(mvp->frp0+2);
       if((timempos<pei->pstime) || (pei->petime && (timempos>=pei->petime)))
        pei=playlist_search_filename(psi,su_oldsongname,timempos,NULL); // !!! detto
       if(!pei)
        return;
      }
     }
    }
    playstartsong=pei-psi->firstsong+1;

    if((playlistload&PLL_TYPE_ALL) || !playlistload || (su_startuptype&(STARTUP_FLAG_RESDIR|STARTUP_FLAG_2SIDES))){
     if(!playstartframe && !playstartpercent && !playstarttime){
      if((pei->infobits&PEIF_INDEXED) && pei->pstime)
       su_oldframenum-=(long)((float)pei->pstime*(float)freq/(float)samplenum/1000.0);
      playstartframe=su_oldframenum;
     }
     if(su_startuptype&STARTUP_FLAG_RESDIR)
      funcbit_enable(playcontrol,PLAYC_STARTNEXT);
    }
   }
  }
 }
}

void mpxplay_control_startup_saveini(mpxini_line_t *mpxini_lines,struct mpxini_part_t *mpxini_partp,FILE *configfile)
{
 struct mainvars *mvp=&mvps;
 struct playlist_entry_info *pei;
 char sout[MPXINI_MAX_CHARSPERLINE];

 if((su_startuptype&STARTUP_FLAG_SAVE) && mvp->aui && mvp->aui->card_handler && !(mvp->aui->card_handler->infobits&SNDCARD_IGNORE_STARTUP)){
  if(su_startuptype&STARTUP_FLAG_CMOS)
   mpxplay_control_startup_savetocmos(mvp);
  else{
   struct playlist_side_info *psi,*psip=mvp->psip;
   struct frame *frp0;
   mpxini_var_s *varp;
   long index_pos,i;

   fseek(configfile,mpxini_partp->filepos,SEEK_SET); // !!!

   if(su_startuptype&STARTUP_FLAG_2SIDES){
    psi=mvp->psi0;
    varp=startup_vars_doubleside;
    su_editorside=mvp->psie-mvp->psi0;
    su_playside=mvp->psip-mvp->psi0;
   }else{
    psi=psip;
    varp=startup_vars_singleside;
   }

   for(i=0;i<((su_startuptype&STARTUP_FLAG_2SIDES)? PLAYLIST_MAX_SIDES:1);i++,psi++){

    if(su_startuptype&STARTUP_FLAG_2SIDES){
     playlist_savelist_save_editedside(psi);
     su_editorhighline[i]=psi->editorhighline-psi->firstentry;
     su_oldorderkeys[i]=playlist_sortlist_get_orderkeys_in_hexa(psi);
    }

    su_oldlisttype[i]=(psi->editloadtype&(PLL_TYPE_ALL|PLL_DOOMBOX));
    su_oldlistname[i][0]=0;
    if(playlist_loadsub_sublist_getlevels(psi,&su_oldlistname[i][0],MPXINI_MAX_CHARSPERLINE-1)){ // sublist
     funcbit_enable(su_oldlisttype[i],PLL_SUBLISTS);
     if(psi->editsidetype&PLT_DIRECTORY)    // in db
      funcbit_disable(su_oldlisttype[i],PLL_TYPE_LOAD);
    }else{
     funcbit_disable(su_oldlisttype[i],PLL_SUBLISTS);
     if(psi->editsidetype&PLT_DIRECTORY){
      pds_strcpy(&su_oldlistname[i][0],psi->currdir);
      mpxplay_diskdrive_drive_config(psi->mdds,MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_REALLYFULLPATH,&su_oldlistname[i][0],&su_oldlistname[i][0]);
      funcbit_disable(su_oldlisttype[i],PLL_TYPE_ALL);
     }else{
      if(drivescanletters && (psi==mvp->psil) && !(su_startuptype&STARTUP_FLAG_2SIDES)) // drive scan
       pds_strcpy(&su_oldlistname[i][0],drivescanletters);
      else
       pds_strcpy(&su_oldlistname[i][0],&psi->sublistnames[psi->sublistlevel][0]);
     }
    }
   }

   frp0=mvp->frp0;
   pei=((mvp->adone==ADONE_EOF) && (mvp->aktfilenum==psip->lastentry) && (mvp->aktfilenum>=psip->firstsong))? psip->firstsong:
       (mvp->aktfilenum>=psip->firstsong && mvp->aktfilenum<=psip->lastentry)? mvp->aktfilenum:NULL;
   if(pei){
    su_oldsongname=pei->filename;
    su_oldsongnum=pei-psip->firstsong+1;
   }else{
    su_oldsongname=NULL;
    su_oldsongnum=0;
   }
   index_pos=frp0->frameNum-frp0->index_start;
   su_oldframenum=((mvp->adone==ADONE_EOF) || (index_pos<100))? frp0->index_start:(frp0->frameNum-10);

   while(varp->name){
    if(varp->type&ARG_CHAR){
     if(varp->type&ARG_POINTER)
      snprintf(sout,sizeof(sout),"%-16s=%s\n",varp->name,(*((char **)varp->c))? (*((char **)varp->c)):"");
     else
      snprintf(sout,sizeof(sout),"%-16s=%s\n",varp->name,(*((char *)varp->c))? ((char *)varp->c):"");
    }else{
     if(varp->type&ARG_HEX)
      snprintf(sout,sizeof(sout),"%-16s=%8.8X\n",varp->name,*((unsigned int *)varp->c));
     else
      snprintf(sout,sizeof(sout),"%-16s=%-5d\n",varp->name,*((unsigned int *)varp->c));
    }
    fputs(sout,configfile);
    varp++;
   }

   pds_memset(sout,32,64);
   sout[0]=';';
   sout[64]=0;
   fputs(sout,configfile);
   pds_chsize(configfile->_handle,pds_tell(configfile->_handle)); // !!!
  }
 }
}
