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
//function: LCD handling (DOS only)

#include "newfunc\newfunc.h"
#include "control\control.h"

#ifdef WIN32
unsigned int outp(DWORD port, DWORD val)
{
	__asm mov edx, port;
	__asm mov eax, val;
	__asm out dx, al;
	return 0;
}
unsigned int inp(unsigned int portid)
{
	int value = 0;
	__asm mov edx, portid __asm in al, dx __asm mov value, eax return value;
}

#define MPXPLAY_LINK_LCD 1
#endif

#ifdef __DOS__
#define MPXPLAY_LINK_LCD 1
#endif

#ifdef MPXPLAY_LINK_LCD

#include "display.h"
#include "playlist\playlist.h"

static void convert_mpxini_to_lcditems(mpxini_line_t *, struct mpxini_part_t *);

static char *create_item_artist(struct mainvars *);
static char *create_item_title(struct mainvars *);
static char *create_item_album(struct mainvars *);
static char *create_item_year(struct mainvars *);
static char *create_item_genre(struct mainvars *);
static char *create_item_comment(struct mainvars *);
static char *create_item_tracknum(struct mainvars *);
static char *create_item_filename(struct mainvars *);
static char *create_item_pathname(struct mainvars *);
static char *create_item_songtime(struct mainvars *);
static char *create_item_entrynum(struct mainvars *);
static char *create_item_allsongnum(struct mainvars *);
static char *create_item_allsongtime(struct mainvars *);

static char *create_item_p_artist(struct mainvars *);
static char *create_item_p_title(struct mainvars *);
static char *create_item_p_album(struct mainvars *);
static char *create_item_p_year(struct mainvars *);
static char *create_item_p_genre(struct mainvars *);
static char *create_item_p_comment(struct mainvars *);
static char *create_item_p_tracknum(struct mainvars *);
static char *create_item_p_filename(struct mainvars *);
static char *create_item_p_pathname(struct mainvars *);
static char *create_item_p_songtime(struct mainvars *);
static char *create_item_p_songfreq(struct mainvars *);
static char *create_item_p_songchan(struct mainvars *);
static char *create_item_p_bitrate(struct mainvars *);
static char *create_item_p_filetype(struct mainvars *);
static char *create_item_p_entrynum(struct mainvars *);
static char *create_item_p_timepos(struct mainvars *);
static char *create_item_p_framepos(struct mainvars *);
static char *create_item_p_allsongnum(struct mainvars *);
static char *create_item_p_allsongtime(struct mainvars *);

static char *create_item_mix_vol(struct mainvars *);
static char *create_item_mix_sur(struct mainvars *);
static char *create_item_mix_spd(struct mainvars *);
static char *create_item_mix_bal(struct mainvars *);
static char *create_item_mix_bass(struct mainvars *);
static char *create_item_mix_treble(struct mainvars *);

static char *create_item_auc_vol(struct mainvars *);

static char *create_item_stat_replay(struct mainvars *);
static char *create_item_stat_random(struct mainvars *);
static char *create_item_stat_autovol(struct mainvars *);
static char *create_item_stat_crossfade(struct mainvars *);
static char *create_item_stat_fadeout(struct mainvars *);
static char *create_item_stat_fadein(struct mainvars *);
static char *create_item_stat_swapchan(struct mainvars *);
static char *create_item_stat_hq(struct mainvars *);
static char *create_item_stat_autopause(struct mainvars *);
static char *create_item_stat_pausenext(struct mainvars *);
static char *create_item_stat_play(struct mainvars *);

static char *create_item_currlist(struct mainvars *);
static char *create_item_currlistp(struct mainvars *);
static char *create_item_currdrive(struct mainvars *);
static char *create_item_volmeter(struct mainvars *);
static char *create_item_systime(struct mainvars *);
static char *create_item_sysdate(struct mainvars *);

static char *create_item_editorhline(struct mainvars *mvp);
static char *create_item_editorhlinep1(struct mainvars *mvp);
static char *create_item_editorhlinep2(struct mainvars *mvp);
static char *create_item_editorhlinep3(struct mainvars *mvp);
static char *create_item_editorhlinem1(struct mainvars *mvp);

static void LCD_draw(struct mainvars *);

static void LCD_hw_init(void);
static void LCD_hw_close(void);
static void LCD_hw_put_char(char);
static void LCD_hw_gotoyx(int y, int x);
static void LCD_hw_clear_display(void);
static void LCD_hw_scroll_left(unsigned int line);

//---------------------------------------------------------------------------

extern int MIXER_var_volume, MIXER_var_balance;
extern int MIXER_var_swapchan, MIXER_var_autovolume, MIXER_var_usehq;
extern unsigned int playreplay, playrand, playcontrol, timemode, id3textconv;
extern unsigned int desktopmode;

static char *biosmem;
static char *lcd_portname;
static unsigned int LCD_portnum, LCD_type_select, LCD_portdelay, lpt_postdelay;
static unsigned int LCD_pages, LCD_lines, LCD_rows, LCD_currpage;
static unsigned int LCD_refresh_delay_general, LCD_refresh_delay_scroll;
static unsigned int LCD_scroll_predelay, LCD_pagereset_delay;
static unsigned int lcd_control, lcd_itemcount, lcd_userstrcount;
static unsigned int lcd_delaycount_general, lcd_delaycount_pagereset;
static unsigned int lpt_cntrlbit_rw, lpt_cntrlbit_rs;
static unsigned int lpt_cntrlbit_e1, lpt_cntrlbit_e2;
static char strtmp[512], strart[512];

//---------------------------------------------------------------------------
#define MAX_LCD_PAGES   4
#define MAX_LCD_LINES   16
#define MAX_LCD_ROWS    512
#define MAX_LCD_ITEMNUM 128

#define LCD_SKIP_CHARS  8		// skip 8 unchanged chars (else write them to LCD)

//default delay values
#define LCD_REFRESH_DELAY_GENERAL  1
#define LCD_REFRESH_DELAY_SCROLL   4
#define LCD_SCROLL_PREDELAY       30

//item types
#define IT_STANDARD 1			// a standard item (artist,title,album,time,etc.)
#define IT_USERSTR  2			// an user defined string
#define IT_MASK     (IT_STANDARD|IT_USERSTR)

//item control types
#define ITC_ALIGN_LEFT         32	// align text left in line
#define ITC_ALIGN_CENTER       64	// align text center in line
#define ITC_ALIGN_RIGHT       128	// align text right in line
#define ITC_ALIGN_FREESEARCH  256	// search free space in any line (not implemented yet)
#define ITC_MASK   (ITC_ALIGN_LEFT|ITC_ALIGN_CENTER|ITC_ALIGN_RIGHT|ITC_ALIGN_FREESEARCH)

//line control/item control (refresh) types
#define LCT_IRT_HEADER     512	// item belongs to header
#define LCT_IRT_OPTION    1024	// item belongs to options (Mpxplay's control values)
#define LCT_IRT_EDITOR    2048	// item belongs to playlist editor
#define LCT_IRT_BROWSER   4096	// usually at playlist/dir change
#define LCT_IRT_TIMED     8192	// item requires auto-timed-refresh (controlled by LCD_refresh)
#define LCT_LINESCROLL    16384	// scroll line (special refreshmode case)
#define LCT_MASK (LCT_IRT_HEADER|LCT_IRT_OPTION|LCT_IRT_EDITOR|LCT_IRT_BROWSER|LCT_IRT_TIMED|LCT_LINESCROLL)	// all LCT types

#define LCT_PAGERESET     32768	// reset page
#define LCT_REFRESH_ONCE  65536	// refresh once (RDT_EDITOR|RDT_OPTIONS|RDT_HEADER)
#define LCT_REFRESH_MASK  (LCT_IRT_TIMED|LCT_LINESCROLL|LCT_PAGERESET|LCT_REFRESH_ONCE)	// refreshed LCT types
//#define LCT_BUILD_ITEMS   65536 // (re)build items

typedef struct {
	char *itemname;
	char *(*createitem_func) (struct mainvars *);
	unsigned int controlflags;
} mpxini_standarditem_s;

typedef struct {
	char *itemname;
	unsigned int controlflags;
} mpxini_controlitem_s;

typedef struct {
	unsigned int page;
	unsigned int ypos;
	unsigned int xpos;
	unsigned int itemselect;
	unsigned int itemcontrol;
} disp_lcditem_s;

typedef struct {
	unsigned int length;
	unsigned int lastx;
	unsigned int linecontrol;
	unsigned int scrollpos;
	unsigned int delaycount_waitscroll;
	unsigned int delaycount_scroll;
	char linestr[MAX_LCD_ROWS + 1];
} disp_lcdline_s;

typedef struct {
	void (*init) (void);
	void (*close) (void);
	void (*put_char) (char);
	void (*gotoyx) (int, int);
	void (*clear_display) (void);
	void (*hw_scroll_left) (unsigned int);
} lcd_lowlevelfunc_s;

static mpxini_var_s lcd_base_infos[] = {
	{"LCDport", &lcd_portname, ARG_CHAR | ARG_POINTER},
	{"LCDtype", &LCD_type_select, ARG_NUM},
	{"LCDrows", &LCD_rows, ARG_NUM},
	{"LCDlines", &LCD_lines, ARG_NUM},
	{"LCDportdelay", &LCD_portdelay, ARG_NUM},
	{"LCDrefresh", &LCD_refresh_delay_general, ARG_NUM},
	{"LCDscrollspeed", &LCD_refresh_delay_scroll, ARG_NUM},
	{"LCDscrolldelay", &LCD_scroll_predelay, ARG_NUM},
	{"LCDpagereset", &LCD_pagereset_delay, ARG_NUM},
	{"LPT_cntrlbit_RW", &lpt_cntrlbit_rw, ARG_NUM | ARG_HEX},
	{"LPT_cntrlbit_RS", &lpt_cntrlbit_rs, ARG_NUM | ARG_HEX},
	{"LPT_cntrlbit_E1", &lpt_cntrlbit_e1, ARG_NUM | ARG_HEX},
	{"LPT_cntrlbit_E2", &lpt_cntrlbit_e2, ARG_NUM | ARG_HEX},
	{NULL, NULL, 0}
};

static mpxini_standarditem_s mpxini_standarditems[] = {
	{"", NULL, 0},				// for easier programming
	{"artist", &create_item_artist, IT_STANDARD | LCT_IRT_EDITOR},
	{"title", &create_item_title, IT_STANDARD | LCT_IRT_EDITOR},
	{"album", &create_item_album, IT_STANDARD | LCT_IRT_EDITOR},
	{"year", &create_item_year, IT_STANDARD | LCT_IRT_EDITOR},
	{"genre", &create_item_genre, IT_STANDARD | LCT_IRT_EDITOR},
	{"comment", &create_item_comment, IT_STANDARD | LCT_IRT_EDITOR},
	{"tracknum", &create_item_tracknum, IT_STANDARD | LCT_IRT_EDITOR},
	{"filename", &create_item_filename, IT_STANDARD | LCT_IRT_EDITOR},
	{"pathname", &create_item_pathname, IT_STANDARD | LCT_IRT_EDITOR},
	{"songtime", &create_item_songtime, IT_STANDARD | LCT_IRT_EDITOR},
	{"entrynum", &create_item_entrynum, IT_STANDARD | LCT_IRT_EDITOR},
	{"allsongnum", &create_item_allsongnum, IT_STANDARD | LCT_IRT_BROWSER},	// ???
	{"allsongtime", &create_item_allsongtime, IT_STANDARD | LCT_IRT_BROWSER},	// ???

	{"p_artist", &create_item_p_artist, IT_STANDARD | LCT_IRT_HEADER},
	{"p_title", &create_item_p_title, IT_STANDARD | LCT_IRT_HEADER},
	{"p_album", &create_item_p_album, IT_STANDARD | LCT_IRT_HEADER},
	{"p_year", &create_item_p_year, IT_STANDARD | LCT_IRT_HEADER},
	{"p_genre", &create_item_p_genre, IT_STANDARD | LCT_IRT_HEADER},
	{"p_comment", &create_item_p_comment, IT_STANDARD | LCT_IRT_HEADER},
	{"p_tracknum", &create_item_p_tracknum, IT_STANDARD | LCT_IRT_HEADER},
	{"p_filename", &create_item_p_filename, IT_STANDARD | LCT_IRT_HEADER},
	{"p_pathname", &create_item_p_pathname, IT_STANDARD | LCT_IRT_HEADER},
	{"p_songtime", &create_item_p_songtime, IT_STANDARD | LCT_IRT_HEADER},
	{"p_songfreq", &create_item_p_songfreq, IT_STANDARD | LCT_IRT_HEADER},
	{"p_songchan", &create_item_p_songchan, IT_STANDARD | LCT_IRT_HEADER},
	{"p_bitrate", &create_item_p_bitrate, IT_STANDARD | LCT_IRT_HEADER},
	{"p_filetype", &create_item_p_filetype, IT_STANDARD | LCT_IRT_HEADER},
	{"p_entrynum", &create_item_p_entrynum, IT_STANDARD | LCT_IRT_HEADER},
	{"p_timepos", &create_item_p_timepos, IT_STANDARD | LCT_IRT_HEADER | LCT_IRT_TIMED},
	{"p_framepos", &create_item_p_framepos, IT_STANDARD | LCT_IRT_HEADER | LCT_IRT_TIMED},
	{"p_allsongnum", &create_item_p_allsongnum, IT_STANDARD | LCT_IRT_BROWSER},
	{"p_allsongtime", &create_item_p_allsongtime, IT_STANDARD | LCT_IRT_BROWSER},	// ??? (-ipl -idl)

	{"mix_vol", &create_item_mix_vol, IT_STANDARD | LCT_IRT_OPTION},
	{"mix_sur", &create_item_mix_sur, IT_STANDARD | LCT_IRT_OPTION},
	{"mix_spd", &create_item_mix_spd, IT_STANDARD | LCT_IRT_OPTION},
	{"mix_bal", &create_item_mix_bal, IT_STANDARD | LCT_IRT_OPTION},
	{"mix_bass", &create_item_mix_bass, IT_STANDARD | LCT_IRT_OPTION},
	{"mix_treble", &create_item_mix_treble, IT_STANDARD | LCT_IRT_OPTION},

	{"auc_vol", &create_item_auc_vol, IT_STANDARD | LCT_IRT_OPTION},

	{"s_re", &create_item_stat_replay, IT_STANDARD | LCT_IRT_OPTION},
	{"s_rn", &create_item_stat_random, IT_STANDARD | LCT_IRT_OPTION},
	{"s_av", &create_item_stat_autovol, IT_STANDARD | LCT_IRT_OPTION},
	{"s_cf", &create_item_stat_crossfade, IT_STANDARD | LCT_IRT_OPTION},
	{"s_fo", &create_item_stat_fadeout, IT_STANDARD | LCT_IRT_OPTION},
	{"s_fi", &create_item_stat_fadein, IT_STANDARD | LCT_IRT_OPTION},
	{"s_sw", &create_item_stat_swapchan, IT_STANDARD | LCT_IRT_OPTION},
	{"s_hq", &create_item_stat_hq, IT_STANDARD | LCT_IRT_OPTION},
	{"s_ap", &create_item_stat_autopause, IT_STANDARD | LCT_IRT_OPTION},
	{"s_pn", &create_item_stat_pausenext, IT_STANDARD | LCT_IRT_OPTION},
	{"s_play", &create_item_stat_play, IT_STANDARD | LCT_IRT_OPTION},

	{"currlist", &create_item_currlist, IT_STANDARD | LCT_IRT_BROWSER},	// ???
	{"currlistp", &create_item_currlistp, IT_STANDARD | LCT_IRT_BROWSER},	// ???
	{"currdrive", &create_item_currdrive, IT_STANDARD | LCT_IRT_BROWSER},	// ???
	{"volmeter", &create_item_volmeter, IT_STANDARD | LCT_IRT_TIMED},
	{"systime", &create_item_systime, IT_STANDARD | LCT_IRT_TIMED},
	{"sysdate", &create_item_sysdate, IT_STANDARD | LCT_IRT_TIMED},	// ???

	{"editorhline", &create_item_editorhline, IT_STANDARD | LCT_IRT_EDITOR},
	{"editorhlinep1", &create_item_editorhlinep1, IT_STANDARD | LCT_IRT_EDITOR},
	{"editorhlinep2", &create_item_editorhlinep2, IT_STANDARD | LCT_IRT_EDITOR},
	{"editorhlinep3", &create_item_editorhlinep3, IT_STANDARD | LCT_IRT_EDITOR},
	{"editorhlinem1", &create_item_editorhlinem1, IT_STANDARD | LCT_IRT_EDITOR}

};

