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
//function:playlist main

#include <malloc.h>
#include "newfunc\newfunc.h"
#include "control\control.h"
#include "display\display.h"
#include "playlist.h"

static void playlist_allocate_memworkarea(void);

extern mainvars mvps;
extern char *drivescanletters, *id3tagset[I3I_MAX + 1];
extern unsigned int outmode, playcontrol, playrand, playstartsong;
extern unsigned int refdisp, writeid3tag, playlistload;
extern unsigned int playlist_max_filenum_list;

static char *memworkarea;
static unsigned int playlist_max_filenum_browser;

void mpxplay_playlist_init(struct mainvars *mvp)
{
	struct playlist_side_info *psi0 = mvp->psi0, *psi1 = psi0 + 1;

	if(playlist_max_filenum_list < 512)
		playlist_max_filenum_list = 512;
	if(playlist_max_filenum_list > 99999999)
		playlist_max_filenum_list = 99999999;

#ifdef __DOS__
	playlist_max_filenum_browser = playlist_max_filenum_list / PLAYLIST_MAXFILENUMDIV_DIR;
	if(playlist_max_filenum_browser < 512)
		playlist_max_filenum_browser = 512;
	if(playlist_max_filenum_browser > 4096)
		playlist_max_filenum_browser = 4096;
#else
	playlist_max_filenum_browser = playlist_max_filenum_list;	// win32 version
#endif

	if((playlistload & PLL_DOOMBOX) && !(psi0->editsidetype & PLT_DOOMQUEUE)) {
		funcbit_enable(psi0->editloadtype, PLL_DOOMBOX);
		funcbit_enable(psi1->editsidetype, PLT_DOOMQUEUE);
	}

	playlist_allocate_memworkarea();
	playlist_init_pointers(mvp);
	mpxplay_playlist_textconv_init();
	playlist_sortlist_init(mvp);
	playlist_close_infile(mvp->frp0, mvp);
}

static void playlist_allocate_memworkarea(void)
{
	unsigned int allfilenums, pointerssize, filenamessize, id3memsize, memworkareasize;
	allfilenums = playlist_max_filenum_browser + playlist_max_filenum_list;	//1999+9999=11998
	pointerssize = allfilenums * sizeof(struct playlist_entry_info);	//  11998*64
	filenamessize = allfilenums * FILENAMELENGTH;	// +11998*64
	id3memsize = allfilenums * ID3LENGTH;	// +11998*64
	memworkareasize = pointerssize + filenamessize + id3memsize + 32768;	// =2336384

	memworkarea = malloc(memworkareasize);
	if(memworkarea == NULL)
		mpxplay_close_program(MPXERROR_XMS_MEM);
	pds_memset(memworkarea, 0, memworkareasize);
}

void mpxplay_playlist_close(void)
{
	playlist_id3list_close();
	mpxplay_playlist_textconv_close();
	playlist_loaddir_diskdrives_unmount();
	if(memworkarea) {
		free(memworkarea);
		memworkarea = NULL;
	}
}

