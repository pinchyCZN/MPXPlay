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
//function:id3 (tag) list routines (load,assign,save)

#include <malloc.h>
#include "newfunc\newfunc.h"
#include "playlist.h"
#include "display\display.h"

typedef int (*qsc_t) (const void *, const void *);

typedef char *ID3LISTTYPE[MAX_ID3LISTPARTS];
static ID3LISTTYPE *id3list;
static char *id3listdataarea;
static unsigned int id3listmaxlen, id3listlength;
static struct playlist_side_info *id3listloadside;

extern char *id3loadname, *id3savename;
extern unsigned int loadid3list, saveid3list, id3savefields, loadid3tag;
extern unsigned int writeid3tag, id3textconv, playlist_max_filenum_list;

// sort id3list by filenames (with qsort) (sorted from right!)
static int id3list_order_id3list_qs(char **i1, char **i2)
{
	return pds_strricmp(i1[0], i2[0]);
}

// logarithmic search in the sorted field
static ID3LISTTYPE *id3list_log_search(char *searchstr)
{
	ID3LISTTYPE *id3lp = id3list;
	unsigned int pos, bottom, top, center;
	int result;

	bottom = 0;
	top = id3listlength;

	result = 1;

	center = id3listlength;
	center >>= 1;

	if(center > 2) {
		do {
			pos = bottom + center;
			result = pds_strricmp(searchstr, id3lp[pos][0]);
			if(result == 0)
				break;
			if(result < 0)
				top = pos;
			else
				bottom = pos;
			center = (top - bottom);
			center >>= 1;
		} while(center > 2);
	}

	if(center <= 2) {
		pos = bottom;
		do {
			result = pds_strricmp(searchstr, id3lp[pos][0]);
			if(result == 0)
				break;
			pos++;
		} while(pos <= top);
	}
	if(result == 0)
		return &id3lp[pos];

	return NULL;
}


static unsigned int open_id3list(struct playlist_side_info *psi, void **id3loadfile)
{
	mpxp_filesize_t filesize;
	id3listloadside = NULL;
	id3listlength = 0;
	if(loadid3list == ID3LISTTYPE_LOCAL)
		*id3loadfile = mpxplay_diskdrive_textfile_open(psi->mdds, id3loadname, (O_RDONLY | O_TEXT));
	else
		*id3loadfile = pds_fopen(id3loadname, "rt");
	if(!(*id3loadfile))
		return 0;
	if(!id3list) {
		id3list = malloc(id3listmaxlen * MAX_ID3LISTPARTS * sizeof(char *));
		if(!id3list)
			return 0;
	}
	if(id3listdataarea)
		free(id3listdataarea);
	if(loadid3list == ID3LISTTYPE_LOCAL)
		filesize = mpxplay_diskdrive_textfile_filelength(*id3loadfile);
	else
		filesize = filelength(((FILE *) * id3loadfile)->_handle);
	id3listdataarea = malloc(filesize);
	if(!id3listdataarea)
		return 0;
	pds_memset(id3list, 0, id3listmaxlen * MAX_ID3LISTPARTS * sizeof(char *));
	return 1;
}

void playlist_id3list_close(void)
{
	if(id3list) {
		free(id3list);
		id3list = NULL;
	}
	if(id3listdataarea) {
		free(id3listdataarea);
		id3listdataarea = NULL;
	}
}

