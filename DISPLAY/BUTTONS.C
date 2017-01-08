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
//function: screen buttons

#include "newfunc\newfunc.h"
#include "display.h"
#include "control\control.h"
#include "au_mixer\au_mixer.h"
#include <mpxinbuf.h>

static unsigned int get_browserbox_color(struct mainvars *, struct playlist_entry_info *);

extern keyconfig kb[];

struct buttons dk[DK_MAX_SIZE] = { {BTNF_BASE, 32, 0, 47, 13, NULL, NULL, NULL, ""},
{BTNF_NONE, 1, 1, 6, 0, NULL, NULL, "Mpxplay", "ver 1.57"},	//1. DK_FILEINFO
{BTNF_NONE, 9, 1, 7, 0, NULL, NULL, NULL, "filetype"},
{BTNF_NONE, 18, 1, 7, 0, NULL, NULL, NULL, "bit/rate"},
{BTNF_NONE, 27, 1, 6, 0, NULL, NULL, NULL, "freqency"},
{BTNF_NONE, 35, 1, 7, 0, NULL, NULL, NULL, "chanmode"},
{BTNF_NONE, 44, 1, 2, 0, NULL, NULL, NULL, "soundcrd"},
{BTNF_NONE, 47, 0, 0, 0, &kb[30].c, &kb[31].c, NULL, "! EXIT !"},
{BTNF_NONE, 99, 3, 0, 0, &kb[67].c, NULL, NULL, "tracknum"},	//8. DK_ID3INFO
{BTNF_NONE, 2, 3, 9, 0, &kb[67].c, NULL, NULL, " artist "},
{BTNF_NONE, 25, 3, 9, 0, &kb[67].c, NULL, NULL, " title  "},	//10
{BTNF_NONE, 1, 4, 9, 0, NULL, NULL, NULL, " album  "},
{BTNF_NONE, 20, 4, 3, 0, NULL, NULL, NULL, "  date  "},
{BTNF_NONE, 25, 4, 9, 0, NULL, NULL, NULL, " genre  "},
{BTNF_NONE, 30, 4, 9, 0, NULL, NULL, NULL, " comment"},
{BTNF_NONE, 1, 6, 3, 0, &kb[53].c, &kb[54].c, NULL, "currsong"},	//15. DK_TIMEPOS
{BTNF_NONE, 6, 6, 3, 0, &kb[53].c, &kb[54].c, NULL, "allsongs"},
{BTNF_NONE, 11, 6, 12, 0, &kb[53].c, &kb[54].c, NULL, " frames "},
{BTNF_NONE, 25, 6, 12, 0, &kb[53].c, &kb[54].c, NULL, " times  "},
{BTNF_NONE, 39, 6, 7, 0, NULL, NULL, NULL, "buf/cpu "},	//19. DK_BUFCPU
{BTNF_NONE, 39, 7, 7, 0, NULL, NULL, NULL, "function"},	//20. DK_FUNCTION
{BTNF_NONE, 39, 8, 7, 0, NULL, NULL, NULL, "mousepos"},	//21
{BTNF_FULL, 0, 8, 3, 2, &kb[23].c, NULL, "A-", "prvAlbum"},	//22. DK_CONTROL
{BTNF_FULL, 4, 8, 3, 2, &kb[20].c, &kb[22].c, "³", "prevsong"},
{BTNF_FULL | BTNF_PRESS, 8, 8, 3, 2, &kb[0].c, &kb[1].c, "", " rewind "},
{BTNF_FULL, 12, 8, 4, 2, &kb[50].c, &kb[52].c, "/º", "ply/paus"},
{BTNF_FULL, 17, 8, 3, 2, &kb[34].c, &kb[35].c, "ÞÝ", "  stop  "},
{BTNF_FULL | BTNF_PRESS, 21, 8, 3, 2, &kb[2].c, &kb[3].c, "", "fforward"},
{BTNF_FULL, 25, 8, 3, 2, &kb[21].c, NULL, "³", "nextsong"},
{BTNF_FULL, 29, 8, 3, 2, &kb[24].c, NULL, "A+", "nxtAlbum"},
{BTNF_ONLY | BTNF_UPTEXT, 33, 8, 4, 2, NULL, NULL, "vol", ""},	//30. DK_OPTIONS
{BTNF_LEFT | BTNF_PRESS, 33, 8, 1, 2, &kb[82].c, &kb[83].c, NULL, "vol-down"},
{BTNF_RGHT | BTNF_PRESS, 36, 8, 1, 2, &kb[80].c, &kb[81].c, NULL, "volum-up"},
{BTNF_FULL, 0, 11, 3, 2, &kb[59].c, &kb[60].c, "Re", " replay "},
{BTNF_FULL, 4, 11, 3, 2, &kb[61].c, &kb[62].c, "Rn", " random "},
{BTNF_FULL, 8, 11, 3, 2, &kb[73].c, &kb[74].c, "Av", "autovol "},
{BTNF_FULL, 12, 11, 3, 2, &kb[55].c, &kb[56].c, "Cf", "crosfade"},
{BTNF_FULL, 16, 11, 5, 2, &kb[57].c, &kb[58].c, NULL, "fadOutIn"},
{BTNF_ONLY | BTNF_UPTEXT, 22, 11, 4, 2, NULL, NULL, "sur", ""},
{BTNF_LEFT | BTNF_PRESS, 22, 11, 1, 2, &kb[87].c, NULL, NULL, "sur-down"},
{BTNF_RGHT | BTNF_PRESS, 25, 11, 1, 2, &kb[86].c, NULL, NULL, "surr-up "},	//40

{BTNF_ONLY | BTNF_UPTEXT, 27, 11, 4, 2, NULL, NULL, "spd", ""},	//41=DK_OPTIONS+11
{BTNF_LEFT | BTNF_PRESS, 27, 11, 1, 2, &kb[89].c, NULL, NULL, "spd-down"},
{BTNF_RGHT | BTNF_PRESS, 30, 11, 1, 2, &kb[88].c, NULL, NULL, "speed-up"},
{BTNF_ONLY | BTNF_UPTEXT, 32, 11, 4, 2, NULL, NULL, "bal", ""},
{BTNF_LEFT | BTNF_PRESS, 32, 11, 1, 2, &kb[84].c, NULL, NULL, "L-balanc"},
{BTNF_RGHT | BTNF_PRESS, 35, 11, 1, 2, &kb[85].c, NULL, NULL, "R-balanc"},
{BTNF_ONLY | BTNF_UPTEXT, 37, 11, 4, 2, NULL, NULL, "bas", ""},
{BTNF_LEFT | BTNF_PRESS, 37, 11, 1, 2, &kb[103].c, NULL, NULL, "bas-down"},
{BTNF_RGHT | BTNF_PRESS, 40, 11, 1, 2, &kb[102].c, NULL, NULL, "bass-up "},
{BTNF_ONLY | BTNF_UPTEXT, 42, 11, 4, 2, NULL, NULL, "trb", ""},	//50
{BTNF_LEFT | BTNF_PRESS, 42, 11, 1, 2, &kb[105].c, NULL, NULL, "trb-down"},
{BTNF_RGHT | BTNF_PRESS, 45, 11, 1, 2, &kb[104].c, NULL, NULL, "trebl-up"},

{BTNF_NONE, 39, 9, 1, 0, &kb[75].c, &kb[76].c, "Sw", "swapchan"},	//53  DK_SMALLBTNS
{BTNF_NONE, 41, 9, 1, 0, &kb[106].c, &kb[106].c, "Ln", "loudness"},
{BTNF_NONE, 43, 9, 1, 0, &kb[63].c, NULL, "Ap", "autopaus"},
{BTNF_NONE, 45, 9, 1, 0, &kb[64].c, NULL, "Pn", "pausnext"},
{BTNF_ENDOFBTN, 0, 0, 0, 0, NULL, NULL, NULL, ""},	//57
};								//58. DK_BROWSER