static mpxini_controlitem_s mpxini_controlitems[] = {
	{"", 0},					// for easier programming
	{"a_left", ITC_ALIGN_LEFT},
	{"a_center", ITC_ALIGN_CENTER},
	{"a_right", ITC_ALIGN_RIGHT},
	{"linescroll", LCT_LINESCROLL}
};

static lcd_lowlevelfunc_s LCD_TYPE1_funcs;
static lcd_lowlevelfunc_s LCD_TYPE2_funcs;
static lcd_lowlevelfunc_s LCD_TYPE3_funcs;
static lcd_lowlevelfunc_s LCD_TYPE4_funcs;
static lcd_lowlevelfunc_s LCD_TYPE5_funcs;
static lcd_lowlevelfunc_s LCD_TYPE6_funcs;
static lcd_lowlevelfunc_s LCD_TYPE7_funcs;
static lcd_lowlevelfunc_s LCD_TYPE8_funcs;
static lcd_lowlevelfunc_s LCD_TYPE9_funcs;

static lcd_lowlevelfunc_s *lcd_all_lowlevelfuncs[] = {
	NULL,						// no type 0
	&LCD_TYPE1_funcs,
	&LCD_TYPE2_funcs,
	&LCD_TYPE3_funcs,
	&LCD_TYPE4_funcs,
	&LCD_TYPE5_funcs,
	&LCD_TYPE6_funcs,
	&LCD_TYPE7_funcs,
	&LCD_TYPE8_funcs,
	&LCD_TYPE9_funcs
};

#define LCD_STANDARDITEMTYPES (sizeof(mpxini_standarditems)/sizeof(mpxini_standarditem_s) - 1)
#define LCD_CONTROLITEMTYPES (sizeof(mpxini_controlitems)/sizeof(mpxini_controlitem_s) - 1)
#define LCD_LOWLEVELFUNCS    (sizeof(lcd_all_lowlevelfuncs)/sizeof(lcd_lowlevelfunc_s *) - 1)

static char *userstrings[MAX_LCD_ITEMNUM];
static disp_lcditem_s disp_lcditems[MAX_LCD_ITEMNUM];
static disp_lcdline_s disp_lcdlines[MAX_LCD_PAGES][MAX_LCD_LINES + 1];
static char lcd_mirror[MAX_LCD_LINES + 1][MAX_LCD_ROWS + 1];	// mirror of LCD display data

#endif							// MPXPLAY_LINK_LCD

void mpxplay_display_lcd_loadini(mpxini_line_t * mpxini_lines, struct mpxini_part_t *mpxini_partp)
{
#ifdef MPXPLAY_LINK_LCD
	unsigned int biosaddr;

	LCD_refresh_delay_general = LCD_REFRESH_DELAY_GENERAL;
	LCD_refresh_delay_scroll = LCD_REFRESH_DELAY_SCROLL;
	LCD_scroll_predelay = LCD_SCROLL_PREDELAY;

	mpxplay_control_general_loadini(mpxini_lines, mpxini_partp, lcd_base_infos);

	if(!lcd_portname)
		return;

	if(pds_strlicmp(lcd_portname, "LPT") == 0)
		biosaddr = 0x408;
	else if(pds_strlicmp(lcd_portname, "COM") == 0)
		biosaddr = 0x400;
	else
		return;

	LCD_portnum = pds_atol(&lcd_portname[3]);	// LPTn or COMn
	if(LCD_portnum < 1 || LCD_portnum > 4) {
		LCD_portnum = 0;
		display_warning_message("Invalid LCD-port number (must be 1-4)!");
		return;
	}
#ifdef WIN32
	LCD_portnum = 0x378;
#else
	LCD_portnum = *((unsigned short *)&biosmem[biosaddr + (LCD_portnum - 1) * 2]);
#endif
	if(!LCD_portnum) {
		display_warning_message("Warning: Couldn't initialize LCD-port! (Is it enabled in bios?)");
		return;
	}

	if(!LCD_type_select || LCD_type_select > LCD_LOWLEVELFUNCS) {
		display_warning_message("Warning: Invalid LCD type!");
		LCD_portnum = 0;
		return;
	}

	if(!LCD_rows || !LCD_lines) {
		LCD_portnum = 0;
		return;
	}

	if(!lcd_all_lowlevelfuncs[LCD_type_select]->init || !lcd_all_lowlevelfuncs[LCD_type_select]->put_char || !lcd_all_lowlevelfuncs[LCD_type_select]->gotoyx) {
		display_warning_message("Warning: Bad (missing) low level LCD routines!");
		LCD_portnum = 0;
		return;
	}

	if(LCD_lines > MAX_LCD_LINES)
		LCD_lines = MAX_LCD_LINES;
	if(LCD_rows > MAX_LCD_ROWS)
		LCD_rows = MAX_LCD_ROWS;

	convert_mpxini_to_lcditems(mpxini_lines, mpxini_partp);

	if(!lcd_itemcount) {
		display_warning_message("Warning: LCD_items line is missing or invalid (no LCD-items)!");
		LCD_portnum = 0;
		return;
	}
#endif							// MPXPLAY_LINK_LCD
}

#ifdef MPXPLAY_LINK_LCD

static void convert_mpxini_to_lcditems(mpxini_line_t * mpxini_lines, struct mpxini_part_t *mpxini_partp)
{
	unsigned int i, j, argument_count, validitem, itemcontrol = IT_STANDARD;
	unsigned int page, ypos, xpos;
	char *itemp, *nextp;
	mpxini_line_t *linep = mpxini_lines;

	linep += mpxini_partp->partbegin_linenum;

	for(j = 0; (j < mpxini_partp->partlinenum) && (lcd_itemcount < MAX_LCD_ITEMNUM); j++, linep++) {
		if(linep->varnamep) {
			if(pds_strlicmp(linep->varnamep, "LCD_items") == 0) {

				page = (unsigned int)*(linep->varnamep + sizeof("LCD_items") - 1);
				if(page >= '0' && page <= '9') {
					page -= '0';
					if(page >= MAX_LCD_PAGES)
						page = 0;
				} else
					page = 0;
				LCD_pages = max(page, LCD_pages);

				itemp = linep->valuep;	// line configuration string (ypos,xpos,item1,item2...)
				argument_count = 0;
				xpos = ypos = 0;
				do {
					nextp = pds_strchr(itemp, ',');	// search next ',' (end of item)
					if(nextp) {
						if(nextp == itemp) {	// no string between ',' chars
							itemp = nextp + 1;
							continue;
						}
						*nextp++ = 0;
					}
					if(!pds_strcutspc(itemp))
						continue;
					validitem = 0;
					for(i = 1; i <= LCD_STANDARDITEMTYPES; i++) {
						if(pds_stricmp(mpxini_standarditems[i].itemname, itemp) == 0) {
							validitem = i;
							break;
						}
					}
					if(validitem) {	// string found in mpxini_standarditems[]
						disp_lcditems[lcd_itemcount].itemselect = validitem;
						disp_lcditems[lcd_itemcount].itemcontrol = IT_STANDARD | itemcontrol;
						disp_lcditems[lcd_itemcount].page = page;
						disp_lcditems[lcd_itemcount].xpos = xpos;
						disp_lcditems[lcd_itemcount].ypos = ypos;
						lcd_itemcount++;
						if(ypos)
							funcbit_enable(disp_lcdlines[page][ypos].linecontrol, (mpxini_standarditems[validitem].controlflags & LCT_MASK));	// control refresh types
						funcbit_enable(lcd_control, (mpxini_standarditems[validitem].controlflags & LCT_MASK));
						xpos = 0;
						itemcontrol = 0;
					} else {
						for(i = 1; i <= LCD_CONTROLITEMTYPES; i++) {
							if(pds_stricmp(mpxini_controlitems[i].itemname, itemp) == 0) {
								validitem = i;
								break;
							}
						}
						if(validitem) {	// string found in mpxini_controlitems[]
							unsigned int ctflag = mpxini_controlitems[validitem].controlflags;
							if(ctflag & ITC_MASK) {	// item control
								funcbit_enable(itemcontrol, ctflag);	// the next standard item will use this
							}
							if((ctflag & LCT_MASK) && ypos) {	// lcd (line) control
								funcbit_enable(disp_lcdlines[page][ypos].linecontrol, ctflag);
								funcbit_enable(lcd_control, ctflag);
							}
						} else {
							switch (argument_count) {
							case 0:
								ypos = pds_atol(itemp);	// 0. argument may be an y-pos (line number)
								if(ypos > MAX_LCD_LINES)
									ypos = MAX_LCD_LINES;
								break;
							case 1:
								xpos = pds_atol(itemp);	// 1. argument may be an x-pos (row number)(or an align)
								if(xpos > MAX_LCD_ROWS)
									xpos = 0;
								break;
							default:
								if(*itemp == '\"') {	// is this an "user defined string" ?
									char *endp;
									itemp++;
									endp = pds_strchr(itemp, '\"');
									if(!endp)
										break;
									*endp = 0;
									userstrings[lcd_userstrcount] = itemp;
									disp_lcditems[lcd_itemcount].itemselect = lcd_userstrcount;
									disp_lcditems[lcd_itemcount].itemcontrol = IT_USERSTR | itemcontrol;
									disp_lcditems[lcd_itemcount].page = page;
									disp_lcditems[lcd_itemcount].xpos = xpos;
									disp_lcditems[lcd_itemcount].ypos = ypos;
									lcd_userstrcount++;
									lcd_itemcount++;
									xpos = 0;
									itemcontrol = 0;
								}
								break;
							}
						}
					}
					itemp = nextp;
					argument_count++;
				} while(nextp && (lcd_itemcount < MAX_LCD_ITEMNUM));
			}
		}
	}
}
#endif							// MPXPLAY_LINK_LCD

void mpxplay_display_lcd_init(void)
{
#ifdef MPXPLAY_LINK_LCD
	if(LCD_portnum)
		LCD_hw_init();
#endif
}

void mpxplay_display_lcd_close(void)
{
	char *close_str = "MPXPLAY EXIT";
	int i, len;
#ifdef MPXPLAY_LINK_LCD
	if(LCD_portnum) {
		LCD_hw_clear_display();
		LCD_hw_gotoyx(0, 0);
		len = strlen(close_str);
		for(i = 0; i < len; i++)
			LCD_hw_put_char(close_str[i]);
		LCD_hw_close();
	}
#endif
}

//**************************************************************************
//create display (standard text) items
//**************************************************************************

#ifdef MPXPLAY_LINK_LCD

static char *create_artist(struct playlist_entry_info *pei)
{
	char *itemp = pei->id3info[I3I_ARTIST];
	unsigned int len;
	// if no artist & title info or entry is a drive/dir/playlist then show filename
	if((desktopmode & DTM_EDIT_FILENAMES) || (!itemp && (!pei->id3info[I3I_TITLE] || GET_HFT(pei->entrytype) == HFT_DFT))) {
		if(!(id3textconv & ID3TEXTCONV_FILENAME) && (id3textconv & (ID3TEXTCONV_CODEPAGE | ID3TEXTCONV_UTF_AUTO))) {
			itemp = &strart[0];
			len = pds_strcpy(itemp, pds_getfilename_from_fullname(pei->filename));
			mpxplay_playlist_textconv_selected_do(itemp, len, (id3textconv | ID3TEXTCONV_UTF8), 0);
		} else
			itemp = pds_getfilename_from_fullname(pei->filename);
	}
	return itemp;
}

static char *create_item_artist(struct mainvars *mvp)
{
	return create_artist(mvp->psie->editorhighline);
}

static char *create_item_title(struct mainvars *mvp)
{
	return mvp->psie->editorhighline->id3info[I3I_TITLE];
}

static char *create_item_album(struct mainvars *mvp)
{
	return mvp->psie->editorhighline->id3info[I3I_ALBUM];
}

static char *create_item_year(struct mainvars *mvp)
{
	return mvp->psie->editorhighline->id3info[I3I_YEAR];
}

static char *create_item_genre(struct mainvars *mvp)
{
	return mvp->psie->editorhighline->id3info[I3I_GENRE];
}

