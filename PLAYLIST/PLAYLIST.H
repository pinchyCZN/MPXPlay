//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: playlist definitions

#ifndef mpxplay_playlist_h
#define mpxplay_playlist_h

#include "mpxplay.h"

#define PLAYLIST_MAX_SIDES     2
#define PLAYLIST_MAX_ORDERKEYS 5	// 5. for internal use only

#define MAX_SUBLIST_LEVELS    8
#define MAX_PATHNAMELEN     300	//truely it's 260 (OS limit)
#define MAX_ID3LEN         1024	//artist+title+etc.

#ifdef MPXPLAY_LINK_FULL
#define FILENAMELENGTH  64		//average ((9999+1999)*64=767872 bytes playlist size)
#define ID3LENGTH       64		//average (dinamically used)
#else
#define FILENAMELENGTH  32
#define ID3LENGTH       32
#endif

#define PLAYLIST_MAXFILENUMDIV_DIR 5	// max_filenum_dir=max_filenum_list/5
#define MAX_ID3LISTPARTS 4

// directory file type (HFT_DFT) masks
#define DFTM_DFT      0x10000000
#define DFTM_DRIVE    0x0004
#define DFTM_DIR      0x0008
#define DFTM_UPDIR    0x0001
#define DFTM_SUBDIR   0x0002
#define DFTM_PLAYLIST 0x0010
#define DFTM_UPLIST   0x0001
#define DFTM_SUBLIST  0x0002
#define DFTM_ROOT     0x0040	// change to root dir/list

// directory file types (0x1) in pei->entrytype
#define DFT_DRIVE    (DFTM_DFT|DFTM_DRIVE)
#define DFT_UPDIR    (DFTM_DFT|DFTM_DIR|DFTM_UPDIR)
#define DFT_SUBDIR   (DFTM_DFT|DFTM_DIR|DFTM_SUBDIR)
#define DFT_ROOTDIR  (DFTM_DFT|DFTM_DIR|DFTM_ROOT)
#define DFT_PLAYLIST (DFTM_DFT|DFTM_PLAYLIST)
#define DFT_UPLIST   (DFTM_DFT|DFTM_PLAYLIST|DFTM_UPLIST)
#define DFT_SUBLIST  (DFTM_DFT|DFTM_PLAYLIST|DFTM_SUBLIST)
#define DFT_ROOTLIST (DFTM_DFT|DFTM_PLAYLIST|DFTM_ROOT)

#define DFT_NOTCHECKED 0x00000000	// not checked, but possible valid audio file
#define DFT_UNKNOWN    0x00000001	// unknown/invalid entry
#define DFT_AUDIOFILE  0x20000000	// file is not a DFT

#define DFTSTR_DRIVE    "<DRIVE>"
#define DFTSTR_UPDIR    "<UP--DIR>"
#define DFTSTR_SUBDIR   "<SUB-DIR>"
#define DFTSTR_PLAYLIST "[playlist]"
#define DFTSTR_SUBLIST  "[sub-list]"
#define DFTSTR_UPLIST   "[up-list]"

// editsidetype bits (left,right side)
#define PLT_ENABLED    1		// playlist used/enabled (not empty)
#define PLT_EXTENDED   2		// extended playlist (mxu,pls,extm3u)
#define PLT_EXTENDINC  4		// incomplete extended playlist (extm3u with missing infos)
#define PLT_DOOMQUEUE  8		// doombox queue
#define PLT_DIRECTORY 16		// directory browser
#define PLT_SORTC_DESCENDING     256	// not used yet
#define PLT_SORTC_MANUALCOPY     512	// ie: disable filename sort in directory browser at F5
#define PLT_SORTC_MAGNETHIGHLINE 1024	// magnetize (move) editorhighline at sort

