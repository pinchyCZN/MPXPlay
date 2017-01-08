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
//function:m3u,pls,fpl,mxu loading

#include <string.h>
#include "playlist.h"
#include "newfunc\newfunc.h"
#include "control\control.h"
#include "display\display.h"
#include "diskdriv\diskdriv.h"

#define LOADLIST_MAX_LINELEN 1024	// have to be enough usually

typedef struct listhandler_s {
	char *extension;
	struct playlist_entry_info *(*loadlist) (struct playlist_entry_info *, struct playlist_side_info *, char *listname, char *loaddir, char *filtermask);
} listhandler_s;

static struct playlist_entry_info *open_load_m3u(struct playlist_entry_info *, struct playlist_side_info *, char *listname, char *loaddir, char *filtermask);
static struct playlist_entry_info *load_m3u(struct playlist_entry_info *, struct playlist_side_info *, void *listfile, char *loaddir, char *filtermask);
static struct playlist_entry_info *open_load_pls(struct playlist_entry_info *, struct playlist_side_info *, char *listname, char *loaddir, char *filtermask);
static struct playlist_entry_info *open_load_mxu(struct playlist_entry_info *, struct playlist_side_info *, char *listname, char *loaddir, char *filtermask);
static struct playlist_entry_info *open_load_fpl(struct playlist_entry_info *, struct playlist_side_info *, char *listname, char *loaddir, char *filtermask);
static struct playlist_entry_info *open_load_cue(struct playlist_entry_info *, struct playlist_side_info *, char *listname, char *loaddir, char *filtermask);

static struct listhandler_s listhandlers[] = {
	{"m3u", &open_load_m3u},
	{"m3u8", &open_load_m3u},
	{"pls", &open_load_pls},
	{"mxu", &open_load_mxu},
	{"fpl", &open_load_fpl},
	{"cue", &open_load_cue},
	{NULL, NULL}
};

extern unsigned int loadid3tag, id3textconv, playlistsave, uselfn, desktopmode;

static struct listhandler_s *select_listhandler_by_ext(char *filename)
{
	struct listhandler_s *lih;
	char *ext = pds_strrchr(filename, '.');
	if(ext) {
		ext++;
		lih = &listhandlers[0];
		do {
			if(pds_stricmp(lih->extension, ext) == 0)
				return lih;
			lih++;
		} while(lih->extension);
	}
	return NULL;
}

unsigned int playlist_loadlist_check_extension(char *filename)
{
	if(select_listhandler_by_ext(filename))
		return 1;
	return 0;
}

unsigned int playlist_loadlist_mainload(struct playlist_side_info *psi, char *listname, unsigned int loadtype, char *filtermask)
{
	struct listhandler_s *listhand;
	struct playlist_entry_info *pei, *begine;
	char loaddir[MAX_PATHNAMELEN];

	display_clear_timed_message();

	display_message(0, 0, "Loading list (press ESC to stop) ...");

	pei = begine = psi->lastentry + 1;

	if(loadtype & PLL_STDIN) {
		pei = load_m3u(pei, psi, stdin, mpxplay_playlist_startdir(), filtermask);
		psi->lastentry = pei - 1;
	} else {
		listhand = select_listhandler_by_ext(listname);
		if(listhand) {
			pds_getpath_from_fullname(loaddir, listname);
			pei = listhand->loadlist(pei, psi, listname, loaddir, filtermask);
		}
		psi->lastentry = pei - 1;
	}

	if(pei > psi->firstentry)
		playlist_enable_side(psi);
	else
		playlist_disable_side_full(psi);

	clear_message();

	if(pei == begine)
		return 0;
	return 1;
}

//-------------------------------------------------------------------------

static struct playlist_entry_info *open_load_m3u(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *listname, char *loaddir, char *filtermask)
{
	void *listfile;

	if((listfile = mpxplay_diskdrive_textfile_open(NULL, listname, (O_RDONLY | O_TEXT))) != NULL) {
		if(pei <= psi->firstsong)
			psi->savelist_textcodetype = mpxplay_diskdrive_textfile_config(listfile, MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_SRC, NULL, NULL);
		pei = load_m3u(pei, psi, listfile, loaddir, filtermask);
		mpxplay_diskdrive_textfile_close(listfile);
	}
	return pei;
}

static struct playlist_entry_info *load_m3u(struct playlist_entry_info *pei, struct playlist_side_info *psi, void *listfile, char *loaddir, char *filtermask)
{
	char *ps, *pa, *pt;			// string pointer of timesec,artist,title
	int timesec;
	unsigned int len, firstline = 1, linecount = 0;
	char strtmp[LOADLIST_MAX_LINELEN], extinfo[LOADLIST_MAX_LINELEN] = "";

	funcbit_disable(psi->editsidetype, (PLT_EXTENDED | PLT_EXTENDINC));	// ???