void playlist_init_pointers(struct mainvars *mvp)
{
	struct playlist_side_info *psi0 = mvp->psi0, *psi1 = psi0 + 1;
	char *workarea = memworkarea;

	if(playlistload & PLL_DOOMBOX) {
		psi0->allfilenum = playlist_max_filenum_list;
		psi1->allfilenum = playlist_max_filenum_browser;
	} else {
		psi0->allfilenum = playlist_max_filenum_browser;
		psi1->allfilenum = playlist_max_filenum_list;
	}

	mvp->pei0 = (struct playlist_entry_info *)workarea;	// 767872 (11998*64)
	workarea += (playlist_max_filenum_browser + playlist_max_filenum_list + 2) * sizeof(struct playlist_entry_info);	// +2 tmp for getting id3tag
	workarea += 1024;

	mvp->pei0->filename = workarea;	// current song's infos
	workarea += MAX_PATHNAMELEN + MAX_ID3LEN + (1024 - MAX_PATHNAMELEN);	// 2048 (1024+1024)

	psi0->filenamesbeginp = workarea;
	psi0->filenameslastp = workarea;	// 127936 (1999*64)
	workarea += psi0->allfilenum * FILENAMELENGTH;
	psi0->filenamesendp = workarea - MAX_PATHNAMELEN;

	psi1->filenamesbeginp = workarea;
	psi1->filenameslastp = workarea;	// 639936 (9999*64)
	workarea += psi1->allfilenum * FILENAMELENGTH;
	psi1->filenamesendp = workarea - MAX_PATHNAMELEN;
	workarea += 1024;

	psi0->id3infobeginp = workarea;
	psi0->id3infolastp = workarea;	// 127936 (1999*64)
	workarea += psi0->allfilenum * ID3LENGTH;
	psi0->id3infoendp = workarea - MAX_ID3LEN;

	psi1->id3infobeginp = workarea;
	psi1->id3infolastp = workarea;	// 639936 (9999*64)
	workarea += psi1->allfilenum * ID3LENGTH;
	psi1->id3infoendp = workarea - MAX_ID3LEN;	//-----------------
	//2305664

	psi0->firstentry = psi0->firstsong = psi0->editorhighline = mvp->pei0 + 1;
	psi0->lastentry = psi0->firstentry - 1;
	psi0->endentry = psi0->firstentry + (psi0->allfilenum - 2);

	psi1->firstentry = psi1->firstsong = psi1->editorhighline = psi0->firstentry + psi0->allfilenum + 1;	// +1 tmp for getting id3tag
	psi1->lastentry = psi1->firstentry - 1;
	psi1->endentry = psi1->firstentry + (psi1->allfilenum - 1);

	mvp->aktfilenum = psi1->lastentry;
}

//-------------------------------------------------------------------------
void playlist_peimyself_reset(struct playlist_side_info *psi, struct playlist_entry_info *firstentry, struct playlist_entry_info *lastentry)
{
	struct playlist_entry_info *pei;
	if(!firstentry)
		firstentry = psi->firstentry;
	if(!lastentry)
		lastentry = psi->endentry;
	if(lastentry >= firstentry) {
		pei = firstentry;
		do {
			pei->myself = pei;
		} while((++pei) <= lastentry);
	}
}

struct playlist_entry_info *playlist_peimyself_search(struct playlist_side_info *psi, struct playlist_entry_info *pei_src)
{
	struct playlist_entry_info *pei_dest = psi->firstentry, *lastentry = psi->lastentry;
	if(lastentry >= pei_dest) {
		do {
			if(pei_dest->myself == pei_src)
				return pei_dest;
		} while((++pei_dest) <= lastentry);
	}
	return NULL;
}

//------------------------------------------------------------------------
void playlist_enable_side(struct playlist_side_info *psi)
{
	if(!(psi->editsidetype & PLT_ENABLED)) {
		funcbit_enable(psi->editsidetype, PLT_ENABLED);
		playlist_peimyself_reset(psi, NULL, NULL);
		display_editorside_reset(psi);
	}
}

void playlist_disable_side_partial(struct playlist_side_info *psi)
{
	struct mainvars *mvp = psi->mvp;

	playlist_editorhighline_set(psi, psi->firstentry);

	psi->firstsong = psi->firstentry;
	psi->lastentry = psi->firstentry - 1;

	psi->filenameslastp = psi->filenamesbeginp;
	psi->id3infolastp = psi->id3infobeginp;

	psi->selected_files = 0;

	playlist_fulltime_clearside(psi);

	if(psi == mvp->psip) {
		mvp->aktfilenum = psi->firstentry - 1;
		mvp->newfilenum = NULL;
		playlist_randlist_clearall(psi);
		if(!(playcontrol & PLAYC_RUNNING))
			mpxplay_stop_and_clear(mvp, 0);
	}
	if(psi == mvp->psie)
		playlist_change_editorside(mvp);
	playlist_chkfile_stop(psi);
	funcbit_disable(psi->editsidetype, PLT_ENABLED | PLT_EXTENDED);
	funcbit_disable(psi->editloadtype, PLL_CHG_ALL);
}