extern struct desktoppos dtp;
extern unsigned int playrand, desktopmode, displaymode, refdisp, textscreen_maxx;
extern unsigned int prebuffertype;
extern mainvars mvps;

static unsigned int browser_lastbox;
unsigned int lastmousebox;

void mpxplay_display_buttons_init(void)
{
	// convert relative buttonpos to absolute
	struct buttons *dp = &dk[0];
	unsigned int basex = dp->xpos, basey = dp->ypos, i;
	dp++;
	while(dp->boxflag != BTNF_ENDOFBTN) {
		dp->xpos += basex;
		dp->ypos += basey;
		dp++;
	}
	dp++;
	dp->boxflag = BTNF_END;

	if(mvps.aui->mixer_infobits & AUINFOS_MIXERINFOBIT_SPEED1000) {
		dp = &dk[DK_OPTIONS + 11];
		dp->xsize++;
		dp += 2;
		for(i = 0; i < 10; i++, dp++)
			dp->xpos++;
	}
	if(prebuffertype & PREBUFTYPE_BACK)
		pds_strcpy(dk[DK_BUFCPU].mousepostext, "filebuff");
}

struct buttons *mpxplay_control_mouse_xy_to_dp(struct mainvars *mvp, unsigned int x, unsigned int y)
{
	struct buttons *dp = &dk[DK_FILEINFO];
	while(dp->boxflag != BTNF_END) {
		if((dp->boxflag & BTNF_MOUSEPOS) && (x >= dp->xpos) && (y >= dp->ypos) && (x <= (dp->xpos + dp->xsize)) && (y <= (dp->ypos + dp->ysize))) {
			if((dp->ypos < dtp.endofbuttonsy) || ((dp->boxflag & BTNF_BROWSER) && (dp->ypos < dtp.songposline_y))) {
				if(dp->boxflag & BTNF_BROWSER) {
					struct playlist_side_info *psi = mvp->psip;
					if((psi->firstsong + (unsigned int)(dp->keycode)) <= psi->lastentry) {
						draw_mouse_listpos(mvp, (unsigned int)(dp->keycode));
						recolor_lastbuttonbox_mousepos(mvp, dp);
						color_one_buttonbox(dp, CLA_BTNBOX);
						return dp;
					}
				} else {
					draw_mousepos_text((unsigned int)(dp->keycode), dp->mousepostext);
					recolor_lastbuttonbox_mousepos(mvp, dp);
					color_one_buttonbox(dp, CLH_BTNBOX);
					return dp;
				}
			}
		}
		dp++;
	}
	return NULL;
}