	while((pei <= psi->endentry) && (psi->filenameslastp < psi->filenamesendp)) {
		if(listfile == stdin) {
			if(!fgets(strtmp, sizeof(strtmp) - 1, listfile))
				break;
			if(pds_look_extgetch() == KEY_ESC) {
				pds_extgetch();
				break;
			}
			len = pds_strlen(strtmp);
			if(len) {
				pa = &strtmp[len - 1];
				do {
					if((*pa == '\r') || (*pa == '\n'))
						*pa = 0;
					else
						break;
					pa--;
				} while(--len);
			}
		} else {
			if(!mpxplay_diskdrive_textfile_readline(listfile, strtmp, sizeof(strtmp) - 1))
				break;
		}
		if(!strtmp[0])
			continue;
		if(firstline) {
			if(pds_strncmp(strtmp, "#EXTM3U", 7) == 0)
				funcbit_enable(psi->editsidetype, PLT_EXTENDED);
			firstline = 0;
		}
		if(strtmp[0] == '#') {
			if(pds_strncmp(strtmp, "#EXTINF:", 8) != 0) {
				extinfo[0] = 0;	// ???
				continue;
			}
			pds_strcpy(extinfo, strtmp);
			continue;
		}

		if(!filtermask || pds_filename_wildchar_cmp(strtmp, filtermask)) {
			pei->filename = psi->filenameslastp;
			pds_filename_conv_slashes_to_local(strtmp);
			len = pds_filename_build_fullpath(pei->filename, loaddir, strtmp);
			if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
				pds_fullpath(pei->filename, pei->filename);
				len = pds_strlen(pei->filename);
			}
			if(id3textconv & ID3TEXTCONV_FILENAME)
				len = mpxplay_playlist_textconv_do(pei->filename, len, ~ID3TEXTCONV_CODEPAGE);
			psi->filenameslastp += len + 1;

			if(extinfo[0]) {
				ps = &extinfo[8];	// begin of timesec
				pa = pds_strchr(ps, ',');
				if(pa) {
					pa[0] = 0;	// end of timesec
					timesec = pds_atol(ps);
					if(timesec >= 0) {
						pei->timemsec = timesec * 1000;
						funcbit_enable(pei->infobits, PEIF_ENABLED);
					}

					if((loadid3tag & ID3LOADMODE_LIST) && (psi->id3infolastp < psi->id3infoendp)) {
						pa++;	// begin of artist
						pt = pds_strstr(pa, " - ");	// search for the ' - ' separator (artist-title)
						if(pt) {
							pt[0] = 0;	// end of artist
							pt += 3;	// begin of title
						}
						len = pds_strcpy(psi->id3infolastp, pa);
						len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, ID3TEXTCONV_UTF_ALL);
						if(len) {
							pei->id3info[I3I_ARTIST] = psi->id3infolastp;
							psi->id3infolastp += len + 1;
							funcbit_enable(pei->infobits, PEIF_ID3EXIST);
						}
						if(pt) {
							len = pds_strcpy(psi->id3infolastp, pt);
							len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, ID3TEXTCONV_UTF_ALL);
							if(len) {
								pei->id3info[I3I_TITLE] = psi->id3infolastp;
								psi->id3infolastp += len + 1;
								funcbit_enable(pei->infobits, PEIF_ID3EXIST);
							}
						}
					}
				}
			}

			if(psi->editsidetype & PLT_EXTENDED)	// #EXTM3U
				if(!(pei->infobits & PEIF_ENABLED))	// entry has no #EXTINF
					funcbit_enable(psi->editsidetype, PLT_EXTENDINC);	// incomplete extm3u

			pei++;
			if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
				sprintf(strtmp, "Loading list: %4d. (press ESC to stop)", ++linecount);
				display_message(0, 0, strtmp);
			}
		}
		extinfo[0] = 0;
	}
	return pei;
}

//------------------------------------------------------------------------

static struct playlist_entry_info *open_load_pls(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *listname, char *loaddir, char *filtermask)
{
	void *listfile;
	unsigned int len, first = 1, validfile = 0, linecount = 0;
	char *datap, strtmp[LOADLIST_MAX_LINELEN];

	if((listfile = mpxplay_diskdrive_textfile_open(NULL, listname, (O_RDONLY | O_TEXT))) == NULL)
		return pei;

	if(pei <= psi->firstsong)
		psi->savelist_textcodetype = mpxplay_diskdrive_textfile_config(listfile, MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_SRC, NULL, NULL);

	funcbit_enable(psi->editsidetype, PLT_EXTENDED);

	pei--;