void playlist_disable_side_list(struct playlist_side_info *psi)
{
	struct mainvars *mvp = psi->mvp;
	playlist_loadsub_sublist_clear(psi);
	playlist_savelist_clear(psi);
	funcbit_disable(psi->editloadtype, PLL_TYPE_ALL | PLL_CHG_ALL);
	if(psi == mvp->psil)
		playlist_clear_freeopts();
	if(psi == mvp->psip)
		funcbit_disable(playlistload, PLL_TYPE_ALL);
}

void playlist_disable_side_full(struct playlist_side_info *psi)
{
	playlist_disable_side_partial(psi);
	playlist_disable_side_list(psi);
}

void playlist_clear_side(struct playlist_side_info *psi)
{
	struct mainvars *mvp = psi->mvp;

	playlist_editorhighline_set(psi, psi->firstentry);

	psi->firstsong = psi->firstentry;
	psi->lastentry = psi->firstentry - 1;

	psi->filenameslastp = psi->filenamesbeginp;
	psi->id3infolastp = psi->id3infobeginp;

	psi->selected_files = 0;

	pds_memset(psi->firstentry, 0, psi->allfilenum * sizeof(struct playlist_entry_info));
	playlist_fulltime_clearside(psi);

	if(psi == mvp->psip) {
		mvp->aktfilenum = psi->firstentry - 1;
		mvp->newfilenum = NULL;
		playlist_randlist_clearall(psi);
	}
	playlist_chkfile_stop(psi);
	funcbit_disable(psi->editsidetype, PLT_ENABLED | PLT_EXTENDED);
	funcbit_disable(psi->editloadtype, PLL_TYPE_CMDL | PLL_CHG_ALL);
}

void playlist_reset_side(struct playlist_side_info *psi)
{
	psi->editsidetype = 0;
}

void playlist_copy_side_infos(struct playlist_side_info *psi_dest, struct playlist_side_info *psi_src)
{
	psi_dest->editsidetype = psi_src->editsidetype;
	psi_dest->editloadtype = psi_src->editloadtype & PLL_TYPE_ALL;	// !!!
	pds_memcpy((char *)&psi_dest->id3ordertype, (char *)&psi_src->id3ordertype, sizeof(psi_dest->id3ordertype));
	psi_dest->mdds = psi_src->mdds;
	if(psi_src->editsidetype & PLT_DIRECTORY) {	// !!!
		psi_dest->currdrive = psi_src->currdrive;
		pds_memcpy((char *)&psi_dest->currdir, (char *)&psi_src->currdir, sizeof(psi_dest->currdir));
	}
	psi_dest->sublistlevel = psi_src->sublistlevel;
	pds_memcpy((char *)&psi_dest->sublistnames, (char *)&psi_src->sublistnames, sizeof(psi_dest->sublistnames));
	psi_dest->savelist_type = psi_src->savelist_type;
	psi_dest->savelist_textcodetype = psi_src->savelist_textcodetype;
	pds_memcpy((char *)&psi_dest->savelist_filename, (char *)&psi_src->savelist_filename, sizeof(psi_dest->savelist_filename));
}

void playlist_clear_freeopts(void)
{
	unsigned int optcount = OPT_INPUTFILE;	//+1;
	while((optcount < MAXFREEOPTS) && freeopts[optcount])
		freeopts[optcount++] = NULL;
}

//------------------------------------------------------------------------
char startdir[MAX_PATHNAMELEN];

char *mpxplay_playlist_startdir(void)
{
	if(!startdir[0])
		pds_getcwd(startdir);
	return startdir;
}

void mpxplay_save_startdir(struct mainvars *mvp)
{
	if(!startdir[0])
		pds_getcwd(startdir);
	playlist_loaddir_initbrowser(mvp, startdir);
}

void mpxplay_restore_startdir(void)
{
	if(startdir[0])
		pds_chdir(startdir);
}

