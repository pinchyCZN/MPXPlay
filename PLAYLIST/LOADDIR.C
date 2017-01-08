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
//function:directory routines

#include "newfunc\newfunc.h"
#include "control\control.h"
#include "display\display.h"
#include "diskdriv\diskdriv.h"
#include "playlist.h"
#include <string.h>

#define SHOW_DRIVE_VOLUME_NAMES 1	// in playlist editor

#define MAX_DIRECTORY_DEPTH 32
#define LOADDIR_MAX_LOCAL_DRIVES   ('Z'-'A'+1)
#define LOADDIR_MAX_VIRTUAL_DRIVES ('7'-'0'+1)
#define LOADDIR_MAX_DRIVES (LOADDIR_MAX_LOCAL_DRIVES+LOADDIR_MAX_VIRTUAL_DRIVES)

#define LOADDIR_FIRSTDRIVE        0	// A:
#define LOADDIR_FIRSTDRV_BROWSER  2	// C:
#define LOADDIR_FIRSTDRV_VIRTUAL  LOADDIR_MAX_LOCAL_DRIVES	// 0:

#define LOADDIR_DRIVE_DRIVENAME_LEN   3
#define LOADDIR_DRIVE_VOLUMENAME_LEN 31

typedef struct mpxplay_drivemap_info_t {
	unsigned int type;
	struct mpxplay_diskdrive_data_s mdds;
	char drivename[LOADDIR_DRIVE_DRIVENAME_LEN + 1];	// have to be filled (with "D:")
	char volumename[LOADDIR_DRIVE_VOLUMENAME_LEN + 1];
} mpxplay_drivemap_info_t;

static struct playlist_entry_info *browser_get_drives(struct playlist_entry_info *, char **);
static struct playlist_entry_info *browser_get_directories(struct playlist_entry_info *, struct playlist_side_info *, char **);
static struct playlist_entry_info *browser_get_files_from_dir(struct playlist_entry_info *, struct playlist_side_info *, char **);
static void loaddir_selectdrive_playlist_to_sublist(struct playlist_side_info *psi);
static void playlist_loaddir_get_driveinfos(void);

extern unsigned int displaymode, desktopmode, refdisp;

static unsigned int loaddir_lastvaliddrive;
static char allscanstring[16] = { 'd', ':', PDS_DIRECTORY_SEPARATOR_CHAR, '*', '.', '*', PDS_DIRECTORY_SEPARATOR_CHAR, '*', '.', '*', 0 };
static mpxplay_drivemap_info_t drivemap_infos[LOADDIR_MAX_DRIVES];

static void loaddir_build_drivename(unsigned int drivenum, char *drivename)
{
	if(drivenum < LOADDIR_MAX_DRIVES) {
		if(drivenum >= LOADDIR_FIRSTDRV_VIRTUAL)
			drivename[0] = '0' + (drivenum - LOADDIR_FIRSTDRV_VIRTUAL);
		else
			drivename[0] = 'A' + drivenum;
		drivename[1] = ':';
		drivename[2] = 0;
	} else
		drivename[0] = 0;
}

static void loaddir_build_rootdir(unsigned int drivenum, char *rootdir)
{
	loaddir_build_drivename(drivenum, rootdir);
	rootdir[2] = PDS_DIRECTORY_SEPARATOR_CHAR;
	rootdir[3] = 0;
}

static void loaddir_clear_driveinfos(unsigned int all)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[0];
	unsigned int i;
	for(i = 0; i < ((all) ? LOADDIR_MAX_DRIVES : LOADDIR_MAX_LOCAL_DRIVES); i++) {
		di->type = DRIVE_TYPE_NONE;
		di->drivename[0] = 0;
		di->volumename[0] = 0;
		di++;
	}
	if(all || (loaddir_lastvaliddrive < LOADDIR_MAX_LOCAL_DRIVES))
		loaddir_lastvaliddrive = 0;
}

static void loaddir_diskdrives_mount_localdisks(void)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[0];
	unsigned int i;

	for(i = 0; i < LOADDIR_MAX_DRIVES; i++) {
		if(di->type != DRIVE_TYPE_NONE) {
			if(!di->mdds.mdfs) {
				di->mdds.mdfs = mpxplay_diskdrive_search_driver(&di->drivename[0]);
				mpxplay_diskdrive_drive_mount(&di->mdds, &di->drivename[0]);
			}
		} else
			mpxplay_diskdrive_drive_unmount(&di->mdds);
		di++;
	}
}

void playlist_loaddir_diskdrives_unmount(void)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[0];
	unsigned int i;

	for(i = 0; i < LOADDIR_MAX_DRIVES; i++) {
		mpxplay_diskdrive_drive_unmount(&di->mdds);
		di++;
	}
	mpxplay_diskdrive_alldrives_close();
	loaddir_clear_driveinfos(1);
}

struct mpxplay_diskdrive_data_s *playlist_loaddir_drivenum_to_drivemap(int drivenum)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[0];

	if((drivenum < 0) || (drivenum >= LOADDIR_MAX_DRIVES))
		return NULL;

	di = &drivemap_infos[drivenum];
	if(!di->mdds.mdfs) {
		loaddir_build_drivename(drivenum, di->drivename);
		di->mdds.mdfs = mpxplay_diskdrive_search_driver(&di->drivename[0]);
		if(!di->mdds.mdfs)
			return NULL;
		if(!mpxplay_diskdrive_drive_mount(&di->mdds, &di->drivename[0]))
			return NULL;
	}
	return (&di->mdds);
}

static unsigned int loaddir_driveinfo_get_drivenum(mpxplay_drivemap_info_t * di)
{
	if((di >= &drivemap_infos[0]) && (di <= &drivemap_infos[LOADDIR_MAX_DRIVES]))
		return (di - (&drivemap_infos[0]));
	return 0;					// ??? 'a'
}

static mpxplay_drivemap_info_t *loaddir_driveinfo_search_free_entry(unsigned int start, unsigned int retry)
{
	int i, end = LOADDIR_MAX_DRIVES, step = 1;
	retry++;
	do {
		mpxplay_drivemap_info_t *di = &drivemap_infos[start];
		for(i = start; i != end; i += step) {
			if(di->type == DRIVE_TYPE_NONE)
				return di;
			di += step;
		}
		end = -1;
		step = -1;
	} while(--retry);
	return NULL;
}

static struct mpxplay_diskdrive_data_s *playlist_loaddir_search_driverhandname_in_drivemap(char *driverhandname)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[0];
	unsigned int i;

	for(i = 0; i < LOADDIR_MAX_DRIVES; i++) {
		if(di->mdds.mdfs && (pds_stricmp(di->mdds.mdfs->drivername, driverhandname) == 0))
			return (&di->mdds);
		di++;
	}
	return NULL;
}

//-------------------------------------------------------------------------
#define LOADDIR_VIRTUALDRIVE_RETRYMS 20000	// 20 sec

typedef struct mpxplay_virtualdrivemount_s {
	struct playlist_side_info *psi;
	void *tw;
	unsigned int ti;
	unsigned int fullinitside;
	unsigned int retry;
	unsigned int allocated;
	mpxp_uint64_t count_end;
	char path[MAX_PATHNAMELEN];
	char errortext[MAX_PATHNAMELEN];
} mpxplay_virtualdrivemount_s;

