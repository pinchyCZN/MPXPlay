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
//function:file handling on disk (copy,move,delete)

#include "mpxinbuf.h"
#include "newfunc\newfunc.h"
#include "playlist.h"
#include "display\display.h"
#include "control\cntfuncs.h"

#define DISKFILE_FILECOPY_BLOCKSIZE 65536
#define DISKFILE_FILECOPY_BARLEN 40
#define DISKFILE_DEFAULT_TIMEOUTMS_WAITDISP 2000 // in ms

#define DISKFILE_CPYTYPE_COPY          1
#define DISKFILE_CPYTYPE_MOVE          2
#define DISKFILE_CPYTYPE_RENAME        4 // no move
#define DISKFILE_CPYTYPE_DEL           8 // some functions are common
#define DISKFILE_CPYTYPE_INFO_SIZE    16 // for playlist_diskfile_show_multifileinfos
#define DISKFILE_CPYTYPE_INFO_TIME    32 // phase 2
#define DISKFILE_CPYTYPE_RENBYID3     64 // rename by ID3

#define DISKFILE_CPYCTRL_MULTIFILE    (1<< 0) // more files are selected
#define DISKFILE_CPYCTRL_NOEXTCHK     (1<< 1) // don't check file extension (copy all files)

#define DISKFILE_CPYCTRL_EMPTYDIRCOPY (1<< 3) // copy empty dirs too
#define DISKFILE_CPYCTRL_SUBONECOPY   (1<< 4) // special copy at one directory if the output dir doesn't exist (don't copy the highest dirname)(for other commander compatibility)
#define DISKFILE_CPYCTRL_WITHSUBDIR   (1<< 5) // copy from playlist to dirbrowser, create one subdir level

#define DISKFILE_CPYCTRL_CREATENOEXT  (1<< 8) // create file without extension
#define DISKFILE_CPYCTRL_MAKEDIR      (1<< 9) // make dir if it doesn't exists
#define DISKFILE_CPYCTRL_OVERWRITE    (1<<10) // overwrite existent file
#define DISKFILE_CPYCTRL_SKIPFILE     (1<<11) // skip existent file

#define DISKFILE_CPYCTRL_DELFILE      (1<<12) //
#define DISKFILE_CPYCTRL_DELSUBDIR    (1<<13) // in delete function only
#define DISKFILE_CPYCTRL_ALLFILE      (1<<14) // for overwrite/skipfile/delfile
#define DISKFILE_CPYCTRL_ALLSUBDIR    (1<<15) // for delsubdir

#define DISKFILE_CPYCTRL_IGNALLERR    (1<<17) // ignore all errors
#define DISKFILE_CPYCTRL_SUBFINDNEXT  (1<<18) // call subdirscan_findnext
#define DISKFILE_CPYCTRL_REOPEN       (1<<19) // reopen currently playing file (ie: after rename)
#define DISKFILE_CPYCTRL_SKIPWAITHPP  (1<<20) // skip one (del/first) waiting
#define DISKFILE_CPYCTRL_DONTWAITHPP  (1<<21) // don't wait for (the end of) high priority processes

#define DISKFILE_COUNTCTRL_NEXTFILE   1
#define DISKFILE_COUNTCTRL_NEWSUBDIR  2 // for !EMPTYDIRCOPY
#define DISKFILE_COUNTCTRL_COMPLETE   4

typedef struct filecopy_t{
 struct mainvars *mvp;
 struct playlist_side_info *psi_src,*psi_dest;
 struct playlist_entry_info *pei_selected; // current subdir or file
 struct playlist_entry_info *pei_curr;     // current file (it's not in subdir)
 struct playlist_entry_info *pei_last;

 unsigned long cpy_type,cpy_ctrl;
 unsigned long directories_selected;
 unsigned long entries_selected;
 unsigned long filenum_selcount;
 unsigned long filenum_curr;
 mpxp_int64_t filebytes_copied;

 unsigned int count_ctrl;
 struct playlist_entry_info *count_pei;
 void *count_tw;
 int count_itemnum;
 unsigned int counted_directories;
 unsigned int counted_filenum_all;
 unsigned int counted_filenum_media,counted_filenum_mtime;
 unsigned int counted_filenum_list;
 unsigned int counted_filenum_other;
 mpxp_int64_t counted_filesizes_all;
 mpxp_int64_t counted_filesizes_media;
 mpxp_int64_t counted_filesizes_list;
 mpxp_int64_t counted_timemsec_media;

 void *statwin_tw; // static count/rename/move/del/copy status window
 mpxp_int64_t statwin_begintime_waitdisp; // timeout for displayin "waiting" window

 int retcode,last_error;
 mpxp_filesize_t filelen;
 mpxp_filesize_t filepos;
 char *buffer;
 struct mpxplay_diskdrive_data_s *mdds_src,*mdds_dest;
 void *filehand_data_src,*filehand_data_dest;
 struct frame frp_src;
 struct frame frp_dest;
 char *selected_filename;            // pei->filename or psi->currdir (type==DFT_UPDIR)
 char infilename[MAX_PATHNAMELEN];
 char outfilename[MAX_PATHNAMELEN];
 char lastfilename[MAX_PATHNAMELEN]; // last outfilename
 char path_src[MAX_PATHNAMELEN];
 char path_dest[MAX_PATHNAMELEN];
 char outpath_argument[MAX_PATHNAMELEN]; // given outpath
 char outpath_root[MAX_PATHNAMELEN]; // starting outpath
 char outpath_newsubdir[MAX_PATHNAMELEN]; // save of the built/renamed outdir
 char outpath_curr[MAX_PATHNAMELEN]; // current outpath (subdir)
 char headtext[48];
 char buttontext[48];
 char source_filtermask[48];
 struct pds_subdirscan_t dsi;
}filecopy_t;

static void diskfile_count_files_stop(struct filecopy_t *fc);
static int  diskfile_filecopymove_make_subdir(struct filecopy_t *fc,char *outdirname);
static void diskfile_filecopymove_makefilenames(struct filecopy_t *fc);
static void diskfile_filecopymove_checkfilenames(struct filecopy_t *fc);
static void diskfile_filecopy_closefiles(struct filecopy_t *fc);
static void diskfile_filecopy_do_copy(struct filecopy_t *fc);
static unsigned int diskfile_filemove_check_samedrive(struct filecopy_t *fc);
static void diskfile_filecopymove_postprocess(struct filecopy_t *fc);
static void diskfile_filecopymove_select(struct filecopy_t *fc);
static void diskfile_filecopymove_loop(struct filecopy_t *fc);
static void playlist_diskfile_delete_do(struct filecopy_t *fc);
static void diskfile_show_multifileinfos_window(struct filecopy_t *fc);
static unsigned int diskfile_renamebyid3_createoutfilename(struct filecopy_t *fc,struct playlist_entry_info *pei,char *outbuf,unsigned int bufsize);

extern unsigned int refdisp,displaymode,desktopmode,preloadinfo,loadid3tag;
#ifdef __DOS__
extern unsigned int is_lfn_support,uselfn;
#endif

static char source_default_filter_copy[48]=PDS_DIRECTORY_ALLFILE_STR; // all supported files
static char source_default_filter_del[48]="*.?*"; // all files (not only the supported)
static char source_default_filter_info[48]=PDS_DIRECTORY_ALLFILE_STR; // no diff at info

static void diskfile_statwin_close(struct filecopy_t *fc)
{
 if(fc && fc->statwin_tw){
  display_textwin_closewindow_buttons(fc->statwin_tw);
  fc->statwin_tw=NULL;
 }
}

static struct filecopy_t *diskfile_filecopy_alloc(struct mainvars *mvp)
{
 struct filecopy_t *fc;

 fc=calloc(1,sizeof(struct filecopy_t));
 if(!fc)
  return fc;

 fc->mvp=mvp;
 fc->psi_src=mvp->psie;

 return fc;
}

static void diskfile_filecopy_dealloc(struct filecopy_t *fc)
{
#ifdef __DOS__
 void *tw;
#endif

 diskfile_statwin_close(fc);

#ifdef __DOS__
 tw=display_textwin_openwindow_message(NULL,NULL,"Flushing disk caches ...");
 pds_drives_flush();
 display_textwin_closewindow_message(tw);
#endif

 if(fc){
  if(fc->buffer){
   free(fc->buffer);
   fc->buffer=NULL;
  }
  mpxplay_infile_frame_free(&fc->frp_src);
  free(fc);
 }
 refdisp|=RDT_RESET_EDIT|RDT_EDITOR;
}

//-------------------------------------------------------------------------
static unsigned int diskfile_count_files_chkentry(struct filecopy_t *fc,char *filename,unsigned long filesize)
{
 if(fc->cpy_type&DISKFILE_CPYTYPE_INFO_TIME){
  if(mpxplay_infile_check_extension(filename,fc->mdds_src)){
   if(mpxplay_infile_get_header_by_ext(&fc->frp_src,fc->mdds_src,filename))
    fc->counted_timemsec_media+=fc->frp_src.infile_infos->timemsec;
   mpxplay_infile_close(&fc->frp_src);
   fc->counted_filenum_mtime++;
  }
  return 1;
 }

 if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_NOEXTCHK)){
  if(mpxplay_infile_check_extension(filename,fc->mdds_src)){
   fc->counted_filenum_media++;
   fc->counted_filesizes_media+=filesize;
  }else if(playlist_loadlist_check_extension(filename)){
   fc->counted_filenum_list++;
   fc->counted_filesizes_list+=filesize;
  }else if(fc->cpy_type&DISKFILE_CPYTYPE_INFO_SIZE){
   fc->counted_filenum_other++;
  }else
   return 0;
 }

 fc->counted_filesizes_all+=filesize;

 return 1;
}

static void diskfile_count_files_updatemsg(struct filecopy_t *fc)
{
 char sout[MAX_PATHNAMELEN+100];
 if(fc->cpy_type&(DISKFILE_CPYTYPE_INFO_SIZE|DISKFILE_CPYTYPE_INFO_TIME))
  diskfile_show_multifileinfos_window(fc);
 else{
  if(fc->cpy_type&DISKFILE_CPYTYPE_DEL)
   sprintf(sout,"(total %d file%s and %d subdir%s)",fc->counted_filenum_all,((fc->counted_filenum_all>1)? "s":""),fc->counted_directories,((fc->counted_directories>1)? "s":""));
  else{
   if(fc->psi_src->selected_files){
    sprintf(sout," %s %d file%s (total %d file%s with %d subdir%s) to",
     ((fc->cpy_type&DISKFILE_CPYTYPE_COPY)? "Copy":"Move"),
     fc->psi_src->selected_files,((fc->psi_src->selected_files>1)? "s":""),
     fc->counted_filenum_all,((fc->counted_filenum_all>1)? "s":""),
     fc->counted_directories,((fc->counted_directories>1)? "s":""));
   }else{
    snprintf(sout,sizeof(sout)," %s \"%s\" (total %d file%s with %d subdir%s) to",
     ((fc->cpy_type&DISKFILE_CPYTYPE_COPY)? "Copy":"Move"),
     pds_getfilename_from_fullname((fc->pei_selected->entrytype==DFT_UPDIR)? fc->psi_src->currdir:fc->pei_selected->filename),
     fc->counted_filenum_all,((fc->counted_filenum_all>1)? "s":""),
     fc->counted_directories,((fc->counted_directories>1)? "s":""));
   }
  }
  display_textwin_update_msg(fc->count_tw,fc->count_itemnum,sout);
 }
}

static void diskfile_count_files_in_subdirs(struct filecopy_t *fc)
{
 struct playlist_entry_info *pei=fc->count_pei;
 struct pds_subdirscan_t *dsi;
 char searchpath[MAX_PATHNAMELEN];

 if((pei>fc->psi_src->lastentry) || (pei>fc->pei_last))
  goto err_out_finish;

 if((fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) && !(pei->infobits&PEIF_SELECTED))
  goto err_out_skip;

 if(pei->entrytype==DFT_UPLIST)
  goto err_out_skip;

 if((fc->cpy_type&DISKFILE_CPYTYPE_INFO_TIME) && ((pei->entrytype>=DFT_AUDIOFILE) || (pei->entrytype==DFT_NOTCHECKED)) && pds_filename_wildchar_cmp(pei->filename,fc->source_filtermask)){
  unsigned long timemsec=playlist_entry_get_timemsec(pei);
  if(timemsec)
   fc->counted_timemsec_media+=timemsec;
  else{
   if(mpxplay_infile_get_header_by_ext(&fc->frp_src,fc->mdds_src,pei->filename))
    fc->counted_timemsec_media+=fc->frp_src.infile_infos->timemsec;
   mpxplay_infile_close(&fc->frp_src);
  }
  fc->counted_filenum_mtime++;
 }
 if((pei->entrytype==DFT_DRIVE) || (pei->entrytype==DFT_UPDIR) || (pei->entrytype==DFT_SUBDIR)){
  dsi=&fc->dsi;
  if(!(fc->count_ctrl&DISKFILE_COUNTCTRL_NEXTFILE)){
   unsigned int len;
   fc->mdds_src=pei->mdds;
   switch(pei->entrytype){
    case DFT_DRIVE:len=pds_strncpy(searchpath,pei->filename,sizeof(PDS_DIRECTORY_DRIVE_STR)-1);break;
    case DFT_UPDIR:len=pds_strcpy(searchpath,fc->psi_src->currdir);break;
    default:len=pds_strcpy(searchpath,pei->filename);
   }
   len+=pds_strcpy(&searchpath[len],PDS_DIRECTORY_SEPARATOR_STR);
   len+=pds_strcpy(&searchpath[len],PDS_DIRECTORY_ALLDIR_STR);
   len+=pds_strcpy(&searchpath[len],PDS_DIRECTORY_SEPARATOR_STR);
   pds_strcpy(&searchpath[len],fc->source_filtermask);
   if(!mpxplay_diskdrive_subdirscan_open(fc->mdds_src,searchpath,_A_NORMAL,dsi))
    funcbit_enable(fc->count_ctrl,DISKFILE_COUNTCTRL_NEXTFILE);
   if((fc->cpy_ctrl&DISKFILE_CPYCTRL_EMPTYDIRCOPY) && (fc->cpy_type!=DISKFILE_CPYTYPE_INFO_TIME))
    fc->counted_directories++;
  }
  if(fc->count_ctrl&DISKFILE_COUNTCTRL_NEXTFILE){
   int fferror=mpxplay_diskdrive_subdirscan_findnextfile(fc->mdds_src,dsi);
   if(fferror<0){
    funcbit_disable(fc->count_ctrl,DISKFILE_COUNTCTRL_NEXTFILE);
    mpxplay_diskdrive_subdirscan_close(fc->mdds_src,dsi);
   }else{
    if(dsi->flags&SUBDIRSCAN_FLAG_SUBDIR){
     if(fc->cpy_ctrl&DISKFILE_CPYCTRL_EMPTYDIRCOPY){
      if(fc->cpy_type!=DISKFILE_CPYTYPE_INFO_TIME)
       fc->counted_directories++;
     }else
      funcbit_enable(fc->count_ctrl,DISKFILE_COUNTCTRL_NEWSUBDIR);
    }
    if((fferror==0) && !(dsi->ff->attrib&(_A_SUBDIR|_A_VOLID))){
     if(diskfile_count_files_chkentry(fc,dsi->fullname,dsi->ff->size)){
      if(fc->cpy_type!=DISKFILE_CPYTYPE_INFO_TIME)
       fc->counted_filenum_all++;
      if(fc->count_ctrl&DISKFILE_COUNTCTRL_NEWSUBDIR){
       if(fc->cpy_type!=DISKFILE_CPYTYPE_INFO_TIME)
        fc->counted_directories++;
       funcbit_disable(fc->count_ctrl,DISKFILE_COUNTCTRL_NEWSUBDIR);
      }
     }
    }
    return;
   }
  }
 }else{
  if(fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_INFO_SIZE))
   if(pds_filename_wildchar_cmp(pei->filename,fc->source_filtermask))
    diskfile_count_files_chkentry(fc,pei->filename,((pei->filesize)? pei->filesize:pds_getfilesize(pei->filename)));
 }

