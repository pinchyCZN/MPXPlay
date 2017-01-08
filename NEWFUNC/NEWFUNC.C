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
//function: newfunc (stuffs) main

#include "newfunc.h"
#include "dll_load.h"
#include "mpxplay.h"

extern unsigned int oldposrow, intsoundconfig, intsoundcontrol;

char DOS4GOPTIONS[] = "dos4g=StartupBanner:OFF\n";	// for DOS4G v2.xx

#ifdef WIN32
/*
unsigned _dos_findfirst(const char *path,unsigned attr,struct find_t *buf )
{
	HANDLE ff;
	WIN32_FIND_DATA wfd;
	ff=FindFirstFile(path,&wfd);
	if(ff==INVALID_HANDLE_VALUE)
		return -1;
	buf->attrib=wfd.dwFileAttributes;
	strncpy(buf->name,wfd.cFileName,sizeof(buf->name));
	buf->size=wfd.nFileSizeLow;
	buf->wr_date=wfd.ftLastWriteTime.dwLowDateTime;
	buf->wr_time=wfd.ftCreationTime.dwLowDateTime;
	return ff;
}
unsigned _dos_findnext(struct find_t *result)
{
}
*/
unsigned _dos_getfileattr(const char *path, unsigned *attr)
{
	int a;
	a = GetFileAttributes(path);
	if(a == 0xFFFFFFFF)
		return -1;
	*attr = a;
	return 0;
}
unsigned _dos_setfileattr(const char *path, unsigned *attr)
{
	if(SetFileAttributes(path, *attr) == 0)
		return -1;				//fail
	else
		return 0;
}
#endif
//-------------------------------------------------------------------------
//newfunc init & close

void newfunc_init(char *prgname)
{
	newfunc_keyboard_init();
	newfunc_cpu_init();
	newfunc_memory_init();
	newfunc_error_handlers_init();

#ifdef __DOS__
	if(!pds_int2x_dosmems_allocate())
		mpxplay_close_program(MPXERROR_CONVENTIONAL_MEM);

	pds_indosflag_init();
#endif							// __DOS__
	pds_check_lfnapi(prgname);

	newfunc_textdisplay_init();
	oldposrow = pds_textdisplay_getcursor_y();
}

void newfunc_close(void)
{
	newfunc_dllload_closeall();
	newfunc_newhandler08_close();
#ifdef __DOS__
	pds_int2x_dosmems_free();
#endif
	newfunc_textdisplay_close();
}

//-------------------------------------------------------------------
//newfunc API for DLLs

#if defined(__DOS__) && defined(MPXPLAY_LINK_DLLLOAD)

#include "control\control.h"

extern mainvars mvps;

struct mpxplay_resource_s mpxplay_resources = {
	&mvps,						// 0.
	NULL,
	&intsoundconfig,
	&intsoundcontrol,
	0,
	0,
	0,
	0,
	0,
	0,

	//dpmi.c
	&pds_dpmi_getrmvect,		// 10.
	&pds_dpmi_setrmvect,
	&pds_dpmi_getexcvect,
	&pds_dpmi_setexcvect,
	&pds_dos_getvect,
	&pds_dos_setvect,
	&pds_dpmi_dos_allocmem,
	&pds_dpmi_dos_freemem,
	&pds_dpmi_realmodeint_call,
	NULL,

	//drivehnd.c
	&pds_fullpath,				// 20.
	&pds_getcwd,
	&pds_getdcwd,
	&pds_chdir,
	&pds_mkdir,
	&pds_rmdir,
	&pds_rename,
	&pds_unlink,
	&pds_findfirst,
	&pds_findnext,
	&pds_findclose,				// 30.
	&pds_truename_dos,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	//filehand.c
	&pds_open_read,				// 40.
	&pds_open_write,
	&pds_open_create,
	&pds_dos_read,
	&pds_dos_write,
	&pds_close,
	&pds_lseek,
	&pds_tell,
	&pds_eof,
	&pds_filelength,
	&pds_chsize,				// 50.
	&pds_fopen,
	&pds_fclose,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	//memory.c
	&pds_memcpy,				// 60.
	&pds_memset,
	&pds_qmemreset,
	&pds_qmemcpy,
	&pds_qmemcpyr,
	&pds_memxch,
	&pds_malloc,
	&pds_calloc,
	&pds_realloc,
	&pds_free,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL,						// 80.
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	//textdisp.c
	&pds_textdisplay_charxy,	// 90.
	&pds_textdisplay_textxy,
	&pds_textdisplay_clrscr,
	&pds_textdisplay_scrollup,
	&pds_textdisplay_printf,
	&pds_textdisplay_getbkcolorxy,
	&pds_textdisplay_setcolorxy,
	&pds_textdisplay_setbkcolorxy,
	&pds_textdisplay_spacecxyn,
	&pds_textdisplay_vidmem_save,
	&pds_textdisplay_vidmem_restore,	//100.
	&pds_textdisplay_setresolution,
	&pds_textdisplay_getresolution,
	&pds_textdisplay_getcursor_y,
	&pds_textdisplay_setcursor_position,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	//time.c
	&pds_gettimeh,				// 110.
	&pds_gettime,
	&pds_getdate,
	&pds_strtime_to_hextime,
	&pds_strtime_to_hexhtime,
	&pds_delay_10us,
	&pds_gettimeu,
	NULL,
	NULL,
	NULL,

	//timer.c
	&mpxplay_timer_secs_to_counternum,	//120.
	&mpxplay_timer_addfunc,
	&mpxplay_timer_modifyfunc,
	&mpxplay_timer_deletefunc,
	&mpxplay_timer_deletehandler,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL						// 129.
};

#endif