void mpxplay_playlist_startfile_fullpath(char *fullpath, char *filename)
{
	int drivenum;
	char *s, strtmp[MAX_PATHNAMELEN], currdir[MAX_PATHNAMELEN];

	if(pds_filename_check_absolutepath(filename)) {	// d: or ftp:
		drivenum = pds_getdrivenum_from_path(filename);
		if(drivenum >= 0) {		// d:
			s = filename;
			if(filename[2] != PDS_DIRECTORY_SEPARATOR_CHAR) {	// d:filename.mp3 (drive without path)
				struct mpxplay_diskdrive_data_s *mdds = playlist_loaddir_drivenum_to_drivemap(drivenum);
				if(mdds) {
					mpxplay_diskdrive_getcwd(mdds, currdir, sizeof(currdir));
					pds_filename_build_fullpath(strtmp, currdir, &filename[2]);
					s = &strtmp[0];
				}
			}
			pds_fullpath(fullpath, s);	// for snf/lfn conv
		} else {
			pds_strcpy(fullpath, filename);	// leave filename as is (possible virtual drive path/filename)
		}
	} else {
		pds_filename_build_fullpath(strtmp, mpxplay_playlist_startdir(), filename);
		pds_fullpath(fullpath, strtmp);	// for snf/lfn conv
	}
}

//*************************************************************************

void playlist_get_allfilenames(struct mainvars *mvp)
{
	struct playlist_side_info *psi0 = mvp->psi0, *psi1 = psi0 + 1;

	playlist_buildlist_all(mvp->psil);

	if(!(mvp->psil->editsidetype & PLT_ENABLED)) {	// playlist is not loaded
		funcbit_disable(mvp->psil->editloadtype, PLL_TYPE_ALL);
		funcbit_disable(playlistload, PLL_TYPE_ALL);
		if(!(mvp->psil->psio->editloadtype & PLL_TYPE_ALL) && !(mvp->psil->psio->editsidetype & PLT_DIRECTORY) && !funcbit_test(playcontrol, PLAYC_EXITENDLIST)) {
			psi0->editsidetype = PLT_DIRECTORY;	// !!! ??? overrides -db
			mvp->psil = psi1;
		}
	}

	if((mvp->psil->psio->editsidetype & PLT_DIRECTORY) || (mvp->psil->psio->editloadtype & PLL_TYPE_LOAD))
		playlist_buildlist_all(mvp->psil->psio);

	if(!(psi1->editsidetype & PLT_DIRECTORY))
		playlist_loadlist_load_autosaved_list(psi1);

	if(!(psi0->editsidetype & PLT_ENABLED) && !(psi1->editsidetype & PLT_ENABLED))
		if(psi0->editsidetype & PLT_DIRECTORY)
			playlist_loaddir_select_drive_retry(psi0);
		else
			mpxplay_close_program(MPXERROR_NOFILE);
}

unsigned int playlist_buildlist_one(struct playlist_side_info *psi, char *listfile, unsigned int loadtype, char *dslp, char *filtermask)
{
	unsigned int result = 0;
	if((loadtype & PLL_STDIN) || (playlist_loadlist_check_extension(listfile) && !pds_strchr(listfile, '*') && !pds_strchr(listfile, '?'))) {
		funcbit_enable(loadtype, PLL_LOADLIST);
		funcbit_disable(loadtype, (PLL_DRIVESCAN | PLL_DIRSCAN));	//|PLL_STDIN));
		if(playlist_loadlist_mainload(psi, listfile, loadtype, filtermask))
			result = PLL_LOADLIST;
	} else {
		struct playlist_entry_info *beginentry = psi->lastentry + 1;
		playlist_loaddir_scandrives(psi, listfile, dslp);
		playlist_order_filenames_block(psi, beginentry, psi->lastentry);
		result = PLL_DIRSCAN;
		funcbit_disable(loadtype, PLL_LOADLIST);
		funcbit_enable(loadtype, PLL_DIRSCAN);
		if(dslp)
			funcbit_enable(loadtype, PLL_DRIVESCAN);
		else
			funcbit_disable(loadtype, PLL_DRIVESCAN);
	}

	if((psi->editsidetype & PLT_DIRECTORY) && psi->sublistlevel)
		funcbit_disable(loadtype, PLL_TYPE_LOAD);

	funcbit_disable(psi->editloadtype, PLL_TYPE_CMDL);
	funcbit_copy(psi->editloadtype, loadtype, PLL_TYPE_CMDL);
	return result;
}