// playlistload types
#define PLL_LOADLIST      1		// load playlist (-@)
#define PLL_DOOMBOX       4		// doombox/jukebox mode loading
#define PLL_DIRSCAN       8		// scan directori(es) for files
#define PLL_DRIVESCAN    16		// scan drive(s) for file(s)
#define PLL_FASTLIST     32		// [fastlists] are used (not a command line list/file)
#define PLL_STDIN        64		// load playlist from stdin
#define PLL_SUBLISTS    128		// sublist flag (for startup)
#define PLL_RESTORED   1024		// list(s) was restored from mpxplay.ini (not from command line)
#define PLL_CHG_MANUAL 2048		// manual playlist change
#define PLL_CHG_ENTRY  4096		// entry(s) has changed (always manual)
#define PLL_CHG_LEN    8192		// length has changed
#define PLL_CHG_ID3   16384		// id3 has changed
#define PLL_TYPE_CMDL  (PLL_LOADLIST|PLL_DIRSCAN|PLL_DRIVESCAN|PLL_STDIN)	// command-line loading-types
#define PLL_TYPE_LOAD  (PLL_TYPE_CMDL|PLL_FASTLIST)	// all loading-types
#define PLL_TYPE_ALL   (PLL_TYPE_LOAD|PLL_SUBLISTS)	// all list types
#define PLL_CHG_ALL    (PLL_CHG_MANUAL|PLL_CHG_ENTRY|PLL_CHG_LEN|PLL_CHG_ID3)

// preloadinfo types
#define PLI_NOTLOAD    0		// don't load id3infos (load at skip only) (-inl)
#define PLI_PRELOAD    1		// load all infos at the start of program
#define PLI_PLAYLOAD   2		// load infos under playing (-ipl)
#define PLI_DISPLOAD   3		// load infos of displayed files (controlled from playlist editor) (-idl)
#define PLI_EHLINELOAD 4		// load infos of highlighted file (-"-) (-ihl)

// sortcontrol flag(s)
#define SORTC_DESCENDING  1		// descending sort (else ascending)

// loadid3list
#define ID3LISTTYPE_LOCAL  1	// -il
#define ID3LISTTYPE_GLOBAL 2	// -ig

// loadid3tag
// external config
#define ID3LOADMODE_NONE  0		// don't use id3 infos at all (-in)
#define ID3LOADMODE_LIST  1		// mxu,extm3u,id3list
#define ID3LOADMODE_FILE  2		// id3tag from file
#define ID3LOADMODE_CLFN  4		// try to create artist:title from a (long)filename
#define ID3LOADMODE_ALL   (ID3LOADMODE_LIST|ID3LOADMODE_FILE|ID3LOADMODE_CLFN)
#define ID3LOADMODE_PREFER_LIST 8	// prefer infos from list (else prefer infos from file)(if list-id3infos (artist,title) exists, program will use those)
#define ID3LOADMODE_NOFILE (ID3LOADMODE_LIST|ID3LOADMODE_CLFN)	// -inf
// internal flags
#define ID3LOADMODE_OTHERENTRY  256	// get infos from other entry (prev index or search on other side) (it's not in the ALL !)
#define ID3LOADMODE_FASTSEARCH 1024	// we don't need deep infos (at collecting infos for editor)

// id3info index values
#define I3I_ARTIST     1
#define I3I_TITLE      0
#define I3I_ALBUM      2
#define I3I_YEAR       3
#define I3I_COMMENT    4
#define I3I_GENRE      5
#define I3I_TRACKNUM   6
#define I3I_MAX        6
#define I3I_DFT_STORE  I3I_TITLE	// store directory names (<DRIVE>,[playlist]) in I3I_TITLE