static unsigned int playlist_loaddir_virtualdrive_mount(struct mpxplay_virtualdrivemount_s *vdm);

static display_textwin_button_t buttons_error_vdm[] = {
	{" Retry ", 0x1372},		// 'r'
	{"", 0x1352},				// 'R'
	{" Cancel ", 0x2e63},		// 'c'
	{"", 0x2e43},				// 'C'
	{"", KEY_ESC},				// ESC
	{NULL, 0}
};

static void playlist_loaddir_vdmretry_keyhand(struct mpxplay_virtualdrivemount_s *vdm, unsigned int extkey)
{
	switch (extkey) {
	case 0x1372:
	case 0x1352:
		vdm->count_end = 0;
		mpxplay_timer_addfunc(playlist_loaddir_virtualdrive_mount, vdm, MPXPLAY_TIMERFLAG_INDOS, 0);
		break;
	case 0x2e63:
	case 0x2e43:
	case KEY_ESC:
		mpxplay_timer_deletefunc(playlist_loaddir_virtualdrive_mount, vdm);
		display_textwin_closewindow_buttons(vdm->tw);
		if(vdm->allocated)
			free(vdm);
	}
}

static unsigned int playlist_loaddir_virtualdrive_mount(struct mpxplay_virtualdrivemount_s *vdm)
{
	mpxplay_drivemap_info_t *di;
	struct mpxplay_virtualdrivemount_s *vdma;
	struct playlist_side_info *psi = vdm->psi;
	unsigned long len;
	char strtmp[MAX_PATHNAMELEN];

	if(vdm->count_end)
		goto err_out_vdm;

	di = loaddir_driveinfo_search_free_entry(LOADDIR_FIRSTDRV_VIRTUAL, 0);
	if(!di) {
		snprintf(vdm->errortext, sizeof(vdm->errortext), "No free slot to mount drive!");
		vdm->retry = 0;
		goto err_out_vdm;
	}
	di->mdds.mdfs = mpxplay_diskdrive_search_driver(vdm->path);
	if(!mpxplay_diskdrive_drive_mount(&di->mdds, vdm->path)) {
		snprintf(vdm->errortext, sizeof(vdm->errortext), "Couldn't mount/connect drive!");
		goto err_out_vdm;
	}
	mpxplay_diskdrive_getcwd(&di->mdds, strtmp, sizeof(strtmp));
	if(!strtmp[0]) {
		mpxplay_diskdrive_drive_unmount(&di->mdds);
		snprintf(vdm->errortext, sizeof(vdm->errortext), "Couldn't get directory info! Closing connection...\n%s", vdm->path);
		vdm->retry = 0;
		goto err_out_vdm;
	}
	di->type = DRIVE_TYPE_VIRTUAL;
	psi->mdds = &di->mdds;
	len = LOADDIR_DRIVE_DRIVENAME_LEN;
	mpxplay_diskdrive_drive_config(psi->mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVLETTERSTR, (void *)&di->drivename[0], &len);
	len = LOADDIR_DRIVE_VOLUMENAME_LEN;
	mpxplay_diskdrive_drive_config(psi->mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVTYPENAME, (void *)&di->volumename[0], &len);

	psi->currdrive = loaddir_driveinfo_get_drivenum(di);
	if(loaddir_lastvaliddrive < psi->currdrive)
		loaddir_lastvaliddrive = psi->currdrive;
	pds_strcpy(psi->currdir, strtmp);

	if(vdm->fullinitside) {
		funcbit_disable(psi->editsidetype, (~PLT_DIRECTORY));	// ???
		playlist_disable_side_list(psi);
		if(!(psi->editsidetype & PLT_DIRECTORY)) {
			loaddir_selectdrive_playlist_to_sublist(psi->psio);
			psi->editsidetype = PLT_DIRECTORY;
			psi->editloadtype = 0;
		}
		playlist_jukebox_set(psi->mvp, 0);
		playlist_loaddir_buildbrowser(psi);
		playlist_id3list_load(psi->mvp, psi);
		playlist_chkfile_start_norm(psi, 0);
		refdisp |= RDT_EDITOR;
		if(psi == psi->mvp->psip)
			refdisp |= RDT_BROWSER;
	}

	mpxplay_timer_deletefunc(playlist_loaddir_virtualdrive_mount, vdm);
	display_textwin_closewindow_buttons(vdm->tw);
	if(vdm->allocated)
		free(vdm);

	return 1;

  err_out_vdm:
	if(vdm->retry) {
		unsigned int flags, leftsec;
		mpxp_uint64_t currtime;
		if(!vdm->allocated) {
			vdma = calloc(1, sizeof(*vdma));
			if(!vdma) {
				display_textwin_openwindow_errormsg_ok(" Virtual drive error ", vdm->errortext);
				return 0;
			}
			pds_memcpy(vdma, vdm, sizeof(*vdm));
			vdma->allocated = 1;
			vdm = vdma;
		}

		currtime = pds_gettimem();
		if(!vdm->count_end) {
			vdm->count_end = currtime + LOADDIR_VIRTUALDRIVE_RETRYMS;
			leftsec = LOADDIR_VIRTUALDRIVE_RETRYMS / 1000;
			mpxplay_timer_addfunc(playlist_loaddir_virtualdrive_mount, vdm, MPXPLAY_TIMERTYPE_REPEAT | MPXPLAY_TIMERFLAG_INDOS, mpxplay_timer_secs_to_counternum(1) / 5);
		} else {
			if(currtime < vdm->count_end)
				leftsec = (unsigned int)((vdm->count_end - currtime + 900) / 1000);
			else {
				leftsec = 0;
				vdm->count_end = 0;
			}
		}
		sprintf(strtmp, "(waiting to auto retry: %d sec) ", leftsec);

		if(vdm->tw) {
			display_textwin_update_msg(vdm->tw, vdm->ti, strtmp);
		} else {
			flags = TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_ERRORMSG | TEXTWIN_FLAG_DONTCLOSE;
			vdm->tw = display_textwin_allocwindow_items(vdm->tw, flags, " Mount drive ", playlist_loaddir_vdmretry_keyhand, vdm);
			display_textwin_additem_msg_alloc(vdm->tw, flags, 0, -1, vdm->errortext);
			display_textwin_additem_editline(vdm->tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, 0, -1, 40, &vdm->path[0], 120);
			vdm->ti = display_textwin_additem_msg_alloc(vdm->tw, flags, 0, -1, strtmp);
			display_textwin_additem_separatorline(vdm->tw, -1);
			display_textwin_additem_buttons(vdm->tw, flags, 0, -1, buttons_error_vdm, buttons_error_vdm);
			display_textwin_openwindow_items(vdm->tw, 0, 0, 0);
		}
	} else {
		mpxplay_timer_deletefunc(playlist_loaddir_virtualdrive_mount, vdm);
		display_textwin_closewindow_buttons(vdm->tw);
		display_textwin_openwindow_errormsg_ok(" Virtual drive error ", vdm->errortext);
		if(vdm->allocated)
			free(vdm);
	}
	return 0;
}

