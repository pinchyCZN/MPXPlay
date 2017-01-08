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
//function: mscdex based audio CD handling

#ifdef __DOS__

#include <newfunc/newfunc.h>
#include "cd_drive.h"

#define MSCDEX_USE_ASM 1

#define MSCDEX_INITSTAT_FAILED_CDROMTEST    1
#define MSCDEX_INITSTAT_FAILED_CONTROL      2
#define MSCDEX_INITSTAT_FAILED_ALL (MSCDEX_INITSTAT_FAILED_CDROMTEST|MSCDEX_INITSTAT_FAILED_CONTROL)

#define MSCDEX_INITSTAT_READY_CDROMTEST   4
#define MSCDEX_INITSTAT_READY_CONTROL     8
#define MSCDEX_INITSTAT_READY_READBUFFER  16

#define CD_INFO_TRACK_DATA    0x40

#define CD_OPEN_DOOR  0
#define CD_CLOSE_DOOR 5

typedef struct mscdex_drive_info_s {
	struct cddrive_disk_info_s *diskinfos;
	struct cddrive_track_info_s trackinfo;
	char *readbuffer;
	unsigned int readbufsize;
	unsigned int checknum;
} mscdex_drive_info_s;

static unsigned int mscdex_cdrom_test(void);
unsigned int mpxplay_diskdrive_mscdex_is_drive_cd(unsigned int drive);
static unsigned int mscdex_init_control(void);
static unsigned int mscdex_get_diskinfo(unsigned int drive, unsigned int *firsttrack, unsigned int *lasttrack, unsigned long *lastframeloc);

static unsigned int mscdex_initstat, mscdex_num_initialized_drives;
static unsigned int cd_drives, cd_firstdrive;
static unsigned int mscdex_drivenum_r, mscdex_checknum;
static char *dosmemput1, *dosmemput2;
static short segment1, segment2, segment_cdw;
static dosmem_t dm_cd_control, dm_cd_info, dm_cd_data;

static unsigned int mscdex_init(void)
{
	if(mscdex_initstat & MSCDEX_INITSTAT_FAILED_ALL)
		return CDDRIVE_RETCODE_FAILED;
	if(mscdex_initstat & MSCDEX_INITSTAT_READY_CDROMTEST)
		return CDDRIVE_RETCODE_OK;
	if(!mscdex_cdrom_test()) {
		funcbit_enable(mscdex_initstat, MSCDEX_INITSTAT_FAILED_CDROMTEST);
		return CDDRIVE_RETCODE_FAILED;
	}
	funcbit_enable(mscdex_initstat, MSCDEX_INITSTAT_READY_CDROMTEST);
	return CDDRIVE_RETCODE_OK;
}

static unsigned int MSCDEX_drive_check(unsigned int drivenum)
{
	unsigned int retcode;
	retcode = mscdex_init();
	if(retcode != CDDRIVE_RETCODE_OK)
		return retcode;
	if(!mpxplay_diskdrive_mscdex_is_drive_cd(drivenum))
		return CDDRIVE_RETCODE_FAILED;
	return CDDRIVE_RETCODE_OK;
}

static void *MSCDEX_drive_mount(unsigned int drivenum)
{
	struct mscdex_drive_info_s *mdis;
	unsigned int retcode;

	retcode = mscdex_init();
	if(retcode != CDDRIVE_RETCODE_OK)
		return NULL;
	retcode = mscdex_init_control();
	if(retcode != CDDRIVE_RETCODE_OK)
		return NULL;

	mdis = (struct mscdex_drive_info_s *)calloc(1, sizeof(*mdis));
	if(!mdis)
		return mdis;

	mscdex_num_initialized_drives++;

	return mdis;
}

static void MSCDEX_drive_unmount(void *mdip)
{
	struct mscdex_drive_info_s *mdis = mdip;
	if(mdis)
		free(mdis);
	if(mscdex_num_initialized_drives)
		mscdex_num_initialized_drives--;
	if(!mscdex_num_initialized_drives) {
		pds_dpmi_dos_freemem(&dm_cd_control);
		pds_dpmi_dos_freemem(&dm_cd_info);
		pds_dpmi_dos_freemem(&dm_cd_data);
		mscdex_initstat = 0;
		dosmemput1 = dosmemput2 = NULL;
		segment1 = segment2 = segment_cdw = 0;
	}
}

#if defined(MSCDEX_USE_ASM) && defined(__WATCOMC__)