	while(mpxplay_diskdrive_textfile_readline(listfile, strtmp, sizeof(strtmp) - 1) && (pei <= psi->endentry) && (psi->filenameslastp < psi->filenamesendp)) {
		datap = pds_strchr(strtmp, '=');
		if(!datap)
			continue;
		*datap++ = 0;

		if(pds_strncmp(strtmp, "File", sizeof("File") - 1) == 0) {
			if(!first && !(pei->infobits & PEIF_ENABLED))
				funcbit_enable(psi->editsidetype, PLT_EXTENDINC);

			first = 0;
			if(filtermask && !pds_filename_wildchar_cmp(datap, filtermask)) {
				validfile = 0;
				continue;
			}
			pei++;
			pei->filename = psi->filenameslastp;
			pds_filename_conv_slashes_to_local(datap);
			len = pds_filename_build_fullpath(pei->filename, loaddir, datap);
			if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
				pds_fullpath(pei->filename, pei->filename);
				len = pds_strlen(pei->filename);
			}
			if(id3textconv & ID3TEXTCONV_FILENAME)
				len = mpxplay_playlist_textconv_do(pei->filename, len, ~ID3TEXTCONV_CODEPAGE);
			psi->filenameslastp += len + 1;
			if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
				sprintf(strtmp, "Loading list: %4d. (press ESC to stop)", ++linecount);
				display_message(0, 0, strtmp);
			}
			validfile = 1;
			continue;
		}
		if(first || !validfile)
			continue;
		if((loadid3tag & ID3LOADMODE_LIST) && (psi->id3infolastp < psi->id3infoendp)) {
			if(pds_strncmp(strtmp, "Title", sizeof("Title") - 1) == 0) {
				if(!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE]) {
					char *pt = pds_strstr(datap, " - ");	// Winamp style
					if(!pt)
						pt = pds_strstr(datap, " / ");	// Sonique style
					if(pt) {
						pt[0] = 0;	// end of artist
						pt += 3;	// begin of title
					}
					len = pds_strcpy(psi->id3infolastp, datap);
					len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, 0);
					if(len) {
						pei->id3info[I3I_ARTIST] = psi->id3infolastp;
						psi->id3infolastp += len + 1;
						funcbit_enable(pei->infobits, PEIF_ID3EXIST);
					}
					if(pt) {
						len = pds_strcpy(psi->id3infolastp, pt);
						len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, 0);
						if(len) {
							pei->id3info[I3I_TITLE] = psi->id3infolastp;
							psi->id3infolastp += len + 1;
							funcbit_enable(pei->infobits, PEIF_ID3EXIST);
						}
					}
				}
				continue;
			}
		}
		if(pds_strncmp(strtmp, "Length", sizeof("Length") - 1) == 0) {
			int timesec = pds_atol(datap);
			if(timesec >= 0) {
				pei->timemsec = timesec * 1000;
				funcbit_enable(pei->infobits, PEIF_ENABLED);
			}
		}
	}
	pei++;
	mpxplay_diskdrive_textfile_close(listfile);
	return pei;
}

//-------------------------------------------------------------------------

static struct playlist_entry_info *open_load_mxu(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *listname, char *loaddir, char *filtermask)
{
	void *listfile;
	unsigned int len, timesec, linecount = 0;
	char *listparts[MAX_ID3LISTPARTS + 2];
	char strtmp[LOADLIST_MAX_LINELEN], strtemp2[LOADLIST_MAX_LINELEN];

	if((listfile = mpxplay_diskdrive_textfile_open(NULL, listname, (O_RDONLY | O_TEXT))) == NULL)
		return pei;

	if(pei <= psi->firstsong)
		psi->savelist_textcodetype = mpxplay_diskdrive_textfile_config(listfile, MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_SRC, NULL, NULL);

	funcbit_enable(psi->editsidetype, PLT_EXTENDED);

	while(mpxplay_diskdrive_textfile_readline(listfile, strtmp, sizeof(strtmp) - 1) && (pei <= psi->endentry) && (psi->filenameslastp < psi->filenamesendp)) {
		if(strtmp[0]) {
			if(strtmp[0] != '#') {
				if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
					sprintf(strtemp2, "Loading list %4d (press ESC to stop)", ++linecount);
					display_message(0, 0, strtemp2);
				}
				pds_memset(&(listparts[0]), 0, MAX_ID3LISTPARTS * sizeof(char *));
				pds_strcpy(strtemp2, strtmp);
				pds_listline_slice(&(listparts[0]), "°±²", strtemp2);
				//new mxu style (from v1.43)
				if(listparts[0] && listparts[1] && listparts[2] && listparts[3]) {
					if(filtermask && !pds_filename_wildchar_cmp(listparts[0], filtermask))
						continue;
					pei->filename = psi->filenameslastp;
					if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
						pds_fullpath(pei->filename, listparts[0]);
						len = pds_strlen(psi->filenameslastp);
					} else
						len = pds_strcpy(pei->filename, listparts[0]);
					if(id3textconv & ID3TEXTCONV_FILENAME)
						len = mpxplay_playlist_textconv_do(pei->filename, len, ~ID3TEXTCONV_CODEPAGE);
					psi->filenameslastp += len + 1;

					if((loadid3tag & ID3LOADMODE_LIST) && (psi->id3infolastp < psi->id3infoendp)) {
						if(listparts[1][0]) {
							len = pds_strcpy(psi->id3infolastp, listparts[1]);
							len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, 0);
							if(len) {
								pei->id3info[I3I_ARTIST] = psi->id3infolastp;
								psi->id3infolastp += len + 1;
								funcbit_enable(pei->infobits, PEIF_ID3EXIST);
							}
						}
						if(listparts[2][0]) {
							len = pds_strcpy(psi->id3infolastp, listparts[2]);
							len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, 0);
							if(len) {
								pei->id3info[I3I_TITLE] = psi->id3infolastp;
								psi->id3infolastp += len + 1;
								funcbit_enable(pei->infobits, PEIF_ID3EXIST);
							}
						}
					}
					timesec = pds_atol16(listparts[3]);
					if(timesec & MXUFLAG_ENABLED) {
						pei->timemsec = (timesec & MXUFLAG_TIMEMASK) * 1000;
						funcbit_enable(pei->infobits, PEIF_ENABLED);
					} else {
						pei->timemsec = 0;
						playlist_loadlist_get_header_by_ext(psi, pei, pei->filename);
					}

				} else {
					// old mxu style (v1.42)
					pds_memset(&(listparts[0]), 0, MAX_ID3LISTPARTS * sizeof(char *));
					pds_strcpy(strtemp2, strtmp);
					pds_listline_slice(&(listparts[0]), "°:±", strtemp2);

					if(listparts[0]) {
						if(filtermask && !pds_filename_wildchar_cmp(listparts[0], filtermask))
							continue;
						pei->filename = psi->filenameslastp;
						if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
							pds_fullpath(pei->filename, listparts[0]);
							len = pds_strlen(psi->filenameslastp);
						} else
							len = pds_strcpy(pei->filename, listparts[0]);
						if(id3textconv & ID3TEXTCONV_FILENAME)
							len = mpxplay_playlist_textconv_do(pei->filename, len, ~ID3TEXTCONV_CODEPAGE);
						psi->filenameslastp += len + 1;
					}
					if((loadid3tag & ID3LOADMODE_LIST) && (psi->id3infolastp < psi->id3infoendp)) {
						if(listparts[1]) {
							if(pds_strcutspc(listparts[1])) {
								len = pds_strcpy(psi->id3infolastp, listparts[1]);
								len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, 0);
								if(len) {
									pei->id3info[I3I_ARTIST] = psi->id3infolastp;
									psi->id3infolastp += len + 1;
									funcbit_enable(pei->infobits, PEIF_ID3EXIST);
								}
							}
						}
						if(listparts[2]) {
							if(pds_strcutspc(listparts[2])) {
								len = pds_strcpy(psi->id3infolastp, listparts[2]);
								len = mpxplay_playlist_textconv_do(psi->id3infolastp, len, 0);
								if(len) {
									pei->id3info[I3I_TITLE] = psi->id3infolastp;
									psi->id3infolastp += len + 1;
									funcbit_enable(pei->infobits, PEIF_ID3EXIST);
								}
							}
						}
					}
					if(listparts[3]) {
						funcbit_enable(pei->infobits, PEIF_ENABLED);
					}
				}
				pei++;
			}
		}
	}
	mpxplay_diskdrive_textfile_close(listfile);
	return pei;
}

