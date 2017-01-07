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
//function:check playlist entries

#include "newfunc\newfunc.h"
#include "control\control.h"
#include "display\display.h"
#include "deparser\tagging.h"
#include "playlist.h"
#include "mpxinbuf.h"
#include <ctype.h>

static void get_fileinfos_under_play(struct mainvars *mvp);
static unsigned int chkentry_get_onefileinfos_check(struct playlist_side_info *psi,struct playlist_entry_info *pei);
static unsigned int chkentry_get_onefileinfos_common(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct frame *frp,unsigned int id3loadflags);
static unsigned int get_onefileinfos_from_tagset(struct playlist_entry_info *pei_dest);
static unsigned int get_onefileinfos_from_previndex(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int id3loadflags);
static unsigned int get_onefileinfos_from_anyside(struct playlist_side_info *psi_dest,struct playlist_entry_info *pei_dest,struct playlist_side_info *psi_src,unsigned int id3loadflags);
static void get_onefileinfos_from_file(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct frame *frp,unsigned int id3loadflags,unsigned int found);
static void create_id3infos_from_filename(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int id3loadflags);

extern char *id3tagset[I3I_MAX+1];
extern unsigned int playcontrol,playrand,preloadinfo,loadid3tag,id3textconv;
extern unsigned int textscreen_maxy,refdisp,fullelapstime,timemode,loadid3list;
extern unsigned int is_lfn_support,uselfn,desktopmode,intsoundcontrol;

void playlist_chkentry_get_allfileinfos(struct mainvars *mvp)
{
 struct playlist_side_info *psi=mvp->psi0;
 struct playlist_entry_info *pei,*dispend;
 int side,allsongs;
 char sout[64];

 for(side=0;side<PLAYLIST_MAX_SIDES;side++,psi++){
  if(psi->editsidetype&PLT_ENABLED){
   if((psi->editsidetype&PLT_EXTENDED) || playlist_sortlist_is_preordered_type(psi->id3ordertype[0]))
    playlist_order_side(psi);
   if((psi->editsidetype&PLT_EXTENDED) && !(psi->editsidetype&PLT_EXTENDINC))
    playlist_fulltime_getside(psi);
   else{
    switch(preloadinfo){
     case PLI_PRELOAD:  // -irl
	  pei=psi->firstsong;
	  dispend=pei+textscreen_maxy;
	  allsongs=psi->lastentry-pei+1;
          display_clear_timed_message();
	  while(pei<=psi->lastentry){
           if(pds_kbhit())
            if(pds_extgetch()==KEY_ESC)
             break;
	   sprintf(sout,"Preloading file informations (%2d/%d) ",pei-psi->firstsong+1,allsongs);
	   display_message(0,0,sout);
           display_message(1,0,"(press ESC to terminate it)");
	   chkentry_get_onefileinfos_check(psi,pei);
	   playlist_order_entry_block(psi,pei,psi->firstentry,pei);
	   if(psi==mvp->psip)
	    draw_browserbox(mvp,pei);
	   if(pei<dispend)
	    draweditor(mvp);
           pei++;
	  }
	  clear_message();
	  if(pei<=psi->lastentry)
	   playlist_chkfile_start_norm(psi,pei);
          break;
     case PLI_PLAYLOAD: // -ipl
          playlist_chkfile_start_norm(psi,NULL);
          break;
    }
   }
   funcbit_disable(psi->editsidetype,(PLT_EXTENDED|PLT_EXTENDINC));
  }
 }
 if(preloadinfo==PLI_PRELOAD) // !!!
  preloadinfo=PLI_DISPLOAD;   //
}

static void get_fileinfos_under_play(struct mainvars *mvp)
{
 unsigned int fastfound,fastcount,side;
 struct playlist_side_info *psi=mvp->psi0;
 struct playlist_entry_info *pei,*spei;
 static unsigned int disp_counter;
 char sout[64];

 side=0;
 do{
  if(psi->chkfilenum_curr)
   break;
  if(++side>=PLAYLIST_MAX_SIDES){
   mpxplay_timer_deletefunc(&get_fileinfos_under_play,NULL);
   return;
  }
  psi++;
 }while(1);

 pei=psi->chkfilenum_curr;
 if(pei<=psi->chkfilenum_end){
  fastcount=5;
  do{
   fastfound=chkentry_get_onefileinfos_check(psi,pei);
   switch(preloadinfo){
    case PLI_DISPLOAD:
     display_draw_editor_oneline(psi,pei);
     pei++;
     break;
    default:
     spei=playlist_order_entry_block(psi,pei,psi->firstentry,pei);
     if(!spei || spei==pei)
      display_draw_editor_oneline(psi,pei);
     else
      refdisp|=RDT_EDITOR; // ??? better solution?
     pei++;
   }
  }while(fastfound && (--fastcount) && (pei<=psi->chkfilenum_end));
  psi->chkfilenum_curr=pei;
  if(!(playcontrol&PLAYC_RUNNING)){
   if((fastcount || !disp_counter) && (pei<=psi->chkfilenum_end)){
    sprintf(sout,"Loading file informations (%2d/%d)",pei-psi->firstsong,psi->lastentry-psi->firstsong+1);
    display_message(0,0,sout);
    display_message(1,0,"");
    disp_counter=15;
   }
   if(disp_counter)
    disp_counter--;
  }
 }else{
  playlist_chkfile_stop(psi);
  if(psi==mvp->psip)
   refdisp|=RDT_BROWSER;
  refdisp|=RDT_EDITOR;
 }
}

