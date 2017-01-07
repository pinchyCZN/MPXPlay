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
//function: drive & directory handling

#include <direct.h>
#include <dos.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include "newfunc.h"

#ifdef MPXPLAY_WIN32
#ifndef GetLongPathName
DWORD WINAPI GetLongPathNameA(LPCSTR,LPSTR,DWORD);
DWORD WINAPI GetLongPathNameW(LPCWSTR,LPWSTR,DWORD);
#ifdef UNICODE
#define GetLongPathName  GetLongPathNameW
#else
#define GetLongPathName  GetLongPathNameA
#endif // !UNICODE
#endif // GetLongPathName
#endif // MPXPLAY_WIN32

static void pds_truename_lfn(char *longname,char *name);

extern volatile unsigned long mpxplay_signal_events;

unsigned int iswin9x,is_lfn_support,uselfn;
#ifdef __DOS__
dosmem_t dm_int2x_1,dm_int2x_2;
#endif

//-------------------------------------------------------------------------
//DOS memory allocations (conventional memory for DOS callings)

#ifdef __DOS__
unsigned int pds_int2x_dosmems_allocate(void)
{
 if(!dm_int2x_1.linearptr)
  if(!pds_dpmi_dos_allocmem(&dm_int2x_1,PDS_INT2X_DOSMEM_SIZE))
   return 0;
 if(!dm_int2x_2.linearptr)
  if(!pds_dpmi_dos_allocmem(&dm_int2x_2,PDS_INT2X_DOSMEM_SIZE))
   return 0;
 return 1;
}

void pds_int2x_dosmems_free(void)
{
 pds_dpmi_dos_freemem(&dm_int2x_1);
 pds_dpmi_dos_freemem(&dm_int2x_2);
}
#endif

//-------------------------------------------------------------------------
/*void pds_dir_save(unsigned int *savedrive,char *savedir)
{
 if(savedir)
  pds_getcwd(savedir);
 if(savedrive)
  *savedrive=pds_getdrive();
}

void pds_dir_restore(unsigned int savedrive,char *savedir)
{
 pds_setdrive(savedrive);
 pds_chdir(savedir);
}*/

//-------------------------------------------------------------------------

void pds_check_lfnapi(char *prgname)
{
#ifdef __DOS__
 struct rminfo RMI;
#endif
 if(pds_mswin_getver()==1024)
  iswin9x=1;
#ifdef MPXPLAY_WIN32
 is_lfn_support=1;
#elif defined(__DOS__)
 if(iswin9x){
  is_lfn_support=1;
 }else{    // detect truename (LFN converter) function
  if(prgname){
   pds_dpmi_rmi_clear(&RMI);
   RMI.EAX=0x00007160;
   RMI.ECX=2;
   RMI.DS =dm_int2x_1.segment;
   RMI.ES =dm_int2x_2.segment;
   funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
   pds_strcpy(dm_int2x_1.linearptr,prgname);
   *((char *)(dm_int2x_2.linearptr))=0;
   pds_dpmi_realmodeint_call(0x21,&RMI);
   if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY) && *((char *)(dm_int2x_2.linearptr))!=0)
    is_lfn_support=1;
  }
 }
#endif
}

void pds_fullpath(char *fullname,char *name)
{
 if(uselfn&USELFN_ENABLED)
  pds_truename_lfn(fullname,name);
 else
  pds_truename_dos(fullname,name);
}

#define LOADDIR_MAX_LOCAL_DRIVES   ('Z'-'A'+1)

void pds_getdcwd(int drive,char *path)  // 0:default drive, A=1,B=2,C=3
{
 if(drive>=LOADDIR_MAX_LOCAL_DRIVES)
  return;
 path[0]=0;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED)){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x00007147;
  RMI.EDX=drive&0x3f; // 0:default drive, A=1,B=2,C=3
  RMI.DS =dm_int2x_1.segment;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  *((char *)(dm_int2x_1.linearptr))=0;
  pds_dpmi_realmodeint_call(0x21,&RMI);
  *((char *)(dm_int2x_1.linearptr+290))=0;
  //fprintf(stdout,"%c: %s\n",(drive+'A'),dm_int2x_1.linearptr);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)){
   if(dm_int2x_1.linearptr[1]!=':'){
    unsigned int drivenum;
    if(drive<=0){
     pds_filehand_unlock_entrance(); // !!!
     drivenum=pds_getdrive();
     pds_filehand_lock_entrance();   //
     drive=drivenum+1;
    }
    path[0]='A'+drive-1;//num;
    path[1]=':';
    path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
    pds_strcpy(&path[3],dm_int2x_1.linearptr);
   }else
    pds_strcpy(path,dm_int2x_1.linearptr);
   //fprintf(stdout,"path: %c: %s\n",('A'+drive-1),&path[-3]);
  }
 }
#endif
 if(!path[0])
  _getdcwd(drive,path,300); // 0:default drive, A=1,B=2,C=3
 /*if((!path[0]) && (drive>0)){ // ??? (if getdcwd fails)
  unsigned int olddrive=pds_getdrive();
  pds_setdrive(drive-1);
  if(pds_getdrive()==(drive-1)){
   getcwd(path,300);
   pds_setdrive(olddrive);
  }
 }*/
 if((!path[0]) && (drive>0)){ // !!! creating a fake (root) path
  path[0]='A'+drive-1;
  path[1]=':';
  path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
  path[3]=0;
 }
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
}