err_out_skip:
 if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE)) // no more file
  goto err_out_finish;
 pei++;
 fc->count_pei=pei;
 return;

err_out_finish:
 if((fc->cpy_type&DISKFILE_CPYTYPE_INFO_SIZE) && fc->counted_filenum_media){
  fc->cpy_type=DISKFILE_CPYTYPE_INFO_TIME;
  if(fc->psi_src->selected_files){
   fc->count_pei=fc->pei_selected=fc->psi_src->firstentry;
   fc->pei_last=fc->psi_src->lastentry;
  }else
   fc->count_pei=fc->pei_selected=fc->pei_last=fc->psi_src->editorhighline;
 }else{
  funcbit_enable(fc->count_ctrl,DISKFILE_COUNTCTRL_COMPLETE);
  diskfile_count_files_stop(fc);
 }
}

static void diskfile_count_files_reset(struct filecopy_t *fc)
{
 fc->count_ctrl=0;
 fc->directories_selected=0;
 fc->counted_directories=0;
 fc->counted_filenum_all=0;
 fc->counted_filenum_media=0;
 fc->counted_filenum_mtime=0;
 fc->counted_filenum_list=0;
 fc->counted_filenum_other=0;
 fc->counted_filesizes_all=0;
 fc->counted_filesizes_media=0;
 fc->counted_filesizes_list=0;
 fc->counted_timemsec_media=0;
}

static void diskfile_count_files_start(struct filecopy_t *fc)
{
 struct playlist_entry_info *pei;
 unsigned int filenum,dirnum;

 diskfile_count_files_reset(fc);

 if(fc->psi_src->selected_files){
  pei=fc->psi_src->firstentry;
  filenum=dirnum=0;
  do{
   if(pei->infobits&PEIF_SELECTED){
    if((pei->entrytype==DFT_DRIVE) || (pei->entrytype==DFT_UPDIR) || (pei->entrytype==DFT_SUBDIR))
     dirnum++;
    else if(pds_filename_wildchar_cmp(pei->filename,fc->source_filtermask))
     filenum++;
   }
   pei++;
  }while(pei<=fc->psi_src->lastentry);
  fc->counted_filenum_all=filenum;
  fc->directories_selected=dirnum;
  fc->entries_selected=dirnum+filenum;
  if(dirnum || (fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_INFO_SIZE)))
   fc->count_pei=fc->psi_src->firstentry;
  else
   funcbit_enable(fc->count_ctrl,DISKFILE_COUNTCTRL_COMPLETE);
 }else{
  pei=fc->psi_src->editorhighline;
  fc->entries_selected=1;
  if((pei->entrytype==DFT_DRIVE) || (pei->entrytype==DFT_UPDIR) || (pei->entrytype==DFT_SUBDIR)){
   fc->count_pei=pei;
   fc->directories_selected=1;
  }else{
   fc->counted_filenum_all=1;
   if(fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_INFO_SIZE)){
    fc->counted_filesizes_all=(pei->filesize)? pei->filesize:pds_getfilesize(pei->filename);
    fc->counted_timemsec_media=playlist_entry_get_timemsec(pei);
   }
   funcbit_enable(fc->count_ctrl,DISKFILE_COUNTCTRL_COMPLETE);
  }
 }

 if(!funcbit_test(fc->count_ctrl,DISKFILE_COUNTCTRL_COMPLETE)){
  mpxplay_timer_addfunc(diskfile_count_files_in_subdirs,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,0);
  mpxplay_timer_addfunc(diskfile_count_files_updatemsg,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_LOWPRIOR,mpxplay_timer_secs_to_counternum(1)/20);
 }
}

static void diskfile_count_files_stop(struct filecopy_t *fc)
{
 mpxplay_timer_deletefunc(diskfile_count_files_in_subdirs,fc);
 mpxplay_timer_deletefunc(diskfile_count_files_updatemsg,fc);
 diskfile_count_files_updatemsg(fc);
}

//-------------------------------------------------------------------------
static void diskfile_filecopymove_correct_fcpei_add(struct filecopy_t *fc,struct playlist_entry_info *pei)
{
 if(fc->pei_curr && pei<=fc->pei_curr)
  fc->pei_curr++;
 if(fc->pei_selected && pei<=fc->pei_selected)
  fc->pei_selected++;
 fc->pei_last++;
}

static void diskfile_filecopymove_correct_fcpei_del(struct filecopy_t *fc,struct playlist_entry_info *pei)
{
 if(pei==fc->pei_curr)
  fc->pei_curr=NULL;
 if(fc->pei_selected && pei>fc->pei_selected) // ???
  fc->pei_selected--;
}

static void diskfile_filecopymove_correct_fcpei_move(struct filecopy_t *fc,struct playlist_entry_info *pei_dest,struct playlist_entry_info *pei_src)
{
 if(pei_src<fc->pei_selected && pei_dest>fc->pei_selected)
  fc->pei_selected--;
 if(pei_src>fc->pei_selected && pei_dest<fc->pei_selected)
  fc->pei_selected++;
 if(fc->pei_curr==pei_src){
  if(pei_dest && (pei_dest->infobits&PEIF_SELECTED)){
   funcbit_disable(pei_dest->infobits,PEIF_SELECTED);
   if(fc->psi_src->selected_files)
    fc->psi_src->selected_files--;
   refdisp|=RDT_EDITOR;
  }
  fc->pei_curr=NULL;
 }
}

//-------------------------------------------------------------------------
static unsigned int diskfile_filecopymove_build_subdirpath(struct filecopy_t *fc,char *path,char *dirname)
{
 unsigned int len=pds_strcpy(path,fc->outpath_root);
 if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_SUBONECOPY)){
  if(path[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR)
   len+=pds_strcpy(&path[len],PDS_DIRECTORY_SEPARATOR_STR);
  len+=pds_strcpy(&path[len],pds_getfilename_from_fullname(dirname));
 }
 return len;
}

static void diskfile_filecopymove_build_outpath(struct filecopy_t *fc,char *dirname)
{
 struct pds_subdirscan_t *dsi;
 unsigned int i,len;
 char path[MAX_PATHNAMELEN];

 if(fc->cpy_type&DISKFILE_CPYTYPE_DEL)
  return;

 len=diskfile_filecopymove_build_subdirpath(fc,path,dirname);

 dsi=&fc->dsi;
 i=pds_strlen(dsi->startdir);
 if((path[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR) && (dsi->currdir[i]!=PDS_DIRECTORY_SEPARATOR_CHAR))
  len+=pds_strcpy(&path[len],PDS_DIRECTORY_SEPARATOR_STR);
 len+=pds_strcpy(&path[len],&dsi->currdir[i]);
 pds_strcpy(fc->outpath_curr,path);
}

static void diskfile_filecopymove_rebuild_outpathargument(struct filecopy_t *fc,char *path)
{
 pds_strcpy(fc->outpath_root,path);
 pds_filename_assemble_fullname(fc->outpath_argument,path,PDS_DIRECTORY_ALLFILE_STR);
}

//--------------------------------------------------------------------------
static void diskfile_filecopymove_update_sides(struct filecopy_t *fc,char *infilename,char *outfilename,struct playlist_entry_info *pei_src)
{
 struct playlist_side_info *psi=fc->mvp->psi0;
 unsigned int side,modified_side,directory;
 struct playlist_entry_info *pei,*new_pei,pei_tmp,pei_temp;
 char inpath[MAX_PATHNAMELEN]="",strtmp[MAX_PATHNAMELEN];

 if(infilename && (fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME|DISKFILE_CPYTYPE_DEL)) && (fc->retcode==MPXPLAY_ERROR_FILEHAND_OK))
  pds_getpath_from_fullname(inpath,infilename);

 if(outfilename){
  pds_memcpy(&pei_tmp,pei_src,sizeof(struct playlist_entry_info));
  pei_tmp.filename=outfilename;
  pei_tmp.mdds=fc->mdds_dest;
 }

 if(outfilename && ((fc->cpy_type&DISKFILE_CPYTYPE_COPY) || (pds_stricmp(inpath,fc->outpath_curr)!=0))){
  for(side=0;side<PLAYLIST_MAX_SIDES;side++,psi++){
   if(!(psi->editsidetype&PLT_ENABLED) || !(psi->editsidetype&PLT_DIRECTORY) || psi->sublistlevel)
    continue;
   if(pds_stricmp(fc->outpath_curr,psi->currdir)!=0)
    continue;
   modified_side=0;
   pei=playlist_search_filename(psi,outfilename,-1,NULL);
   if((fc->retcode==MPXPLAY_ERROR_FILEHAND_OK) || (fc->retcode==MPXPLAY_ERROR_FILEHAND_DELETE)){
    if(pei){ // update an existent playlist entry (with the datas from infile)
     do{
      playlist_editlist_delfile_one(psi,pei,EDITLIST_MODE_HEAD|EDITLIST_MODE_ID3);
      if(playlist_editlist_addfile_one(fc->psi_src,psi,&pei_tmp,pei,EDITLIST_MODE_HEAD|EDITLIST_MODE_ID3))
       modified_side=1;
      new_pei=playlist_search_filename(psi,outfilename,-1,pei+1);
      if(!new_pei)
       break;
      pei=new_pei;
     }while(1); // maybe the same file is more times in the playlist
    }else{   // create a new playlist entry (copy datas from infile)
     if(playlist_editlist_addfile_one(fc->psi_src,psi,&pei_tmp,NULL,(EDITLIST_MODE_ALL&(~EDITLIST_MODE_INDEX)))){
      pei=playlist_editlist_addfileone_postproc(psi,psi->lastentry);
      modified_side=1;
      if(psi==fc->psi_src)
       diskfile_filecopymove_correct_fcpei_add(fc,pei);
     }
    }
   }else{
    if(pei){ // delete entry (output file) at error
     playlist_editlist_delfile_one(psi,pei,EDITLIST_MODE_ALL);
     modified_side=1;
     if(psi==fc->psi_src)
      diskfile_filecopymove_correct_fcpei_del(fc,pei);
    }
   }

   if(modified_side){
    if((psi!=fc->mvp->psie) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) && !fc->directories_selected)
     playlist_editorhighline_set(psi,pei);
    if(psi==fc->mvp->psip)
     refdisp|=RDT_EDITOR|RDT_BROWSER;
    else
     refdisp|=RDT_EDITOR;
   }
  }
 }

 if(infilename && (fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME|DISKFILE_CPYTYPE_DEL)) && (fc->retcode==MPXPLAY_ERROR_FILEHAND_OK)){
  psi=fc->mvp->psi0;
  pds_getpath_from_fullname(inpath,infilename);
  for(side=0;side<PLAYLIST_MAX_SIDES;side++,psi++){
   if(!(psi->editsidetype&PLT_ENABLED))
    continue;
   directory=((psi->editsidetype&PLT_DIRECTORY) && !psi->sublistlevel);
   if(directory && fc->directories_selected && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) && (fc->cpy_type&DISKFILE_CPYTYPE_RENAME) && pds_strnicmp(psi->currdir,infilename,pds_strlen(infilename))==0){ // (up)directory rename
    unsigned int inflen=pds_strlen(infilename);
    unsigned int i=pds_strcpy(strtmp,outfilename);
    pds_strcpy(&strtmp[i],&psi->currdir[inflen]);
    pds_strcpy(psi->currdir,strtmp);
    for(pei=psi->firstentry;pei<=psi->lastentry;pei++){
     if(pds_strnicmp(pei->filename,infilename,inflen)==0){
      unsigned int pos=pds_strcpy(strtmp,outfilename);
      pds_strcpy(&strtmp[pos],&pei->filename[inflen]);
      pds_memcpy(&pei_temp,pei,sizeof(struct playlist_entry_info));
      pei_temp.filename=strtmp;
      pei_temp.mdds=fc->mdds_dest;
      playlist_editlist_delfile_one(psi,pei,EDITLIST_MODE_FILENAME);
      playlist_editlist_addfile_one(fc->psi_src,psi,&pei_temp,pei,EDITLIST_MODE_FILENAME);
      funcbit_disable(pei->infobits,PEIF_SORTED); // ??? after manual sort?
      refdisp|=RDT_EDITOR;
     }
    }
   }else if(!directory || pds_stricmp(inpath,psi->currdir)==0){
    pei=playlist_search_filename(psi,infilename,-1,NULL);
    if(pei){
     if((!directory && (fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME))) || (pds_stricmp(inpath,fc->outpath_curr)==0)){ // rename (or rename in playlist by move)
      if(outfilename){ // ???
       do{
        playlist_editlist_delfile_one(psi,pei,EDITLIST_MODE_FILENAME);
        playlist_editlist_addfile_one(fc->psi_src,psi,&pei_tmp,pei,EDITLIST_MODE_FILENAME);
        funcbit_disable(pei->infobits,PEIF_SORTED); // ??? after manual sort?
        new_pei=playlist_order_entry(psi,pei);
        if(new_pei && (psi==fc->psi_src)){
         if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) && (pei==fc->pei_curr)) // !!!
          playlist_editorhighline_set(psi,new_pei);
         diskfile_filecopymove_correct_fcpei_move(fc,new_pei,pei);
        }
        pei=playlist_search_filename(psi,infilename,-1,pei+1);
       }while(pei); // maybe the same file is more times in the playlist
       refdisp|=RDT_EDITOR;
      }
     }else{                                       // move or del
      if(directory || (psi==fc->psi_src)){
       playlist_editlist_delfile_one(psi,pei,EDITLIST_MODE_ALL);
       if(psi==fc->psi_src)
        diskfile_filecopymove_correct_fcpei_del(fc,pei);
       if(psi==fc->mvp->psip)
        refdisp|=RDT_EDITOR|RDT_BROWSER;
       else
        refdisp|=RDT_EDITOR;
      }
     }
    }
   }
  }
 }
}