void color_one_buttonbox(struct buttons *dp, unsigned int color)
{
	if((dp->boxflag & BTNF_FULL) || (dp->boxflag & BTNF_ONLY) || (dp->boxflag & BTNF_LEFT)
	   || (dp->boxflag & BTNF_RGHT)) {
		unsigned int j;
		for(j = 0; j <= dp->xsize; j++) {
			pds_textdisplay_setcolorxy(color, dp->xpos + j, dp->ypos);
			pds_textdisplay_setcolorxy(color, dp->xpos + j, dp->ypos + dp->ysize);
		}
		for(j = 0; j <= dp->ysize; j++) {
			if(!(dp->boxflag & BTNF_RGHT))
				pds_textdisplay_setcolorxy(color, dp->xpos, dp->ypos + j);
			if(!(dp->boxflag & BTNF_LEFT))
				pds_textdisplay_setcolorxy(color, dp->xpos + dp->xsize, dp->ypos + j);
		}
		if(!(dp->boxflag & BTNF_RGHT)) {
			pds_textdisplay_setcolorxy(color, dp->xpos, dp->ypos);
			pds_textdisplay_setcolorxy(color, dp->xpos, dp->ypos + dp->ysize);
		}
		if(!(dp->boxflag & BTNF_LEFT)) {
			pds_textdisplay_setcolorxy(color, dp->xpos + dp->xsize, dp->ypos);
			pds_textdisplay_setcolorxy(color, dp->xpos + dp->xsize, dp->ypos + dp->ysize);
		}
		if(dp->intext != NULL && (dp->boxflag & BTNF_UPTEXT))
			for(j = 0; j < pds_strlen(dp->intext); j++)
				pds_textdisplay_setcolorxy(CL_BTNUPTEXT, dp->xpos + 1 + j, dp->ypos);
	}
}

static struct buttons *lastdp_mousepos;

void recolor_lastbuttonbox_key(struct buttons *dp, unsigned int newbutton)
{
	static struct buttons *lastdp_key;
	static unsigned int btn_flashdelay;
	if(btn_flashdelay)
		btn_flashdelay--;
	if(newbutton)
		btn_flashdelay = BTN_FLASHTIME;
	if(!btn_flashdelay || newbutton) {
		if(lastdp_key != NULL) {
			if(lastdp_key == lastdp_mousepos)
				color_one_buttonbox(lastdp_key, CLH_BTNBOX);
			else
				color_one_buttonbox(lastdp_key, CL_BTNBOX);
		}
		lastdp_key = dp;
	}
}

void recolor_lastbuttonbox_mousepos(struct mainvars *mvp, struct buttons *dp)
{
	if(lastdp_mousepos != NULL) {
		struct playlist_side_info *psip = mvp->psip;
		unsigned int color;
		if(lastdp_mousepos->boxflag & BTNF_BROWSER) {
			color = get_browserbox_color(mvp, psip->firstsong + (unsigned int)(lastdp_mousepos->keycode));
			if(desktopmode & DTM_EDIT_FOLLOWBROWSER) {
				if(((dp == NULL) || !(dp->boxflag & BTNF_BROWSER)) && (mvp->psie == psip)) {
					playlist_editorhighline_set(psip, mvp->aktfilenum);
					refdisp |= RDT_EDITOR;
				}
			}
		} else
			color = CL_BTNBOX;
		color_one_buttonbox(lastdp_mousepos, color);
	}
	lastdp_mousepos = dp;
	if(dp == NULL)
		clear_mousepos_text();
}

