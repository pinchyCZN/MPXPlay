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
//function:playlist save

#include "newfunc\newfunc.h"
#include "control\control.h"
#include "display\display.h"
#include "playlist.h"

#define MPXP_SAVELIST_RETCODE_OK         0
#define MPXP_SAVELIST_RETCODE_ERROR      1  // unknown error
#define MPXP_SAVELIST_RETCODE_CANTWRITE  2
#define MPXP_SAVELIST_RETCODE_NONSTANDARD_CUE 3

#define MPXP_SAVELIST_SWITCHBIT_UTFTEXTENC     1
#define MPXP_SAVELIST_SWITCHBIT_REMOTEFULLPATH 2
#define MPXP_SAVELIST_SWITCHBIT_ALLFULLPATH    4

#define LOADDIR_MAX_LOCAL_DRIVES   ('Z'-'A'+1)

extern char *m3usavename,*mxusavename,*cuesavename;
extern unsigned int playrand,desktopmode,playlistload;
static mpxp_uint32_t savelist_switch_config=MPXP_SAVELIST_SWITCHBIT_REMOTEFULLPATH;

static char *savelist_get_relative_filename(char *outbuf,unsigned int buflen,char *filename,char *path,unsigned int pathlen)
{
 char csave,*rp=filename;
 struct mpxplay_diskdrive_data_s mdds_tmp;

 csave=filename[pathlen];
 filename[pathlen]=0;
 if(!funcbit_test(savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_ALLFULLPATH) && (pds_stricmp(filename,path)==0)){  // if path of startdir and save-dir is the same
  if(pathlen==(sizeof(PDS_DIRECTORY_ROOTDIR_STR)-1))
   rp=&filename[pathlen];
  else
   rp=&filename[pathlen+1];  // save relative to currdir (subdir\filename.mp3)
  filename[pathlen]=csave;
 }else{               // save with full path      ([d:]\subdir\filename.mp3)
  int drivenum_src,drivenum_dest;
  filename[pathlen]=csave;
  drivenum_src=pds_getdrivenum_from_path(filename);
  if(drivenum_src<0){ // a non local (virtual) filename (like ftp://)
   mdds_tmp.mdfs=mpxplay_diskdrive_search_driver(filename);
   if(mdds_tmp.mdfs){
    mdds_tmp.drive_data=NULL;
    if(mpxplay_diskdrive_drive_config(&mdds_tmp,MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_REALLYFULLPATH,outbuf,filename)==MPXPLAY_DISKDRIV_CFGERROR_SET_OK)
     return outbuf;
   }
   goto sgrf_out;
  }
  drivenum_dest=pds_getdrivenum_from_path(path);
  if(drivenum_src==drivenum_dest){ // drives are same
   if(!funcbit_test(savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_ALLFULLPATH) || (drivenum_dest>=LOADDIR_MAX_LOCAL_DRIVES)) // we don't write the drive letter on remote drives
    rp=&filename[2];   // save relative to rootdir (\subdir\filename.mp3)
   goto sgrf_out;
  }
  if(funcbit_test(savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_REMOTEFULLPATH)){
   struct mpxplay_diskdrive_data_s *mdds_src=playlist_loaddir_drivenum_to_drivemap(drivenum_src);
   struct mpxplay_diskdrive_data_s *mdds_dest=playlist_loaddir_drivenum_to_drivemap(drivenum_dest);
   if(mdds_src && mdds_dest && (mdds_src!=mdds_dest))
    if(mpxplay_diskdrive_drive_config(mdds_src,MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_REALLYFULLPATH,outbuf,filename)==MPXPLAY_DISKDRIV_CFGERROR_SET_OK)
     return outbuf;
  }
 }
sgrf_out:
 pds_strncpy(outbuf,rp,buflen);
 outbuf[buflen-1]=0;
 return outbuf;
}