//--------------------------------------------------------------------------

void playlist_chkentry_get_onefileinfos_open(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 if((pei<psi->firstentry) || (pei>psi->lastentry))
  return;
 if(pei->entrytype==DFT_NOTCHECKED){
  pei->entrytype=DFT_UNKNOWN;
  chkentry_get_onefileinfos_common(psi,pei,psi->mvp->frp0+2,loadid3tag);
  mpxplay_infile_close(psi->mvp->frp0+2);
 }
} // file is closed

static unsigned int chkentry_get_onefileinfos_check(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 unsigned int fastfound;
 struct pds_find_t ffblk;

 if((pei<psi->firstentry) || (pei>psi->lastentry))
  return 0;
 if((desktopmode&DTM_MASK_COMMANDER)==DTM_MASK_COMMANDER){
  if(!pei->filesize || !pei->filedate.month){
   if(!pei->mdds)
    pei->mdds=playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(pei->filename));
   fastfound=mpxplay_diskdrive_findfirst(pei->mdds,pei->filename,_A_NORMAL,&ffblk);
   if(fastfound==0){
    pei->filesize=ffblk.size;
    *((mpxp_uint32_t *)&pei->filedate)=*((mpxp_uint32_t *)&ffblk.fdate);
    mpxplay_diskdrive_findclose(pei->mdds,&ffblk);
    funcbit_enable(psi->editloadtype,PLL_CHG_LEN);
   }
  }
  fastfound=EDITLIST_MODE_HEAD;
 }else{
  fastfound=chkentry_get_onefileinfos_common(psi,pei,psi->mvp->frp0+2,(loadid3tag|ID3LOADMODE_OTHERENTRY|ID3LOADMODE_FASTSEARCH));
  mpxplay_infile_close(psi->mvp->frp0+2);
 }
 return fastfound;
} // file is closed

void playlist_chkentry_get_onefileinfos_is(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 if((pei<psi->firstentry) || (pei>psi->lastentry))
  return;
 if(pei->entrytype==DFT_NOTCHECKED)
  pei->entrytype=DFT_UNKNOWN;
 chkentry_get_onefileinfos_common(psi,pei,psi->mvp->frp0+2,loadid3tag);
} // file is open

void playlist_chkentry_get_onefileinfos_allagain(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct frame *frp,unsigned int id3loadflags)
{
 pei->entrytype=DFT_UNKNOWN;
 funcbit_disable(pei->infobits,(PEIF_ID3EXIST|PEIF_ID3LOADED));
 chkentry_get_onefileinfos_common(psi,pei,frp,id3loadflags);
 if(pei==psi->mvp->aktfilenum)
  playlist_pei0_set(psi->mvp,pei,0);
} // file is open