void playlist_id3list_load(struct mainvars *mvp, struct playlist_side_info *psi)
{
	void *id3loadfile;
	char *i3mem, *p2, **id3lp;
	// 3 different possible id3list format with max. 4 line-parts
	char *separators[3] = { "°±²", "°:±", " :" }, *listparts[MAX_ID3LISTPARTS + 2];
	const int sep_lens[3] = { 4, 4, 3 };
	const int do852s[MAX_ID3LISTPARTS] = { 0, 1, 1, 0 };	// parts: filename,artist,title,time/other
	int id3listcount, i, j, foundparts, i3mlinelen;
	char strtemp[MAX_ID3LEN];

	switch (loadid3list) {
	case ID3LISTTYPE_LOCAL:
		if(!psi || !(psi->editsidetype & PLT_DIRECTORY) || psi->sublistlevel)
			return;
		id3listmaxlen = playlist_max_filenum_list / PLAYLIST_MAXFILENUMDIV_DIR;
		break;
	case ID3LISTTYPE_GLOBAL:
		id3listmaxlen = playlist_max_filenum_list;
		break;
	default:
		return;
	}

	if(!open_id3list(psi, &id3loadfile)) {
		if(writeid3tag) {
			pds_textdisplay_printf("ID3-(info)list loading error! Cannot write ID3-tags...");
			writeid3tag = 0;
		}
		return;
	}

	i3mem = id3listdataarea;
	id3lp = &id3list[0][0];
	id3listcount = 0;
	while(id3listcount < id3listmaxlen) {
		if(loadid3list == ID3LISTTYPE_LOCAL) {
			if(!mpxplay_diskdrive_textfile_readline(id3loadfile, strtemp, sizeof(strtemp) - 1))
				break;
		} else {
			if(!fgets(strtemp, sizeof(strtemp) - 1, (FILE *) id3loadfile))
				break;
			i = pds_strlen(strtemp);
			if(!i)
				continue;
			if(strtemp[i - 1] == '\n')
				strtemp[i - 1] = 0;
		}
		for(i = 0; i < 3; i++) {
			pds_memset(&(listparts[0]), 0, MAX_ID3LISTPARTS * sizeof(char *));
			i3mlinelen = pds_strcpy(i3mem, strtemp) + 1;
			pds_listline_slice(&(listparts[0]), separators[i], i3mem);
			foundparts = 0;
			for(j = 0; j < sep_lens[i]; j++)
				foundparts += (listparts[j]) ? 1 : 0;
			if((foundparts == sep_lens[i]) || (i == 2)) {
				if(listparts[2] == NULL)	// if no 'title'
					if(listparts[1] != NULL) {
						listparts[2] = listparts[1];	// use 'artist' for title
						listparts[1] = NULL;
					}
				for(j = 0; j < sep_lens[i]; j++) {
					p2 = listparts[j];
					if(p2) {
						unsigned int p2len = pds_strcutspc(p2);
						if(p2len) {
							if(do852s[j])
								mpxplay_playlist_textconv_do(p2, p2len, 0);
							id3lp[j] = p2;
						}
					}
				}
				i3mem += i3mlinelen;
				id3lp += MAX_ID3LISTPARTS;
				id3listcount++;
				break;
			}
		}
	}
	if(id3listcount > 0) {
		id3listloadside = psi;	// ID3LISTTYPE_LOCAL
		qsort((void *)id3list, id3listcount, MAX_ID3LISTPARTS * sizeof(char *), (qsc_t) id3list_order_id3list_qs);
	}
	id3listlength = id3listcount;
	if(loadid3list == ID3LISTTYPE_LOCAL)
		mpxplay_diskdrive_textfile_close(id3loadfile);
	else
		pds_fclose((FILE *) id3loadfile);
}

unsigned int get_onefileinfos_from_id3list(struct playlist_side_info *psi, struct playlist_entry_info *pei, unsigned int id3loadflags)
{
	char *id3p, **id3lp;
	unsigned int found = 0, len;

	if(!id3list || !id3listlength || !(id3loadflags & ID3LOADMODE_LIST))
		return found;

	switch (loadid3list) {
	case ID3LISTTYPE_LOCAL:
		if(psi != id3listloadside)
			return found;
		break;
	case ID3LISTTYPE_GLOBAL:
		break;
	default:
		return found;
	}

	if((pei >= psi->firstsong) && (pei <= psi->lastentry)) {
		id3lp = id3list_log_search(pei->filename);
		if(id3lp) {
			if(id3lp[1] || id3lp[2]) {
				id3p = psi->id3infolastp;
				if(id3lp[1]) {
					len = pds_strlen(id3lp[1]) + 1;
					if((id3p + len) < psi->id3infoendp) {
						pds_strcpy(id3p, id3lp[1]);
						pei->id3info[I3I_ARTIST] = id3p;
						id3p += len;
					}
				}
				if(id3lp[2]) {
					len = pds_strlen(id3lp[2]) + 1;
					if((id3p + len) < psi->id3infoendp) {
						pds_strcpy(id3p, id3lp[2]);
						pei->id3info[I3I_TITLE] = id3p;
						id3p += len;
					}
				}
				psi->id3infolastp = id3p;
				pei->infobits |= PEIF_ID3EXIST;
				found |= EDITLIST_MODE_ID3;
			}
			if(id3lp[3] && (pei->entrytype == DFT_NOTCHECKED || pei->entrytype == DFT_UNKNOWN) && (id3loadflags & ID3LOADMODE_FASTSEARCH)) {
				if(id3lp[3][8] != '|') {	// if not old mxu style (v1.42)
					unsigned int timesec = pds_atol16(id3lp[3]);
					if(timesec & MXUFLAG_ENABLED) {
						pei->timemsec = (timesec & MXUFLAG_TIMEMASK) * 1000;
						pei->infobits |= PEIF_ENABLED;
						pei->entrytype = DFT_NOTCHECKED;
						found |= EDITLIST_MODE_HEAD;
					}
				}
			}
		}
	}
	return found;
}