static unsigned int save_m3u_playlist(struct playlist_side_info *psi,void *fp,char *path,unsigned int list_type)
{
 struct playlist_entry_info *pei;
 unsigned int pathlen;
 char sout[MAX_ID3LEN],cnvtmp1[MAX_ID3LEN],cnvtmp2[MAX_ID3LEN];

 pathlen=pds_strlen(path);

 if(pathlen){ // allways have to be
  if(list_type&PLST_EXTM3U){
   sprintf(sout,"#EXTM3U");
   if(mpxplay_diskdrive_textfile_writeline(fp,sout)<=0)
    return MPXP_SAVELIST_RETCODE_CANTWRITE;
  }

  for(pei=psi->firstsong;pei<=psi->lastentry;pei++){
   if((list_type&PLST_EXTM3U) && (pei->infobits&PEIF_ENABLED)){
    snprintf(sout,sizeof(sout),"#EXTINF:%d,%.200s - %.200s",
      (pei->timemsec+500)/1000,
      mpxplay_playlist_textconv_back(cnvtmp1,pei->id3info[I3I_ARTIST]),
      mpxplay_playlist_textconv_back(cnvtmp2,pei->id3info[I3I_TITLE]));
    if(mpxplay_diskdrive_textfile_writeline(fp,sout)<=0)
     return MPXP_SAVELIST_RETCODE_CANTWRITE;
   }
   savelist_get_relative_filename(sout,sizeof(sout),pei->filename,path,pathlen);
   if(mpxplay_diskdrive_textfile_writeline(fp,sout)<0)
    return MPXP_SAVELIST_RETCODE_CANTWRITE;
  }
 }else{ // ???
  for(pei=psi->firstsong;pei<=psi->lastentry;pei++)
   if(mpxplay_diskdrive_textfile_writeline(fp,pei->filename)<0) // save with full path
    return MPXP_SAVELIST_RETCODE_CANTWRITE;
 }
 return MPXP_SAVELIST_RETCODE_OK;
}

static unsigned int save_mxu_playlist(struct playlist_side_info *psi,void *fp)
{
 struct playlist_entry_info *pei;
 char sout[MAX_ID3LEN],cnvtmp1[MAX_ID3LEN],cnvtmp2[MAX_ID3LEN];

 for(pei=psi->firstsong;pei<=psi->lastentry;pei++){
  snprintf(sout,sizeof(sout),"%.400s°%.200s±%.200s²%8.8X",(pei->filename)? pei->filename:"",
   mpxplay_playlist_textconv_back(cnvtmp1,pei->id3info[I3I_ARTIST]),
   mpxplay_playlist_textconv_back(cnvtmp2,pei->id3info[I3I_TITLE]),
   (pei->infobits&PEIF_ENABLED)? (MXUFLAG_ENABLED|(((pei->timemsec+500)/1000)&MXUFLAG_TIMEMASK)):0);
  if(mpxplay_diskdrive_textfile_writeline(fp,sout)<=0)
   return MPXP_SAVELIST_RETCODE_CANTWRITE;
 }
 return MPXP_SAVELIST_RETCODE_OK;
}