static char *create_item_comment(struct mainvars *mvp)
{
	return mvp->psie->editorhighline->id3info[I3I_COMMENT];
}

static char *create_item_tracknum(struct mainvars *mvp)
{
	return mvp->psie->editorhighline->id3info[I3I_TRACKNUM];
}

static char *create_item_filename(struct mainvars *mvp)
{
	struct playlist_entry_info *pei = mvp->psie->editorhighline;
	if((pei->entrytype & (DFTM_DFT | DFTM_DRIVE)) == (DFTM_DFT | DFTM_DRIVE))
		return pei->filename;
	return pds_getfilename_from_fullname(pei->filename);
}

static char *create_item_pathname(struct mainvars *mvp)
{
	pds_getpath_from_fullname(&strtmp[0], mvp->psie->editorhighline->filename);
	return (&strtmp[0]);
}

static char *create_item_songtime(struct mainvars *mvp)
{
	struct playlist_entry_info *pei = mvp->psie->editorhighline;
	unsigned int timesec;
	if(pei->infobits & PEIF_ENABLED)
		timesec = (playlist_entry_get_timemsec(pei) + 500) / 1000;
	else
		timesec = 0;
	sprintf(strtmp, "%d:%2.2d", timesec / 60, timesec % 60);
	return (&strtmp[0]);
}

static char *create_item_entrynum(struct mainvars *mvp)
{
	struct playlist_side_info *psi = mvp->psie;
	struct playlist_entry_info *pei = psi->editorhighline;
	sprintf(strtmp, "%d", (pei >= psi->firstentry) ? (pei - psi->firstentry + 1) : 0);
	return (&strtmp[0]);
}

static char *create_item_allsongnum(struct mainvars *mvp)
{
	struct playlist_side_info *psi = mvp->psie;
	sprintf(strtmp, "%d", (psi->lastentry >= psi->firstsong) ? (psi->lastentry - psi->firstsong + 1) : 0);
	return (&strtmp[0]);
}

static char *create_item_allsongtime(struct mainvars *mvp)
{
	unsigned int timesec = mvp->psie->fulltimesec;
	sprintf(strtmp, "%d:%2.2d:%2.2d", timesec / 3600, (timesec / 60) % 60, timesec % 60);
	return (&strtmp[0]);
}

//-----------------------------------------------------------------------

static char *create_item_p_artist(struct mainvars *mvp)
{
	return create_artist(mvp->pei0);
}

static char *create_item_p_title(struct mainvars *mvp)
{
	return mvp->pei0->id3info[I3I_TITLE];
}

static char *create_item_p_album(struct mainvars *mvp)
{
	return mvp->pei0->id3info[I3I_ALBUM];
}

static char *create_item_p_year(struct mainvars *mvp)
{
	return mvp->pei0->id3info[I3I_YEAR];
}

static char *create_item_p_genre(struct mainvars *mvp)
{
	return mvp->pei0->id3info[I3I_GENRE];
}

static char *create_item_p_comment(struct mainvars *mvp)
{
	return mvp->pei0->id3info[I3I_COMMENT];
}

static char *create_item_p_tracknum(struct mainvars *mvp)
{
	return mvp->pei0->id3info[I3I_TRACKNUM];
}

static char *create_item_p_filename(struct mainvars *mvp)
{
	return pds_getfilename_from_fullname(mvp->pei0->filename);
}

static char *create_item_p_pathname(struct mainvars *mvp)
{
	pds_getpath_from_fullname(&strtmp[0], mvp->pei0->filename);
	if(pds_strlen(strtmp) > LCD_rows) {
		int i = pds_strlen(strtmp);
		pds_memcpy(strtmp, strtmp + i - LCD_rows, LCD_rows);
		strtmp[LCD_rows] = 0;
	}
	return (&strtmp[0]);
}

static char *create_item_p_songtime(struct mainvars *mvp)
{
	unsigned long timesec = mvp->frp0->timesec;
	sprintf(strtmp, "%d:%2.2d", timesec / 60, timesec % 60);
	return (&strtmp[0]);
}

static char *create_item_p_songfreq(struct mainvars *mvp)
{
	sprintf(strtmp, "%d", (unsigned int)mvp->frp0->infile_infos->audio_decoder_infos->freq);
	return (&strtmp[0]);
}

static char *create_item_p_songchan(struct mainvars *mvp)
{
	sprintf(strtmp, "%d", (unsigned int)mvp->frp0->infile_infos->audio_decoder_infos->filechannels);
	return (&strtmp[0]);
}

static char *create_item_p_bitrate(struct mainvars *mvp)
{
	mpxplay_audio_decoder_info_s *adi = mvp->frp0->infile_infos->audio_decoder_infos;
	if(adi->bitratetext)
		return adi->bitratetext;
	if(adi->bitrate)
		sprintf(strtmp, "%d kbit", (unsigned int)adi->bitrate);
	else
		sprintf(strtmp, "%d bit", (unsigned int)adi->bits);
	return (&strtmp[0]);
}

static char *create_item_p_filetype(struct mainvars *mvp)
{
	struct mpxplay_infile_info_s *miis = mvp->frp0->infile_infos;
	mpxplay_audio_decoder_info_s *adi;
	char *ext;

	if(miis->longname)
		return miis->longname;
	adi = miis->audio_decoder_infos;
	if(mvp->frp0->infile_funcs && mvp->frp0->infile_funcs->file_extensions[0])
		ext = mvp->frp0->infile_funcs->file_extensions[0];
	else {
		ext = pds_strrchr(mvp->pei0->filename, '.');
		if(ext)
			ext++;
	}
	if(ext || adi->shortname)
		sprintf(strtmp, "%3.3s->%3.3s", ((ext) ? ext : "???"), (adi->shortname ? adi->shortname : "???"));
	else
		pds_strcpy(strtmp, "  ----  ");
	return (&strtmp[0]);
}

static char *create_item_p_entrynum(struct mainvars *mvp)
{
	struct playlist_side_info *psi = mvp->psip;
	sprintf(strtmp, "%d", (mvp->aktfilenum >= psi->firstsong) ? (mvp->aktfilenum - psi->firstsong + 1) : 0);
	return (&strtmp[0]);
}

static char *create_item_p_timepos(struct mainvars *mvp)
{
	long cframe, ctime, index_pos;
	struct frame *frp = mvp->frp0;

	index_pos = frp->frameNum - frp->index_start;
	ctime = 0;
	switch (timemode) {
	case 0:
		cframe = index_pos;
		break;
	case 1:
		cframe = frp->index_len - index_pos;
		break;
	case 2:
		cframe = index_pos;
		ctime = playlist_fulltime_getelapsed(mvp, 0);
		break;
	case 3:
		cframe = frp->index_len - index_pos;
		if(mvp->psip->fulltimesec)
			ctime = mvp->psip->fulltimesec - playlist_fulltime_getelapsed(mvp, 0) - frp->timesec;
		break;
	default:
		cframe = index_pos;
	}
	if(cframe < 0)
		cframe = 0;

	ctime += (long)((float)cframe * (float)frp->timesec / (float)frp->index_len);	// float needed to avoid integer overflow at huge files

	if(ctime < 3600)			// < 1 hour
		sprintf(strtmp, "%d:%2.2d", ctime / 60, ctime % 60);	// m:ss
	else
		sprintf(strtmp, "%d:%2.2d:%2.2d", ctime / 3600, (ctime / 60) % 60, ctime % 60);	// h:mm:ss
	return (&strtmp[0]);
}

static char *create_item_p_framepos(struct mainvars *mvp)
{
	long cframe, index_pos;
	struct frame *frp = mvp->frp0;

	index_pos = frp->frameNum - frp->index_start;

	switch (timemode & 2) {
	case 0:
		cframe = index_pos;
		break;
	case 1:
		cframe = frp->index_len - index_pos;
		if(cframe < 0)
			cframe = 0;
		break;
	}

	sprintf(strtmp, "%4d", cframe);
	return (&strtmp[0]);
}

static char *create_item_p_allsongnum(struct mainvars *mvp)
{
	struct playlist_side_info *psi = mvp->psip;
	sprintf(strtmp, "%d", (psi->lastentry >= psi->firstsong) ? (psi->lastentry - psi->firstsong + 1) : 0);
	return (&strtmp[0]);
}

static char *create_item_p_allsongtime(struct mainvars *mvp)
{
	unsigned int timesec = mvp->psip->fulltimesec;
	sprintf(strtmp, "%d:%2.2d:%2.2d", timesec / 3600, (timesec / 60) % 60, timesec % 60);
	return (&strtmp[0]);
}

//-------------------------------------------------------------------------

static char *create_item_mix_vol(struct mainvars *mvp)
{
	sprintf(strtmp, "%3d", MIXER_var_volume);
	return (&strtmp[0]);
}

static char *create_item_mix_sur(struct mainvars *mvp)
{
	sprintf(strtmp, "%3d", MIXER_getvalue("MIX_SURROUND"));
	return (&strtmp[0]);
}

static char *create_item_mix_spd(struct mainvars *mvp)
{
	sprintf(strtmp, "%3d", MIXER_getvalue("MIX_SPEED"));
	return (&strtmp[0]);
}

static char *create_item_mix_bal(struct mainvars *mvp)
{
	if(MIXER_var_balance != 0) {
		sprintf(strtmp, "%+2d ", MIXER_var_balance);
		strtmp[3] = 0;
	} else
		pds_strcpy(strtmp, " 0 ");
	return (&strtmp[0]);
}

static char *create_item_mix_bass(struct mainvars *mvp)
{
	sprintf(strtmp, "%3d", mvp->aui->card_mixer_values[AU_MIXCHAN_BASS]);
	return (&strtmp[0]);
}

static char *create_item_mix_treble(struct mainvars *mvp)
{
	sprintf(strtmp, "%3d", mvp->aui->card_mixer_values[AU_MIXCHAN_TREBLE]);
	return (&strtmp[0]);
}

static char *create_item_auc_vol(struct mainvars *mvp)
{
	sprintf(strtmp, "%3d", mvp->aui->card_mixer_values[AU_MIXCHAN_MASTER]);
	return (&strtmp[0]);
}

//-------------------------------------------------------------------------
typedef struct one_ed_text {
	char *disabled;
	char *enabled;
} one_ed_text;

static struct {
	one_ed_text replay;
	one_ed_text random;
	one_ed_text autovol;
	one_ed_text crossfade;
	one_ed_text fadeout;
	one_ed_text fadein;
	one_ed_text swapchan;
	one_ed_text hq;
	one_ed_text autopause;
	one_ed_text pausenext;
} all_ed_text = {
	{
	"re", "RE"}, {
	"rn", "RN"}, {
	"av", "AV"}, {
	"cf", "CF"}, {
	"fo", "FO"}, {
	"fi", "FI"}, {
	"sw", "SW"}, {
	"hq", "HQ"}, {
	"ap", "AP"}, {
	"pn", "PN"}
};

static char *create_item_stat_replay(struct mainvars *mvp)
{
	static char *r1 = "R1";
	if(playreplay & REPLAY_SONG)
		return r1;
	return ((playreplay) ? all_ed_text.replay.enabled : all_ed_text.replay.disabled);
}

static char *create_item_stat_random(struct mainvars *mvp)
{
	return ((playrand) ? all_ed_text.random.enabled : all_ed_text.random.disabled);
}

static char *create_item_stat_autovol(struct mainvars *mvp)
{
	return ((MIXER_var_autovolume) ? all_ed_text.autovol.enabled : all_ed_text.autovol.disabled);
}

static char *create_item_stat_crossfade(struct mainvars *mvp)
{
	return ((mvp->cfi->usecrossfade) ? all_ed_text.crossfade.enabled : all_ed_text.crossfade.disabled);
}

static char *create_item_stat_fadeout(struct mainvars *mvp)
{
	return ((mvp->cfi->crossfadetype & CFT_FADEOUT) ? all_ed_text.fadeout.enabled : all_ed_text.fadeout.disabled);
}

static char *create_item_stat_fadein(struct mainvars *mvp)
{
	return ((mvp->cfi->crossfadetype & CFT_FADEIN) ? all_ed_text.fadein.enabled : all_ed_text.fadein.disabled);
}

static char *create_item_stat_swapchan(struct mainvars *mvp)
{
	return ((MIXER_var_swapchan) ? all_ed_text.swapchan.enabled : all_ed_text.swapchan.disabled);
}

static char *create_item_stat_hq(struct mainvars *mvp)
{
	return ((MIXER_var_usehq) ? all_ed_text.hq.enabled : all_ed_text.hq.disabled);
}

static char *create_item_stat_autopause(struct mainvars *mvp)
{
	return ((playcontrol & PLAYC_PAUSEALL) ? all_ed_text.autopause.enabled : all_ed_text.autopause.disabled);
}

static char *create_item_stat_pausenext(struct mainvars *mvp)
{
	return ((playcontrol & PLAYC_PAUSENEXT) ? all_ed_text.pausenext.enabled : all_ed_text.pausenext.disabled);
}

static char *create_item_stat_play(struct mainvars *mvp)
{
	unsigned int color;
	return (get_playstatus_string(&color));
}

//------------------------------------------------------------------------
static char *create_item_currlist(struct mainvars *mvp)
{
	return pds_getfilename_from_fullname(playlist_loadsub_getinputfile(mvp->psil));
}

static char *create_item_currlistp(struct mainvars *mvp)
{
	pds_getpath_from_fullname(&strtmp[0], playlist_loadsub_getinputfile(mvp->psil));
	return (&strtmp[0]);
}

static char *create_item_currdrive(struct mainvars *mvp)
{
	strtmp[0] = mvp->psie->currdrive + 'A';
	strtmp[1] = 0;
	return (&strtmp[0]);
}

/*#define todB(x) ((x)? (long)(20.0*log10((float)(x))) : 0)

static char *create_item_volmeter(struct mainvars *mvp)
{
 int i,ch,vol,outstep,len;
 int begin[2],step[2];
 char *strpos;

 len=LCD_rows/2;
 begin[0]=len-2;
 begin[1]=len+1;
 step[0]=-1;
 step[1]=+1;

 for(ch=0;ch<2;ch++){
  vol=todB(mvp->vds->soundvolumes[ch]/4);
  //fprintf(stdout,"%6d %2d \n",mvp->vds->soundvolumes[ch]/4,vol);
  strpos=&strtmp[begin[ch]];
  outstep=step[ch];
  for(i=80;i<104;i+=(24/len)){
   if(vol>(i+12/len))
    *strpos=(ch)? ')':'(';
    //*strpos=(ch)? '>':'<';
    //*strpos='=';
   else
    *strpos=' ';
   strpos+=outstep;
  }
 }
 strtmp[LCD_rows]=0;
 strtmp[len-1]=strtmp[len]='-';
 return (&strtmp[0]);
}*/