static void diskfile_filecopy_makebar(char *barstr,unsigned int percent)
{
 unsigned int i,b=percent*DISKFILE_FILECOPY_BARLEN/100;

 for(i=0;i<DISKFILE_FILECOPY_BARLEN;i++){
  if(i<b)
   barstr[i]='Û'; // 219
  else
   barstr[i]='°'; // 176
 }
 barstr[i]=0;
}

static display_textwin_button_t buttons_errorhand_select[]={
 {" Ignore "    ,0x1769}, // 'i'
 {""            ,0x1749}, // 'I'
 {" Ignore All ",0x1e61}, // 'a'
 {""            ,0x1e41}, // 'A'
 {" Cancel "    ,0x2e63}, // 'c'
 {""            ,0x2e43}, // 'C'
 {""            ,KEY_ESC},// ESC
 {NULL,0}
};

static display_textwin_button_t buttons_errorhand_ok[]={
 {"[ Ok ]"    ,KEY_ESC}, //
 {NULL,0}
};

static void diskfile_filecopy_errorhandler(struct filecopy_t *fc,unsigned int extkey)
{
 switch(extkey){
  case 0x1e61:
  case 0x1e41:funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_IGNALLERR);
  case 0x1769:
  case 0x1749:mpxplay_timer_addfunc(diskfile_filecopymove_loop,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
              break;
  default:mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
 }
}

static void diskfile_filecopy_errormessage(struct filecopy_t *fc,char *msg,char *filename)
{
 const unsigned int flags=(TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN);
 void *tw;

 diskfile_statwin_close(fc);

 tw=display_textwin_allocwindow_items(NULL,flags," Error ",diskfile_filecopy_errorhandler,fc);
 display_textwin_additem_msg_alloc(tw,flags,0,-1,msg);
 display_textwin_additem_msg_alloc(tw,flags,0,-1,filename);
 if((fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) || (fc->cpy_ctrl&DISKFILE_CPYCTRL_SUBFINDNEXT)){
  display_textwin_additem_msg_static(tw,flags,0,-1,"");
  display_textwin_additem_buttons(tw,flags,0,-1,buttons_errorhand_select,NULL);
 }else
  display_textwin_additem_buttons(tw,flags,0,-1,buttons_errorhand_ok,NULL);
 display_textwin_openwindow_items(tw,0,0,0);
}

