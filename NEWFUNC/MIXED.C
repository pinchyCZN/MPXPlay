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
//function: mixed functions

#include <stdlib.h>
#include <dos.h>
#include "newfunc.h"

#ifdef MPXPLAY_WIN32
 #include <wincon.h>
#endif

extern unsigned int iswin9x,id3textconv;
extern dosmem_t dm_int2x_1;

static unsigned int firstrand;

unsigned long pds_rand(int maxnum)
{
#ifdef __DOS__
 unsigned int i;
 char *biosmem;
#endif
 unsigned int r;

 if(maxnum<1)
  return 0;
 if(!firstrand){
  srand(pds_gettime());
  firstrand=1;
 }
#ifdef __DOS__
 r=0;
 biosmem=(char *)0;
 for(i=(biosmem[0x46c]&127);i;i--)
  r+=(biosmem[0x046c]);
 r+=rand();
#else
 r=rand()*rand()*rand();
#endif
 return (r%maxnum);
}

//--------------------------------------------------------------------------
#ifdef __DOS__
static int mswin_version=-1;
#endif

#ifdef MPXPLAY_WIN32
#include <playlist\playlist.h>
#endif

unsigned int pds_mswin_getver(void)
{
#ifdef __DOS__
 union REGPACK regp;
 if(mswin_version<0){
  pds_newfunc_regp_clear(&regp);
  regp.w.ax=0x160A;
  intr(0x2f,&regp);
  mswin_version=(unsigned int)(regp.w.bx);
 }
 return mswin_version;
#else
 #ifdef MPXPLAY_WIN32
 OSVERSIONINFO	osVersion;
 pds_memset(&osVersion,0,sizeof(osVersion));
 osVersion.dwOSVersionInfoSize = sizeof( osVersion );
 GetVersionEx(&osVersion);
 if(osVersion.dwPlatformId!=VER_PLATFORM_WIN32_NT)
  return 1024;
 #endif
 return 0;
#endif
}

void pds_mswin_setapplicationtitle(char *title)
{
#ifdef MPXPLAY_WIN32
 /*unsigned char utftmp[512];
 if(iswin9x)
  SetConsoleTitleA(title);
 else{
  mpxplay_playlist_textconv_selected_back(utftmp,sizeof(utftmp),title,(id3textconv&ID3TEXTCONV_CODEPAGE)|ID3TEXTCONV_UTF16);
  SetConsoleTitleW((unsigned short *)&utftmp[0]);
 }*/
 if(iswin9x)
  mpxplay_playlist_textconv_back(title,title);
 SetConsoleTitleA(title);
#elif defined(__DOS__)
 if(iswin9x && !pds_filehand_check_infilehand()){
  struct rminfo RMI;
  pds_dpmi_rmi_clear(&RMI);
  RMI.EAX=0x0000168E;
  RMI.ECX=80;
  RMI.ES =dm_int2x_1.segment;
  pds_strcpy(dm_int2x_1.linearptr,title);
  pds_dpmi_realmodeint_call(0x2f,&RMI);
 }
#endif
}

//--------------------------------------------------------------------------
//shutdown routine
void asm_disk_reset(void);
void asm_apm_shutdown(void);

#ifdef MPXPLAY_WIN32
 #ifndef EWX_FORCEIFHUNG
  #define EWX_FORCEIFHUNG  0x00000010
 #endif
#endif

void pds_shutdown_atx(void)
{
#ifdef MPXPLAY_WIN32
 if(!(GetVersion()&0x80000000)){
  HANDLE h;
  if(OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&h)){
   LUID luid;
   if(LookupPrivilegeValue(NULL,"SeShutdownPrivilege",&luid)){
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount=1;
    tp.Privileges[0].Luid=luid;
    tp.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(h,FALSE,&tp,0,NULL,NULL);
   }
   CloseHandle(h);
  }
 }
 ExitWindowsEx(EWX_SHUTDOWN|EWX_POWEROFF|EWX_FORCEIFHUNG, 0);
#elif defined(__DOS__)
 if(iswin9x){
  system("rundll32.exe shell32.dll,SHExitWindowsEx 5"); // faster
  //system("rundll32.exe User,ExitWindows"); // more safe
 }else{
  //1.disks reset
  //2.reset disk system
 #ifdef __WATCOMC__
  #pragma aux asm_disk_reset=\
  "mov ax,0x0d00"\
  "int 0x21"\
  "xor eax,eax"\
  "mov dx,128"\
  "int 0x13"\
  modify[eax edx];
  asm_disk_reset(); // flush caches and reset disks

  //1.RealMode Interface connect
  //2.Engage power management
  //3.Enable APM for all devices
  //4.Force version 1.1 compatibility
  //5 First attempt: using APM 1.1 or later
  //  Shutdown all the devices supported by APM
  //6.Second attempt: using APM 1.0
  //  Shutdown only the system BIOS
  #pragma aux asm_apm_shutdown=\
  "mov ax,0x5301"\
  "mov bx,0"\
  "int 0x15"\
  "mov ax,0x530f"\
  "mov bx,1"\
  "mov cx,1"\
  "int 0x15"\
  "mov ax,0x5308"\
  "mov bx,1"\
  "mov cx,1"\
  "int 0x15"\
  "mov ax,0x530e"\
  "mov bx,0"\
  "mov cx,0x0101"\
  "int 0x15"\
  "mov ax,0x5307"\
  "mov bx,1"\
  "mov cx,3"\
  "int 0x15"\
  "mov ax,0x5307"\
  "mov bx,0"\
  "mov cx,3"\
  "int 0x15"\
  modify[eax ebx ecx];
  asm_apm_shutdown(); // shutdown by advanced power managment (BIOS)
 #endif // __WATCOMC__
 }
#endif // __DOS__
}