//------------------------------------------------------------------------
// FPL loading (only filenames without extended informations)
// for Foobar2000 v1.0.1 saved fpl files

typedef struct fpl_main_header_s {
	unsigned long a;
	unsigned long b;
	unsigned long c;
	unsigned long e;
	unsigned long size_of_entries;
} fpl_main_header_s;

static struct playlist_entry_info *open_load_fpl(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *listname, char *loaddir, char *filtermask)
{
	void *listfile;
	unsigned long bytecount = 0, filecount = 0, convdone, len;
	struct fpl_main_header_s fpl_mainhead;
	char *s, strtmp[LOADLIST_MAX_LINELEN];

	if((listfile = mpxplay_diskdrive_textfile_open(NULL, listname, (O_RDONLY | O_BINARY))) == NULL)
		return pei;

	if(id3textconv & ID3TEXTCONV_UTF_AUTO)
		convdone = ID3TEXTCONV_UTF_ALL;
	else
		convdone = ~ID3TEXTCONV_CODEPAGE;

	len = mpxplay_diskdrive_textfile_readline(listfile, (char *)&fpl_mainhead, sizeof(struct fpl_main_header_s));
	if(len < sizeof(struct fpl_main_header_s))
		goto err_out_fpl;
	mpxplay_diskdrive_textfile_config(listfile, MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_SEARCHSTRZ_ENABLE, NULL, NULL);

	do {
		len = mpxplay_diskdrive_textfile_readline(listfile, strtmp, sizeof(strtmp) - 1);
		if(!len)
			break;
		bytecount += len + 1;
		if(bytecount >= fpl_mainhead.size_of_entries)
			break;
		s = &strtmp[0];
		if(pds_strncmp(s, "file://", sizeof("file://") - 1) == 0) {
			s += sizeof("file://") - 1;
			if(!filtermask || pds_filename_wildchar_cmp(s, filtermask)) {
				pds_filename_conv_slashes_to_local(s);
				pei->filename = psi->filenameslastp;
				len = pds_filename_build_fullpath(pei->filename, loaddir, s);
				if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
					pds_fullpath(pei->filename, pei->filename);
					len = pds_strlen(pei->filename);
					sprintf(strtmp, "Loading list %4d (press ESC to stop)", filecount++);
					display_message(0, 0, strtmp);
				}
				if(id3textconv & ID3TEXTCONV_UTF_AUTO)
					len = mpxplay_playlist_textconv_funcs.utf8_to_char(pei->filename, len);	// fpl stores filenames in UTF8
				if(id3textconv & ID3TEXTCONV_FILENAME)
					len = mpxplay_playlist_textconv_do(pei->filename, len, convdone);
				psi->filenameslastp += len + 1;
				pei++;
			}
		}
	} while(1);

  err_out_fpl:
	mpxplay_diskdrive_textfile_close(listfile);
	return pei;
}

//-------------------------------------------------------------------------
// CUE loading

typedef struct cue_handler_s {
	char *command;
	struct playlist_entry_info *(*handler) (struct playlist_entry_info * pei, struct playlist_side_info * psi, char *comm_arg, char *loaddir, char *filtermask);
} cue_handler_s;