unsigned int playlist_buildlist_all(struct playlist_side_info *psi)
{
	char inputfile[MAX_PATHNAMELEN];

	if((psi->editsidetype & PLT_DIRECTORY) && !psi->sublistlevel) {
		playlist_loaddir_buildbrowser(psi);
		return 1;
	}

	playlist_clear_side(psi);
	playlist_loadsub_addsubdots(psi);
	// first input file
	if(psi->sublistlevel)
		return playlist_buildlist_one(psi, psi->sublistnames[psi->sublistlevel], 0, NULL, NULL);
	else if(!playlist_buildlist_one(psi, playlist_loadsub_getinputfile(psi), (playlistload & PLL_TYPE_ALL), drivescanletters, NULL))
		return 0;

	// multiply input file
	if(!(outmode & OUTMODE_TYPE_FILE) && freeopts[OPT_INPUTFILE + 1]) {
		unsigned int optcount = OPT_INPUTFILE + 1;
		unsigned int allextended = psi->editsidetype & PLT_EXTENDED;
		while((optcount < MAXFREEOPTS) && freeopts[optcount]) {
			unsigned int result;
			mpxplay_playlist_startfile_fullpath(inputfile, freeopts[optcount]);
			result = playlist_buildlist_one(psi, inputfile, 0, NULL, NULL);
			if(result & PLL_LOADLIST)
				allextended &= (psi->editsidetype & PLT_EXTENDED);
			else
				allextended = 0;
			optcount++;
		}
		funcbit_disable(psi->editsidetype, PLT_EXTENDED);
		funcbit_copy(psi->editsidetype, allextended, PLT_EXTENDED);	// playlist (side) is extended if all loaded lists are extended (then we don't check the entries)
	}

	if((playrand & 2) && (psi->editsidetype & PLT_ENABLED)) {
		playlist_randlist_randomize_side(psi);
		playrand = 0;
	}
	return 1;
}

//*************************************************************************
//set starting playlist side & entry
void playlist_init_playside(struct mainvars *mvp)
{
	struct playlist_side_info *psi0 = mvp->psi0, *psi1 = psi0 + 1;

	if(psi0->editsidetype & PLT_DOOMQUEUE) {
		mvp->psip = psi0;
		if(psi0->editsidetype & PLT_ENABLED)
			funcbit_enable(playcontrol, PLAYC_STARTNEXT);
	} else if(psi1->editsidetype & PLT_DOOMQUEUE) {
		mvp->psip = psi1;
		if(psi1->editsidetype & PLT_ENABLED)
			funcbit_enable(playcontrol, PLAYC_STARTNEXT);
	} else if((psi1->editsidetype & PLT_ENABLED) && (psi1->editloadtype & PLL_TYPE_LOAD) && (!(playlistload & PLL_RESTORED) || (playlistload & PLL_TYPE_LOAD))) {
		mvp->psie = psi1;
		mvp->psip = psi1;
		funcbit_enable(playcontrol, PLAYC_STARTNEXT);
	} else if(!(psi1->editsidetype & PLT_ENABLED)) {
		mvp->psie = psi0;
		mvp->psip = psi0;
	} else if(!(psi0->editsidetype & PLT_ENABLED)) {
		mvp->psie = psi1;
		mvp->psip = psi1;
	}
}

void playlist_init_playsong(struct mainvars *mvp)
{
	struct playlist_side_info *psip = mvp->psip;

	mvp->aktfilenum = psip->firstsong;
	if(psip->lastentry >= psip->firstsong) {
		if(playstartsong) {
			if((mvp->aktfilenum + playstartsong - 1) <= psip->lastentry)
				mvp->aktfilenum += playstartsong - 1;
		} else {
			if(playrand) {
				struct playlist_entry_info *newfilenum = playlist_randlist_getnext(psip);	// start with a random file
				if(newfilenum)
					mvp->aktfilenum = newfilenum;
			}
		}
	} else {
		funcbit_enable(playcontrol, PLAYC_PAUSENEXT);	// ???
	}
	playlist_editorhighline_set(psip, ((mvp->aktfilenum <= psip->lastentry) ? mvp->aktfilenum : psip->firstentry));
}

