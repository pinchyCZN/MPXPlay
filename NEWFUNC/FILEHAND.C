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
//function: file handler routines

#include <share.h>
#include "newfunc.h"
#include "mpxplay.h"

#ifdef __DOS__
extern unsigned int intsoundcontrol;
extern unsigned int is_lfn_support, uselfn;
extern dosmem_t dm_int2x_1, dm_int2x_2;
static char far *indosptr;
#endif
static unsigned int in_filehand;

extern volatile unsigned long mpxplay_signal_events;

//--------------------------------------------------------------------------
//indos flag

void pds_indosflag_init(void)
{
#ifdef __DOS__
	struct rminfo RMI;
	long selector;

	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x00003400;
	pds_dpmi_realmodeint_call(0x21, &RMI);
	selector = pds_dpmi_segment_to_selector(RMI.ES);
	if(selector >= 0)
		indosptr = MK_FP(selector, RMI.EBX);
#endif
}

unsigned int pds_indos_flag(void)
{
#ifdef __DOS
	if(intsoundcontrol & INTSOUND_DOSSHELL)	// !!! because it's allways 1 then
		return 0;				//
	if(indosptr)
		return (unsigned int)(*indosptr);
#endif
	return 0;
}

//-------------------------------------------------------------------------

unsigned int pds_filehand_check_infilehand(void)
{
	return in_filehand;
}

#ifdef __DOS__
unsigned int pds_filehand_check_entrance(void)
{
	if(in_filehand)
		return 1;
	if(intsoundcontrol & INTSOUND_DOSSHELL)
		return 1;
	if(pds_indos_flag())
		return 1;
	return 0;
}

void pds_filehand_lock_entrance(void)
{
	in_filehand = 1;
}

void pds_filehand_unlock_entrance(void)
{
	in_filehand = 0;
}
#endif

//----------------------------------------------------------------------
#ifdef __DOS__

static int pds_lfn_open_read(char *filename, unsigned int mode)
{
	struct rminfo RMI;
	unsigned int retry = 2;		// possible DOSLFN bug (not a 100% solution)
	do {
		pds_dpmi_rmi_clear(&RMI);
		RMI.EAX = 0x0000716C;
		RMI.EDX = 0x01;			// open, fail if not exist
		RMI.EBX = (mode & 0x0f) | SH_DENYNO;	// O_RDONLY|O_BINARY
		RMI.DS = dm_int2x_1.segment;
		funcbit_enable(RMI.flags, RMINFO_FLAG_CARRY);
		pds_strcpy(dm_int2x_1.linearptr, filename);
		pds_dpmi_realmodeint_call(0x21, &RMI);
		if(!funcbit_test(RMI.flags, RMINFO_FLAG_CARRY))
			return (RMI.EAX & 0xffff);
	} while(--retry);
	return 0;
}

static int pds_lfn_open_write(char *filename, unsigned int mode)
{
	struct rminfo RMI;

	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x0000716C;
	RMI.EDX = 0x01;				// open, fail if not exist
	RMI.EBX = (mode & 0x0f) | SH_DENYWR;	// O_RDWR|O_BINARY
	RMI.DS = dm_int2x_1.segment;
	funcbit_enable(RMI.flags, RMINFO_FLAG_CARRY);
	pds_strcpy(dm_int2x_1.linearptr, filename);
	pds_dpmi_realmodeint_call(0x21, &RMI);
	if(!funcbit_test(RMI.flags, RMINFO_FLAG_CARRY))
		return (RMI.EAX & 0xffff);
	return 0;
}

static int pds_lfn_open_create(char *filename, unsigned int mode)
{
	struct rminfo RMI;

	pds_dpmi_rmi_clear(&RMI);
	RMI.EAX = 0x0000716C;
	RMI.EDX = 0x12;				// truncate if exist, create if not
	RMI.EBX = (mode & 0x0f) | SH_DENYWR;	// O_RDWR|O_BINARY
	RMI.DS = dm_int2x_1.segment;
	funcbit_enable(RMI.flags, RMINFO_FLAG_CARRY);
	pds_strcpy(dm_int2x_1.linearptr, filename);
	pds_dpmi_realmodeint_call(0x21, &RMI);
	if(!funcbit_test(RMI.flags, RMINFO_FLAG_CARRY))
		return (RMI.EAX & 0xffff);
	return 0;
}

#endif

int pds_open_read(char *filename, unsigned int mode)
{
	int filehand = 0;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return filehand;
	pds_filehand_lock_entrance();
	if(is_lfn_support && (uselfn & USELFN_ENABLED))
		filehand = pds_lfn_open_read(filename, mode);
	else
#endif
		filehand = sopen(filename, mode, SH_COMPAT);

#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif

	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);

	if(filehand < 0)
		filehand = 0;
	return filehand;
}