void pds_getcwd(char *path)
{
#ifdef MPXPLAY_WIN32
 GetCurrentDirectory(MAX_PATHNAMELEN,path);
#else
 pds_getdcwd(0,path);
#endif
}

int pds_chdir(char *setpath)
{
 int result=-1;
 if(!setpath || !setpath[0])
  return result;
#ifdef MPXPLAY_WIN32
 if(SetCurrentDirectory(setpath))
  result=0;
#else
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED)){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x0000713B;
  RMI.DS =dm_int2x_1.segment;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_strcpy(dm_int2x_1.linearptr,setpath);
  pds_dpmi_realmodeint_call(0x21,&RMI);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))
   result=0;
 }else
#endif
  result=chdir(setpath);
#endif
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
 return result;
}

int pds_mkdir(char *newdirname)
{
 int result=-1;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
#ifdef __DOS__
 if(is_lfn_support && (uselfn&USELFN_ENABLED)){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x00007139;
  RMI.DS =dm_int2x_1.segment;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_strcpy(dm_int2x_1.linearptr,newdirname);
  pds_dpmi_realmodeint_call(0x21,&RMI);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))
   result=0;
 }else
#endif
  result=mkdir(newdirname);
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
 return result;
}

int pds_rmdir(char *dirname)
{
 int result=-1;
 pds_fileattrib_reset(dirname,_A_RDONLY); // sometimes can fail?
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
#ifdef __DOS__
 if(is_lfn_support && (uselfn&USELFN_ENABLED)){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x0000713A;
  RMI.DS =dm_int2x_1.segment;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_strcpy(dm_int2x_1.linearptr,dirname);
  pds_dpmi_realmodeint_call(0x21,&RMI);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))
   result=0;
 }else
#endif
  result=rmdir(dirname);
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
 return result;
}

#ifdef MPXPLAY_WIN32
 #ifndef MOVEFILE_CREATE_HARDLINK
  #define MOVEFILE_CREATE_HARDLINK 0x00000010
 #endif
#endif

int pds_rename(char *oldfilename,char *newfilename)
{
 int result=MPXPLAY_ERROR_FILEHAND_RENAME;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
#ifdef MPXPLAY_WIN32
 if(MoveFileEx(oldfilename,newfilename,0))//MOVEFILE_CREATE_HARDLINK))
  result=MPXPLAY_ERROR_FILEHAND_OK;
 else // -> rename()
#elif definded(__DOS__)
 if(is_lfn_support && (uselfn&USELFN_ENABLED)){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x00007156;
  RMI.DS =dm_int2x_1.segment;
  RMI.ES =dm_int2x_2.segment;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_strcpy(dm_int2x_1.linearptr,oldfilename);
  pds_strcpy(dm_int2x_2.linearptr,newfilename);
  pds_dpmi_realmodeint_call(0x21,&RMI);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))
   result=MPXPLAY_ERROR_FILEHAND_OK;
 }else
#endif
  if(rename(oldfilename,newfilename)==0)
   result=MPXPLAY_ERROR_FILEHAND_OK;
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
 return result;
}

int pds_unlink(char *filename)
{
 int result=MPXPLAY_ERROR_FILEHAND_DELETE;

 if(pds_fileattrib_reset(filename,_A_RDONLY)!=MPXPLAY_ERROR_FILEHAND_OK) // !!!
  return result;

#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);

#ifdef __DOS__
 if(is_lfn_support && (uselfn&USELFN_ENABLED)){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x00007141;
  RMI.DS =dm_int2x_1.segment;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_strcpy(dm_int2x_1.linearptr,filename);
  pds_dpmi_realmodeint_call(0x21,&RMI);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))
   result=MPXPLAY_ERROR_FILEHAND_OK;
 }else
#endif
 {
  if(unlink(filename)==0)
   result=MPXPLAY_ERROR_FILEHAND_OK;
 }

#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
 return result;
}

/*int pds_fileattrib_get(char *filename)
{
 unsigned int attrib;
#ifdef __DOS__
 char shortname[300];
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  pds_truename_dos(shortname,filename);
 else
  pds_strcpy(shortname,filename);
#else
 char *shortname=filename;
#endif

 if(_dos_getfileattr(shortname,&attrib)!=0)
  return MPXPLAY_ERROR_FILEHAND_CHANGEATTR;

 return attrib;
}*/

int pds_fileattrib_reset(char *filename,unsigned int clearflags)
{
 unsigned int attrib;
 int result=MPXPLAY_ERROR_FILEHAND_CHANGEATTR;
#ifdef __DOS__
 char shortname[300];
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  pds_truename_dos(shortname,filename);
 else
  pds_strcpy(shortname,filename);
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
#else
 char *shortname=filename;
#endif

 if(_dos_getfileattr(shortname,&attrib)!=0)
  goto fileattr_end;
 //if(attrib&(_A_HIDDEN|_A_SYSTEM|_A_VOLID|_A_SUBDIR)) // !!! change normal files only
 if(attrib&(_A_HIDDEN|_A_SYSTEM|_A_VOLID)) // !!! change file and dir only
  goto fileattr_end;
 funcbit_disable(attrib,clearflags);
 if(_dos_setfileattr(shortname,attrib)!=0)
  goto fileattr_end;

 result=MPXPLAY_ERROR_FILEHAND_OK;

fileattr_end:
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
 return result;
}