static unsigned int save_cue_playlist(struct playlist_side_info *psi,void *fp,char *path,unsigned int fullinfo)
{
 struct playlist_entry_info *pei,*ppn;
 pds_fdate_t *d;
 unsigned int pathlen=pds_strlen(path),trackcount=1,nonstandard=0,len;
 char cnvtmp[MAX_ID3LEN],lastfile[MAX_PATHNAMELEN]="",sout[MAX_ID3LEN];

 for(pei=psi->firstsong;pei<=psi->lastentry;pei++){
  if(!(pei->infobits&PEIF_INDEXED) || (pds_strcmp(pei->filename,lastfile)!=0)){
   if(!fullinfo){
    char *ext=pds_strrchr(pei->filename,'.');
    if(ext){
     ext++;
     if(pds_stricmp(ext,"wav")==0)
      ext="WAVE";
     else
      ext="MP3";
    }else
     ext="MP3";
    snprintf(sout,sizeof(sout),"FILE \"%s\" %s",savelist_get_relative_filename(cnvtmp,sizeof(cnvtmp),pei->filename,path,pathlen),ext);
   }else
    snprintf(sout,sizeof(sout),"FILE \"%s\"",savelist_get_relative_filename(cnvtmp,sizeof(cnvtmp),pei->filename,path,pathlen));
   if(mpxplay_diskdrive_textfile_writeline(fp,sout)<=0)
    return MPXP_SAVELIST_RETCODE_CANTWRITE;
   pds_strcpy(lastfile,pei->filename);
  }
  if(!fullinfo){
   sprintf(sout," TRACK %2.2d AUDIO",trackcount++);
   if(mpxplay_diskdrive_textfile_writeline(fp,sout)<=0)
    return MPXP_SAVELIST_RETCODE_CANTWRITE;
   if(trackcount>99)
    trackcount=99;
  }
  if(pei->id3info[I3I_ARTIST]){
   snprintf(sout,sizeof(sout),"  PERFORMER \"%.200s\"",mpxplay_playlist_textconv_back(cnvtmp,pei->id3info[I3I_ARTIST]));
   mpxplay_diskdrive_textfile_writeline(fp,sout);
  }
  if(pei->id3info[I3I_TITLE]){
   snprintf(sout,sizeof(sout),"  TITLE \"%.200s\"",mpxplay_playlist_textconv_back(cnvtmp,pei->id3info[I3I_TITLE]));
   mpxplay_diskdrive_textfile_writeline(fp,sout);
  }
  if(!fullinfo || (pei->infobits&PEIF_INDEXED)){
   sprintf(sout,"  INDEX 01 %2.2d:%2.2d:%2.2d",(pei->pstime/60000),((pei->pstime/1000)%60),((pei->pstime%1000)*75/1000));
   mpxplay_diskdrive_textfile_writeline(fp,sout);
  }

  // saving Mpxplay's inside infos
  len=pds_strcpy(sout,"REM MPXPINFO ");
  d=&pei->filedate;
  if(d->month){
   sprintf(cnvtmp,"FIDA=%4.4d%2.2d%2.2d%2.2d%2.2d;",((unsigned long)d->year+1980),(unsigned long)d->month,(unsigned long)d->day,(unsigned long)d->hours,(unsigned long)d->minutes);
   len+=pds_strcpy(&sout[len],cnvtmp);
  }
  if(pei->filesize){
   sprintf(cnvtmp,"FISI=%d;",pei->filesize);
   len+=pds_strcpy(&sout[len],cnvtmp);
  }
  if(pei->infobits&PEIF_ENABLED){
   sprintf(cnvtmp,"LNMS=%d;",pei->timemsec);
   len+=pds_strcpy(&sout[len],cnvtmp);
  }
  if(pei->infobits&PEIF_INDEXED){
   if(pei->pstime){
    sprintf(cnvtmp,"INDB=%d;",pei->pstime);
    len+=pds_strcpy(&sout[len],cnvtmp);
   }
   ppn=pei+1;
   if((ppn>psi->lastentry) || !(ppn->infobits&PEIF_INDEXED) || !pei->petime || (pei->petime!=ppn->pstime) || (pds_stricmp(pei->filename,ppn->filename)!=0))
    ppn=NULL; // the next entry is not a continuous index
   if(pei->petime && (pei->petime!=pei->timemsec) && !ppn){
    sprintf(cnvtmp,"INDE=%d;",pei->petime);
    len+=pds_strcpy(&sout[len],cnvtmp);
    nonstandard=1;
   }
  }
  if(fullinfo && (playrand==1)){
   sprintf(cnvtmp,"PEIF=%8.8X;",(pei->infobits&PEIF_CUESAVEMASK));
   len+=pds_strcpy(&sout[len],cnvtmp);
  }
  if(len>sizeof("REM MPXPINFO ")){
   sout[len-1]=0; // to clear last ';'
   if(mpxplay_diskdrive_textfile_writeline(fp,sout)<=0)
    return MPXP_SAVELIST_RETCODE_CANTWRITE;
  }
 }
 if(nonstandard)
  return MPXP_SAVELIST_RETCODE_NONSTANDARD_CUE;
 return MPXP_SAVELIST_RETCODE_OK;
}

