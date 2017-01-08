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
//function:playlist entry add,delete,move

#include "newfunc\newfunc.h"
#include "playlist.h"
#include "display\display.h"
#include <malloc.h>

#define EDITLIST_ASM 1

extern char *id3filterkeyword;
extern unsigned int displaymode, desktopmode, refdisp;

struct playlist_entry_info *playlist_editlist_addfileone_postproc(struct playlist_side_info *psi_dest, struct playlist_entry_info *pei_dest)
{
	struct mainvars *mvp = psi_dest->mvp;
	unsigned int entrytype = pei_dest->entrytype;
	struct playlist_entry_info *pei_newpos;

	if((psi_dest == mvp->psip) && !(psi_dest->editsidetype & PLT_DOOMQUEUE) && (mvp->aktfilenum < psi_dest->firstsong) && (pei_dest->entrytype >= DFT_AUDIOFILE)
	   && (pds_stricmp(pei_dest->filename, mvp->pei0->filename) == 0)) {
		struct frame *frp = mvp->frp0;
		mvp->aktfilenum = pei_dest;
		playlist_pei0_set(mvp, pei_dest, 0);
		playlist_randlist_pushq(psi_dest, pei_dest);
		mpxplay_calculate_index_start_end(frp, mvp, pei_dest);	// switch between cue and other playlist
	}
	pei_newpos = playlist_order_entry(psi_dest, pei_dest);
	if(entrytype & (DFTM_DIR | DFTM_PLAYLIST))
		playlist_search_firstsong(psi_dest);
	return pei_newpos;
}