//-------------------------------------------------------------------------
#ifdef __DOS__

#define PDS_DTA_SIZE 128 // dos-disk transfer area

typedef struct pds_dos_find_t{
 char *dta_addr;
 char dosdata[43];
}pds_dos_find_t;

static void pds_dta_get(unsigned int *dta_seg,unsigned int *dta_off)
{
 struct rminfo RMI;
 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x2f00;
 pds_dpmi_realmodeint_call(0x21,&RMI);
 *dta_seg=RMI.ES;
 *dta_off=RMI.EBX;
}

static void pds_dta_to_ffblk(struct pds_find_t *ffblk)
{
 struct pds_dos_find_t *ffdos=(struct pds_dos_find_t *)ffblk->ff_data;
 char *dta_addr;

 if(!ffdos)
  return;
 dta_addr=ffdos->dta_addr;
 ffblk->attrib =*((char *)(dta_addr+21+0));
 ffblk->size   =*((unsigned long  *)(dta_addr+21+5));
 pds_memcpy((void *)&ffblk->fdate,(void *)(dta_addr+21+1),sizeof(struct pds_fdate_t));
 pds_strncpy(ffblk->name,(unsigned char *)(dta_addr+21+9),12);
 ffblk->name[12]=0;

 pds_memcpy(ffdos->dosdata,dta_addr,sizeof(ffdos->dosdata));
}

static unsigned int pds_dos_findfirst(char *path,int attrib,struct pds_find_t *ffblk)
{
 struct pds_dos_find_t *ffdos;
 struct rminfo RMI;
 unsigned int dta_seg,dta_off,result=1;

 ffdos=calloc(1,sizeof(struct pds_dos_find_t));
 if(!ffdos)
  return result;
 ffblk->ff_data=(void *)ffdos;

 pds_dta_get(&dta_seg,&dta_off);
 ffdos->dta_addr=(char *)((dta_seg<<4)+dta_off);
 if(!ffdos->dta_addr)
  goto err_out_ff;

 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x4e00;
 RMI.ECX=attrib;
 RMI.DS =dm_int2x_1.segment;
 pds_strcpy(dm_int2x_1.linearptr,path);
 funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
 pds_dpmi_realmodeint_call(0x21,&RMI);
 if(funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)){
  result=RMI.EAX;
  goto err_out_ff;
 }

 pds_dta_to_ffblk(ffblk);

 return 0;

err_out_ff:
 pds_dos_findclose(ffblk);
 return result;
}

static unsigned int pds_dos_findnext(struct pds_find_t *ffblk)
{
 struct pds_dos_find_t *ffdos=(struct pds_dos_find_t *)ffblk->ff_data;
 struct rminfo RMI;
 unsigned int result=1;

 if(!ffdos)
  goto end_ff;

 pds_memcpy(ffdos->dta_addr,ffdos->dosdata,sizeof(ffdos->dosdata));

 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x4f00;
 funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
 pds_dpmi_realmodeint_call(0x21,&RMI);
 if(funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)){
  result=RMI.EAX;
  goto end_ff;
 }
 pds_dta_to_ffblk(ffblk);
 result=0;

end_ff:
 return result;
}

static void pds_dos_findclose(struct pds_find_t *ffblk)
{
 struct pds_dos_find_t *ffdos=(struct pds_dos_find_t *)ffblk->ff_data;
 if(ffdos){
  free(ffdos);
  ffblk->ff_data=NULL;
 }
}

#endif

//-------------------------------------------------------------------------
#ifdef __DOS__
struct lfn_find_t
{
 unsigned char ff_attrib[4];  /* Attributes, DOS and extended   */
 unsigned long ff_lctime;     /* Creation time (low)   DOS date */
 unsigned long ff_hctime;     /* Creation time (high)           */
 unsigned long ff_latime;     /* Access time (low)    Date only */
 unsigned long ff_hatime;     /* Access time (high)             */
 unsigned long ff_lmtime;     /* Modification time (low)        */
 unsigned long ff_hmtime;     /* Modification time (high)       */
 unsigned long ff_hsize;      /* File size (high end)           */
 unsigned long ff_lsize;      /* File size (low end)            */
 unsigned char ff_reserved[8];
 unsigned char ff_longname[260];
 unsigned char ff_shortname[14];
};