static char *cue_cut_string(char *comm_arg)	// cut " at begin and end
{
	char *s;

	if(*comm_arg == '\"') {
		comm_arg++;
		s = strchr(comm_arg, '\"');
		if(s)
			*s = 0;
	} else {
		s = strrchr(comm_arg, '\"');
		if(s)
			*s = 0;
		else {
			s = strchr(comm_arg, ' ');
			if(s)
				*s = 0;
		}
	}
	return comm_arg;
}

static unsigned int cue_allcount, cue_filecount, cue_validfile, cue_newfile;
static unsigned int cue_trackcount, cue_indexcount;
static char cue_track_artist[MAX_ID3LEN / 2], cue_track_title[MAX_ID3LEN / 2];
static char cue_main_artist[MAX_ID3LEN / 2], cue_main_title[MAX_ID3LEN / 2];
static unsigned int cue_main_filenumpos_artist, cue_main_filenumpos_title;

static void cue_check_index(struct playlist_entry_info *plx, struct playlist_side_info *psi)
{
	if(cue_trackcount <= 1)		// if file has only one track/index
		cue_trackcount = 0;
	if(cue_indexcount <= 1)
		cue_indexcount = 0;
	if(!cue_trackcount && !cue_indexcount && (plx >= psi->firstsong) && (plx <= psi->endentry) && !plx->pstime && !plx->petime)	// then it's not indexed really
		funcbit_disable(plx->infobits, PEIF_INDEXED);
}

static void cue_set_id3info(struct playlist_entry_info *pei, struct playlist_side_info *psi)
{
	unsigned int len;

	if(pei < psi->firstsong)
		return;

	if(!pei->id3info[I3I_ARTIST] && (cue_track_artist[0] || cue_main_artist[0] || (cue_main_title[0] && cue_track_title[0])) && (psi->id3infolastp < psi->id3infoendp)) {
		char *p;
		if(cue_track_artist[0])
			p = &cue_track_artist[0];
		else if(cue_main_artist[0])
			p = &cue_main_artist[0];
		else if(cue_main_title[0] && cue_track_title[0])	// main title may be an album name
			p = &cue_main_title[0];
		len = pds_strcpy(psi->id3infolastp, p);
		if(len > 0) {
			pei->id3info[I3I_ARTIST] = psi->id3infolastp;
			psi->id3infolastp += len + 1;
			funcbit_enable(pei->infobits, PEIF_ID3EXIST);
		}
	}
	if(!pei->id3info[I3I_TITLE] && (psi->id3infolastp < psi->id3infoendp)) {
		char *title;
		if(cue_track_title[0]) {
			title = &cue_track_title[0];
			if(cue_indexcount)
				len = snprintf(psi->id3infolastp, MAX_ID3LEN / 2, "%s%s(index %d.)", title, ((title[0]) ? " " : ""), cue_indexcount);
			else
				len = pds_strcpy(psi->id3infolastp, title);
		} else {
			if(cue_main_title[0])
				title = &cue_main_title[0];
			else
				title = pds_getfilename_from_fullname(pei->filename);
			if(cue_trackcount)
				len = snprintf(psi->id3infolastp, MAX_ID3LEN / 2, "%s%s(track %d.)", title, ((title[0]) ? " " : ""), cue_trackcount);
			else if(cue_indexcount)
				len = snprintf(psi->id3infolastp, MAX_ID3LEN / 2, "%s%s(index %d.)", title, ((title[0]) ? " " : ""), cue_indexcount);
			else
				len = 0;
		}

		if(len > 0) {
			pei->id3info[I3I_TITLE] = psi->id3infolastp;
			psi->id3infolastp += len + 1;
			funcbit_enable(pei->infobits, PEIF_ID3EXIST);
		}
	}
	if(!pei->id3info[I3I_ALBUM] && cue_main_title[0] && (psi->id3infolastp < psi->id3infoendp)) {	// && (loadid3tag&ID3LOADMODE_PREFER_LIST)){
		len = pds_strcpy(psi->id3infolastp, cue_main_title);
		pei->id3info[I3I_ALBUM] = psi->id3infolastp;
		psi->id3infolastp += len + 1;
	}
}

static struct playlist_entry_info *cue_commandhand_file(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	unsigned int len = 0;
	char sout[128];

	if(cue_filecount > cue_main_filenumpos_artist)	// obsolete main artist
		cue_main_artist[0] = 0;
	if(cue_filecount > cue_main_filenumpos_title)	// or title
		cue_main_title[0] = 0;
	cue_filecount++;

	if(cue_validfile) {
		struct playlist_entry_info *plx = (cue_newfile) ? pei : pei - 1;
		cue_check_index(plx, psi);
		cue_set_id3info(plx, psi);
	}

	if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
		sprintf(sout, "Loading list: %4d. (press ESC to stop)", ++cue_allcount);
		display_message(0, 0, sout);
	}

	comm_arg = cue_cut_string(comm_arg);

	if(!filtermask || pds_filename_wildchar_cmp(comm_arg, filtermask))
		cue_validfile = 1;
	else
		cue_validfile = 0;

	if(cue_validfile) {
		pds_filename_conv_slashes_to_local(comm_arg);
		len = pds_filename_build_fullpath(psi->filenameslastp, loaddir, comm_arg);
		if(uselfn & USELFN_AUTO_SFN_TO_LFN) {
			pds_fullpath(psi->filenameslastp, psi->filenameslastp);
			len = pds_strlen(psi->filenameslastp);
		}
	}

	if(len || !cue_validfile) {
		if(cue_newfile && cue_validfile)	// !!! (else pei was incremented in commandhand_index)
			pei++;
		if(pei > psi->endentry) {
			cue_newfile = 0;
			return pei;
		}
		if(cue_validfile) {
			pei->filename = psi->filenameslastp;
			if(id3textconv & ID3TEXTCONV_FILENAME)
				len = mpxplay_playlist_textconv_do(pei->filename, len, ~ID3TEXTCONV_CODEPAGE);
			psi->filenameslastp += len + 1;
			cue_newfile = 1;
		}
		cue_trackcount = 0;
		cue_indexcount = 0;
	}

	cue_track_artist[0] = cue_track_title[0] = 0;

	return pei;
}