void playlist_editlist_addfile_any(struct playlist_side_info *psi_src, struct playlist_entry_info *pei_src, char *source_filtermask)
{
	struct playlist_side_info *psi_dest = psi_src->psio;
	struct playlist_entry_info *startentry;
	unsigned int len, loadtype = 0;
	char *loc_filtermask, strtmp[MAX_PATHNAMELEN];

	if(!(psi_src->editsidetype & PLT_ENABLED))
		return;

	if(GET_HFT(pei_src->entrytype) == HFT_DFT) {	// add drive, dir or playlist
		if(source_filtermask)
			loc_filtermask = source_filtermask;
		else
			loc_filtermask = PDS_DIRECTORY_ALLFILE_STR;
		startentry = psi_dest->lastentry;
		switch (pei_src->entrytype) {
		case DFT_PLAYLIST:
		case DFT_SUBLIST:
			pds_strcpy(strtmp, pei_src->filename);
			loadtype = PLL_LOADLIST;
			break;
		case DFT_UPLIST:
			pds_strcpy(strtmp, psi_src->sublistnames[psi_src->sublistlevel]);
			loadtype = PLL_LOADLIST;
			break;
		case DFT_UPDIR:
			len = pds_strcpy(strtmp, psi_src->currdir);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_ALLDIR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			pds_strcpy(&strtmp[len], loc_filtermask);
			loadtype = PLL_DIRSCAN;
			break;
		case DFT_SUBDIR:
			len = pds_strcpy(strtmp, pei_src->filename);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_ALLDIR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			pds_strcpy(&strtmp[len], loc_filtermask);
			loadtype = PLL_DIRSCAN;
			break;
		case DFT_DRIVE:
			len = pds_strcpy(strtmp, PDS_DIRECTORY_DRIVE_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_ALLDIR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			pds_strcpy(&strtmp[len], loc_filtermask);
			strtmp[0] = pei_src->filename[0];	// copy drive label
			loadtype = PLL_DIRSCAN;
			break;
		default:
			return;
		}
		playlist_buildlist_one(psi_dest, strtmp, loadtype, NULL, source_filtermask);
		if(psi_dest->lastentry > startentry) {
			if(startentry < psi_dest->firstentry) {
				psi_dest->editloadtype = loadtype;
				playlist_loadsub_setnewinputfile(psi_dest, strtmp, loadtype);
			} else
				funcbit_enable(psi_dest->editloadtype, PLL_CHG_ENTRY);
			playlist_chkfile_start_norm(psi_dest, startentry + 1);
		}
	} else {
		if(!source_filtermask || pds_filename_wildchar_cmp(pei_src->filename, source_filtermask))
			if(playlist_editlist_addfile_one(psi_src, psi_dest, pei_src, NULL, EDITLIST_MODE_ALL))
				playlist_editlist_addfileone_postproc(psi_dest, psi_dest->lastentry);
	}
}

void playlist_editlist_copy_entry(struct playlist_side_info *psi_src, struct playlist_entry_info *pei_src)
{
	struct playlist_side_info *psi_dest = psi_src->psio;
	struct playlist_entry_info pei_tmp;
	unsigned int len;
	char strtmp[MAX_PATHNAMELEN];

	if(!(psi_src->editsidetype & PLT_ENABLED))
		return;

	if(GET_HFT(pei_src->entrytype) == HFT_DFT) {	// copy drive, dir or playlist
		pds_memset(&pei_tmp, 0, sizeof(struct playlist_entry_info));
		switch (pei_src->entrytype) {
		case DFT_PLAYLIST:
		case DFT_SUBLIST:
			pds_strcpy(strtmp, pei_src->filename);
			pei_tmp.id3info[I3I_ARTIST] = pei_src->id3info[I3I_ARTIST];
			pei_tmp.id3info[I3I_TITLE] = pei_src->id3info[I3I_TITLE];
			break;
		case DFT_SUBDIR:
			len = pds_strcpy(strtmp, pei_src->filename);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_ALLDIR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			pds_strcpy(&strtmp[len], PDS_DIRECTORY_ALLFILE_STR);
			break;
		case DFT_DRIVE:
			len = pds_strcpy(strtmp, PDS_DIRECTORY_DRIVE_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_ALLDIR_STR);
			len += pds_strcpy(&strtmp[len], PDS_DIRECTORY_SEPARATOR_STR);
			pds_strcpy(&strtmp[len], PDS_DIRECTORY_ALLFILE_STR);
			strtmp[0] = pei_src->filename[0];	// copy drive label
			break;
		default:
			return;
		}
		pei_tmp.filename = &strtmp[0];
		if((desktopmode & DTM_EDIT_LOADLIST) && (pei_src->entrytype & DFTM_PLAYLIST) && (psi_dest->editsidetype & PLT_DIRECTORY) && !psi_dest->sublistlevel && !(psi_src->editsidetype & PLT_DIRECTORY)) {
			pei_tmp.entrytype = DFT_PLAYLIST;
			if(pds_strcmp(pei_tmp.id3info[I3I_DFT_STORE], DFTSTR_SUBLIST) == 0)
				pei_tmp.id3info[I3I_DFT_STORE] = DFTSTR_PLAYLIST;
		} else {
			pei_tmp.entrytype = DFT_SUBLIST;
			if((!pei_tmp.id3info[I3I_ARTIST] && !pei_tmp.id3info[I3I_TITLE]) || pds_strcmp(pei_tmp.id3info[I3I_DFT_STORE], DFTSTR_PLAYLIST) == 0)
				pei_tmp.id3info[I3I_DFT_STORE] = DFTSTR_SUBLIST;
		}
		playlist_editlist_addfile_one(psi_src, psi_dest, &pei_tmp, NULL, EDITLIST_MODE_ALL);
		if(pei_tmp.entrytype & DFTM_PLAYLIST) {
			playlist_order_entry(psi_dest, psi_dest->lastentry);
			playlist_search_firstsong(psi_dest);
		}
	} else {
		if(playlist_editlist_addfile_one(psi_src, psi_dest, pei_src, NULL, EDITLIST_MODE_ALL))
			playlist_editlist_addfileone_postproc(psi_dest, psi_dest->lastentry);
	}
}

unsigned int playlist_editlist_addfile_one(struct playlist_side_info *psi_src, struct playlist_side_info *psi_dest, struct playlist_entry_info *pei_src, struct playlist_entry_info *pei_dest,
										   unsigned int modify)
{
	unsigned int i;

	if(psi_src && !(psi_src->editsidetype & PLT_ENABLED))
		return 0;

	if(!psi_dest)
		if(psi_src)
			psi_dest = psi_src->psio;
		else
			return 0;

	playlist_enable_side(psi_dest);

	if(!pei_dest) {
		pei_dest = psi_dest->lastentry + 1;
		if(pei_dest > psi_dest->endentry)
			return 0;
		pds_memset(pei_dest, 0, sizeof(struct playlist_entry_info));
	} else {
		if(pei_dest > psi_dest->endentry)
			return 0;
	}
	if(modify & EDITLIST_MODE_FILENAME) {
		char *flastp = psi_dest->filenameslastp;
		if(flastp >= psi_dest->filenamesendp)
			return 0;
		pei_dest->filename = flastp;
		flastp += pds_strcpy(flastp, pei_src->filename) + 1;
		psi_dest->filenameslastp = flastp;
	}
	if(pei_dest > psi_dest->lastentry) {
		if(psi_dest->chkfilenum_end == psi_dest->lastentry)
			psi_dest->chkfilenum_end = pei_dest;
		else if(psi_src && psi_src->chkfilenum_curr && (pei_src >= psi_src->chkfilenum_curr))
			playlist_chkfile_start_norm(psi_dest, pei_dest);
		psi_dest->lastentry = pei_dest;
		funcbit_enable(psi_dest->editloadtype, PLL_CHG_ENTRY);
	}
	if(modify & EDITLIST_MODE_INDEX) {
		funcbit_disable(pei_dest->infobits, PEIF_INDEXED);
		funcbit_copy(pei_dest->infobits, pei_src->infobits, PEIF_INDEXED);
		pei_dest->pstime = pei_src->pstime;
		pei_dest->petime = pei_src->petime;
	}
	if(modify & (EDITLIST_MODE_HEAD | EDITLIST_MODE_ENTRY)) {
		pei_dest->entrytype = pei_src->entrytype;
		funcbit_disable(pei_dest->infobits, PEIF_COPYMASK);
		funcbit_copy(pei_dest->infobits, pei_src->infobits, PEIF_COPYMASK);
		pei_dest->timemsec = pei_src->timemsec;
		pei_dest->filesize = pei_src->filesize;
		pei_dest->mdds = pei_src->mdds;
		pei_dest->infile_funcs = pei_src->infile_funcs;
		*((mpxp_uint32_t *) & pei_dest->filedate) = *((mpxp_uint32_t *) & pei_src->filedate);
		playlist_fulltime_add(psi_dest, pei_dest);
	}
	if(modify & EDITLIST_MODE_ID3) {
		char *id3lastp = psi_dest->id3infolastp;
		for(i = 0; (i <= I3I_MAX) && (id3lastp < psi_dest->id3infoendp); i++) {
			if(!pei_dest->id3info[i]) {	// !!! get_onefileinfos_from_otherside()
				if((!psi_src && pei_src->id3info[i]) || (psi_src && (pei_src->id3info[i] >= psi_src->id3infobeginp) && (pei_src->id3info[i] < psi_src->id3infoendp))) {
					pei_dest->id3info[i] = id3lastp;
					id3lastp += pds_strcpy(id3lastp, pei_src->id3info[i]) + 1;
				} else
					pei_dest->id3info[i] = pei_src->id3info[i];	// NULL or mp3-genre
			}
		}
		psi_dest->id3infolastp = id3lastp;
		funcbit_disable(pei_dest->infobits, PEIF_ID3MASK);
		funcbit_copy(pei_dest->infobits, pei_src->infobits, PEIF_ID3MASK);
	}
	return modify;
}

#ifdef __WATCOMC__
void asm_delfile_filename(char **, unsigned int, unsigned int, char *, char *);
void asm_delfile_id3infos(char **, unsigned int, unsigned int, char *, char *);
#endif

void playlist_editlist_delfile_one(struct playlist_side_info *psi, struct playlist_entry_info *pei, unsigned int modify)
{
	char *pf;
	unsigned int i, memlen_move;

	if((psi->lastentry < psi->firstentry) || (pei < psi->firstentry) || (pei > psi->lastentry)
	   || (pei->entrytype & DFTM_DRIVE) || (pei->entrytype == DFT_UPDIR) || (pei->entrytype == DFT_UPLIST))
		return;

	//delete filename from the filenamesworkarea
	if(modify & EDITLIST_MODE_FILENAME) {
		pf = pei->filename;
		pei->filename = NULL;
		if(pf >= psi->filenamesbeginp && pf < psi->filenameslastp) {
			unsigned int memlen_del;
			char **fn, *flp;
			memlen_del = pds_strlen(pf) + 1;
			memlen_move = psi->filenameslastp - pf;
			pds_memcpy(pf, pf + memlen_del, memlen_move);

			i = psi->lastentry - psi->firstentry + 1;
			fn = &(psi->firstentry->filename);
			flp = psi->filenameslastp;

			// !!! sizeof(struct playlist_entry_info) dependant
#if defined(EDITLIST_ASM) && defined(__WATCOMC__)
#pragma aux asm_delfile_filename=\
   "back1:"\
    "mov eax,dword ptr [ebx]"\
    "cmp eax,edi"\
    "jbe skip"\
    "cmp eax,esi"\
    "jae skip"\
     "sub eax,edx"\
     "mov dword ptr [ebx],eax"\
    "skip:add ebx,72"\
    "dec ecx"\
    "jnz back1"\
    parm [ebx][ecx][edx][edi][esi] modify[eax ebx ecx];
			asm_delfile_filename(fn, i, memlen_del, pf, flp);
#else
			do {
				char *fnp = *fn;
				if((fnp > pf) && (fnp < flp)) {	// pointer is in filenamesworkarea
					fnp -= memlen_del;	// move down filename pointer
					*fn = fnp;
				}
				fn += sizeof(struct playlist_entry_info) / sizeof(*fn);
			} while(--i);
#endif

			psi->filenameslastp -= memlen_del;
		}
	}
	//delete id3infos from the id3infoworkarea
	if(modify & EDITLIST_MODE_ID3) {
		i = 0;
		do {
			pf = pei->id3info[i];
			pei->id3info[i] = NULL;
			if((pf >= psi->id3infobeginp) && (pf < psi->id3infolastp)) {
				char **id3iip, *ilp;
				unsigned int k, memlen_del = pds_strlen(pf) + 1;
				memlen_move = psi->id3infolastp - pf;
				pds_memcpy(pf, pf + memlen_del, memlen_move);

				k = psi->lastentry - psi->firstentry + 1;
				id3iip = &(psi->firstentry->id3info[0]);
				ilp = psi->id3infolastp;

				// !!! sizeof(struct playlist_entry_info) dependant
#if defined(EDITLIST_ASM) && defined(__WATCOMC__) && (I3I_MAX==6)
#pragma aux asm_delfile_id3infos=\
    "push ebp"\
    "back_k:"\
     "mov ebp,7"\
     "back_j:"\
      "mov eax,dword ptr [ebx]"\
      "cmp eax,edi"\
      "jbe skip"\
      "cmp eax,esi"\
      "jae skip"\
       "sub eax,edx"\
       "mov dword ptr [ebx],eax"\
      "skip:add ebx,4"\
      "dec ebp"\
     "jnz back_j"\
     "add ebx,44"\
     "dec ecx"\
    "jnz back_k"\
    "pop ebp"\
    parm [ebx][ecx][edx][edi][esi] modify[eax ebx ecx];
				asm_delfile_id3infos(id3iip, k, memlen_del, pf, ilp);
#else
				do {
					int j = I3I_MAX + 1;
					do {
						char *idp = *id3iip;
						if((idp > pf) && (idp < ilp)) {	// pointer is in id3infoworkarea
							idp -= memlen_del;	//move down id3info[] pointer
							*id3iip = idp;
						}
						id3iip++;
					} while(--j);
					id3iip += (sizeof(struct playlist_entry_info) - sizeof(pei->id3info)) / sizeof(*id3iip);
				} while(--k);
#endif
				psi->id3infolastp -= memlen_del;
			}
		} while(++i <= I3I_MAX);
	}

	if(modify & EDITLIST_MODE_HEAD)
		playlist_fulltime_del(psi, pei);

	if(modify & EDITLIST_MODE_ENTRY) {
		if((pei->infobits & PEIF_SELECTED) && psi->selected_files)
			psi->selected_files--;
		if((pei->entrytype & DFTM_DFT) && (pei < psi->firstsong) && (psi->firstsong > psi->firstentry))
			psi->firstsong--;

		playlist_randlist_delete(pei);

		//move down entries
		if(pei < psi->lastentry)
			pds_memcpy(pei, pei + 1, (char *)(psi->lastentry) - (char *)pei);

		//clear last entry
		pds_memset(psi->lastentry, 0, sizeof(struct playlist_entry_info));

		//correct pointers in random queue and at pei->myself
		playlist_randlist_correctq(psi, pei, psi->lastentry);
		playlist_peimyself_reset(psi, pei, psi->lastentry);

		//correct some variables (chkfilenum,aktfilenum,newfilenum)
		psi->lastentry--;
		if(psi->lastentry < psi->firstentry) {
			playlist_disable_side_full(psi);
		} else {
			struct mainvars *mvp = psi->mvp;
			if(psi->chkfilenum_curr) {
				if(psi->chkfilenum_begin >= pei)
					psi->chkfilenum_begin--;
				if(psi->chkfilenum_curr >= pei)
					psi->chkfilenum_curr--;
				psi->chkfilenum_end--;
			}
			if(psi == mvp->psip) {
				if(mvp->newfilenum) {
					if(mvp->newfilenum > pei)
						mvp->newfilenum--;
					if(mvp->newfilenum > psi->lastentry)
						mvp->newfilenum = psi->lastentry;
					if(mvp->newfilenum < psi->firstsong)
						mvp->newfilenum = NULL;
				}
				if(mvp->aktfilenum >= psi->firstsong) {
					if(mvp->aktfilenum > pei)
						mvp->aktfilenum--;
					else {
						if(mvp->aktfilenum == pei) {
							mvp->aktfilenum = psi->firstsong - 1;
							if(!mvp->newfilenum && (pei > psi->firstsong)) {
								if(pei > psi->lastentry)
									pei = psi->lastentry;
								mvp->newfilenum = pei;
							}
						}
					}
				}
			}
			if(pei < psi->editorhighline)
				psi->editorhighline--;
			playlist_editorhighline_check(psi);
			funcbit_enable(psi->editloadtype, PLL_CHG_ENTRY);
		}
	}
}

void playlist_editlist_delete_entry_manual(struct playlist_side_info *psi, struct playlist_entry_info *pei)
{
	if(!(pei->entrytype & DFTM_DRIVE) && !(pei->entrytype & DFTM_DIR) && ((psi->lastentry > psi->firstentry) || (psi->psio->editsidetype & PLT_ENABLED))) {
		playlist_editlist_delfile_one(psi, pei, EDITLIST_MODE_ALL);
		display_editorside_reset(psi);
	}
}

unsigned int playlist_editlist_allocated_copy_entry(struct playlist_entry_info *pei_dest, struct playlist_entry_info *pei_src)
{
	unsigned int len, i;
	if(!pei_src || !pei_dest)
		return 0;
	pds_memcpy(pei_dest, pei_src, sizeof(struct playlist_entry_info));
	pei_dest->filename = NULL;
	pds_memset(&pei_dest->id3info[0], 0, sizeof(pei_dest->id3info));
	funcbit_enable(pei_dest->infobits, PEIF_ALLOCATED);
	len = pds_strlen(pei_src->filename);
	if(len) {
		pei_dest->filename = malloc(len + 1);
		if(!pei_dest->filename)
			goto err_out_dup;
		pds_strcpy(pei_dest->filename, pei_src->filename);
	}
	for(i = 0; i <= I3I_MAX; i++) {
		len = pds_strlen(pei_src->id3info[i]);
		if(len) {
			pei_dest->id3info[i] = malloc(len + 1);
			if(!pei_dest->id3info[i])
				goto err_out_dup;
			pds_strcpy(pei_dest->id3info[i], pei_src->id3info[i]);
		}
	}
	return 1;

  err_out_dup:
	playlist_editlist_allocated_clear_entry(pei_dest);
	return 0;
}

void playlist_editlist_allocated_clear_entry(struct playlist_entry_info *pei)
{
	unsigned int i;
	if(!funcbit_test(pei->infobits, PEIF_ALLOCATED))
		return;
	if(pei->filename)
		free(pei->filename);
	for(i = 0; i <= I3I_MAX; i++)
		if(pei->id3info[i])
			free(pei->id3info[i]);
	pds_memset(pei, 0, sizeof(playlist_entry_info));
}

void playlist_editlist_addfile_ins_ehl(struct playlist_side_info *psi_src, struct playlist_entry_info *pei_src)
{
	if(GET_HFT(pei_src->entrytype) == HFT_DFT)
		playlist_editlist_addfile_any(psi_src, pei_src, NULL);
	else {
		struct playlist_side_info *psi_dest;
		struct playlist_entry_info *pei_dest;
		if(!playlist_editlist_addfile_one(psi_src, NULL, pei_src, NULL, EDITLIST_MODE_ALL))
			return;
		psi_dest = psi_src->psio;
		pei_dest = psi_dest->editorhighline;
		if(pei_dest < psi_dest->firstsong)
			pei_dest = psi_dest->firstsong;
		if(pei_dest < (psi_dest->lastentry - 1)) {
			playlist_editorhighline_set(psi_dest, psi_dest->lastentry);
			do {
				playlist_editlist_shiftfile(psi_dest, -1);
			} while(psi_dest->editorhighline > pei_dest);
		} else {
			playlist_editorhighline_set(psi_dest, pei_dest);
		}
		playlist_editorhighline_seek(psi_dest, +1, SEEK_CUR);
	}
}

// move/shift entry (up or down) in the playlist (ie: at ctrl-up/ctrl-down)
void playlist_editlist_shiftfile(struct playlist_side_info *psi, int direction)
{
	struct playlist_entry_info *pei_dest;

	pei_dest = psi->editorhighline + direction;
	if((psi->editorhighline >= psi->firstsong) && (pei_dest >= psi->firstsong) && (pei_dest <= psi->lastentry)) {
		playlist_swap_entries(psi, psi->editorhighline, pei_dest);
		playlist_editorhighline_set(psi, pei_dest);
		funcbit_enable(psi->editloadtype, PLL_CHG_ENTRY);
	}
}

//move playlist entry via the mouse (drag & move)
void playlist_editlist_mouse_shiftfile(struct mainvars *mvp, struct playlist_entry_info *ehls)
{
	int mfd;
	unsigned int i;
	struct playlist_side_info *psi = mvp->psie;
	struct playlist_entry_info *ehld = psi->editorhighline;

	if(ehls != ehld) {
		playlist_editorhighline_set_nocenter(psi, ehls);
		if(ehld > ehls) {
			i = ehld - ehls;
			mfd = 1;
		} else {
			i = ehls - ehld;
			mfd = -1;
		}
		for(; i; i--)
			playlist_editlist_shiftfile(psi, mfd);

		if(psi == mvp->psip)
			refdisp |= RDT_EDITOR | RDT_BROWSER;
		else
			refdisp |= RDT_EDITOR;
	}
}

//----------------------------------------------------------------------
//group functions

// copy (add) all playlist entries to the other side
void playlist_editlist_copyside(struct playlist_side_info *psi_src)
{
	struct playlist_entry_info *pei_src;

	for(pei_src = psi_src->firstsong; pei_src <= psi_src->lastentry; pei_src++)
		if(GET_HFT(pei_src->entrytype) != HFT_DFT)
			playlist_editlist_addfile_one(psi_src, NULL, pei_src, NULL, EDITLIST_MODE_ALL);

	playlist_order_side(psi_src->psio);

	refdisp |= RDT_EDITOR;
	if(psi_src->psio == psi_src->mvp->psip)
		refdisp |= RDT_BROWSER;
	display_editorside_reset(psi_src->psio);
}

typedef struct mpxp_easg_s {
	struct playlist_side_info *psi;
	mpxp_char_t source_filtermask[48];
} mpxp_easg_s;

static display_textwin_button_t addfile_group_buttons[] = {
	{"[ Add ]", KEY_ENTER1},	// gray enter
	{"", KEY_ENTER2},			// white enter
	{"[ Cancel ]", 0x2e63},		// 'c'
	{"", 0x2e43},				// 'C'
	{"", 0x3f00},				// F5
	{"", KEY_ESC},				// ESC
	{NULL, 0}
};

static char source_default_filter_addfiles[48] = PDS_DIRECTORY_ALLFILE_STR;

static void editlist_addfile_group_start(struct mpxp_easg_s *ag)
{
	struct playlist_side_info *psi_src = ag->psi;
	struct playlist_entry_info *pei;

	if(psi_src->selected_files) {
		for(pei = psi_src->firstentry; pei <= psi_src->lastentry; pei++)
			if(pei->infobits & PEIF_SELECTED) {
				playlist_editlist_addfile_any(psi_src, pei, ag->source_filtermask);
				funcbit_disable(pei->infobits, PEIF_SELECTED);
			}
		psi_src->selected_files = 0;
	} else
		playlist_editlist_addfile_any(psi_src, psi_src->editorhighline, ag->source_filtermask);	// !!!

	refdisp |= RDT_EDITOR;
	if(psi_src->psio == psi_src->mvp->psip)
		refdisp |= RDT_BROWSER;
	display_editorside_reset(psi_src->psio);

	pds_strcpy(source_default_filter_addfiles, ag->source_filtermask);
	free(ag);
}

void playlist_editlist_addfile_selected_group(struct playlist_side_info *psi_src)
{
	struct playlist_entry_info *pei = psi_src->editorhighline;
	struct mpxp_easg_s *ag;
	void *tw;

	if(!psi_src->selected_files && (GET_HFT(pei->entrytype) != HFT_DFT)) {
		playlist_editlist_addfile_any(psi_src, pei, NULL);
		return;
	}

	ag = calloc(1, sizeof(*ag));
	if(!ag)
		return;

	ag->psi = psi_src;

	if(!source_default_filter_addfiles[0] || !pds_strlenc(source_default_filter_addfiles, ' '))
		pds_strcpy(source_default_filter_addfiles, PDS_DIRECTORY_ALLFILE_STR);

	pds_strcpy(ag->source_filtermask, source_default_filter_addfiles);

	tw = display_textwin_allocwindow_items(NULL, TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_CONFIRM, " Copy entries ", editlist_addfile_group_start, ag);
	display_textwin_additem_msg_static(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, 0, "Add/load selected entries to the other side?");
	display_textwin_additem_separatorline(tw, 1);
	display_textwin_additem_msg_static(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, 2, "Source filter: ");
	display_textwin_additem_editline(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, sizeof("Source filter: ") - 1, 2, 8, ag->source_filtermask, sizeof(ag->source_filtermask) - 1);
	display_textwin_additem_separatorline(tw, 3);
	display_textwin_additem_buttons(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, 4, &addfile_group_buttons[0], NULL);
	display_textwin_openwindow_items(tw, 0, 0, 0);
}

void playlist_editlist_copy_selected_group(struct playlist_side_info *psi_src)
{
	struct playlist_entry_info *pei;

	if(psi_src->selected_files) {
		for(pei = psi_src->firstentry; pei <= psi_src->lastentry; pei++)
			if(pei->infobits & PEIF_SELECTED) {
				playlist_editlist_copy_entry(psi_src, pei);
				funcbit_disable(pei->infobits, PEIF_SELECTED);
			}
		psi_src->selected_files = 0;
	} else
		playlist_editlist_copy_entry(psi_src, psi_src->editorhighline);	// !!!

	refdisp |= RDT_EDITOR;
	if(psi_src->psio == psi_src->mvp->psip)
		refdisp |= RDT_BROWSER;
	display_editorside_reset(psi_src->psio);
}

void playlist_editlist_move_selected_group(struct playlist_side_info *psi_src)
{
	struct playlist_entry_info *pei;

	if(psi_src->selected_files) {
		for(pei = psi_src->firstentry; pei <= psi_src->lastentry; pei++)
			if(pei->infobits & PEIF_SELECTED)
				playlist_editlist_copy_entry(psi_src, pei);
	} else
		playlist_editlist_copy_entry(psi_src, psi_src->editorhighline);	// !!!

	playlist_editlist_delfile_selected_group(psi_src);

	refdisp |= RDT_EDITOR;
	if(psi_src->psio == psi_src->mvp->psip)
		refdisp |= RDT_BROWSER;
}

void playlist_editlist_delfile_selected_group(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	unsigned int delcount, delfilenum, allfilenum;
	void *tw = NULL;
	char sout[48];

	if(psi->selected_files) {
		pei = psi->lastentry;
		delcount = 0;
		delfilenum = psi->selected_files;
		allfilenum = (psi->lastentry - psi->firstentry + 1);
		if((delfilenum == allfilenum) && !(psi->editsidetype & PLT_DIRECTORY) && !(psi->psio->editsidetype & PLT_DOOMQUEUE)) {
			playlist_clear_side(psi);
			playlist_disable_side_full(psi);
		} else {
			do {
				if(pei->infobits & PEIF_SELECTED) {
					if((psi->psio->editsidetype & PLT_DOOMQUEUE) && (psi->lastentry <= psi->firstentry))	// !!!
						break;
					if(pei->entrytype != DFT_SUBDIR) {
						playlist_editlist_delfile_one(psi, pei, EDITLIST_MODE_ALL);
						funcbit_disable(pei->infobits, PEIF_SELECTED);
						delcount++;

						if(!(delcount & 31)) {
							if(pds_kbhit())
								if(pds_extgetch() == KEY_ESC)
									break;
							sprintf(sout, "\nRemoving entries: %3d/%d\n", delcount, delfilenum);
							tw = display_textwin_openwindow_message(tw, NULL, sout);
						}
					}
				}
				pei--;
			} while(pei >= psi->firstentry);
			if(delcount >= psi->selected_files)
				psi->selected_files = 0;
			else
				psi->selected_files -= delcount;
			display_textwin_closewindow_message(tw);
			display_editorside_reset(psi);
		}
	} else
		playlist_editlist_delete_entry_manual(psi, psi->editorhighline);

	refdisp |= RDT_EDITOR;
	if(psi == psi->mvp->psip)
		refdisp |= RDT_BROWSER;
}

void playlist_editlist_group_invert_selection(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	unsigned int selected = 0;
	char sout[64];

	for(pei = psi->firstentry; pei <= psi->lastentry; pei++) {
		if((pei->entrytype & DFTM_DFT) && ((pei->entrytype & DFTM_DRIVE) || (pei->entrytype & DFTM_DIR) || (pei->entrytype == DFT_UPLIST)))
			continue;
		funcbit_inverse(pei->infobits, PEIF_SELECTED);
		if(funcbit_test(pei->infobits, PEIF_SELECTED))
			selected++;
	}

	psi->selected_files = selected;

	sprintf(sout, "Selected %d file%c", selected, ((selected > 1) ? 's' : ' '));
	display_timed_message(sout);
}

void playlist_editlist_group_select_all(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	unsigned int selected = 0;
	char sout[64];

	for(pei = psi->firstentry; pei <= psi->lastentry; pei++) {
		if((pei->entrytype & DFTM_DFT) && ((pei->entrytype & DFTM_DRIVE) || (pei->entrytype & DFTM_DIR) || (pei->entrytype == DFT_UPLIST)))
			continue;
		funcbit_enable(pei->infobits, PEIF_SELECTED);
		selected++;
	}

	psi->selected_files = selected;

	sprintf(sout, "Selected all (%d) file%c", selected, ((selected > 1) ? 's' : ' '));
	display_timed_message(sout);
}

void playlist_editlist_group_unselect_all(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	char sout[64];

	for(pei = psi->firstentry; pei <= psi->lastentry; pei++)
		funcbit_disable(pei->infobits, PEIF_SELECTED);

	sprintf(sout, "Unselected %d (all) file%c", psi->selected_files, ((psi->selected_files > 1) ? 's' : ' '));
	display_timed_message(sout);
	psi->selected_files = 0;
}

//------------------------------------------------------------------------
#define METASELECT_WINSIZE 40

#define SELECTTYPE_UNKNOWN 0
#define SELECTTYPE_DIRONLY 1

static unsigned int selecthand_unselect;
static char selectstring[METASELECT_WINSIZE + 20] = PDS_DIRECTORY_ALLFILE_STR;

static void editlist_groupselect_metadata(struct playlist_side_info *psi)
{
	struct playlist_entry_info *pei;
	unsigned int ts_len, as_len, selected, selecttype = SELECTTYPE_UNKNOWN, found;
	char *p, *s_titlep, temps[sizeof(selectstring)], sout[50];

	pds_strcpy(temps, selectstring);
	s_titlep = pds_strchr(temps, ':');
	if(s_titlep) {
		*s_titlep++ = 0;
		ts_len = pds_strlen(s_titlep);
	} else
		ts_len = 0;
	as_len = pds_strlen(temps);

	p = pds_strchr(temps, '.');
	if(p && !p[1])
		selecttype = SELECTTYPE_DIRONLY;

	selected = 0;
	pei = psi->firstentry;
	do {
		found = 0;
		switch (selecttype) {
		case SELECTTYPE_UNKNOWN:
			if((pei->entrytype & DFTM_DRIVE) || (pei->entrytype == DFT_UPDIR) || (pei->entrytype == DFT_UPLIST))
				break;
			if(!s_titlep) {
				unsigned int i;
				for(i = 0; i <= I3I_MAX; i++) {
					if(pei->id3info[i] && pds_stri_wildchar_cmp(pei->id3info[i], temps)) {
						found = 1;
						break;
					}
				}
				if(!found) {
					if(pds_stri_wildchar_cmp(pei->filename, temps) || pds_filename_wildchar_cmp(pei->filename, temps))
						found = 1;
				}
			} else {
				char *ipa, *ipt;
				if(GET_HFT(pei->entrytype) == HFT_DFT || (!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE])) {
					ipa = ipt = pds_getfilename_from_fullname(pei->filename);
				} else {
					ipa = pei->id3info[I3I_ARTIST];
					ipt = pei->id3info[I3I_TITLE];
					if(!ipa)
						ipa = ipt;
					if(!ipt)
						ipt = ipa;
				}
				if(!as_len || pds_strstri(ipa, temps)) {	// title search only || artist found
					if(!ts_len || pds_strstri(ipt, s_titlep))	// artist search only || title found
						found = 1;
				}
			}
			break;
		case SELECTTYPE_DIRONLY:
			if((pei->entrytype == DFT_SUBDIR) && pds_filename_wildchar_cmp(pei->filename, temps))
				found = 1;
			break;
		}
		if(found) {
			if(selecthand_unselect) {
				if(pei->infobits & PEIF_SELECTED) {
					funcbit_disable(pei->infobits, PEIF_SELECTED);
					psi->selected_files--;
					selected++;
				}
			} else {
				if(!(pei->infobits & PEIF_SELECTED)) {
					funcbit_enable(pei->infobits, PEIF_SELECTED);
					psi->selected_files++;
				}
				selected++;
			}
		}
	} while(++pei <= psi->lastentry);
	sprintf(sout, "%s %d file%c", ((selecthand_unselect) ? "Unselected" : "Selected"), selected, ((selected > 1) ? 's' : ' '));
	display_timed_message(sout);
	if(selected)
		refdisp |= RDT_EDITOR;
}