static void playlist_loaddir_drive_unmount(struct playlist_side_info *psi, struct mpxplay_diskdrive_data_s *mdds)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[LOADDIR_FIRSTDRIVE];
	unsigned int i, bothsides = 0;

	for(i = LOADDIR_FIRSTDRIVE; i < LOADDIR_MAX_DRIVES; i++) {
		if(mdds == (&di->mdds)) {
			di->type = DRIVE_TYPE_NONE;
			break;
		}
		di++;
	}
	mpxplay_diskdrive_drive_unmount(mdds);

	if(!psi)
		return;
	if(psi->psio->mdds == mdds)
		bothsides = 1;
	else if(psi->psio->editsidetype & PLT_DIRECTORY) {
		psi->currdrive = psi->psio->currdrive;
		psi->mdds = psi->psio->mdds;
		pds_strcpy(psi->currdir, psi->psio->currdir);
		if(mpxplay_diskdrive_checkdir(psi->mdds, psi->currdir))
			goto make_dirside;
		loaddir_build_rootdir(psi->currdrive, psi->currdir);
		mpxplay_diskdrive_chdir(psi->mdds, psi->currdir);
		if(mpxplay_diskdrive_checkdir(psi->mdds, psi->currdir))
			goto make_dirside;
	}

	playlist_loaddir_get_driveinfos();
	di = &drivemap_infos[LOADDIR_FIRSTDRV_BROWSER];
	for(i = LOADDIR_FIRSTDRV_BROWSER; i < LOADDIR_MAX_DRIVES; i++) {
		if(di->type != DRIVE_TYPE_NONE) {
			psi->currdrive = i;
			psi->mdds = playlist_loaddir_drivenum_to_drivemap(i);
			if(psi->mdds->lastdir[0]) {
				pds_strcpy(psi->currdir, psi->mdds->lastdir);
				if(mpxplay_diskdrive_checkdir(psi->mdds, psi->currdir))
					break;
			}
			mpxplay_diskdrive_getcwd(psi->mdds, psi->currdir, sizeof(psi->currdir));
			if(mpxplay_diskdrive_checkdir(psi->mdds, psi->currdir))
				break;
		}
		di++;
	}
	if(i >= LOADDIR_MAX_DRIVES) {
		mpxplay_timer_addfunc(playlist_loaddir_select_drive_retry, psi, MPXPLAY_TIMERTYPE_WAKEUP, 0);
		return;
	}

  make_dirside:
	if(bothsides) {
		psi->psio->currdrive = psi->currdrive;
		psi->psio->mdds = psi->mdds;
		pds_strcpy(psi->psio->currdir, psi->currdir);
	}
	for(i = 0; i <= bothsides; i++) {
		funcbit_disable(psi->editsidetype, (~PLT_DIRECTORY));	// ???
		playlist_disable_side_list(psi);
		if(!(psi->editsidetype & PLT_DIRECTORY)) {
			loaddir_selectdrive_playlist_to_sublist(psi->psio);
			psi->editsidetype = PLT_DIRECTORY;
			psi->editloadtype = 0;
		}
		playlist_jukebox_set(psi->mvp, 0);
		playlist_loaddir_buildbrowser(psi);
		playlist_id3list_load(psi->mvp, psi);
		playlist_chkfile_start_norm(psi, 0);
		if(psi == psi->mvp->psip)
			refdisp |= RDT_BROWSER;
		if(bothsides)
			psi = psi->psio;
	}
	refdisp |= RDT_EDITOR;
}

// must match with browser_get_drives function!
static void loaddir_get_currdrivename(unsigned int drivenum, char *fnp)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[LOADDIR_FIRSTDRV_BROWSER];
	unsigned int i;
	for(i = LOADDIR_FIRSTDRV_BROWSER; i <= loaddir_lastvaliddrive; i++) {
		if((di->type != DRIVE_TYPE_NONE) && (pds_getdrivenum_from_path(di->drivename) == drivenum)) {
			fnp += pds_strcpy(fnp, di->drivename);
#ifdef SHOW_DRIVE_VOLUME_NAMES
			if(di->volumename[0]) {
				*fnp++ = ' ';
				fnp += pds_strncpy(fnp, di->volumename, 8);
			}
#endif
			*fnp = 0;
			break;
		}
		di++;
	}
}

#ifdef MPXPLAY_WIN32
#include <malloc.h>

static void playlist_loaddir_get_driveinfos(void)
{
	DWORD drivesbits, i, vlen;

	loaddir_clear_driveinfos(0);

	drivesbits = GetLogicalDrives();
	if(!drivesbits)
		return;

	i = LOADDIR_FIRSTDRIVE;
	drivesbits >>= LOADDIR_FIRSTDRIVE;
	do {
		if(drivesbits & 1) {
			mpxplay_drivemap_info_t *di = &drivemap_infos[i];
			di->type = pds_chkdrive(i);
			loaddir_build_drivename(i, di->drivename);
			if((i >= 2) || (di->type != DRIVE_TYPE_FLOPPY)) {
				if(di->type == DRIVE_TYPE_NETWORK) {
					vlen = LOADDIR_DRIVE_VOLUMENAME_LEN;
					WNetGetConnection(di->drivename, &di->volumename[0], &vlen);
				}
				if((di->type != DRIVE_TYPE_NETWORK) || !di->volumename[0])
					pds_drive_getvolumelabel(i, &di->volumename[0], LOADDIR_DRIVE_VOLUMENAME_LEN);
			}
			if(loaddir_lastvaliddrive < i)
				loaddir_lastvaliddrive = i;
		}
		drivesbits >>= 1;
		i++;
	} while(i < LOADDIR_MAX_LOCAL_DRIVES);

	loaddir_diskdrives_mount_localdisks();
}

#else

static mpxplay_drivemap_info_t *loaddir_driveinfo_search_drivename(char *drvname)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[0];
	unsigned int i;
	for(i = 0; i <= loaddir_lastvaliddrive; i++) {
		if(di->type != DRIVE_TYPE_NONE)
			if(pds_stricmp(drvname, di->drivename) == 0)
				return di;
		di++;
	}
	return NULL;
}