/*unsigned int get_onefileinfos_from_id3list(struct playlist_side_info *psi,struct playlist_entry_info *pei,unsigned int fastsearch)
{
 char *id3p,**id3lp;
 unsigned int found=0,len;

 if(!id3list || !id3listlength || !(loadid3tag&ID3LOADMODE_LIST))
  return found;

 switch(loadid3list){
  case ID3LISTTYPE_LOCAL:
    if(psi!=id3listloadside)
     return found;
    break;
  case ID3LISTTYPE_GLOBAL:
   break;
  default:return found;
 }

 if((pei>=psi->firstsong) && (pei<=psi->lastentry)){
  id3lp=id3list_log_search(pei->filename);
  if(id3lp){
   if(id3lp[1] || id3lp[2]){
    id3p=psi->id3infolastp;
    if(id3lp[1]){
     len=pds_strlen(id3lp[1])+1;
     if((id3p+len)<psi->id3infoendp){
      pds_strcpy(id3p,id3lp[1]);
      pei->id3info[I3I_ARTIST]=id3p;
      id3p+=len;
     }
    }
    if(id3lp[2]){
     len=pds_strlen(id3lp[2])+1;
     if((id3p+len)<psi->id3infoendp){
      pds_strcpy(id3p,id3lp[2]);
      pei->id3info[I3I_TITLE]=id3p;
      id3p+=len;
     }
    }
    psi->id3infolastp=id3p;
    pei->infobits|=PEIF_ID3EXIST;
    found|=EDITLIST_MODE_ID3;
   }
   if(id3lp[3] && (pei->entrytype==DFT_NOTCHECKED || pei->entrytype==DFT_UNKNOWN) && fastsearch){
    if(id3lp[3][8]!='|'){ // if not old mxu style (v1.42)
     unsigned int timesec=pds_atol16(id3lp[3]);
     if(timesec&MXUFLAG_ENABLED){
      pei->timemsec=(timesec&MXUFLAG_TIMEMASK)*1000;
      pei->infobits|=PEIF_ENABLED;
      pei->entrytype=DFT_NOTCHECKED;
      found|=EDITLIST_MODE_HEAD;
     }
    }
   }
  }
 }
 return found;
}*/

