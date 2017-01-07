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
//function: normal/general drive/file handling (floppy,hd)

#include "in_file.h"

#define DRVHD_MAX_LOCAL_DRIVES   ('Z'-'A'+1)

typedef struct hddrive_info_s{
 unsigned int drivenum;
}hddrive_info_s;

static unsigned int hddrive_drive_check(char *pathname)
{
 int drivenum=pds_getdrivenum_from_path(pathname);
 if((drivenum<0) || (drivenum>=DRVHD_MAX_LOCAL_DRIVES))
  return 0;
 return 1;
}

/*static long hddrive_drive_config(void *drivehand_data,unsigned long funcnum,void *argp1,void *argp2)
{
 struct cddrive_info_s *hdis=drivehand_data;

 if(!hdis)
  return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;

 switch(funcnum){
  case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIREXISTS:
   if(pds_access((char *)argp1,F_OK)==0)
    return 1;
   return 0;
 }
 return MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC;
}*/

static void *hddrive_drive_mount(char *pathname)
{
 struct hddrive_info_s *hdis;
 int drivenum=pds_getdrivenum_from_path(pathname);
 if((drivenum<0) || (drivenum>=DRVHD_MAX_LOCAL_DRIVES))
  return NULL;

 hdis=(struct hddrive_info_s *)calloc(1,sizeof(*hdis));
 if(!hdis)
  return hdis;

 hdis->drivenum=drivenum;

 return hdis;
}

static void hddrive_drive_unmount(void *drivehand_data)
{
 struct hddrive_info_s *hdis=drivehand_data;
 if(hdis){
  free(hdis);
 }
}

static unsigned int hddrive_findfirst(void *drivehand_data,char *pathname,unsigned int attrib,struct pds_find_t *ffblk)
{
 return pds_findfirst(pathname,attrib,ffblk);
}

static unsigned int hddrive_findnext(void *drivehand_data,struct pds_find_t *ffblk)
{
 return pds_findnext(ffblk);
}

static void hddrive_findclose(void *drivehand_data,struct pds_find_t *ffblk)
{
 pds_findclose(ffblk);
}

static char *hddrive_getcwd(void *drivehand_data,char *buf,unsigned int buflen)
{
 struct hddrive_info_s *hdis=drivehand_data;
 pds_getdcwd(hdis->drivenum+1,buf);
 return buf;
}

static int hddrive_chdir(void *drivehand_data,char *path)
{
 return pds_chdir(path);
}

static int hddrive_mkdir(void *drivehand_data,char *path)
{
 return pds_mkdir(path);
}

static int hddrive_rmdir(void *drivehand_data,char *path)
{
 return pds_rmdir(path);
}

static int hddrive_rename(void *drivehand_data,char *oldfilename,char *newfilename)
{
 return pds_rename(oldfilename,newfilename);
}

static int hddrive_unlink(void *drivehand_data,char *filename)
{
 return pds_unlink(filename);
}

//---------------------------------------------------------------------

typedef struct hdfile_info_s{
 int filehand;
}hdfile_info_s;

static void hddrive_file_close(void *filehand_data);

static unsigned int hddrive_file_check(void *drivehand_data,char *filename)
{
 struct hddrive_info_s *hdis=drivehand_data;
 int drivenum;
 if(!hdis)
  return 1; // !!!
 drivenum=pds_getdrivenum_from_path(filename);
 if((drivenum<0) || (drivenum>=DRVHD_MAX_LOCAL_DRIVES))
  return 1; // !!!
 if(drivenum==hdis->drivenum)
  return 1;
 return 0;
}

static void *hddrive_file_open(void *drivehand_data,char *filename,unsigned long openmode)
{
 struct hdfile_info_s *hdfi=calloc(1,sizeof(*hdfi));
 if(!hdfi)
  return hdfi;

 switch(openmode&(O_RDONLY|O_RDWR|O_WRONLY)){
  case O_RDONLY:hdfi->filehand=pds_open_read(filename,openmode);break;
  case O_RDWR:
  case O_WRONLY:if(openmode&O_CREAT)
                 hdfi->filehand=pds_open_create(filename,openmode);
                else
                 hdfi->filehand=pds_open_write(filename,openmode);
                break;
 }
 if(hdfi->filehand<=0)
  goto err_out_openr;

 return hdfi;

err_out_openr:
 hddrive_file_close(hdfi);
 return NULL;
}

static void hddrive_file_close(void *filehand_data)
{
 struct hdfile_info_s *hdfi=filehand_data;
 if(hdfi){
  pds_close(hdfi->filehand);
  free(hdfi);
 }
}

static long hddrive_file_read(void *filehand_data,char *ptr,unsigned int num)
{
 struct hdfile_info_s *hdfi=filehand_data;
 return pds_dos_read(hdfi->filehand,ptr,num);
}

static long hddrive_file_write(void *filehand_data,char *ptr,unsigned int num)
{
 struct hdfile_info_s *hdfi=filehand_data;
 return pds_dos_write(hdfi->filehand,ptr,num);
}

static mpxp_filesize_t hddrive_file_tell(void *filehand_data)
{
 struct hdfile_info_s *hdfi=filehand_data;
 return pds_tell(hdfi->filehand);
}

static mpxp_filesize_t hddrive_file_seek(void *filehand_data,mpxp_filesize_t pos,int fromwhere)
{
 struct hdfile_info_s *hdfi=filehand_data;
 return pds_lseek(hdfi->filehand,pos,fromwhere);
}

static mpxp_filesize_t hddrive_file_length(void *filehand_data)
{
 struct hdfile_info_s *hdfi=filehand_data;
 return pds_filelength(hdfi->filehand);
}

static int hddrive_file_eof(void *filehand_data)
{
 struct hdfile_info_s *hdfi=filehand_data;
 return pds_eof(hdfi->filehand);
}

static int hddrive_file_chsize(void *filehand_data,mpxp_filesize_t offset)
{
 struct hdfile_info_s *hdfi=filehand_data;
 return pds_chsize(hdfi->filehand,offset);
}

//------------------------------------------------------------------------

struct mpxplay_drivehand_func_s HDDRIVE_drivehand_funcs={
 "HDDRIVE",
 0,    // infobits
 NULL, //&hddrive_drive_config,
 &hddrive_drive_check,
 &hddrive_drive_mount,
 &hddrive_drive_unmount,
 &hddrive_findfirst,
 &hddrive_findnext,
 &hddrive_findclose,
 &hddrive_getcwd,
 &hddrive_chdir,
 &hddrive_mkdir,
 &hddrive_rmdir,
 &hddrive_rename,
 &hddrive_unlink,
 NULL, // r15
 NULL, // r16
 NULL, // r17
 NULL, // r18
 NULL, // r19
 NULL, // r20

 &hddrive_file_check,
 &hddrive_file_open,
 &hddrive_file_close,
 &hddrive_file_read,
 &hddrive_file_write,
 &hddrive_file_seek,
 &hddrive_file_tell,
 &hddrive_file_length,
 &hddrive_file_eof,
 &hddrive_file_chsize,
 NULL // r31
};