static struct playlist_entry_info *cue_commandhand_track(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	if(cue_trackcount) {		// if not 1st track
		if(cue_indexcount <= 1)
			cue_indexcount = 0;
		cue_set_id3info(pei - 1, psi);	// then close prev track infos
	}
	if(cue_validfile) {
		cue_trackcount++;
		cue_track_artist[0] = cue_track_title[0] = 0;
		cue_indexcount = 0;
	}
	return pei;
}

static struct playlist_entry_info *cue_commandhand_performer(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	if(loadid3tag & ID3LOADMODE_LIST) {
		unsigned int len;
		char *destp;

		if(!cue_filecount) {	// PERFORMER before FILE
			destp = &cue_main_artist[0];
			cue_main_filenumpos_artist = cue_filecount;	// assigned to next file only
		} else {
			if(!cue_validfile || pei->id3info[I3I_ARTIST])
				return pei;
			destp = &cue_track_artist[0];
		}

		comm_arg = cue_cut_string(comm_arg);

		len = pds_strcpy(destp, comm_arg);
		len = mpxplay_playlist_textconv_do(destp, len, 0);
	}

	return pei;
}

static struct playlist_entry_info *cue_commandhand_title(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	if(loadid3tag & ID3LOADMODE_LIST) {
		unsigned int len;
		char *destp;

		if(!cue_filecount) {	// TITLE before FILE
			destp = &cue_main_title[0];
			cue_main_filenumpos_title = cue_filecount;	// assigned to next file only
		} else {
			if(!cue_validfile || pei->id3info[I3I_TITLE])
				return pei;
			destp = &cue_track_title[0];
		}

		comm_arg = cue_cut_string(comm_arg);

		len = pds_strcpy(destp, comm_arg);
		len = mpxplay_playlist_textconv_do(destp, len, 0);
	}

	return pei;
}

static struct playlist_entry_info *cue_commandhand_index(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	long len;
	unsigned long hextime, pt;
	struct playlist_entry_info *plx;

	if(pei <= psi->lastentry || !cue_validfile)	// first we need a filename (we haven't got it) ...
		return pei;

	if(cue_newfile)
		plx = NULL;
	else
		plx = pei - 1;			// previous entry

	if((comm_arg[0] == 'E') && (comm_arg[1] == 'E')) {	// magic index (EE) closes (sets the end of the) last index
		if(plx) {
			comm_arg += 2;		// skip EE
			while(*comm_arg == ' ')	// skip spaces after EE
				comm_arg++;
			hextime = pds_strtime_to_hextime(comm_arg, 1);	// convert MM:SS:FF to a hex number (xxMMSSFF)
			plx->petime = ((hextime >> 16) & 0x3fff) * 60 * 1000 + ((hextime >> 8) & 0xff) * 1000 + ((hextime & 0xff) * 1000 + 75 / 2) / 75;	// convert hex number to millisecs
			funcbit_enable(plx->infobits, PEIF_ENABLED);
		}
	} else {
		if(!cue_trackcount || !cue_indexcount) {	// !!! to merge indexes in one track (or using INDEXes only without TRACKs)

			while((*comm_arg != ' ') && (*comm_arg != 0))	// skip index number
				comm_arg++;
			pt = 0;
			if(*comm_arg) {
				while(*comm_arg == ' ')	// skip spaces after index number
					comm_arg++;
				hextime = pds_strtime_to_hextime(comm_arg, 1);	// convert MM:SS:FF to a hex number (xxMMSSFF)
				pt = ((hextime >> 16) & 0x3fff) * 60 * 1000 + ((hextime >> 8) & 0xff) * 1000 + ((hextime & 0xff) * 1000 + 75 / 2) / 75;	// convert hex number to millisecs
			}

			pei->pstime = pt;	// set the start of index
			funcbit_enable(pei->infobits, PEIF_INDEXED);

			if(plx) {
				if(!plx->petime)	// if no INDEX EE
					if(pt > plx->pstime)	// probably sequential indexes
						plx->petime = pt;	// then end of prev index = start of curr index
				if(plx->petime)
					funcbit_enable(plx->infobits, PEIF_ENABLED);
				cue_set_id3info(plx, psi);

				// copy filename from previos track/index (if not exists in the current one)
				if(!pei->filename && plx->filename) {
					if(psi->filenameslastp >= psi->filenamesendp)
						return pei;
					len = pds_strcpy(psi->filenameslastp, plx->filename);
					if(len <= 0)
						return pei;
					pei->filename = psi->filenameslastp;
					psi->filenameslastp += len + 1;
				}
			}

			pei++;
			cue_allcount++;
			cue_indexcount++;
		}
		cue_newfile = 0;
	}

	return pei;
}