static unsigned int chkentry_get_onefileinfos_common(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct frame *frp,unsigned int id3loadflags)
{
 unsigned int found=0,exist=0;

 if((pei<psi->firstentry) || (pei>psi->lastentry))
  return found;
 if(pei->entrytype&(DFTM_DIR|DFTM_DRIVE))
  return (EDITLIST_MODE_HEAD|EDITLIST_MODE_ID3);
 if(pei->infobits&PEIF_ENABLED)
  exist|=EDITLIST_MODE_HEAD;
 if(pei->infobits&PEIF_ID3EXIST)
  exist|=EDITLIST_MODE_ID3;
 playlist_fulltime_del(psi,pei);
 if((pei->entrytype!=DFT_UNKNOWN) && (pei->infobits&PEIF_ENABLED))// && (!(pei->infobits&PEIF_INDEXED) || (pei->entrytype>=DFT_AUDIOFILE)))
  found|=EDITLIST_MODE_HEAD;
 if((pei->infobits&PEIF_ID3EXIST) || !funcbit_test(id3loadflags,ID3LOADMODE_ALL))
  found|=EDITLIST_MODE_ID3;
 if(found!=(EDITLIST_MODE_HEAD|EDITLIST_MODE_ID3)){
  found|=get_onefileinfos_from_tagset(pei);
  found|=get_onefileinfos_from_id3list(psi,pei,id3loadflags);
  if(found!=(EDITLIST_MODE_HEAD|EDITLIST_MODE_ID3))
   found|=get_onefileinfos_from_previndex(psi,pei,id3loadflags);
  if(found!=(EDITLIST_MODE_HEAD|EDITLIST_MODE_ID3))
   found|=get_onefileinfos_from_anyside(psi,pei,psi->psio,id3loadflags);
  if(!funcbit_test(id3loadflags,ID3LOADMODE_FILE) || funcbit_test(id3loadflags,ID3LOADMODE_PREFER_LIST))
   create_id3infos_from_filename(psi,pei,id3loadflags);
  if(!(found&EDITLIST_MODE_HEAD))
   get_onefileinfos_from_file(psi,pei,frp,id3loadflags,found);
  if(funcbit_test(id3loadflags,ID3LOADMODE_FILE) && !funcbit_test(id3loadflags,ID3LOADMODE_PREFER_LIST))
   create_id3infos_from_filename(psi,pei,id3loadflags);
  if((pei->infobits&PEIF_ENABLED) && !(exist&EDITLIST_MODE_HEAD)) // ??? we should check deeper the length and id3 changes
   funcbit_enable(psi->editloadtype,PLL_CHG_LEN); // file infos have changed
  if((pei->infobits&PEIF_ID3EXIST) && !(exist&EDITLIST_MODE_ID3))
   funcbit_enable(psi->editloadtype,PLL_CHG_ID3); // id3 infos have changed
 }
 playlist_fulltime_add(psi,pei);

 return (found&EDITLIST_MODE_HEAD);
}

static unsigned int get_onefileinfos_from_tagset(struct playlist_entry_info *pei_dest)
{
 unsigned int i;

 for(i=0;i<=I3I_MAX;i++)
  if(id3tagset[i])
   pei_dest->id3info[i]=id3tagset[i];

 if(pei_dest->id3info[I3I_ARTIST] || pei_dest->id3info[I3I_TITLE])
  return EDITLIST_MODE_ID3;

 return 0;
}

static unsigned int get_onefileinfos_from_previndex(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int id3loadflags)
{
 struct playlist_entry_info *pen=NULL;
 unsigned int found=0;
 if(!funcbit_test(id3loadflags,ID3LOADMODE_LIST) || !funcbit_test(id3loadflags,ID3LOADMODE_OTHERENTRY) || !(pei->infobits&PEIF_INDEXED))
  return found;
 while((pen=playlist_search_filename(psi,pei->filename,-1,pen))!=NULL){
  if(pen->entrytype>=DFT_AUDIOFILE){
   found|=playlist_editlist_addfile_one(psi,psi,pen,pei,(EDITLIST_MODE_HEAD|EDITLIST_MODE_ID3));
   break;
  }
  pen++;
 }
 return found;
}

static unsigned int get_onefileinfos_from_anyside(struct playlist_side_info *psi_dest,struct playlist_entry_info *pei_dest,struct playlist_side_info *psi_src,unsigned int id3loadflags)
{
 struct playlist_entry_info *pei_src;
 unsigned int modify;

 if(!funcbit_test(id3loadflags,ID3LOADMODE_LIST) || !funcbit_test(id3loadflags,ID3LOADMODE_OTHERENTRY) || !funcbit_test(psi_src->editsidetype,PLT_ENABLED) || (psi_src->lastentry<psi_src->firstsong))
  return 0;
 modify=0;
 for(pei_src=psi_src->firstsong;pei_src<=psi_src->lastentry;pei_src++){
  if((pei_src!=pei_dest) && (pds_stricmp(pei_src->filename,pei_dest->filename)==0)){
   if((pei_dest->entrytype==DFT_NOTCHECKED || pei_dest->entrytype==DFT_UNKNOWN) && (pei_src->entrytype!=DFT_UNKNOWN) && (pei_src->entrytype!=DFT_NOTCHECKED || (id3loadflags&ID3LOADMODE_FASTSEARCH)) && (pei_src->infobits&PEIF_ENABLED) && !(pei_src->infobits&PEIF_INDEXED))
    modify|=EDITLIST_MODE_HEAD;
   if(pei_src->infobits&PEIF_ID3EXIST)
    if( (!(pei_dest->infobits&PEIF_ID3EXIST) || ((pei_src->infobits&PEIF_ID3MASK) > (pei_dest->infobits&PEIF_ID3MASK)) ) && !(pei_src->infobits&PEIF_INDEXED))
     modify|=EDITLIST_MODE_ID3;
   if(modify)
    modify=playlist_editlist_addfile_one(psi_src,NULL,pei_src,pei_dest,modify);
   break;
  }
 }
 return modify;
}

