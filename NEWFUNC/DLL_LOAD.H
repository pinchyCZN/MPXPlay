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
//function: definitions for DLL handling

#ifndef mpxplay_dll_load_h
#define mpxplay_dll_load_h

//#define MPXPLAY_LINK_DLLLOAD 1

#include <mpxplay.h>
#include "newfunc.h"

//current dll and module entry_structure_version
#define MPXPLAY_DLLENTRY_STRUCTURE_VERSION  0x0150
#define MPXPLAY_DLLENTRY_MAX_MODULES 16  // just for verifying. need more?

//moduletype_major values
#define MPXPLAY_DLLMODULETYPE_CONTROL_CMDLINE     0x0100 // extended command-line arguments
#define MPXPLAY_DLLMODULETYPE_CONTROL_KEYBOARD    0x0110
#define MPXPLAY_DLLMODULETYPE_CONTROL_SERIAL      0x0140 // serialhandler
#define MPXPLAY_DLLMODULETYPE_DISPLAY_VISUALPI    0x0340 // visual-plugin
#define MPXPLAY_DLLMODULETYPE_DISPLAY_LCDLOWFUNC  0x03C0 // LCD hardware
#define MPXPLAY_DLLMODULETYPE_DRIVEHAND           0x0400
//#define MPXPLAY_DLLMODULETYPE_FILEHAND_LOW        0x0410 // alternative file source (http,ftp)
#define MPXPLAY_DLLMODULETYPE_FILEIN_PLAYLIST     0x0500
#define MPXPLAY_DLLMODULETYPE_FILEIN_PARSER       0x0540 // stream
#define MPXPLAY_DLLMODULETYPE_FILEIN_CONTAINER    0x0541 // container
#define MPXPLAY_DLLMODULETYPE_DECODER_AUDIO       0x0600
#define MPXPLAY_DLLMODULETYPE_DECODER_VIDEO       0x0601
#define MPXPLAY_DLLMODULETYPE_AUCARD              0x0800
#define MPXPLAY_DLLMODULETYPE_AUMIXER             0x0810
#define MPXPLAY_DLLMODULETYPE_VIDEOOUT            0x0900 // video output
#define MPXPLAY_DLLMODULETYPE_VIDEOMIX            0x0910 // video mixer (postporcess functions)

//current module (structure) versions
#define MPXPLAY_DLLMODULEVER_CONTROL_CMDLINE     0x0153
//#define MPXPLAY_DLLMODULEVER_CONTROL_KEYBOARD    0x0153
#define MPXPLAY_DLLMODULEVER_CONTROL_SERIAL      0x0153
#define MPXPLAY_DLLMODULEVER_DISPLAY_VISUALPI    0x0153
//#define MPXPLAY_DLLMODULEVER_DISPLAY_LCDLOWFUNC  0x0153
#define MPXPLAY_DLLMODULEVER_DRIVEHAND           0x0156
//#define MPXPLAY_DLLMODULEVER_FILEHAND_LOW        0x0153
//#define MPXPLAY_DLLMODULEVER_FILEIN_PLAYLIST     0x0153
#define MPXPLAY_DLLMODULEVER_FILEIN_PARSER       0x0154
#define MPXPLAY_DLLMODULEVER_FILEIN_CONTAINER    0x0154
#define MPXPLAY_DLLMODULEVER_DECODER_AUDIO       0x0154
//#define MPXPLAY_DLLMODULEVER_DECODER_VIDEO       0x0153
#define MPXPLAY_DLLMODULEVER_AUCARD              0x0154
#define MPXPLAY_DLLMODULEVER_AUMIXER             0x0154
#define MPXPLAY_DLLMODULEVER_VIDEOOUT            0x0153
//#define MPXPLAY_DLLMODULEVER_VIDEOMIX            0x0153

typedef struct mpxplay_module_entry_v0150_s{
 unsigned long moduletype_major;
 unsigned long moduletype_minor;
 char         *modulename_minor;
 unsigned long module_structure_version;
 void         *module_callpoint;
}mpxplay_module_entry_v0150_s;