int pds_open_write(char *filename, unsigned int mode)
{
	int filehand = 0;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return filehand;
	pds_filehand_lock_entrance();
	if(is_lfn_support && (uselfn & USELFN_ENABLED))
		filehand = pds_lfn_open_write(filename, mode);
	else
#endif
		filehand = sopen(filename, mode, SH_DENYWR);

#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif

	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);

	if(filehand < 0)
		filehand = 0;
	return filehand;
}

int pds_open_create(char *filename, unsigned int mode)
{
	int filehand = 0;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return filehand;
	pds_filehand_lock_entrance();
	if(is_lfn_support && (uselfn & USELFN_ENABLED))
		filehand = pds_lfn_open_create(filename, mode);
	else
#endif
		filehand = sopen(filename, mode | O_CREAT | O_TRUNC, SH_DENYWR, S_IREAD | S_IWRITE);

#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif

	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);

	if(filehand < 0)
		filehand = 0;
	return filehand;
}

void pds_close(int filehand)
{
	if(filehand < 1)
		return;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return;
	pds_filehand_lock_entrance();
#endif
	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
#ifdef __DOS__
	if(is_lfn_support && (uselfn & USELFN_ENABLED))
		_dos_close(filehand);
	else
#endif
		close(filehand);
#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif
}

int pds_dos_read(int filehand, char *buf, unsigned int len)
{
	int b;
	if(filehand < 1)
		return 0;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return 0;
	pds_filehand_lock_entrance();
#endif
	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
#ifdef __DOS__
	if(is_lfn_support && (uselfn & USELFN_ENABLED)) {
		if(_dos_read(filehand, buf, len, (unsigned int *)&b) != 0)
			b = 0;
	} else
#endif
		b = read(filehand, buf, len);

#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif

	return b;
}

int pds_dos_write(int filehand, char *buf, unsigned int len)
{
	int b;
	if(filehand < 1)
		return 0;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return 0;
	pds_filehand_lock_entrance();
#endif
	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
#ifdef __DOS__
	if(is_lfn_support && (uselfn & USELFN_ENABLED)) {
		if(_dos_write(filehand, buf, len, (unsigned int *)&b) != 0)
			b = 0;
	} else
#endif
		b = write(filehand, buf, len);

#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif

	return b;
}

mpxp_filesize_t pds_lseek(int filehand, mpxp_filesize_t offset, int fromwhere)
{
	mpxp_filesize_t newpos = -1;
	if(filehand < 1)
		return newpos;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return newpos;
	pds_filehand_lock_entrance();
#endif
	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
	newpos = lseek(filehand, offset, fromwhere);
#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif
	return newpos;
}

mpxp_filesize_t pds_tell(int filehand)
{
	mpxp_filesize_t filepos = -1;
	if(filehand < 1)
		return filepos;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return filepos;
	pds_filehand_lock_entrance();
#endif
	filepos = tell(filehand);
#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif
	return filepos;
}

int pds_eof(int filehand)
{
	int flag = 1;
	if(filehand < 1)
		return flag;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return flag;
	pds_filehand_lock_entrance();
#endif
	flag = eof(filehand);
#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif
	return flag;
}

mpxp_filesize_t pds_filelength(int filehand)
{
	mpxp_filesize_t filelen = 0;
	if(filehand < 1)
		return filelen;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return filelen;
	pds_filehand_lock_entrance();
#endif
	filelen = filelength(filehand);
#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif
	return filelen;
}

int pds_chsize(int filehand, mpxp_filesize_t size)
{
	int success;
	if(filehand < 1)
		return 0;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return 0;
	pds_filehand_lock_entrance();
#endif
	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
	success = chsize(filehand, size);
	if(success < 0)
		success = 0;
	else
		success = 1;
#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif
	return success;
}

// for non-audio files (playlists)
FILE *pds_fopen(char *filename, char *mode)
{
	FILE *fp = NULL;
#ifdef __DOS__
	if(pds_filehand_check_entrance())
		return fp;
	if(is_lfn_support && (uselfn & USELFN_ENABLED)) {	// truename_dos can't make filename if it doesn't exist
		int filehand = 0;
		char shortfname[300];
		if(pds_strchr(mode, 'w')) {
			filehand = pds_open_create(filename, (O_RDWR | O_BINARY));
			if(!filehand)
				return fp;
		} else if(pds_strchr(mode, 'a')) {
			filehand = pds_open_write(filename, (O_RDWR | O_BINARY));
			if(!filehand)
				filehand = pds_open_create(filename, (O_RDWR | O_BINARY));
			if(!filehand)
				return fp;
		}
		if(filehand)
			pds_close(filehand);
		pds_truename_dos(shortfname, filename);
		filename = &shortfname[0];
	}
	pds_filehand_lock_entrance();
#endif
	fp = fopen(filename, mode);
#ifdef __DOS__
	pds_filehand_unlock_entrance();
#endif
	return fp;
}

int pds_fclose(FILE * fp)
{
	return fclose(fp);
}