void playlist_editlist_groupselect_open(struct playlist_side_info *psi, unsigned int unselect)
{
	void *tw;
	selecthand_unselect = unselect;
	tw = display_textwin_allocwindow_items(NULL, TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_CONFIRM | TEXTWIN_FLAG_NOWINMINSIZE,
										   ((unselect) ? " Unselect files by filemask or metadata " : " Select files by filemask or metadata "), editlist_groupselect_metadata, psi);
	display_textwin_additem_editline(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, 0, -1, METASELECT_WINSIZE, &selectstring[0], sizeof(selectstring) - 1);
	display_textwin_openwindow_items(tw, 0, 0, 0);
}

//--------------------------------------------------------------------------
void playlist_editlist_id3filter(struct mainvars *mvp)
{
	struct playlist_side_info *psi = mvp->psip;
	struct playlist_entry_info *pei;
	unsigned int i, found, countf, counti, allfilenums;
	char sout[64];

	if(id3filterkeyword && (psi->editsidetype & PLT_ENABLED)) {
		counti = countf = 0;
		allfilenums = psi->lastentry - psi->firstsong + 1;
		display_clear_timed_message();
		for(pei = psi->lastentry; pei >= psi->firstsong; pei--) {
			found = 0;
			if(pei->infobits & PEIF_ID3EXIST) {
				for(i = 0; i <= I3I_MAX; i++) {
					if(pei->id3info[i]) {
						if(pds_strstri(pei->id3info[i], id3filterkeyword)) {
							found = 1;
							break;
						}
					}
				}
			} else {
				if(pds_strstri(pei->filename, id3filterkeyword))
					found = 1;
			}

			if(!found)
				playlist_editlist_delfile_one(psi, pei, EDITLIST_MODE_ALL);
			else
				countf++;

			counti++;
			if(!(counti & 31)) {
				sprintf(sout, "Filtering: %4d/%d, Found: %d", counti, allfilenums, countf);
				display_message(0, 0, sout);
				if(pds_look_extgetch() == KEY_ESC) {
					pds_extgetch();
					break;
				}
			}
		}
		clear_message();
	}
}