void color_buttonbox_key(unsigned int extkey)
{
	if((displaymode & DISP_FULLSCREEN) && (displaymode & DISP_NOFULLEDIT)) {
		struct buttons *dp = &dk[DK_CONTROL];
		while(dp->boxflag != BTNF_ENDOFBTN) {
			if(((dp->keycode && (*(dp->keycode)) == extkey)
				|| (dp->keycode2 && (*(dp->keycode2)) == extkey))
			   && (dp->ypos < dtp.endofbuttonsy)) {
				recolor_lastbuttonbox_key(dp, 1);
				color_one_buttonbox(dp, CLA_BTNBOX);
				break;
			}
			dp++;
		}
	}
}

//*************************************************************************
// song browser functions
//*************************************************************************

void generate_browserboxes(void)
{
	unsigned int boxnum = 0, begin2;
	struct buttons *dp = &dk[DK_BROWSER];

	browser_lastbox = 0;
	if(!(displaymode & DISP_ANALISER) && (dtp.endofbuttonsy > 6)) {
		unsigned int ysize = (dtp.endofbuttonsy + 1);
		browser_lastbox = (ysize / 3) * (32 >> 2);
		while(boxnum < browser_lastbox) {
			dp->boxflag = BTNF_BROWSER | BTNF_FULL;
			dp->xpos = (boxnum % (32 >> 2)) * 4;
			dp->ypos = (boxnum / (32 >> 2)) * 3;
			dp->xsize = 3;
			dp->ysize = 2;
			dp->keycode = (unsigned int *)boxnum;
			boxnum++;
			dp++;
		}
	}
	begin2 = dtp.endofbuttonsy + 1;
	if(dtp.songposline_y > begin2) {
		unsigned int box2num = 0;
		browser_lastbox += ((dtp.songposline_y - begin2) / 3) * (textscreen_maxx >> 2);
		while(boxnum < browser_lastbox) {
			dp->boxflag = BTNF_BROWSER | BTNF_FULL;
			dp->xpos = (box2num % (textscreen_maxx >> 2)) * 4;
			dp->ypos = (box2num / (textscreen_maxx >> 2)) * 3 + begin2;
			dp->xsize = 3;
			dp->ysize = 2;
			dp->keycode = (unsigned int *)boxnum;
			boxnum++;
			box2num++;
			dp++;
		}
	}
	dp->boxflag = BTNF_END;
	boxnum += DK_BROWSER;
	while(++boxnum < DK_MAX_SIZE) {
		dp++;
		dp->boxflag = 0;
	}
}

void drawbrowser(struct mainvars *mvp)
{
	if(displaymode & DISP_FULLSCREEN) {
		struct playlist_side_info *psi = mvp->psip;
		struct playlist_entry_info *pei = psi->firstsong;
		struct playlist_entry_info *end = pei + browser_lastbox;

		for(; pei < end; pei++)
			draw_browserbox(mvp, pei);

		lastmousebox = LASTMOUSEBOX_INVALID;
	}
}

void draw_browserbox(struct mainvars *mvp, struct playlist_entry_info *pei)
{
	struct playlist_side_info *psi = mvp->psip;
	unsigned int boxnum, x, y, color;
	char souttext[16];

	if(!(displaymode & DISP_FULLSCREEN) || pei < psi->firstsong)
		return;
	color = get_browserbox_color(mvp, pei);
	boxnum = pei - psi->firstsong;
	if(boxnum >= browser_lastbox)
		return;
	x = dk[DK_BROWSER + boxnum].xpos;
	y = dk[DK_BROWSER + boxnum].ypos;
	boxnum++;
	if(boxnum > 99)
		boxnum = boxnum % 100;
	pds_textdisplay_textxy(color, x, y, "ÚÄÄ¿");
	sprintf(souttext, "³%2d³", boxnum);
	pds_textdisplay_textxy(color, x, y + 1, souttext);
	pds_textdisplay_textxy(color, x, y + 2, "ÀÄÄÙ");
}

static unsigned int get_browserbox_color(struct mainvars *mvp, struct playlist_entry_info *pei)
{
	unsigned int color;

	if(pei == mvp->aktfilenum)
		color = CL_BROWSER_AKTFILENUM;
	else {
		if(pei > mvp->psip->lastentry)
			color = CL_BROWSER_OVERENTRY;
		else {
			if(playrand) {
				if(pei->infobits & PEIF_RNDPLAYED)
					color = CL_BROWSER_PLAYED;
				else
					color = CL_BROWSER_NOTPLAYED;
			} else {
				if(pei < mvp->aktfilenum)
					color = CL_BROWSER_PLAYED;
				else
					color = CL_BROWSER_NOTPLAYED;
			}
		}
	}
	return (color | (CLB_BROWSER_BASE << 4));
}