static void get_onefileinfos_from_file(struct playlist_side_info *psi,struct playlist_entry_info *pei,struct frame *frp,unsigned int id3loadflags,unsigned int found)
{
 unsigned int i;
 struct mpxplay_infile_info_s *miis;
 struct mpxplay_audio_decoder_info_s *adi;
 struct playlist_entry_info *pei_tmp;
 char *id3p_old,*id3p_new;
 struct playlist_entry_info pei_save;

 if(!pei->mdds)
  pei->mdds=playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(pei->filename));

 // is file playlist?
 if(playlist_loadlist_get_header_by_ext(psi,pei,pei->filename))
  return;

 pei->entrytype=DFT_UNKNOWN;
 funcbit_disable(pei->infobits,PEIF_ENABLED);

 // is file audio?
 if(!mpxplay_infile_get_header_by_ext(frp,pei->mdds,pei->filename))
  return;

 miis=frp->infile_infos;
 adi=miis->audio_decoder_infos;

 if(frp->filetype && frp->infile_funcs && miis->timemsec){
  pei->filesize=frp->filesize;
  pei->timemsec=miis->timemsec;
  pei->mdds=frp->mdds;
  pei->infile_funcs=frp->infile_funcs;
  funcbit_enable(pei->infobits,PEIF_ENABLED);
  pei->entrytype=((unsigned long)frp->filetype<<28);
 }else{
  mpxplay_infile_close(frp);
  return;
 }

 //we have to run get_headerinfo (to open infile) before get_id3tag (it's done above)
 if((id3loadflags&ID3LOADMODE_FILE) && (!(found&EDITLIST_MODE_ID3) || !(pei->infobits&PEIF_ID3LOADED))){
  id3p_old=psi->id3infolastp;
  if((id3p_old<psi->id3infoendp) && (psi->lastentry<(psi->endentry+1))){
   psi->lastentry++;
   pei_tmp=psi->lastentry;

   pds_memcpy(&pei_save,pei_tmp,sizeof(struct playlist_entry_info));
   pds_memcpy(pei_tmp,pei,sizeof(struct playlist_entry_info));
   pds_memset(&pei_tmp->id3info[0],0,(I3I_MAX+1)*sizeof(char *));

   id3p_new=mpxplay_infile_get_id3tag(frp,&pei_tmp->id3info[0],id3p_old);                // load tag infos from audio file

   // keep old (if new doesn't exist), delete old (if new exists), add new id3infos (MXU/EXTM3U/id3list vs. file-tags (ID3v1,v2,APETAG,etc.))
   if(id3p_new>id3p_old){                        // audio file has id3-tag
    pei->infobits|=PEIF_ID3EXIST|PEIF_ID3LOADED;
    psi->id3infolastp=id3p_new;
    for(i=0;i<=I3I_MAX;i++)
     if(!pei_tmp->id3info[i] || (pei->id3info[i] && ((id3loadflags&ID3LOADMODE_PREFER_LIST) || (pei->infobits&PEIF_INDEXED))))
      pds_memxch((char *)&pei_tmp->id3info[i],(char *)&pei->id3info[i],sizeof(char *));

    playlist_editlist_delfile_one(psi,pei,EDITLIST_MODE_ID3); // delete old (duplicated,non-used) id3-infos

    pds_memcpy((char *)&pei->id3info[0],(char *)&pei_tmp->id3info[0],(I3I_MAX+1)*sizeof(char *));
   }
   pds_memcpy(pei_tmp,&pei_save,sizeof(struct playlist_entry_info));
   psi->lastentry--;
  }else{
   if(!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE]){
    playlist_editlist_delfile_one(psi,pei,EDITLIST_MODE_ID3);
    if(psi->id3infolastp<psi->id3infoendp){
     psi->id3infolastp=mpxplay_infile_get_id3tag(frp,&pei->id3info[0],id3p_old); // load tag infos from audio file
     pei->infobits|=PEIF_ID3EXIST|PEIF_ID3LOADED;
    }
   }
  }
 }
}

//--------------------------------------------------------------------------
static void ciff_add_id3info(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int i3i,char *str)
{
 if(!pei->id3info[i3i] && str && str[0]){
  char *id3p=psi->id3infolastp,*sp=str;
  unsigned int len;
  do{
   if(sp[0]=='_')  // convert all underlines to spaces
    sp[0]=' ';
   sp++;
  }while(sp[0]);

  pds_strcutspc(str);

  if(!str[0])
   return;
  len=pds_strcpy(id3p,str);
  if(len){
   if(!(id3textconv&ID3TEXTCONV_FILENAME))
    len=mpxplay_playlist_textconv_do(id3p,len,0);
   if(len){
    pei->id3info[i3i]=id3p;
    psi->id3infolastp+=len+1;
    funcbit_enable(pei->infobits,PEIF_ID3EXIST);
   }
  }
 }
}