static char *create_item_volmeter(struct mainvars *mvp)
{
	int i, ch, vol, outstep, len;
	int begin[2], step[2];
	char *strpos;

	len = LCD_rows / 2;
	begin[0] = len - 2;
	begin[1] = len + 1;
	step[0] = -1;
	step[1] = +1;

	for(ch = 0; ch < 2; ch++) {
		vol = mvp->vds->soundvolumes[ch] / 4;
		strpos = &strtmp[begin[ch]];
		outstep = step[ch];
		for(i = 0; i < 70000; i += (70000 / len)) {
			if(vol > (i + 5000 / len))
				*strpos = (ch) ? ')' : '(';
			//*strpos=(ch)? '>':'<';
			//*strpos='=';
			else
				*strpos = ' ';
			strpos += outstep;
		}
	}
	strtmp[LCD_rows] = 0;
	strtmp[len - 1] = strtmp[len] = '-';
	return (&strtmp[0]);
}

static char *create_item_systime(struct mainvars *mvp)
{
	unsigned long timeval = pds_gettime();
	sprintf(&strtmp[0], "%2d:%2.2d:%2.2d", timeval >> 16, (timeval >> 8) & 0xff, timeval & 0xff);
	return (&strtmp[0]);
}

static char *create_item_sysdate(struct mainvars *mvp)
{
	unsigned long dateval = pds_getdate();
	sprintf(&strtmp[0], "%4d-%2.2d-%2.2d", dateval >> 16, (dateval >> 8) & 0xff, dateval & 0xff);
	return (&strtmp[0]);
}

//-----------------------------------------------------------------------
static char *create_editorline(struct playlist_side_info *psi, struct playlist_entry_info *pei)
{
	if(pei >= psi->firstentry && pei <= psi->lastentry) {
		unsigned int entrynum = (pei < psi->firstsong) ? 0 : (pei - psi->firstsong + 1);
		if(entrynum)
			sprintf(strtmp, "%d. ", entrynum);
		else
			strtmp[0] = 0;
		sprintf(strtmp, "%s%.100s", strtmp, create_artist(pei));
		if(!(desktopmode & DTM_EDIT_FILENAMES) && pei->id3info[I3I_TITLE]) {
			pds_strcat(strtmp, (GET_HFT(pei->entrytype) == HFT_DFT) ? " " : ":");
			sprintf(strtmp, "%s%.100s", strtmp, pei->id3info[I3I_TITLE]);
		}
		if(pei->infobits & PEIF_ENABLED) {
			unsigned long timesec = (playlist_entry_get_timemsec(pei) + 500) / 1000;
			sprintf(strtmp, "%s (%d:%2.2d)", strtmp, timesec / 60, timesec % 60);
		}
		return (&strtmp[0]);
	}
	return NULL;
}

static char *create_item_editorhline(struct mainvars *mvp)
{
	return create_editorline(mvp->psie, mvp->psie->editorhighline);
}

static char *create_item_editorhlinep1(struct mainvars *mvp)
{
	return create_editorline(mvp->psie, mvp->psie->editorhighline + 1);
}

static char *create_item_editorhlinep2(struct mainvars *mvp)
{
	return create_editorline(mvp->psie, mvp->psie->editorhighline + 2);
}

static char *create_item_editorhlinep3(struct mainvars *mvp)
{
	return create_editorline(mvp->psie, mvp->psie->editorhighline + 3);
}

static char *create_item_editorhlinem1(struct mainvars *mvp)
{
	return create_editorline(mvp->psie, mvp->psie->editorhighline - 1);
}

//**************************************************************************
//**************************************************************************

static void clear_lcdlines(void)
{
	unsigned int p = LCD_currpage, y;
	for(y = 1; y <= LCD_lines; y++) {
		pds_memset(&(disp_lcdlines[p][y].linestr[0]), 32, MAX_LCD_ROWS);
		disp_lcdlines[p][y].linestr[MAX_LCD_ROWS] = 0;
		disp_lcdlines[p][y].length = LCD_rows;	// else division by zero
		disp_lcdlines[p][y].lastx = 0;
	}
}

static unsigned int add_item_to_lcdline(char *itemp, unsigned int p, unsigned int y, int x, unsigned int itemcontrol)
{
	char *dispp;
	unsigned int len;
	if(itemp) {
		len = pds_strlen(itemp);
		if(len) {
			switch (itemcontrol & ITC_MASK) {
			case ITC_ALIGN_LEFT:
				x = 1;
				break;
			case ITC_ALIGN_CENTER:
				x = LCD_rows / 2 - len / 2;
				break;
			case ITC_ALIGN_RIGHT:
				x = LCD_rows - len + 1;
				break;
			}
			if(x < 1)
				x = 1;
			dispp = &(disp_lcdlines[p][y].linestr[x - 1]);
			x += len;
			if(x >= MAX_LCD_ROWS) {	// to avoid disp_lcdlines.linestr[] overflow
				len -= x - MAX_LCD_ROWS;
				x = MAX_LCD_ROWS;
			}
			pds_strncpy(dispp, itemp, len);
		}
	}
	return x;
}

static void build_lcdlines_from_lcditems(struct mainvars *mvp)
{
	int x;
	unsigned int i, p, y, lasty = 1, itemselect;
	char *itemp;

	clear_lcdlines();

	for(i = 0; i < lcd_itemcount; i++) {
		p = disp_lcditems[i].page;
		if(p != LCD_currpage)
			continue;
		y = disp_lcditems[i].ypos;
		x = disp_lcditems[i].xpos;
		if(!y)
			y = lasty;
		if(!x)
			x = disp_lcdlines[p][y].lastx;
		if(!x)
			x = 1;
		itemselect = disp_lcditems[i].itemselect;
		switch (disp_lcditems[i].itemcontrol & IT_MASK) {
		case IT_STANDARD:
			itemp = mpxini_standarditems[itemselect].createitem_func(mvp);
			x = add_item_to_lcdline(itemp, p, y, x, disp_lcditems[i].itemcontrol);
			break;
		case IT_USERSTR:
			x = add_item_to_lcdline(userstrings[itemselect], p, y, x, disp_lcditems[i].itemcontrol);
			break;
		}
		lasty = y;
		disp_lcdlines[p][y].lastx = x;
		disp_lcdlines[p][y].length = max(disp_lcdlines[p][y].length, x - 1);
	}
}

//------------------------------------------------------------------------

static void LCD_draw(struct mainvars *mvp)
{
	unsigned int y, x, skippedchars;
	//unsigned int outchars=0;
	if(!LCD_portnum)
		return;

	build_lcdlines_from_lcditems(mvp);

	// partial refresh (send only the new/changed chars to LCD to reduce communication (speed up))
	for(y = 1; y <= LCD_lines; y++) {
		unsigned int linelen = disp_lcdlines[LCD_currpage][y].length;	// length of line
		unsigned int linepos = disp_lcdlines[LCD_currpage][y].scrollpos;	// display position of line (at scroll)
		char *linesp = &(disp_lcdlines[LCD_currpage][y].linestr[0]);	// begin of linestring
		char *lcdmp = &lcd_mirror[y][0];

		skippedchars = MAX_LCD_ROWS;	// we have to call an LCD_hw_gotoyx before the 1st new char
		for(x = 0; x < LCD_rows; x++) {
			unsigned int lpx = linepos + x;
			char linechar;
			if(lpx >= linelen)
				lpx -= linelen;
			linechar = linesp[lpx];
			if(linechar != *lcdmp) {
				if(skippedchars) {
					if(skippedchars >= LCD_SKIP_CHARS) {
						LCD_hw_gotoyx(y, x + 1);	// skip the unchanged chars (if we can skip a lot or it's the 1st new char in this line)
						skippedchars = 0;
					} else {
						do {
							lpx = linepos + x - skippedchars;
							if(lpx >= linelen)
								lpx -= linelen;
							LCD_hw_put_char(linesp[lpx]);	// write the unchanged chars too
							//outchars++;
						} while(--skippedchars);
					}
				}
				LCD_hw_put_char(linechar);	// display data changed on y,x position, we send it to LCD
				//outchars++;
				*lcdmp = linechar;	// we save the new display information into the mirror too
			} else {
				skippedchars++;	// display data in x,y is the same
			}
			lcdmp++;
		}
	}

	//for testing
	//display_message(0,0,lcd_mirror[1]);
	//display_message(1,0,lcd_mirror[2]);
	//sprintf(strtmp,"outchars:%d",outchars);
	//display_message(1,0,strtmp);
	//pds_textdisplay_printf(lcd_mirror[1]);
	//pds_textdisplay_printf(lcd_mirror[2]);
	//pds_textdisplay_textxy(7,50,3,lcd_mirror[1]);
	//pds_textdisplay_textxy(7,50,4,lcd_mirror[2]);
}

#endif							// MPXPLAY_LINK_LCD

void LCD_refresh_once(unsigned int refresh_type)
{
#ifdef MPXPLAY_LINK_LCD
	if(LCD_portnum) {
		disp_lcdline_s *linep = &disp_lcdlines[LCD_currpage][1];
		unsigned int y;
		for(y = 1; y <= LCD_lines; y++) {
			if((refresh_type & RDT_HEADER) && (linep->linecontrol & LCT_IRT_HEADER)) {
				if(linep->linecontrol & LCT_LINESCROLL) {
					linep->delaycount_waitscroll = 0;
					linep->scrollpos = 0;
				}
				//funcbit_enable(linep->linecontrol,LCT_BUILD_ITEMS);
			}
			if((refresh_type & (RDT_OPTIONS | RDT_VOL)) && (linep->linecontrol & LCT_IRT_OPTION)) {
				//we don't stop the scroll in this case
				lcd_delaycount_pagereset = 0;	// do not reset page if there's desktop activity
				//funcbit_enable(linep->linecontrol,LCT_BUILD_ITEMS);
			}
			if((refresh_type & RDT_EDITOR) && (linep->linecontrol & LCT_IRT_EDITOR)) {
				if(linep->linecontrol & LCT_LINESCROLL) {
					linep->delaycount_waitscroll = 0;
					linep->scrollpos = 0;
				}
				lcd_delaycount_pagereset = 0;	// do not reset page if there's desktop activity
				//funcbit_enable(linep->linecontrol,LCT_BUILD_ITEMS);
			}
			/*if((refresh_type&(RDT_BROWSER)) && (linep->linecontrol&LCT_IRT_BROWSER)){
			   if(linep->linecontrol&LCT_LINESCROLL){
			   linep->delaycount_waitscroll=0;
			   linep->scrollpos=0;
			   }
			   //funcbit_enable(linep->linecontrol,LCT_BUILD_ITEMS);
			   } */
			linep++;
		}
		funcbit_enable(lcd_control, LCT_REFRESH_ONCE);
	}
#endif
}

#ifdef MPXPLAY_LINK_LCD
static void LCD_linescroll_stop(void)
{
	unsigned int y;
	disp_lcdline_s *linep = &disp_lcdlines[LCD_currpage][1];
	for(y = 1; y <= LCD_lines; y++, linep++) {
		if(linep->linecontrol & LCT_LINESCROLL) {
			linep->delaycount_waitscroll = 0;
			linep->scrollpos = 0;
		}
	}
}
#endif

void LCD_refresh_timer(struct mainvars *mvp)
{
#ifdef MPXPLAY_LINK_LCD
	disp_lcdline_s *linep;
	unsigned int y, refresh;
	int firstdcount_scroll;

	if(!(lcd_control & LCT_REFRESH_MASK))
		return;

	refresh = 0;

	firstdcount_scroll = -1;
	linep = &disp_lcdlines[LCD_currpage][1];
	for(y = 1; y <= LCD_lines; y++) {	// check every lcd-lines separated
		if(linep->linecontrol & LCT_LINESCROLL) {
			if(linep->delaycount_waitscroll >= LCD_scroll_predelay) {	// scroll pre-delay
				if(++(linep->delaycount_scroll) >= LCD_refresh_delay_scroll) {
					if(linep->length > LCD_rows) {	// scroll if the line is longer than the LCD_rows
						if(++linep->scrollpos >= linep->length)	// rewind scroll
							linep->scrollpos = 0;
						LCD_hw_scroll_left(y);	// if possible
						refresh = 1;
					} else
						linep->scrollpos = 0;	// reset scrollpos (if no scrolling)
					if(firstdcount_scroll >= 0)
						linep->delaycount_scroll = firstdcount_scroll;	// syncronize scrolling to the first scrolled line
					else
						linep->delaycount_scroll = 0;
				}
				firstdcount_scroll = linep->delaycount_scroll;	// sync of first scrolled line
			} else {
				linep->delaycount_waitscroll++;
			}
		}
		linep++;
	}

	if(lcd_control & (LCT_IRT_TIMED | LCT_REFRESH_ONCE)) {
		if(++lcd_delaycount_general > LCD_refresh_delay_general) {
			lcd_delaycount_general = 0;
			funcbit_disable(lcd_control, LCT_REFRESH_ONCE);
			refresh = 1;
		}
	}
	if(lcd_control & LCT_PAGERESET) {
		if(++lcd_delaycount_pagereset > LCD_pagereset_delay) {
			LCD_currpage = 0;
			lcd_delaycount_pagereset = 0;
			funcbit_disable(lcd_control, LCT_PAGERESET);
			LCD_linescroll_stop();
			refresh = 1;
		}
	}

	if(refresh)
		LCD_draw(mvp);
#endif
}

void LCD_page_select(struct mainvars *mvp, int select)
{
#ifdef MPXPLAY_LINK_LCD
	if(LCD_portnum) {
		LCD_hw_init();			//reset for cold chip fix
		if(select < 0) {		// skip 1 page
			if(++LCD_currpage > LCD_pages)
				LCD_currpage = 0;
		} else {				// select page
			if(select <= LCD_pages)
				LCD_currpage = select;
		}
		if(LCD_pagereset_delay) {
			lcd_delaycount_pagereset = 0;
			funcbit_enable(lcd_control, LCT_PAGERESET);
		}
		LCD_linescroll_stop();
	}
#endif
}