// id3order types
#define ID3ORDER_ARTIST    I3I_ARTIST
#define ID3ORDER_TITLE     I3I_TITLE
#define ID3ORDER_ALBUM     I3I_ALBUM
#define ID3ORDER_YEAR      I3I_YEAR
#define ID3ORDER_COMMENT   I3I_COMMENT
#define ID3ORDER_GENRE     I3I_GENRE
#define ID3ORDER_TRACKNUM  I3I_TRACKNUM
#define ID3ORDER_TIME      7
#define ID3ORDER_FILESIZE  8
#define ID3ORDER_PATH      9	// path only (without filename)
#define ID3ORDER_FILENAME 10	// filename only (without path)
#define ID3ORDER_PATHFILE 11	// path + filename
#define ID3ORDER_FILEDATE 12	// pei->filedate
#define ID3ORDER_DFT      13	// pei->entrytype
#define ID3ORDER_INDEX    14	// pei->pstime
#define ID3ORDER_DISABLED 15
#define ID3ORDER_FILEEXT  ID3ORDER_COMMENT	// at funcs_commander
#define ID3ORDER_SAVEMASK 0xf

// playlistsave types
#define PLST_AUTO     1			// save to mpxplay's dir
#define PLST_MANUAL   2			// save to starting/selected dir
#define PLST_MXU      4			// save in mxu format
#define PLST_EXTM3U   8			// winamp's style m3u (with time,artist,title infos)
#define PLST_CUE     16			//
#define PLST_M3U     256		// used internally only
#define PLST_LISTS   (PLST_MXU|PLST_EXTM3U|PLST_CUE)
#define PLST_DEFAULT PLST_EXTM3U	//

// id3savefield types
#define IST_DIRECTORY 1			// separate directories
#define IST_FILENAME  2			// write filename in the lines
#define IST_AT_FIXED  4			// put artist:title at fixed position
#define IST_TIME      8			// write time in the lines
#define IST_BITRATE   16		// write bitrate
#define IST_FILESIZE  32		// write filesize
#define IST_FULLPATH  64		//
#define IST_DEFAULT  (IST_DIRECTORY|IST_AT_FIXED|IST_TIME|IST_BITRATE|IST_FILESIZE)	//=61

// mxu flags (old mptime[]/pei->timesec)
#define MXUFLAG_TIMEMASK    0x000fffff	// time (in seconds) is stored on 20 bits (it's enough for a more than 12 days long audio)
#define MXUFLAG_ENABLED     0x80000000	// audio file

// pei->infobits
#define PEIF_ENABLED        1	// enabled entry (checked) - an audio file
#define PEIF_ID3LOADED      2	// id3 loaded from the file (1 time at least)
#define PEIF_ID3EXIST       4	// file has id3 info
#define PEIF_RNDPLAYED      8	// entry is already played in random mode
#define PEIF_RNDSIGNED     16	//
#define PEIF_FULLTIMEADDED 32	// entry has been added to fulltime
#define PEIF_SELECTED      64	// entry is selected in playlist editor
#define PEIF_INDEXED      128	// entry is indexed (by CUE)
#define PEIF_ALLOCATED    256	// pointers use dynamic data (allocated with malloc)
#define PEIF_SORTED       512	// entry is already sorted (don't re-order it again)
#define PEIF_ID3MASK     (PEIF_ID3LOADED|PEIF_ID3EXIST)
#define PEIF_COPYMASK    (PEIF_ENABLED)
#define PEIF_CUESAVEMASK (PEIF_ENABLED|PEIF_RNDPLAYED|PEIF_INDEXED)

// add/delete/found
#define EDITLIST_MODE_FILENAME 1
#define EDITLIST_MODE_HEAD     2
#define EDITLIST_MODE_INDEX    4
#define EDITLIST_MODE_ID3      8
#define EDITLIST_MODE_ENTRY    16
#define EDITLIST_MODE_ALL      (EDITLIST_MODE_FILENAME|EDITLIST_MODE_HEAD|EDITLIST_MODE_INDEX|EDITLIST_MODE_ID3|EDITLIST_MODE_ENTRY)

//------------------------------------------------------------------------
//playlist structures

typedef int check_order_func_t(struct playlist_entry_info *pei0, struct playlist_entry_info *pei1);

#include "diskdriv/diskdriv.h"