void playlist_start_sideplay(struct mainvars *mvp, struct playlist_side_info *psi)
{
	if(!(playcontrol & PLAYC_RUNNING) && (psi->editsidetype & PLT_ENABLED)) {
		mvp->psip = psi;
		if(psi->editsidetype & PLT_DOOMQUEUE) {
			playlist_editorhighline_set(psi, psi->firstentry);
			mvp->aktfilenum = psi->firstentry - 1;
			mvp->adone = ADONE_RESTART;
		} else {
			if(playrand)
				mvp->newsong = playlist_randlist_getnext(psi);
			else
				mvp->newsong = psi->firstsong;
		}
		playcontrol |= PLAYC_STARTNEXT;
	}
}

//**************************************************************************

unsigned int playlist_open_infile(struct frame *frp, struct mainvars *mvp, struct playlist_entry_info *pei)
{
	struct playlist_side_info *psi = mvp->psip;
	struct mpxplay_infile_info_s *miis = frp->infile_infos;
	char sout[128];

	mpxplay_infile_close(frp);

	if(pei < psi->firstsong || pei > psi->lastentry)
		return 0;
	if(!pei->filename || !pei->filename[0])
		return 0;

	//funcbit_disable(pei->infobits,PEIF_ENABLED);
	frp->filetype = GET_HFT(pei->entrytype);
	frp->filesize = pei->filesize;
	frp->mdds = pei->mdds;
	frp->infile_funcs = pei->infile_funcs;
	miis->filesize = pei->filesize;
	miis->timemsec = pei->timemsec;

	playlist_chkentry_get_onefileinfos_open(mvp->psip, pei);
	if(pei->entrytype < DFT_AUDIOFILE) {
		snprintf(sout, sizeof(sout), "Can't play (unknown) file:\n%s", pei->filename);
		display_timed_message(sout);
		return 0;
	}

	frp->filetype = GET_HFT(pei->entrytype);
	frp->filesize = pei->filesize;
	frp->mdds = pei->mdds;
	frp->infile_funcs = pei->infile_funcs;
	miis->filesize = pei->filesize;
	miis->timemsec = pei->timemsec;

	if(!mpxplay_infile_open(frp, pei->filename)) {
		snprintf(sout, sizeof(sout), "Can't open file:\n%s", pei->filename);
		display_timed_message(sout);
		return 0;
	}

	funcbit_enable(pei->infobits, PEIF_ENABLED);
	pei->filesize = frp->filesize;
	pei->timemsec = miis->timemsec;
	pei->filesize = miis->filesize;
	pei->mdds = frp->mdds;
	pei->infile_funcs = frp->infile_funcs;

	return 1;
}

void playlist_close_infile(struct frame *frp, struct mainvars *mvp)
{
	struct playlist_entry_info *pei0 = mvp->pei0;
	unsigned int i;

	mpxplay_infile_close(frp);

	//pei0 reset
	pds_strcpy(pei0->filename, "No file");
	for(i = 0; i <= I3I_MAX; i++)
		pei0->id3info[i] = NULL;
	pei0->pstime = 0;
	pei0->petime = 0;
	pei0->infobits = 0;
	pei0->timemsec = 0;
	pei0->filesize = 0;
}

