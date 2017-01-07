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
//function:get and set file/entry infos (ID3 tag,index)

#include "newfunc\newfunc.h"
#include "control\control.h"
#include "display\display.h"
#include "deparser\tagging.h"
#include "playlist.h"
#include "mpxinbuf.h"
#include <malloc.h>
#include <string.h>

#define SHOW_FILEINFOS_ALLOCLEN    3072
#define SHOW_FILEINFOS_HEADINFOSIZE 384 // max. from "Length" to "Filedate"

extern unsigned int is_lfn_support,uselfn,refdisp,loadid3tag;

static unsigned int id3order[7]={I3I_ARTIST,I3I_TITLE,I3I_ALBUM,I3I_YEAR,I3I_GENRE,I3I_TRACKNUM,I3I_COMMENT};
static char *id3text[7]={"Artist  : ","Title   : ","Album   : ","Year    : ","Genre   : ","Track   : ","Comment : "};
static char *tagtypetext[3]={"ID3v1","ID3v2","APETag"};

static unsigned int fileinfo_build_tagtype(char *sout,struct mpxplay_infile_info_s *miis,unsigned int control_write)
{
 unsigned int i,len,tag_types=MPXPLAY_TAGTYPE_GET_FOUND(miis->standard_id3tag_support);
 unsigned int nb_tagtypes=0;
 if(tag_types){
  if(control_write)
   len=pds_strcpy(sout,"Updated tag-type(s): ");
  else
   len=0;
 }else{
  if(!control_write)
   return 0;
  tag_types=MPXPLAY_TAGTYPE_GET_PRIMARY(miis->standard_id3tag_support);
  if(!tag_types)
   return 0;
  len=pds_strcpy(sout,"Target (new) tag-type: ");
 }

 for(i=0;i<MPXPLAY_TAGTYPES_NUM;i++){
  if(tag_types&(1<<i)){
   if(nb_tagtypes)
    len+=pds_strcpy(&sout[len],", ");
   len+=pds_strcpy(&sout[len],tagtypetext[i]);
   nb_tagtypes++;
  }
 }

 return 1;
}

// make MMM:SS.HH format
static void fileinfo_maketimemstr_info(char *str,unsigned long msec)
{
 sprintf(str,"%d:%2.2d.%2.2d",(unsigned long)(msec/(60*1000)),(unsigned long)(msec%(60*1000)/1000),(unsigned long)(msec%1000/10));
}