static long ciff_add_tracknum(struct playlist_side_info *psi,struct playlist_entry_info *pei,char *str)
{
 long tracknum;
 while(str[0]==' ') // skip spaces
  str++;
 if(!str[0])
  return 0;
 tracknum=pds_atol(str);
 if(tracknum<=0 || (tracknum<10 && str[0]!='0')) // 01-09,10-NNN
  return 0;
 if(!pei->id3info[I3I_TRACKNUM]){
  char *id3p=psi->id3infolastp;
  pei->id3info[I3I_TRACKNUM]=id3p;
  id3p+=sprintf(id3p,"%d",tracknum)+1;
  psi->id3infolastp=id3p;
 }
 return tracknum;
}

// recommended/handled formats are:
//  Artist - Album (year) \ NN. Title.mp3
//  Album (year) \ NN. Artist - Title.mp3
//  Album (year) \ Artist - NN - Title.mp3
static void create_id3infos_from_filename(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int id3loadflags)
{
 char *fn,*a,*t,*album,*t2,*e,*ext;
 long tracknum=0,endtrnum=0,len;
 unsigned int original_title=0,original_artist=0,long_tr_selector=0,long_at_selector=0;
 char filename[MAX_PATHNAMELEN],updirname[MAX_PATHNAMELEN];

 if(!(id3loadflags&ID3LOADMODE_CLFN))
  return;
 if(!is_lfn_support || !(uselfn&USELFN_ENABLED))
  return;
 if((pei->entrytype>=DFTM_DFT) && (pei->entrytype<DFT_AUDIOFILE))
  return;
 if((pei->id3info[I3I_ARTIST] && pei->id3info[I3I_TITLE]) && pei->id3info[I3I_ALBUM] && pei->id3info[I3I_TRACKNUM] && (pds_atol(pei->id3info[I3I_TRACKNUM])>0))
  return;

 fn=pds_strrchr(pei->filename,PDS_DIRECTORY_SEPARATOR_CHAR);
 if(fn)
  fn++;
 else
  fn=pei->filename;

 pds_strcpy(filename,fn);
 fn=&filename[0];

 ext=pds_strrchr(fn,'.'); // cut file extension
 if(!ext)
  return;
 *ext=0;

 // get tracknum from the end or the begin of filename
 if(pds_strlicmp(fn,"track")==0){  // trackNN
  tracknum=ciff_add_tracknum(psi,pei,&fn[sizeof("track")-1]);
  if(tracknum>0)
   endtrnum=1;
 }
 if(tracknum<=0)                  // NN artist-title
  tracknum=ciff_add_tracknum(psi,pei,fn);
 if(tracknum<=0){                 // ??? search for "anythingNN" format
  len=pds_strlen(fn);
  if((len>=3) && isdigit(fn[len-1]) && isdigit(fn[len-2]) && !isdigit(fn[len-3]) && (fn[len-3]!='\'')){
   tracknum=ciff_add_tracknum(psi,pei,&fn[len-2]);
   if(tracknum>0)
    endtrnum=1;
  }
 }

 if(pei->id3info[I3I_ARTIST] && pei->id3info[I3I_TITLE] && pei->id3info[I3I_ALBUM])
  return;
 if(pei->id3info[I3I_ARTIST])
  original_artist=1;
 if(pei->id3info[I3I_TITLE])
  original_title=1;

 a=fn;   // skip tracknum (at begin of filename)
 t=fn;
 do{
  if(t[0]=='_')  // convert all underlines to spaces
   t[0]=' ';
  t++;
 }while(t[0]);

 if((tracknum>0) && !endtrnum){
  while((a[0]>='0' && a[0]<='9') || a[0]==' ') // skip numbers (tracknum) and spaces
   a++;
  if(pds_strncmp((a-1)," - ",3)==0) // sign " - " selector
   long_tr_selector=1;
  while(a[0]=='.'){ // skip dots
   long_tr_selector=1; // hack
   a++;
  }
 }
 while(a[0]==' ' || a[0]=='-' || a[0]=='_')  // skip spaces,'-','_'
  a++;
 if(!a[0])       // no more data (end of filename)
  return;

 fn=a;           // possible artist begins here

 if(pds_strlicmp(a,"track")==0){ // possibly no real artist-title
  t=fn;
  goto skip_artist_title;
 }

 t=pds_strstr(fn," - ");      // search begin of title
 if(t){
  e=t;  // end of artist
  t+=3; // begin of title
  long_at_selector=1;
 }else{
  t=pds_strchr(fn,'-');
  if(t){
   e=t;
   t++;
  }
 }

 if(t){
  a=fn; // possible artist
  if(tracknum<=0){
   t2=pds_strchr(t,'-');         // check "artist - NN - title" format
   if(t2){
    tracknum=ciff_add_tracknum(psi,pei,t);
    if(tracknum>0)
     t=t2+1;      // title
   }
  }else
   t2=NULL;

  while(t[0] && t[0]==' ') // title: cut spaces from begin
   t++;

  if(t[0] || (t2 && tracknum))
   if(
    ((long_at_selector) || (!long_tr_selector && !long_at_selector))
    &&
    (
     ((a[0]>='A') && (a[0]<='Z') && (t[0]>='A') && (t[0]<='Z')) // case of begin chars are the same
     || ((a[0]>='a') && (a[0]<='z') && (t[0]>='a') && (t[0]<='z')) //
     || (!((a[0]>='A') && (a[0]<='Z')) && !((a[0]>='a') && (a[0]<='z'))) // non US chars in artist or title
     || (!((t[0]>='A') && (t[0]<='Z')) && !((t[0]>='a') && (t[0]<='z')))
    )
    || (t2 && tracknum)  // artist-NN-title
    //|| !t2 ){            // only one " - "
   ){
    *e=0;
    ciff_add_id3info(psi,pei,I3I_ARTIST,a);
   }else
    t=a;
 }else
  t=fn;             // title only

skip_artist_title:

 if(!endtrnum) // tracknum at end of filename is not lfn-like title
  if(((tracknum>0) || pei->id3info[I3I_ARTIST]) && !original_artist) // "NN.title.MP3" or "artist - title.MP3"
   ciff_add_id3info(psi,pei,I3I_TITLE,t);

 if(!pei->id3info[I3I_ARTIST] || !pei->id3info[I3I_ALBUM]){   // get artist - album from updir name
  pds_strcpy(updirname,pei->filename);
  e=pds_strrchr(updirname,PDS_DIRECTORY_SEPARATOR_CHAR);
  if(e){
   *e=0;
   a=pds_strrchr(updirname,PDS_DIRECTORY_SEPARATOR_CHAR); // artist
   if(a){
    a++;
    album=pds_strstr(a," - ");     // album
    if(album){
     *album=0;
     album+=3;
    }else{
     album=pds_strchr(a,'-');      //
     if(album)
      *album++=0;
    }
    if(album && album[0]){ // artist-album\NN-title.mp3
     ciff_add_id3info(psi,pei,I3I_ARTIST,a);
     ciff_add_id3info(psi,pei,I3I_TITLE,t);
    }else if(!pei->id3info[I3I_ARTIST] && (tracknum>0) && !endtrnum){ // artist\NN.title.mp3
     if(pei->id3info[I3I_TITLE] && !original_title && ((e-a)>=2))
      ciff_add_id3info(psi,pei,I3I_ARTIST,a);
    }
    if(album && album[0]){
     char *year,*yb;
     yb=year=pds_strchr(album,'(');
     if(year){
      int val;
      year++;
      if(year[0]=='\''){ // ie: '99
       year++;
       val=pds_atol(year);
       if(val<30)        // !!! will not good after 2029
        val+=2000;
       else
        val+=1900;
      }else
       val=pds_atol(year);
      e=pds_strchr(year,')');
      if(val>=1200 && val<=9999 && e){
       char ytmp[8];
       *yb=0;*e=0;
       snprintf(ytmp,sizeof(ytmp),"%4d",val);
       ciff_add_id3info(psi,pei,I3I_YEAR,ytmp);
      }
     }
     ciff_add_id3info(psi,pei,I3I_ALBUM,album);
    }
   }
  }
 }
}