static void playlist_diskfile_show_errmsg(struct filecopy_t *fc)
{
 switch(fc->last_error){
  case MPXPLAY_ERROR_FILEHAND_OK:break;
  case MPXPLAY_ERROR_FILEHAND_USERABORT:/*diskfile_filecopy_errormessage(fc,"Copy aborted by user!");*/break;
  case MPXPLAY_ERROR_FILEHAND_MEMORY   :diskfile_filecopy_errormessage(fc,"Memory allocation error at",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_CANTOPEN :diskfile_filecopy_errormessage(fc,"Cannot open for reading at",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_CANTCREATE:diskfile_filecopy_errormessage(fc,"Cannot create",fc->lastfilename);break;
  case MPXPLAY_ERROR_FILEHAND_CANTREAD :diskfile_filecopy_errormessage(fc,"Cannot read file (Where is the disk?)",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_CANTWRITE:diskfile_filecopy_errormessage(fc,"Cannot write file (Disk full?)",fc->lastfilename);break;
  case MPXPLAY_ERROR_FILEHAND_DELETE   :diskfile_filecopy_errormessage(fc,"Couldn't delete file",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_RENAME   :diskfile_filecopy_errormessage(fc,"Couldn't rename/move file",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_REMOVEDIR:diskfile_filecopy_errormessage(fc,"Couldn't delete directory",fc->lastfilename);break;
  case MPXPLAY_ERROR_FILEHAND_CHANGEATTR:diskfile_filecopy_errormessage(fc,"Couldn't modify the attribs of",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_CANTPERFORM:diskfile_filecopy_errormessage(fc,"Cannot perform operation on this filetype",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_CANTCOPY :diskfile_filecopy_errormessage(fc,"Cannot copy/move (probably a correct fullpath is missing)!",fc->lastfilename);break;
  case MPXPLAY_ERROR_FILEHAND_COPYDIR  :diskfile_filecopy_errormessage(fc,"Cannot copy/move/del drive/updir/uplist",fc->infilename);break;
  case MPXPLAY_ERROR_FILEHAND_SAMEDIR  :diskfile_filecopy_errormessage(fc,"Cannot copy/move a directory to itself!",fc->lastfilename);break;
  case MPXPLAY_ERROR_FILEHAND_MULTITO1 :diskfile_filecopy_errormessage(fc,"Cannot copy/move multiply files to one!\nDestination must be an existent path:",fc->outpath_argument);break;
  case MPXPLAY_ERROR_FILEHAND_SKIPFILE :break;
  default:diskfile_filecopy_errormessage(fc,"Unknown error in copy at",fc->infilename);break;
 }
}

static display_textwin_button_t buttons_error_existfile[]={
 {" Overwrite ",0x186f}, // 'o'
 {""           ,0x184f}, // 'O'
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
};

static display_textwin_button_t buttons_error_samefile[]={
 {"[Rename/cont]",0x1372}, // 'r'
 {""           ,0x1352}, // 'R'
 {"[Skip file]",0x1f73}, // 's'
 {""           ,0x1f53}, // 'S'
 {"[Cancel]"   ,0x2e63}, // 'c'
 {""           ,0x2e43}, // 'C'
 {""           ,KEY_ESC},// ESC
 {NULL,0}
};

static display_textwin_button_t buttons_error_noext[]={
 {"[Rename/cont]",0x1372}, // 'r'
 {""           ,0x1352}, // 'R'
 {"[Make dir]" ,0x326d}, // 'm'
 {""           ,0x324d}, // 'M'
 {"[Skip file]",0x1f73}, // 's'
 {""           ,0x1f53}, // 'S'
 {"[Cancel]"   ,0x2e63}, // 'c'
 {""           ,0x2e43}, // 'C'
 {""           ,KEY_ESC},// ESC
 {NULL,0}
};

static void diskfile_filecopymove_existfile_keyhand(struct filecopy_t *fc,unsigned int extkey)
{
 switch(extkey){
  case 0x1372: // rename
  case 0x1352:
  case 0x186f: // overwrite
  case 0x184f:if(pds_stricmp(fc->outfilename,fc->lastfilename)==0){ // if the filename is not modified manually
               funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_OVERWRITE); // overwrite it
               funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_CREATENOEXT);
               mpxplay_timer_addfunc(diskfile_filecopymove_checkfilenames,fc,MPXPLAY_TIMERFLAG_INDOS,0); // ???
              }else
               mpxplay_timer_addfunc(diskfile_filecopymove_makefilenames,fc,MPXPLAY_TIMERFLAG_INDOS,0);
              return;
  case 0x326d: // make dir
  case 0x324d:if(diskfile_filecopymove_make_subdir(fc,fc->outfilename)==MPXPLAY_ERROR_FILEHAND_OK)
               mpxplay_timer_addfunc(diskfile_filecopymove_makefilenames,fc,MPXPLAY_TIMERFLAG_INDOS,0);
              else
               diskfile_filecopy_dealloc(fc);
              return;
  case 0x1e61: // overwrite all
  case 0x1e41:if(pds_stricmp(fc->outfilename,fc->lastfilename)==0){
               funcbit_enable(fc->cpy_ctrl,(DISKFILE_CPYCTRL_OVERWRITE|DISKFILE_CPYCTRL_ALLFILE));
               mpxplay_timer_addfunc(diskfile_filecopymove_checkfilenames,fc,MPXPLAY_TIMERFLAG_INDOS,0); // ???
              }else
               mpxplay_timer_addfunc(diskfile_filecopymove_makefilenames,fc,MPXPLAY_TIMERFLAG_INDOS,0);
              return;
  case 0x1f73: // skip
  case 0x1f53:funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SKIPFILE);
              fc->retcode=MPXPLAY_ERROR_FILEHAND_SKIPFILE;
              break;
  case 0x256b: // skip all
  case 0x254b:funcbit_enable(fc->cpy_ctrl,(DISKFILE_CPYCTRL_SKIPFILE|DISKFILE_CPYCTRL_ALLFILE));
              fc->retcode=MPXPLAY_ERROR_FILEHAND_SKIPFILE;
              break;
  case 0x2e63: // cancel
  case 0x2e43:
  case KEY_ESC:fc->retcode=MPXPLAY_ERROR_FILEHAND_USERABORT;
               diskfile_filecopy_dealloc(fc);
               return;
 }
 diskfile_filecopymove_select(fc);
}

static display_textwin_button_t buttons_error_delsubdir[]={
 {" Delete "   ,0x2064}, // 'd'
 {""           ,0x2044}, // 'D'
 {" All "      ,0x1e61}, // 'a'
 {""           ,0x1e41}, // 'A'
 {" Skip "     ,0x1f73}, // 's'
 {""           ,0x1f53}, // 'S'
 //{" Skipall "  ,0x256b}, // 'k'
 //{""           ,0x254b}, // 'K'
 {" Cancel "   ,0x2e63}, // 'c'
 {""           ,0x2e43}, // 'C'
 {""           ,KEY_ESC},// ESC
 {NULL,0}
};

static void diskfile_filemovedel_delsubdir_keyhand(struct filecopy_t *fc,unsigned int extkey)
{
 switch(extkey){
  case 0x2064:
  case 0x2044:funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_DELSUBDIR);
              break;
  case 0x1e61:
  case 0x1e41:funcbit_enable(fc->cpy_ctrl,(DISKFILE_CPYCTRL_DELSUBDIR|DISKFILE_CPYCTRL_ALLSUBDIR));
              break;
  case 0x1f73:
  case 0x1f53:fc->pei_selected++;
              break;
  //case 0x256b:
  //case 0x254b:fc->pei_selected++;
  //
  //            break;
  case 0x2e63:
  case 0x2e43:
  case KEY_ESC:fc->retcode=MPXPLAY_ERROR_FILEHAND_USERABORT;
               diskfile_filecopy_dealloc(fc);
               return;
 }
 mpxplay_timer_addfunc(diskfile_filecopymove_loop,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
}

//-------------------------------------------------------------------------
static int diskfile_filecopymove_make_dirtree(struct filecopy_t *fc,unsigned int tree)
{
 if(tree){ // create all subdirectories step by step
  struct pds_subdirscan_t *dsi=&fc->dsi;
  unsigned int i,len;
  char *path=&fc->outpath_curr[0];
  len=diskfile_filecopymove_build_subdirpath(fc,path,fc->outpath_newsubdir);
  if(!mpxplay_diskdrive_checkdir(fc->mdds_dest,path))
   if(mpxplay_diskdrive_mkdir(fc->mdds_dest,path)<0)
    goto err_out_co;
  for(i=0;i<dsi->subdir_level;i++){
   len+=pds_strcpy(&path[len],PDS_DIRECTORY_SEPARATOR_STR);
   len+=pds_strcpy(&path[len],dsi->subdir_names[i]);
   if(!mpxplay_diskdrive_checkdir(fc->mdds_dest,path))
    if(mpxplay_diskdrive_mkdir(fc->mdds_dest,path)<0)
     goto err_out_co;
  }
  return 0;
 }

 diskfile_filecopymove_build_outpath(fc,fc->outpath_newsubdir);

 if(mpxplay_diskdrive_checkdir(fc->mdds_dest,fc->outpath_curr))
  return 0;
 if(mpxplay_diskdrive_mkdir(fc->mdds_dest,fc->outpath_curr)==MPXPLAY_ERROR_FILEHAND_OK)
  return 0;

err_out_co:
 fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTCREATE;
 pds_strcpy(fc->lastfilename,fc->outpath_curr);
 return -1;
}

static int diskfile_filecopymove_make_subdir(struct filecopy_t *fc,char *outdirname)
{
 struct playlist_entry_info pei_dir;
 if(!mpxplay_diskdrive_checkdir(fc->mdds_dest,outdirname)){  //
  if(mpxplay_diskdrive_mkdir(fc->mdds_dest,outdirname)<0){ // create the dir if it doesn't exist
   fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTCREATE;
   pds_strcpy(fc->lastfilename,outdirname);
   return -1;
  }
  pds_memset(&pei_dir,0,sizeof(pei_dir));
  pei_dir.entrytype=DFT_SUBDIR;
  pei_dir.id3info[I3I_DFT_STORE]=DFTSTR_SUBDIR;
  diskfile_filecopymove_update_sides(fc,NULL,outdirname,&pei_dir);
 }
 return 0;
}

static unsigned int diskfile_filecopymove_check_samerootdir(char *rootdir,char *subdir)
{
 unsigned int rootlen=pds_strlen(rootdir),sublen=pds_strlen(subdir);
 if((sublen==rootlen) && pds_stricmp(rootdir,subdir)==0)
  return 1;
 if((sublen>rootlen) && (subdir[rootlen]==PDS_DIRECTORY_SEPARATOR_CHAR) && pds_strlicmp(subdir,rootdir)==0)
  return 1;
 return 0;
}

static int diskfile_filecopymove_remove_indir(struct filecopy_t *fc,char *path)
{
 int retcode;

 retcode=mpxplay_diskdrive_rmdir(fc->mdds_src,path);
 if(retcode==MPXPLAY_ERROR_FILEHAND_OK)
  return 0;

 fc->retcode=MPXPLAY_ERROR_FILEHAND_REMOVEDIR;
 pds_strcpy(fc->lastfilename,path);
 return -1;
}

static int diskfile_filecopymove_getnextfile(struct filecopy_t *fc)
{
 struct playlist_entry_info *pei;
 struct pds_subdirscan_t *dsi;
 int fferror;
 char srcname[MAX_PATHNAMELEN],path[MAX_PATHNAMELEN],newname[MAX_PATHNAMELEN];

 if(fc->pei_curr)
  pei=fc->pei_selected=fc->pei_curr;
 else
  pei=fc->pei_selected;

 fc->pei_curr=NULL;

 if((pei>fc->psi_src->lastentry) || (pei>fc->pei_last))
  goto err_out_finish;
 if(fc->filenum_selcount>=fc->entries_selected)
  goto err_out_finish;

 if((fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) && !(pei->infobits&PEIF_SELECTED))
  goto err_out_skip;

 if((fc->cpy_type&DISKFILE_CPYTYPE_RENBYID3) && (GET_HFT(pei->entrytype)==HFT_DFT)){
  pds_strcpy(fc->infilename,pei->filename);
  fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTPERFORM;
  goto err_out_skip;
 }

 if((pei->entrytype==DFT_DRIVE) || ((pei->entrytype==DFT_UPDIR) && !(fc->cpy_type&DISKFILE_CPYTYPE_COPY)) || (pei->entrytype==DFT_UPLIST)){
  fc->retcode=MPXPLAY_ERROR_FILEHAND_COPYDIR;
  goto err_out_skip;
 }

 switch(pei->entrytype){
  case DFT_UPDIR:pds_strcpy(srcname,fc->psi_src->currdir);break;
  default:pds_strcpy(srcname,pei->filename);
 }

 if(((pei->entrytype==DFT_UPDIR) || (pei->entrytype==DFT_SUBDIR)) && !(fc->cpy_type&DISKFILE_CPYTYPE_RENAME)){
  dsi=&fc->dsi;
  if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_SUBFINDNEXT)){
   unsigned int len;

   if((fc->cpy_type&DISKFILE_CPYTYPE_DEL) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_DELSUBDIR)){
    void *tw;

    diskfile_statwin_close(fc);

    tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN,"Delete directory",diskfile_filemovedel_delsubdir_keyhand,fc);
    display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,"The following directory will be DELETED");
    display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,srcname);
    display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,buttons_error_delsubdir,NULL);
    display_textwin_openwindow_items(tw,0,0,0);
    return 2;
   }
   if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_ALLSUBDIR))
    funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_DELSUBDIR);

   fc->mdds_src=pei->mdds;
   pds_strcpy(fc->outpath_curr,fc->outpath_root);
   // create subdirectory in output dir
   if(fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_MOVE) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_SUBONECOPY)){

    pds_filename_wildchar_rename(newname,srcname,pds_getfilename_from_fullname(fc->outpath_argument));
    pds_filename_assemble_fullname(fc->outpath_newsubdir,fc->outpath_root,newname);

    if(diskfile_filecopymove_check_samerootdir(srcname,fc->outpath_newsubdir)){ // copy/move a directory to itself?
     fc->retcode=MPXPLAY_ERROR_FILEHAND_SAMEDIR;
     pds_strcpy(fc->lastfilename,fc->outpath_newsubdir);
     goto err_out_skip;
    }

    if(fc->cpy_ctrl&DISKFILE_CPYCTRL_EMPTYDIRCOPY){
     if(diskfile_filecopymove_make_subdir(fc,fc->outpath_newsubdir)<0)
      goto err_out_skip;
     pei=fc->pei_selected;
    }
   }else
    pds_strcpy(fc->outpath_newsubdir,srcname);

   // start search in subdirs
   len=pds_strcpy(path,srcname);
   len+=pds_strcpy(&path[len],PDS_DIRECTORY_SEPARATOR_STR);
   len+=pds_strcpy(&path[len],PDS_DIRECTORY_ALLDIR_STR);
   len+=pds_strcpy(&path[len],PDS_DIRECTORY_SEPARATOR_STR);
   pds_strcpy(&path[len],fc->source_filtermask);
   if(mpxplay_diskdrive_subdirscan_open(fc->mdds_src,path,_A_NORMAL,dsi)==0){
    funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SUBFINDNEXT);
    diskfile_filecopymove_build_outpath(fc,fc->outpath_newsubdir);
   }else{
    fc->filenum_selcount++;
    goto err_out_skip;
   }
  }

  fferror=mpxplay_diskdrive_subdirscan_findnextfile(fc->mdds_src,dsi);

  if(fferror<0){ // no more files in subdir
   mpxplay_diskdrive_subdirscan_close(fc->mdds_src,dsi);
   if((fc->retcode==MPXPLAY_ERROR_FILEHAND_OK) && (fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_DEL))){
    if(diskfile_filecopymove_remove_indir(fc,dsi->startdir)==0) // delete subdir at move/del
     diskfile_filecopymove_update_sides(fc,dsi->startdir,NULL,pei);
   }
   if((fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK) || !(fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_DEL))){
    if((pei->infobits&PEIF_SELECTED) && ((fc->retcode==MPXPLAY_ERROR_FILEHAND_OK) || (fc->cpy_type&DISKFILE_CPYTYPE_MOVE))){ // !!! failed copy at move?
     funcbit_disable(pei->infobits,PEIF_SELECTED);
     if(fc->psi_src->selected_files)
      fc->psi_src->selected_files--;
     refdisp|=RDT_EDITOR;
    }
    fc->pei_selected++;
   }
   funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SUBFINDNEXT);
   fc->filenum_selcount++;
   goto err_out_getnext;
  }

  if(dsi->flags&SUBDIRSCAN_FLAG_UPDIR){
   if((fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_DEL)) && (fc->retcode==MPXPLAY_ERROR_FILEHAND_OK))
    diskfile_filecopymove_remove_indir(fc,dsi->prevdir); // delete subdir at move/del
   diskfile_filecopymove_build_outpath(fc,fc->outpath_newsubdir);
  }
  if((dsi->flags&SUBDIRSCAN_FLAG_SUBDIR) && (fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_MOVE))){
   if(fc->cpy_ctrl&DISKFILE_CPYCTRL_EMPTYDIRCOPY){
    if(diskfile_filecopymove_make_dirtree(fc,0)<0)
     return 1;
   }else
    funcbit_enable(fc->count_ctrl,DISKFILE_COUNTCTRL_NEWSUBDIR);
  }

  if((fferror==0) && !(dsi->ff->attrib&(_A_SUBDIR|_A_VOLID))){
   if((fc->cpy_ctrl&DISKFILE_CPYCTRL_NOEXTCHK) || mpxplay_infile_check_extension(dsi->ff->name,fc->mdds_src) || playlist_loadlist_check_extension(dsi->ff->name)){
    if(fc->count_ctrl&DISKFILE_COUNTCTRL_NEWSUBDIR){
     funcbit_disable(fc->count_ctrl,DISKFILE_COUNTCTRL_NEWSUBDIR);
     if(diskfile_filecopymove_make_dirtree(fc,1)<0)
      return 1;
    }
    pds_strcpy(fc->infilename,dsi->fullname);
    pds_filename_assemble_fullname(fc->outfilename,fc->outpath_curr,fc->infilename);
    return 0;
   }
  }
  return 1;
 }

 // normal file (audio, playlist)
 if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) || (fc->cpy_type&DISKFILE_CPYTYPE_RENAME) || pds_filename_wildchar_cmp(pei->filename,fc->source_filtermask)){
  pds_strcpy(fc->infilename,pei->filename);
  if(fc->cpy_type&DISKFILE_CPYTYPE_RENAME){
   if(fc->cpy_type&DISKFILE_CPYTYPE_RENBYID3)
    diskfile_renamebyid3_createoutfilename(fc,pei,newname,sizeof(newname));
   else
    pds_filename_wildchar_rename(newname,fc->infilename,pds_getfilename_from_fullname(fc->outpath_argument));
   pds_getpath_from_fullname(fc->outpath_curr,fc->infilename);
   pds_filename_assemble_fullname(fc->outfilename,fc->outpath_curr,newname);
  }else{
   pds_strcpy(fc->outpath_curr,fc->outpath_root);
   if(fc->cpy_ctrl&DISKFILE_CPYCTRL_WITHSUBDIR){
    pds_getpath_from_fullname(path,fc->infilename); // get the deepest subdir from infile (cut filename)
    pds_filename_assemble_fullname(fc->outpath_newsubdir,fc->outpath_root,pds_getfilename_from_fullname(path)); // and add to the path of outfile (cut last dir)
    if(diskfile_filecopymove_make_subdir(fc,fc->outpath_newsubdir)<0)
     goto err_out_skip;
    pds_strcpy(fc->outpath_curr,fc->outpath_newsubdir);
   }
   pds_filename_wildchar_rename(newname,fc->infilename,pds_getfilename_from_fullname(fc->outpath_argument));
   pds_filename_assemble_fullname(fc->outfilename,fc->outpath_curr,newname);
  }
  fc->pei_curr=pei;
  if(!pei->mdds)
   pei->mdds=playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(pei->filename));
  fc->mdds_src=pei->mdds;
  fc->filenum_selcount++;
  return 0;
 }

err_out_skip:
 fc->pei_selected++;
err_out_getnext:
 if(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) // there are more files/dirs
  return 1;
err_out_finish:
 return -1; // no next, finish
}

// possible dirname in outfilename (rename at overwrite)
static void diskfile_filecopymove_makefilenames(struct filecopy_t *fc)
{
 char newname[MAX_PATHNAMELEN];

 if(pds_strcutspc(fc->outfilename)){

  pds_sfn_limit(fc->outfilename);

  pds_filename_build_fullpath(newname,fc->outpath_curr,fc->outfilename);
  fc->mdds_dest=playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(newname));

  if(pds_path_is_dir(newname) && mpxplay_diskdrive_checkdir(fc->mdds_dest,newname)){
   pds_strcpy(fc->outpath_curr,newname);
   pds_filename_assemble_fullname(fc->outfilename,fc->outpath_curr,pds_getfilename_from_fullname(fc->infilename));
  }else{
   pds_strcpy(fc->outfilename,newname);
   pds_getpath_from_fullname(fc->outpath_curr,fc->outfilename);
  }
 }

 diskfile_filecopymove_checkfilenames(fc);
}

// output filename has made (in getnextfile), check it only
static void diskfile_filecopymove_checkfilenames(struct filecopy_t *fc)
{
 unsigned int el_len;
 void *tw;

 if(!fc->outfilename[0] || !fc->mdds_dest){ // ??? skips without notification
  fc->retcode=MPXPLAY_ERROR_FILEHAND_SKIPFILE;
  goto err_out_continue;
 }

 fc->retcode=MPXPLAY_ERROR_FILEHAND_OK;

 if(fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME)){ // move or rename
  if(pds_strcmp(fc->infilename,fc->outfilename)==0){ // input and output filenames are the same
   fc->retcode=MPXPLAY_ERROR_FILEHAND_SKIPFILE; // !!! automatic skip at move/rename
   goto err_out_continue;
  }
  if(pds_stricmp(fc->infilename,fc->outfilename)==0) // rename
   goto err_out_continue;
 }else{   // copy
  if(pds_stricmp(fc->infilename,fc->outfilename)==0) // input and output filenames are the same
   goto err_out_samefile;
 }

 if(fc->cpy_ctrl&DISKFILE_CPYCTRL_CREATENOEXT)
  goto err_out_continue;
 if(fc->cpy_ctrl&DISKFILE_CPYCTRL_OVERWRITE)
  goto err_out_continue;

 fc->filehand_data_dest=mpxplay_diskdrive_file_open(fc->mdds_dest,fc->outfilename,(O_RDONLY|O_BINARY));
 if(fc->filehand_data_dest){
  mpxplay_diskdrive_file_close(fc->filehand_data_dest);
  fc->filehand_data_dest=NULL;
  if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_SKIPFILE))
   goto err_out_exists;
  fc->retcode=MPXPLAY_ERROR_FILEHAND_SKIPFILE;
  goto err_out_continue;
 }

 if(!fc->pei_curr || (fc->pei_curr->entrytype!=DFT_SUBDIR))
  if(pds_filename_get_extension(fc->infilename) && !pds_filename_get_extension(fc->outfilename))
   goto err_out_noext;