unsigned int asm_cdrom_test(void);
static unsigned int mscdex_cdrom_test(void)
{
#pragma aux asm_cdrom_test=\
 "mov eax,0x1500"\
 "xor ebx,ebx"\
 "xor ecx,ecx"\
 "int 0x2f"\
 "mov eax,ebx"\
 "test eax,eax"\
 "jz nocd"\
 "cmp ecx,0"\
 "jbe nocd"\
  "mov dword ptr cd_drives,eax"\
  "mov dword ptr cd_firstdrive,ecx"\
 "nocd:"\
 value[eax] modify[ebx ecx edx edi esi];
	return asm_cdrom_test();
}

unsigned int asm_is_drive_cd(unsigned int);
unsigned int mpxplay_diskdrive_mscdex_is_drive_cd(unsigned int drive)
{
#pragma aux asm_is_drive_cd=\
 "cmp cd_firstdrive,0"\
 "jbe cd_no"\
 "cmp eax,cd_firstdrive"\
 "je  cd_ok"\
 "cmp cd_drives,1"\
 "je  cd_no"\
 "mov ecx,eax"\
 "mov eax,0x150b"\
 "xor ebx,ebx"\
 "int 0x2f"\
 "cmp bx,0xadad"\
 "jne cd_no"\
 "test ax,ax"\
 "jnz cd_ok"\
 "cd_no:"\
  "xor eax,eax"\
  "jmp cd_end"\
 "cd_ok:mov eax,1"\
 "cd_end:"\
 parm[eax] value[eax] modify[ebx ecx edx edi esi];
	return asm_is_drive_cd(drive);
}

#else							// MSCDEX_USE_ASM

static unsigned int mscdex_cdrom_test(void)
{
	union REGS regs;
	pds_memset(&regs, 0, sizeof(union REGS));
	regs.w.ax = 0x1500;
	int386(0x2f, &regs, &regs);
	if(regs.w.bx && (regs.w.cx > 0)) {
		cd_drives = regs.w.bx;
		cd_firstdrive = regs.w.cx;
	}
	return ((unsigned int)regs.w.bx);
}

unsigned int mpxplay_diskdrive_mscdex_is_drive_cd(unsigned int drive)
{
	union REGS regs;
	if(cd_firstdrive <= 0)
		return CDDRIVE_RETCODE_FAILED;
	if(drive == cd_firstdrive)
		return 1;
	if(cd_drives == 1)
		return 0;
	pds_memset(&regs, 0, sizeof(union REGS));
	regs.w.ax = 0x150b;
	regs.w.cx = drive;
	int386(0x2f, &regs, &regs);
	if((regs.w.bx == 0xadad) && regs.w.ax)
		return 1;
	return 0;
}

#endif							// MSCDEX_USE_ASM

static unsigned int mscdex_init_control(void)
{
	if(mscdex_initstat & MSCDEX_INITSTAT_FAILED_ALL)
		return CDDRIVE_RETCODE_FAILED;

	if(!(mscdex_initstat & MSCDEX_INITSTAT_READY_CONTROL)) {
		funcbit_enable(mscdex_initstat, MSCDEX_INITSTAT_FAILED_CONTROL);

		if(!pds_dpmi_dos_allocmem(&dm_cd_control, 256))
			return CDDRIVE_RETCODE_FAILED;
		if(!pds_dpmi_dos_allocmem(&dm_cd_info, 256))
			return CDDRIVE_RETCODE_FAILED;

		// if we don't do this, we get : Memory allocation error, system halted...
		dm_cd_control.linearptr += 16;
		dm_cd_control.segment++;
		dm_cd_info.linearptr += 16;
		dm_cd_info.segment++;

		dosmemput1 = dm_cd_control.linearptr;
		dosmemput2 = dm_cd_info.linearptr;
		segment1 = dm_cd_control.segment;
		segment2 = dm_cd_info.segment;

		funcbit_disable(mscdex_initstat, MSCDEX_INITSTAT_FAILED_CONTROL);
		funcbit_enable(mscdex_initstat, MSCDEX_INITSTAT_READY_CONTROL);
	}
	return CDDRIVE_RETCODE_OK;
}

static unsigned int mscdex_init_readbuf(struct mscdex_drive_info_s *mdis)
{
	if(!(mscdex_initstat & MSCDEX_INITSTAT_READY_READBUFFER)) {
		if(!mscdex_init_control())
			return CDDRIVE_RETCODE_FAILED;
		if(!pds_dpmi_dos_allocmem(&dm_cd_data, CD_READBUF_SIZE + CD_SYNC_SIZE + 32))
			return CDDRIVE_RETCODE_FAILED;

		dm_cd_data.linearptr += 16;
		dm_cd_data.segment++;

		segment_cdw = dm_cd_data.segment;
		funcbit_enable(mscdex_initstat, MSCDEX_INITSTAT_READY_READBUFFER);
	}
	mdis->readbuffer = dm_cd_data.linearptr;
	mdis->readbufsize = CD_READBUF_SIZE + CD_SYNC_SIZE;
	return CDDRIVE_RETCODE_OK;
}