//***************************************************************************
//low level LCD routines
//***************************************************************************

#ifdef MPXPLAY_LINK_LCD

#define LCDdataport   (LCD_portnum  )
#define LCDstatusport (LCD_portnum+1)
#define LCDcntrlport  (LCD_portnum+2)

#define LPT_CNTRLBIT_IRQ       0x10
#define LPT_CNTRLBIT_DIRECTION 0x20

#define LPT_SET_DATAIN  outp(LCDcntrlport, (inp(LCDcntrlport) | LPT_CNTRLBIT_DIRECTION) )	// 8bit Data input
#define LPT_SET_DATAOUT outp(LCDcntrlport, (inp(LCDcntrlport) & (~LPT_CNTRLBIT_DIRECTION)) )	// 8bit Data output

#define LPT_CNTRL_INMASK  0xF0	// &
#define LPT_CNTRL_OUTMASK 0x0B	// ^   0x01,0x02,0x08 are inverted

#define LPT_CNTRL_INP     (inp(LCDcntrlport)&LPT_CNTRL_INMASK)	// mask out (clears) LCD-control lines
#define LPT_CNTRL_OUTP(c) outp(LCDcntrlport,(c)^LPT_CNTRL_OUTMASK)	// (required) bits are inverted here !!!

static void LCD_hw_init(void)
{
	lpt_postdelay = max(LCD_portdelay / 5, 1);
	lcd_all_lowlevelfuncs[LCD_type_select]->init();
}

static void LCD_hw_close(void)
{
	if(lcd_all_lowlevelfuncs[LCD_type_select]->close)
		lcd_all_lowlevelfuncs[LCD_type_select]->close();
}

static void LCD_hw_put_char(char Chr)
{
	if(Chr >= 32)				// filter out control codes
		lcd_all_lowlevelfuncs[LCD_type_select]->put_char(Chr);
}

static void LCD_hw_gotoyx(int y, int x)
{
	if(y < 1)
		y = 1;
	else if(y > LCD_lines)
		y = LCD_lines;
	if(x < 1)
		x = 1;
	else if(x > LCD_rows)
		x = LCD_rows;
	lcd_all_lowlevelfuncs[LCD_type_select]->gotoyx(y, x);
}

static void LCD_hw_clear_display(void)
{
	if(lcd_all_lowlevelfuncs[LCD_type_select]->clear_display) {
		lcd_all_lowlevelfuncs[LCD_type_select]->clear_display();
		pds_memset(&lcd_mirror[0][0], 32, (MAX_LCD_LINES + 1) * (MAX_LCD_ROWS + 1));
	}
}

static void LCD_hw_scroll_left(unsigned int line)
{
	if(lcd_all_lowlevelfuncs[LCD_type_select]->hw_scroll_left) {
		pds_memcpy(&lcd_mirror[line][0], &lcd_mirror[line][1], LCD_rows);
		lcd_all_lowlevelfuncs[LCD_type_select]->hw_scroll_left(line);
	}
}

//---------------------------------------------------------------------------
//type 1
//
// HD44780 LCD on LPT port (8-bit output)
//
// 1 controller (max. 2x40 char displays):
//
// LPT Pin:           LCD Pin:
//
//                     1 Vss = GND
//                     2 Vcc = 5V
//                     3 Vo  = Contrast 0-5V; 5V connected to a variable resistor
// 16 Initialize       4 RS  = Register Select
// 14 Auto Line Feed   5 R/W = Read/Write (not used)
//  1  Strobe          6 E   = Enable; edge triggered from high to low
//  2  D0              7 D0
//  3  D1              8 D1
//  4  D2              9 D2
//  5  D3             10 D3
//  6  D4             11 D4
//  7  D5             12 D5
//  8  D6             13 D6
//  9  D7             14 D7
//            15 LED backlight +
//            16 LED backlight -
//
// 2 controllers (for 4x40 displays):
//
// LPT Pin:           LCD Pin:
//
//  9 D7               1 D7
//  8 D6               2 D6
//  7 D5               3 D5
//  6 D4               4 D4
//  5 D3               5 D3
//  4 D2               6 D2
//  3 D1               7 D1
//  2 D0               8 D0
//  1 Strobe           9 E1   = Enable; edge triggered from high to low
// 14 Auto LF         10 R/W  = Read/Write (not used)
// 16 INIT            11 RS   = Register Select (command/data)
//                    12 VLc  = Contrast 0-5V; 5V connected to a variable resistor
//                    13 Vss  = GND
//                    14 Vcc  = +5V
// 17 SLCT        15 E2   = Enable; 2. controller
//            16
//                    17 LED backlight +
//                    18 LED backlight -

//-------------------------------------------------------------------------
#define HD44780_DELAY_TRANSITION    5
#define HD44780_DELAY_INIT       (((LCD_portdelay*500)<2500)? 2500:LCD_portdelay*500)	// was static 1500
#define HD44780_DELAY_CLEAR_HOME (((LCD_portdelay*50)<250)? 250:LCD_portdelay*50)	// was static 170

// HD44780 instructions (RE=0)
#define Clear_Display          0x01
#define Set_Cursor_Home        0x02
#define Set_Entry_Mode         0x04
#define Set_Display            0x08
#define Set_Shift              0x10
#define Set_Function           0x20
#define SET_CG_RAM_ADR         0x40
#define SET_DD_RAM_ADR         0x80

// HD44780 extended register instructions (RE=1)
#define Power_Down_Mode        0x02
#define Extended_Set_Function  0x08
#define Scroll_enable_Line     0x10
#define Set_Function2          0x20
#define Set_SEG_RAM_ADDR       0x40
#define Set_Scroll_Quantity    0x80

// entry mode
#define Decrement_Address      0x00
#define Increment_Address      0x02
#define Shift_Display_Off      0x00
#define Shift_Display_On       0x01

//display/cursor
#define Display_On             0x04
#define Display_Off            0x00
#define Cursor_On              0x02
#define Cursor_Off             0x00
#define Blink_On               0x01
#define Blink_Off              0x00

//cursor and display shift
#define SHIFT_CURSOR_RIGHT     0x00
#define SHIFT_CURSOR_LEFT      0x04
#define SHIFT_DISPLAY_RIGHT    0x08
#define SHIFT_DISPLAY_LEFT     0x0c

//display paramters
#define Data_Length_4          0x00
#define Data_Length_8          0x10
#define One_Display_Line       0x00
#define Two_Display_Lines      0x08
#define Four_Display_Lines     0x08	// ??? doesn't match specification
#define Font_5x7               0x00	// ??? doesn't match specification
#define Font_5x10              0x04	// ??? doesn't match specification

#define RE_bit                 0x04

//Extended Set Function
#define Dot_Font_Width         0x04
#define Inverting_Cursor       0x02
#define Four_Line_Display      0x01

static unsigned int hd44780_enable_select;

/*static void LCD_type1_wait(void)
{
 unsigned int timeout=LCD_portdelay,control,status1,status2;
 control=LPT_CNTRL_INP;
 funcbit_enable(control,LPT_CNTRLBIT_DIRECTION); // set LPT input direction (set direction bit)
 funcbit_enable(control,lpt_cntrlbit_rw);  // set LCD read mode (datain)
 funcbit_disable(control,lpt_cntrlbit_rs); // set LCD command mode

 do{
  funcbit_enable(control,lpt_cntrlbit_e1);
  //funcbit_disable(control,lpt_cntrlbit_e2);
  LPT_CNTRL_OUTP(control);
  pds_delay_10us(1);
  status1=inp(LCDdataport);
  funcbit_disable(control,lpt_cntrlbit_e1);
  LPT_CNTRL_OUTP(control);
  //if((LCD_rows==40) && (LCD_lines==4)){         // 4x40 displays
  if((LCD_rows*LCD_lines)>80 && (y>(LCD_lines/2))){ // 80 (or more) char display
   //funcbit_disable(control,lpt_cntrlbit_e1);
   funcbit_enable(control,lpt_cntrlbit_e2);
   LPT_CNTRL_OUTP(control);
   pds_delay_10us(1);
   status2=inp(LCDdataport);
   funcbit_disable(control,lpt_cntrlbit_e2);
   LPT_CNTRL_OUTP(control);
  }else
   status2=0;
 }while(((status1&0x80) || (status2&0x80)) && (--timeout));

 funcbit_disable(control,LPT_CNTRLBIT_DIRECTION); // set LPT output direction (reset direction bit)
 funcbit_disable(control,lpt_cntrlbit_rw); // set LCD write mode (dataout)
 LPT_CNTRL_OUTP(control);
}*/

static void LCD_type1_put_command(int Cmd)
{
	unsigned int control, cntrl_save;
	//LCD_type1_wait();
	outp(LCDdataport, Cmd);		// send command
	control = cntrl_save = LPT_CNTRL_INP;
	funcbit_disable(control, LPT_CNTRLBIT_DIRECTION);	// set LPT output direction (reset direction bit)
	funcbit_disable(control, lpt_cntrlbit_rs);	// set LCD command mode
	funcbit_enable(control, hd44780_enable_select);
	LPT_CNTRL_OUTP(control);	// LCD enable high
	pds_delay_10us(LCD_portdelay);
	funcbit_disable(control, hd44780_enable_select);
	LPT_CNTRL_OUTP(control);	// LCD enable low
	pds_delay_10us(LCD_portdelay);
	LPT_CNTRL_OUTP(cntrl_save);	// restore control bits
	if((Cmd == Clear_Display) || (Cmd == Set_Cursor_Home))
		pds_delay_10us(HD44780_DELAY_CLEAR_HOME);
	else
		pds_delay_10us(lpt_postdelay);
}

static void lcd_type1_9_init(unsigned int type)
{
	int tmp, control, cntrl_save;

	control = cntrl_save = LPT_CNTRL_INP;

	funcbit_disable(control, LPT_CNTRLBIT_DIRECTION);	// set LPT output direction (reset direction bit)
	outp(LCDdataport, 0);		// clear data line
	LPT_CNTRL_OUTP(control);	// clear control line

	pds_delay_10us(HD44780_DELAY_INIT);

	hd44780_enable_select = lpt_cntrlbit_e1 | lpt_cntrlbit_e2;

	LCD_type1_put_command(Set_Function + Data_Length_8);	// ???
	pds_delay_10us(410);		// ???
	LCD_type1_put_command(Set_Function + Data_Length_8);	// ???
	pds_delay_10us(10);			// ???

	tmp = Set_Function + Data_Length_8 + Font_5x7;	// 0x30
	if(LCD_lines > 1)
		tmp += Two_Display_Lines;	// + 8
	LCD_type1_put_command(tmp);	// = 0x38
	pds_delay_10us(10);			// ???

	if((type == 9) && (LCD_lines > 2)) {
		LCD_type1_put_command(tmp + RE_bit);	// Set RE bit
		pds_delay_10us(10);		// ???
		LCD_type1_put_command(Extended_Set_Function + Four_Line_Display);
		pds_delay_10us(10);		// ???
		LCD_type1_put_command(tmp);	// Clear RE bit
		pds_delay_10us(10);		// ???
	}

	LCD_type1_put_command(Set_Display + Display_Off + Cursor_Off + Blink_Off);	// 0x08
	LCD_type1_put_command(Clear_Display);	// 0x01
	LCD_type1_put_command(Set_Entry_Mode + Increment_Address + Shift_Display_Off);	//0x06
	LCD_type1_put_command(Set_Display + Display_On + Cursor_Off + Blink_Off);	// 0x0c
	LPT_CNTRL_OUTP(cntrl_save);	// restore control bits
	pds_delay_10us(lpt_postdelay);
}

static void LCD_type1_init(void)
{
	lcd_type1_9_init(1);
}

static void LCD_type1_put_char(char Chr)
{
	unsigned int control, cntrl_save;
	switch (Chr) {				// char conversion to LCD
	case '\\':
		Chr = '/';
		break;
	}
	//LCD_type1_wait();
	outp(LCDdataport, Chr);		// send data
	control = cntrl_save = LPT_CNTRL_INP;
	funcbit_disable(control, LPT_CNTRLBIT_DIRECTION);	// set LPT output direction (reset direction bit)
	funcbit_enable(control, lpt_cntrlbit_rs);	// set LCD data mode
	funcbit_enable(control, hd44780_enable_select);
	LPT_CNTRL_OUTP(control);	// LCD enable high
	pds_delay_10us(LCD_portdelay);
	funcbit_disable(control, hd44780_enable_select);
	LPT_CNTRL_OUTP(control);	// LCD enable low
	pds_delay_10us(LCD_portdelay);
	LPT_CNTRL_OUTP(cntrl_save);	// restore control bits
	pds_delay_10us(lpt_postdelay);
}

static void LCD_type1_gotoyx(int y, int x)
{
	unsigned int n;

	if((LCD_rows * LCD_lines) > 80 && (y > (LCD_lines / 2))) {	// 80 (or more) char display
		y -= LCD_lines / 2;
		hd44780_enable_select = lpt_cntrlbit_e2;
	} else
		hd44780_enable_select = lpt_cntrlbit_e1;

	switch (y) {
	case 1:
		n = (x - 1) + 0x00;
		break;
	case 2:
		n = (x - 1) + 0x40;
		break;
	case 3:
		n = (x - 1) + 0x00 + LCD_rows;
		break;					// for 4x20 displays
	case 4:
		n = (x - 1) + 0x40 + LCD_rows;
		break;					// for 4x20 displays
	}

	LCD_type1_put_command(Set_Cursor_Home);	// cursor home
	LCD_type1_put_command(SET_DD_RAM_ADR + n);	// set cursor to yx
}

static void LCD_type1_cleardisplay(void)
{
	hd44780_enable_select = lpt_cntrlbit_e1 | lpt_cntrlbit_e2;
	LCD_type1_put_command(Clear_Display);
}

static lcd_lowlevelfunc_s LCD_TYPE1_funcs = {
	&LCD_type1_init,
	NULL,
	&LCD_type1_put_char,
	&LCD_type1_gotoyx,
	&LCD_type1_cleardisplay,
	NULL
};

//------------------------------------------------------------------------
// type 9 (LPT)
// KS0073 controller (is not 100% HD44780 compatible, but similar)

static void LCD_type9_init(void)
{
	lcd_type1_9_init(9);
}