static unsigned int pds_lfn_findfirst(char *path,int attrib,struct pds_find_t *ffblk)
{
 struct rminfo RMI;
 struct lfn_find_t *flblk;
 unsigned int retry=2; // possible DOSLFN bug (this is not 100% solution)

 do{
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x0000714E;
  RMI.ECX=attrib;
  RMI.ESI=0x0001;
  RMI.DS =dm_int2x_1.segment;
  RMI.ES =dm_int2x_2.segment;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_strcpy(dm_int2x_1.linearptr,path);
  pds_memset(dm_int2x_2.linearptr,0,sizeof(struct lfn_find_t));
  pds_dpmi_realmodeint_call(0x21,&RMI);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)){
   flblk=(struct lfn_find_t *)dm_int2x_2.linearptr;
   if(flblk->ff_longname[0])
    break;
  }
 }while(--retry);
 if(!retry) //error
  return 1;
 ffblk->attrib =flblk->ff_attrib[0];
 pds_memcpy((void *)(&ffblk->cdate),(void *)(&flblk->ff_lctime),sizeof(struct pds_fdate_t));
 pds_memcpy((void *)(&ffblk->fdate),(void *)(&flblk->ff_lmtime),sizeof(struct pds_fdate_t));
 pds_memcpy((void *)(&ffblk->adate),(void *)(&flblk->ff_latime),sizeof(struct pds_fdate_t));
 ffblk->size=flblk->ff_lsize; // | ((unsigned long long)flblk->ff_hsize<<32);
 pds_strcpy(ffblk->name,flblk->ff_longname);
 ffblk->ff_data=(void *)RMI.EAX;
 return 0;
}

/*static unsigned int pds_lfn_findfirst(char *path,int attrib,struct pds_find_t *ffblk)
{
 struct rminfo RMI;
 struct lfn_find_t *flblk;

 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x0000714E;
 RMI.ECX=attrib;
 RMI.ESI=0x0001;
 RMI.DS =dm_int2x_1.segment;
 RMI.ES =dm_int2x_2.segment;
 funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
 pds_strcpy(dm_int2x_1.linearptr,path);
 pds_memset(dm_int2x_2.linearptr,0,sizeof(struct lfn_find_t));
 pds_dpmi_realmodeint_call(0x21,&RMI);
 if(funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))
  return 1;
 flblk=(struct lfn_find_t *)dm_int2x_2.linearptr;
 if(!flblk->ff_longname[0])
  return 1;
 ffblk->attrib=flblk->ff_attrib[0];
 pds_memcpy((void *)(&ffblk->cdate),(void *)(&flblk->ff_lctime),sizeof(struct pds_fdate_t));
 pds_memcpy((void *)(&ffblk->fdate),(void *)(&flblk->ff_lmtime),sizeof(struct pds_fdate_t));
 pds_memcpy((void *)(&ffblk->adate),(void *)(&flblk->ff_latime),sizeof(struct pds_fdate_t));
 ffblk->size=flblk->ff_lsize; // | ((unsigned long long)flblk->ff_hsize<<32);
 pds_strcpy(ffblk->name,flblk->ff_longname);
 ffblk->ff_data=(void *)RMI.EAX;
 return 0;
}*/

static unsigned int pds_lfn_findnext(struct pds_find_t *ffblk)
{
 struct rminfo RMI;
 struct lfn_find_t *flblk;

 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x0000714F;
 RMI.EBX=(unsigned long)ffblk->ff_data;
 RMI.ESI=0x0001;
 RMI.ES =dm_int2x_2.segment;
 funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
 pds_memset(dm_int2x_2.linearptr,0,sizeof(struct lfn_find_t));
 pds_dpmi_realmodeint_call(0x21,&RMI);
 if(funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))
  return 1;
 flblk=(struct lfn_find_t *)dm_int2x_2.linearptr;
 ffblk->attrib=flblk->ff_attrib[0];
 pds_memcpy((void *)(&ffblk->cdate),(void *)(&flblk->ff_lctime),sizeof(struct pds_fdate_t));
 pds_memcpy((void *)(&ffblk->fdate),(void *)(&flblk->ff_lmtime),sizeof(struct pds_fdate_t));
 pds_memcpy((void *)(&ffblk->adate),(void *)(&flblk->ff_latime),sizeof(struct pds_fdate_t));
 ffblk->size=flblk->ff_lsize; // | ((unsigned long long)flblk->ff_hsize<<32);
 pds_strcpy(ffblk->name,flblk->ff_longname);
 return 0;
}

static void pds_lfn_findclose(struct pds_find_t *ffblk)
{
 if(ffblk->ff_data){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x000071A1;
  RMI.EBX=(unsigned long)ffblk->ff_data;
  pds_dpmi_realmodeint_call(0x21,&RMI);
 }
}

#endif

unsigned int pds_findfirst(char *path,int attrib,struct pds_find_t *ffblk)
{
 unsigned int result=1;
 ffblk->ff_data=NULL;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  result=pds_lfn_findfirst(path,attrib,ffblk);
 else
  result=pds_dos_findfirst(path,attrib,ffblk);
 pds_filehand_unlock_entrance();
#else
 {
  struct find_t *fsblk=pds_calloc(1,sizeof(struct find_t));
  if(!fsblk){
   result=5; // ENOMEM
   goto ff_end;
  }
  result=_dos_findfirst(path,attrib,fsblk);
  if(result==0){
   ffblk->attrib =fsblk->attrib;
   pds_memcpy((void *)(&ffblk->fdate),(void *)(&fsblk->wr_time),sizeof(struct pds_fdate_t));
   pds_memset((void *)(&ffblk->cdate),0,sizeof(struct pds_fdate_t));
   pds_memset((void *)(&ffblk->adate),0,sizeof(struct pds_fdate_t));
   ffblk->size   =fsblk->size;
   pds_strcpy(ffblk->name,fsblk->name);
  }else{
   pds_free(fsblk);
   fsblk=NULL;
  }
  ffblk->ff_data=fsblk;
 }
ff_end:
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
 return result;
}