char *playlist_savelist_get_savename(unsigned int list_type)
{
 switch(list_type&PLST_LISTS){
  case PLST_MXU:return mxusavename;
  case PLST_CUE:return cuesavename;
  default:return m3usavename;
 }
}

static void playlist_savelist_create_path_for_manualsavename(struct playlist_side_info *psi,char *path)
{
 path[0]=0;
 if(psi->editsidetype&PLT_DIRECTORY)
  pds_strcpy(path,psi->currdir);
 else if(psi->psio->editsidetype&PLT_DIRECTORY)
  pds_strcpy(path,psi->psio->currdir);
 else if(mpxplay_diskdrive_checkdir(psi->mdds,psi->currdir))
  pds_strcpy(path,psi->currdir);
 else
  mpxplay_diskdrive_getcwd(psi->mdds,path,sizeof(path));

 if(!path[0])
  pds_getpath_from_fullname(path,freeopts[OPT_PROGNAME]);
}

static char *playlist_savelist_create_manual_savename(struct playlist_side_info *psi,unsigned int list_type,char *file_name,char *savename)
{
 char path[MAX_PATHNAMELEN];

 if(!savename)
  return savename;

 playlist_savelist_create_path_for_manualsavename(psi,path);

 if(file_name && file_name[0])
  pds_filename_build_fullpath(savename,path,pds_getfilename_from_fullname(file_name));
 else{
  char *updir;
  if(psi->editsidetype&PLT_DIRECTORY)
   updir=pds_getfilename_from_fullname(psi->currdir);
  else if(psi->psio->editsidetype&PLT_DIRECTORY)
   updir=pds_getfilename_from_fullname(psi->psio->currdir);
  else
   updir=NULL;
  if(updir && *updir){ // create playlistname from up-dir name
   pds_filename_build_fullpath(savename,path,updir);
   pds_strcat(savename,".m3u");
  }else
   pds_filename_build_fullpath(savename,path,playlist_savelist_get_savename(list_type));
 }

 return savename;
}

static unsigned int savelist_gettype_from_filename(struct playlist_side_info *psi,char *filename)
{
 char *ext;
 if(pds_filename_wildchar_chk(filename)) // it's not a playlist-filename (ie: *.m3u)
  return 0;
 ext=pds_strrchr(filename,'.');
 if(ext){
  if(pds_stricmp(ext,".m3u")==0)
   return PLST_EXTM3U; // cannot detect M3U/EXTM3U diff
  if(pds_stricmp(ext,".mxu")==0)
   return PLST_MXU;
  if(pds_stricmp(ext,".cue")==0)
   return PLST_CUE;
 }
 return 0;
}

static unsigned int savelist_open_and_write_list(struct playlist_side_info *psi,char *filename,unsigned int list_type,unsigned int cuefullinfo)
{
 void *fp;
 struct mpxplay_diskdrive_data_s *mdds;
 unsigned int retcode=MPXP_SAVELIST_RETCODE_ERROR;
 mpxp_uint32_t enable_utftextenc=funcbit_test(savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_UTFTEXTENC);
 long dest_textenctype;
 char path[MAX_PATHNAMELEN];

 funcbit_enable(savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_UTFTEXTENC);

 mdds=playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(filename));
 if(!mdds)
  mdds=psi->mdds;

 fp=mpxplay_diskdrive_textfile_open(mdds,filename,(O_WRONLY|O_CREAT|O_TEXT));
 if(fp==NULL){
  retcode=MPXP_SAVELIST_RETCODE_CANTWRITE;
  goto err_out_open;
 }

 dest_textenctype=psi->savelist_textcodetype;
 if(!enable_utftextenc)
  funcbit_disable(dest_textenctype,MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF_ALL);
 else if(!funcbit_test(dest_textenctype,MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF_ALL))
  funcbit_enable(dest_textenctype,MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8); // !!! default forced utftype
 switch(list_type&PLST_LISTS){
  case PLST_MXU:funcbit_disable(dest_textenctype,MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF_ALL);break;
 }

 mpxplay_diskdrive_textfile_config(fp,MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_TEXTCODETYPE_DEST,((void *)&dest_textenctype),NULL);

 pds_getpath_from_fullname(path,filename);

 switch(list_type&PLST_LISTS){
  case PLST_MXU:retcode=save_mxu_playlist(psi,fp);break;
  case PLST_CUE:retcode=save_cue_playlist(psi,fp,path,cuefullinfo);break;
  default:retcode=save_m3u_playlist(psi,fp,path,list_type);break;
 }
 mpxplay_diskdrive_textfile_close(fp);