//------------------------------------------------------------------------
void playlist_editlist_insert_index(struct mainvars *mvp)
{
	struct playlist_side_info *psi = mvp->psip;
	struct playlist_entry_info *pei;
	struct frame *frp;
	struct mpxplay_infile_info_s *miis;
	if(!mvp->aktfilenum || (mvp->aktfilenum < psi->firstsong) || (mvp->aktfilenum > psi->lastentry))
		return;
	if(!playlist_editlist_addfile_one(psi, psi, mvp->aktfilenum, NULL, EDITLIST_MODE_ALL))
		return;
	pei = psi->lastentry;
	while(pei > (mvp->aktfilenum + 1)) {
		playlist_swap_entries(psi, pei, pei - 1);
		pei--;
	}
	playlist_fulltime_del(psi, pei);
	frp = mvp->frp0;
	miis = frp->infile_infos;
	pei->pstime = (long)((float)frp->frameNum * (float)miis->timemsec / (float)frp->allframes);
	funcbit_enable(pei->infobits, PEIF_INDEXED);
	mvp->aktfilenum->petime = pei->pstime;
	funcbit_enable(mvp->aktfilenum->infobits, PEIF_INDEXED);
	mvp->aktfilenum = pei;
	playlist_pei0_set(mvp, pei, 0);
	mpxplay_calculate_index_start_end(frp, mvp, mvp->pei0);
	playlist_editorhighline_set(psi, pei);
}