void playlist_fileinfo_show_infos(struct mainvars *mvp)
{
 struct playlist_side_info *psi=mvp->psie;
 struct playlist_entry_info *pei=psi->editorhighline;
 struct frame *frp,fr_data;
 struct mpxplay_infile_info_s *miis;
 struct mpxplay_audio_decoder_info_s *adi;
 unsigned int len,i,ff_result;
 mpxp_int64_t timemsec;
 long timesec;
 struct pds_find_t ffblk;
 char *shortfname,*msg,strtmp[64];

 if(psi->selected_files || (pei->entrytype&(DFTM_DRIVE|DFTM_DIR))){
  playlist_diskfile_show_multifileinfos(mvp);
  return;
 }
 if(pei->entrytype&DFTM_PLAYLIST){
  display_textwin_openwindow_keymessage(TEXTWIN_FLAG_MSGCENTERALIGN," Fileinfo ","Playlist");
  return;
 }

 msg=(char *)alloca(SHOW_FILEINFOS_ALLOCLEN);
 if(!msg)
  return;

 frp=&fr_data;
 if(!mpxplay_infile_frame_alloc(frp))
  return;

 playlist_chkentry_get_onefileinfos_allagain(psi,pei,frp,((loadid3tag)? (loadid3tag|ID3LOADMODE_FILE):ID3LOADMODE_ALL));

 shortfname=pds_getfilename_from_fullname(pei->filename);

 len =pds_strcpy(msg,"Filename: ");
 len+=pds_strcat(msg,shortfname);
 len+=pds_strcat(msg,"\n");

 for(i=0;i<=I3I_MAX;i++){
  unsigned int index=id3order[i];
  if(len>=(SHOW_FILEINFOS_ALLOCLEN-SHOW_FILEINFOS_HEADINFOSIZE))
   break;
  len+=pds_strcat(msg,id3text[i]);
  if(pei->id3info[index]){
   if((len+pds_strlen(pei->id3info[index]))>(SHOW_FILEINFOS_ALLOCLEN-SHOW_FILEINFOS_HEADINFOSIZE))
    break;
   len+=pds_strcat(msg,pei->id3info[index]);
  }else
   len+=pds_strcat(msg,"-");
  len+=pds_strcat(msg,"\n");
 }

 miis=frp->infile_infos;

 /*len+=pds_strcat(msg,"Tag-type: ");
 if(fileinfo_build_tagtype(strtmp,miis,0)){
  len+=pds_strcat(msg,strtmp);
  len+=pds_strcat(msg,"\n");
 }else
  len+=pds_strcat(msg,"-\n");*/

 if(frp->infile_funcs){

  timemsec=playlist_entry_get_timemsec(pei);
  timesec=(timemsec+50)/1000;
  timemsec=(timemsec+50)%1000;
  timemsec=timemsec/100;
  sprintf(strtmp,"Length  : %d:%2.2d.%d",(timesec/60),(timesec%60),(long)timemsec);
  len+=pds_strcat(msg,strtmp);
  if(pei->infobits&PEIF_INDEXED){
   unsigned long endtime;
   len+=pds_strcat(msg," (index: ");
   fileinfo_maketimemstr_info(strtmp,pei->pstime);
   len+=pds_strcat(msg,strtmp);
   len+=pds_strcat(msg," - ");
   endtime=pei->petime;
   if(pei->petime)
    endtime=pei->petime;
   else
    endtime=pei->timemsec;
   fileinfo_maketimemstr_info(strtmp,endtime);
   len+=pds_strcat(msg,strtmp);
   len+=pds_strcat(msg,")");
  }
  len+=pds_strcat(msg,"\n");

  adi=miis->audio_decoder_infos;

  len+=pds_strcat(msg,"Filetype: ");
  if(miis->longname){
   len+=pds_strcat(msg,miis->longname);
   len+=pds_strcat(msg,"\n");
  }else{
   char *e;
   if(frp->infile_funcs && frp->infile_funcs->file_extensions[0])
    e=frp->infile_funcs->file_extensions[0];
   else{
    e=pds_strrchr(shortfname,'.');
    if(e)
     e++;
   }
   if(e || adi->shortname){
    sprintf(strtmp,"%3.3s->%3.3s\n",((e)? e:"???"),(adi->shortname? adi->shortname:"???"));
    len+=pds_strcat(msg,strtmp);
   }else
    len+=pds_strcat(msg,"n/a\n");
  }

  len+=pds_strcat(msg,"Freq    : ");
  if(adi->freq<100000)
   sprintf(strtmp,"%dHz\n",adi->freq);
  else if(adi->freq<10000000)
   sprintf(strtmp,"%dkHz\n",adi->freq/1000);
  else
   sprintf(strtmp,"%2.1fMHz\n",(float)adi->freq/1000000);
  len+=pds_strcat(msg,strtmp);

  len+=pds_strcat(msg,"Channels: ");
  if(adi->channeltext){
   len+=pds_strcat(msg,adi->channeltext);
   len+=pds_strcat(msg,"\n");
  }else{
   switch(adi->filechannels){
    case 1:len+=pds_strcat(msg,"Mono\n");break;
    case 2:len+=pds_strcat(msg,"Stereo\n");break;
    default:sprintf(strtmp,"%d\n",adi->filechannels);
            len+=pds_strcat(msg,strtmp);
            break;
   }
  }

  len+=pds_strcat(msg,"Bit/rate: ");
  if(adi->bitratetext){
   len+=pds_strcat(msg,adi->bitratetext);
   len+=pds_strcat(msg,"\n");
  }else{
   if(adi->bitrate)
    sprintf(strtmp,"%d kbit/s\n",adi->bitrate);
   else
    sprintf(strtmp,"%d\n",adi->bits);
   len+=pds_strcat(msg,strtmp);
  }

 }

 ff_result=mpxplay_diskdrive_findfirst(pei->mdds,pei->filename,_A_NORMAL,&ffblk);

 len+=pds_strcat(msg,"Filesize: ");

 if(pei->filesize)
  sprintf(strtmp,"%1.2f MB",(float)pei->filesize/1048576.0);
 else if(ff_result==0)
  sprintf(strtmp,"%1.2f MB",(float)ffblk.size/1048576.0);
 else
  pds_strcpy(strtmp,"-");

 len+=pds_strcat(msg,strtmp);

 if(ff_result==0){
  struct pds_fdate_t *fdate=&ffblk.fdate;
  sprintf(strtmp,"\nFiledate: %4.4d.%2.2d.%2.2d. %2d:%2.2d",fdate->year+1980,
   fdate->month,fdate->day,fdate->hours,fdate->minutes);
  len+=pds_strcat(msg,strtmp);
  if(is_lfn_support && (uselfn&USELFN_ENABLED)){
   struct pds_fdate_t *cdate=&ffblk.cdate;
   struct pds_fdate_t *adate=&ffblk.adate;
   if(!cdate->year && !cdate->month && !cdate->day && !cdate->hours && !cdate->minutes)
    sprintf(strtmp,"\nCreated : -");
   else
    sprintf(strtmp,"\nCreated : %4.4d.%2.2d.%2.2d. %2d:%2.2d",cdate->year+1980,
   cdate->month,cdate->day,cdate->hours,cdate->minutes);
   len+=pds_strcat(msg,strtmp);
   if(!adate->year && !adate->month && !adate->day && !adate->hours && !adate->minutes)
    sprintf(strtmp,"\nAccessed: -");
   else
    sprintf(strtmp,"\nAccessed: %4.4d.%2.2d.%2.2d. %2d:%2.2d",adate->year+1980,
   adate->month,adate->day,adate->hours,adate->minutes);
   len+=pds_strcat(msg,strtmp);
  }
  mpxplay_diskdrive_findclose(pei->mdds,&ffblk);
 }

 display_textwin_openwindow_keymessage(TEXTWIN_FLAG_MSGLEFTALIGN," Fileinfo ",msg);

 mpxplay_infile_close(frp);
 mpxplay_infile_frame_free(frp);
}