unsigned int pds_findnext(struct pds_find_t *ffblk)
{
 unsigned int result=1;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  result=pds_lfn_findnext(ffblk);
 else
  result=pds_dos_findnext(ffblk);
 pds_filehand_unlock_entrance();
#else
 struct find_t *fsblk=(struct find_t *)ffblk->ff_data;
 result=_dos_findnext(fsblk);
 if(result==0){
  ffblk->attrib =fsblk->attrib;
  pds_memcpy((void *)(&ffblk->fdate),(void *)(&fsblk->wr_time),sizeof(struct pds_fdate_t));
  pds_memset((void *)(&ffblk->cdate),0,sizeof(struct pds_fdate_t));
  pds_memset((void *)(&ffblk->adate),0,sizeof(struct pds_fdate_t));
  ffblk->size   =fsblk->size;
  pds_strcpy(ffblk->name,fsblk->name);
 }
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
 return result;
}

void pds_findclose(struct pds_find_t *ffblk)
{
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  pds_lfn_findclose(ffblk);
 else
  pds_dos_findclose(ffblk);
 pds_filehand_unlock_entrance();
#else
  if(ffblk->ff_data){
   #ifdef __WATCOMC__
   _dos_findclose((struct find_t *)ffblk->ff_data);
   #endif
   pds_free(ffblk->ff_data);
  }
#endif
 ffblk->ff_data=NULL;
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
}

/*
#ifdef MPXPLAY_WIN32
static void pds_filetime_to_dosdatetime(void *target,time_t src)
{
 FILETIME ft;
 struct cv_s{
  WORD time;
  WORD date;
 }cvs;
 ft.dwLowDateTime=src;
 ft.dwHighDateTime=0;
 cvs.time=cvs.date=0;

 FileTimeToDosDateTime(&ft,&cvs.date,&cvs.time);

 pds_memcpy(target,(void *)&cvs,sizeof(cvs));
}
#endif

unsigned int pds_findfirst(char *path,int attrib,struct pds_find_t *ffblk)
{
 unsigned int result=1;
 ffblk->ff_data=NULL;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  result=pds_lfn_findfirst(path,attrib,ffblk);
 else
  result=pds_dos_findfirst(path,attrib,ffblk);
 pds_filehand_unlock_entrance();
#else
 {
  struct _finddata_t *fsblk=pds_calloc(1,sizeof(struct _finddata_t));
  if(!fsblk){
   result=5; // ENOMEM
   goto ff_end;
  }
  ffblk->ff_handler=_findfirst(path,fsblk);
  if(ffblk->ff_handler>=0){
   result=0;
   ffblk->attrib =fsblk->attrib;
   pds_filetime_to_dosdatetime((void *)(&ffblk->cdate),fsblk->time_create);
   pds_filetime_to_dosdatetime((void *)(&ffblk->fdate),fsblk->time_write);
   pds_filetime_to_dosdatetime((void *)(&ffblk->adate),fsblk->time_access);
   //pds_memcpy((void *)(&ffblk->cdate),(void *)(&fsblk->time_create),sizeof(struct pds_fdate_t));
   //pds_memcpy((void *)(&ffblk->fdate),(void *)(&fsblk->time_write),sizeof(struct pds_fdate_t));
   //pds_memcpy((void *)(&ffblk->adate),(void *)(&fsblk->time_access),sizeof(struct pds_fdate_t));
   ffblk->size   =fsblk->size;
   pds_strcpy(ffblk->name,fsblk->name);
  }else{
   result=1;
   pds_free(fsblk);
   fsblk=NULL;
  }
  ffblk->ff_data=fsblk;
 }
ff_end:
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
 return result;
}

unsigned int pds_findnext(struct pds_find_t *ffblk)
{
 long result=1;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return result;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  result=pds_lfn_findnext(ffblk);
 else
  result=pds_dos_findnext(ffblk);
 pds_filehand_unlock_entrance();
#else
 struct _finddata_t *fsblk=(struct _finddata_t *)ffblk->ff_data;
 result=_findnext(ffblk->ff_handler,fsblk);
 if(result>=0){
  result=0;
  ffblk->attrib =fsblk->attrib;
  pds_filetime_to_dosdatetime((void *)(&ffblk->cdate),fsblk->time_create);
  pds_filetime_to_dosdatetime((void *)(&ffblk->fdate),fsblk->time_write);
  pds_filetime_to_dosdatetime((void *)(&ffblk->adate),fsblk->time_access);
  //pds_memcpy((void *)(&ffblk->cdate),(void *)(&fsblk->time_create),sizeof(struct pds_fdate_t));
  //pds_memcpy((void *)(&ffblk->fdate),(void *)(&fsblk->time_write),sizeof(struct pds_fdate_t));
  //pds_memcpy((void *)(&ffblk->adate),(void *)(&fsblk->time_access),sizeof(struct pds_fdate_t));
  ffblk->size   =fsblk->size;
  pds_strcpy(ffblk->name,fsblk->name);
 }else
  result=1;
#endif
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
 return result;
}

void pds_findclose(struct pds_find_t *ffblk)
{
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return;
 pds_filehand_lock_entrance();
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  pds_lfn_findclose(ffblk);
 else
  pds_dos_findclose(ffblk);
 pds_filehand_unlock_entrance();
#else
  if(ffblk->ff_data){
   #ifdef __WATCOMC__
   _findclose(ffblk->ff_handler);
   #endif
   pds_free(ffblk->ff_data);
  }
#endif
 ffblk->ff_data=NULL;
 funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISKACCESS);
}*/