void playlist_editlist_delete_index(struct mainvars *mvp)
{
	struct playlist_side_info *psi = mvp->psie;
	struct playlist_entry_info *pei0 = psi->editorhighline, *pei1, *aktfi = NULL;

	if(!(pei0->infobits & PEIF_INDEXED))
		return;
	pei1 = pei0 + 1;

	if((pei1 <= psi->lastentry) && pei0->petime && (pei0->petime == pei1->pstime) && (pds_strcmp(pei0->filename, pei1->filename) == 0)) {
		pei1->pstime = pei0->pstime;
		if((pei0 == mvp->aktfilenum) || (pei1 == mvp->aktfilenum))
			aktfi = pei0;
		playlist_editlist_delfile_one(psi, pei0, EDITLIST_MODE_ALL);
	} else {
		pei1 = pei0 - 1;
		if((pei1 >= psi->firstsong) && (pei1->infobits & PEIF_INDEXED) && pei0->pstime && (pei0->pstime == pei1->petime) && (pds_strcmp(pei0->filename, pei1->filename) == 0)) {
			if(pei0->petime)
				pei1->petime = pei0->petime;
			else if(pei0->timemsec) {	//
				pei1->timemsec = pei0->timemsec;	// ???
				pei1->petime = 0;
			}
			if((pei0 == mvp->aktfilenum) || (pei1 == mvp->aktfilenum))
				aktfi = pei1;
			playlist_editlist_delfile_one(psi, pei0, EDITLIST_MODE_ALL);
			playlist_editorhighline_set(psi, pei1);
		}
	}

	if(!pei1->pstime && !pei1->petime)
		funcbit_disable(pei1->infobits, PEIF_INDEXED);
	if(aktfi) {
		mvp->newfilenum = NULL;
		mvp->aktfilenum = aktfi;
		playlist_pei0_set(mvp, aktfi, 0);
		mpxplay_calculate_index_start_end(mvp->frp0, mvp, mvp->pei0);
	}
}