/*typedef struct playlist_id3func_s{
 void *(*create_new_entry)(void *passdata); // passdata=psi, returns *entry (pei)
 unsigned int (*add_filename)(void *passdata,void *entry,char *filename);
 unsigned int (*add_id3tag)(void *passdata,void *entry,unsigned int i3i_num,char *tagstr);
 unsigned int (*delete_id3tag)(void *passdata,void *entry,unsigned int i3i_num);
 unsigned int (*delete_all_id3tags)(void *passdata,void *entry);
 unsigned int (*delete_entry)(void *passdata,void *entry);
}playlist_id3func_s;*/

typedef struct playlist_side_info {
	unsigned long editsidetype;	// side infobits
	unsigned long editloadtype;	// PLL_ bits for side

	struct playlist_entry_info *editorhighline;

	struct playlist_entry_info *firstentry;	// 1. playlist entry
	struct playlist_entry_info *firstsong;	// 1. audio song (usually)
	struct playlist_entry_info *lastentry;	// last used entry
	struct playlist_entry_info *endentry;	// last possible entry

	int allfilenum;				// all possible entries (on this side)

	char *filenamesbeginp;		// area begin
	char *filenameslastp;		// first unused (last used + 1) memory point
	char *filenamesendp;		// area end

	char *id3infobeginp;		// area begin
	char *id3infolastp;			// first unused (last used + 1) memory point
	char *id3infoendp;			// area end

	struct playlist_entry_info *chkfilenum_begin;
	struct playlist_entry_info *chkfilenum_curr;
	struct playlist_entry_info *chkfilenum_end;

	unsigned int fulltimesec;
	unsigned int selected_files;	// PEIF_SELECTED

	check_order_func_t *id3ordertype[PLAYLIST_MAX_ORDERKEYS];

	struct playlist_side_info *psio;	// other side's info
	struct mainvars *mvp;		// &mvps

	struct mpxplay_diskdrive_data_s *mdds;

	unsigned int currdrive;		// -db2
	char currdir[MAX_PATHNAMELEN];	// -db2

	unsigned int savelist_type;	// PLST_xxx
	unsigned long savelist_textcodetype;	// utf16,utf8,ascii
	char savelist_filename[MAX_PATHNAMELEN];	//

	unsigned int sublistlevel;	// -db2
	char sublistnames[MAX_SUBLIST_LEVELS + 1][MAX_PATHNAMELEN];
} playlist_side_info;

typedef struct playlist_entry_info {
	unsigned long entrytype;	// HFT_ << 28
	unsigned long infobits;		// PEIF_
	unsigned long timemsec;		// time length in milliseconds
	mpxp_filesize_t filesize;
	char *filename;
	char *id3info[I3I_MAX + 1];
	struct mpxplay_diskdrive_data_s *mdds;
	//struct mpxplay_filehand_low_func_s *filehand_funcs; // ??? low level file-handling
	struct mpxplay_infile_func_s *infile_funcs;	//
	unsigned long pstime;		// play-start-time of the entry in msecs (CUE)
	unsigned long petime;		// play-count-time of the entry in msecs (CUE)
	struct playlist_entry_info *myself;	// sort helper
	pds_fdate_t filedate;		// !!! breaks the au_cards api, but here (last pos) it doesn't cause problems
} playlist_entry_info;			// 72 bytes (!!! update editlist.c asm if this changes)

//-------------------------------------------------------------------------
//playlist functions

//playlist.c
extern void mpxplay_playlist_init(struct mainvars *);
extern void mpxplay_playlist_close(void);
extern void playlist_init_pointers(struct mainvars *);
extern void playlist_peimyself_reset(struct playlist_side_info *psi, struct playlist_entry_info *firstentry, struct playlist_entry_info *lastentry);
extern struct playlist_entry_info *playlist_peimyself_search(struct playlist_side_info *psi, struct playlist_entry_info *pei_src);
extern void playlist_enable_side(struct playlist_side_info *);
extern void playlist_disable_side_partial(struct playlist_side_info *);
extern void playlist_disable_side_list(struct playlist_side_info *);
extern void playlist_disable_side_full(struct playlist_side_info *);
extern void playlist_clear_side(struct playlist_side_info *);
extern void playlist_reset_side(struct playlist_side_info *);
extern void playlist_copy_side_infos(struct playlist_side_info *psi1, struct playlist_side_info *psi2);
extern void playlist_clear_freeopts(void);