static void playlist_loaddir_get_driveinfos(void)
{
	unsigned int i;
	mpxplay_drivemap_info_t *di;
	char drivename[LOADDIR_DRIVE_DRIVENAME_LEN + 1], volumename[LOADDIR_DRIVE_VOLUMENAME_LEN + 1];

	loaddir_clear_driveinfos(0);

	// check drives
	di = &drivemap_infos[LOADDIR_FIRSTDRIVE];
	for(i = LOADDIR_FIRSTDRIVE; i < LOADDIR_MAX_LOCAL_DRIVES; i++, di++) {
		di->type = pds_chkdrive(i);
		if(di->type != DRIVE_TYPE_NONE) {
			loaddir_build_drivename(i, di->drivename);
			if((di->type == DRIVE_TYPE_HD) || (di->type == DRIVE_TYPE_CD))
				pds_drive_getvolumelabel(i, &di->volumename[0], LOADDIR_DRIVE_VOLUMENAME_LEN);
			if(loaddir_lastvaliddrive < i)
				loaddir_lastvaliddrive = i;
		}
	}

	// check network
	if(!pds_network_check())
		return;

	// get network volume names
	for(i = 0; i < LOADDIR_MAX_DRIVES; i++) {
		int retcode;
		drivename[0] = volumename[0] = 0;
		retcode = pds_network_query_assign(i, drivename, LOADDIR_DRIVE_DRIVENAME_LEN, volumename, LOADDIR_DRIVE_VOLUMENAME_LEN);
		if(retcode == -1)		// ??? end of assign list
			break;
		if(retcode == 0) {		// no error
			di = loaddir_driveinfo_search_drivename(drivename);	// does this drive exist in the drivemap_infos already?
			if(!di) {
				int drivenum = pds_getdrivenum_from_path(drivename);
				if(drivenum < 0)
					drivenum = 0;
				di = loaddir_driveinfo_search_free_entry(drivenum, 1);	// if not, search a free entry
				if(!di)
					break;
				drivenum = loaddir_driveinfo_get_drivenum(di);
				if(loaddir_lastvaliddrive < drivenum)
					loaddir_lastvaliddrive = drivenum;
			}
			di->type = DRIVE_TYPE_NETWORK;
			pds_strcpy(di->drivename, drivename);
			if(volumename[0])	// from network_query
				pds_strcpy(di->volumename, volumename);
			else {
				int drivenum = pds_getdrivenum_from_path(drivename);
				if(drivenum >= 0)
					pds_drive_getvolumelabel(i, &di->volumename[0], LOADDIR_DRIVE_VOLUMENAME_LEN);	// from DOS
			}
		}
	}

	loaddir_diskdrives_mount_localdisks();
}
#endif

static unsigned int loaddir_scansubdirs(struct playlist_side_info *psi, char *searchpath)
{
	struct playlist_entry_info *pei;
	unsigned int filecount = 0, scansupportedfilenamesonly;
	char *filenamesfieldp, *ext;
	struct mpxplay_diskdrive_data_s *mdds;
	struct pds_subdirscan_t dsi;
	char sout[64];

	if(!searchpath || !searchpath[0])
		return 0;

	pei = psi->lastentry + 1;
	filenamesfieldp = psi->filenameslastp;

	mdds = playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(searchpath));
	if(!mdds) {					// possible virtual drive path (only single files work!)
		if(!mpxplay_diskdrive_search_driver(searchpath))
			return 0;
		pei->filename = filenamesfieldp;
		filenamesfieldp += pds_strcpy(filenamesfieldp, searchpath) + 1;
		pei++;
		goto finish_scan;
	}

	if(mpxplay_diskdrive_subdirscan_open(mdds, searchpath, _A_NORMAL, &dsi))
		return 0;

	display_clear_timed_message();

	ext = pds_strrchr(searchpath, '.');
	if(!funcbit_test(desktopmode, DTM_EDIT_ALLFILES) && ext && pds_strcmp(ext, ".*") == 0)
		scansupportedfilenamesonly = 1;
	else
		scansupportedfilenamesonly = 0;

	do {
		int fferror = mpxplay_diskdrive_subdirscan_findnextfile(mdds, &dsi);
		if(fferror < 0)
			break;
		if(dsi.flags & SUBDIRSCAN_FLAG_SUBDIR)
			display_message(1, 0, dsi.currdir);
		if((fferror == 0) && !(dsi.ff->attrib & (_A_SUBDIR | _A_VOLID)) && (!scansupportedfilenamesonly || mpxplay_infile_check_extension(dsi.ff->name, mdds))) {
			if((pei > psi->endentry) || (filenamesfieldp >= psi->filenamesendp))
				break;
			pei->filename = filenamesfieldp;
			filenamesfieldp += pds_strcpy(filenamesfieldp, dsi.fullname) + 1;
			pei->filesize = dsi.ff->size;
			pds_memcpy(&pei->filedate, &dsi.ff->fdate, sizeof(pei->filedate));
			pei->mdds = mdds;
			pei++;
			sprintf(sout, "Read filenames (%3d) from ", ++filecount);
			display_message(0, 0, sout);
		}
	} while(pds_look_extgetch() != KEY_ESC);

	mpxplay_diskdrive_subdirscan_close(mdds, &dsi);

  finish_scan:

	if(pei > psi->firstentry)
		playlist_enable_side(psi);
	else
		playlist_disable_side_full(psi);

	psi->lastentry = pei - 1;
	psi->filenameslastp = filenamesfieldp;
	clear_message();

	if(pds_look_extgetch() == KEY_ESC) {
		pds_extgetch();
		return KEY_ESC;
	}
	return 1;
}

void playlist_loaddir_scandrives(struct playlist_side_info *psi, char *searchpath, char *dslp)
{
	char filemask[MAX_PATHNAMELEN];

	if(dslp && dslp[0]) {
		if(searchpath && searchpath[0]) {
			unsigned int len = pds_strcpy(filemask, "d:");
			len += pds_strcpy(&filemask[len], PDS_DIRECTORY_SEPARATOR_STR);
			len += pds_strcpy(&filemask[len], PDS_DIRECTORY_ALLDIR_STR);
			len += pds_strcpy(&filemask[len], PDS_DIRECTORY_SEPARATOR_STR);
			pds_strcpy(&filemask[len], pds_getfilename_from_fullname(searchpath));
		} else
			pds_strcpy(filemask, allscanstring);
		do {
			filemask[0] = dslp[0];
			if(loaddir_scansubdirs(psi, filemask) == KEY_ESC)
				break;
			dslp++;
		} while(dslp[0]);
	} else {
		loaddir_scansubdirs(psi, searchpath);
	}
}

//***************************************************************************
// init from [startup]
void playlist_loaddir_initdirside(struct playlist_side_info *psi, char *dir)
{
	int drive = pds_getdrivenum_from_path(dir);
	struct mpxplay_diskdrive_data_s *mdds = playlist_loaddir_drivenum_to_drivemap(drive);
	char newdir[MAX_PATHNAMELEN];
	struct mpxplay_virtualdrivemount_s vdm;

	if(mdds) {					// old drive not exists
		if(!mpxplay_diskdrive_checkdir(mdds, dir)) {	// old path is not exists
			loaddir_build_rootdir(drive, newdir);
			if(mpxplay_diskdrive_checkdir(mdds, newdir))
				dir = &newdir[0];
			else				// root dir not exists/accessible
				dir = NULL;
		}
		if(dir) {
			psi->currdrive = drive;
			psi->mdds = mdds;
			pds_strcpy(psi->currdir, dir);
			pds_strcpy(psi->mdds->lastdir, dir);
		}
	} else {
		pds_memset((char *)(&vdm), 0, sizeof(vdm));
		vdm.psi = psi;
		pds_strcpy(vdm.path, dir);
		playlist_loaddir_virtualdrive_mount(&vdm);
	}							//else using/keeping the start dir
}

//init with the starting directory
void playlist_loaddir_initbrowser(struct mainvars *mvp, char *dir)
{
	struct playlist_side_info *psi = mvp->psi0;
	unsigned int i;

	for(i = 0; i < PLAYLIST_MAX_SIDES; i++, psi++) {
		pds_strcpy(psi->currdir, dir);
		psi->currdrive = pds_getdrivenum_from_path(psi->currdir);
		psi->mdds = playlist_loaddir_drivenum_to_drivemap(psi->currdrive);
		if(psi->mdds)
			pds_strcpy(psi->mdds->lastdir, dir);	// ???
	}
}