err_out_continue:
 diskfile_filecopymove_select(fc);
 return;
err_out_exists:
 pds_strcpy(fc->lastfilename,fc->outfilename);
 el_len=pds_strlen(fc->outfilename)+1;
 if(el_len>50)
  el_len=50;
 diskfile_statwin_close(fc);
 tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN,fc->headtext,diskfile_filecopymove_existfile_keyhand,fc);
 display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,"The following file exists");
 display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,-1,el_len,fc->outfilename,MAX_PATHNAMELEN-2);
 display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,"Do you wish to write over the old file?");
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,buttons_error_existfile,&buttons_error_existfile[0]);
 display_textwin_openwindow_items(tw,0,0,0);
 return;
err_out_samefile:
 diskfile_statwin_close(fc);
 tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN,fc->headtext,diskfile_filecopymove_existfile_keyhand,fc);
 display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,"Cannot copy/move a file to itself!\nRename it or give a new target directory:");
 display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,-1,50,fc->outfilename,MAX_PATHNAMELEN-2);
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,buttons_error_samefile,NULL);
 display_textwin_openwindow_items(tw,0,0,0);
 return;
err_out_noext:
 pds_strcpy(fc->lastfilename,fc->outfilename);
 diskfile_statwin_close(fc);
 tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN,fc->headtext,diskfile_filecopymove_existfile_keyhand,fc);
 display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,"You perform a file copy/move without extension\nor the target directory doesn't exists.\nYou can continue or give an other target:");
 display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,-1,50,fc->outfilename,MAX_PATHNAMELEN-2);
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,buttons_error_noext,NULL);
 display_textwin_openwindow_items(tw,0,0,0);
}

