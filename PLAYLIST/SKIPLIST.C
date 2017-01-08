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
//function:skip in playlist

#include "newfunc\newfunc.h"
#include "playlist.h"
#include "display\display.h"
#include "mpxinbuf.h"

extern unsigned int playcontrol, playreplay, playrand, intsoundcontrol;
extern unsigned int refdisp, desktopmode;

unsigned int playlist_skip(struct mainvars *mvp)
{
	funcbit_disable(playcontrol, PLAYC_ABORTNEXT);

	if(!mvp->newfilenum)
		playlist_get_newfilenum(mvp);

	if(mvp->newfilenum) {
		//if(mvp->psie->editorhighline==mvp->aktfilenum)
		// playlist_editorhighline_set(mvp->psie,mvp->newfilenum);
		mvp->aktfilenum = mvp->newfilenum;
		mvp->newfilenum = NULL;
		return 1;
	}
	return 0;
}

struct playlist_entry_info *playlist_get_newfilenum(struct mainvars *mvp)
{
	int loc_playrand = playrand;
	struct playlist_side_info *psi = mvp->psip;
	struct playlist_entry_info *pei = (mvp->newfilenum) ? mvp->newfilenum : mvp->aktfilenum;
	struct playlist_entry_info *newsong = mvp->newsong, *last_failed_entry = NULL;
	char sout[128];

	display_clear_timed_message();

	funcbit_disable(playcontrol, PLAYC_ABORTNEXT);

	if(pei < (psi->firstsong - 1))	// ???
		pei = psi->firstsong - 1;
	else if(pei > psi->lastentry)
		pei = psi->lastentry;

	if(newsong) {
		if(loc_playrand)
			playlist_randlist_setsignflag(newsong);
		if(newsong == pei)
			pei--;
		mvp->direction = newsong - pei;
		loc_playrand = 0;
		mvp->newsong = NULL;
	} else {
		if(mvp->step != 0)
			mvp->direction = mvp->step;
		else if(mvp->direction == 0)
			mvp->direction = 1;
	}
	mvp->step = 0;

	do {
		pei += mvp->direction;
		if(mvp->direction >= 0) {
			if(loc_playrand)
				pei = playlist_randlist_getnext(psi);
			if((pei >= mvp->pei0) && (pei <= psi->lastentry))
				mvp->direction = 1;	// corrects direction > 1
			else {
				if((psi->editsidetype & PLT_DOOMQUEUE) && (psi != mvp->psie) && (pei > (psi->firstsong + 1)))
					mvp->direction = -1;
				else {
					if((playreplay & REPLAY_LIST) && mvp->foundfile)
						pei = psi->firstsong;
					else
						pei = NULL;
					mvp->foundfile = 0;
				}
			}
		} else {
			if(loc_playrand)
				pei = playlist_randlist_getprev(pei);
			if(pei >= psi->firstentry)
				mvp->direction = -1;	// corrects direction < -1
			else {
				if((playreplay & REPLAY_LIST) && mvp->foundfile)
					pei = psi->lastentry;
				else
					pei = NULL;
				mvp->foundfile = 0;
			}
		}
		if(pei >= psi->firstsong && pei <= psi->lastentry) {
			if((intsoundcontrol & INTSOUND_DECODER) || (pei->infobits & PEIF_INDEXED)) {
				playlist_chkentry_get_onefileinfos_open(psi, pei);
				if(pei->entrytype >= DFT_AUDIOFILE)
					break;
				sprintf(sout, "Checking next file (%2d/%d)", pei - psi->firstsong + 1, psi->lastentry - psi->firstsong + 1);
				display_message(0, 0, sout);
				display_message(1, 0, "");
				last_failed_entry = pei;
			} else {
				if((pei->entrytype >= DFT_AUDIOFILE) || (pei->entrytype == DFT_NOTCHECKED))
					break;
			}
		}

		if(pds_kbhit())
			if(pds_extgetch() == KEY_ESC)
				funcbit_enable(playcontrol, PLAYC_ABORTNEXT);

	} while(pei && (mvp->direction != 0) && !funcbit_test(playcontrol, PLAYC_ABORTNEXT));