//save infos of current song from playlist (entry)
void playlist_pei0_set(struct mainvars *mvp, struct playlist_entry_info *pei, unsigned long update_mode)
{
	struct playlist_entry_info *pei0 = mvp->pei0;
	unsigned int i;
	char *sp;

	if(!(update_mode & (EDITLIST_MODE_ID3 | EDITLIST_MODE_INDEX))
	   || ((update_mode & EDITLIST_MODE_ID3) && !pei0->id3info[I3I_ARTIST] && !pei0->id3info[I3I_TITLE])
	   || ((update_mode & EDITLIST_MODE_INDEX) && ((pei0->infobits & PEIF_INDEXED) || (pei->infobits & PEIF_INDEXED)))) {
		sp = pei0->filename + pds_strcpy(pei0->filename, pei->filename) + 1;
		for(i = 0; i <= I3I_MAX; i++) {
			if(id3tagset[i])
				pei0->id3info[i] = id3tagset[i];
			else if(pei->id3info[i] && (sp < (pei0->filename + MAX_PATHNAMELEN + MAX_ID3LEN))) {
				pei0->id3info[i] = sp;
				sp += pds_strcpy(sp, pei->id3info[i]) + 1;
			} else
				pei0->id3info[i] = NULL;
		}
		pei0->entrytype = pei->entrytype;
		pei0->infobits = pei->infobits;
		pei0->timemsec = pei->timemsec;
		pei0->filesize = pei->filesize;
		pei0->mdds = pei->mdds;
		pei0->infile_funcs = pei->infile_funcs;
		pei0->pstime = pei->pstime;
		pei0->petime = pei->petime;
		refdisp |= RDT_HEADER | RDT_ID3INFO;	// !!!
	}
}

//*************************************************************************

void playlist_editorhighline_check(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei = psi->editorhighline;

	if(pei > psi->lastentry)
		pei = psi->lastentry;
	if(pei < psi->firstentry)
		pei = psi->firstentry;
	psi->editorhighline = pei;
	mpxplay_display_center_editorhighline(psi, 0);
}

void playlist_editorhighline_seek(struct playlist_side_info *psi, long offset, int whence)
{
	switch (whence) {
	case SEEK_SET:
		psi->editorhighline = psi->firstentry + offset;
		break;
	case SEEK_CUR:
		psi->editorhighline += offset;
		break;
	case SEEK_END:
		psi->editorhighline = psi->lastentry - offset;
		break;
	}
	playlist_editorhighline_check(psi);
}

void playlist_editorhighline_set(struct playlist_side_info *psi, struct playlist_entry_info *pei)
{
	psi->editorhighline = pei;
	playlist_editorhighline_check(psi);
}

void playlist_editorhighline_set_nocenter(struct playlist_side_info *psi, struct playlist_entry_info *pei)
{
	if(pei > psi->lastentry)
		pei = psi->lastentry;
	if(pei < psi->firstentry)
		pei = psi->firstentry;
	psi->editorhighline = pei;
}

//-------------------------------------------------------------------------
void playlist_change_editorside(struct mainvars *mvp)
{
	struct playlist_side_info *psie = mvp->psie;
	if(psie->psio->editsidetype & PLT_ENABLED) {
		psie = psie->psio;
		mvp->psie = psie;
		refdisp |= RDT_INIT_EDIT;
	}
}

//*************************************************************************
playlist_entry_info *playlist_search_filename(struct playlist_side_info *psi, char *filename, long timempos, struct playlist_entry_info *pei)
{
	if(!filename || !filename[0])
		return NULL;
	while(*filename == ' ')
		filename++;
	if(!*filename)
		return NULL;
	if(!pei)
		pei = psi->firstentry;
	while(pei <= psi->lastentry) {
		if(pds_stricmp(pei->filename, filename) == 0) {
			if((timempos < 0) || !(pei->infobits & PEIF_INDEXED))
				return pei;
			if((timempos >= pei->pstime) && (!pei->petime || (timempos < pei->petime)))
				return pei;
		}
		pei++;
	}
	return NULL;
}

//move the cursor to the subdir/list where you came from
void playlist_search_lastdir(struct playlist_side_info *psi, char *lastdir)
{
	struct playlist_entry_info *pei_set;

	pei_set = playlist_search_filename(psi, lastdir, -1, NULL);
	if(!pei_set)
		pei_set = psi->firstentry;
	playlist_editorhighline_set(psi, pei_set);
}