void playlist_loaddir_buildbrowser(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	char *filenamesp;

	playlist_disable_side_list(psi);
	playlist_clear_side(psi);
	playlist_enable_side(psi);

	filenamesp = psi->filenameslastp;
	pei = psi->firstentry;
	if(desktopmode & DTM_EDIT_DRIVES)
		pei = browser_get_drives(pei, &filenamesp);

	pei = browser_get_directories(pei, psi, &filenamesp);
	pei = browser_get_files_from_dir(pei, psi, &filenamesp);	// load all supported filetypes
	psi->lastentry = pei - 1;
	if(pei > psi->firstentry) {
		playlist_order_dft(psi);	// order directories & filenames
		psi->filenameslastp = filenamesp;
		playlist_search_firstsong(psi);
	}
}

static struct playlist_entry_info *browser_get_drives(struct playlist_entry_info *pei, char **filenamesfieldp)
{
	mpxplay_drivemap_info_t *di = &drivemap_infos[LOADDIR_FIRSTDRV_BROWSER];
	char *fnp = *filenamesfieldp;
	unsigned int i;

	if(!loaddir_lastvaliddrive)
		playlist_loaddir_get_driveinfos();
	if(loaddir_lastvaliddrive <= LOADDIR_FIRSTDRV_BROWSER)
		return pei;

	for(i = LOADDIR_FIRSTDRV_BROWSER; i <= loaddir_lastvaliddrive; i++) {
		if(di->type != DRIVE_TYPE_NONE) {
			pei->entrytype = DFT_DRIVE;
			pei->filename = fnp;
			fnp += pds_strcpy(fnp, di->drivename);
#ifdef SHOW_DRIVE_VOLUME_NAMES
			if(di->volumename[0]) {
				*fnp++ = ' ';
				fnp += pds_strncpy(fnp, di->volumename, 8);
			}
#endif
			switch (di->type) {
			case DRIVE_TYPE_FLOPPY:
				pei->id3info[I3I_DFT_STORE] = ((i < 2) ? "<FLOPPY>" : "<REMOVABLE>");
				break;
			case DRIVE_TYPE_CD:
				pei->id3info[I3I_DFT_STORE] = "<CD_ROM>";
				break;
			case DRIVE_TYPE_NETWORK:
				pei->id3info[I3I_DFT_STORE] = "<NETWRK>";
				break;
			case DRIVE_TYPE_RAMDISK:
				pei->id3info[I3I_DFT_STORE] = "<RAMDISK>";
				break;
			default:
				pei->id3info[I3I_DFT_STORE] = DFTSTR_DRIVE;
				break;
			}
			*fnp++ = 0;
			pei->mdds = &di->mdds;
			pei++;
		}
		di++;
	}
	*filenamesfieldp = fnp;
	return pei;
}

static struct playlist_entry_info *browser_get_directories(struct playlist_entry_info *pei, struct playlist_side_info *psi, char **filenamesfieldp)
{
	unsigned int error, len;
	struct mpxplay_diskdrive_data_s *mdds = psi->mdds;
	struct pds_find_t ffblk;
	char *fnp, path[MAX_PATHNAMELEN], searchname[MAX_PATHNAMELEN];

	len = pds_strcpy(path, psi->currdir);
	if(path[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR)
		pds_strcpy(&path[len], PDS_DIRECTORY_SEPARATOR_STR);

	fnp = *filenamesfieldp;

	if(!mpxplay_diskdrive_isdirroot(psi->mdds, path)) {
		pei->entrytype = DFT_UPDIR;
		pei->id3info[I3I_DFT_STORE] = DFTSTR_UPDIR;
		pei->filename = fnp;
		fnp += pds_strcpy(fnp, path);
		fnp += pds_strcpy(fnp, "..") + 1;
		pei->mdds = mdds;
		pei++;
	}

	len = pds_strcpy(searchname, path);
	pds_strcpy(&searchname[len], PDS_DIRECTORY_ALLDIR_STR);

	error = mpxplay_diskdrive_findfirst(mdds, searchname, _A_SUBDIR, &ffblk);
	while(!error && (pei <= psi->endentry) && (fnp < psi->filenamesendp)) {
		if((ffblk.attrib & _A_SUBDIR) && pds_strcmp(".", ffblk.name) != 0 && pds_strcmp("..", ffblk.name) != 0) {
			pei->entrytype = DFT_SUBDIR;
			pei->id3info[I3I_DFT_STORE] = DFTSTR_SUBDIR;
			pei->filename = fnp;
			fnp += pds_strcpy(fnp, path);
			fnp += pds_strcpy(fnp, ffblk.name) + 1;
			pds_memcpy(&pei->filedate, &ffblk.fdate, sizeof(pei->filedate));
			pei->mdds = mdds;
			pei++;
		}
		error = mpxplay_diskdrive_findnext(mdds, &ffblk);
	}
	mpxplay_diskdrive_findclose(mdds, &ffblk);
	*filenamesfieldp = fnp;
	return pei;
}

static struct playlist_entry_info *browser_get_files_from_dir(struct playlist_entry_info *pei, struct playlist_side_info *psi, char **filenamesfieldp)
{
	unsigned int error, len;
	struct mpxplay_diskdrive_data_s *mdds = psi->mdds;
	struct pds_find_t ffblk;
	char *fnp, path[MAX_PATHNAMELEN], searchname[MAX_PATHNAMELEN];

	len = pds_strcpy(path, psi->currdir);
	if(path[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR)
		pds_strcpy(&path[len], PDS_DIRECTORY_SEPARATOR_STR);

	len = pds_strcpy(searchname, path);
	pds_strcpy(&searchname[len], PDS_DIRECTORY_ALLFILE_STR);

	fnp = *filenamesfieldp;

	error = mpxplay_diskdrive_findfirst(mdds, searchname, _A_NORMAL, &ffblk);
	while(!error && (pei <= psi->endentry) && (fnp < psi->filenamesendp)) {
		if(!(ffblk.attrib & (_A_SUBDIR | _A_VOLID))) {
			pei->mdds = mdds;
			pei->filesize = ffblk.size;
			if(playlist_loadlist_get_header_by_ext(psi, pei, ffblk.name) || funcbit_test(desktopmode, DTM_EDIT_ALLFILES) || mpxplay_infile_check_extension(ffblk.name, mdds)) {
				pei->filename = fnp;
				fnp += pds_strcpy(fnp, path);
				fnp += pds_strcpy(fnp, ffblk.name) + 1;
				pds_memcpy(&pei->filedate, &ffblk.fdate, sizeof(pei->filedate));
				pei++;
			}
		}
		error = mpxplay_diskdrive_findnext(mdds, &ffblk);
	}
	mpxplay_diskdrive_findclose(mdds, &ffblk);
	*filenamesfieldp = fnp;
	return pei;
}

struct playlist_side_info *playlist_loaddir_changedir(struct playlist_side_info *psi, unsigned long head)
{
	struct playlist_entry_info *pei = psi->editorhighline;
	unsigned int len;
	int drive;
	char lastdir[MAX_PATHNAMELEN], newdir[MAX_PATHNAMELEN];