//-------------------------------------------------------------------------
//static unsigned long starttime;

void playlist_chkfile_start_norm(struct playlist_side_info *psi,struct playlist_entry_info *startsong)
{
 //starttime=pds_gettimeh();
 if(psi->editsidetype&PLT_ENABLED){
  struct mainvars *mvp=psi->mvp;
  if((psi==mvp->psip) && !(psi->editsidetype&PLT_DOOMQUEUE) && (mvp->aktfilenum<psi->firstsong)){
   struct playlist_entry_info *pei;
   struct frame *frp=mvp->frp0;
   long timempos=(frp->allframes)? ((long)((float)frp->infile_infos->timemsec*(float)frp->frameNum/(float)frp->allframes)):-1;

   pei=playlist_search_filename(psi,mvp->pei0->filename,timempos,NULL);
   if(pei){
    mvp->aktfilenum=pei;
    if(pei->infobits&PEIF_INDEXED)
     playlist_chkentry_get_onefileinfos_open(psi,pei); // to get all id3 infos (not only artist/title/album from cue)
    else
     chkentry_get_onefileinfos_check(psi,pei);
    playlist_pei0_set(mvp,pei,(EDITLIST_MODE_ID3|EDITLIST_MODE_INDEX));
    playlist_randlist_pushq(psi,pei);
    mpxplay_calculate_index_start_end(frp,mvp,pei); // switch between cue and other playlist
    if((psi==mvp->psie) && (psi->editorhighline>=psi->firstentry) && (psi->editorhighline<=psi->lastentry) && !psi->editorhighline->entrytype)
     playlist_loadlist_get_header_by_ext(psi,psi->editorhighline,psi->editorhighline->filename);
    if((psi!=mvp->psie) || (psi->editorhighline<psi->firstentry) || (GET_HFT(psi->editorhighline->entrytype)!=HFT_DFT))
     playlist_editorhighline_set(psi,pei);
   }
  }
  if((desktopmode&DTM_MASK_COMMANDER)==DTM_MASK_COMMANDER) // !!!
   funcbit_enable(psi->editsidetype,PLT_EXTENDINC);        // better solution?
  if(((psi->editsidetype&PLT_EXTENDED) || playlist_sortlist_is_preordered_type(psi->id3ordertype[0])) && !psi->chkfilenum_curr){
   playlist_order_side(psi);
   playlist_fulltime_getside(psi);
  }
  if(!(psi->editsidetype&PLT_EXTENDED) || (psi->editsidetype&PLT_EXTENDINC) || psi->chkfilenum_curr){
   switch(preloadinfo){
    case PLI_PLAYLOAD:
      if(startsong<psi->firstsong)
       startsong=psi->firstsong;
      if(!psi->chkfilenum_curr || startsong<psi->chkfilenum_curr)
       psi->chkfilenum_curr=startsong;
      if(!psi->chkfilenum_begin || (psi->chkfilenum_begin>psi->chkfilenum_curr))
       psi->chkfilenum_begin=psi->chkfilenum_curr;
      psi->chkfilenum_end=psi->lastentry;
      mpxplay_timer_addfunc(&get_fileinfos_under_play,mvp,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
      break;
    case PLI_PRELOAD :
    case PLI_DISPLOAD:
      display_editorside_reset(psi);
      break;
   }
  }
  funcbit_disable(psi->editsidetype,(PLT_EXTENDED|PLT_EXTENDINC));
  if(psi==mvp->psip)
   refdisp|=RDT_EDITOR|RDT_BROWSER;
  else
   refdisp|=RDT_EDITOR;
 }
}

void playlist_chkfile_start_disp(struct playlist_side_info *psi,struct playlist_entry_info *startsong,struct playlist_entry_info *endsong)
{
 if((preloadinfo==PLI_DISPLOAD) && (psi->editsidetype&PLT_ENABLED)){
  playlist_chkfile_stop(psi);
  if(startsong<psi->firstsong)
   startsong=psi->firstsong;
  psi->chkfilenum_begin=psi->chkfilenum_curr=startsong;
  if(!endsong || (endsong>psi->lastentry))
   endsong=psi->lastentry;
  psi->chkfilenum_end=endsong;
  mpxplay_timer_addfunc(&get_fileinfos_under_play,psi->mvp,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
 }
}

void playlist_chkfile_start_ehline(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 if((preloadinfo==PLI_EHLINELOAD) && (psi->editsidetype&PLT_ENABLED)){
  if(pei>=psi->firstsong && pei<=psi->lastentry){
   struct mainvars *mvp=psi->mvp;
   if(((mvp->aui->card_infobits&AUINFOS_CARDINFOBIT_DMAFULL) && (mvp->frp0->buffertype&PREBUFTYPE_FILLED)) || !(playcontrol&PLAYC_RUNNING)){
    chkentry_get_onefileinfos_check(psi,pei);
    playlist_order_entry(psi,pei);
    if(pei->entrytype==DFT_NOTCHECKED)
     chkentry_get_onefileinfos_check(psi,pei);
   }else
    refdisp|=RDT_EDITOR;
  }
 }
}

void playlist_chkfile_stop(struct playlist_side_info *psi)
{
 //char sout[100];
 if((preloadinfo==PLI_DISPLOAD) && psi->chkfilenum_begin)
  playlist_order_block(psi,psi->chkfilenum_begin,(min(psi->chkfilenum_curr,psi->chkfilenum_end)));
 psi->chkfilenum_begin=NULL;
 psi->chkfilenum_curr=NULL;
 psi->chkfilenum_end=NULL;
 if(loadid3list==ID3LISTTYPE_LOCAL){ // ??? other cases?
  struct mainvars *mvp=psi->mvp;
  struct playlist_entry_info *pei=mvp->aktfilenum;
  if((psi==mvp->psip) && (pei>=psi->firstsong))
   playlist_pei0_set(mvp,pei,EDITLIST_MODE_ID3);
 }
 if(!(playcontrol&PLAYC_RUNNING))
  clear_message();
 //sprintf(sout,"chktime:%4d",pds_gettimeh()-starttime);
 //display_timed_message(sout);
}

//------------------------------------------------------------------------
unsigned long playlist_entry_get_timemsec(struct playlist_entry_info *pei)
{
 unsigned long timemsec;

 if(pei->infobits&PEIF_INDEXED){
  timemsec=(pei->petime)? pei->petime:pei->timemsec;
  if(pei->pstime<=timemsec)
   timemsec-=pei->pstime;
 }else
  timemsec=pei->timemsec;

 return timemsec;
}

void playlist_fulltime_add(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 if(pei->infobits&PEIF_ENABLED){
  if(!(pei->infobits&PEIF_FULLTIMEADDED)){
   struct mainvars *mvp;
   unsigned long timesec=(playlist_entry_get_timemsec(pei)+500)/1000;

   psi->fulltimesec+=timesec;
   funcbit_enable(pei->infobits,PEIF_FULLTIMEADDED);

   mvp=psi->mvp;
   if(!playrand && (psi==mvp->psip) && ((pei<mvp->aktfilenum) || (mvp->aktfilenum<psi->firstsong)))
    fullelapstime+=timesec;
  }
 }
}

void playlist_fulltime_del(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 unsigned long timesec=(playlist_entry_get_timemsec(pei)+500)/1000;

 if(pei->infobits&PEIF_FULLTIMEADDED){
  psi->fulltimesec-=timesec;
  funcbit_disable(pei->infobits,PEIF_FULLTIMEADDED);

  if(playrand){
   if(pei->infobits&PEIF_RNDPLAYED)
    fullelapstime-=timesec;
  }else{
   struct mainvars *mvp=psi->mvp;
   if((psi==mvp->psip) && ((pei<mvp->aktfilenum) || (mvp->aktfilenum<psi->firstsong)))
    fullelapstime-=timesec;
  }
 }
}

void playlist_fulltime_clearside(struct playlist_side_info *psi)
{
 psi->fulltimesec=0;
 if(psi==psi->mvp->psip)
  fullelapstime=0;
}

void playlist_fulltime_getside(struct playlist_side_info *psi)
{
 struct playlist_entry_info *pei;
 mpxp_int64_t loc_fulltime=0;

 for(pei=psi->firstsong;pei<=psi->lastentry;pei++){
  if(pei->infobits&PEIF_ENABLED){
   loc_fulltime+=playlist_entry_get_timemsec(pei);
   funcbit_enable(pei->infobits,PEIF_FULLTIMEADDED);
  }
 }
 psi->fulltimesec=(loc_fulltime+500)/1000;
}

unsigned int playlist_fulltime_getelapsed(struct mainvars *mvp,unsigned int cleartime)
{
 struct playlist_side_info *psip;
 struct playlist_entry_info *pei;
 struct playlist_entry_info *end;

 if(cleartime)
  fullelapstime=0;

 if(timemode>=2 && !fullelapstime){
  psip=mvp->psip;
  if(psip->chkfilenum_curr)
   end=psip->chkfilenum_curr;
  else
   end=psip->lastentry;
  if(end>=psip->firstsong){
   mpxp_int64_t loc_fullelapstime=0;
   if(playrand){
    for(pei=psip->firstsong;pei<=end;pei++)
     if((pei->infobits&PEIF_RNDPLAYED) && (pei->infobits&PEIF_ENABLED))
      loc_fullelapstime+=playlist_entry_get_timemsec(pei);
    if(mvp->aktfilenum>=psip->firstsong)
     loc_fullelapstime-=playlist_entry_get_timemsec(mvp->aktfilenum);
   }else{
    if(end>=mvp->aktfilenum)
     end=mvp->aktfilenum-1;
    for(pei=psip->firstsong;pei<=end;pei++)
     if(pei->infobits&PEIF_ENABLED)
      loc_fullelapstime+=playlist_entry_get_timemsec(pei);
   }
   fullelapstime=(loc_fullelapstime+500)/1000;
  }else{
   fullelapstime=0;
  }
 }
 return fullelapstime;
}

void playlist_chkentry_enable_entries(struct playlist_side_info *psi)
{
 struct playlist_entry_info *pei;
 for(pei=psi->firstsong;pei<=psi->lastentry;pei++)
  if(pei->entrytype==DFT_UNKNOWN)
   pei->entrytype=DFT_NOTCHECKED;
}