err_out_open:
 return retcode;
}

unsigned int playlist_savelist_save_playlist(struct mainvars *mvp,struct playlist_side_info *psi,char *file_name,unsigned int list_type)
{
 unsigned int retcode=MPXP_SAVELIST_RETCODE_ERROR;
 char filename[MAX_PATHNAMELEN];

 if(!list_type)
  goto err_out_sl;

 if(list_type&PLST_MANUAL){
  if(!psi)
   psi=mvp->psil; // playlist side ???
  if(file_name)
   pds_strcpy(filename,file_name);
  else
   pds_filename_build_fullpath(filename,mpxplay_playlist_startdir(),playlist_savelist_get_savename(list_type));
 }else{ // PLST_AUTO
  psi=mvp->psi0+1; // right side ???
  pds_getpath_from_fullname(filename,freeopts[OPT_PROGNAME]); // save in the directory of mpxplay.exe
  pds_filename_assemble_fullname(filename,filename,playlist_savelist_get_savename(list_type));
  playlist_loadsub_setnewinputfile(psi,filename,PLL_LOADLIST);  // for startup ???
  funcbit_disable(psi->editloadtype,PLL_TYPE_ALL);    // !!!
  funcbit_enable(psi->editloadtype,PLL_LOADLIST);     //
 }

 retcode=savelist_open_and_write_list(psi,filename,list_type,0);

err_out_sl:
 return retcode;
}

unsigned int playlist_savelist_save_editedside(struct playlist_side_info *psi)
{
 unsigned int retcode=MPXP_SAVELIST_RETCODE_ERROR,page=0;
 char *filename=&psi->sublistnames[psi->sublistlevel][0],savename[16];

 if(!(psi->editsidetype&PLT_ENABLED))
  goto err_out_se;
 if((psi->editsidetype&PLT_DIRECTORY) && !psi->sublistlevel)
  goto err_out_se;
 if( !(psi->editloadtype&(PLL_CHG_ENTRY|PLL_CHG_MANUAL))
  && !((psi->editloadtype&(PLL_CHG_LEN|PLL_CHG_ID3)) && (playlistload&PLL_RESTORED))
  && !(psi->editloadtype&(PLL_DIRSCAN|PLL_DRIVESCAN)) && !playrand )
  goto err_out_se;

 sprintf(savename,"MPXP%1.1d%3.3d.CUE",(psi-psi->mvp->psi0),page);

 pds_getpath_from_fullname(filename,freeopts[OPT_PROGNAME]); // save in the directory of mpxplay.exe
 pds_filename_assemble_fullname(filename,filename,savename);

 funcbit_disable(psi->editloadtype,PLL_TYPE_LOAD);    // !!!
 if(!(psi->editsidetype&PLT_DIRECTORY))
  funcbit_enable(psi->editloadtype,PLL_LOADLIST);     //

 retcode=savelist_open_and_write_list(psi,filename,PLST_CUE,1);

err_out_se:
 return retcode;
}