extern char *mpxplay_playlist_startdir(void);
extern void mpxplay_save_startdir(struct mainvars *);
extern void mpxplay_restore_startdir(void);
extern void mpxplay_playlist_startfile_fullpath(char *fullpath, char *filename);

extern void playlist_get_allfilenames(struct mainvars *);
extern unsigned int playlist_buildlist_one(struct playlist_side_info *psi, char *listfile, unsigned int loadtype, char *dslp, char *filtermask);
extern unsigned int playlist_buildlist_all(struct playlist_side_info *psi);
extern void playlist_init_playside(struct mainvars *);
extern void playlist_init_playsong(struct mainvars *);
extern void playlist_start_sideplay(struct mainvars *mvp, struct playlist_side_info *psi);
extern unsigned int playlist_open_infile(struct frame *frp, struct mainvars *mvp, struct playlist_entry_info *pei);
extern void playlist_close_infile(struct frame *, struct mainvars *);
extern void playlist_pei0_set(struct mainvars *mvp, struct playlist_entry_info *pei, unsigned long update_mode);

extern void playlist_editorhighline_check(struct playlist_side_info *psi);
extern void playlist_editorhighline_seek(struct playlist_side_info *psi, long offset, int whence);
extern void playlist_editorhighline_set(struct playlist_side_info *, struct playlist_entry_info *);
extern void playlist_editorhighline_set_nocenter(struct playlist_side_info *psi, struct playlist_entry_info *pei);
extern void playlist_change_editorside(struct mainvars *);

extern playlist_entry_info *playlist_search_filename(struct playlist_side_info *psi, char *filename, long timempos, struct playlist_entry_info *pei_begin);
extern void playlist_search_lastdir(struct playlist_side_info *psi, char *lastdir);
extern void playlist_search_firstsong(struct playlist_side_info *psi);
extern void playlist_change_sublist_or_directory(struct playlist_side_info *, unsigned long head);
extern void playlist_reload_side(struct mainvars *, struct playlist_side_info *);
extern void playlist_reload_dirs(struct mainvars *);

extern void playlist_write_id3tags(struct mainvars *);
extern void playlist_savelist_save_list(struct mainvars *);

//chkentry.c
extern void playlist_chkentry_get_allfileinfos(struct mainvars *);
extern void playlist_chkentry_get_onefileinfos_open(struct playlist_side_info *, struct playlist_entry_info *);
extern void playlist_chkentry_get_onefileinfos_is(struct playlist_side_info *, struct playlist_entry_info *);
extern void playlist_chkentry_get_onefileinfos_allagain(struct playlist_side_info *psi, struct playlist_entry_info *pei, struct frame *frp, unsigned int id3loadflags);

extern void playlist_chkfile_start_norm(struct playlist_side_info *psi, struct playlist_entry_info *startsong);
extern void playlist_chkfile_start_disp(struct playlist_side_info *psi, struct playlist_entry_info *startsong, struct playlist_entry_info *endsong);
extern void playlist_chkfile_start_ehline(struct playlist_side_info *psi, struct playlist_entry_info *pei);
extern void playlist_chkfile_stop(struct playlist_side_info *);

extern unsigned long playlist_entry_get_timemsec(struct playlist_entry_info *pei);
extern void playlist_fulltime_add(struct playlist_side_info *psi, struct playlist_entry_info *pei);
extern void playlist_fulltime_del(struct playlist_side_info *psi, struct playlist_entry_info *pei);
extern void playlist_fulltime_clearside(struct playlist_side_info *psi);
extern void playlist_fulltime_getside(struct playlist_side_info *psi);
extern unsigned int playlist_fulltime_getelapsed(struct mainvars *, unsigned int cleartime);