static void diskfile_filecopy_openfiles(struct filecopy_t *fc)
{
 fc->retcode=MPXPLAY_ERROR_FILEHAND_OK;
 fc->filepos=0;

 if(!fc->buffer)
  fc->buffer=pds_malloc(DISKFILE_FILECOPY_BLOCKSIZE);
 if(!fc->buffer){
  fc->retcode=MPXPLAY_ERROR_FILEHAND_MEMORY;
  goto err_out_open;
 }

 fc->filehand_data_src=mpxplay_diskdrive_file_open(fc->mdds_src,fc->infilename,(O_RDONLY|O_BINARY));
 if(!fc->filehand_data_src){
  fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTOPEN;
  goto err_out_open;
 }

 fc->filehand_data_dest=mpxplay_diskdrive_file_open(fc->mdds_dest,fc->outfilename,(O_WRONLY|O_CREAT|O_BINARY));
 if(!fc->filehand_data_dest){
  fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTCREATE;
  goto err_out_open;
 }
 fc->filelen=mpxplay_diskdrive_file_length(fc->filehand_data_src);

 mpxplay_timer_addfunc(diskfile_filecopy_do_copy,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);

 return;

err_out_open:
 mpxplay_timer_addfunc(diskfile_filecopy_closefiles,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

static void diskfile_filecopy_closefiles(struct filecopy_t *fc)
{
 struct frame *frp=fc->mvp->frp0;

 mpxplay_diskdrive_file_close(fc->filehand_data_src);
 mpxplay_diskdrive_file_close(fc->filehand_data_dest);
 if(fc->filehand_data_dest && (fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK))
  mpxplay_diskdrive_unlink(fc->mdds_dest,fc->outfilename);

 fc->filehand_data_src=fc->filehand_data_dest=NULL;
 fc->filebytes_copied+=fc->filelen;

 if((fc->cpy_type&DISKFILE_CPYTYPE_MOVE) && (pds_stricmp(fc->mvp->pei0->filename,fc->infilename)==0) && !diskfile_filemove_check_samedrive(fc)){ // currently playing file is moved/renamed to another drive
  if(frp->filehand_datas){
   mpxplay_diskdrive_file_close(frp->filehand_datas);
   frp->filehand_datas=NULL;
   fc->retcode=mpxplay_diskdrive_unlink(fc->mdds_src,fc->infilename); // delete from old location
   frp->mdds=fc->mdds_dest;
   funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_REOPEN);
  }
 }

 mpxplay_timer_addfunc(diskfile_filecopymove_postprocess,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

static void diskfile_filecopy_keycheck(struct filecopy_t *fc,unsigned int extkey)
{
 if(extkey==KEY_ESC)
  fc->retcode=MPXPLAY_ERROR_FILEHAND_USERABORT;
 if((extkey==KEY_ENTER1) || (extkey==KEY_ENTER2))
  funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_DONTWAITHPP);
}

static void diskfile_filecopymove_progress_window(struct filecopy_t *fc,unsigned int waitflag)
{
 long percent_file,percent_group;
 char *hedtext,barstr_file[DISKFILE_FILECOPY_BARLEN+1],barstr_group[DISKFILE_FILECOPY_BARLEN+1];
 char msg[2*MAX_PATHNAMELEN+48+DISKFILE_FILECOPY_BARLEN*2],tottmp[64+DISKFILE_FILECOPY_BARLEN];

 if(!(fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_DEL|DISKFILE_CPYTYPE_RENAME|DISKFILE_CPYTYPE_MOVE))){
  fc->retcode=MPXPLAY_ERROR_FILEHAND_USERABORT; // ???
  return;
 }
 if(waitflag){
  if(!fc->statwin_begintime_waitdisp){
   fc->statwin_begintime_waitdisp=pds_gettimem()+DISKFILE_DEFAULT_TIMEOUTMS_WAITDISP;
   waitflag=0;
  }else if(pds_gettimem()<fc->statwin_begintime_waitdisp)
   waitflag=0;
 }

 if(fc->cpy_type&DISKFILE_CPYTYPE_DEL)
  hedtext=" Delete ";
 else if(fc->cpy_type&DISKFILE_CPYTYPE_RENAME)
  hedtext=" Rename ";
 else if(fc->cpy_type&DISKFILE_CPYTYPE_MOVE)
  hedtext=" Move file ";
 else
  hedtext=" File copy ";

 if(waitflag)
  pds_strcpy(msg,"Waiting for the higher priority processes...\n(press ESC to exit, ENTER to skip)");
 else{
  if(fc->cpy_type&(DISKFILE_CPYTYPE_DEL|DISKFILE_CPYTYPE_RENAME|DISKFILE_CPYTYPE_MOVE)){
   if(fc->counted_filenum_all>1)
    percent_group=(long)(100.0*((float)fc->filenum_curr)/(float)fc->counted_filenum_all);
   if(fc->cpy_type&DISKFILE_CPYTYPE_DEL)
    snprintf(msg,sizeof(msg),"Deleting\n%s",fc->infilename);
   else if(fc->cpy_type&DISKFILE_CPYTYPE_RENAME)
    snprintf(msg,sizeof(msg),"Renaming\n%s\nto\n%s",fc->infilename,fc->outfilename);
   else if(fc->cpy_type&DISKFILE_CPYTYPE_MOVE)
    snprintf(msg,sizeof(msg),"Moving\n%s\nto\n%s",fc->infilename,fc->outfilename);
  }else{
   if(fc->filelen<1)
    percent_file=0;
   else if(fc->filelen<10000)
    percent_file=100*fc->filepos/fc->filelen;
   else
    percent_file=fc->filepos/(fc->filelen/100);
   diskfile_filecopy_makebar(barstr_file,percent_file);
   if(fc->counted_filenum_all>1)
    percent_group=(long)(100.0*((float)fc->filebytes_copied+(float)fc->filepos)/(float)fc->counted_filesizes_all);
    snprintf(msg,sizeof(msg),"Copying the file\n%s\nto\n%s\n%s",fc->infilename,fc->outfilename,barstr_file);
  }
  if(fc->directories_selected || (fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE)){
   if(fc->counted_filenum_all>1){
    diskfile_filecopy_makebar(barstr_group,percent_group);
    sprintf(tottmp,"\nTotal %d/%d\n%s",fc->filenum_curr,fc->counted_filenum_all,barstr_group);
   }else
    sprintf(tottmp,"\nTotal: %d",fc->filenum_curr);
  }
  pds_strcat(msg,tottmp);
 }
 fc->statwin_tw=display_textwin_openwindow_buttons(fc->statwin_tw,TEXTWIN_FLAG_MSGCENTERALIGN,hedtext,msg,diskfile_filecopy_keycheck,fc,NULL);
}

static void diskfile_filecopy_do_copy(struct filecopy_t *fc)
{
 long inbytes,outbytes,lps_wait=0;

 if(!funcbit_test(fc->cpy_ctrl,DISKFILE_CPYCTRL_DONTWAITHPP))
  lps_wait=mpxplay_timer_lowpriorstart_wait();

 diskfile_filecopymove_progress_window(fc,lps_wait);
 if(fc->retcode==MPXPLAY_ERROR_FILEHAND_USERABORT)
  goto end_copy;
 if(lps_wait)
  return;

 inbytes=mpxplay_diskdrive_file_read(fc->filehand_data_src,fc->buffer,DISKFILE_FILECOPY_BLOCKSIZE);
 if(inbytes<=0){
  if(fc->filepos<fc->filelen)
   fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTREAD;
  goto end_copy;
 }
 outbytes=mpxplay_diskdrive_file_write(fc->filehand_data_dest,fc->buffer,inbytes);
 if(outbytes<inbytes){
  fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTWRITE;
  goto end_copy;
 }
 fc->filepos+=outbytes;

 return; // continue copy

end_copy:
 mpxplay_timer_deletefunc(diskfile_filecopy_do_copy,fc);
 mpxplay_timer_addfunc(diskfile_filecopy_closefiles,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

static void diskfile_filerename_do(struct filecopy_t *fc)
{
 struct frame *frp;
 diskfile_filecopymove_progress_window(fc,0);
 if(fc->retcode==MPXPLAY_ERROR_FILEHAND_USERABORT)
  return;
 frp=fc->mvp->frp0;
 if(frp->filehand_datas){
  if(pds_strnicmp(fc->mvp->pei0->filename,fc->infilename,pds_strlen(fc->infilename))==0){ // currently playing file or it's updir is renamed
   mpxplay_diskdrive_file_close(frp->filehand_datas);
   frp->filehand_datas=NULL;
   funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_REOPEN);
  }
 }
 fc->retcode=mpxplay_diskdrive_rename(fc->mdds_src,fc->infilename,fc->outfilename);
}

static unsigned int diskfile_filemove_check_samedrive(struct filecopy_t *fc)
{
 int drive_d,drive_s;

 drive_s=pds_getdrivenum_from_path(fc->infilename);
 drive_d=pds_getdrivenum_from_path(fc->outfilename);

 if(drive_d==drive_s)
  return 1;

 return 0;
}

static void diskfile_filemove_do(struct filecopy_t *fc)
{
 struct frame *frp;
 diskfile_filecopymove_progress_window(fc,0);
 if(fc->retcode==MPXPLAY_ERROR_FILEHAND_USERABORT)
  return;
 if(fc->cpy_ctrl&DISKFILE_CPYCTRL_OVERWRITE)              // ???
  mpxplay_diskdrive_unlink(fc->mdds_src,fc->outfilename); // !!!
 frp=fc->mvp->frp0;
 if(frp->filehand_datas && pds_stricmp(fc->mvp->pei0->filename,fc->infilename)==0){ // currently playing file is moved
  mpxplay_diskdrive_file_close(frp->filehand_datas);
  frp->filehand_datas=NULL;
  funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_REOPEN);
 }
 fc->retcode=mpxplay_diskdrive_rename(fc->mdds_src,fc->infilename,fc->outfilename);
}

static void diskfile_filecopymove_postprocess(struct filecopy_t *fc)
{
 struct frame *frp=fc->mvp->frp0;
 char *reopenfilename,strtmp[MAX_PATHNAMELEN];

 if(funcbit_test(fc->cpy_ctrl,DISKFILE_CPYCTRL_REOPEN)){ // currently playing file is moved/renamed
  if(fc->retcode==MPXPLAY_ERROR_FILEHAND_OK){ // re-open (out/new)file at new location
   struct playlist_side_info *psi=fc->psi_src;
   if(fc->pei_curr && (fc->pei_curr->entrytype==DFT_SUBDIR)){ // parent/updir of currently playing file is renamed
    unsigned int pos=pds_strcpy(strtmp,fc->outfilename);
    pds_strcpy(&strtmp[pos],&fc->mvp->pei0->filename[pds_strlen(fc->infilename)]);
    reopenfilename=&strtmp[0];
   }else
    reopenfilename=&fc->outfilename[0];
   if(playlist_editlist_addfile_one(NULL,psi,fc->mvp->pei0,NULL,EDITLIST_MODE_ID3|EDITLIST_MODE_INDEX)){
    psi->lastentry->filename=reopenfilename;
    playlist_pei0_set(fc->mvp,psi->lastentry,0);
    playlist_editlist_delfile_one(psi,psi->lastentry,(EDITLIST_MODE_FILENAME|EDITLIST_MODE_ID3|EDITLIST_MODE_ENTRY));
   }
  }else{ // re-open input file at move fault
   reopenfilename=fc->mvp->pei0->filename;
  }
  frp->filehand_datas=mpxplay_diskdrive_file_open(frp->mdds,reopenfilename,(O_RDONLY|O_BINARY));
  mpxplay_diskdrive_file_seek(frp->filehand_datas,frp->filepos,SEEK_SET);
  funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_REOPEN);
 }

 //if((fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_COPY)) && (fc->retcode==MPXPLAY_ERROR_FILEHAND_OK) && !mpxinbuf_lowlevel_file_isopen(frp) && (pds_stricmp(fc->mvp->pei0->filename,fc->outfilename)==0)){ // re-open currently played file (delete, after copy/create again)
 // if(mpxinbuf_lowlevel_file_open_read(frp,fc->outfilename))
 //  mpxinbuf_lowlevel_file_seek(frp,frp->filepos);
 //}

 if(fc->pei_curr && (fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME))){
  // delete, update or create new playlist entry in directory browser after copy/move/rename
  if((fc->retcode==MPXPLAY_ERROR_FILEHAND_OK) || (fc->retcode==MPXPLAY_ERROR_FILEHAND_CANTREAD) || (fc->retcode==MPXPLAY_ERROR_FILEHAND_CANTWRITE) || (fc->retcode==MPXPLAY_ERROR_FILEHAND_USERABORT) || (fc->retcode==MPXPLAY_ERROR_FILEHAND_DELETE)){
   pds_getpath_from_fullname(fc->outpath_curr,fc->outfilename);
   diskfile_filecopymove_update_sides(fc,fc->infilename,fc->outfilename,fc->pei_curr);
  }
 }

 if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_ALLFILE))
  funcbit_disable(fc->cpy_ctrl,(DISKFILE_CPYCTRL_CREATENOEXT|DISKFILE_CPYCTRL_OVERWRITE|DISKFILE_CPYCTRL_SKIPFILE));

 if(fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK){
  if(fc->retcode==MPXPLAY_ERROR_FILEHAND_USERABORT) // aborted by user
   goto err_out_finish;
  pds_strcpy(fc->lastfilename,fc->outfilename);
 }

 fc->outfilename[0]=0;

 if(fc->pei_curr){ // if not a subdir-filename
  // clear SELECTED flag and skip entry after copy/rename
  if(((fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_RENAME)) && (fc->retcode==MPXPLAY_ERROR_FILEHAND_OK)) || (fc->retcode==MPXPLAY_ERROR_FILEHAND_DELETE) || (fc->retcode==MPXPLAY_ERROR_FILEHAND_SKIPFILE)){
   if(fc->pei_curr->infobits&PEIF_SELECTED){
    funcbit_disable(fc->pei_curr->infobits,PEIF_SELECTED);
    if(fc->psi_src->selected_files)
     fc->psi_src->selected_files--;
    refdisp|=RDT_EDITOR;
   }
  }
  if((fc->cpy_type&(DISKFILE_CPYTYPE_COPY|DISKFILE_CPYTYPE_RENAME)) || (fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK))
   fc->pei_curr++; // not incremented if move was successfull (because then the entry was deleted above)
 }

 if(fc->retcode==MPXPLAY_ERROR_FILEHAND_SKIPFILE) // not a real error, just sign
  fc->retcode=MPXPLAY_ERROR_FILEHAND_OK;          // clear it

 if((fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_IGNALLERR)){
  fc->last_error=fc->retcode;
  playlist_diskfile_show_errmsg(fc);
  return;
 }

 mpxplay_timer_addfunc(diskfile_filecopymove_loop,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
 return;

err_out_finish:
 mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

static void diskfile_filecopymove_select(struct filecopy_t *fc)
{
 fc->filenum_curr++;
 if(fc->retcode==MPXPLAY_ERROR_FILEHAND_OK){
  if(fc->cpy_type&DISKFILE_CPYTYPE_RENAME){ // remame
   diskfile_filerename_do(fc);
  }else if((fc->cpy_type&DISKFILE_CPYTYPE_MOVE) && diskfile_filemove_check_samedrive(fc)) // move
   diskfile_filemove_do(fc);
  else{ // copy
   mpxplay_timer_addfunc(diskfile_filecopy_openfiles,fc,MPXPLAY_TIMERFLAG_INDOS,0);
   return;
  }
 }
 mpxplay_timer_addfunc(diskfile_filecopymove_postprocess,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

static void diskfile_filecopymove_loop(struct filecopy_t *fc)
{
 int retcode,lps_wait=0;

 if(!funcbit_test(fc->cpy_ctrl,(DISKFILE_CPYCTRL_DONTWAITHPP|DISKFILE_CPYCTRL_SKIPWAITHPP)))
  lps_wait=mpxplay_timer_lowpriorstart_wait();

 if(lps_wait){
  diskfile_filecopymove_progress_window(fc,lps_wait);
  if(fc->retcode==MPXPLAY_ERROR_FILEHAND_USERABORT){
   mpxplay_timer_modifyfunc(&diskfile_filecopymove_loop,0,0);
   mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
  }
  return;
 }

 if(!funcbit_test(fc->cpy_ctrl,DISKFILE_CPYCTRL_SKIPWAITHPP))
  fc->statwin_begintime_waitdisp=0;
 funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SKIPWAITHPP);

 fc->retcode=MPXPLAY_ERROR_FILEHAND_OK;

 retcode=diskfile_filecopymove_getnextfile(fc);

 if(retcode>0){  // no file
  if(retcode==1){ // no file, get next (loop in timer)
   if((fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_IGNALLERR))
    goto err_out_loop;
   else
    mpxplay_timer_modifyfunc(diskfile_filecopymove_loop,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
  }else{ //do nothing (warning message follows)
   mpxplay_timer_modifyfunc(&diskfile_filecopymove_loop,0,0);
  }
 }else{
  mpxplay_timer_modifyfunc(&diskfile_filecopymove_loop,0,0);
  if(retcode==0){ // no error, do copy/move/del
   if(fc->cpy_type&DISKFILE_CPYTYPE_DEL)
    playlist_diskfile_delete_do(fc);
   else
    diskfile_filecopymove_checkfilenames(fc);
  }else{           // finish/abort copy
   if((fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_IGNALLERR)){
    funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_MULTIFILE); // hack for [Ok] button
    goto err_out_loop;
   }else
    mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
  }
 }
 return;

err_out_loop:
 mpxplay_timer_modifyfunc(&diskfile_filecopymove_loop,0,0);
 fc->last_error=fc->retcode;
 playlist_diskfile_show_errmsg(fc);
}

static display_textwin_button_t buttons_copymove_confirm[]={
 {""          ,KEY_ENTER1},// gray enter
 {""          ,KEY_ENTER2},// white enter
 {"[ Cancel ]",0x2e63},    // 'c'
 {""          ,0x2e43},    // 'C'
 {""          ,0x6c00},    // alt-F5
 {""          ,KEY_ESC},   // ESC
 {NULL,0}
};

static display_textwin_button_t buttons_copymove_copywithsubdir_confirm[]={
 {"[ single Files ]",KEY_ENTER1},// gray enter
 {""          ,KEY_ENTER2},// white enter
 {""          ,0x2166},    // 'f'
 {""          ,0x2146},    // 'F'
 {"[ with Subdir ]",0x1f73}, // 's'
 {""          ,0x1f53},    // 'S'
 {"[ Cancel ]",0x2e63},    // 'c'
 {""          ,0x2e43},    // 'C'
 {""          ,0x6c00},    // alt-F5
 {""          ,KEY_ESC},   // ESC
 {NULL,0}
};

static void diskfile_copymove_start(struct filecopy_t *fc,unsigned int extkey)
{
 unsigned int len;
 char destfn[MAX_PATHNAMELEN],path[MAX_PATHNAMELEN],newname[MAX_PATHNAMELEN];

 switch(extkey){
  case KEY_ENTER1:
  case KEY_ENTER2:
  case 0x2166:  // 'f'
  case 0x2146:funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_WITHSUBDIR);
  case 0x1f73:  // 's'
  case 0x1f53:  // 'S'
   diskfile_count_files_stop(fc);
   if(((fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) || fc->directories_selected) && (!fc->source_filtermask[0] || !pds_strlenc(fc->source_filtermask,' '))){ // empty source_filtermask, do nothing
    pds_strcpy(source_default_filter_copy,PDS_DIRECTORY_ALLFILE_STR); //
    break;
   }
   pds_sfn_limit(fc->outpath_argument);

   if(fc->cpy_type&DISKFILE_CPYTYPE_RENAME){
    funcbit_disable(fc->cpy_type,DISKFILE_CPYTYPE_MOVE);
    goto copymove_start_skip1;
   }

   pds_strcpy(destfn,pds_getpath_from_fullname(fc->outpath_curr,fc->outpath_argument));

   if(pds_filename_wildchar_chk(destfn)){
    if((fc->directories_selected>1) || (fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE)){ // multiply dir/file copy/move/rename
     if(fc->outpath_curr[0])
      pds_filename_build_fullpath(fc->outpath_root,fc->path_src,fc->outpath_curr);
     else{
      pds_strcpy(fc->outpath_root,fc->path_src);
      if(fc->cpy_type&DISKFILE_CPYTYPE_MOVE){ // multiply rename
       funcbit_disable(fc->cpy_type,DISKFILE_CPYTYPE_MOVE);
       funcbit_enable(fc->cpy_type,DISKFILE_CPYTYPE_RENAME);
      }
     }
     goto copymove_start_skip1;
    }else{ // copy one directory or file to a new one (given by a wildchar string)
     char *srcfn=pds_getfilename_from_fullname(fc->selected_filename);
     pds_filename_wildchar_rename(newname,srcfn,destfn);
     if(fc->outpath_curr[0])
      pds_filename_build_fullpath(path,fc->path_src,fc->outpath_curr);
     else
      pds_getpath_from_fullname(path,fc->selected_filename);
     len=pds_strlen(path);
     if(path[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR)
      len+=pds_strcpy(&path[len],PDS_DIRECTORY_SEPARATOR_STR);
     pds_strcpy(&path[len],newname);
     funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SUBONECOPY);
    }
   }else
    pds_filename_build_fullpath(path,fc->path_src,fc->outpath_argument);

   fc->mdds_dest=playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(path));

   if(pds_path_is_dir(path) && mpxplay_diskdrive_checkdir(fc->mdds_dest,path)){ // argument is an existent dir
    if((fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME)) && !(fc->outpath_curr[0]) && (fc->directories_selected==1) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) && (pds_stricmp(fc->selected_filename,path)==0)){ // uppercase/lowercase rename dir
     pds_getpath_from_fullname(fc->outpath_root,fc->selected_filename);
     funcbit_disable(fc->cpy_type,DISKFILE_CPYTYPE_MOVE);
     funcbit_enable(fc->cpy_type,DISKFILE_CPYTYPE_RENAME);
    }else
     diskfile_filecopymove_rebuild_outpathargument(fc,path);
   }else{
    if((fc->directories_selected>1) || (fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE)){
     fc->retcode=MPXPLAY_ERROR_FILEHAND_MULTITO1;
     goto err_out_start;
    }
    if((fc->cpy_type&(DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME)) && !(fc->outpath_curr[0])){ // rename one dir or file
     pds_getpath_from_fullname(fc->outpath_root,fc->selected_filename);
     funcbit_disable(fc->cpy_type,DISKFILE_CPYTYPE_MOVE);
     funcbit_enable(fc->cpy_type,DISKFILE_CPYTYPE_RENAME);
    }else{
     if(fc->directories_selected){ // copy/move content of a subdir to another (create outdir, don't copy highest subdirname)
      if(diskfile_filecopymove_check_samerootdir(fc->selected_filename,path)){ // copy/move a directory to itself?
       fc->retcode=MPXPLAY_ERROR_FILEHAND_SAMEDIR;
       pds_strcpy(fc->lastfilename,path);
       goto err_out_start;
      }
      pds_getpath_from_fullname(fc->outpath_curr,path);
      diskfile_filecopymove_rebuild_outpathargument(fc,path);
      if(diskfile_filecopymove_make_subdir(fc,path)<0)
       goto err_out_start;
      funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SUBONECOPY);
     }else{ // single file
      pds_filename_build_fullpath(fc->outpath_root,fc->path_src,fc->outpath_curr);
      pds_filename_assemble_fullname(fc->outpath_argument,fc->outpath_root,destfn);
      funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SUBONECOPY);
     }
    }
   }

copymove_start_skip1:

   if(!funcbit_test(fc->cpy_type,DISKFILE_CPYTYPE_RENAME) && !fc->mdds_dest){
    fc->retcode=MPXPLAY_ERROR_FILEHAND_CANTCOPY;
    pds_strcpy(fc->lastfilename,fc->outpath_argument);
    goto err_out_start;
   }

   fc->outpath_curr[0]=0;

   if(fc->directories_selected || (fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE)){
    pds_strcpy(source_default_filter_copy,fc->source_filtermask);
    if(funcbit_test(desktopmode,DTM_EDIT_ALLFILES) || pds_strricmp(fc->source_filtermask,".*")!=0)
     funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_NOEXTCHK);
   }
   fc->statwin_begintime_waitdisp=pds_gettimem();
   mpxplay_timer_addfunc(diskfile_filecopymove_loop,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
   return; // success, continue copy/move
 }
 mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
 return;

err_out_start:
 fc->last_error=fc->retcode;
 funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_MULTIFILE); // hack for [Ok] button
 playlist_diskfile_show_errmsg(fc);
}