/*void playlist_id3list_save(struct mainvars *mvp)
{
 FILE *id3savefile;
 unsigned int songcounter,isf,len_id3,len_artist,len_title;
 struct frame *frp;
 struct mpxplay_audio_decoder_info_s *adi;
 struct playlist_side_info *psi;
 struct playlist_entry_info *pei;
 char *shortfname,souttext[300],strtemp[300],lastdir[300];
 const char *formats[3][2]={{"%s","%s"},{"%.64s","%.30s"},{"%-28.28s","%-19.19s"}};

 if(!saveid3list || (id3savefile=pds_fopen(id3savename,"wt"))==NULL)
  return;

 pds_textdisplay_printf("Extended playlist save ...");

 isf=id3savefields;
 frp=mvp->frp0+2;
 adi=frp->infile_infos->audio_decoder_infos;
 psi=mvp->psil;
 songcounter=1;
 lastdir[0]=0;

 if(isf&(IST_TIME|IST_BITRATE|IST_FILESIZE))
  len_id3=64;
 else
  len_id3=128;
 if(isf&IST_FILENAME)
  len_id3-=14;
 else
  len_id3-=4;
 len_artist=len_id3/3;
 len_title=len_id3-len_artist;

 for(pei=psi->firstentry;pei<=psi->lastentry;pei++){
  playlist_chkentry_get_onefileinfos_is(psi,pei);
  shortfname=pds_getpath_from_fullname(strtemp,pei->filename);
  if(isf&IST_DIRECTORY){
   if(pds_stricmp(lastdir,strtemp)!=0){
    fprintf(id3savefile,"***** %s **********\n",strtemp);
    pds_strcpy(lastdir,strtemp);
    songcounter=1;
   }
  }
  if(isf&IST_FILENAME){
   if(isf&IST_FULLPATH)
    sprintf(souttext,"%-12s  ",pei->filename);
   else
    sprintf(souttext,"%-12s  ",shortfname);
  }else
   sprintf(souttext,"%2d. ",songcounter++);

  if(!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE]){
   if(isf&(IST_TIME|IST_BITRATE|IST_FILESIZE)){
    pds_strncpy(strtemp,shortfname,len_id3);
    strtemp[len_id3]=0;
    //sprintf(strtemp,"%48.48s",(isf&IST_FILENAME)? "":shortfname);
    pds_strcat(souttext,strtemp);
   }
  }else{
   int form;
   if(isf&IST_AT_FIXED)
    form=2;
   else
    if(isf&(IST_TIME|IST_BITRATE|IST_FILESIZE))
     form=1;
    else
     form=0;
   sprintf(strtemp,formats[form][1],((pei->id3info[I3I_ARTIST])? pei->id3info[I3I_ARTIST]:""));
   if(funcbit_test(id3textconv,ID3TEXTCONV_CP_BACK))
    mpxplay_playlist_textconv_back(strtemp,strtemp);
   pds_strcat(souttext,strtemp);
   if(isf&(IST_TIME|IST_BITRATE|IST_FILESIZE)){
    pds_strcat(souttext,":");
    sprintf(strtemp,formats[form][0],((pei->id3info[I3I_TITLE])? pei->id3info[I3I_TITLE]:""));
    if(funcbit_test(id3textconv,ID3TEXTCONV_CP_BACK))
     mpxplay_playlist_textconv_back(strtemp,strtemp);
    pds_strcat(souttext,strtemp);
   }else{
    pds_strcat(souttext," : ");
    if(pei->id3info[I3I_TITLE]){
     sprintf(strtemp,formats[form][0],pei->id3info[I3I_TITLE]);
     if(funcbit_test(id3textconv,ID3TEXTCONV_CP_BACK))
      mpxplay_playlist_textconv_back(strtemp,strtemp);
     pds_strcat(souttext,strtemp);
    }
   }
  }

  if(isf&(IST_TIME|IST_BITRATE|IST_FILESIZE))
   pds_strcat(souttext," ");
  if((pei->infobits&PEIF_ENABLED) || frp->infile_infos->timemsec){
   if(isf&IST_TIME){
    long timesec=(playlist_entry_get_timemsec(pei)+500)/1000;
    sprintf(strtemp,"%d:%2.2d",timesec/60,timesec%60);
    pds_strcat(souttext,strtemp);
   }
   if(isf&IST_BITRATE){
    sprintf(strtemp,"%s%3d",(isf&IST_TIME)? "-":"",(adi->bitrate)? adi->bitrate:adi->bits);
    pds_strcat(souttext,strtemp);
   }
   if(isf&IST_FILESIZE){
    sprintf(strtemp,"%s%1.1fMB",(isf&(IST_TIME|IST_BITRATE))? "-":"",(float)frp->filesize/1048576.0);
    pds_strcat(souttext,strtemp);
   }
  }
  pds_strcat(souttext,"\n");
  fprintf(id3savefile,souttext);
 }
 mpxplay_close_program(0);
}*/