static void LCD_type9_gotoyx(int y, int x)
{
	unsigned int n;

	if((LCD_rows * LCD_lines) > 80 && (y > (LCD_lines / 2))) {	// 80 (or more) char display
		y -= LCD_lines / 2;
		hd44780_enable_select = lpt_cntrlbit_e2;
	} else
		hd44780_enable_select = lpt_cntrlbit_e1;

	if(LCD_lines > 2) {
		switch (y) {
		case 1:
			n = (x - 1) + 0x00;
			break;
		case 2:
			n = (x - 1) + 0x20;
			break;				//4x20 display
		case 3:
			n = (x - 1) + 0x40;
			break;				//4x20 display
		case 4:
			n = (x - 1) + 0x60;
			break;				//4x20 display
		}
	} else {
		switch (y) {
		case 1:
			n = (x - 1) + 0x00;
			break;
		case 2:
			n = (x - 1) + 0x40;
			break;				//2x40 or 2x20 display
		}
	}

	LCD_type1_put_command(Set_Cursor_Home);	// cursor home
	LCD_type1_put_command(SET_DD_RAM_ADR + n);	// set cursor to yx
}

static lcd_lowlevelfunc_s LCD_TYPE9_funcs = {
	&LCD_type9_init,
	NULL,
	&LCD_type1_put_char,
	&LCD_type9_gotoyx,
	&LCD_type1_cleardisplay,
	NULL
};

//-------------------------------------------------------------------------
//type 2
//
//  4-bit driver module for Hitachi HD44780 based LCD displays.
//
//  1 controller (max. 2x40 char displays):
//
//  LPT pin:    LCD pin:
//
//  2  D0   11  D4
//  3  D1   12  D5
//  4  D2   13  D6
//  5  D3   14  D7
//  6  D4    4  RS register select (command/data)
//  7  D5    5  RW (not used)
//  8  D6    6  EN enable line
//  9  D7    -
//               1  Vss = GND
//               2  Vcc = 5V
//               3  Vo  = Contrast 0-5V; 5V connected to a variable resistor
//              15  LED backlight +
//      16  LED backlight -
//
//
//  2 controllers (for 4x40 displays):
//
//  LPT Pin:    LCD Pin:
//
//  2  D0        4  D4
//  3  D1        3  D5
//  4  D2        2  D6
//  5  D3        1  D7
//  6  D4       11  RS   = Register Select (command/data)
//  7  D5       10  R/W  = Read/Write (not used)
//  8  D6        9  E1   = Enable; edge triggered from high to low
//  9  D7       15  E2   = Enable; 2. controller
//              12  VLc  = Contrast 0-5V; 5V connected to a variable resistor
//              13  Vss  = GND
//              14  Vcc  = +5V
//              17  LED backlight +
//              18  LED backlight -

#define HD44780_4BIT_RS 0x10
#define HD44780_4BIT_RW 0x20	// not used
#define HD44780_4BIT_E1 0x40
#define HD44780_4BIT_E2 0x80

static void LCD_type2_put_command(int Cmd)
{
	unsigned int h = (Cmd >> 4) & 0x0f, l = Cmd & 0x0f;

	outp(LCDdataport, h);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, h | hd44780_enable_select);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, h);
	pds_delay_10us(LCD_portdelay);

	outp(LCDdataport, l);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, l | hd44780_enable_select);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, l);
	pds_delay_10us(LCD_portdelay);

	if((Cmd == Clear_Display) || (Cmd == Set_Cursor_Home))
		pds_delay_10us(HD44780_DELAY_CLEAR_HOME);
}

static void LCD_type2_init(void)
{
	int tmp;
	const int enablelines = HD44780_4BIT_E1 | HD44780_4BIT_E2;

	LPT_SET_DATAOUT;

	outp(LCDdataport, 0x03);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, 0x03 | enablelines);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, 0x03);
	pds_delay_10us(HD44780_DELAY_INIT);

	outp(LCDdataport, 0x03 | enablelines);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, 0x03);
	pds_delay_10us(LCD_portdelay * 30);

	outp(LCDdataport, 0x03 | enablelines);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, 0x03);
	pds_delay_10us(LCD_portdelay * 5);

	// now in 8-bit mode,  set 4-bit mode
	outp(LCDdataport, 0x02);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, 0x02 | enablelines);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, 0x02);
	pds_delay_10us(LCD_portdelay * 5);

	tmp = Set_Function + Data_Length_4 + Font_5x7;	// 0x20
	if(LCD_lines > 1)
		tmp += Two_Display_Lines;	// + 8
	LCD_type2_put_command(tmp);	// =0x28
	pds_delay_10us(LCD_portdelay * 5);
	LCD_type2_put_command(Set_Display + Cursor_Off + Blink_Off);	// 0x08
	LCD_type2_put_command(Clear_Display);	// 0x01
	LCD_type2_put_command(Set_Entry_Mode + Increment_Address + Shift_Display_Off);	//0x06
	LCD_type2_put_command(Set_Display + Display_On + Cursor_Off + Blink_Off);	// 0x0c
}

static void LCD_type2_put_char(char Chr)
{
	unsigned int h, l, control;
	switch (Chr) {				// char conversion to LCD
	case '\\':
		Chr = '/';
		break;
	}
	h = (Chr >> 4) & 0x0f;
	l = Chr & 0x0f;
	control = HD44780_4BIT_RS;	// set data mode

	outp(LCDdataport, control | h);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, control | h | hd44780_enable_select);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, control | h);
	pds_delay_10us(LCD_portdelay);

	outp(LCDdataport, control | l);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, control | l | hd44780_enable_select);
	pds_delay_10us(LCD_portdelay);
	outp(LCDdataport, control | l);
	pds_delay_10us(LCD_portdelay);
}

static void LCD_type2_gotoyx(int y, int x)
{
	unsigned int n;

	if((LCD_rows == 40) && (y > 2)) {	// 40x4 char display
		y -= 2;
		hd44780_enable_select = HD44780_4BIT_E2;
	} else
		hd44780_enable_select = HD44780_4BIT_E1;

	switch (y) {
	case 1:
		n = (x - 1) + 0x00;
		break;
	case 2:
		n = (x - 1) + 0x40;
		break;
	case 3:
		n = (x - 1) + 0x00 + LCD_rows;
		break;
	case 4:
		n = (x - 1) + 0x40 + LCD_rows;
		break;
	}
	LCD_type2_put_command(Set_Cursor_Home);
	LCD_type2_put_command(SET_DD_RAM_ADR + n);	// set cursor to yx
}

static void LCD_type2_cleardisplay(void)
{
	LCD_type2_put_command(Clear_Display);
}

static lcd_lowlevelfunc_s LCD_TYPE2_funcs = {
	&LCD_type2_init,
	NULL,
	&LCD_type2_put_char,
	&LCD_type2_gotoyx,
	&LCD_type2_cleardisplay,
	NULL
};

//-----------------------------------------------------------------------
//type 3 - Noritake VFD display

//Controlling a Noritake 800A 128x64 pixel graphical VFD display using PIC16F877.
//Datasheet with more detailed information about the display is available
//from Noritake http://www.noritake-elec.com/800.htm
//
//based on a sample code by Henri Skippari

//  connection is configurable in mpxplay.ini with the modification of
//  LPT_cntrlbit_RW=0x02
//  LPT_cntrlbit_RS=0x04
//  LPT_cntrlbit_E1=0x01
//
//  LPT             VFD
//
//  2-9 D0 - D7     D0 - D7
//  1   Strobe      WR (E1)  // send data (enable)
//  14  Auto LF     RD (RW)  // direction (read/write)
//  16  Init        CD (RS)  // command/data (register select)

#define NORITAKE_DELAY_BUS  0
#define NORITAKE_DELAY_INIT 410

#define NORITAKE_CHAR_WIDTH  8	// 5x7 chars are placed in a 6x8 box
#define NORITAKE_CHAR_HEIGHT 6

#define NORITAKE_INIT_800A 0x5f
#define NORITAKE_INIT_800B 0x62
#define NORITAKE_INIT_800C 0x00
#define NORITAKE_INIT_800D 0xff

#define NORITAKE_SETLAYER       0x20
#define NORITAKE_LAYER0         0x04
#define NORITAKE_LAYER1         0x08

#define NORITAKE_SETDISPLAY_ON  0x40
#define NORITAKE_SETDISPLAY_OFF 0x00
#define NORITAKE_SETREVERSE_ON  0x10
#define NORITAKE_SETREVERSE_OFF 0x00
#define NORITAKE_SETMODE_OR     0x00
#define NORITAKE_SETMODE_XOR    0x04
#define NORITAKE_SETMODE_AND    0x08

#define NORITAKE_SETAUTOINCREMENT         0x80
#define NORITAKE_AUTOINCREMENT_VERTICAL   0x02
#define NORITAKE_AUTOINCREMENT_HORIZONTAL 0x04

#define NORITAKE_HSHIFT    0x70
#define NORITAKE_VSHIFT    0xB0

static unsigned char font5x7[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF2, 0x00, 0x00, 0x00, 0xC0, 0x00, 0xC0, 0x00,
	0x28, 0xFE, 0x28, 0xFE, 0x28, 0x24, 0x54, 0xFE, 0x54, 0x48, 0xC4, 0xC8, 0x10, 0x26, 0x46,
	0x6C, 0x92, 0x9A, 0x64, 0x0A, 0x00, 0xA0, 0xC0, 0x00, 0x00, 0x00, 0x38, 0x44, 0x82, 0x00,
	0x00, 0x82, 0x44, 0x38, 0x00, 0x28, 0x10, 0x7C, 0x10, 0x28, 0x10, 0x10, 0x7C, 0x10, 0x10,
	0x0A, 0x0C, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x06, 0x06, 0x00, 0x00, 0x00,
	0x04, 0x08, 0x10, 0x20, 0x40, 0x7C, 0x8A, 0x92, 0xA2, 0x7C, 0x00, 0x42, 0xFE, 0x02, 0x00,
	0x42, 0x86, 0x8A, 0x92, 0x62, 0x84, 0x82, 0xA2, 0xD2, 0x8C, 0x18, 0x28, 0x48, 0xFE, 0x08,
	0xE4, 0xA2, 0xA2, 0xA2, 0x9C, 0x3C, 0x52, 0x92, 0x92, 0x0C, 0x80, 0x8E, 0x90, 0xA0, 0xC0,
	0x6C, 0x92, 0x92, 0x92, 0x6C, 0x60, 0x92, 0x92, 0x94, 0x78, 0x00, 0x6C, 0x6C, 0x00, 0x00,
	0x00, 0x6A, 0x6C, 0x00, 0x00, 0x10, 0x28, 0x44, 0x82, 0x00, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x00, 0x82, 0x44, 0x28, 0x10, 0x40, 0x80, 0x8A, 0x90, 0x60, 0x4C, 0x92, 0x9E, 0x82, 0x7C,
	0x3E, 0x48, 0x88, 0x48, 0x3E, 0xFE, 0x92, 0x92, 0x92, 0x6C, 0x7C, 0x82, 0x82, 0x82, 0x44,
	0xFE, 0x82, 0x82, 0x44, 0x38, 0xFE, 0x92, 0x92, 0x82, 0x82, 0xFE, 0x90, 0x90, 0x90, 0x80,
	0x7C, 0x82, 0x92, 0x92, 0x5C, 0xFE, 0x10, 0x10, 0x10, 0xFE, 0x00, 0x82, 0xFE, 0x82, 0x00,
	0x04, 0x02, 0x82, 0xFC, 0x80, 0xFE, 0x10, 0x28, 0x44, 0x82, 0xFE, 0x02, 0x02, 0x02, 0x02,
	0xFE, 0x40, 0x20, 0x40, 0xFE, 0xFE, 0x20, 0x10, 0x08, 0xFE, 0x7C, 0x82, 0x82, 0x82, 0x7C,
	0xFE, 0x90, 0x90, 0x90, 0x60, 0x7C, 0x82, 0x8A, 0x84, 0x7A, 0xFE, 0x90, 0x98, 0x94, 0x62,
	0x64, 0x92, 0x92, 0x92, 0x4C, 0x80, 0x80, 0xFE, 0x80, 0x80, 0xFC, 0x02, 0x02, 0x02, 0xFC,
	0xF8, 0x04, 0x02, 0x04, 0xF8, 0xFC, 0x02, 0x0C, 0x02, 0xFC, 0xC6, 0x28, 0x10, 0x28, 0xC6,
	0xE0, 0x10, 0x0E, 0x10, 0xE0, 0x86, 0x8A, 0x92, 0xA2, 0xC2, 0x00, 0xFE, 0x82, 0x82, 0x00,
	0x40, 0x20, 0x10, 0x08, 0x04, 0x00, 0x82, 0x82, 0xFE, 0x00, 0x20, 0x40, 0x80, 0x40, 0x20,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x80, 0x40, 0x20, 0x00, 0x04, 0x2A, 0x2A, 0x2A, 0x1E,
	0xFE, 0x12, 0x12, 0x12, 0x0C, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x0C, 0x12, 0x12, 0x12, 0xFE,
	0x1C, 0x2A, 0x2A, 0x2A, 0x1A, 0x10, 0x7E, 0x90, 0x40, 0x00, 0x12, 0x2A, 0x2A, 0x2A, 0x3C,
	0xFE, 0x10, 0x10, 0x10, 0x0E, 0x00, 0x00, 0x5E, 0x00, 0x00, 0x04, 0x02, 0x02, 0xBC, 0x00,
	0xFE, 0x08, 0x14, 0x22, 0x00, 0x00, 0x82, 0xFE, 0x02, 0x00, 0x3E, 0x20, 0x18, 0x20, 0x3E,
	0x3E, 0x10, 0x20, 0x20, 0x1E, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x3E, 0x28, 0x28, 0x28, 0x10,
	0x10, 0x28, 0x28, 0x28, 0x3E, 0x3E, 0x10, 0x20, 0x20, 0x10, 0x12, 0x2A, 0x2A, 0x2A, 0x24,
	0x20, 0xFC, 0x22, 0x04, 0x00, 0x3C, 0x02, 0x02, 0x02, 0x3C, 0x38, 0x04, 0x02, 0x04, 0x38,
	0x3C, 0x02, 0x0C, 0x02, 0x3C, 0x22, 0x14, 0x08, 0x14, 0x22, 0x20, 0x12, 0x0C, 0x10, 0x20,
	0x22, 0x26, 0x2A, 0x32, 0x22, 0x10, 0x6C, 0x82, 0x82, 0x00, 0x12, 0x7E, 0x92, 0x82, 0x42,
	0x00, 0x82, 0x82, 0x6C, 0x10, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
};