void playlist_diskfile_copy_or_move(struct mainvars *mvp,unsigned int move)
{
 struct filecopy_t *fc;
 struct playlist_side_info *psi=mvp->psie,*psio=psi->psio;
 struct playlist_entry_info *pei=psi->editorhighline;
 unsigned int pathlen;
 void *tw;
 const static char *functext[2]={"Copy","Move or rename"};
 char msg[MAX_PATHNAMELEN+64];

 if(move>1)
  return;
 if(!(displaymode&DISP_FULLSCREEN))
  return;

 fc=diskfile_filecopy_alloc(mvp);
 if(!fc)
  return;

 if(move)
  funcbit_enable(fc->cpy_type,DISKFILE_CPYTYPE_MOVE);
 else
  funcbit_enable(fc->cpy_type,DISKFILE_CPYTYPE_COPY);

 funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_EMPTYDIRCOPY); // !!!

 fc->selected_filename=pei->filename;
 if(psi->selected_files){
  funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_MULTIFILE);
  fc->pei_selected=psi->firstentry;
  fc->pei_last=psi->lastentry;
 }else{
  if((pei->entrytype==DFT_DRIVE) || ((pei->entrytype==DFT_UPDIR) && !(fc->cpy_type&DISKFILE_CPYTYPE_COPY)) || (pei->entrytype==DFT_UPLIST)){
   fc->last_error=MPXPLAY_ERROR_FILEHAND_COPYDIR;
   playlist_diskfile_show_errmsg(fc);
   return;
  }
  fc->pei_selected=fc->pei_last=pei;
  if(pei->entrytype==DFT_UPDIR)
   fc->selected_filename=&psi->currdir[0];
 }

 if((psi->editsidetype&PLT_DIRECTORY) && !psi->sublistlevel)
  pds_strcpy(fc->path_src,psi->currdir);
 else if(fc->cpy_type&DISKFILE_CPYTYPE_COPY)
  funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_WITHSUBDIR);

 if((psio->editsidetype&PLT_DIRECTORY) && (psio->sublistlevel<=1)){
  if(!psio->sublistlevel)
   fc->psi_dest=psio;
  pds_strcpy(fc->path_dest,psio->currdir);
 }else
  pds_strcpy(fc->path_dest,fc->path_src);
 if(!fc->psi_dest)
  fc->psi_dest=psi;

 pds_strcpy(fc->outpath_root,fc->path_dest);
 fc->mdds_dest=fc->psi_dest->mdds;

 if(fc->path_dest[0] && (!(psi->editsidetype&PLT_DIRECTORY) || psi->sublistlevel || (!psi->selected_files && (pei->entrytype==DFT_UPDIR)) || (pds_strcmp(fc->path_dest,fc->path_src)!=0))){
  pathlen=pds_strcpy(fc->outpath_argument,fc->path_dest);
  if(fc->outpath_argument[pathlen-1]!=PDS_DIRECTORY_SEPARATOR_CHAR)
   pds_strcpy(&fc->outpath_argument[pathlen++],PDS_DIRECTORY_SEPARATOR_STR);
 }else       // assumed rename or copy to the same dir
  pathlen=0; // show only filename

 pds_strcpy(fc->source_filtermask,source_default_filter_copy);

 diskfile_count_files_start(fc);

 if(psi->selected_files){
  sprintf(fc->headtext," %s files ",functext[move]);
  sprintf(msg," %s %d file%s to",functext[move],psi->selected_files,((psi->selected_files>1)? "s":""));
 }else{
  sprintf(fc->headtext," %s file ",functext[move]);
  sprintf(msg," %s \"%s\" to",functext[move],pds_getfilename_from_fullname(fc->selected_filename));
 }
 if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_WITHSUBDIR)){
  sprintf(fc->buttontext,"[ %s ]",functext[move]);
  buttons_copymove_confirm[0].text=&fc->buttontext[0];
  buttons_copymove_confirm[4].extkey=(move)? 0x6d00:0x6c00; // !!! alt-F6/F5 hardwired
 }

 fc->count_tw=tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_MSGCENTERALIGN,fc->headtext,diskfile_copymove_start,fc);
 fc->count_itemnum=display_textwin_additem_msg_alloc(tw,0,0,0,msg);

 if((fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) || fc->directories_selected){
  if(pathlen || psi->selected_files)
   pds_strcpy(&fc->outpath_argument[pathlen],PDS_DIRECTORY_ALLFILE_STR);
  else // assumed rename/simple copy
   pds_strcpy(&fc->outpath_argument[pathlen],pds_getfilename_from_fullname(fc->selected_filename));
  display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,1,50,fc->outpath_argument,MAX_PATHNAMELEN-2);
  display_textwin_additem_separatorline(tw,2);
  display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,3,"Source filter: ");
  display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,sizeof("Source filter: ")-1,3,8,fc->source_filtermask,sizeof(fc->source_filtermask)-1);
  //display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,4,"(\"*.*\" : supported files; \"*.?*\" : all files)");
  if(fc->directories_selected && (fc->cpy_type&DISKFILE_CPYTYPE_MOVE)) // ???
   funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_NOEXTCHK);
 }else{
  pds_strcpy(&fc->outpath_argument[pathlen],pds_getfilename_from_fullname(fc->selected_filename));
  display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,1,50,fc->outpath_argument,MAX_PATHNAMELEN-2);
 }

 display_textwin_additem_separatorline(tw,-1);
 if(fc->cpy_ctrl&DISKFILE_CPYCTRL_WITHSUBDIR){
  display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,&buttons_copymove_copywithsubdir_confirm[0],NULL);
 }else
  display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,&buttons_copymove_confirm[0],NULL);
 display_textwin_openwindow_items(tw,0,0,0);
}

//-------------------------------------------------------------------------
// delete files

static display_textwin_button_t buttons_error_delcurrplay[]={
 {" Delete "   ,0x2064}, // 'd'
 {""           ,0x2044}, // 'D'
 {" Skip "     ,0x1f73}, // 's'
 {""           ,0x1f53}, // 'S'
 {" Cancel "   ,0x2e63}, // 'c'
 {""           ,0x2e43}, // 'C'
 {""           ,KEY_ESC},// ESC
 {NULL,0}
};

static void diskfile_filedel_delcurrplay_keyhand(struct filecopy_t *fc,unsigned int extkey)
{
 switch(extkey){
  case 0x2064:
  case 0x2044:funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_DELFILE);
              playlist_diskfile_delete_do(fc);
              break;
  case 0x1f73:
  case 0x1f53:if(fc->pei_curr)
               fc->pei_curr++;
              mpxplay_timer_addfunc(diskfile_filecopymove_loop,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
              break;
  case 0x2e63:
  case 0x2e43:
  case KEY_ESC:fc->retcode=MPXPLAY_ERROR_FILEHAND_USERABORT;
               diskfile_filecopy_dealloc(fc);
 }
}

static void playlist_diskfile_delete_do(struct filecopy_t *fc)
{
 void *tw;
 struct frame *frp=fc->mvp->frp0;

 if(pds_stricmp(fc->mvp->pei0->filename,fc->infilename)==0){
  if(fc->cpy_ctrl&DISKFILE_CPYCTRL_DELFILE){
   mpxplay_diskdrive_file_close(frp->filehand_datas); // ???
   frp->filehand_datas=NULL;
  }else
   goto err_out_delcurrplay;
 }
 if(!(fc->cpy_ctrl&DISKFILE_CPYCTRL_ALLFILE))
  funcbit_disable(fc->cpy_ctrl,DISKFILE_CPYCTRL_DELFILE);

 fc->filenum_curr++;

 diskfile_filecopymove_progress_window(fc,0);
 if(fc->retcode==MPXPLAY_ERROR_FILEHAND_USERABORT)
  goto err_out_finish;

 fc->retcode=mpxplay_diskdrive_unlink(fc->mdds_src,fc->infilename);

 if(fc->retcode==MPXPLAY_ERROR_FILEHAND_OK){
  diskfile_filecopymove_update_sides(fc,fc->infilename,NULL,NULL);
 }else{
  pds_strcpy(fc->lastfilename,fc->infilename);
  if((pds_stricmp(fc->mvp->pei0->filename,fc->infilename)==0) && !frp->filehand_datas){ // re-open currently played file (if delete was not successull)
   frp->filehand_datas=mpxplay_diskdrive_file_open(frp->mdds,fc->infilename,(O_RDONLY|O_BINARY));
   mpxplay_diskdrive_file_seek(frp->filehand_datas,frp->filepos,SEEK_SET);
  }
  if(fc->pei_curr) // if not a subdir-filename
   fc->pei_curr++;
 }

 if((fc->retcode!=MPXPLAY_ERROR_FILEHAND_OK) && !(fc->cpy_ctrl&DISKFILE_CPYCTRL_IGNALLERR)){
  fc->last_error=fc->retcode;
  playlist_diskfile_show_errmsg(fc);
  return;
 }

 mpxplay_timer_addfunc(diskfile_filecopymove_loop,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
 return;

err_out_delcurrplay:
 diskfile_statwin_close(fc);
 tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_ERRORMSG|TEXTWIN_FLAG_MSGCENTERALIGN,fc->headtext,diskfile_filedel_delcurrplay_keyhand,fc);
 display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,"Program is currently playing this file");
 display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,fc->infilename);
 display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,"Do you wish to delete it?");
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,buttons_error_delcurrplay,NULL);
 display_textwin_openwindow_items(tw,0,0,0);
 return;

err_out_finish:
 mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

static void diskfile_delete_start(struct filecopy_t *fc,unsigned int extkey)
{
 switch(extkey){
  case KEY_ENTER1:
  case KEY_ENTER2:
   diskfile_count_files_stop(fc);
   if(((fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE) || fc->directories_selected) && (!fc->source_filtermask[0] || !pds_strlenc(fc->source_filtermask,' '))){ // empty source_filtermask, do nothing
    pds_strcpy(source_default_filter_del,"*.?*"); // restore default
    break;
   }
   if(fc->directories_selected || (fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE)){
    pds_strcpy(source_default_filter_del,fc->source_filtermask);
    if(funcbit_test(desktopmode,DTM_EDIT_ALLFILES) || pds_strricmp(fc->source_filtermask,".*")!=0)
     funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_NOEXTCHK);
   }
   fc->statwin_begintime_waitdisp=pds_gettimem();
   funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_SKIPWAITHPP);
   mpxplay_timer_addfunc(diskfile_filecopymove_loop,fc,MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_INDOS,0);
   return; // success, continue delete
 }
 mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

void playlist_diskfile_delete_init(struct mainvars *mvp)
{
 struct filecopy_t *fc;
 struct playlist_side_info *psi=mvp->psie;
 struct playlist_entry_info *pei=psi->editorhighline;
 unsigned int y;
 void *tw;
 char msg[MAX_PATHNAMELEN+64];

 if(!(displaymode&DISP_FULLSCREEN))
  return;

 fc=diskfile_filecopy_alloc(mvp);
 if(!fc)
  return;

 funcbit_enable(fc->cpy_type,DISKFILE_CPYTYPE_DEL);
 funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_EMPTYDIRCOPY);

 if(psi->selected_files){
  funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_MULTIFILE);
  fc->pei_selected=psi->firstentry;
  fc->pei_last=psi->lastentry;
 }else{
  if((pei->entrytype==DFT_DRIVE) || (pei->entrytype==DFT_UPDIR) || (pei->entrytype==DFT_UPLIST)){
   fc->last_error=MPXPLAY_ERROR_FILEHAND_COPYDIR;
   playlist_diskfile_show_errmsg(fc);
   return;
  }
  fc->pei_selected=fc->pei_last=pei;
 }

 pds_strcpy(fc->source_filtermask,source_default_filter_del);

 diskfile_count_files_start(fc);

 if(psi->selected_files || !(fc->count_ctrl&DISKFILE_COUNTCTRL_COMPLETE))
  pds_strcpy(fc->headtext," Delete files ");
 else
  pds_strcpy(fc->headtext," Delete file ");

 sprintf(fc->buttontext,"[%s]",fc->headtext);
 buttons_copymove_confirm[0].text=&fc->buttontext[0];
 buttons_copymove_confirm[4].extkey=0x6f00; // !!! alt-F8 hardwired

 fc->count_tw=tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_MSGCENTERALIGN,fc->headtext,diskfile_delete_start,fc);

 y=0;
 if(psi->selected_files || !(fc->count_ctrl&DISKFILE_COUNTCTRL_COMPLETE)){
  if(fc->directories_selected){
   unsigned long files_selected=(psi->selected_files)? (psi->selected_files-fc->directories_selected):0;
   sprintf(msg,"You have selected %d file%s and %d director%s",files_selected,((files_selected>1)? "s":""),fc->directories_selected,((fc->directories_selected>1)? "ies":"y"));
   display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y++,msg);
   sprintf(msg,"(total %d file%s and %d subdir%s)",fc->counted_filenum_all,((fc->counted_filenum_all>1)? "s":""),fc->counted_directories,((fc->counted_directories>1)? "s":""));
   fc->count_itemnum=display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y++,msg);
  }else{
   sprintf(msg,"You have selected %d file%s",fc->counted_filenum_all,((fc->counted_filenum_all>1)? "s":""));
   display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y++,msg);
  }
  display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y++,"Do you wish to delete them?");
  display_textwin_additem_separatorline(tw,y++);
 }else{
  snprintf(msg,sizeof(msg),"Delete \"%s\" %s from disk?\nAre you sure?",pds_getfilename_from_fullname(pei->filename),((pei->entrytype==DFT_SUBDIR)? "directory":"file"));
  display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y++,msg);
 }
 if(psi->selected_files || fc->directories_selected){
  display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,y,"Filter: ");
  display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,sizeof("Filter: ")-1,y++,8,fc->source_filtermask,sizeof(fc->source_filtermask)-1);
  //display_textwin_additem_msg_static(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,5,"(\"*.*\" : supported files; \"*.?*\" : all files)");
 }

 display_textwin_additem_separatorline(tw,-1);
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,&buttons_copymove_confirm[0],NULL);
 display_textwin_openwindow_items(tw,0,0,0);
}

//-------------------------------------------------------------------------
// collect/show multiply file infos from subdirs too

static display_textwin_button_t buttons_multifileinfos[]={
 {"[ Restart ]",0x1372},     // 'r'
 {""           ,0x1352},     // 'R'
 {"[ Stop ]"   ,0x1f73},     // 's'
 {""           ,0x1f53},     // 'S'
 {"[ Close ]"  ,0x2e63},     // 'c'
 {""           ,0x2e43},     // 'C'
 {""           ,0x3d00},     // F3
 {""           ,KEY_ESC},    // ESC
 {NULL,0}
};