	if(funcbit_test(playcontrol, PLAYC_ABORTNEXT))
		pei = NULL;

	if((pei < psi->firstsong) || (pei > psi->lastentry))
		pei = NULL;

	if(pei) {
		if(!(desktopmode & DTM_EDIT_MAGNETFOLLOWSKIP) || (psi->editorhighline == mvp->aktfilenum) || (psi->editorhighline == mvp->newfilenum))
			playlist_editorhighline_set(psi, pei);
		mvp->newfilenum = pei;
		playlist_skiplist_reset_loadnext(mvp);
		if(playrand && newsong)
			mvp->direction = 0;
	} else {
		if(loc_playrand && (mvp->direction < 0) && !mvp->newfilenum)	// push back
			playlist_randlist_pushq(psi, mvp->aktfilenum);	// current file
		mvp->direction = mvp->seek_relative = 0;
		funcbit_disable(playcontrol, PLAYC_CONTINUOUS_SEEK);
		refdisp |= RDT_EDITOR;
	}
	if(intsoundcontrol & INTSOUND_DECODER)
		clear_message();
	if(last_failed_entry) {
		snprintf(sout, sizeof(sout), "Couldn't play file: %s", pds_getfilename_from_fullname(last_failed_entry->filename));
		display_timed_message(sout);
	}

	return pei;
}

//--------------------------------------------------------------------------
//gray-'/' & '*' ; alt-pgup & alt-pgdn
struct playlist_entry_info *playlist_get_nextalbum(struct playlist_entry_info *pei, struct playlist_side_info *psi, int step, unsigned int steplevel, unsigned int ring)
{
	unsigned int lcount, actsubdirs, cutlevel;
	char *pointer, actpath[MAX_PATHNAMELEN], newpath[MAX_PATHNAMELEN];	//,sout[100];

	if(psi->lastentry < psi->firstsong)
		return NULL;

	if(step == -1) {
		if(pei <= psi->firstsong) {
			if(ring)
				pei = psi->lastentry;
			else
				return NULL;
		} else
			pei--;
	} else if(pei < psi->firstsong)
		return psi->firstsong;
	pds_strcpy(actpath, pei->filename);

	if(steplevel) {
		pointer = &actpath[0];
		actsubdirs = 0;
		do {
			pointer = pds_strchr(pointer, PDS_DIRECTORY_SEPARATOR_CHAR);
			if(!pointer)
				break;
			pointer++;
			actsubdirs++;
		} while(1);

		if(steplevel > actsubdirs)
			return NULL;

		lcount = 0;
		do {
			pointer = pds_strrchr(actpath, PDS_DIRECTORY_SEPARATOR_CHAR);
			if(!pointer)
				break;
			*pointer = 0;
			lcount++;
		} while(lcount <= steplevel);

		cutlevel = actsubdirs - steplevel;
	} else {
		pointer = pds_strrchr(actpath, PDS_DIRECTORY_SEPARATOR_CHAR);
		if(pointer <= (&actpath[0]))
			return NULL;
		*pointer = 0;
	}

	//sprintf(sout,"ACT: %s %d %d",actpath,cutlevel,actsubdirs);
	//pds_textdisplay_printf(sout);

	pei -= step;
	do {
		pei += step;
		pds_strcpy(newpath, pei->filename);
		if(steplevel) {
			pointer = &newpath[0];
			lcount = 0;
			do {
				char *next = pds_strchr(pointer, PDS_DIRECTORY_SEPARATOR_CHAR);
				if(!next)
					break;
				pointer = next;
				lcount++;
				if(lcount >= cutlevel)
					break;
				pointer++;
			} while(1);
		} else {
			pointer = pds_strrchr(newpath, PDS_DIRECTORY_SEPARATOR_CHAR);
			if(!pointer)
				pointer = &newpath[0];
		}
		*pointer = 0;
		//sprintf(sout,"new: %s %d",newpath,lcount);
		//pds_textdisplay_printf(sout);
	} while((pds_stricmp(actpath, newpath) == 0) && (pei >= psi->firstsong) && (pei <= psi->lastentry));