mpxp_filesize_t pds_getfilesize(char *filename)
{
 mpxp_filesize_t filesize;
 struct pds_find_t ffblk;
 if(pds_findfirst(filename,_A_NORMAL,&ffblk)!=0)
  return 0;
 filesize=ffblk.size;
 pds_findclose(&ffblk);
 return filesize;
}

//----------------------------------------------------------------------

static void pds_truename_select(char *fullname,char *name,int mode)
{
#ifdef __DOS__
 struct rminfo RMI;
#endif
 char *p,strtmp[300];
 if(fullname!=NULL){
  if(fullname==name){
   pds_strcpy(strtmp,name);
   name=&strtmp[0];
  }else
   fullname[0]=0;
#ifdef __DOS__
  if(pds_filehand_check_entrance())
   return;
  pds_filehand_lock_entrance();

  fullname[0]=0;
  if(is_lfn_support){
   pds_dpmi_rmi_clear(&RMI);
   RMI.EAX=0x00007160;
   RMI.ECX=mode;
   RMI.DS =dm_int2x_1.segment;
   RMI.ES =dm_int2x_2.segment;
   funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
   pds_strcpy(dm_int2x_1.linearptr,name);
   *((char *)(dm_int2x_2.linearptr))=0;
   pds_dpmi_realmodeint_call(0x21,&RMI);
   if(!(funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)))
    pds_strcpy(fullname,dm_int2x_2.linearptr);
  }
  if(!fullname[0])
#endif
   _fullpath(fullname,name,270);

#ifdef MPXPLAY_WIN32
  if(!pds_filename_wildchar_chk(fullname)){
   pds_strcpy(strtmp,fullname);
   if(mode==2)
    GetLongPathName(strtmp,fullname,270);
   else
    GetShortPathName(strtmp,fullname,270);
   if(!fullname[0])
    pds_strcpy(fullname,strtmp);
  }
#endif

  p=pds_strrchr(fullname,PDS_DIRECTORY_SEPARATOR_CHAR);
  if(p && !p[1] && (p-fullname)>=sizeof("d:\\")) // cut '\\' from the end of path
   p[0]=0;

#ifdef __DOS__
  pds_filehand_unlock_entrance();
#endif
 }
}

// get long filename
static void pds_truename_lfn(char *longname,char *name)
{
 pds_truename_select(longname,name,2);
}

// get short filename
void pds_truename_dos(char *shortname,char *name)
{
 pds_truename_select(shortname,name,1);
}

//we already have got the fullpath (not in LFN), we just want to convert it to LFN (if possible)
/*void pds_truename(char *longname,char *name)
{
 if(is_lfn_support && (uselfn&USELFN_ENABLED))
  pds_truename_lfn(longname,name);
 else
  pds_strcpy(longname,name);
}*/

//drive functions
unsigned int asm_getdrive(void);

unsigned int pds_getdrive(void) // A=0,B=1,C=2,...
{
 unsigned int drivenum;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return ('C'-'A');
 pds_filehand_lock_entrance();
#endif
#if defined(__DOS__) && defined(NEWFUNC_ASM) && defined(__WATCOMC__)
 #pragma aux asm_getdrive=\
 "mov ax,1900h"\
 "int 21h"\
 "and eax,0x000000ff"\
 value[eax] modify[eax ebx ecx edx edi esi];
 drivenum=asm_getdrive();
#else
 drivenum=_getdrive()-1;
#endif
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
 return drivenum;
}

#ifdef MPXPLAY_LINK_WATTCP32
void asm_setdrive(unsigned int);

void pds_setdrive(int drivenum) // A=0,B=1,C=2,...
{
#if !defined(__DOS__) || !defined(NEWFUNC_ASM) || !defined(__WATCOMC__)
 unsigned int lastdrive;
#endif
 if((drivenum<0) || (drivenum>('Z'-'A')))
  return;
#ifdef __DOS__
 if(pds_filehand_check_entrance())
  return;
 pds_filehand_lock_entrance();
#endif
#if defined(__DOS__) && defined(NEWFUNC_ASM) && defined(__WATCOMC__)
 #pragma aux asm_setdrive=\
 "mov edx,eax"\
 "mov ax,0e00h"\
 "int 21h"\
 parm[eax] modify[eax ebx ecx edx edi esi];
 asm_setdrive(drivenum);
#else
 _dos_setdrive(drivenum+1,&lastdrive);
#endif
#ifdef __DOS__
 pds_filehand_unlock_entrance();
#endif
}
#endif