static void diskfile_multifileinfos_start(struct filecopy_t *fc)
{
 fc->cpy_type=DISKFILE_CPYTYPE_INFO_SIZE;
 fc->cpy_ctrl=DISKFILE_CPYCTRL_EMPTYDIRCOPY;

 if(fc->psi_src->selected_files){
  funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_MULTIFILE);
  fc->pei_selected=fc->psi_src->firstentry;
  fc->pei_last=fc->psi_src->lastentry;
 }else
  fc->pei_selected=fc->pei_last=fc->psi_src->editorhighline;

 diskfile_count_files_start(fc);
}

static void diskfile_multifileinfos_stop(struct filecopy_t *fc)
{
 diskfile_count_files_stop(fc);
}

static void diskfile_multifileinfos_close(struct filecopy_t *fc)
{
 //if(!fc->psi_src->selected_files && (fc->pei_selected->entrytype==DFT_SUBDIR) && funcbit_test(fc->count_ctrl,DISKFILE_COUNTCTRL_COMPLETE))
 // fc->pei_selected->filesize=(mpxp_filesize_t)fc->counted_filesizes_all;
 pds_strcpy(source_default_filter_info,fc->source_filtermask);
 diskfile_count_files_stop(fc);
 mpxplay_timer_addfunc(diskfile_filecopy_dealloc,fc,MPXPLAY_TIMERFLAG_INDOS,0);
}

static void diskfile_multifileinfos_buttonhandler(struct filecopy_t *fc,unsigned int extkey)
{
 switch(extkey){
  case KEY_ENTER1:
  case KEY_ENTER2:
  case 0x1372:
  case 0x1352:diskfile_multifileinfos_start(fc);break;
  case 0x1f73:
  case 0x1f53:diskfile_multifileinfos_stop(fc);break;
  case 0x2e63:
  case 0x2e43:
  case 0x3d00:
  case KEY_ESC:diskfile_multifileinfos_close(fc);return;
 }
}

static void diskfile_show_multifileinfos_window(struct filecopy_t *fc)
{
 char sout[6*80];
 snprintf(sout,sizeof(sout),"                        filesizes      time\n Directories:%7d\n Audio files:%7d %9.1f Mb %4d:%2.2d:%2.2d \n List files :%7d %9.1f Mb\n Other files:%7d %9.1f Mb\n All files  :%7d %9.1f Mb",
  fc->counted_directories,
  fc->counted_filenum_media,(float)fc->counted_filesizes_media/1048576.0,
   (long)((fc->counted_timemsec_media+500)/3600000),((long)((fc->counted_timemsec_media+500)/60000))%60,((long)((fc->counted_timemsec_media+500)/1000))%60,
  fc->counted_filenum_list,(float)fc->counted_filesizes_list/1048576.0,
  fc->counted_filenum_other,
   ((float)(fc->counted_filesizes_all-fc->counted_filesizes_media-fc->counted_filesizes_list)/1048576.0),
  fc->counted_filenum_all,(float)fc->counted_filesizes_all/1048576.0);

 if(funcbit_test(fc->count_ctrl,DISKFILE_COUNTCTRL_COMPLETE))
  pds_strcpy(fc->headtext," Counted files ");
 else if(funcbit_test(fc->cpy_type,DISKFILE_CPYTYPE_INFO_SIZE))
  snprintf(fc->headtext,sizeof(fc->headtext)," Counting files (%d) ",fc->counted_filenum_media+fc->counted_filenum_list+fc->counted_filenum_other);
 else
  snprintf(fc->headtext,sizeof(fc->headtext)," Counting times (%d/%d) ",fc->counted_filenum_mtime,fc->counted_filenum_media);

 if(fc->statwin_tw){
  display_textwin_draw_window_headtext(fc->statwin_tw,fc->headtext);
  display_textwin_update_msg(fc->statwin_tw,fc->count_itemnum,sout);
 }else{
  fc->statwin_tw=display_textwin_allocwindow_items(fc->statwin_tw,TEXTWIN_FLAG_MSGCENTERALIGN|TEXTWIN_FLAG_DONTCLOSE,fc->headtext,diskfile_multifileinfos_buttonhandler,fc);
  fc->count_itemnum=display_textwin_additem_msg_alloc(fc->statwin_tw,TEXTWIN_FLAG_MSGLEFTALIGN,0,-1,sout);
  display_textwin_additem_separatorline(fc->statwin_tw,6);
  display_textwin_additem_msg_static(fc->statwin_tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,7,"Source filter: ");
  display_textwin_additem_editline(fc->statwin_tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,sizeof("Source filter: ")-1,7,8,fc->source_filtermask,sizeof(fc->source_filtermask)-1);
  display_textwin_additem_separatorline(fc->statwin_tw,8);
  display_textwin_additem_buttons(fc->statwin_tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,9,buttons_multifileinfos,NULL);
  display_textwin_openwindow_items(fc->statwin_tw,0,0,0);
 }
}

void playlist_diskfile_show_multifileinfos(struct mainvars *mvp)
{
 struct filecopy_t *fc;

 if(!(displaymode&DISP_FULLSCREEN))
  return;

 fc=diskfile_filecopy_alloc(mvp);
 if(!fc)
  return;

 if(!mpxplay_infile_frame_alloc(&fc->frp_src)){
  diskfile_filecopy_dealloc(fc);
  return;
 }

 if(!source_default_filter_info[0] || !pds_strlenc(source_default_filter_info,' '))
  pds_strcpy(source_default_filter_info,PDS_DIRECTORY_ALLFILE_STR);

 pds_strcpy(fc->source_filtermask,source_default_filter_info);

 diskfile_multifileinfos_start(fc);

 diskfile_show_multifileinfos_window(fc);
}

//------------------------------------------------------------------------
static unsigned int diskfile_renamebyid3_createoutfilename(struct filecopy_t *fc,struct playlist_entry_info *pei,char *outbuf,unsigned int bufsize)
{
 struct playlist_side_info *psi=fc->psi_src;
 unsigned int tracknum=0,digits=2;
 char *ext,artist[128],title[128];

 if((pei<psi->firstentry) || (pei>psi->lastentry))
  return 0;
 if(GET_HFT(pei->entrytype)==HFT_DFT)
  return 0;
 if(pei->infobits&PEIF_INDEXED)
  return 0;

 if((pei->entrytype==DFT_NOTCHECKED) || ((loadid3tag&ID3LOADMODE_ALL)!=ID3LOADMODE_ALL)){
  playlist_chkentry_get_onefileinfos_allagain(psi,pei,&fc->frp_src,(ID3LOADMODE_ALL|(loadid3tag&ID3LOADMODE_PREFER_LIST)));
  mpxplay_infile_close(&fc->frp_src);
 }

 if(!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE]){
  pds_strncpy(outbuf,pds_getfilename_from_fullname(pei->filename),bufsize);
  outbuf[bufsize-1]=0;
  return 1;
 }

 if(pei->id3info[I3I_TRACKNUM])
  tracknum=pds_atol(pei->id3info[I3I_TRACKNUM]);
 if((psi->editsidetype&PLT_DIRECTORY) && !psi->sublistlevel){
  unsigned int i=psi->lastentry-psi->firstsong+1;
  if(i>=100)
   digits=3;
  else if(i>=1000)
   digits=4;
  else if(i>=10000)
   digits=5;
  else if(i>=100000)
   digits=6;
  if(!tracknum)
   tracknum=pei-psi->firstsong+1;
 }

 ext=pds_strrchr(pei->filename,'.');

 if(pei->id3info[I3I_ARTIST]){
  pds_strncpy(artist,pei->id3info[I3I_ARTIST],127);
  artist[127]=0;
  pds_filename_conv_forbidden_chars(artist);
  pds_strcutspc(artist);
  mpxplay_playlist_textconv_back(artist,artist);
 }else
  artist[0]=0;
 if(pei->id3info[I3I_TITLE]){
  pds_strncpy(title,pei->id3info[I3I_TITLE],127);
  title[127]=0;
  pds_filename_conv_forbidden_chars(title);
  pds_strcutspc(title);
  mpxplay_playlist_textconv_back(title,title);
 }else
  title[0]=0;

 if(tracknum){     // !!!
  switch(digits){  // optimize size
   case 1:         //
   case 2:snprintf(outbuf,bufsize,"%2.2d. %.60s%s%.60s%s",tracknum,&artist[0],
           ((artist[0] && title[0])? " - ":""),&title[0],((ext)? ext:""));break;
   case 3:snprintf(outbuf,bufsize,"%3.3d. %.60s%s%.60s%s",tracknum,&artist[0],
           ((artist[0] && title[0])? " - ":""),&title[0],((ext)? ext:""));break;
   case 4:snprintf(outbuf,bufsize,"%4.4d. %.60s%s%.60s%s",tracknum,&artist[0],
           ((artist[0] && title[0])? " - ":""),&title[0],((ext)? ext:""));break;
   case 5:snprintf(outbuf,bufsize,"%5.5d. %.60s%s%.60s%s",tracknum,&artist[0],
           ((artist[0] && title[0])? " - ":""),&title[0],((ext)? ext:""));break;
   default:snprintf(outbuf,bufsize,"%6.6d. %.60s%s%.60s%s",tracknum,&artist[0],
           ((artist[0] && title[0])? " - ":""),&title[0],((ext)? ext:""));break;
  }
 }else{
  snprintf(outbuf,bufsize,"%.60s%s%.60s%s",&artist[0],
   ((artist[0] && title[0])? " - ":""),&title[0],((ext)? ext:""));
 }

 return 1;
}

void playlist_diskfile_rename_by_id3(struct mainvars *mvp)
{
 struct filecopy_t *fc;
 struct playlist_side_info *psi=mvp->psie;
 struct playlist_entry_info *pei=psi->editorhighline;
 void *tw;
 char msg[MAX_PATHNAMELEN+64];

 if(!(displaymode&DISP_FULLSCREEN))
  return;

#ifdef __DOS__
 if(!is_lfn_support || !(uselfn&USELFN_ENABLED)){
  display_textwin_openwindow_errormsg_ok("Rename by ID3","Long filenames (LFN) support is missing or disabled!\n(this function cannot work without it)");
  return;
 }
#endif

 fc=diskfile_filecopy_alloc(mvp);
 if(!fc)
  return;

 fc->cpy_type=DISKFILE_CPYTYPE_MOVE|DISKFILE_CPYTYPE_RENAME; // move flag needed for correct start/init

 fc->selected_filename=pei->filename;
 if(psi->selected_files){
  funcbit_enable(fc->cpy_ctrl,DISKFILE_CPYCTRL_MULTIFILE);
  fc->pei_selected=psi->firstentry;
  fc->pei_last=psi->lastentry;
 }else{
  if(GET_HFT(pei->entrytype)==HFT_DFT){
   fc->last_error=MPXPLAY_ERROR_FILEHAND_CANTPERFORM;
   pds_strcpy(fc->infilename,pei->filename);
   playlist_diskfile_show_errmsg(fc);
   return;
  }
  fc->pei_selected=fc->pei_last=pei;
 }

 if(!mpxplay_infile_frame_alloc(&fc->frp_src)){
  fc->last_error=MPXPLAY_ERROR_FILEHAND_MEMORY;
  playlist_diskfile_show_errmsg(fc);
  return;
 }

 pds_strcpy(fc->source_filtermask,PDS_DIRECTORY_ALLFILE_STR);

 if((psi->editsidetype&PLT_DIRECTORY) && !psi->sublistlevel)
  pds_strcpy(fc->path_src,psi->currdir);
 pds_strcpy(fc->path_dest,fc->path_src);
 pds_strcpy(fc->outpath_root,fc->path_dest);
 fc->psi_dest=psi;
 fc->mdds_dest=psi->mdds;

 diskfile_count_files_start(fc);

 pds_strcpy(fc->headtext," Rename by ID3 ");

 if(psi->selected_files)
  sprintf(msg," Rename the selected %d file%s \n to \"NN. Artist - Title\" format by ID3-info %s",psi->selected_files,((psi->selected_files>1)? "s":""),((fc->path_src[0])? "in ":""));
 else
  sprintf(msg," Rename \"%s\" to",pds_getfilename_from_fullname(fc->pei_selected->filename));
 pds_strcpy(fc->buttontext,"[ !!! Rename by ID3 !!! ]");
 buttons_copymove_confirm[0].text=&fc->buttontext[0];
 buttons_copymove_confirm[4].extkey=0x6300; // !!! ctrl-F6 hardwired

 tw=display_textwin_allocwindow_items(NULL,TEXTWIN_FLAG_MSGCENTERALIGN,fc->headtext,diskfile_copymove_start,fc);
 display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,msg);

 if(fc->cpy_ctrl&DISKFILE_CPYCTRL_MULTIFILE){
  funcbit_enable(fc->cpy_type,DISKFILE_CPYTYPE_RENBYID3);
  pds_strcpy(fc->outpath_argument,PDS_DIRECTORY_ALLFILE_STR);
  if(fc->path_src[0])
   display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,fc->path_src);
  else
   display_textwin_additem_msg_alloc(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,"\n Warning! Using this function in playlist, \n if the tracknumber is missing or unknown \n the program will not write the leading NN. ! ");
 }else{
  diskfile_renamebyid3_createoutfilename(fc,pei,&fc->outpath_argument[0],sizeof(fc->outpath_argument));
  display_textwin_additem_editline(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,0,1,50,fc->outpath_argument,MAX_PATHNAMELEN-2);
 }

 display_textwin_additem_separatorline(tw,-1);
 display_textwin_additem_buttons(tw,TEXTWIN_FLAG_MSGCENTERALIGN,0,-1,&buttons_copymove_confirm[0],NULL);
 display_textwin_openwindow_items(tw,0,0,0);
}