	lastdir[0] = 0;
	switch (head) {
	case DFT_ROOTDIR:
		pds_filename_build_fullpath(newdir, psi->currdir, PDS_DIRECTORY_SEPARATOR_STR);
		mpxplay_diskdrive_chdir(psi->mdds, newdir);
		mpxplay_diskdrive_getcwd(psi->mdds, psi->currdir, sizeof(psi->currdir));
		break;
	case DFT_DRIVE:
		playlist_loaddir_get_driveinfos();
		loaddir_get_currdrivename(psi->currdrive, lastdir);
		drive = pds_getdrivenum_from_path(pei->filename);
		if(drive < 0)
			return psi;
		psi->currdrive = drive;
		psi->mdds = playlist_loaddir_drivenum_to_drivemap(drive);
		if(psi->mdds->lastdir[0])
			pds_strcpy(psi->currdir, psi->mdds->lastdir);
		else
			mpxplay_diskdrive_getcwd(psi->mdds, psi->currdir, sizeof(psi->currdir));
		break;
	case DFT_SUBDIR:
		if(pei->filename) {
			pds_filename_assemble_fullname(lastdir, pei->filename, "..");
			mpxplay_diskdrive_chdir(psi->mdds, pei->filename);
		}
		mpxplay_diskdrive_getcwd(psi->mdds, psi->currdir, sizeof(psi->currdir));
		break;
	case DFT_UPDIR:
		len = pds_strcpy(lastdir, psi->currdir);
		if(lastdir[len - 1] == PDS_DIRECTORY_SEPARATOR_CHAR)
			lastdir[len - 1] = 0;
		pds_filename_assemble_fullname(newdir, psi->currdir, "..");
		mpxplay_diskdrive_chdir(psi->mdds, newdir);
		mpxplay_diskdrive_getcwd(psi->mdds, psi->currdir, sizeof(psi->currdir));
		break;
	}