//------------------------------------------------------------------------

#define EDITFILEINFOS_FLAG_IGNORE_ERRORS  1
#define EDITFILEINFOS_FLAG_READONLY_WRITE 2
#define EDITFILEINFOS_FLAG_READONLY_SKIP  4
#define EDITFILEINFOS_FLAG_READONLY_ALL   8
#define EDITFILEINFOS_FLAG_READONLY_TRIED 16

typedef struct editfileinfo_s{
 unsigned long flags;
 struct mainvars *mvp;
 unsigned long i3i_end;
 unsigned long old_pstime,old_petime;
 struct playlist_side_info *psi;
 struct playlist_entry_info *pei_loop;
 struct playlist_entry_info pei;
 struct frame fr_data;
 char filename[MAX_PATHNAMELEN];
 char id3infos[I3I_MAX+1][256];
 char pstimestr_origi[16];
 char pstimestr_edited[16];
 char petimestr_origi[16];
 char petimestr_edited[16];
 char pltimestr_origi[16];
 char pltimestr_edited[16];
}editfileinfo_s;

static display_textwin_button_t buttons_writetag[]={
 {"[ Write tags ]",0x1177}, // 'w'
 {""              ,0x1157}, // 'W'
 {"[ Cancel ]"    ,0x2e63}, // 'c'
 {""              ,0x2e43}, // 'C'
 {""              ,0x3e00}, // F4
 {""              ,KEY_ESC},// ESC
 {NULL,0}
};

static display_textwin_button_t buttons_errorhand[]={
 {" Ignore "    ,0x1769}, // 'i'
 {""            ,0x1749}, // 'I'
 {" Ignore All ",0x1e61}, // 'a'
 {""            ,0x1e41}, // 'A'
 {" Cancel "    ,0x2e63}, // 'c'
 {""            ,0x2e43}, // 'C'
 {""            ,KEY_ESC},// ESC
 {NULL,0}
};

static display_textwin_button_t button_errorhand_ok[]={
 {"[ Ok ]"    ,KEY_ESC}, //
 {NULL,0}
};

/*static display_textwin_button_t buttons_readonly[]={
 {" Write "    ,0x1177}, // 'w'
 {""           ,0x1157}, // 'W'
 {" All "      ,0x1e61}, // 'a'
 {""           ,0x1e41}, // 'A'
 {" Skip "     ,0x1f73}, // 's'
 {""           ,0x1f53}, // 'S'
 {" Skipall "  ,0x256b}, // 'k'
 {""           ,0x254b}, // 'K'
 {" Cancel "   ,0x2e63}, // 'c'
 {""           ,0x2e43}, // 'C'
 {""           ,KEY_ESC},// ESC
 {NULL,0}
};*/

static int  editfileinfos_writetag_to_file(struct editfileinfo_s *efi);
static void editfileinfos_update_all_loop(struct editfileinfo_s *efi);

static void editfileinfos_dealloc(struct editfileinfo_s *efi)
{
 void *tw;
 tw=display_textwin_openwindow_message(NULL,NULL,"Flushing disk caches ...");
 pds_drives_flush();
 display_textwin_closewindow_message(tw);

 if(efi && efi->psi)
  display_editorside_reset(efi->psi);
 else
  refdisp|=RDT_RESET_EDIT;

 if(efi){
  mpxplay_infile_frame_free(&efi->fr_data);
  free(efi);
 }
}

/*static void editfileinfos_tagwrite_readonlybuttonhand(struct editfileinfo_s *efi,unsigned int extkey)
{
 switch(extkey){
  case 0x1177:
  case 0x1157:funcbit_enable(efi->flags,EDITFILEINFOS_FLAG_READONLY_WRITE);
              mpxplay_timer_addfunc(editfileinfos_writetag_to_file,efi,MPXPLAY_TIMERFLAG_LOWPRIOR,0);
              break;
  case 0x1e61:
  case 0x1e41:funcbit_enable(efi->flags,(EDITFILEINFOS_FLAG_READONLY_WRITE|EDITFILEINFOS_FLAG_READONLY_ALL));
              mpxplay_timer_addfunc(editfileinfos_writetag_to_file,efi,MPXPLAY_TIMERFLAG_LOWPRIOR,0);
              break;
  case 0x1f73:
  case 0x1f53:funcbit_enable(efi->flags,EDITFILEINFOS_FLAG_READONLY_SKIP);
              mpxplay_timer_addfunc(editfileinfos_update_all_loop,efi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
              break;
  case 0x256b:
  case 0x254b:funcbit_enable(efi->flags,(EDITFILEINFOS_FLAG_READONLY_SKIP|EDITFILEINFOS_FLAG_READONLY_ALL));
              mpxplay_timer_addfunc(editfileinfos_update_all_loop,efi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
              break;
  default:mpxplay_timer_addfunc(editfileinfos_dealloc,efi,MPXPLAY_TIMERFLAG_INDOS,0);
 }
}*/