extern void playlist_chkentry_enable_entries(struct playlist_side_info *psi);

//editlist.c
extern struct playlist_entry_info *playlist_editlist_addfileone_postproc(struct playlist_side_info *psi_dest, struct playlist_entry_info *pei_dest);
extern void playlist_editlist_addfile_any(struct playlist_side_info *psi_src, struct playlist_entry_info *pei_src, char *source_filtermask);
extern void playlist_editlist_copy_entry(struct playlist_side_info *psi_src, struct playlist_entry_info *pei_src);
extern unsigned int playlist_editlist_addfile_one(struct playlist_side_info *psi_src, struct playlist_side_info *psi_dest, struct playlist_entry_info *pei_src, struct playlist_entry_info *pei_dest,
												  unsigned int modify);
extern void playlist_editlist_delfile_one(struct playlist_side_info *, struct playlist_entry_info *, unsigned int modify);
extern void playlist_editlist_delete_entry_manual(struct playlist_side_info *psi, struct playlist_entry_info *pei);
extern unsigned int playlist_editlist_allocated_copy_entry(struct playlist_entry_info *pei_dest, struct playlist_entry_info *pei_src);
extern void playlist_editlist_allocated_clear_entry(struct playlist_entry_info *pei);
extern void playlist_editlist_addfile_ins_ehl(struct playlist_side_info *psi_src, struct playlist_entry_info *pei_src);
extern void playlist_editlist_shiftfile(struct playlist_side_info *, int);
extern void playlist_editlist_mouse_shiftfile(struct mainvars *mvp, struct playlist_entry_info *);
extern void playlist_editlist_copyside(struct playlist_side_info *psi_src);
extern void playlist_editlist_addfile_selected_group(struct playlist_side_info *psi_src);
extern void playlist_editlist_copy_selected_group(struct playlist_side_info *psi_src);
extern void playlist_editlist_move_selected_group(struct playlist_side_info *psi_src);
extern void playlist_editlist_delfile_selected_group(struct playlist_side_info *psi);
extern void playlist_editlist_group_invert_selection(struct playlist_side_info *psi);
extern void playlist_editlist_group_select_all(struct playlist_side_info *psi);
extern void playlist_editlist_group_unselect_all(struct playlist_side_info *psi);
extern void playlist_editlist_groupselect_open(struct playlist_side_info *psi, unsigned int unselect);
extern void playlist_editlist_id3filter(struct mainvars *);
extern void playlist_editlist_insert_index(struct mainvars *mvp);
extern void playlist_editlist_delete_index(struct mainvars *mvp);
extern struct playlist_entry_info *playlist_editlist_updatesides_add_dft(struct mainvars *mvp, char *filename, unsigned int dft_type);

//diskfile.c
extern void playlist_diskfile_copy_or_move(struct mainvars *mvp, unsigned int move);
extern void playlist_diskfile_delete_init(struct mainvars *mvp);
extern void playlist_diskfile_show_multifileinfos(struct mainvars *mvp);
extern void playlist_diskfile_rename_by_id3(struct mainvars *mvp);

//fileinfo.c
extern void playlist_fileinfo_show_infos(struct mainvars *mvp);
extern void playlist_fileinfo_edit_infos(struct mainvars *mvp);

//id3list.c
extern void playlist_id3list_close(void);
extern void playlist_id3list_load(struct mainvars *, struct playlist_side_info *);
extern unsigned int get_onefileinfos_from_id3list(struct playlist_side_info *psi, struct playlist_entry_info *pei_dest, unsigned int fastsearch);
extern void playlist_id3list_save(struct mainvars *);

//jukebox.c
extern void playlist_jukebox_add_entry(struct mainvars *mvp, struct playlist_side_info *psi_src);
extern unsigned int playlist_jukebox_skip(struct mainvars *mvp);
extern void playlist_jukebox_switch(struct mainvars *mvp);
extern void playlist_jukebox_set(struct mainvars *mvp, unsigned int loadtype_priority);