//-------------------------------------------------------------------------
static void playlist_savelist_manualsave(struct playlist_side_info *psi)
{
 unsigned int retcode;
 char *sn,*fn,msg[MAX_PATHNAMELEN+80];

 if(psi->savelist_type){
  // correct file-extension for list_type (.M3U,.MXU,.CUE)
  sn=playlist_savelist_get_savename(psi->savelist_type);
  sn=pds_strrchr(sn,'.');
  if(sn){
   fn=pds_strrchr(psi->savelist_filename,'.');
   if(fn){
    *fn=0;
    pds_strcpy(fn,sn);
   }
  }
 }else{
  // correct list_type for file-extension
  psi->savelist_type=savelist_gettype_from_filename(psi,psi->savelist_filename);
  if(!psi->savelist_type){
   sn=pds_strrchr(psi->savelist_filename,'.');
   sprintf(msg,"Invalid savelist filetype \"%s\" at\n",((sn)? (sn+1):"n/a"));
   pds_strcat(msg,pds_getfilename_from_fullname(psi->savelist_filename));
   pds_strcat(msg,"\n(use M3U,MXU,CUE file extensions)");
   display_textwin_openwindow_errormsg_ok(NULL,msg);
   return;
  }
 }

 pds_strcpy(msg,"Saving playlist to\n");
 pds_strcat(msg,psi->savelist_filename);
 display_timed_message(msg);

 pds_sfn_limit(psi->savelist_filename);

 retcode=playlist_savelist_save_playlist(psi->mvp,psi,psi->savelist_filename,(psi->savelist_type|PLST_MANUAL));
 if((retcode==MPXP_SAVELIST_RETCODE_OK) || (retcode==MPXP_SAVELIST_RETCODE_NONSTANDARD_CUE)){
  pds_strcpy(msg,"Playlist is saved to\n");
  pds_strcat(msg,psi->savelist_filename);
  display_timed_message(msg);
  playlist_editlist_updatesides_add_dft(psi->mvp,psi->savelist_filename,DFT_PLAYLIST);
  if(pds_stricmp(psi->savelist_filename,psi->sublistnames[psi->sublistlevel])==0)
   funcbit_disable(psi->editloadtype,PLL_CHG_ALL);
  if(!(psi->editsidetype&PLT_DIRECTORY) && !psi->sublistnames[0][0])
   pds_strcpy(psi->sublistnames[0],psi->savelist_filename);
  if((retcode==MPXP_SAVELIST_RETCODE_NONSTANDARD_CUE) && !(desktopmode&DTM_EDIT_NOLISTWARNINGS)){
   pds_strcpy(msg,"Warning!\n The saved CUE contains non standard end-of-index (INDE) setting!\n");
   pds_strcat(msg,psi->savelist_filename);
   display_textwin_openwindow_errormsg_ok(NULL,msg);
  }
 }else{
  display_clear_timed_message();
  pds_strcpy(msg,"Couldn't save playlist to\n");
  pds_strcat(msg,psi->savelist_filename);
  display_textwin_openwindow_errormsg_ok(NULL,msg);
 }
}

static display_textwin_button_t savelist_buttons[]={
 {""          ,0xffff}, // enter in edit-field executes the first button (savetype == extension of filename)
 {"[ EXTM3U ]",0x1265}, // 'e'
 {""          ,0x1245}, // 'E'
 {"[ M3U ]"   ,0x326d}, // 'm'
 {""          ,0x324d}, // 'M'
 {"[ CUE ]"   ,0x2e63}, // 'c'
 {""          ,0x2e43}, // 'C'
 {"[ MXU ]"   ,0x2d78}, // 'x'
 {""          ,0x2d58}, // 'X'
 {""          ,0x3c00}, // F2
 {"[ Cancel ]",KEY_ESC},// ESC
 {NULL,0}
};

static char savelist_manual_savename[MAX_PATHNAMELEN];

static void manual_savelist_keyhand(struct playlist_side_info *psi,unsigned int extkey)
{
 char path[MAX_PATHNAMELEN];
 switch(extkey){
  case 0xffff:psi->savelist_type=0;break;
  case 0x1265:
  case 0x1245:psi->savelist_type=PLST_EXTM3U;break;
  case 0x326d:
  case 0x324d:psi->savelist_type=PLST_M3U;break;
  case 0x2e63:
  case 0x2e43:psi->savelist_type=PLST_CUE;break;
  case 0x2d78:
  case 0x2d58:psi->savelist_type=PLST_MXU;break;
  default:return;
 }
 playlist_savelist_create_path_for_manualsavename(psi,path);
 pds_filename_build_fullpath(psi->savelist_filename,path,savelist_manual_savename);
 playlist_savelist_manualsave(psi);
}