#pragma pack(push,1)

struct mscd_ioctl_s {
	unsigned char len;
	unsigned char subunit;
	unsigned char command;
	unsigned short status;
	unsigned char reserved[8];
	unsigned char mediadesc;
	unsigned short transoff;
	unsigned short transseg;
	unsigned short numbytes;
	unsigned short startsec;
	unsigned long unused;
};

struct mscd_statusinfo_s {
	unsigned char control;
	unsigned long status;
};

struct mscd_readctl_s {
	unsigned char len;
	unsigned char subunit;
	unsigned char command;
	unsigned short status;
	unsigned char reserved[8];
	unsigned char mode;
	unsigned short transoff;
	unsigned short transseg;
	unsigned short secnum;
	unsigned long loc;
	unsigned char readmode;
	unsigned char skip[2];
};

struct mscd_diskinfo_s {
	unsigned char control;
	unsigned char lowest;
	unsigned char highest;
	unsigned long total;
};

struct mscd_trackinfo_s {
	unsigned char control;
	unsigned char track;
	unsigned long loc;
	unsigned char info;
};

#pragma pack(pop)

static unsigned int cd_device_command(unsigned int drive, unsigned int command, void *info2, unsigned int inf2size)
{
	struct mscd_ioctl_s cdinfo_device;
	struct rminfo RMI;

	pds_dpmi_rmi_clear(&RMI);
	pds_memset(&cdinfo_device, 0, sizeof(struct mscd_ioctl_s));

	cdinfo_device.len = sizeof(struct mscd_ioctl_s);
	cdinfo_device.command = command;
	cdinfo_device.transseg = segment2;
	cdinfo_device.numbytes = inf2size;

	pds_memcpy(dosmemput1, &cdinfo_device, sizeof(struct mscd_ioctl_s));
	pds_memcpy(dosmemput2, info2, inf2size);
	RMI.EAX = 0x00001510;
	RMI.ECX = drive;
	RMI.ES = segment1;
	pds_dpmi_realmodeint_call(0x2f, &RMI);
	pds_memcpy(&cdinfo_device, dosmemput1, sizeof(struct mscd_ioctl_s));
	pds_memcpy(info2, dosmemput2, inf2size);
	return cdinfo_device.status;
}

static unsigned int cd_device_info(unsigned int drive, void *info2, unsigned int inf2size)
{
	unsigned int status, counter = 3;
	do {
		status = cd_device_command(drive, 3, info2, inf2size);
		status &= 0x100;
	} while(!status && (--counter));
	return status;
}

static unsigned long Red2Sierra(unsigned long locstr)
{
	unsigned long min, sec, frame;

	min = (locstr >> 16) & 0xff;
	sec = (locstr >> 8) & 0xff;
	frame = locstr & 0xff;
	return min * 75 * 60 + sec * 75 + frame - 150;
}

static unsigned int MSCDEX_get_diskinfo(void *mdip, unsigned int drivenum, struct cddrive_disk_info_s *diskinfos)
{
	struct mscdex_drive_info_s *mdis = mdip;
	struct mscd_diskinfo_s cdinfo_disk;
	if(!mdis)
		return CDDRIVE_RETCODE_FAILED;
	mdis->diskinfos = diskinfos;

	pds_memset(&cdinfo_disk, 0, sizeof(struct mscd_diskinfo_s));
	cdinfo_disk.control = 10;
	if(!cd_device_info(drivenum, &cdinfo_disk, sizeof(struct mscd_diskinfo_s)))
		return CDDRIVE_RETCODE_FAILED;
	if((cdinfo_disk.lowest <= 0) || (cdinfo_disk.highest <= 0) || (cdinfo_disk.lowest > cdinfo_disk.highest))
		return CDDRIVE_RETCODE_FAILED;
	diskinfos->firsttrack = cdinfo_disk.lowest;
	diskinfos->lasttrack = cdinfo_disk.highest;
	diskinfos->nb_frames = Red2Sierra(cdinfo_disk.total) - 1;
	return CDDRIVE_RETCODE_OK;
}