static struct playlist_entry_info *cue_commandhand_length(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	unsigned long hextime;
	struct playlist_entry_info *plx;

	if(pei <= psi->lastentry || !cue_validfile)	// first we need a filename (we haven't got it) ...
		return pei;

	if(cue_indexcount)			// LENGTH after INDEX nn
		plx = pei - 1;			// previous entry
	else						// LENGHT after FILE or TRACK
		plx = pei;

	hextime = pds_strtime_to_hextime(comm_arg, 1);	// convert MM:SS:FF to a hex number (xxMMSSFF)
	plx->timemsec = ((hextime >> 16) & 0x3fff) * 60 * 1000 + ((hextime >> 8) & 0xff) * 1000 + ((hextime & 0xff) * 1000 + 75 / 2) / 75;	// convert hex number to millisecs (with rounding?)
	if(plx->timemsec)
		funcbit_enable(plx->infobits, PEIF_ENABLED);

	return pei;
}

extern unsigned int playrand;

static struct playlist_entry_info *cue_commandhand_mpxpeflags(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	unsigned long flags;
	struct playlist_entry_info *plx, *le;

	if(pei <= psi->lastentry || !cue_validfile)	// first we need a filename (we haven't got it) ...
		return pei;

	if(cue_indexcount)			// MPXPEFLAGS after INDEX nn
		plx = pei - 1;			// previous entry
	else						// MPXPEFLAGS after FILE or TRACK
		plx = pei;

	funcbit_disable(plx->infobits, PEIF_CUESAVEMASK);
	flags = pds_atol16(comm_arg);
	funcbit_copy(plx->infobits, flags, (PEIF_CUESAVEMASK & (~PEIF_RNDPLAYED)));
	if((flags & PEIF_RNDPLAYED) && playrand) {
		le = psi->lastentry;
		psi->lastentry = plx;
		playlist_randlist_pushq(psi, plx);
		psi->lastentry = le;
	}

	return pei;
}

static struct playlist_entry_info *cue_commandhand_rem_mpxpinfo(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *comm_arg, char *loaddir, char *filtermask)
{
	struct playlist_entry_info *plx, *pl2x;
	char *data, *next;
	unsigned long year, month, day, hour, minute;

	if(pei <= psi->lastentry || !cue_validfile)	// first we need a filename (we haven't got it) ...
		return pei;

	if(pds_strncmp(comm_arg, "MPXPINFO", sizeof("MPXPINFO") - 1) != 0)
		return pei;

	comm_arg += sizeof("MPXPINFO");

	if(cue_indexcount)			// MPXPINFO after INDEX nn
		plx = pei - 1;			// previous entry
	else						// MPXPINFO after FILE or TRACK
		plx = pei;

	do {
		next = pds_strchr(comm_arg, ';');
		if(next)
			*next++ = 0;
		while(*comm_arg == ' ')
			comm_arg++;
		if(!*comm_arg)
			break;
		data = pds_strchr(comm_arg, '=');
		if(data && data[1]) {
			unsigned long type = PDS_GETB_LE32(comm_arg), diff;
			data++;
			switch (type) {
			case PDS_GET4C_LE32('F', 'I', 'D', 'A'):{
					pds_fdate_t *d = &plx->filedate;
					sscanf(data, "%4d%2d%2d%2d%2d", &year, &month, &day, &hour, &minute);
					d->year = year - 1980;
					d->month = month;
					d->day = day;
					d->hours = hour, d->minutes = minute;
					break;
				}
			case PDS_GET4C_LE32('F', 'I', 'S', 'I'):
				plx->filesize = pds_atol(data);
				break;
			case PDS_GET4C_LE32('L', 'N', 'M', 'S'):
				plx->timemsec = pds_atol(data);
				funcbit_enable(plx->infobits, PEIF_ENABLED);
				break;
			case PDS_GET4C_LE32('I', 'N', 'D', 'B'):
				minute = pds_atol(data);
				if(minute > plx->pstime)
					diff = minute - plx->pstime;
				else
					diff = plx->pstime - minute;
				if(!plx->pstime || (diff < 150)) {	// !!! INDEX command has higher priority (INDB value has to be in precision loss 1/75s) (at the case of manual cue modification)
					if(cue_indexcount) {
						pl2x = plx - 1;
						if((pl2x >= psi->firstentry) && (pl2x->infobits & PEIF_INDEXED) && pl2x->petime && (pl2x->petime == plx->pstime))
							pl2x->petime = minute;
					}
					plx->pstime = minute;
				}
				funcbit_enable(plx->infobits, (PEIF_ENABLED | PEIF_INDEXED));
				break;
			case PDS_GET4C_LE32('I', 'N', 'D', 'E'):
				plx->petime = pds_atol(data);
				funcbit_enable(plx->infobits, (PEIF_ENABLED | PEIF_INDEXED));
				break;
			case PDS_GET4C_LE32('P', 'E', 'I', 'F'):{
					unsigned long flags = pds_atol16(data);
					funcbit_disable(plx->infobits, PEIF_CUESAVEMASK);
					funcbit_copy(plx->infobits, flags, (PEIF_CUESAVEMASK & (~PEIF_RNDPLAYED)));
					if((flags & PEIF_RNDPLAYED) && playrand) {
						struct playlist_entry_info *le = psi->lastentry;
						psi->lastentry = plx;
						playlist_randlist_pushq(psi, plx);
						psi->lastentry = le;
					}
					break;
				}
			}
		}
		comm_arg = next;
	} while(comm_arg);