//loaddir.c
extern void playlist_loaddir_diskdrives_unmount(void);
extern struct mpxplay_diskdrive_data_s *playlist_loaddir_drivenum_to_drivemap(int drivenum);
extern unsigned int playlist_get_extension_number(char *);
extern unsigned int playlist_get_extension_filetype(char *);
extern void playlist_loaddir_scandrives(struct playlist_side_info *psi, char *searchpath, char *dslp);
extern void playlist_loaddir_initdirside(struct playlist_side_info *psi, char *dir);
extern void playlist_loaddir_initbrowser(struct mainvars *mvp, char *dir);
extern void playlist_loaddir_buildbrowser(struct playlist_side_info *);
extern struct playlist_side_info *playlist_loaddir_changedir(struct playlist_side_info *psi, unsigned long head);
extern unsigned int playlist_loaddir_browser_gotodir(struct playlist_side_info *psi, char *newdir);
extern void playlist_loaddir_search_paralell_dir(struct playlist_side_info *psi, int step);
extern void playlist_loaddir_select_drive(struct mainvars *mvp, unsigned int side);
extern void playlist_loaddir_select_drive_retry(struct playlist_side_info *psi);
extern void playlist_loaddir_makedir_open(struct playlist_side_info *psi);
extern int playlist_loaddir_disk_unload_load(struct playlist_side_info *psi);
extern void playlist_loaddir_ftpurl_open(struct playlist_side_info *psi);
extern void playlist_loaddir_ftpurl_close(struct playlist_side_info *psi);

//loadlist.c
extern unsigned int playlist_loadlist_check_extension(char *filename);
extern unsigned int playlist_loadlist_mainload(struct playlist_side_info *psi, char *listname, unsigned int loadtype, char *filtermask);
extern unsigned int playlist_loadlist_get_header_by_ext(struct playlist_side_info *psi, struct playlist_entry_info *pei, char *filename);
extern void playlist_loadlist_load_autosaved_list(struct playlist_side_info *psi);

//loadsub.c
extern unsigned int playlist_loadsub_check_extension(struct playlist_side_info *psi, char *filename);
extern void playlist_loadsub_setnewinputfile(struct playlist_side_info *psi, char *newinputfile, unsigned long loadtype);
extern char *playlist_loadsub_getinputfile(struct playlist_side_info *psi);
extern void playlist_loadsub_addsubdots(struct playlist_side_info *psi);
extern struct playlist_side_info *playlist_loadsub_sublist_change(struct playlist_side_info *psi, unsigned long head);
extern void playlist_loadsub_search_paralell_list(struct playlist_side_info *psi, int step);
extern unsigned int playlist_loadsub_sublist_setlevels(struct playlist_side_info *psi, char *sublists);
extern unsigned int playlist_loadsub_sublist_getlevels(struct playlist_side_info *psi, char *destbuf, unsigned int buflen);
extern void playlist_loadsub_sublist_clear(struct playlist_side_info *psi);
extern char *playlist_loadsub_get_dftstr(unsigned int entrytype);

//randlist.c
extern void playlist_randlist_clearall(struct playlist_side_info *);
extern void playlist_randlist_delete(struct playlist_entry_info *);
extern void playlist_randlist_correctq(struct playlist_side_info *psi, struct playlist_entry_info *firstentry, struct playlist_entry_info *lastentry);
extern void playlist_randlist_xchq(struct playlist_entry_info *pei1, struct playlist_entry_info *pei2);
extern unsigned int playlist_getsongcounter(struct mainvars *mvp);
extern void playlist_randlist_pushq(struct playlist_side_info *psi, struct playlist_entry_info *);
extern playlist_entry_info *playlist_randlist_popq(void);
extern void playlist_randlist_setsignflag(struct playlist_entry_info *);
extern void playlist_randlist_resetsignflag(struct playlist_entry_info *pei);
extern struct playlist_entry_info *playlist_randlist_getnext(struct playlist_side_info *);
extern struct playlist_entry_info *playlist_randlist_getprev(struct playlist_entry_info *loc_newfilenum);
extern void playlist_randlist_randomize_side(struct playlist_side_info *psi);