void playlist_search_firstsong(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	psi->firstsong = psi->lastentry + 1;
	for(pei = psi->firstentry; pei <= psi->lastentry; pei++)
		if(GET_HFT(pei->entrytype) != HFT_DFT) {
			psi->firstsong = pei;
			break;
		}
}

//-------------------------------------------------------------------------
void playlist_change_sublist_or_directory(struct playlist_side_info *psi, unsigned long head)
{
	if(GET_HFT(head) == HFT_DFT) {
		if(head & DFTM_PLAYLIST)
			psi = playlist_loadsub_sublist_change(psi, head);
		else
			psi = playlist_loaddir_changedir(psi, head);
		playlist_chkfile_start_norm(psi, NULL);
	}
}

//-------------------------------------------------------------------------
void playlist_reload_side(struct mainvars *mvp, struct playlist_side_info *psi)
{
	if(!(psi->editsidetype & PLT_DOOMQUEUE)) {
		struct playlist_entry_info *ehl_save = psi->editorhighline;
		if((psi->editsidetype & PLT_DIRECTORY) && !psi->sublistlevel)
			mpxplay_diskdrive_drive_config(psi->mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_CMD_RESETDRIVE, NULL, NULL);
		playlist_buildlist_all(psi);
		playlist_chkfile_start_norm(psi, NULL);
		playlist_editorhighline_set(psi, ehl_save);
	}
}

void playlist_reload_dirs(struct mainvars *mvp)
{
	unsigned int i;
	struct playlist_side_info *psi = mvp->psi0;
	for(i = 0; i < PLAYLIST_MAX_SIDES; i++, psi++) {
		struct playlist_entry_info *ehl_save = psi->editorhighline;
		if((psi->editsidetype & PLT_DIRECTORY) && !psi->sublistlevel)
			playlist_loaddir_buildbrowser(psi);
		playlist_chkfile_start_norm(psi, NULL);
		playlist_editorhighline_set(psi, ehl_save);
	}
}

//-------------------------------------------------------------------------
void playlist_write_id3tags(struct mainvars *mvp)
{
	struct frame *frp;
	struct playlist_side_info *psi;
	struct playlist_entry_info *pei;
	int error;
	char *shortfname;
	char sout[128];

	if(!writeid3tag)
		return;

	frp = mvp->frp0 + 2;
	psi = mvp->psil;

	for(pei = psi->firstsong; pei <= psi->lastentry; pei++) {
		playlist_chkentry_get_onefileinfos_allagain(psi, pei, frp, ID3LOADMODE_ALL | ID3LOADMODE_PREFER_LIST);
		mpxplay_infile_close(frp);
		frp->filetype = GET_HFT(pei->entrytype);
		frp->filesize = pei->filesize;
		frp->mdds = pei->mdds;
		frp->infile_funcs = pei->infile_funcs;
		frp->infile_infos->timemsec = pei->timemsec;
		error = mpxplay_infile_write_id3tag(frp, pei->filename, &pei->id3info[0]);
		shortfname = pds_getfilename_from_fullname(pei->filename);
		switch (error) {
		case MPXPLAY_ERROR_INFILE_OK:
			sprintf(sout, "%.15s  %-30.30s : %-30.30s", shortfname, pei->id3info[I3I_ARTIST], pei->id3info[I3I_TITLE]);
			break;
		case MPXPLAY_ERROR_INFILE_CANTOPEN:
			sprintf(sout, "ID3TAG write error at %.25s (read-only or not exists)!", shortfname);
			break;
		case MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE:
			sprintf(sout, "ID3TAG write is not supported for this filetype (%.25s)", shortfname);
			break;
		case MPXPLAY_ERROR_INFILE_WRITETAG_TAGTYPE:
			sprintf(sout, "ID3TAG wrong or unsupported tagtype (%.20s)", shortfname);
			break;
		default:
			sprintf(sout, "ID3TAG write error at %.20s (unknown error)!", shortfname);
			break;
		}
		pds_textdisplay_printf(sout);
	}
	mpxplay_close_program(0);
}