//----------------------------------------------------------------------
struct playlist_entry_info *playlist_editlist_updatesides_add_dft(struct mainvars *mvp, char *fullname, unsigned int dft_type)
{
	struct playlist_side_info *psi = mvp->psi0;
	struct playlist_entry_info *pei, *pei_pos = NULL, pei_tmp;
	unsigned int side;
	char path[MAX_PATHNAMELEN];

	pds_getpath_from_fullname(path, fullname);

	for(side = 0; side < PLAYLIST_MAX_SIDES; side++, psi++) {
		if((psi->editsidetype & PLT_DIRECTORY) && !psi->sublistlevel && (pds_stricmp(psi->currdir, path) == 0)) {
			pei = playlist_search_filename(psi, fullname, -1, NULL);
			if(!pei) {
				pds_memset(&pei_tmp, 0, sizeof(struct playlist_entry_info));
				pei_tmp.filename = &fullname[0];
				pei_tmp.mdds = psi->mdds;
				if(dft_type & DFTM_PLAYLIST) {
					if((desktopmode & DTM_EDIT_LOADLIST) && !(psi->psio->editsidetype & PLT_DIRECTORY))
						pei_tmp.entrytype = DFT_PLAYLIST;
					else
						pei_tmp.entrytype = DFT_SUBLIST;
				} else
					pei_tmp.entrytype = dft_type;
				pei_tmp.id3info[I3I_DFT_STORE] = playlist_loadsub_get_dftstr(pei_tmp.entrytype);
				playlist_editlist_addfile_one(NULL, psi, &pei_tmp, NULL, EDITLIST_MODE_ALL);
				pei = playlist_editlist_addfileone_postproc(psi, psi->lastentry);
				refdisp |= RDT_EDITOR;
			}
			if(psi == mvp->psie)
				pei_pos = pei;
		}
	}
	return pei_pos;
}