/*void pds_drive_getvolumelabel(unsigned int drivenum,char *volumename,unsigned int labbuflen)
{
#ifdef MPXPLAY_WIN32
 DWORD sernum,maxcomp,fsflags;
 char path[4]="C:\\";
 path[0]='A'+drivenum;
 path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
 GetVolumeInformation(path,volumename,labbuflen,&sernum,&maxcomp,&fsflags,NULL,0);
#elif defined(__DOS__)
 unsigned int error;
 char path[]="C:\\*.*";
 struct pds_find_t ffblk;
 struct rminfo RMI;

 volumename[0]=0;
 pds_memset(&ffblk,0,sizeof(ffblk));
 path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
 path[0]='A'+drivenum;
 error=pds_findfirst(path,_A_VOLID,&ffblk);
 if(!error && (ffblk.attrib&_A_VOLID)){
  char *ext;
  pds_strncpy(volumename,ffblk.name,labbuflen);
  volumename[labbuflen]=0;
  ext=pds_strrchr(volumename,'.');
  if(ext)
   pds_strcpy(ext,ext+1); // removes dot from disk volume name
 }
 pds_findclose(&ffblk);

 if(!volumename[0]){
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x440D;
  RMI.EBX=drivenum+1;
  RMI.ECX=0x0866;
  RMI.DS =dm_int2x_1.segment;
  *((char *)(dm_int2x_1.linearptr))=0;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_dpmi_realmodeint_call(0x21,&RMI);
  if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)){
   pds_strncpy(volumename,dm_int2x_1.linearptr+6,11);
   volumename[11]=0;
   pds_strcutspc(volumename);
  }
 }
#endif
}*/

void pds_drive_getvolumelabel(unsigned int drivenum,char *volumename,unsigned int labbuflen)
{
#ifdef MPXPLAY_WIN32
 DWORD sernum,maxcomp,fsflags;
 char path[4]="C:\\";
 path[0]='A'+drivenum;
 path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
 GetVolumeInformation(path,volumename,labbuflen,&sernum,&maxcomp,&fsflags,NULL,0);
#elif defined(__DOS__)
 struct rminfo RMI;
 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x440D;
 RMI.EBX=drivenum+1;
 RMI.ECX=0x0866;
 RMI.DS =dm_int2x_1.segment;
 *((char *)(dm_int2x_1.linearptr))=0;
 volumename[0]=0;
 funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
 pds_dpmi_realmodeint_call(0x21,&RMI);
 if(!funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)){
  unsigned int len=min(11,labbuflen);
  pds_strncpy(volumename,dm_int2x_1.linearptr+6,len);
  volumename[len]=0;
  pds_strcutspc(volumename);
 }
 if(!volumename[0] || (pds_stricmp(volumename,"No Name")==0)){
  unsigned int error;
  char path[]="C:\\*.*";
  struct pds_find_t ffblk;

  path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
  path[0]='A'+drivenum;
  error=pds_findfirst(path,_A_VOLID,&ffblk);
  if(!error && (ffblk.attrib&_A_VOLID)){
   char *ext;
   pds_strncpy(volumename,ffblk.name,labbuflen);
   volumename[labbuflen]=0;
   ext=pds_strrchr(volumename,'.');
   if(ext)
    pds_strcpy(ext,ext+1); // removes dot from disk volume name
  }
  pds_findclose(&ffblk);
 }
#endif
}

#ifndef __DOS__
#include <sys\stat.h>
#endif

extern unsigned int mpxplay_diskdrive_mscdex_is_drive_cd(int drive); // diskdriv\cd_drive\dos_mscd.c

//#define CHKDRIVE_DEBUG 1

unsigned int pds_chkdrive(unsigned int drive) // A=0 B=1 C=2 ...
{
#ifdef MPXPLAY_WIN32
 char path[4]="C:\\";
 path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
 path[0]='A'+drive;
 return GetDriveType(path); // !!! DRIVE_nnn and DRIVE_TYPE_nnn values have to be match
#elif defined(__DOS__)
 unsigned int drivetype=DRIVE_TYPE_NONE;
 struct rminfo RMI;

 if(mpxplay_diskdrive_mscdex_is_drive_cd(drive)){ // is drive CD?
#ifdef CHKDRIVE_DEBUG
  fprintf(stdout,"%c: CD\n",(drive+'A'));
#endif
  return DRIVE_TYPE_CD;
 }

 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x4408; // is drive removable (floppy)?
 RMI.EBX=drive+1;
 pds_dpmi_realmodeint_call(0x21,&RMI);
#ifdef CHKDRIVE_DEBUG
 fprintf(stdout,"%c: 4408H:%8.8X carry:%d\n",(drive+'A'),RMI.EAX,((funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))? 1:0));
#endif
 switch(RMI.EAX&0xff){
  case 0x00:drivetype=DRIVE_TYPE_FLOPPY;break; // removable
  case 0x01:drivetype=DRIVE_TYPE_HD;break;     // fixed
  case 0x0f:return DRIVE_TYPE_NONE;            // invalid drive
 }

 if((drivetype==DRIVE_TYPE_FLOPPY) && (drive<2)){ // check only floppy (do not check other removable medias like pendrive)
  unsigned int mapdrv;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x440e; // get logical drive mapping (of local drives)
  RMI.EBX=drive+1;
  funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
  pds_dpmi_realmodeint_call(0x21,&RMI);
#ifdef CHKDRIVE_DEBUG
   fprintf(stdout,"   440EH:%8.8X carry:%d\n",RMI.EAX,((funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))? 1:0));