void playlist_savelist_manual_save(struct mainvars *mvp)
{
 void *tw;
 unsigned int side,listtype,texttype;
 struct playlist_side_info *psi=mvp->psie;
 struct display_textwin_button_t *btsel;
 char strtmp[MAX_PATHNAMELEN],msg[50];

 savelist_manual_savename[0]=0;

 if(psi->savelist_filename[0]){
  playlist_savelist_create_manual_savename(psi,psi->savelist_type,psi->savelist_filename,savelist_manual_savename);
 }else{ // 1st calling or after directory/sublist change
  char *inpfile=NULL;
  if(psi->sublistlevel)
   inpfile=psi->sublistnames[psi->sublistlevel];
  else if(psi==mvp->psil)
   inpfile=playlist_loadsub_getinputfile(psi);
  if(inpfile){
   listtype=savelist_gettype_from_filename(psi,inpfile);
   if(listtype){
    psi->savelist_type=listtype;
    playlist_savelist_create_manual_savename(psi,psi->savelist_type,inpfile,savelist_manual_savename);
   }
  }
  if(!savelist_manual_savename[0]){
   if(!psi->savelist_type){
    psi->savelist_type=PLST_DEFAULT;
    psi->savelist_textcodetype=0;
   }
   playlist_savelist_create_manual_savename(psi,psi->savelist_type,psi->savelist_filename,savelist_manual_savename);
  }
 }

 pds_sfn_limit(savelist_manual_savename);

 texttype=psi->savelist_textcodetype;

 switch(psi->savelist_type&PLST_LISTS){
  case PLST_EXTM3U:btsel=&savelist_buttons[1];break;
  case PLST_CUE:btsel=&savelist_buttons[5];break;
  case PLST_MXU:btsel=&savelist_buttons[7];funcbit_disable(texttype,MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF_ALL);break;
  default:btsel=&savelist_buttons[3];
 }

 tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_MSGCENTERALIGN," Savelist ",manual_savelist_keyhand,psi);
 side=psi-mvp->psi0; // editorside
 sprintf(msg," Save playlist (%s side) to",((side)? "RIGHT":"LEFT"));
 display_textwin_additem_msg_alloc(tw,0,0,-1,msg);
 display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGLEFTALIGN,0,1,-1,46,savelist_manual_savename,MAX_PATHNAMELEN-2);
 //display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGLEFTALIGN,TEXTWIN_EDITFLAG_STARTEND,1,-1,46,savelist_manual_savename,MAX_PATHNAMELEN-2);
 display_textwin_additem_separatorline(tw,-1);
 funcbit_enable(savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_UTFTEXTENC);
 switch(texttype&MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF_ALL){
  case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8:pds_strcpy(msg,"UTF-8");break;
  case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE:pds_strcpy(msg,"UTF-16LE");break;
  case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16BE:pds_strcpy(msg,"UTF-16BE");break;
  default:pds_strcpy(msg,"UTF-8"); // !!! default forced type (if user enables the switch)
   funcbit_disable(savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_UTFTEXTENC);
 }
 sprintf(strtmp,"%s text encoding",msg);
 display_textwin_additem_switchline(tw,0,1,-1,&savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_UTFTEXTENC,strtmp);
 display_textwin_additem_switchline(tw,0,1,-1,&savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_REMOTEFULLPATH,"Full path for virtual filenames (0: -> ftp://)");
 display_textwin_additem_switchline(tw,0,1,-1,&savelist_switch_config,MPXP_SAVELIST_SWITCHBIT_ALLFULLPATH,"Full path for all filenames (else relative)");
 display_textwin_additem_separatorline(tw,-1);
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,savelist_buttons,btsel);
 display_textwin_openwindow_items(tw,0,0,0);
}

// clear savelist filename at loading of new playlist (in directory browser)
void playlist_savelist_clear(struct playlist_side_info *psi)
{
 psi->savelist_type=0;
 psi->savelist_textcodetype=0;
 psi->savelist_filename[0]=0;
}