	if(step == -1) {
		//if(steplevel && pei<psi->firstsong)
		// return NULL;
		pei++;
	}

	if(pei > psi->lastentry && !ring)
		return NULL;
	//sprintf(sout,"END: %s ",pei->filename);
	//pds_textdisplay_printf(sout);
	return pei;
}

//------------------------------------------------------------------------
void playlist_newsong_enter(struct mainvars *mvp, struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei = psi->editorhighline;
	unsigned long oldentrytype = pei->entrytype;

	if(pei->entrytype == DFT_UNKNOWN)
		pei->entrytype = DFT_NOTCHECKED;

	playlist_chkentry_get_onefileinfos_open(psi, pei);

	if(GET_HFT(pei->entrytype) == HFT_DFT) {
		playlist_change_sublist_or_directory(psi, pei->entrytype);
	} else {
		mvp->newsong = pei;
		if((mvp->psip != psi) && !(psi->psio->editsidetype & PLT_DOOMQUEUE)) {
			mvp->psip = psi;
			if(mvp->cfi->usecrossfade)
				mvp->aktfilenum = psi->firstsong - 1;
			else
				mvp->aktfilenum = psi->editorhighline;
			playlist_randlist_clearall(psi);
		}
		if(!(playcontrol & PLAYC_RUNNING)) {
			crossfade_reset(mvp);
			funcbit_enable(playcontrol, PLAYC_STARTNEXT);
		}
		if(!mvp->foundfile && (oldentrytype == DFT_UNKNOWN)) {
			playlist_chkentry_enable_entries(psi);
			playlist_chkfile_start_norm(psi, NULL);
		}
	}
}

void playlist_nextsong_select(struct mainvars *mvp, struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei = psi->editorhighline;

	if(psi == mvp->psip)
		playlist_randlist_resetsignflag(mvp->newfilenum);

	if(mvp->newfilenum == pei)
		mvp->newfilenum = NULL;
	else {
		playlist_chkentry_get_onefileinfos_open(psi, pei);
		if((mvp->psip != psi) && !(psi->psio->editsidetype & PLT_DOOMQUEUE)) {
			struct playlist_entry_info *peia;
			struct frame *frp = mvp->frp0;
			long timempos;

			mvp->newfilenum = NULL;
			mvp->psip = psi;
			playlist_randlist_clearall(psi);

			timempos = (frp->allframes) ? ((long)((float)frp->infile_infos->timemsec * (float)frp->frameNum / (float)frp->allframes)) : -1;
			peia = playlist_search_filename(psi, mvp->pei0->filename, timempos, NULL);
			if(peia) {
				mvp->aktfilenum = peia;
				playlist_pei0_set(mvp, peia, (EDITLIST_MODE_ID3 | EDITLIST_MODE_INDEX));
				playlist_randlist_pushq(psi, peia);
				mpxplay_calculate_index_start_end(frp, mvp, peia);
			} else
				mvp->aktfilenum = psi->firstsong - 1;
		}
		if(pei->entrytype >= DFT_AUDIOFILE) {
			if(!(playcontrol & PLAYC_RUNNING))
				mvp->newsong = pei;
			else {
				mvp->newfilenum = pei;
				playlist_randlist_setsignflag(pei);
			}
		}
	}
	playlist_skiplist_reset_loadnext(mvp);
}

void playlist_skiplist_reset_loadnext(struct mainvars *mvp)
{
	funcbit_disable((mvp->frp0 + 1)->buffertype, PREBUFTYPE_LOADNEXT_MASK);	// !!! reset/reload next file
}