#endif
  if(!(funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))){
   mapdrv=(RMI.EAX&0xff);
   if(mapdrv && (mapdrv!=(drive+1))) // phantom floppy
    return DRIVE_TYPE_NONE;
  }
 }else{
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x4409; // is drive remote (network) or local?
  RMI.EBX=drive+1;
  pds_dpmi_realmodeint_call(0x21,&RMI);
 #ifdef CHKDRIVE_DEBUG
  fprintf(stdout,"   4409H:%8.8X carry:%d\n",RMI.EDX,((funcbit_test(RMI.flags,RMINFO_FLAG_CARRY))? 1:0));
 #endif
  if(RMI.EDX<0xffff){
   if(RMI.EDX&0x1000)  // remote
    drivetype=DRIVE_TYPE_NETWORK;
   if((drivetype==DRIVE_TYPE_NONE) && (RMI.EDX&0x4040))  // IOCTL support (possibly a valid local disk drive) (if 0x4408 fails)
    drivetype=DRIVE_TYPE_HD;
  }
 }

 return drivetype;

#else
 char path[4]="C:\\";
 struct stat buf;
 path[2]=PDS_DIRECTORY_SEPARATOR_CHAR;
 path[0]='A'+drive;
 buf.st_mode=S_IFDIR;
 if(stat(path,&buf)==0)
  return DRIVE_TYPE_HD;
 //if(pds_access(path,F_OK)==0)
 // return DRIVE_TYPE_HD;
 if(mpxplay_diskdrive_mscdex_is_drive_cd(drive))
  return DRIVE_TYPE_CD;
 return DRIVE_TYPE_NONE;
#endif
}

unsigned int pds_dir_exists(char *path)
{
 unsigned int retcode=0;
 struct pds_find_t ffblk;

 pds_memset((void *)&ffblk,0,sizeof(ffblk));
 if(pds_findfirst(path,(_A_NORMAL|_A_SUBDIR),&ffblk)==0){ // can be a filename
  if(ffblk.attrib&_A_SUBDIR) // not filename
   retcode=1;
  pds_findclose(&ffblk);
 }else{
  if(pds_access(path,F_OK)==0) // for rootdir
   retcode=1;
 }
 return retcode;
}

//-------------------------------------------------------------------------
unsigned int pds_network_check(void)
{
#ifdef __DOS__
 struct rminfo RMI;
 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x1100; // is network supported/installed
 pds_dpmi_realmodeint_call(0x2f,&RMI);
 return (RMI.EAX&0xff);
#else
 return 0;
#endif
}

int pds_network_query_assign(unsigned int index,char *drvname,unsigned int maxdrvlen,char *volname,unsigned int maxvollen)
{
#ifdef __DOS__
 struct rminfo RMI;
 pds_dpmi_rmi_clear(&RMI);
 RMI.EAX=0x5f02; // query network assign list entry
 RMI.EBX=index;
 RMI.DS =dm_int2x_1.segment;
 RMI.ES =dm_int2x_2.segment;
 *((char *)(dm_int2x_1.linearptr))=0;
 *((char *)(dm_int2x_2.linearptr))=0;
 funcbit_enable(RMI.flags,RMINFO_FLAG_CARRY);
 pds_dpmi_realmodeint_call(0x21,&RMI);
 if(funcbit_test(RMI.flags,RMINFO_FLAG_CARRY)) // fatal error (?) (or end of assign list)
  return -1;
 //if((RMI.EBX&0xff00)==0x0000) // is entry enabled (accessable)?
 // return RMI.EBX;
 if((RMI.EBX&0x00ff)!=0x0004) // is entry drive?
  return RMI.EBX;
 pds_strncpy(drvname,dm_int2x_1.linearptr,maxdrvlen);
 pds_strncpy(volname,dm_int2x_2.linearptr,maxvollen);
 return 0;
#else
 return 1;
#endif
}

//-------------------------------------------------------------------------
void pds_drives_flush(void)
{
#ifdef __DOS__
 union REGPACK regp;
 pds_newfunc_regp_clear(&regp);
 regp.w.ax=0x0d00;
 intr(0x21,&regp);
#endif
}

//-------------------------------------------------------------------------
//limit short pathes\filenames to 8.3
void pds_sfn_limit(char *fn)
{
#ifdef __DOS__
 char *nextdir,*fn_save;
 if((is_lfn_support && (uselfn&USELFN_ENABLED)) || !fn || !fn[0])
  return;
 fn_save=fn;
 do{
  char *ext;
  unsigned int alen,flen,elen;
  nextdir=pds_strchr(fn,PDS_DIRECTORY_SEPARATOR_CHAR);
  if(nextdir)
   alen=nextdir-fn;
  else
   alen=pds_strlen(fn);
  ext=pds_strnchr(fn,'.',alen);
  if(ext){
   flen=ext-fn;
   elen=alen-flen-1;
  }else{
   flen=alen;
   elen=0;
  }
  if(elen>3){
   if(nextdir)
    pds_strcpy(&ext[4],nextdir);
   else
    ext[4]=0;
  }
  if(flen>8){
   if(ext)
    pds_strcpy(&fn[8],ext);
   else if(nextdir)
    pds_strcpy(&fn[8],nextdir);
   else
    fn[8]=0;
  }
  fn=nextdir+1;
 }while(nextdir);
 strupr(fn_save);
#endif
}

int pds_access(char *path,int amode)
{
 //struct stat buf;
 //buf.st_mode=S_IFDIR;
 //if(stat(path,&buf)!=0)
 // return -1;
 return access(path,amode);
}