void playlist_id3list_save(struct mainvars *mvp)
{
	FILE *id3savefile;
	unsigned int songcounter, isf;
	struct frame *frp;
	struct mpxplay_audio_decoder_info_s *adi;
	struct playlist_side_info *psi;
	struct playlist_entry_info *pei;
	char *shortfname, souttext[300], strtemp[300], lastdir[300];
	const char *formats[3][2] = { {"%s", "%s"}, {"%.64s", "%.30s"}, {"%-28.28s", "%-19.19s"} };

	if(!saveid3list || (id3savefile = pds_fopen(id3savename, "wt")) == NULL)
		return;

	pds_textdisplay_printf("Extended playlist save ...");

	isf = id3savefields;
	frp = mvp->frp0 + 2;
	adi = frp->infile_infos->audio_decoder_infos;
	psi = mvp->psil;
	songcounter = 1;
	lastdir[0] = 0;

	for(pei = psi->firstentry; pei <= psi->lastentry; pei++) {
		playlist_chkentry_get_onefileinfos_is(psi, pei);
		shortfname = pds_getpath_from_fullname(strtemp, pei->filename);
		if(isf & IST_DIRECTORY) {
			if(pds_stricmp(lastdir, strtemp) != 0) {
				fprintf(id3savefile, "***** %s **********\n", strtemp);
				pds_strcpy(lastdir, strtemp);
				songcounter = 1;
			}
		}
		if(isf & IST_FILENAME) {
			if(isf & IST_FULLPATH)
				sprintf(souttext, "%-12s  ", pei->filename);
			else
				sprintf(souttext, "%-12s  ", shortfname);
		} else
			sprintf(souttext, "%2d. ", songcounter++);

		if(!pei->id3info[I3I_ARTIST] && !pei->id3info[I3I_TITLE]) {
			if(isf & (IST_TIME | IST_BITRATE | IST_FILESIZE)) {
				sprintf(strtemp, "%48.48s", (isf & IST_FILENAME) ? "" : shortfname);
				pds_strcat(souttext, strtemp);
			}
		} else {
			int form;
			if(isf & IST_AT_FIXED)
				form = 2;
			else if(isf & (IST_TIME | IST_BITRATE | IST_FILESIZE))
				form = 1;
			else
				form = 0;
			sprintf(strtemp, formats[form][1], ((pei->id3info[I3I_ARTIST]) ? pei->id3info[I3I_ARTIST] : ""));
			if(funcbit_test(id3textconv, ID3TEXTCONV_CP_BACK))
				mpxplay_playlist_textconv_back(strtemp, strtemp);
			pds_strcat(souttext, strtemp);
			if(isf & (IST_TIME | IST_BITRATE | IST_FILESIZE)) {
				pds_strcat(souttext, ":");
				sprintf(strtemp, formats[form][0], ((pei->id3info[I3I_TITLE]) ? pei->id3info[I3I_TITLE] : ""));
				if(funcbit_test(id3textconv, ID3TEXTCONV_CP_BACK))
					mpxplay_playlist_textconv_back(strtemp, strtemp);
				pds_strcat(souttext, strtemp);
			} else {
				pds_strcat(souttext, " : ");
				if(pei->id3info[I3I_TITLE]) {
					sprintf(strtemp, formats[form][0], pei->id3info[I3I_TITLE]);
					if(funcbit_test(id3textconv, ID3TEXTCONV_CP_BACK))
						mpxplay_playlist_textconv_back(strtemp, strtemp);
					pds_strcat(souttext, strtemp);
				}
			}
		}

		if(isf & (IST_TIME | IST_BITRATE | IST_FILESIZE))
			pds_strcat(souttext, " ");
		if((pei->infobits & PEIF_ENABLED) || frp->infile_infos->timemsec) {
			if(isf & IST_TIME) {
				long timesec = (playlist_entry_get_timemsec(pei) + 500) / 1000;
				sprintf(strtemp, "%d:%2.2d", timesec / 60, timesec % 60);
				pds_strcat(souttext, strtemp);
			}
			if(isf & IST_BITRATE) {
				sprintf(strtemp, "%s%3d", (isf & IST_TIME) ? "-" : "", (adi->bitrate) ? adi->bitrate : adi->bits);
				pds_strcat(souttext, strtemp);
			}
			if(isf & IST_FILESIZE) {
				sprintf(strtemp, "%s%1.1fMB", (isf & (IST_TIME | IST_BITRATE)) ? "-" : "", (float)frp->filesize / 1048576.0);
				pds_strcat(souttext, strtemp);
			}
		}
		pds_strcat(souttext, "\n");
		fprintf(id3savefile, souttext);
	}
	mpxplay_close_program(0);
}