static unsigned int MSCDEX_get_trackinfos(void *mdip, unsigned int drivenum, struct cddrive_track_info_s *trackinfos)
{
	struct mscdex_drive_info_s *mdis = mdip;
	struct cddrive_disk_info_s *diskinfos;
	unsigned int i;
	struct mscd_trackinfo_s cdinfo_track;

	if(!mdis)
		return CDDRIVE_RETCODE_FAILED;
	diskinfos = mdis->diskinfos;
	if(!diskinfos || !diskinfos->nb_tracks)
		return CDDRIVE_RETCODE_FAILED;
	for(i = diskinfos->firsttrack; i <= diskinfos->lasttrack; i++) {
		pds_memset(&cdinfo_track, 0, sizeof(struct mscd_trackinfo_s));
		cdinfo_track.control = 11;
		cdinfo_track.track = i;
		if(!cd_device_info(drivenum, &cdinfo_track, sizeof(struct mscd_trackinfo_s)))
			return CDDRIVE_RETCODE_FAILED;
		trackinfos->tracknum = i;
		trackinfos->tracktype = (cdinfo_track.info & CD_INFO_TRACK_DATA) ? CDDRIVE_TRACKTYPE_DATA : CDDRIVE_TRACKTYPE_AUDIO;
		trackinfos->framepos_begin = Red2Sierra(cdinfo_track.loc);
		trackinfos++;
	}
	return CDDRIVE_RETCODE_OK;
}

static unsigned int MSCDEX_start_read(void *mdip, unsigned int drivenum, struct cddrive_track_info_s *trackinfo)
{
	struct mscdex_drive_info_s *mdis = mdip;
	if(!mdis)
		return CDDRIVE_RETCODE_FAILED;
	if(!mscdex_init_readbuf(mdis))
		return CDDRIVE_RETCODE_FAILED;
	pds_memcpy(&mdis->trackinfo, trackinfo, sizeof(struct cddrive_track_info_s));
	mscdex_checknum++;
	mdis->checknum = mscdex_checknum;
	if(mscdex_drivenum_r != drivenum) {
		mscdex_drivenum_r = drivenum;
		return CDDRIVE_RETCODE_RESET_READ;
	}
	return CDDRIVE_RETCODE_OK;
}

static unsigned int MSCDEX_read_sectors_cda(void *mdip, char *buffer, unsigned long sectorpos, unsigned long sectornum)
{
	struct mscdex_drive_info_s *mdis = mdip;
	struct mscd_readctl_s cdinfo_read;
	struct rminfo RMI;

	if(!mdis)
		return CDDRIVE_RETCODE_FAILED;
	if(mdis->checknum != mscdex_checknum)	// we read the last opened track only
		return CDDRIVE_RETCODE_FAILED;

	pds_dpmi_rmi_clear(&RMI);
	pds_memset(&cdinfo_read, 0, sizeof(struct mscd_readctl_s));

	cdinfo_read.len = sizeof(struct mscd_readctl_s);
	cdinfo_read.command = 128;
	cdinfo_read.transseg = segment_cdw;
	cdinfo_read.loc = sectorpos;
	cdinfo_read.secnum = sectornum;
	cdinfo_read.readmode = 1;

	pds_memcpy(dosmemput1, &cdinfo_read, sizeof(struct mscd_readctl_s));
	RMI.EAX = 0x00001510;
	RMI.ECX = mscdex_drivenum_r;
	RMI.ES = segment1;
	pds_dpmi_realmodeint_call(0x2f, &RMI);
	pds_memcpy(&cdinfo_read, dosmemput1, sizeof(struct mscd_readctl_s));
	if(cdinfo_read.status & 0x100) {
		pds_memcpy(buffer, mdis->readbuffer, sectornum * CD_FRAME_SIZE);
		return CDDRIVE_RETCODE_OK;
	}
	return CDDRIVE_RETCODE_FAILED;
}

static unsigned int MSCDEX_load_unload_media(void *mdip, unsigned int drivenum)
{
	struct mscd_statusinfo_s cdinfo_status;
	if(!mscdex_init_control())
		return CDDRIVE_RETCODE_FAILED;

	cdinfo_status.control = 6;
	cdinfo_status.status = 0;
	if(!cd_device_info(drivenum, &cdinfo_status, sizeof(struct mscd_statusinfo_s)))
		return CDDRIVE_RETCODE_FAILED;
	if(cdinfo_status.status & 0x1) {	// door is open, so close it
		cdinfo_status.control = CD_CLOSE_DOOR;
		cd_device_command(drivenum, 12, &cdinfo_status, sizeof(struct mscd_statusinfo_s));
	} else {					// door is closed, so open it
		cdinfo_status.control = CD_OPEN_DOOR;
		cd_device_command(drivenum, 12, &cdinfo_status, sizeof(struct mscd_statusinfo_s));
	}
	return CDDRIVE_RETCODE_OK;
}

struct cddrive_lowlevel_func_s MSCDEX_lowlevel_funcs = {
	&MSCDEX_drive_check,
	&MSCDEX_drive_mount,
	&MSCDEX_drive_unmount,
	&MSCDEX_get_diskinfo,
	&MSCDEX_get_trackinfos,
	&MSCDEX_start_read,
	&MSCDEX_read_sectors_cda,
	NULL,
	&MSCDEX_load_unload_media,
	NULL
};

#endif							// __DOS__