//savelist.c
extern char *playlist_savelist_get_savename(unsigned int list_type);
extern unsigned int playlist_savelist_save_playlist(struct mainvars *mvp, struct playlist_side_info *psi, char *file_name, unsigned int list_type);
extern unsigned int playlist_savelist_save_editedside(struct playlist_side_info *psi);
extern void playlist_savelist_manual_save(struct mainvars *mvp);
extern void playlist_savelist_clear(struct playlist_side_info *psi);

//skiplist.c
extern unsigned int playlist_skip(struct mainvars *mvp);
extern struct playlist_entry_info *playlist_get_newfilenum(struct mainvars *mvp);
extern struct playlist_entry_info *playlist_get_nextalbum(struct playlist_entry_info *, struct playlist_side_info *, int step, unsigned int steplevel, unsigned int ring);
extern void playlist_newsong_enter(struct mainvars *, struct playlist_side_info *psie);
extern void playlist_nextsong_select(struct mainvars *, struct playlist_side_info *psie);
extern void playlist_skiplist_reset_loadnext(struct mainvars *mvp);

//sortlist.c
extern void playlist_sortlist_init(struct mainvars *mvp);
#define PLAYLIST_SORTLIST_SELECTFUNCS_PLAYER    0
#define PLAYLIST_SORTLIST_SELECTFUNCS_COMMANDER 1
extern void playlist_sortlist_selectfuncs(unsigned int select);
extern void playlist_sortlist_selectorder(struct playlist_side_info *psi, unsigned int key, unsigned int type);
extern void playlist_sortlist_clearorder(struct playlist_side_info *psi);
extern mpxp_uint32_t playlist_sortlist_get_orderkeys_in_hexa(struct playlist_side_info *psi);
extern void playlist_sortlist_set_orderkeys_from_hexa(struct playlist_side_info *psi, mpxp_uint32_t value);
extern struct playlist_entry_info *playlist_order_entry_block(struct playlist_side_info *psi, struct playlist_entry_info *pei_src, struct playlist_entry_info *firstentry,
															  struct playlist_entry_info *lastentry);
extern struct playlist_entry_info *playlist_order_entry(struct playlist_side_info *, struct playlist_entry_info *pei_src);
extern void playlist_order_block(struct playlist_side_info *psi, struct playlist_entry_info *firstentry, struct playlist_entry_info *lastentry);
extern void playlist_order_side(struct playlist_side_info *psi);
extern void playlist_order_dft(struct playlist_side_info *psi);
extern void playlist_order_filenames_block(struct playlist_side_info *psi, struct playlist_entry_info *firstentry, struct playlist_entry_info *lastentry);
extern void playlist_order_filenames(struct playlist_side_info *psi);
extern void playlist_swap_entries(struct playlist_side_info *psi, struct playlist_entry_info *e1, struct playlist_entry_info *e2);
extern unsigned int playlist_sortlist_is_preordered_type(check_order_func_t * ot);

//textconv.c
extern void mpxplay_playlist_textconv_init(void);
extern void mpxplay_playlist_textconv_close(void);
extern unsigned int mpxplay_playlist_textconv_do(char *str, unsigned int len, unsigned int doneconv);
extern unsigned int mpxplay_playlist_textconv_selected_do(char *str, unsigned int datalen, unsigned int select, unsigned int doneconv);
extern char *mpxplay_playlist_textconv_back(unsigned char *dest, unsigned char *src);
extern unsigned int mpxplay_playlist_textconv_selected_back(unsigned char *dest, unsigned int dest_buflen, char *src, unsigned int select);
#include "in_file.h"
extern struct mpxplay_textconv_func_s mpxplay_playlist_textconv_funcs;

#endif							// mpxplay_playlist_h