static void editfileinfos_tagwrite_errorhandler(struct editfileinfo_s *efi,unsigned int extkey)
{
 switch(extkey){
  case 0x1769:
  case 0x1749:mpxplay_timer_addfunc(editfileinfos_update_all_loop,efi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
              break;
  case 0x1e61:
  case 0x1e41:funcbit_enable(efi->flags,EDITFILEINFOS_FLAG_IGNORE_ERRORS);
              mpxplay_timer_addfunc(editfileinfos_update_all_loop,efi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
              break;
  default:mpxplay_timer_addfunc(editfileinfos_dealloc,efi,MPXPLAY_TIMERFLAG_INDOS,0);
 }
}

static void editfileinfos_maketimemstr(char *str,unsigned long msec)
{
 sprintf(str,"%3d:%2.2d.%2.2d",(unsigned long)(msec/(60*1000)),(unsigned long)(msec%(60*1000)/1000),(unsigned long)(msec%1000/10));
}

static void editfileinfos_update_index(struct editfileinfo_s *efi)
{
 if(efi->pei.infobits&PEIF_INDEXED){
  unsigned long hextime;
  long diff;

  if(pds_strcmp(efi->pltimestr_origi,efi->pltimestr_edited)!=0){
   unsigned long lentime;
   hextime=pds_strtime_to_hexhtime(efi->pltimestr_edited);
   lentime=(((hextime>>24)&0xff)*(60*60*1000))+(((hextime>>16)&0xff)*(60*1000))
          +(((hextime>> 8)&0xff)*1000)+((hextime&0xff)*10);
   efi->pei.petime=efi->pei.pstime+lentime;
  }
  if(pds_strcmp(efi->petimestr_origi,efi->petimestr_edited)!=0){
   hextime=pds_strtime_to_hexhtime(efi->petimestr_edited);
   efi->pei.petime=(((hextime>>24)&0xff)*(60*60*1000))+(((hextime>>16)&0xff)*(60*1000))
                  +(((hextime>> 8)&0xff)*1000)+((hextime&0xff)*10);
   if(efi->old_petime)
    diff=(long)efi->old_petime-(long)efi->pei.petime;
   else
    diff=(long)efi->pei.timemsec-(long)efi->pei.petime;
   if((diff<10) && (diff>=0)) // small diff (inside rounding error), petime is not modified
    efi->pei.petime=efi->old_petime;
  }
  if(efi->pei.petime){
   if(efi->pei.petime>efi->pei.timemsec)
    efi->pei.petime=0;
  }
  if(pds_strcmp(efi->pstimestr_origi,efi->pstimestr_edited)!=0){
   unsigned long new_pstime;
   hextime=pds_strtime_to_hexhtime(efi->pstimestr_edited);
   new_pstime=(((hextime>>24)&0xff)*(60*60*1000))+(((hextime>>16)&0xff)*(60*1000))
            +(((hextime>> 8)&0xff)*1000)+((hextime&0xff)*10);
   diff=(long)efi->old_pstime-(long)new_pstime;
   if((diff>=10) || (diff<0)){
    if((new_pstime<=efi->pei.timemsec) && (!efi->pei.petime || (new_pstime<=efi->pei.petime)))
     efi->pei.pstime=new_pstime;
   }
  }
 }
}

/*static unsigned int editfileinfos_check_id3info_update(struct editfileinfo_s *efi)
{
 unsigned int i;
 for(i=0;i<=I3I_MAX;i++)
  if(efi->id3infos[i][0])
   return 1;
 return 0;
}*/

static void editfileinfos_update_id3info(struct editfileinfo_s *efi,unsigned int clear_blank)
{
 unsigned int i;
 efi->pei.filename=&efi->filename[0];
 for(i=0;i<=I3I_MAX;i++){
  unsigned int index=id3order[i];
  if(efi->id3infos[i][0])
   efi->pei.id3info[index]=&efi->id3infos[i][0];
  else
   if(clear_blank)
    efi->pei.id3info[index]=NULL;
 }
 efi->old_pstime=efi->pei.pstime;
 efi->old_petime=efi->pei.petime;
}

static void editfileinfos_update_aktfile(struct editfileinfo_s *efi)
{
 struct playlist_entry_info *pei=efi->mvp->pei0;
 if(pds_stricmp(pei->filename,efi->filename)==0 && (pei->pstime==efi->old_pstime) && (pei->petime==efi->old_petime)){
  playlist_pei0_set(efi->mvp,&efi->pei,0);
  if(efi->pei.infobits&PEIF_INDEXED)
   mpxplay_calculate_index_start_end(efi->mvp->frp0,efi->mvp,&efi->pei);
 }
}

static void editfileinfos_update_editor(struct editfileinfo_s *efi)
{
 struct playlist_side_info *psi=efi->mvp->psi0;
 struct playlist_entry_info *pei,*lastpei;
 unsigned int side,modify;

 for(side=0;side<PLAYLIST_MAX_SIDES;side++,psi++){
  pei=lastpei=NULL;
  do{
   pei=playlist_search_filename(psi,efi->filename,((efi->old_pstime)? efi->old_pstime:-1),pei);
   if(!pei)
    break;
   if((pei->pstime!=efi->old_pstime) || (pei->petime!=efi->old_petime)){
    pei++;
    continue;
   }
   modify=EDITLIST_MODE_ID3;
   if(pei->infobits&PEIF_INDEXED)
    modify|=EDITLIST_MODE_HEAD|EDITLIST_MODE_INDEX;
   playlist_editlist_delfile_one(psi,pei,modify);
   playlist_editlist_addfile_one(NULL,psi,&efi->pei,pei,modify);
   refdisp|=RDT_EDITOR;
   lastpei=pei;
   pei++;
  }while(1);

  // !!! auto correct index end/start of prev/next entry (end of prev index eq to start of curr index)
  if(lastpei && (lastpei->infobits&PEIF_INDEXED)){
   if(lastpei>psi->firstsong){
    struct playlist_entry_info *ppn=lastpei-1;
    if((ppn->infobits&PEIF_INDEXED) && (ppn->petime==efi->old_pstime) && (pds_stricmp(ppn->filename,efi->filename)==0))
     ppn->petime=efi->pei.pstime;
   }
   if(lastpei<psi->lastentry){
    struct playlist_entry_info *ppn=lastpei+1;
    if((ppn->infobits&PEIF_INDEXED) && (ppn->pstime==efi->old_petime) && (pds_stricmp(ppn->filename,efi->filename)==0))
     ppn->pstime=efi->pei.petime;
   }
  }
 }
}

static int editfileinfos_writetag_to_file(struct editfileinfo_s *efi)
{
 struct frame *frp=&efi->fr_data;
 frp->filetype=GET_HFT(efi->pei.entrytype);
 frp->filesize=efi->pei.filesize;
 frp->mdds=efi->pei.mdds;
 frp->infile_funcs=efi->pei.infile_funcs;
 frp->infile_infos->timemsec=efi->pei.timemsec;
 return mpxplay_infile_write_id3tag(frp,efi->filename,&efi->pei.id3info[0]);
}

/*static int editfileinfos_writetag_to_file(struct editfileinfo_s *efi)
{
 int retcode;
 struct frame *frp=&efi->fr_data;
 char sout[MAX_PATHNAMELEN+128];

wtf_retry:
 frp->filetype=GET_HFT(efi->pei.entrytype);
 frp->filesize=efi->pei.filesize;
 frp->mdds=efi->pei.mdds;
 frp->infile_funcs=efi->pei.infile_funcs;
 frp->infile_infos->timemsec=efi->pei.timemsec;
 if((efi->flags&EDITFILEINFOS_FLAG_READONLY_WRITE) && (efi->flags&EDITFILEINFOS_FLAG_READONLY_TRIED)){
  if(!(efi->flags&EDITFILEINFOS_FLAG_READONLY_ALL))
   funcbit_disable(efi->flags,EDITFILEINFOS_FLAG_READONLY_WRITE);
  funcbit_disable(efi->flags,EDITFILEINFOS_FLAG_READONLY_TRIED);
  retcode=pds_fileattrib_reset(efi->filename,_A_RDONLY);
  if(retcode!=MPXPLAY_ERROR_FILEHAND_OK)
   goto wtf_end;
 }
 retcode=mpxplay_infile_write_id3tag(frp,efi->filename,&efi->pei.id3info[0]);
 if(retcode==MPXPLAY_ERROR_INFILE_CANTOPEN){
  if(efi->flags&EDITFILEINFOS_FLAG_IGNORE_ERRORS)
   goto wtf_end;
  if(efi->flags&EDITFILEINFOS_FLAG_READONLY_TRIED){
   funcbit_disable(efi->flags,EDITFILEINFOS_FLAG_READONLY_TRIED);
   goto wtf_end;
  }
  if(efi->flags&EDITFILEINFOS_FLAG_READONLY_SKIP){
   if(!(efi->flags&EDITFILEINFOS_FLAG_READONLY_ALL))
    funcbit_disable(efi->flags,EDITFILEINFOS_FLAG_READONLY_SKIP);
   goto wtf_end;
  }
  funcbit_enable(efi->flags,EDITFILEINFOS_FLAG_READONLY_TRIED);
  if(!(efi->flags&EDITFILEINFOS_FLAG_READONLY_WRITE)){
   if(pds_fileattrib_get(efi->filename)&_A_RDONLY){
    const unsigned int flags=(TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN);
    void *tw=display_textwin_allocwindow_items(NULL,flags," Error ",editfileinfos_tagwrite_readonlybuttonhand,efi);
    snprintf(sout,sizeof(sout),"The following file is read-only\n%s\nWrite the tags anyway?",efi->filename);
    display_textwin_additem_msg_alloc(tw,flags,0,-1,sout);
    display_textwin_additem_msg_static(tw,flags,0,-1,"");
    display_textwin_additem_buttons(tw,flags,0,-1,buttons_readonly,NULL);
    display_textwin_openwindow_items(tw,0,0,0);
    mpxplay_timer_deletefunc(editfileinfos_update_all_loop,efi);
    return MPXPLAY_ERROR_INFILE_OK;
   }
  }
  goto wtf_retry;
 }

wtf_end:
 mpxplay_timer_addfunc(editfileinfos_update_all_loop,efi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
 return retcode;
}*/

static void editfileinfos_get_errormsg(struct editfileinfo_s *efi,int error,char *sbuf,int sbufsize)
{
 char *shortfname=pds_getfilename_from_fullname(efi->filename);
 switch(error){
  case MPXPLAY_ERROR_INFILE_OK:/*pds_strcpy(sout,"Tags saved");*/*sbuf=0;return;
  case MPXPLAY_ERROR_INFILE_CANTOPEN:snprintf(sbuf,sbufsize,"Write error (read-only or not exists) at\n%s",shortfname);break;
  case MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE:snprintf(sbuf,sbufsize,"Tag-write is not supported for this filetype\n%s",shortfname);break;
  case MPXPLAY_ERROR_INFILE_WRITETAG_TAGTYPE :snprintf(sbuf,sbufsize,"Wrong or unsupported tagtype in\n%s",shortfname);break;
  case MPXPLAY_ERROR_FILEHAND_CANTWRITE:snprintf(sbuf,sbufsize,"Cannot write to file\n%s",shortfname);break;
  //case MPXPLAY_ERROR_FILEHAND_CHANGEATTR:snprintf(sbuf,sbufsize,"Cannot change the attrib of\n%s",shortfname);break;
  case MPXPLAY_ERROR_INFILE_WRITETAG_IV2_SPACE:snprintf(sbuf,sbufsize,"Not enough space to store all frames in ID3V2!\nMaybe some tag-infos have lost in\n%s",shortfname);return;
  default:snprintf(sbuf,sbufsize,"Tag-write (%d) error at\n%s",error,shortfname);break;
 }
 pds_strcat(sbuf,"\nTags are not saved in the file!");
}

static unsigned int editfileinfos_update_file(struct editfileinfo_s *efi)
{
 char sout[MAX_PATHNAMELEN+128];
 funcbit_disable(efi->flags,EDITFILEINFOS_FLAG_READONLY_TRIED);
 if(!(efi->pei.infobits&PEIF_INDEXED)){
  int error=editfileinfos_writetag_to_file(efi);
  if(!(efi->flags&EDITFILEINFOS_FLAG_IGNORE_ERRORS)){
   editfileinfos_get_errormsg(efi,error,sout,sizeof(sout));
   if(*sout){
    const unsigned int flags=(TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN);
    void *tw=display_textwin_allocwindow_items(NULL,flags," Error ",editfileinfos_tagwrite_errorhandler,efi);
    display_textwin_additem_msg_alloc(tw,flags,0,-1,sout);
    if(efi->pei_loop){
     display_textwin_additem_msg_static(tw,flags,0,-1,"");
     display_textwin_additem_buttons(tw,flags,0,-1,buttons_errorhand,NULL);
     mpxplay_timer_deletefunc(editfileinfos_update_all_loop,efi);
    }else
     display_textwin_additem_buttons(tw,flags,0,-1,button_errorhand_ok,NULL);
    display_textwin_openwindow_items(tw,0,0,0);
    return 0;
   }
  }
 }
 return 1;
}

static void editfileinfos_update_one(struct editfileinfo_s *efi,unsigned int extkey)
{
 switch(extkey){
  case 0x1177:           // 'w'
  case 0x1157:break;     // 'W'
  default:goto end_fi_wr;
 }
 funcbit_enable(efi->psi->editloadtype,(PLL_CHG_MANUAL|PLL_CHG_ID3));

 editfileinfos_update_id3info(efi,1);
 editfileinfos_update_index(efi);
 editfileinfos_update_aktfile(efi);
 editfileinfos_update_editor(efi);
 if(!editfileinfos_update_file(efi))
  return;

end_fi_wr:
 editfileinfos_dealloc(efi);
}

static void editfileinfos_update_all_loop(struct editfileinfo_s *efi)
{
 struct playlist_side_info *psi=efi->psi;
 struct playlist_entry_info *pei=efi->pei_loop,pei_tmp;

 if(pei>psi->lastentry)
  goto err_out_loop;

 if(pei->infobits&PEIF_SELECTED){
  if(GET_HFT(pei->entrytype)!=HFT_DFT){
   playlist_chkentry_get_onefileinfos_allagain(psi,pei,&efi->fr_data,((loadid3tag)? (loadid3tag|ID3LOADMODE_FILE):ID3LOADMODE_ALL));
   mpxplay_infile_close(&efi->fr_data);
   pds_strcpy(efi->filename,pei->filename);
   pds_memcpy(&efi->pei,pei,sizeof(struct playlist_entry_info));
   editfileinfos_update_id3info(efi,0);
   pds_memcpy(&pei_tmp,&efi->pei,sizeof(struct playlist_entry_info));
   if(playlist_editlist_allocated_copy_entry(&efi->pei,&pei_tmp)){
    editfileinfos_update_aktfile(efi);
    editfileinfos_update_editor(efi);
    if(!(pei->infobits&PEIF_INDEXED))
     editfileinfos_update_file(efi);
    playlist_editlist_allocated_clear_entry(&efi->pei);
   }
  }
  funcbit_disable(pei->infobits,PEIF_SELECTED);
  if(psi->selected_files)
   psi->selected_files--;
  refdisp|=RDT_EDITOR;
 }
 pei++;
 efi->pei_loop=pei;
 return;

err_out_loop:
 mpxplay_timer_deletefunc(editfileinfos_update_all_loop,efi);
 editfileinfos_dealloc(efi);
}

static void editfileinfos_update_all_start(struct editfileinfo_s *efi,unsigned int extkey)
{
 switch(extkey){
  case 0x1177:           // 'w'
  case 0x1157:break;     // 'W'
  default:free(efi);return;
 }
 funcbit_enable(efi->psi->editloadtype,(PLL_CHG_MANUAL|PLL_CHG_ID3));
 //if(editfileinfos_check_id3info_update(efi))
  mpxplay_timer_addfunc(editfileinfos_update_all_loop,efi,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
 //else
 // free(efi);
}


void playlist_fileinfo_edit_infos(struct mainvars *mvp)
{
 struct playlist_side_info *psi=mvp->psie;
 struct playlist_entry_info *pei=psi->editorhighline;
 struct frame *frp;
 struct mpxplay_infile_info_s *miis;
 struct editfileinfo_s *efi;
 unsigned int i,y,error=0;
 void *tw;
 char *shortfname,sout[100];

 if(!psi->selected_files){
  if((pei->entrytype&DFTM_DFT) && ((pei->entrytype&DFTM_DRIVE) || (pei->entrytype&DFTM_DIR) || (pei->entrytype==DFT_UPLIST))){
   display_textwin_openwindow_errormessage("Cannot edit id3 info of drive/directory/uplist!");
   return;
  }
 }

 if(psi->id3infolastp>=psi->id3infoendp){
  display_textwin_openwindow_errormsg_ok(" Edit ID3 ","No free space to edit id3-tags!\nTry to delete some files from the playlist...");
  return;
 }

 efi=(editfileinfo_s *)calloc(1,sizeof(struct editfileinfo_s));
 if(!efi)
  return;
 efi->mvp=mvp;
 efi->psi=psi;
 frp=&efi->fr_data;
 if(!mpxplay_infile_frame_alloc(frp))
  goto err_out_fei;
 miis=frp->infile_infos;

 if(psi->selected_files){
  efi->pei_loop=psi->firstentry;
  efi->i3i_end=I3I_MAX;

  tw=display_textwin_allocwindow_items(NULL,0," Edit multiply ID3s ",editfileinfos_update_all_start,efi);
  sprintf(sout," Selected %d file(s)",psi->selected_files);
  display_textwin_additem_msg_alloc(tw,0,0,0,sout);
  y=2;

  for(i=0;i<=efi->i3i_end;i++){
   display_textwin_additem_msg_static(tw,0,0,y,id3text[i]);
   display_textwin_additem_editline(tw,0,0,pds_strlen(id3text[i]),y,50,efi->id3infos[i],sizeof(efi->id3infos[i])-1);
   y+=2;
  }

  display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y,"note: blank tags are not modified in the file(s)");

 }else{

  if(pei->infobits&PEIF_INDEXED){
   tw=display_textwin_allocwindow_items(NULL,0," Edit index infos ",editfileinfos_update_one,efi);
   efi->i3i_end=1;
  }else{
   playlist_chkentry_get_onefileinfos_allagain(psi,pei,frp,((loadid3tag)? (loadid3tag|ID3LOADMODE_FILE):ID3LOADMODE_ALL));
   tw=display_textwin_allocwindow_items(NULL,0," Edit ID3 ",editfileinfos_update_one,efi);
   efi->i3i_end=I3I_MAX;
   if((pei->entrytype<DFT_AUDIOFILE) || (!pei->infile_funcs->write_id3tag && !MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support)))
    error=1;
  }
  shortfname=pds_getfilename_from_fullname(pei->filename);
  display_textwin_additem_msg_static(tw,0,0,0,"Filename: ");
  display_textwin_additem_msg_alloc(tw,0,sizeof("Filename: ")-1,0,shortfname);
  y=2;

  pds_strcpy(efi->filename,pei->filename);
  pds_memcpy(&efi->pei,pei,sizeof(struct playlist_entry_info));
  for(i=0;i<=I3I_MAX;i++){
   unsigned int index=id3order[i];
   if(pei->id3info[index])
    pds_strncpy(efi->id3infos[i],pei->id3info[index],sizeof(efi->id3infos[i])-1);
  }

  for(i=0;i<=efi->i3i_end;i++){
   display_textwin_additem_msg_static(tw,0,0,y,id3text[i]);
   display_textwin_additem_editline(tw,0,0,pds_strlen(id3text[i]),y,50,efi->id3infos[i],sizeof(efi->id3infos[i])-1);
   y+=2;
  }
  if(pei->infobits&PEIF_INDEXED){
   unsigned long endtime=((pei->petime)? pei->petime:pei->timemsec);
   unsigned long lentime=endtime-((pei->pstime<=endtime)? pei->pstime:endtime);
   const unsigned int eflags=TEXTWIN_EDITFLAG_OVERWRITE|TEXTWIN_EDITFLAG_NUMERIC;
   unsigned int x=1;

   display_textwin_additem_msg_static(tw,0,x,y,"Start time:"); x+=sizeof("Start time:")-1;
   editfileinfos_maketimemstr(efi->pstimestr_origi,pei->pstime);
   pds_strcpy(efi->pstimestr_edited,efi->pstimestr_origi);
   display_textwin_additem_editline(tw,0,eflags,x,y,3,efi->pstimestr_edited,3); x+=3;
   display_textwin_additem_msg_static(tw,0,x,y,&efi->pstimestr_edited[3]);      x++;
   display_textwin_additem_editline(tw,0,eflags,x,y,2,&efi->pstimestr_edited[4],2); x+=2;
   display_textwin_additem_msg_static(tw,0,x,y,&efi->pstimestr_edited[6]);      x++;
   display_textwin_additem_editline(tw,0,eflags,x,y,2,&efi->pstimestr_edited[7],2); x+=2+2;

   display_textwin_additem_msg_static(tw,0,x,y,"Length:"); x+=sizeof("Length:")-1;
   editfileinfos_maketimemstr(efi->pltimestr_origi,lentime);
   pds_strcpy(efi->pltimestr_edited,efi->pltimestr_origi);
   display_textwin_additem_editline(tw,0,eflags,x,y,3,efi->pltimestr_edited,3); x+=3;
   display_textwin_additem_msg_static(tw,0,x,y,&efi->pltimestr_edited[3]);      x++;
   display_textwin_additem_editline(tw,0,eflags,x,y,2,&efi->pltimestr_edited[4],2); x+=2;
   display_textwin_additem_msg_static(tw,0,x,y,&efi->pltimestr_edited[6]);      x++;
   display_textwin_additem_editline(tw,0,eflags,x,y,2,&efi->pltimestr_edited[7],2); x+=2+2;

   display_textwin_additem_msg_static(tw,0,x,y,"End time:"); x+=sizeof("End time:")-1;
   editfileinfos_maketimemstr(efi->petimestr_origi,endtime);
   pds_strcpy(efi->petimestr_edited,efi->petimestr_origi);
   display_textwin_additem_editline(tw,0,eflags,x,y,3,efi->petimestr_edited,3); x+=3;
   display_textwin_additem_msg_static(tw,0,x,y,&efi->petimestr_edited[3]);      x++;
   display_textwin_additem_editline(tw,0,eflags,x,y,2,&efi->petimestr_edited[4],2); x+=2;
   display_textwin_additem_msg_static(tw,0,x,y,&efi->petimestr_edited[6]);      x++;
   display_textwin_additem_editline(tw,0,eflags,x,y,2,&efi->petimestr_edited[7],2); x+=2+2;

   y++;
  }else{
   if(error)
    display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y,"Warning: cannot save the modifications in the (audio)file!");
   else if(fileinfo_build_tagtype(sout,miis,1))
    display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y,sout);
   mpxplay_infile_close(frp); // ??? here?
  }
 }
 display_textwin_additem_separatorline(tw,-1);
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,buttons_writetag,NULL);
 display_textwin_openwindow_items(tw,0,0,0);
 return;

err_out_fei:
 editfileinfos_dealloc(efi);
}