	playlist_loaddir_buildbrowser(psi);
	playlist_id3list_load(psi->mvp, psi);
	playlist_search_lastdir(psi, lastdir);
	return psi;
}

unsigned int playlist_loaddir_browser_gotodir(struct playlist_side_info *psi, char *newdir)
{
	if(!newdir || !newdir[0])
		return 0;
	if(pds_stricmp(newdir, psi->currdir) != 0) {
		int drive = pds_getdrivenum_from_path(newdir);
		if(drive < 0)
			return 0;
		if(drive != psi->currdrive) {
			struct mpxplay_diskdrive_data_s *mdds;
			mdds = playlist_loaddir_drivenum_to_drivemap(drive);
			if(!mdds)
				return 0;
			if(!mpxplay_diskdrive_checkdir(mdds, newdir))
				return 0;
			psi->mdds = mdds;
			psi->currdrive = drive;
		}
		mpxplay_diskdrive_chdir(psi->mdds, newdir);
		mpxplay_diskdrive_getcwd(psi->mdds, psi->currdir, sizeof(psi->currdir));
		playlist_loaddir_buildbrowser(psi);
		playlist_id3list_load(psi->mvp, psi);
		playlist_chkfile_start_norm(psi, NULL);
	}
	return 1;
}

void playlist_loaddir_search_paralell_dir(struct playlist_side_info *psi, int step)
{
	unsigned int found;
	struct playlist_entry_info *pei;
	char lastdir[MAX_PATHNAMELEN];

	pds_strcpy(lastdir, psi->currdir);
	playlist_loaddir_changedir(psi, DFT_UPDIR);
	if(pds_stricmp(lastdir, psi->currdir) != 0) {
		found = 0;
		pei = psi->editorhighline;
		pei += step;
		while((step == 1 && pei <= psi->lastentry) || (step == -1 && pei >= psi->firstentry)) {
			if(pei->entrytype == DFT_SUBDIR) {
				psi->editorhighline = pei;
				playlist_loaddir_changedir(psi, DFT_SUBDIR);
				found = 1;
				break;
			}
			pei += step;
		}
		if(!found)
			playlist_loaddir_changedir(psi, DFT_SUBDIR);
	}
	playlist_chkfile_start_norm(psi, 0);
}

//-----------------------------------------------------------------------
static display_textwin_button_t drive_buttons[LOADDIR_MAX_DRIVES * 2 + 1];
static char drive_button_str[LOADDIR_MAX_DRIVES * (sizeof(mpxplay_drivemap_info_t) + 2) + sizeof("9:playlist") + sizeof("8:jukebox") + 32];

static unsigned int loaddir_selectdrive_buildbuttons(struct mainvars *mvp, unsigned int side, display_textwin_button_t ** selected_button)
{
	struct playlist_side_info *psi = mvp->psi0 + side;
	display_textwin_button_t *bt = &drive_buttons[0];
	mpxplay_drivemap_info_t *di = &drivemap_infos[LOADDIR_FIRSTDRIVE];
	char *dbs = &drive_button_str[0];
	unsigned int i;

	playlist_loaddir_get_driveinfos();

	for(i = LOADDIR_FIRSTDRIVE; i <= loaddir_lastvaliddrive; i++) {
		if(di->type != DRIVE_TYPE_NONE) {
			char *vol, drivenum, isdriveletter;
			unsigned int len;
			bt->text = dbs;
			*dbs++ = ' ';
			dbs += pds_strcpy(dbs, di->drivename);
			*dbs++ = ' ';
			if(di->volumename[0])
				vol = &di->volumename[0];
			else {
				switch (di->type) {
				case DRIVE_TYPE_FLOPPY:
					vol = ((i < 2) ? "<FLOPPY>" : "<REMOVABLE>");
					break;
				case DRIVE_TYPE_CD:
					vol = "<CD_ROM>";
					break;
				case DRIVE_TYPE_NETWORK:
					vol = "<NETWRK>";
					break;
				case DRIVE_TYPE_RAMDISK:
					vol = "<RAMDISK>";
					break;
				default:
					vol = DFTSTR_DRIVE;
					break;
				}
			}

			len = pds_strcpy(dbs, vol);
			dbs += len;
			do {
				*dbs++ = ' ';
			} while(++len < 9);	// min length of drive volume names (adds spaces to end)
			*dbs = 0;

			drivenum = di->drivename[0];
			if((drivenum >= 'A') && (drivenum <= 'Z')) {
				drivenum -= 'A';
				isdriveletter = 1;
			} else if((drivenum >= 'a') && (drivenum <= 'z')) {
				drivenum -= 'a';
				isdriveletter = 1;
			} else {			// '0'-'7'
				drivenum -= '0';
				drivenum += LOADDIR_MAX_LOCAL_DRIVES;
				isdriveletter = 0;
			}

			if(drivenum == psi->currdrive)
				*selected_button = bt;

			if(isdriveletter) {
				bt->extkey = newfunc_keyboard_char_to_extkey('A' + drivenum);
				bt++;
				bt->text = dbs;
				bt->extkey = newfunc_keyboard_char_to_extkey('a' + drivenum);
			} else
				bt->extkey = newfunc_keyboard_char_to_extkey(drivenum - LOADDIR_MAX_LOCAL_DRIVES + '0');

			bt++;
			dbs++;
		}
		di++;
	}
	if(psi->psio->editsidetype & PLT_ENABLED) {
		if(!(psi->psio->editsidetype & PLT_DOOMQUEUE)) {
			bt->text = " 8: jukebox  ";
			bt->extkey = newfunc_keyboard_char_to_extkey('8');
			bt++;
		}
		bt->text = " 9: playlist ";
		bt->extkey = newfunc_keyboard_char_to_extkey('9');
		bt++;
	}

	if(bt == (&drive_buttons[0]))
		return 0;

	bt->text = "";
	bt->extkey = KEY_ESC;
	bt++;
	bt->text = NULL;
	bt->extkey = 0;

	return 1;
}

// change [sublist] entries to [playlist]
static void loaddir_selectdrive_sublist_to_playlist(struct playlist_side_info *psi)
{
	if((desktopmode & DTM_EDIT_LOADLIST) && (psi->editsidetype & PLT_DIRECTORY)) {
		struct playlist_entry_info *pei, *pei_end;
		pei = psi->firstentry;
		pei_end = psi->firstsong;
		do {
			if(pei->entrytype == DFT_SUBLIST) {
				pei->entrytype = DFT_PLAYLIST;
				if(pds_strcmp(pei->id3info[I3I_DFT_STORE], DFTSTR_SUBLIST) == 0)
					pei->id3info[I3I_DFT_STORE] = DFTSTR_PLAYLIST;
			}
			pei++;
		} while(pei <= pei_end);
	}
}

static void loaddir_selectdrive_playlist_to_sublist(struct playlist_side_info *psi)
{
	if((desktopmode & DTM_EDIT_LOADLIST) && (psi->editsidetype & PLT_DIRECTORY)) {
		struct playlist_entry_info *pei, *pei_end;
		pei = psi->firstentry;
		pei_end = psi->firstsong;
		do {
			if(pei->entrytype == DFT_PLAYLIST) {
				pei->entrytype = DFT_SUBLIST;
				if(pds_strcmp(pei->id3info[I3I_DFT_STORE], DFTSTR_PLAYLIST) == 0)
					pei->id3info[I3I_DFT_STORE] = DFTSTR_SUBLIST;
			}
			pei++;
		} while(pei <= pei_end);
	}
}

static void loaddir_selectdrive_keycheck(struct playlist_side_info *psi, unsigned int extkey)
{
	display_textwin_button_t *bt = &drive_buttons[0];

	if(extkey == KEY_ESC)
		return;

	while(bt->extkey) {
		if(bt->extkey == extkey) {
			char drive = newfunc_keyboard_extkey_to_char(extkey);
			if(drive != 0xff) {	// found extkey
				struct mainvars *mvp = psi->mvp;
				switch (drive) {
				case '8':		// jukebox queue
					if(!(psi->editsidetype & PLT_DOOMQUEUE) && !(psi->psio->editsidetype & PLT_DOOMQUEUE)) {
						psi->editsidetype = PLT_DOOMQUEUE;
						playlist_jukebox_set(mvp, 0);
						playlist_clear_side(psi);
						playlist_disable_side_full(psi);
						mvp->aktfilenum = psi->firstsong - 1;
						mvp->newfilenum = NULL;
						loaddir_selectdrive_sublist_to_playlist(psi->psio);
						funcbit_enable(psi->editloadtype, PLL_CHG_ENTRY);
					}
					break;
				case '9':		// normal playlist
					if(psi->editsidetype & (PLT_DIRECTORY | PLT_DOOMQUEUE)) {
						psi->editsidetype = 0;
						playlist_jukebox_set(mvp, 0);
						playlist_clear_side(psi);
						playlist_disable_side_full(psi);
						loaddir_selectdrive_sublist_to_playlist(psi->psio);
					}
					break;
				default:		// drive
					if((drive >= '0') && (drive <= '7'))	// virtual drives
						drive = LOADDIR_MAX_LOCAL_DRIVES + (drive - '0');
					else {		// local drives
						if((drive >= 'a') && (drive <= 'z'))
							drive -= 'a';
						else
							drive -= 'A';
					}
					if((psi->psio->editsidetype & PLT_DIRECTORY) && (psi->psio->currdrive == drive)) {
						psi->mdds = psi->psio->mdds;
						pds_strcpy(psi->currdir, psi->psio->currdir);
					} else if((psi->currdrive != drive) || !psi->currdir[0]) {
						psi->mdds = playlist_loaddir_drivenum_to_drivemap(drive);
						if(psi->mdds->lastdir[0])
							pds_strcpy(psi->currdir, psi->mdds->lastdir);
						else
							mpxplay_diskdrive_getcwd(psi->mdds, psi->currdir, sizeof(psi->currdir));
					}
					psi->currdrive = drive;
					if(mpxplay_diskdrive_checkdir(psi->mdds, psi->currdir)) {
						funcbit_disable(psi->editsidetype, (~PLT_DIRECTORY));	// ???
						playlist_disable_side_list(psi);
						if(!(psi->editsidetype & PLT_DIRECTORY)) {
							loaddir_selectdrive_playlist_to_sublist(psi->psio);
							psi->editsidetype = PLT_DIRECTORY;
							psi->editloadtype = 0;
						}
						playlist_jukebox_set(mvp, 0);
						playlist_loaddir_buildbrowser(psi);
						playlist_id3list_load(mvp, psi);
						playlist_chkfile_start_norm(psi, 0);
					} else {
						loaddir_build_rootdir(drive, psi->currdir);
						mpxplay_diskdrive_chdir(psi->mdds, psi->currdir);
						if(psi->editsidetype & PLT_DIRECTORY) {
							//psi->editsidetype=0;                // ???
							//playlist_jukebox_set(mvp,0);        //
							playlist_clear_side(psi);
							playlist_disable_side_full(psi);
							//loaddir_selectdrive_sublist_to_playlist(psi->psio); // ???
						}
						mpxplay_timer_addfunc(playlist_loaddir_select_drive_retry, psi, MPXPLAY_TIMERTYPE_WAKEUP, 0);
					}
					break;
				}
				refdisp |= RDT_EDITOR;
				if(psi == mvp->psip)
					refdisp |= RDT_BROWSER;
			}
			break;
		}
		bt++;
	}
}

void playlist_loaddir_select_drive(struct mainvars *mvp, unsigned int side)
{
	display_textwin_button_t *selected_button = NULL;
	char msg[64];

	if(!(displaymode & DISP_FULLSCREEN))
		return;

	if(loaddir_selectdrive_buildbuttons(mvp, side, &selected_button)) {
		unsigned int flags = TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_VERTICALBUTTONS | TEXTWIN_FLAG_NOWINMINSIZE;
		void *tw;
		sprintf(msg, "Choose %s drive:", ((side) ? "RIGHT" : "LEFT"));
		tw = display_textwin_allocwindow_items(NULL, flags, " Drive letter ", loaddir_selectdrive_keycheck, (mvp->psi0 + side));
		display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
		display_textwin_additem_buttons(tw, flags, 0, -1, drive_buttons, selected_button);
		display_textwin_openwindow_items(tw, 0, 0, 0);
	} else
		display_textwin_openwindow_errormsg_ok(" Error ", "Couldn't build drive infos!");
}

void playlist_loaddir_select_drive_retry(struct playlist_side_info *psi)
{
	struct mainvars *mvp = psi->mvp;
	unsigned int side = psi - mvp->psi0;
	display_textwin_button_t *selected_button = NULL;
	char msg[80];

	if(loaddir_selectdrive_buildbuttons(mvp, side, &selected_button)) {
		unsigned int flags = TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_VERTICALBUTTONS | TEXTWIN_FLAG_NOWINMINSIZE | TEXTWIN_FLAG_ERRORMSG;
		void *tw;
		sprintf(msg, "\nCouldn't read drive!\n\nChoose %s drive:", ((side) ? "RIGHT" : "LEFT"));
		tw = display_textwin_allocwindow_items(NULL, flags, " Drive letter ", loaddir_selectdrive_keycheck, (mvp->psi0 + side));
		display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
		display_textwin_additem_buttons(tw, flags, 0, -1, drive_buttons, selected_button);
		display_textwin_openwindow_items(tw, 0, 0, 0);
	} else
		display_textwin_openwindow_errormsg_ok(" Error ", "Couldn't build drive infos!");
}

static char makedir_name[128];

static void loaddir_makedir_do(struct playlist_side_info *psi)
{
	char fulldirname[MAX_PATHNAMELEN], msg[MAX_PATHNAMELEN];

	if(!makedir_name[0])
		return;

	pds_sfn_limit(makedir_name);
	pds_filename_build_fullpath(fulldirname, psi->currdir, makedir_name);

	if(mpxplay_diskdrive_checkdir(psi->mdds, fulldirname)) {
		snprintf(msg, sizeof(msg), "Directory already exists\n%s", fulldirname);
		display_textwin_openwindow_errormsg_ok(" Makedir error ", msg);
		return;
	}

	if(mpxplay_diskdrive_mkdir(psi->mdds, fulldirname) < 0) {
		snprintf(msg, sizeof(msg), "Can't create the directory\n%s", fulldirname);
		display_textwin_openwindow_errormsg_ok(" Makedir error ", msg);
		return;
	} else {
		struct playlist_entry_info *pei_pos;
		pei_pos = playlist_editlist_updatesides_add_dft(psi->mvp, fulldirname, DFT_SUBDIR);
		playlist_editorhighline_set(psi, pei_pos);
	}
}

#define MAKEDIR_WINSIZE_X 40

static display_textwin_button_t makedir_buttons[] = {
	{"", KEY_ENTER1},			// gray enter
	{"", KEY_ENTER2},			// white enter
	{"", 0x4100},				// F7
	{"", KEY_ESC},				// ESC
	{NULL, 0}
};

void playlist_loaddir_makedir_open(struct playlist_side_info *psi)
{
	void *tw;

	if((psi->editsidetype & PLT_DIRECTORY) && !psi->sublistlevel) {
		makedir_name[0] = 0;
		tw = display_textwin_allocwindow_items(NULL, TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_CONFIRM | TEXTWIN_FLAG_NOWINMINSIZE, " Make directory ", loaddir_makedir_do, psi);
		display_textwin_additem_msg_static(tw, 0, 0, -1, " Create the directory");
		display_textwin_additem_editline(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, 0, -1, MAKEDIR_WINSIZE_X, &makedir_name[0], 120);
		display_textwin_additem_buttons(tw, 0, 0, -1, &makedir_buttons[0], NULL);
		display_textwin_openwindow_items(tw, 0, 0, 0);
	} else
		display_textwin_openwindow_errormessage("Cannot create directory here (in playlist)!");
}

//------------------------------------------------------------------------

int playlist_loaddir_disk_unload_load(struct playlist_side_info *psi)
{
	struct mpxplay_diskdrive_data_s *mdds;
	int retcode = -1;

	if(psi->mdds)
		retcode = mpxplay_diskdrive_drive_config(psi->mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_CMD_MEDIAEJECTLOAD, NULL, NULL);
	if(retcode < 0) {
		if(!loaddir_lastvaliddrive)
			playlist_loaddir_get_driveinfos();
		mdds = playlist_loaddir_search_driverhandname_in_drivemap("CDDRIVE");
		if(mdds)
			retcode = mpxplay_diskdrive_drive_config(mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_CMD_MEDIAEJECTLOAD, NULL, NULL);
	}
	return retcode;
}

//--------------------------------------------------------------------------
#ifdef MPXPLAY_LINK_TCPIP

static display_textwin_button_t ftpurl_buttons[] = {
	{"", KEY_ENTER1},			// gray enter
	{"", KEY_ENTER2},			// white enter
	{"", 0x310e},				// ctrl-n
	{"", KEY_ESC},				// ESC
	{NULL, 0}
};

static char ftpurl_name[MAX_PATHNAMELEN];

static void loaddir_ftpurl_do(struct playlist_side_info *psi)
{
	struct mpxplay_virtualdrivemount_s vdm;
	pds_memset((char *)(&vdm), 0, sizeof(vdm));
	vdm.psi = psi;
	vdm.fullinitside = 1;
	vdm.retry = 1;
	pds_strcpy(vdm.path, ftpurl_name);
	playlist_loaddir_virtualdrive_mount(&vdm);
}

#endif							// TCPIP

void playlist_loaddir_ftpurl_open(struct playlist_side_info *psi)
{
#ifdef MPXPLAY_LINK_TCPIP
	void *tw;
	struct mpxplay_diskdrive_data_s mdds;
	unsigned long len = sizeof(ftpurl_name);
	pds_memset(&mdds, 0, sizeof(mdds));
	mdds.mdfs = mpxplay_diskdrive_search_driver((ftpurl_name[0]) ? ftpurl_name : "ftp:");
	mpxplay_diskdrive_drive_config(&mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVOPENNAME, &ftpurl_name[0], &len);
	//if(!ftpurl_name[0])
	// pds_strcpy(ftpurl_name,"ftp://mp3:mp3@127.0.0.1");

	tw = display_textwin_allocwindow_items(NULL, TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_CONFIRM | TEXTWIN_FLAG_NOWINMINSIZE, " FTP ", loaddir_ftpurl_do, psi);
#ifdef MPXPLAY_WIN32
	display_textwin_additem_msg_static(tw, 0, 0, -1, " Open FTP session (ftp[es]://user:pasw@server[:port][/dir])");
#else
	display_textwin_additem_msg_static(tw, 0, 0, -1, " Open FTP session (ftp://user:pasw@server[:port][/dir])");
#endif
	display_textwin_additem_editline(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, 0, -1, MAKEDIR_WINSIZE_X + 6, &ftpurl_name[0], 120);
	display_textwin_additem_buttons(tw, 0, 0, -1, &ftpurl_buttons[0], NULL);
	display_textwin_openwindow_items(tw, 0, 0, 0);
#else
	display_textwin_openwindow_errormsg_ok(" FTP ", " FTP client is not implemented in this version!");
#endif
}

void playlist_loaddir_ftpurl_close(struct playlist_side_info *psi)
{
	if(psi->mdds && psi->mdds->mdfs && pds_stricmp(psi->mdds->mdfs->drivername, "FTPDRIVE") == 0)
		playlist_loaddir_drive_unmount(psi, psi->mdds);
	else if(psi->psio->mdds && psi->psio->mdds->mdfs && pds_stricmp(psi->psio->mdds->mdfs->drivername, "FTPDRIVE") == 0)
		playlist_loaddir_drive_unmount(psi->psio, psi->psio->mdds);
}