typedef struct mpxplay_dll_entry_v0150_s{
 unsigned long entry_structure_version;        // dll & module entry versions
 mpxplay_module_entry_v0150_s *module_entries[]; // closed with a NULL
}mpxplay_dll_entry_v0150_s;

// the current structures
typedef mpxplay_dll_entry_v0150_s    mpxplay_dll_entry_s;
typedef mpxplay_module_entry_v0150_s mpxplay_module_entry_s;

typedef struct dll_found_s{
 char *dllfilename;
#ifdef MPXPLAY_WIN32
 HMODULE prochandle;
#else
 int prochandle;
#endif
 mpxplay_module_entry_s **module_entries; // copy of me_dll structure (closed with a NULL)
 mpxplay_module_entry_s **me_dll;         // direct pointer to dll    (closed with a NULL)
}dll_found_s;

#ifdef __DOS__

typedef struct mpxplay_resource_s{
 struct mainvars *mvp;            // 0.
 char  *outfilepath;
 unsigned int *intsoundconfig;
 unsigned int *intsoundcontrol;
 unsigned long res4;
 unsigned long res5;
 unsigned long res6;
 unsigned long res7;
 unsigned long res8;
 unsigned long res9;

 //dpmi.c                           10.
 void far *(*pds_dpmi_getrmvect)(unsigned int intno);
 void (*pds_dpmi_setrmvect)(unsigned int intno, unsigned int segment,unsigned int offset);
 void far *(*pds_dpmi_getexcvect)(unsigned int intno);
 void  (*pds_dpmi_setexcvect)(unsigned int intno, void far *vect);
 void far *(*pds_dos_getvect)(unsigned int intno);
 void  (*pds_dos_setvect)(unsigned int intno, void far *vect);
 int   (*pds_dpmi_dos_allocmem)(dosmem_t *,unsigned int);
 void  (*pds_dpmi_dos_freemem)(dosmem_t *);
 void  (*pds_dpmi_realmodeint_call)(unsigned int intnum,struct rminfo *rmi);
 unsigned long res19;

 //drivehnd.c                       20.
 void  (*pds_fullpath)(char *fullname,char *name);
 void  (*pds_getcwd)(char *pathbuf); // pathbuf must be min. 300 chars long
 void  (*pds_getdcwd)(int drive,char *pathbuf); // pathbuf must be min. 300 chars long
 int   (*pds_chdir)(char *setdir);
 int   (*pds_mkdir)(char *newdirname);
 int   (*pds_rmdir)(char *dirname);
 int   (*pds_rename)(char *oldfilename,char *newfilename);
 int   (*pds_unlink)(char *filename);
 unsigned int (*pds_findfirst)(char *path,int attrib,struct pds_find_t *ffblk);
 unsigned int (*pds_findnext)(struct pds_find_t *ffblk);
 void  (*pds_findclose)(struct pds_find_t *ffblk);
 void  (*pds_truename_dos)(char *shortname,char *name);
 unsigned long res32;
 unsigned long res33;
 unsigned long res34;
 unsigned long res35;
 unsigned long res36;
 unsigned long res37;
 unsigned long res38;
 unsigned long res39;

 //filehand.c                       40.
 int   (*pds_open_read)(char *filename,unsigned int mode);  // mode: O_RDONLY| O_BINARY|O_TEXT
 int   (*pds_open_write)(char *filename,unsigned int mode); //       O_RDWR| O_BINARY|O_TEXT
 int   (*pds_open_create)(char *filename,unsigned int mode); //      O_RDWR| O_BINARY|O_TEXT
 int   (*pds_dos_read)(int,char *,unsigned int);
 int   (*pds_dos_write)(int,char *,unsigned int);
 void  (*pds_close)(int);
 long  (*pds_lseek)(int,long,int);
 long  (*pds_tell)(int);
 int   (*pds_eof)(int);
 long  (*pds_filelength)(int);
 int   (*pds_chsize)(int filehand,long size);
 FILE *(*pds_fopen)(char *,char *);
 int   (*pds_fclose)(FILE *);
 unsigned long res53;
 unsigned long res54;
 unsigned long res55;
 unsigned long res56;
 unsigned long res57;
 unsigned long res58;
 unsigned long res59;

 //memory.c                         60.
 void  (*pds_memcpy)(void *,void *,unsigned int);
 void  (*pds_memset)(void *,int,unsigned int);
 void  (*pds_qmemreset)(void *,unsigned int);
 void  (*pds_qmemcpy)(void *,void *,unsigned int);
 void  (*pds_qmemcpyr)(void *,void *,unsigned int);
 void  (*pds_memxch)(char *,char *,unsigned int);
 void *(*pds_malloc)(unsigned int bufsize);
 void *(*pds_calloc)(unsigned int nitems,unsigned int itemsize);
 void *(*pds_realloc)(void *bufptr,unsigned int bufsize);
 void  (*pds_free)(void *bufptr);
 unsigned long res70;
 unsigned long res71;
 unsigned long res72;
 unsigned long res73;
 unsigned long res74;
 unsigned long res75;
 unsigned long res76;
 unsigned long res77;
 unsigned long res78;
 unsigned long res79;

 unsigned long res80;
 unsigned long res81;
 unsigned long res82;
 unsigned long res83;
 unsigned long res84;
 unsigned long res85;
 unsigned long res86;
 unsigned long res87;
 unsigned long res88;
 unsigned long res89;

 //textdisp.c                      90.
 void  (*pds_textdisplay_charxy)(unsigned int,unsigned int,unsigned int,char);
 unsigned int (*pds_textdisplay_textxy)(unsigned int,unsigned int,unsigned int,char *);
 void  (*pds_textdisplay_clrscr)(void);
 void  (*pds_textdisplay_scrollup)(unsigned int);
 void  (*pds_textdisplay_printf)(char *);
 unsigned int (*pds_textdisplay_getbkcolorxy)(unsigned int,unsigned int);
 void  (*pds_textdisplay_setcolorxy)(unsigned int,unsigned int,unsigned int);
 void  (*pds_textdisplay_setbkcolorxy)(unsigned int,unsigned int,unsigned int);
 void  (*pds_textdisplay_spacecxyn)(unsigned int,unsigned int,unsigned int,unsigned int);
 void  (*pds_textdisplay_vidmem_save)(void);
 void  (*pds_textdisplay_vidmem_restore)(void);
 void  (*pds_textdisplay_setresolution)(unsigned int lines);
 void  (*pds_textdisplay_getresolution)(void);
 unsigned int (*pds_textdisplay_getcursor_y)(void);
 void  (*pds_textdisplay_setcursor_position)(unsigned int x,unsigned int y);
 unsigned long res105;
 unsigned long res106;
 unsigned long res107;
 unsigned long res108;
 unsigned long res109;

 //time.c                          110.
 unsigned long (*pds_gettimeh)(void);
 unsigned long (*pds_gettime)(void);
 unsigned long (*pds_getdate)(void);
 unsigned long (*pds_strtime_to_hextime)(char *timestr,unsigned int houralign);
 unsigned long (*pds_strtime_to_hexhtime)(char *timestr);
 void  (*pds_delay_10us)(unsigned int ticks);
 mpxp_uint64_t (*pds_gettimeu)(void);
 unsigned long res117;
 unsigned long res118;
 unsigned long res119;

 //timer.c                         120.
 unsigned long (*mpxplay_timer_secs_to_counternum)(unsigned long secs);
 int  (*mpxplay_timer_addfunc)(void *callback_func,void *callback_data,unsigned int flags,unsigned int refresh_delay);
 int  (*mpxplay_timer_modifyfunc)(void *func,int flags,int refresh_delay);
 void (*mpxplay_timer_deletefunc)(void *callback_func,void *callback_data);
 void (*mpxplay_timer_deletehandler)(void *func,int handlernum_index);
 unsigned long res125;
 unsigned long res126;
 unsigned long res127;
 unsigned long res128;
 unsigned long res129;

}mpxplay_resource_s;

#endif // __DOS__

#endif // mpxplay_dll_load_h