	return pei;
}

static struct cue_handler_s cue_handlers[] = {
	{"FILE", &cue_commandhand_file},
	{"TRACK", &cue_commandhand_track},
	{"PERFORMER", &cue_commandhand_performer},
	{"TITLE", &cue_commandhand_title},
	{"INDEX", &cue_commandhand_index},
	{"LENGTH", &cue_commandhand_length},
	{"MPXPEFLAGS", &cue_commandhand_mpxpeflags},
	{"REM", &cue_commandhand_rem_mpxpinfo},
	{NULL, NULL}
};

static struct playlist_entry_info *open_load_cue(struct playlist_entry_info *pei, struct playlist_side_info *psi, char *listname, char *loaddir, char *filtermask)
{
	void *listfile;
	char strtmp[LOADLIST_MAX_LINELEN];

	if((listfile = mpxplay_diskdrive_textfile_open(NULL, listname, (O_RDONLY | O_TEXT))) == NULL)
		return pei;

	funcbit_enable(psi->editsidetype, (PLT_EXTENDED | PLT_EXTENDINC));	// !!!

	if(pei <= psi->firstsong)
		psi->savelist_textcodetype = mpxplay_diskdrive_textfile_config(listfile, MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_SRC, NULL, NULL);

	cue_allcount = 0;
	cue_filecount = 0;
	cue_validfile = 0;
	cue_newfile = 1;
	cue_trackcount = 0;
	cue_indexcount = 0;
	cue_track_artist[0] = cue_track_title[0] = 0;
	cue_main_artist[0] = cue_main_title[0] = 0;

	pei--;

	while(mpxplay_diskdrive_textfile_readline(listfile, strtmp, sizeof(strtmp) - 1) && (pei <= psi->endentry) && (psi->filenameslastp < psi->filenamesendp)) {
		struct cue_handler_s *cuh = &cue_handlers[0];
		char *commp = &strtmp[0], *argp;

		while(*commp == ' ')	// skip spaces before command
			commp++;

		argp = commp;
		while((*argp != ' ') && (*argp != 0))	// skip command (search for the argument)
			argp++;

		if(*argp != 0) {
			*argp = 0;
			argp++;
			while(*argp == ' ')	// skip spaces before argument
				argp++;
			do {
				if(pds_stricmp(commp, cuh->command) == 0) {
					pei = cuh->handler(pei, psi, argp, loaddir, filtermask);
					break;
				}
				cuh++;
			} while(cuh->command);
		}
	}

	if(cue_newfile)
		pei++;

	if((pei - 1) > psi->lastentry) {	// prev entry is a new entry
		cue_check_index(pei - 1, psi);
		cue_set_id3info(pei - 1, psi);	// if last FILE entry has no TRACK nor INDEX
	}

	mpxplay_diskdrive_textfile_close(listfile);

	return pei;
}

//-------------------------------------------------------------------------
// assign playlist entrytype/id3-name
unsigned int playlist_loadlist_get_header_by_ext(struct playlist_side_info *psi, struct playlist_entry_info *pei, char *filename)
{
	if(!playlist_loadsub_check_extension(psi, filename))
		return 0;

	if((desktopmode & DTM_EDIT_LOADLIST) && (psi->editsidetype & PLT_DIRECTORY) && !(psi->psio->editsidetype & PLT_DIRECTORY))
		pei->entrytype = DFT_PLAYLIST;
	else
		pei->entrytype = DFT_SUBLIST;
	if(!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE])
		switch (pei->entrytype) {
		case DFT_PLAYLIST:
			pei->id3info[I3I_DFT_STORE] = DFTSTR_PLAYLIST;
			break;
		case DFT_SUBLIST:
			pei->id3info[I3I_DFT_STORE] = DFTSTR_SUBLIST;
			break;
		}
	return 1;
}

//-------------------------------------------------------------------------
void playlist_loadlist_load_autosaved_list(struct playlist_side_info *psi)
{
	if((playlistsave & PLST_AUTO) && !(psi->editsidetype & PLT_ENABLED)) {
		unsigned int len;
		char filename[MAX_PATHNAMELEN];
		pds_getpath_from_fullname(filename, freeopts[OPT_PROGNAME]);
		len = pds_strlen(filename);
		if(len && (filename[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR))
			len += pds_strcpy(&filename[len], PDS_DIRECTORY_SEPARATOR_STR);
		pds_strcpy(&filename[len], playlist_savelist_get_savename(playlistsave));
		if(playlist_loadlist_mainload(psi, filename, 0, NULL)) {
			funcbit_enable(psi->editloadtype, PLL_LOADLIST);
			playlist_loadsub_setnewinputfile(psi, filename, PLL_LOADLIST);	// for ctrl-R
		}
	}
}