static void LCD_noritake_put_command(int command)
{
	unsigned int control, cntrl_save;

	control = cntrl_save = LPT_CNTRL_INP;
	funcbit_disable(control, LPT_CNTRLBIT_DIRECTION);	// set LPT output direction (reset direction bit)
	funcbit_enable(control, lpt_cntrlbit_rw);	// set write mode
	funcbit_enable(control, lpt_cntrlbit_rs);	// set command mode
	funcbit_disable(control, lpt_cntrlbit_e1);
	LPT_CNTRL_OUTP(control);	// LCD enable low
	pds_delay_10us(NORITAKE_DELAY_BUS);
	outp(LCDdataport, command);	// send command
	pds_delay_10us(NORITAKE_DELAY_BUS);
	funcbit_enable(control, lpt_cntrlbit_e1);
	LPT_CNTRL_OUTP(control);	// LCD enable high
	pds_delay_10us(NORITAKE_DELAY_BUS);
	LPT_CNTRL_OUTP(cntrl_save);	// restore control bits
	pds_delay_10us(lpt_postdelay);
}

static void LCD_noritake_put_data(int data)
{
	unsigned int control, cntrl_save;

	control = cntrl_save = LPT_CNTRL_INP;
	funcbit_disable(control, LPT_CNTRLBIT_DIRECTION);	// set LPT output direction (reset direction bit)
	funcbit_enable(control, lpt_cntrlbit_rw);	// set write mode
	funcbit_disable(control, lpt_cntrlbit_rs);	// set data mode
	funcbit_disable(control, lpt_cntrlbit_e1);
	LPT_CNTRL_OUTP(control);	// LCD enable low
	pds_delay_10us(NORITAKE_DELAY_BUS);
	outp(LCDdataport, data);	// send data
	pds_delay_10us(NORITAKE_DELAY_BUS);
	funcbit_enable(control, lpt_cntrlbit_e1);
	LPT_CNTRL_OUTP(control);	// LCD enable high
	pds_delay_10us(NORITAKE_DELAY_BUS);
	LPT_CNTRL_OUTP(cntrl_save);	// restore control bits
	pds_delay_10us(lpt_postdelay);
}

static void LCD_type3_gotoyx(int y, int x)
{
	LCD_noritake_put_command(0x60);
	LCD_noritake_put_command(y - 1);
	LCD_noritake_put_command(0x64);
	LCD_noritake_put_command((x - 1) * NORITAKE_CHAR_HEIGHT);
}

static void LCD_type3_cleardisplay(void)
{
	//LCD_noritake_put_command(0x50|NORITAKE_LAYER0|NORITAKE_LAYER1);
	LCD_noritake_put_command(0x5f);
	delay(1);
}

static void LCD_noritake_SetBrightness(unsigned int brightness)
{
	LCD_noritake_put_command(0x40 + (0x0F - brightness));	// 0..15
	delay(1);
}

static void LCD_type3_init(void)
{
	unsigned int i;

	LCD_noritake_put_command(NORITAKE_INIT_800A);	// ???
	pds_delay_10us(NORITAKE_DELAY_INIT);	//
	LCD_noritake_put_command(NORITAKE_INIT_800A);	//
	pds_delay_10us(NORITAKE_DELAY_INIT);	//

	for(i = 0; i < 8; i++) {
		LCD_noritake_put_command(NORITAKE_INIT_800B);
		pds_delay_10us(NORITAKE_DELAY_INIT);
		LCD_noritake_put_command(NORITAKE_INIT_800C + i);
		pds_delay_10us(NORITAKE_DELAY_INIT);
		LCD_noritake_put_data(NORITAKE_INIT_800D);
		pds_delay_10us(NORITAKE_DELAY_INIT);
	}

	LCD_noritake_put_command(NORITAKE_SETAUTOINCREMENT | NORITAKE_AUTOINCREMENT_HORIZONTAL);
	LCD_type3_gotoyx(1, 1);
	LCD_noritake_SetBrightness(0);
	LCD_noritake_put_command(NORITAKE_SETLAYER);	// set all layers off
	LCD_noritake_put_command(NORITAKE_SETDISPLAY_OFF | NORITAKE_SETMODE_AND);
	LCD_type3_cleardisplay();
	LCD_noritake_put_command(NORITAKE_SETLAYER | NORITAKE_LAYER0 | NORITAKE_LAYER1);	// ???
	LCD_noritake_put_command(NORITAKE_SETDISPLAY_ON | NORITAKE_SETMODE_OR);
	LCD_noritake_SetBrightness(15);
	//LCD_noritake_put_command(NORITAKE_HSHIFT); // ???
	//LCD_noritake_put_command(0x00);
	//LCD_noritake_put_command(NORITAKE_VSHIFT);
}

static unsigned char lcd_noritake_bitswap(unsigned char x)
{
	x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
	x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
	x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
	return x;
}

static void LCD_type3_put_char(char Chr)
{
	unsigned char data, i;

	LCD_noritake_put_command(NORITAKE_SETAUTOINCREMENT | NORITAKE_AUTOINCREMENT_HORIZONTAL);

	if((Chr < 32) || (Chr >= 128))
		Chr = '?';

	Chr = Chr - 32;
	for(i = 0; i < 5; i++) {
		data = font5x7[Chr * 5 + i];
		data = lcd_noritake_bitswap(data);
		LCD_noritake_put_data(data);
	}
	LCD_noritake_put_data(0);
}

static lcd_lowlevelfunc_s LCD_TYPE3_funcs = {
	&LCD_type3_init,
	NULL,
	&LCD_type3_put_char,
	&LCD_type3_gotoyx,
	&LCD_type3_cleardisplay,
	NULL
};

//--------------------------------------------------------------------------
//type 4
/* ----------------------------------------------------------
 * Program to control a T6963C-based 240x64 pixel LCD display
 * using the PC's Parallel Port (LPT) in bidirectional mode
 *
 * Written by John P. Beale May 3-4, 1997  beale@best.com
 *
 *  Based on information from Steve Lawther,
 *  "Writing Software for T6963C based Graphic LCDs", 1997 which is at
 *  http://ourworld.compuserve.com/homepages/steve_lawther/t6963c.pdf
 *
 *  and the Toshiba T6963C data sheet, also on Steve's WWW page
 *
 *  and info at: http://www.citilink.com/~jsampson/lcdindex.htm
 *               http://www.cs.colostate.edu/~hirsch/LCD.html
 *               http://www.hantronix.com/
 *
 *  See also: http://members1.chello.nl/r.schotsman/LCDFrame.htm
 * ----------------------------------------------------------
 */

 // Corrections and enhancements (fast block write mode) by S.Zeller - zeller@bnro.de
 // Optimizations and code cleaning by PDSoft - mpxplay@freemail.hu

/* --------------------------------------------------------------
 *
 * -------------------------------------------
 *  20-pin header on TOSHIBA TLX-711A module *
 *  (viewed from top-note pin numbering)     *
 * -------------------------------------------
   *  19--FS    X1--20    *
   *  17--D6    D7--18    *
   *  15--D4    D5--16    *
   *  13--D2    D3--14    *
   *  11--D0    D1--12    *
   *   9--XX  /RST--10    *
   *   7--/CE   CD--8     *
   *   5--/WR  /RD--6     *
   *   3--Vdd   Vl--4     *
   *   1--Fgnd  Vss-2     *
 * ------------------------
 *
 * Connecting LCD module TLX-711A-E0 (uses T6963C controller)
 * to the PC parallel port: pin connections needed are below.
 *
 * "PC Port pin" numbers refer to pins on PC's DB25 parallel port.
 * Recall that SEL (pin 17), LF (14), and STROBE (1) control ouputs
 * are inverted, but Init (16) is true.
 *
 * The LPT must be in EPP mode (in BIOS), else maybe the LCD won't work
 *
 *  LCD Pin ----- PC Port Pin  Status Reg. bit
 * ------------------------------------------
 *  /RD  (6) <--> (17) /SEL      3
 *  C/D  (8) <--> (16) Init      2
 *  /CE  (7) <--> (14) /LF       1
 *  /WR  (5) <--> (1)  /Strobe   0

 * -----------------------
 *  D0  (11) <--> (2)  D0
 *  D1  (12) <--> (3)  D1
 *  D2  (13) <--> (4)  D2
 *  D3  (14) <--> (5)  D3
 *  D4  (15) <--> (6)  D4
 *  D5  (16) <--> (7)  D5
 *  D6  (17) <--> (8)  D6
 *  D7  (18) <--> (9)  D7
 *  GND (2)  <--> (25) GND
 * --------------------------------------------------------------
 *  FG    (1)  frame ground
 *  +5V   (3)  LCD logic supply
 *  -7.8V (4)  LCD display contrast
 *  FS    (19) font select
 *  RST   (10) active low
 */

#define T6963C_TEXTMEM_BASE  0x0000	// base address of text memory
#define T6963C_GRAPHMEM_BASE 0x0200	// base address of graphics memory

#define T6963C_CEHI outp(LCDcntrlport, (inp(LCDcntrlport) & (~0x02)) )
#define T6963C_CELO outp(LCDcntrlport, (inp(LCDcntrlport) | 0x02) )

#define T6963C_RDHI outp(LCDcntrlport, (inp(LCDcntrlport) & (~0x08)) )
#define T6963C_RDLO outp(LCDcntrlport, (inp(LCDcntrlport) | 0x08) )

#define T6963C_WRHI outp(LCDcntrlport, (inp(LCDcntrlport) & (~0x01)) )
#define T6963C_WRLO outp(LCDcntrlport, (inp(LCDcntrlport) | 0x01) )

#define T6963C_CDHI outp(LCDcntrlport, (inp(LCDcntrlport) | 0x04) )
#define T6963C_CDLO outp(LCDcntrlport, (inp(LCDcntrlport) & (~0x04)) )

// get LCD display status byte
static int LCD_type_T6963C_getstatus(void)
{
	int lcd_status;

	LPT_SET_DATAIN;				// make 8-bit parallel port an input
	T6963C_WRHI;
	T6963C_CDHI;				// bring LCD C/D line high (read status byte)
	T6963C_RDLO;				// bring LCD /RD line low (read active)
	T6963C_CELO;				// bring LCD /CE line low (chip-enable active)
	lcd_status = inp(LCDdataport);	// read LCD status byte
	T6963C_CEHI;				// bring LCD /CE line high, disabling it
	T6963C_RDHI;				// deactivate LCD read mode
	LPT_SET_DATAOUT;			// make 8-bit parallel port an output port

	return (lcd_status);
}

// wait until display ready
static void LCD_type_T6963C_wait(unsigned int bitmask)
{
	unsigned int i = LCD_portdelay * 20000;
	while(!(LCD_type_T6963C_getstatus() & bitmask) && (--i)) {
	}
}

static void LCD_type_T6963C_endofputdata(void)
{
	T6963C_CDLO;
	T6963C_RDHI;				// make sure LCD read mode is off
	T6963C_WRLO;				// activate LCD write mode
	T6963C_CELO;				// pulse ChipEnable LOW, > 80 ns, enables LCD I/O
	T6963C_CEHI;				// disable LCD I/O
	T6963C_WRHI;				// deactivate write mode
}

// write data byte to LCD module over LPT port
// assume PC port in data OUTPUT mode
static void LCD_type_T6963C_putdata(int byte)
{
	LCD_type_T6963C_wait(0x03);
	outp(LCDdataport, byte);
	LCD_type_T6963C_endofputdata();
}

// write data byte to LCD module over LPT port in block mode
// assume PC port in data OUTPUT mode
static void LCD_type_T6963C_putdata_block(int byte)
{
	LCD_type_T6963C_wait(0x08);
	outp(LCDdataport, byte);
	LCD_type_T6963C_endofputdata();
}

static void LCD_type_T6963C_endofputcommand(void)
{
	T6963C_CDHI;				// control/status mode
	T6963C_RDHI;				// make sure LCD read mode is off
	T6963C_WRLO;				// activate LCD write mode
	T6963C_CELO;				// pulse ChipEnable LOW, > 80 ns, enables LCD I/O
	T6963C_CEHI;				// disable LCD I/O
	T6963C_WRHI;				// deactivate write mode
}

// write command byte to LCD module
// assumes port is in data OUTPUT mode
static void LCD_type_T6963C_putcommand(int byte)
{
	LCD_type_T6963C_wait(0x03);
	outp(LCDdataport, byte);
	LCD_type_T6963C_endofputcommand();
}

// write command byte to LCD module in block mode
// assumes port is in data OUTPUT mode
static void LCD_type_T6963C_putcommand_block(int byte)
{
	LCD_type_T6963C_wait(0x08);
	outp(LCDdataport, byte);
	LCD_type_T6963C_endofputcommand();
}

static void LCD_type4_init(void)
{
	int i;

	T6963C_CEHI;				// disable chip
	T6963C_RDHI;				// disable reading from LCD
	T6963C_WRHI;				// disable writing to LCD
	T6963C_CDHI;				// command/status mode
	LPT_SET_DATAOUT;			// make 8-bit parallel port an output port

	LCD_type_T6963C_putdata(T6963C_GRAPHMEM_BASE & 0xff);
	LCD_type_T6963C_putdata(T6963C_GRAPHMEM_BASE >> 8);
	LCD_type_T6963C_putcommand(0x42);	// graphics memory at address T6963C_GRAPHMEM_BASE

	LCD_type_T6963C_putdata(LCD_rows & 0xff);
	LCD_type_T6963C_putdata(LCD_rows >> 8);
	LCD_type_T6963C_putcommand(0x43);	// n bytes per graphics line

	LCD_type_T6963C_putdata(T6963C_TEXTMEM_BASE & 0xff);
	LCD_type_T6963C_putdata(T6963C_TEXTMEM_BASE >> 8);
	LCD_type_T6963C_putcommand(0x40);	// text memory at address T6963C_TEXTMEM_BASE

	LCD_type_T6963C_putdata(LCD_rows & 0xff);
	LCD_type_T6963C_putdata(LCD_rows >> 8);
	LCD_type_T6963C_putcommand(0x41);	// n bytes per text line

	LCD_type_T6963C_putcommand(0x80);	// mode set: Graphics OR Text, ROM CGen

	LCD_type_T6963C_putdata(0x02);
	LCD_type_T6963C_putdata(0x00);
	LCD_type_T6963C_putcommand(0x22);	// set CG RAM start address

	LCD_type_T6963C_putcommand(0x9F);	// Graphics & Text ON, cursor blinking
	// (For cursor to be visible, need to set up position)

	// initial clearing of graphics display
	LCD_type_T6963C_putdata(T6963C_GRAPHMEM_BASE & 0xff);
	LCD_type_T6963C_putdata(T6963C_GRAPHMEM_BASE >> 8);
	LCD_type_T6963C_putcommand(0x24);	// addrptr at address T6963C_GRAPHMEM_BASE
	LCD_type_T6963C_putcommand(0xB0);

	for(i = 0; i < 2560; i++)
		LCD_type_T6963C_putdata_block(0);
	LCD_type_T6963C_putcommand_block(0xB2);
}

static void LCD_type4_put_char(char Chr)
{
	LCD_type_T6963C_putdata(Chr - 0x20);
	LCD_type_T6963C_putcommand(0xC0);
}

static void LCD_type4_gotoyx(int y, int x)
{
	int addr;

	addr = T6963C_TEXTMEM_BASE + ((y - 1) * LCD_rows) + x - 1;
	LCD_type_T6963C_putdata(addr & 0xff);
	LCD_type_T6963C_putdata(addr >> 8);
	LCD_type_T6963C_putcommand(0x24);	// set LCD addr. pointer
}

static void LCD_type4_cleardisplay(void)
{
	int i;

	LCD_type_T6963C_putdata(T6963C_TEXTMEM_BASE & 0xff);
	LCD_type_T6963C_putdata(T6963C_TEXTMEM_BASE >> 8);
	LCD_type_T6963C_putcommand(0x24);	// addrptr at address T6963C_TEXTMEM_BASE
	LCD_type_T6963C_putcommand(0xB0);

	for(i = 0; i < 320; i++)
		LCD_type_T6963C_putdata_block(0);
	LCD_type_T6963C_putcommand_block(0xB2);
}

static lcd_lowlevelfunc_s LCD_TYPE4_funcs = {
	&LCD_type4_init,
	NULL,
	&LCD_type4_put_char,
	&LCD_type4_gotoyx,
	&LCD_type4_cleardisplay,
	NULL
};

//--------------------------------------------------------------------------
//type 5
//HD44780 Serial Port Display; 8051 / VT100 compatible
//originaly written by Uros Palmin (ustudent@email.si.com)

static void LCD_type5_wait(void)
{
	//unsigned int i=LCD_portdelay;
	//while(i--);
	while(!(inp(LCD_portnum + 5) & 0x20)) {
	}
}

static void LCD_type5_put_port_command(int port, int Cmd)
{
	outp(port, Cmd);
}

static void LCD_type5_put_char(char Chr)
{
	switch (Chr) {				// char conversion on LCD
	case '\\':
		Chr = '/';
		break;
	}
	LCD_type5_wait();
	outp(LCDdataport, Chr);
}

static void LCD_type5_init(void)
{
	//LCD_type5_put_port_command(LCD_portnum+3,0x00); // DLAB OFF
	//LCD_type5_put_port_command(LCD_portnum+1,0x00); // disable irq
	LCD_type5_put_port_command(LCD_portnum + 3, 0x80);	// DLAB ON
	LCD_type5_put_port_command(LCD_portnum + 0, 0x0C);	// Baud Rate 9600 BPS MAX SPEED
	LCD_type5_put_port_command(LCD_portnum + 1, 0x00);	// Baud Rate - Divisor Latch High Byte
	LCD_type5_put_port_command(LCD_portnum + 3, 0x03);	// DLAB OFF, 8 Bits, No Parity, 1 Stop Bit
	LCD_type5_put_port_command(LCD_portnum + 2, 0xC7);	// FIFO Control Register (enable and clear FIFO, trigger=16 bytes)
	LCD_type5_put_char(0x19);	// Reset LCD *Must have*
	LCD_type5_wait();
	LCD_type5_put_port_command(LCD_portnum + 4, 0x0B);	// Enable DTR & RTS
	LCD_type5_put_char(0x19);	// Hide Cur
	LCD_type5_put_char(0x0C);	// Clear LCD
}

static void LCD_type5_gotoyx(int y, int x)
{
	unsigned int n;

	switch (y) {
	case 1:
		n = (x - 1) + 0x00;
		break;
	case 2:
		n = (x - 1) + 0x40;
		break;
	case 3:
		n = (x - 1) + 0x00 + LCD_rows;
		break;
	case 4:
		n = (x - 1) + 0x40 + LCD_rows;
		break;
	}

	LCD_type5_put_char(0x02);	// cursor home
	LCD_type5_put_char(0x80 + n);	// set cursor to yx
}

static void LCD_type5_cleardisplay(void)
{
	LCD_type5_put_char(0x01);
}

static lcd_lowlevelfunc_s LCD_TYPE5_funcs = {
	&LCD_type5_init,
	NULL,
	&LCD_type5_put_char,
	&LCD_type5_gotoyx,
	&LCD_type5_cleardisplay,
	NULL
};

//-----------------------------------------------------------------------
// Type 6: Matrix Orbital LCD Support (LK204-25 Tested) on serial port
//
//  Added: Simon J Mackenzie (email: project.mpxplay@smackoz.fastmail.fm)
//   Date: Tuesday, 6 August, 2002
// Update: Tuesday, 20 May, 2003
//
//  more info about this hardware on http://www.matrix-orbital.com
//

static void LCD_type6_wait(void)
{
	while(!(inp(LCD_portnum + 5) & 0x20)) {
	}
}

static void LCD_type6_put_port_command(int port, int Cmd)
{
	outp(port, Cmd);
}

static void LCD_type6_put_char(char Chr)
{
	LCD_type6_wait();
	outp(LCDdataport, Chr);
}

static void LCD_type6_init(void)
{
	// Initialize Com port
	LCD_type6_put_port_command(LCD_portnum + 3, 0x80);	// DLAB ON
	LCD_type6_put_port_command(LCD_portnum + 0, 0x06);	// Baud Rate 19200 BPS MAX SPEED
	LCD_type6_put_port_command(LCD_portnum + 1, 0x00);	// Baud Rate - Divisor Latch High Byte
	LCD_type6_put_port_command(LCD_portnum + 3, 0x03);	// DLAB OFF, 8 Bits, No Parity, 1 Stop Bit
	LCD_type6_put_port_command(LCD_portnum + 2, 0xC7);	// FIFO Control Register enable and clear
	LCD_type6_put_port_command(LCD_portnum + 4, 0x0B);	// Enable DTR & RTS

	// Initialize LCD
	LCD_type6_put_char(0xFE);	// Clear Display
	LCD_type6_put_char(0x58);

	LCD_type6_put_char(0xFE);	// Backlight ON
	LCD_type6_put_char(0x42);
	LCD_type6_put_char(0x00);	// Remain ON (No delay off)

	LCD_type6_put_char(0xFE);	// Cursor OFF
	LCD_type6_put_char(0x4B);

	LCD_type6_put_char(0xFE);	// Cursor Blink OFF
	LCD_type6_put_char(0x54);

	LCD_type6_put_char(0xFE);	// Auto Line Wrap OFF
	LCD_type6_put_char(0x44);

	LCD_type6_put_char(0xFE);	// Auto Scroll OFF
	LCD_type6_put_char(0x82);

	LCD_type6_put_char(0xFE);	// Auto Repeat ON (Matrix Orbital keypad)
	LCD_type6_put_char(0x7E);
	LCD_type6_put_char(0x00);	// 200ms typematic rate

	LCD_type6_put_char(0xFE);	// Default Contrast
	LCD_type6_put_char(0x50);
	LCD_type6_put_char(0xB4);	// 180 = B4 ??
}

static void LCD_type6_gotoyx(int y, int x)
{
	LCD_type6_put_char(0xFE);	// Goto X Y
	LCD_type6_put_char(0x47);	//
	LCD_type6_put_char(x);		//
	LCD_type6_put_char(y);		//
}

static void LCD_type6_cleardisplay(void)
{
	LCD_type6_put_char(0xFE);	// Clear Display
	LCD_type6_put_char(0x58);
}

static lcd_lowlevelfunc_s LCD_TYPE6_funcs = {
	&LCD_type6_init,
	NULL,
	&LCD_type6_put_char,
	&LCD_type6_gotoyx,
	&LCD_type6_cleardisplay,
	NULL
};

//-----------------------------------------------------------------------
// Type 7: EDE702 Serial LCD Interface Support
//
//  By Brent Harris (http://techworld.dyndns.org) and Glenn Garrett
//   Monday, March 24 2003
//   For 16x1 to 20x4 LCD character displays (Tested on a 20x2 LCD)
//   See http://www.elabinc.com for more info on this hardware
//

static void LCD_type7_wait(void)
{
	while(!(inp(LCD_portnum + 5) & 0x20)) {
	}
}

static void LCD_type7_put_port_command(int port, int Cmd)
{
	outp(port, Cmd);
}

static void LCD_type7_put_char(char Chr)
{
	LCD_type7_wait();
	outp(LCDdataport, Chr);
}

static void LCD_type7_cleardisplay(void)	// Clear display and home cursor
{
	//unsigned int i=LCD_portdelay;
	LCD_type7_put_char(0xFE);
	LCD_type7_put_char(0x01);	// Clear display command
	delay(30);					// ???                         // Pause while the command to completes
	//while(--i){}
	LCD_type7_wait();
}

static void LCD_type7_init(void)
{
	// Initialize Com port
	LCD_type7_put_port_command(LCD_portnum + 3, 0x80);	// DLAB ON
	LCD_type7_put_port_command(LCD_portnum + 0, 0x0C);	// Baud Rate 9600 BPS MAX SPEED
	LCD_type7_put_port_command(LCD_portnum + 1, 0x00);	// cont. Hige Latch
	LCD_type7_put_port_command(LCD_portnum + 3, 0x03);	// DLAB OFF, 8 Bits, No Parity, 1 Stop Bit
	LCD_type7_put_port_command(LCD_portnum + 2, 0xC7);	// FIFO Control Register
	LCD_type7_put_port_command(LCD_portnum + 4, 0x0B);	// Enable DTR & RTS

	// Initialize LCD
	LCD_type7_put_char(0xFE);	// Set Length / Font / Lines
	if(LCD_lines > 1)
		LCD_type7_put_char(0x28);	//   0x28 for 2,3,4 lines
	else
		LCD_type7_put_char(0x20);	//   0x20 if only 1 line

	LCD_type7_put_char(0xFE);	// Display/Cursor/Blink OFF
	LCD_type7_put_char(0x08);

	LCD_type7_put_char(0xFE);	// Display ON / Cursor OFF
	LCD_type7_put_char(0x0C);

	LCD_type7_put_char(0xFE);	// Don't shift display /
	LCD_type7_put_char(0x06);	//  Cursor moves to the right

	LCD_type7_cleardisplay();	// Clear display / home cursor
}

static void LCD_type7_gotoyx(int y, int x)
{
	unsigned int n;

	switch (y) {
	case 1:
		n = (x - 1) + 0x00;
		break;
	case 2:
		n = (x - 1) + 0x40;
		break;
	case 3:
		n = (x - 1) + 0x14;
		break;
	case 4:
		n = (x - 1) + 0x54;
		break;
	}

	LCD_type7_put_char(0xFE);
	LCD_type7_put_char(0x80 + n);	// set cursor to yx
}

static lcd_lowlevelfunc_s LCD_TYPE7_funcs = {
	&LCD_type7_init,
	NULL,
	&LCD_type7_put_char,
	&LCD_type7_gotoyx,
	&LCD_type7_cleardisplay,
	NULL
};

//--------------------------------------------------------------------------
//type 8
//VT100 terminal output on serial port - by S.Zeller

static void LCD_type8_put_port_command(int port, int Cmd)
{
	outp(port, Cmd);
}

static void LCD_type8_init(void)
{
	LCD_type8_put_port_command(LCD_portnum + 3, 0x80);	// DLAB ON
	LCD_type8_put_port_command(LCD_portnum + 0, 0x0C);	// Baud Rate 9600 BPS MAX SPEED
	LCD_type8_put_port_command(LCD_portnum + 1, 0x00);	// cont. Hige Latch
	LCD_type8_put_port_command(LCD_portnum + 3, 0x03);	// DLAB OFF, 8 Bits, No Parity, 1 Stop Bit
	LCD_type8_put_port_command(LCD_portnum + 2, 0xC7);	// FIFO enable and clear
	LCD_type8_put_port_command(LCD_portnum + 4, 0x0B);	// Enable DTR & RTS
}

static void LCD_type8_put_char(char Chr)
{
	while(!(inp(LCD_portnum + 5) & 0x20)) {
	}							// is always safe, even if port does not exist (reads 0xFF)
	outp(LCD_portnum + 0, Chr);
}

static void LCD_type8_put_str(char *escptr)
{
	while(*escptr) {
		LCD_type8_put_char(*escptr);
		escptr++;
	}
}

static void LCD_type8_gotoyx(int y, int x)
{
	char escstr[16];
	sprintf(escstr, "\x1B[%u;%uH", y, x);
	LCD_type8_put_str(escstr);
}

static void LCD_type8_cleardisplay(void)
{
	LCD_type8_put_str("\x1B[2J");
}

static lcd_lowlevelfunc_s LCD_TYPE8_funcs = {
	&LCD_type8_init,
	NULL,
	&LCD_type8_put_char,
	&LCD_type8_gotoyx,
	&LCD_type8_cleardisplay,
	NULL
};

//-----------------------------------------------------------------------
//type 10 (you can add a new lcd type here)

/*
static void LCD_type10_init(void)
{

}

static void LCD_type10_close(void)
{

}

static void LCD_type10_put_char(char Chr)
{

}

static void LCD_type10_gotoyx(int y,int x)
{

}

static void LCD_type10_cleardisplay(void)
{

}

static lcd_lowlevelfunc_s LCD_TYPE10_funcs={
 &LCD_type10_init,               // required
 &LCD_type10_close,              // not required
 &LCD_type10_put_char,           // required
 &LCD_type10_gotoyx,             // required
 &LCD_type10_cleardisplay,       // not required
 NULL                            // not required (hw_scroll_left)
};
*/

#endif							// MPXPLAY_LINK_LCD
