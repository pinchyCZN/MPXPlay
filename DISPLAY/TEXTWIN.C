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
//function: text window make

#include "newfunc\newfunc.h"
#include "control\cntfuncs.h"
#include "display.h"
#include "visualpi.h"

#define TEXTWIN_LOCFLAG_EDITLINE_OVERWRITE   1	// Ins key enables/disables (default is inserting)
//#define TEXTWIN_LOCFLAG_EDITLINE_NONFIRSTKEY 2 // first Ins disables auto-clear

#define TEXTWIN_WINDOWSIZE_MIN_Y 3
#define TEXTWIN_WINDOWSIZE_MAX_Y (textscreen_maxy)
#define TEXTWIN_WINDOWSIZE_MIN_X (textscreen_maxx/2)	//((textscreen_maxx*5)/8)
#define TEXTWIN_WINDOWSIZE_MAX_X (textscreen_maxx)
#define TEXTWIN_BORDERSIZE_X     4
#define TEXTWIN_BORDERSIZE_Y     2

#define TEXTWIN_WINDOW_SHADOW_X 2
#define TEXTWIN_WINDOW_SHADOW_Y 1

#define TEXTWIN_ITEMNUM_ALLOC 32
#define TEXTWIN_BUTTONS_GAPSIZE 1

#define TEXTWIN_LINETYPE_MSG_STATIC     1	// static message (can be a pointer)
#define TEXTWIN_LINETYPE_MSG_ALLOCATED  2	// allocate and copy data (if data has passed through stack)
#define TEXTWIN_LINETYPE_MSG_EDIT       4	// edit text
#define TEXTWIN_LINETYPE_BUTTON         8	// buttons
#define TEXTWIN_LINETYPE_SWITCH        16	// [x] switch
#define TEXTWIN_LINETYPE_SEPARATOR     32	// vertical separator line
#define TEXTWIN_LINETYPES_ACTIVE (TEXTWIN_LINETYPE_MSG_EDIT|TEXTWIN_LINETYPE_BUTTON|TEXTWIN_LINETYPE_SWITCH)

// !!! other (external) flags in display.c (TEXTWIN_EDITFLAG_)
#define TEXTWIN_EDITLINEFLAG_SECMOUSEPOS 0x4000000	// 2. mouse positioning
#define TEXTWIN_EDITLINEFLAG_FIRSTKEY    0x8000000

#define TEXTWIN_MAX_WINDOWS 1

typedef char *textwin_msgtype_passive_t;
typedef char **textwin_msgtype_active_t;
typedef display_textwin_button_t *textwin_msgtype_button_t;

typedef void (*execfunc_data_t) (void *);
typedef void (*execfunc_nodata_t) (void);

typedef void (*buttonhand_func_t) (void *, unsigned int);

typedef struct textwin_editline_t {
	char *strbuf;
	unsigned int maxdatalen;
	unsigned int line_data_pos;
	unsigned int line_data_begin;
	unsigned int line_data_len;
	unsigned long flags;
} textwin_editline_t;

typedef struct textwin_switchline_t {
	mpxp_uint32_t *switchvar;
	mpxp_uint32_t switchbit;
	char *switchtext;
} textwin_switchline_t;

typedef struct textwin_item_t {
	unsigned int type;
	unsigned int flags;
	int xpos, ypos;
	unsigned int xsize;			// ysize currently is always 1
	void *data;					// can be button_t, editline_t, switchline_t, msg (char)
} textwin_item_t;

typedef struct textwin_window_t {
	unsigned int flags;			// external control (bits are defined in display.h)
	unsigned int locflags;		// internal control
	unsigned char *winmem;		// window memory
	unsigned int basecolor, buttoncolor;	// colors
	unsigned int xpos, ypos, xsize, ysize;	// pos,size

	char *headtext;

	unsigned int nb_buttons;
	buttonhand_func_t *buttonhandlerfunc;
	void *buttonhandlerdata;

	unsigned int nb_items;
	textwin_item_t *item_first;
	unsigned int nb_allocated_items;
	unsigned int item_pos_y;
	int item_selected;
} textwin_window_t;

static void display_textwin_desktop_close(void);
static void display_textwin_reset_window(textwin_window_t * tw);

extern unsigned int displaymode, textscreen_maxx, textscreen_maxy;
extern unsigned long textscreen_linear_address;

static unsigned long ts_linear_address_save, textscreen_memory_size;
static unsigned long textscreen_target_address;
static unsigned long textwin_vidmem_address, textwin_vidmem_size;
static unsigned long *textwin_address_map;
#ifdef MPXPLAY_WIN32
static unsigned short *textwin_vidmem_tmp_attrib;
static char *textwin_vidmem_tmp_text;
#endif
static unsigned int textwin_keycode;
static unsigned int textwin_shadowcolor = (CL_TEXTWIN_SHADOW | (CLB_TEXTWIN_SHADOW << 4));
static textwin_window_t *textwin_windows[TEXTWIN_MAX_WINDOWS];

static const char twborderchars[12] = "ͺɻȼǶ�ϳ�";

static display_textwin_button_t confirm_buttons_notext[] = {
	{"", KEY_ENTER1},			// gray enter
	{"", KEY_ENTER2},			// white enter
	{"", KEY_ESC},				// ESC
	{NULL, 0}
};

static display_textwin_button_t confirm_buttons_yesno[] = {
	{" Yes ", 0x1579},			// 'y'
	{"", 0x1559},				// 'Y'
	{" No ", 0x316e},			// 'n'
	{"", 0x314e},				// 'N'
	{"", KEY_ESC},				// ESC
	{NULL, 0}
};

static display_textwin_button_t errorhand_button_ok[] = {
	{"[ Ok ]", KEY_ESC},		//
	{NULL, 0}
};

unsigned int display_textwin_init(void)
{
	if(!(displaymode & DISP_FULLSCREEN))
		return 0;
	textscreen_memory_size = textscreen_maxx * textscreen_maxy * TEXTSCREEN_BYTES_PER_CHAR;
	if(textwin_vidmem_size < textscreen_memory_size) {
		display_textwin_close();
		textwin_vidmem_address = (unsigned long)malloc(textscreen_memory_size * 2);
		if(!textwin_vidmem_address)
			goto err_out_init;
		textwin_vidmem_size = textscreen_memory_size * 2;
		textwin_address_map = (unsigned long *)malloc(textwin_vidmem_size * sizeof(unsigned char *));
		if(!textwin_address_map)
			goto err_out_init;
#ifdef MPXPLAY_WIN32
		textwin_vidmem_tmp_attrib = (unsigned short *)malloc(textwin_vidmem_size / TEXTSCREEN_BYTES_PER_CHAR * sizeof(unsigned short));
		if(!textwin_vidmem_tmp_attrib)
			goto err_out_init;
		textwin_vidmem_tmp_text = (char *)malloc(textwin_vidmem_size / TEXTSCREEN_BYTES_PER_CHAR * sizeof(char));
		if(!textwin_vidmem_tmp_text)
			goto err_out_init;
#endif
	}

	return 1;

  err_out_init:
	display_textwin_close();
	return 0;
}

void display_textwin_close(void)
{
	display_textwin_desktop_close();
	if(textwin_vidmem_address) {
		free((void *)textwin_vidmem_address);
		textwin_vidmem_address = 0;
	}
	if(textwin_address_map) {
		free(textwin_address_map);
		textwin_address_map = NULL;
	}
#ifdef MPXPLAY_WIN32
	if(textwin_vidmem_tmp_attrib) {
		free(textwin_vidmem_tmp_attrib);
		textwin_vidmem_tmp_attrib = NULL;
	}
	if(textwin_vidmem_tmp_text) {
		free(textwin_vidmem_tmp_text);
		textwin_vidmem_tmp_text = NULL;
	}
#endif
	textwin_vidmem_size = 0;
}

//-----------------------------------------------------------------------

static void display_textwin_addressmap_reset(void)
{
	if(textwin_address_map && textwin_vidmem_size) {
		unsigned int i = textwin_vidmem_size;
		unsigned long *map = textwin_address_map;
		unsigned char *pt = (unsigned char *)textwin_vidmem_address;
		do {
			*map = (unsigned long)pt;
			map++;
			pt++;
		} while(--i);
	}
}

static void display_textwin_addressmap_window_shadow(textwin_window_t * tw)
{
	const unsigned long shadow_color_addr = (unsigned long)(&textwin_shadowcolor);
	unsigned int y, sx, sy;

	if(!tw->xsize)
		return;

	sy = min(tw->ysize, (textscreen_maxy - 1 - tw->ypos - TEXTWIN_WINDOW_SHADOW_Y));
	sx = min(tw->xsize, (textscreen_maxx - 1 - tw->xpos - TEXTWIN_WINDOW_SHADOW_X));

	for(y = 0; y < sy; y++) {
		unsigned long *map =
			textwin_address_map + (tw->ypos + y + TEXTWIN_WINDOW_SHADOW_Y) * textscreen_maxx * TEXTSCREEN_BYTES_PER_CHAR + (tw->xpos + TEXTWIN_WINDOW_SHADOW_X) * TEXTSCREEN_BYTES_PER_CHAR;
		unsigned int x = sx;
		do {
			map[1] = shadow_color_addr;
			map += 2;
		} while(--x);
	}
}

static void display_textwin_addressmap_window_add(textwin_window_t * tw)
{
	unsigned char *winmem;
	unsigned int y;

	if(!tw->xsize)
		return;

	display_textwin_addressmap_window_shadow(tw);

	winmem = tw->winmem;

	for(y = 0; y < tw->ysize; y++) {
		unsigned long *map = textwin_address_map + (tw->ypos + y) * textscreen_maxx * TEXTSCREEN_BYTES_PER_CHAR + tw->xpos * TEXTSCREEN_BYTES_PER_CHAR;
		unsigned int x = tw->xsize * TEXTSCREEN_BYTES_PER_CHAR;
		do {
			*map++ = (unsigned long)winmem++;
		} while(--x);
	}
}

//-------------------------------------------------------------------------
#ifdef MPXPLAY_WIN32
static void display_textwin_desktop_refresh(void)
{
	if(textwin_address_map && textwin_vidmem_size) {
		unsigned int x, y;
		unsigned long *map = textwin_address_map;
		unsigned short *pta = textwin_vidmem_tmp_attrib;
		char *ptt = textwin_vidmem_tmp_text;

		for(y = 0; y < textscreen_maxy; y++) {
			int beginx = -1, endx = -1;
			for(x = 0; x < textscreen_maxx; x++) {
				char text = *((char *)map[0]);
				unsigned short attrib = *((unsigned char *)map[1]);
				if((*pta != attrib) || (*ptt != text)) {
					if(beginx < 0)
						beginx = x;
					else
						endx = x;
					*pta = attrib;
					*ptt = text;
				}
				pta++;
				ptt++;
				map += 2;
			}
			if(beginx >= 0) {
				unsigned int pos = (y * textscreen_maxx) + beginx;
				if(endx < 0)
					endx = beginx;
				pds_textdisplay_consolevidmem_xywrite(beginx, y, textwin_vidmem_tmp_attrib + pos, textwin_vidmem_tmp_text + pos, (endx - beginx + 1));
			}
		}
	}
}

#else

static void display_textwin_desktop_refresh(void)
{
	if(textwin_address_map && textwin_vidmem_size) {
		unsigned int i = textscreen_memory_size;
		unsigned long *map = textwin_address_map;
		unsigned char *pt = (unsigned char *)textscreen_target_address;
		do {
			unsigned char byte = *((unsigned char *)*map);
			*pt = byte;
			map++;
			pt++;
		} while(--i);
	}
}
#endif

static void display_textwin_desktop_close(void)
{
#ifdef MPXPLAY_LINK_DLLLOAD
	mpxplay_timer_executefunc(display_visualplugin_stop);	// !!! hack
#endif
	if(ts_linear_address_save) {
		textscreen_linear_address = ts_linear_address_save;
		ts_linear_address_save = 0;
		if(textwin_vidmem_address) {
#ifdef MPXPLAY_WIN32
			pds_textdisplay_consolevidmem_separate(textwin_vidmem_tmp_attrib, textwin_vidmem_tmp_text, (char *)textwin_vidmem_address, textscreen_memory_size);
			pds_textdisplay_consolevidmem_write(textwin_vidmem_tmp_attrib, textwin_vidmem_tmp_text, textscreen_memory_size / TEXTSCREEN_BYTES_PER_CHAR);
#else
			pds_memcpy((void *)textscreen_linear_address, (void *)textwin_vidmem_address, textscreen_memory_size);
#endif
		}
	}
	mpxplay_timer_deletefunc(display_textwin_desktop_refresh, NULL);
}

static unsigned int display_textwin_desktop_init(void)
{
	display_textwin_desktop_close();

	if(!display_textwin_init())
		return 0;
	textscreen_target_address = textscreen_linear_address;
	ts_linear_address_save = textscreen_linear_address;
	textscreen_linear_address = textwin_vidmem_address;

	display_textwin_addressmap_reset();
	draw_mouse_restorelastmousec();

#ifdef MPXPLAY_WIN32
	pds_textdisplay_consolevidmem_read(textwin_vidmem_tmp_attrib, textwin_vidmem_tmp_text, textscreen_memory_size / TEXTSCREEN_BYTES_PER_CHAR);
	pds_textdisplay_consolevidmem_merge(textwin_vidmem_tmp_attrib, textwin_vidmem_tmp_text, (char *)textwin_vidmem_address, textscreen_memory_size);
	pds_memset(textwin_vidmem_tmp_text, 1, textscreen_memory_size / TEXTSCREEN_BYTES_PER_CHAR);
#else
	pds_memcpy((void *)textwin_vidmem_address, (void *)ts_linear_address_save, textscreen_memory_size);
#endif

	textwin_keycode = 0;

	mpxplay_timer_addfunc(display_textwin_desktop_refresh, NULL, MPXPLAY_TIMERTYPE_REPEAT, mpxplay_timer_secs_to_counternum(1) / 19);

	return 1;
}

static void *display_textwin_keyboard_topfunc(unsigned int extkey, struct mainvars *mvp)
{
	textwin_keycode = extkey;
	if(extkey == KEY_ESC)
		return NULL;
	return (&display_textwin_keyboard_topfunc);
}

//-------------------------------------------------------------------------
static void display_textwin_clrscr(textwin_window_t * tw, unsigned int color)
{
	unsigned char *winmem = tw->winmem;
	unsigned int winsize = tw->xsize * tw->ysize;

	do {
		winmem[0] = ' ';
		winmem[1] = color;
		winmem += 2;
	} while(--winsize);
}

static void display_textwin_charxy(textwin_window_t * tw, unsigned int color, unsigned int x, unsigned int y, char outchar)
{
	unsigned char *winmem;

	if((y >= tw->ysize) || (x >= tw->xsize))
		return;

	winmem = tw->winmem + (y * tw->xsize + x) * TEXTSCREEN_BYTES_PER_CHAR;

	winmem[0] = outchar;
	winmem[1] = color;
}

static void display_textwin_textxy(textwin_window_t * tw, unsigned int color, unsigned int x, unsigned int y, char *text)
{
	unsigned char *winmem, *winend;

	if((y >= tw->ysize) || (x >= tw->xsize))
		return;

	winmem = tw->winmem + (y * tw->xsize + x) * TEXTSCREEN_BYTES_PER_CHAR;
	winend = tw->winmem + tw->ysize * tw->xsize * TEXTSCREEN_BYTES_PER_CHAR;

	while((text[0] != 0) && (winmem < winend)) {
		winmem[0] = text[0];
		winmem[1] = color;
		winmem += 2;
		text++;
	}
}

static void display_textwin_textxyn(textwin_window_t * tw, unsigned int color, unsigned int x, unsigned int y, unsigned int maxlen, char *text)
{
	unsigned char *winmem, *winend;

	if((y >= tw->ysize) || (x >= tw->xsize))
		return;

	winmem = tw->winmem + (y * tw->xsize + x) * TEXTSCREEN_BYTES_PER_CHAR;
	winend = tw->winmem + tw->ysize * tw->xsize * TEXTSCREEN_BYTES_PER_CHAR;

	while((text[0] != 0) && maxlen && (winmem < winend)) {
		winmem[0] = text[0];
		winmem[1] = color;
		winmem += 2;
		text++;
		maxlen--;
	}
}

static void display_textwin_textxye(textwin_window_t * tw, unsigned int color, unsigned int x, unsigned int y, unsigned int maxlen, char *text)
{
	unsigned char *winmem, *winend;

	if((y >= tw->ysize) || (x >= tw->xsize))
		return;

	winmem = tw->winmem + (y * tw->xsize + x) * TEXTSCREEN_BYTES_PER_CHAR;
	winend = tw->winmem + tw->ysize * tw->xsize * TEXTSCREEN_BYTES_PER_CHAR;

	while(maxlen && (winmem < winend)) {
		if(text[0] != 0) {
			winmem[0] = text[0];
			text++;
		} else {
			winmem[0] = ' ';
		}
		winmem[1] = color;
		winmem += 2;
		maxlen--;
	}
}

//-------------------------------------------------------------------------
void display_textwin_draw_window_headtext(void *tw_update, char *headtext)
{
	textwin_window_t *tw = tw_update;
	unsigned int x, len;
	if(!tw)
		return;
	if(headtext) {
		len = pds_strlen(headtext);
		for(x = TEXTWIN_BORDERSIZE_X; x < (tw->xsize - TEXTWIN_BORDERSIZE_X); x++)
			display_textwin_charxy(tw, tw->basecolor, x, TEXTWIN_BORDERSIZE_Y - 1, twborderchars[0]);
		display_textwin_textxy(tw, tw->basecolor, (tw->xsize - len) / 2, TEXTWIN_BORDERSIZE_Y - 1, headtext);
	}
}

static void display_textwin_draw_window_border(textwin_window_t * tw, char *headtext)
{
	const unsigned int color = tw->basecolor;
	unsigned int x, y, len;

	display_textwin_clrscr(tw, color);

	for(x = TEXTWIN_BORDERSIZE_X; x < (tw->xsize - TEXTWIN_BORDERSIZE_X); x++) {
		display_textwin_charxy(tw, color, x, TEXTWIN_BORDERSIZE_Y - 1, twborderchars[0]);
		display_textwin_charxy(tw, color, x, tw->ysize - TEXTWIN_BORDERSIZE_Y, twborderchars[0]);
	}
	for(y = TEXTWIN_BORDERSIZE_Y; y < (tw->ysize - TEXTWIN_BORDERSIZE_Y); y++) {
		display_textwin_charxy(tw, color, TEXTWIN_BORDERSIZE_X - 1, y, twborderchars[1]);
		display_textwin_charxy(tw, color, tw->xsize - TEXTWIN_BORDERSIZE_X, y, twborderchars[1]);
	}
	display_textwin_charxy(tw, color, TEXTWIN_BORDERSIZE_X - 1, TEXTWIN_BORDERSIZE_Y - 1, twborderchars[2]);
	display_textwin_charxy(tw, color, tw->xsize - TEXTWIN_BORDERSIZE_X, TEXTWIN_BORDERSIZE_Y - 1, twborderchars[3]);
	display_textwin_charxy(tw, color, TEXTWIN_BORDERSIZE_X - 1, tw->ysize - TEXTWIN_BORDERSIZE_Y, twborderchars[4]);
	display_textwin_charxy(tw, color, tw->xsize - TEXTWIN_BORDERSIZE_X, tw->ysize - TEXTWIN_BORDERSIZE_Y, twborderchars[5]);

	if(headtext) {
		len = pds_strlen(headtext);
		display_textwin_textxy(tw, color, (tw->xsize - len) / 2, TEXTWIN_BORDERSIZE_Y - 1, headtext);
	}
}

static void display_textwin_draw_window_items(textwin_window_t * tw)
{
	textwin_item_t *it = tw->item_first;
	unsigned int ic;

	if(!it)
		return;
	ic = 0;
	do {
		unsigned int color, x, y;

		color = tw->basecolor;
		y = it->ypos + TEXTWIN_BORDERSIZE_Y;
		x = it->xpos + TEXTWIN_BORDERSIZE_X;

		switch (it->type) {
		case TEXTWIN_LINETYPE_MSG_STATIC:
		case TEXTWIN_LINETYPE_MSG_ALLOCATED:
			{
				char *text = (char *)it->data;
				if(text) {
					unsigned int len = it->xsize;
					if(x + len > (tw->xsize - TEXTWIN_BORDERSIZE_X))
						len = tw->xsize - TEXTWIN_BORDERSIZE_X - x;
					display_textwin_textxyn(tw, color, x, y, len, text);
				}
			}
			break;
		case TEXTWIN_LINETYPE_MSG_EDIT:
			//color=tw->buttoncolor;
			color = (CLB_TEXTWIN_BUTTON << 4) | CL_TEXTWIN_BUTTON;
			if(ic == tw->item_selected) {
				textwin_editline_t *el = it->data;
				char *text;
				if(el->line_data_pos < el->line_data_begin)
					el->line_data_begin = el->line_data_pos;
				if(el->line_data_pos >= (el->line_data_begin + it->xsize))
					el->line_data_begin = el->line_data_pos - it->xsize;
				text = el->strbuf + el->line_data_begin;
				display_textwin_textxye(tw, color, x, y, it->xsize, text);
				x = tw->xpos + TEXTWIN_BORDERSIZE_X + it->xpos + el->line_data_pos - el->line_data_begin;
				y = tw->ypos + TEXTWIN_BORDERSIZE_Y + it->ypos;
				pds_textdisplay_setcursor_position(x, y);
			} else {
				textwin_editline_t *el = it->data;
				char *text = el->strbuf;
				display_textwin_textxye(tw, color, x, y, it->xsize, text);
			}
			break;
		case TEXTWIN_LINETYPE_BUTTON:
			if(ic == tw->item_selected)
				color = tw->buttoncolor;
			{
				display_textwin_button_t *bt = it->data;
				if(bt->text && bt->text[0] && (x + it->xsize) < (tw->xsize - TEXTWIN_BORDERSIZE_X))
					display_textwin_textxy(tw, color, x, y, bt->text);
			}
			break;
		case TEXTWIN_LINETYPE_SWITCH:
			{
				textwin_switchline_t *sl = it->data;
				if(funcbit_test(*(sl->switchvar), sl->switchbit))
					display_textwin_textxy(tw, color, x, y, "[x] ");
				else
					display_textwin_textxy(tw, color, x, y, "[ ] ");
				display_textwin_textxy(tw, color, x + sizeof("[x] ") - 1, y, sl->switchtext);
				if(ic == tw->item_selected) {
					x = tw->xpos + TEXTWIN_BORDERSIZE_X + it->xpos + 1;
					y = tw->ypos + TEXTWIN_BORDERSIZE_Y + it->ypos;
					pds_textdisplay_setcursor_position(x, y);
				}
			}
			break;
		case TEXTWIN_LINETYPE_SEPARATOR:
			for(x = TEXTWIN_BORDERSIZE_X; x < (tw->xsize - TEXTWIN_BORDERSIZE_X); x++)
				display_textwin_charxy(tw, tw->basecolor, x, y, twborderchars[11]);
			display_textwin_charxy(tw, tw->basecolor, TEXTWIN_BORDERSIZE_X - 1, y, twborderchars[6]);
			display_textwin_charxy(tw, tw->basecolor, tw->xsize - TEXTWIN_BORDERSIZE_X, y, twborderchars[7]);
			break;
		}
		it++;
		ic++;
	} while(ic < tw->nb_items);
}

//-----------------------------------------------------------------------

static textwin_window_t *display_textwin_alloc_window(void)
{
	textwin_window_t *tw;
	tw = (textwin_window_t *) calloc(1, sizeof(textwin_window_t));
	if(tw)
		textwin_windows[0] = tw;	// !!!
	return tw;
}

static void display_textwin_free_window(textwin_window_t * tw)
{
	if(tw) {
		if(textwin_windows[0] == tw)
			textwin_windows[0] = NULL;
		if(tw->winmem)
			free(tw->winmem);
		if(tw->item_first)
			free(tw->item_first);
		free(tw);
	}
}

static void display_textwin_close_window(textwin_window_t * tw)
{
	display_textwin_reset_window(tw);
	display_textwin_free_window(tw);
	display_textwin_desktop_close();
}

static unsigned int display_textwin_init_window(textwin_window_t * tw, unsigned int flags, unsigned int ysize, unsigned int maxtextlen)
{
	unsigned int xsize;

	if(!tw->basecolor)
		tw->basecolor = (CL_TEXTWIN_BASE | (CLB_TEXTWIN_BASE << 4));

	ysize += TEXTWIN_BORDERSIZE_Y * 2;

	if(ysize < TEXTWIN_WINDOWSIZE_MIN_Y)
		ysize = TEXTWIN_WINDOWSIZE_MIN_Y;
	if(ysize > TEXTWIN_WINDOWSIZE_MAX_Y)
		ysize = TEXTWIN_WINDOWSIZE_MAX_Y;

	tw->ypos = (textscreen_maxy - ysize) / 3;

	xsize = maxtextlen;
	if(flags & TEXTWIN_FLAG_MSGCENTERALIGN)
		xsize += 2 * TEXTWIN_BORDERSIZE_X + 2;
	else
		xsize += 2 * TEXTWIN_BORDERSIZE_X;

	if(xsize < TEXTWIN_WINDOWSIZE_MIN_X && !(tw->flags & TEXTWIN_FLAG_NOWINMINSIZE))
		xsize = TEXTWIN_WINDOWSIZE_MIN_X;
	if(xsize > TEXTWIN_WINDOWSIZE_MAX_X)
		xsize = TEXTWIN_WINDOWSIZE_MAX_X;

	tw->xpos = (textscreen_maxx - xsize) / 2;

	if(!tw->winmem || (xsize != tw->xsize) || (ysize != tw->ysize)) {
		if(tw->winmem)
			free(tw->winmem);
		tw->winmem = malloc(xsize * ysize * TEXTSCREEN_BYTES_PER_CHAR);
		if(!tw->winmem)
			return 0;
		tw->xsize = xsize;
		tw->ysize = ysize;
		display_textwin_addressmap_reset();
	}

	display_textwin_addressmap_window_add(tw);

	return 1;
}

static void display_textwin_reset_window(textwin_window_t * tw)
{
	textwin_item_t *it;
	textwin_switchline_t *sl;
	unsigned int ic;

	if(!tw)
		return;

	if(tw->item_selected >= 0) {
		it = tw->item_first + tw->item_selected;
		switch (it->type) {
		case TEXTWIN_LINETYPE_MSG_EDIT:
		case TEXTWIN_LINETYPE_SWITCH:
			pds_textdisplay_setcursor_position(0, 0);
			pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_HIDDEN);	// disable cursor
		}
	}

	it = tw->item_first;
	ic = tw->nb_items;
	if(it && ic) {
		do {
			if(it->data) {
				switch (it->type) {
				case TEXTWIN_LINETYPE_SWITCH:
					sl = it->data;
					if(sl->switchtext)
						free(sl->switchtext);
				case TEXTWIN_LINETYPE_MSG_ALLOCATED:
				case TEXTWIN_LINETYPE_MSG_EDIT:
					free(it->data);
				}
			}
			it++;
		} while(--ic);
		pds_memset((void *)tw->item_first, 0, tw->nb_items * sizeof(struct textwin_item_t));
	}

	tw->nb_items = tw->nb_buttons = tw->item_pos_y = 0;
	tw->item_selected = -1;
}

//-------------------------------------------------------------------------
static void display_textwin_execute_window_confirm(unsigned int keycode, void *buttonfunc, void *buttondata)
{
	switch (keycode) {
	case 0x1579:				// 'y'
	case 0x1559:				// 'Y'
	case KEY_ENTER1:
	case KEY_ENTER2:
		if(buttondata)			// !!!
			((execfunc_data_t) buttonfunc) (buttondata);	// different buttonfunc arguments
		else					//
			((execfunc_nodata_t) buttonfunc) ();	//
		break;
	}
}

//-------------------------------------------------------------------------

static unsigned int textwin_item_step(textwin_window_t * tw, int step, unsigned int modify_y)
{
	int ic = tw->item_selected;
	display_textwin_button_t *bt;
	textwin_item_t *it = tw->item_first + ic;
	textwin_editline_t *el;
	unsigned int retry = 0, last_y = it->ypos, last_type = it->type;

	do {
		unsigned int ok;

		ic += step;
		if(ic < 0) {
			if(retry) {
				ic = tw->item_selected;
				break;
			}
			ic = tw->nb_items - 1;
			retry++;
		} else if(ic >= tw->nb_items) {
			if(retry) {
				ic = tw->item_selected;
				break;
			}
			ic = 0;
			retry++;
		}

		it = tw->item_first + ic;
		ok = 0;
		switch (modify_y) {
		case 0:
			if(it->ypos == last_y)
				ok = 1;
			break;				// left/right arrows
		case 1:
			ok = 1;
			break;				// tab
		case 2:
			if(it->ypos != last_y)
				ok = 1;
			break;				// up/down arrows
		}

		if(ok) {
			switch (it->type) {
			case TEXTWIN_LINETYPE_MSG_EDIT:
				el = it->data;
				if(el) {
					if((tw->locflags & TEXTWIN_LOCFLAG_EDITLINE_OVERWRITE) || (el->flags & TEXTWIN_EDITFLAG_OVERWRITE))
						pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_FULLBOX);	// enable cursor
					else
						pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_NORMAL);	// enable cursor
					if(modify_y == 0 && ok && step > 0 && ic != tw->item_selected)
						el->line_data_pos = 0;
				}
				goto end_step;
			case TEXTWIN_LINETYPE_BUTTON:
				bt = it->data;
				if(bt->text && bt->text[0] && bt->extkey) {
					if(last_type != it->type) {
						pds_textdisplay_setcursor_position(0, 0);
						pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_HIDDEN);	// disable cursor
					}
					goto end_step;
				}
				break;
			case TEXTWIN_LINETYPE_SWITCH:
				pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_NORMAL);	// enable cursor
				goto end_step;
			}
		}
	} while(step != 0);			// else endless loop if step==0 (called from textwin_line_edit)
  end_step:
	return ic;
}

static unsigned int textwin_item_select(textwin_window_t * tw, unsigned int keycode)
{
	textwin_item_t *it;
	if(tw->item_selected < 0)
		return 0;
	switch (keycode) {
	case 0x4800:				// up arrows
	case 0x48e0:
		tw->item_selected = textwin_item_step(tw, -1, 2);
		goto it_disp;
	case 0x5000:				// down arrows
	case 0x50e0:
		tw->item_selected = textwin_item_step(tw, +1, 2);
		goto it_disp;
	case 0x0f09:
		tw->item_selected = textwin_item_step(tw, +1, 1);
		goto it_disp;			// tab
	}
	it = tw->item_first + tw->item_selected;
	switch (it->type) {
	case TEXTWIN_LINETYPE_MSG_EDIT:
	case TEXTWIN_LINETYPE_SWITCH:
		return 0;
	}
	switch (keycode) {
	case 0x4b00:				// left arrows
	case 0x4be0:
		tw->item_selected = textwin_item_step(tw, -1, 0);
		break;
	case 0x4d00:				// right arrows
	case 0x4de0:
		tw->item_selected = textwin_item_step(tw, +1, 0);
		break;
	default:
		return 0;
	}
  it_disp:
	display_textwin_draw_window_items(tw);
	return 1;
}

static unsigned int textwin_line_edit(textwin_window_t * tw, unsigned int keycode)
{
	textwin_item_t *it;
	textwin_editline_t *el;

	if(!tw->item_first || (tw->item_selected < 0))
		return 0;
	it = tw->item_first + tw->item_selected;
	el = it->data;
	if(it->type == TEXTWIN_LINETYPE_MSG_EDIT) {
		char *strbuf = el->strbuf;
		char typed;
		switch (keycode) {
		case 0x5200:			// ins keys
		case 0x52e0:
			if((el->flags & TEXTWIN_EDITLINEFLAG_FIRSTKEY) && !(el->flags & TEXTWIN_EDITFLAG_OVERWRITE))
				funcbit_disable(el->flags, TEXTWIN_EDITLINEFLAG_FIRSTKEY);	// !!! first Ins disables auto-clear
			else {
				funcbit_inverse(tw->locflags, TEXTWIN_LOCFLAG_EDITLINE_OVERWRITE);
				textwin_item_step(tw, 0, 0);
			}
			goto le_disp;
		case 0x4b00:			// left arrows
		case 0x4be0:
			if(el->line_data_pos > 0)
				el->line_data_pos--;
			else {
				int is = textwin_item_step(tw, -1, 0);	// skip to prev edit-item
				if(is >= 0 && is != tw->item_selected) {
					tw->item_selected = is;
					it = tw->item_first + is;
					el = it->data;
				}
			}
			goto le_disp;
		case 0x4d00:			// right arrows
		case 0x4de0:
			el->line_data_pos++;
			if(el->line_data_pos > el->line_data_len) {
				int is;
				el->line_data_pos = el->line_data_len;
				is = textwin_item_step(tw, +1, 0);	// skip to next edit-item
				if(is >= 0 && is != tw->item_selected) {
					tw->item_selected = is;
					it = tw->item_first + is;
					el = it->data;
				}
			}
			goto le_disp;
		case 0x4700:			// home keys
		case 0x47e0:
			el->line_data_pos = 0;
			goto le_disp;
		case 0x4f00:			// end keys
		case 0x4fe0:
			el->line_data_pos = el->line_data_len;
			goto le_disp;
		case 0x0e08:
			if(el->line_data_pos && el->line_data_len) {	// backspace (clear back)
				pds_memcpy(&strbuf[el->line_data_pos - 1], &strbuf[el->line_data_pos], (el->line_data_len - el->line_data_pos));
				if(strbuf[el->line_data_len] == 0) {
					el->line_data_len--;
					strbuf[el->line_data_len] = 0;
				} else
					strbuf[el->line_data_len - 1] = ' ';
				el->line_data_pos--;
				if(el->line_data_begin && ((el->line_data_len - el->line_data_begin) < it->xsize))
					el->line_data_begin--;
			}
			goto le_disp;
		case 0x5300:			// del keys (clear at cursor)
		case 0x53e0:
			if(el->line_data_len && (el->line_data_pos < el->line_data_len)) {
				pds_memcpy(&strbuf[el->line_data_pos], &strbuf[el->line_data_pos + 1], (el->line_data_len - el->line_data_pos));
				if(strbuf[el->line_data_len] == 0) {
					el->line_data_len--;
					strbuf[el->line_data_len] = 0;
				} else
					strbuf[el->line_data_len - 1] = ' ';
			}
			goto le_disp;
		}
		//typed=newfunc_keyboard_extkey_to_char(keycode);
		//if(typed!=0xff){
		typed = keycode & 0xff;
		if((typed >= 0x20) && (typed < 0xff)) {	// ??? usually good
			if((el->flags & TEXTWIN_EDITFLAG_NUMERIC) && ((typed < '0') || (typed > '9')))
				goto le_nodisp;
			if(el->flags & TEXTWIN_EDITLINEFLAG_FIRSTKEY) {	// !!! clears line if the first pressed keycode is a character
				strbuf[0] = 0;
				el->line_data_len = el->line_data_pos = 0;
			}
			if((tw->locflags & TEXTWIN_LOCFLAG_EDITLINE_OVERWRITE) || (el->flags & TEXTWIN_EDITFLAG_OVERWRITE)) {
				if(el->line_data_pos >= el->maxdatalen) {
					if((el->flags & TEXTWIN_EDITFLAG_OVERWRITE) && (el->maxdatalen == it->xsize)) {	// !!! auto skip to next edit-item at end of small-window
						unsigned int last_type = it->type;
						int is;
						is = textwin_item_step(tw, +1, 0);	// skip to next edit-item
						if(is >= 0 && is > tw->item_selected && (((textwin_item_t *) (tw->item_first + is))->type == last_type)) {
							tw->item_selected = is;
							it = tw->item_first + is;
							el = it->data;
						} else
							goto le_nodisp;
					} else
						goto le_nodisp;
				}
				if(el->line_data_pos < el->maxdatalen) {	// ???
					el->strbuf[el->line_data_pos] = typed;
					el->line_data_pos++;
					if(el->line_data_len < el->line_data_pos)
						el->line_data_len = el->line_data_pos;
				}
			} else {			// inserting
				if(el->line_data_len < el->maxdatalen) {
					pds_strmove(&strbuf[el->line_data_pos + 1], &strbuf[el->line_data_pos]);
					strbuf[el->line_data_pos] = typed;
					el->line_data_pos++;
					el->line_data_len++;
				}
			}
			goto le_disp;
		}
	}

  le_nodisp:

	return 0;

  le_disp:
	funcbit_disable(el->flags, TEXTWIN_EDITLINEFLAG_FIRSTKEY);
	display_textwin_draw_window_items(tw);
	return 1;
}

static unsigned int textwin_switch_edit(textwin_window_t * tw, unsigned int keycode)
{
	textwin_item_t *it;
	textwin_switchline_t *sl;

	if(!tw->item_first || (tw->item_selected < 0))
		return 0;
	it = tw->item_first + tw->item_selected;
	if(it->type != TEXTWIN_LINETYPE_SWITCH)
		return 0;
	switch (keycode) {
	case 0x3920:				// switch on/off by space
		sl = it->data;
		funcbit_inverse(*(sl->switchvar), sl->switchbit);
		display_textwin_draw_window_items(tw);
		return 1;
	}
	return 0;
}

static void display_textwin_execute_window_items(textwin_window_t * tw)
{
	unsigned int keycode = textwin_keycode;
	textwin_item_t *it;
	void *buttonfunc, *buttondata;
	unsigned int ic, flags;

	if(!keycode)
		return;

	textwin_keycode = 0;

	if(textwin_item_select(tw, keycode))
		return;
	if(textwin_line_edit(tw, keycode))
		return;
	if(textwin_switch_edit(tw, keycode))
		return;

	if(tw->nb_buttons) {
		switch (keycode) {
		case KEY_ENTER1:
		case KEY_ENTER2:
			if(tw->item_selected >= 0) {	// if selected item is a button, execute it
				it = tw->item_first + tw->item_selected;
				if(it->type == TEXTWIN_LINETYPE_BUTTON) {
					display_textwin_button_t *bt = it->data;
					if(bt->text && bt->text[0] && bt->extkey) {
						keycode = bt->extkey;
						goto do_execute;
					}
				}
			}
			it = tw->item_first;	// else search first button
			ic = 0;
			do {
				if(it->type == TEXTWIN_LINETYPE_BUTTON) {
					display_textwin_button_t *bt = it->data;
					if(bt->extkey) {	// ??? bt->extkey only
						keycode = bt->extkey;
						goto do_execute;
					}
				}
				it++;
				ic++;
			} while(ic < tw->nb_items);
			break;
		default:				// select button with keycode
			it = tw->item_first;
			ic = 0;
			do {
				if(it->type == TEXTWIN_LINETYPE_BUTTON) {
					display_textwin_button_t *bt = it->data;
					if(bt->extkey == keycode) {
						if(bt->text && bt->text[0])
							tw->item_selected = textwin_item_step(tw, (int)(it - tw->item_first), 1);
						goto do_execute;
					}
				}
				it++;
				ic++;
			} while(ic < tw->nb_items);
		}
		return;
	}

  do_execute:
	buttonfunc = tw->buttonhandlerfunc;
	buttondata = tw->buttonhandlerdata;
	flags = tw->flags;

	if(!(flags & TEXTWIN_FLAG_DONTCLOSE) && (tw->nb_buttons || (flags & TEXTWIN_FLAG_KEYCLOSEMSG)))
		display_textwin_closewindow_items(tw);

	if(buttonfunc)
		if(flags & TEXTWIN_FLAG_CONFIRM)
			display_textwin_execute_window_confirm(keycode, buttonfunc, buttondata);
		else
			((buttonhand_func_t) buttonfunc) (buttondata, keycode);
}

//--------------------------------------------------------------------------
unsigned int display_textwin_handle_mousepos(unsigned int x, unsigned int y, unsigned int click)
{
	textwin_window_t *tw = textwin_windows[0];
	textwin_item_t *it;
	textwin_editline_t *el;
	display_textwin_button_t *bt;
	textwin_switchline_t *sl;
	unsigned int i, prev_item;

	if(!tw)						// no window
		return 0;
	if((x < tw->xpos) || (x >= (tw->xpos + tw->xsize)) || (y < tw->ypos) || (y >= (tw->ypos + tw->ysize)))	// out of window
		return 1;
	if(!click)
		return 1;
	if((x < (tw->xpos + TEXTWIN_BORDERSIZE_X)) || (x >= (tw->xpos + tw->xsize - TEXTWIN_BORDERSIZE_X)))	// in border
		return 1;
	if((y < tw->ypos + TEXTWIN_BORDERSIZE_Y) || (y >= (tw->ypos + tw->ysize - TEXTWIN_BORDERSIZE_Y)))
		return 1;
	if(tw->flags & TEXTWIN_FLAG_KEYCLOSEMSG) {
		pds_pushkey(KEY_ENTER1);
		return 1;
	}

	x -= tw->xpos + TEXTWIN_BORDERSIZE_X;	// relative in window
	y -= tw->ypos + TEXTWIN_BORDERSIZE_Y;

	it = tw->item_first;
	if(!it || !tw->nb_items)
		return 0;

	for(i = 0; i < tw->nb_items; i++, it++) {
		if((x >= it->xpos) && (y == it->ypos) && (x < (it->xpos + it->xsize))) {
			if(it->type & TEXTWIN_LINETYPES_ACTIVE) {
				prev_item = tw->item_selected;
				tw->item_selected = textwin_item_step(tw, (int)i - (int)tw->item_selected, 1);
				it = tw->item_first + tw->item_selected;	// ???
				switch (it->type) {
				case TEXTWIN_LINETYPE_MSG_EDIT:
					el = it->data;
					if(funcbit_test(el->flags, TEXTWIN_EDITLINEFLAG_SECMOUSEPOS) || (prev_item == tw->item_selected)) {
						el->line_data_pos = x - it->xpos;
						if(el->line_data_pos > el->line_data_len)
							el->line_data_pos = el->line_data_len;
						funcbit_disable(el->flags, TEXTWIN_EDITLINEFLAG_FIRSTKEY);
					}
					funcbit_enable(el->flags, TEXTWIN_EDITLINEFLAG_SECMOUSEPOS);
					break;
				case TEXTWIN_LINETYPE_BUTTON:
					bt = it->data;
					pds_pushkey(bt->extkey);
					break;
				case TEXTWIN_LINETYPE_SWITCH:
					if((x >= it->xpos) && (x <= (it->xpos + 2))) {
						sl = it->data;
						funcbit_inverse(*(sl->switchvar), sl->switchbit);
					}
					break;
				}
				display_textwin_draw_window_items(tw);
			}
			break;
		}
	}
	return 1;
}

//--------------------------------------------------------------------------
static unsigned int textwin_itemspace_alloc(textwin_window_t * tw, unsigned int new_items)
{
	if((tw->nb_items + new_items) >= tw->nb_allocated_items) {
		unsigned int alloc_itemnum = tw->nb_allocated_items + new_items + TEXTWIN_ITEMNUM_ALLOC;
		textwin_item_t *it = (textwin_item_t *) calloc(alloc_itemnum, sizeof(struct textwin_item_t));
		if(!it)
			return 0;
		if(tw->item_first) {
			if(tw->nb_items)
				pds_memcpy((void *)it, (void *)tw->item_first, (tw->nb_items * sizeof(struct textwin_item_t)));
			free(tw->item_first);
		}
		tw->item_first = it;
		tw->nb_allocated_items = alloc_itemnum;
	}
	return 1;
}

static unsigned int textwin_check_ypos(textwin_window_t * tw, unsigned int ypos)
{
	ypos += TEXTWIN_BORDERSIZE_Y * 2;
	if(ypos >= TEXTWIN_WINDOWSIZE_MAX_Y)
		return 0;
	return 1;
}

static void textwin_calculate_window_lines(textwin_window_t * tw, unsigned int *ysize, unsigned int *xsize)
{
	textwin_item_t *it = tw->item_first;
	unsigned int itemcount, maxy = 0, maxx = 0;

	if(it && tw->nb_items) {
		itemcount = tw->nb_items;
		do {
			maxy = max(maxy, it->ypos + 1);
			maxx = max(maxx, it->xpos + it->xsize);
			it++;
		} while(--itemcount);
	}
	*ysize = maxy;
	*xsize = maxx;
}

static unsigned int textwin_get_itemline_len(textwin_window_t * tw, textwin_item_t * it)
{
	unsigned int ic = it - tw->item_first, ypos = it->ypos, xsize = 0;
	unsigned int align_flags = it->flags & TEXTWIN_FLAGS_MSG_ALIGN;
	unsigned int begin_type = it->type, begin_flags = it->flags;
	do {
		xsize = max(xsize, it->xpos + it->xsize);
		it++;
		ic++;
	} while((ic < tw->nb_items)
			&& ((ypos == it->ypos) || (it->type == TEXTWIN_LINETYPE_BUTTON && it->type == begin_type && it->flags == begin_flags))	// horizontal || vertical buttons
			&& ((it->flags & TEXTWIN_FLAGS_MSG_ALIGN) == align_flags));
	return xsize;
}

static unsigned int textwin_set_itemline_xpos(textwin_window_t * tw, textwin_item_t * it, unsigned int xpos_base)
{
	unsigned int ic = it - tw->item_first, ic_begin = ic, ypos = it->ypos;
	unsigned int align_flags = it->flags & TEXTWIN_FLAGS_MSG_ALIGN;
	unsigned int begin_type = it->type, begin_flags = it->flags;
	do {
		it->xpos += xpos_base;
		it++;
		ic++;
	} while((ic < tw->nb_items)
			&& ((ypos == it->ypos) || (it->type == TEXTWIN_LINETYPE_BUTTON && it->type == begin_type && it->flags == begin_flags))
			&& ((it->flags & TEXTWIN_FLAGS_MSG_ALIGN) == align_flags));
	return (ic - ic_begin);
}

static unsigned int textwin_itemline_align(textwin_window_t * tw, textwin_item_t * it)
{
	if(it->flags & (TEXTWIN_FLAG_MSGCENTERALIGN | TEXTWIN_FLAG_MSGRIGHTALIGN)) {
		unsigned int xpos_base, len = textwin_get_itemline_len(tw, it);
		xpos_base = tw->xsize - 2 * TEXTWIN_BORDERSIZE_X;
		if(xpos_base >= len)
			xpos_base -= len;
		else
			xpos_base = 0;
		if(it->flags & TEXTWIN_FLAG_MSGCENTERALIGN)
			xpos_base /= 2;
		return textwin_set_itemline_xpos(tw, it, xpos_base);
	}
	return 1;					// skip item
}

//-------------------------------------------------------------------------

void *display_textwin_allocwindow_items(void *tw_update, unsigned int flags, char *headtext, void *buttonhandfunc, void *buttonhanddata)
{
	textwin_window_t *tw = tw_update;

	if(tw)
		display_textwin_reset_window(tw);
	else {
		if(!display_textwin_desktop_init())
			goto err_out_awi;
		tw = display_textwin_alloc_window();
		if(!tw)
			goto err_out_awi;
	}

	tw->flags = flags;
	tw->headtext = headtext;
	tw->buttonhandlerfunc = buttonhandfunc;
	tw->buttonhandlerdata = buttonhanddata;
	tw->item_selected = -1;

	if(!textwin_itemspace_alloc(tw, 0))
		goto err_out_awi;

	return tw;

  err_out_awi:
	display_textwin_close_window(tw);
	return NULL;
}

void *display_textwin_openwindow_items(void *tw_update, unsigned int xpos, unsigned int ypos, unsigned int minxsize)
{
	textwin_window_t *tw = tw_update;
	textwin_item_t *it;
	unsigned int ysize, xsize, ic;

	if(!tw)
		return tw;

	if(tw->flags & TEXTWIN_FLAG_ERRORMSG) {
		tw->basecolor = (CL_TEXTWIN_ERROR | (CLB_TEXTWIN_ERROR << 4));
		tw->buttoncolor = (CL_TEXTWIN_EBUTTON | (CLB_TEXTWIN_EBUTTON << 4));
	} else {
		tw->basecolor = (CL_TEXTWIN_BASE | (CLB_TEXTWIN_BASE << 4));
		tw->buttoncolor = (CL_TEXTWIN_BUTTON | (CLB_TEXTWIN_BUTTON << 4));
	}

	if((tw->flags & TEXTWIN_FLAG_CONFIRM) && !tw->nb_buttons)
		display_textwin_additem_buttons(tw, 0, 0, -1, &confirm_buttons_notext[0], NULL);

	it = tw->item_first;
	if(!it || !tw->nb_items)
		goto err_out_owi;

	textwin_calculate_window_lines(tw, &ysize, &xsize);

	if(!display_textwin_init_window(tw, tw->flags, ysize, xsize))
		goto err_out_owi;

	ic = 0;
	do {
		unsigned int aligned_items;
		if(tw->item_selected < 0) {	// select active item (editline or button)
			switch (it->type) {
			case TEXTWIN_LINETYPE_MSG_EDIT:
			case TEXTWIN_LINETYPE_SWITCH:
				tw->item_selected = ic;
				pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_NORMAL);	// enable cursor
				break;
			case TEXTWIN_LINETYPE_BUTTON:
				{
					display_textwin_button_t *bt = it->data;
					if(bt->text && bt->text[0] && bt->extkey)
						tw->item_selected = ic;
				}
				break;
			}
		}
		aligned_items = textwin_itemline_align(tw, it);	// align items (center,right)
		it += aligned_items;
		ic += aligned_items;
	} while(ic < tw->nb_items);

	display_textwin_draw_window_border(tw, tw->headtext);
	display_textwin_draw_window_items(tw);

	display_textwin_desktop_refresh();

	if((tw->buttonhandlerfunc) || (tw->flags & TEXTWIN_FLAG_KEYCLOSEMSG)) {
		mpxplay_control_keyboard_set_topfunc(&display_textwin_keyboard_topfunc);
		mpxplay_timer_addfunc(display_textwin_execute_window_items, tw, MPXPLAY_TIMERTYPE_SIGNAL | MPXPLAY_TIMERTYPE_REPEAT, MPXPLAY_SIGNALTYPE_KEYBOARD);
	}
	if(tw->flags & TEXTWIN_FLAG_TIMEDMESSAGE)
		mpxplay_timer_addfunc(&display_textwin_closewindow_items, tw, MPXPLAY_TIMERTYPE_WAKEUP, mpxplay_timer_secs_to_counternum(DISPLAY_MESSAGE_TIME));

	return tw;

  err_out_owi:
	display_textwin_close_window(tw);
	return NULL;
}

void display_textwin_closewindow_items(void *tw_p)
{
	textwin_window_t *tw = tw_p;
	if(!tw)
		return;

	if((tw->buttonhandlerfunc) || (tw->flags & TEXTWIN_FLAG_KEYCLOSEMSG)) {
		mpxplay_timer_deletefunc(display_textwin_execute_window_items, tw);
		mpxplay_control_keyboard_set_topfunc(NULL);
	}
	if(tw->flags & TEXTWIN_FLAG_TIMEDMESSAGE)
		mpxplay_timer_deletefunc(display_textwin_closewindow_items, tw);

	display_textwin_close_window(tw);
}

//-------------------------------------------------------------------------

static int textwin_additem_msg(void *tw_update, unsigned int flags, unsigned int xpos, int ypos, char *msg, unsigned int type)
{
	textwin_window_t *tw = tw_update;
	int itemnum = -1;
	char *next;

	if(!tw)
		return itemnum;

	if(ypos < 0)
		ypos = tw->item_pos_y;

	do {
		textwin_item_t *it;
		if(!textwin_itemspace_alloc(tw, 1))
			break;
		if(!textwin_check_ypos(tw, ypos))
			break;
		if(itemnum < 0)
			itemnum = tw->nb_items;	// the starting itemnum
		it = tw->item_first + tw->nb_items;
		it->type = type;
		it->flags = flags;
		it->xpos = xpos;
		it->ypos = ypos++;
		next = pds_strchr(msg, '\n');
		if(next)
			it->xsize = next - msg;
		else
			it->xsize = pds_strlen(msg);
		switch (type) {
		case TEXTWIN_LINETYPE_MSG_STATIC:
			it->data = msg;
			break;
		case TEXTWIN_LINETYPE_MSG_ALLOCATED:
			it->data = malloc(it->xsize + 1);
			if(it->data) {
				pds_strncpy((char *)it->data, msg, it->xsize);
				((char *)it->data)[it->xsize] = 0;
			}
			break;
		}
		if(next)
			msg = next + 1;
		tw->nb_items++;
	} while(next);
	tw->item_pos_y = max(tw->item_pos_y, ypos);
	return itemnum;
}

int display_textwin_additem_msg_static(void *tw_update, unsigned int flags, unsigned int xpos, int ypos, char *msg)
{
	return textwin_additem_msg(tw_update, flags, xpos, ypos, msg, TEXTWIN_LINETYPE_MSG_STATIC);
}

int display_textwin_additem_msg_alloc(void *tw_update, unsigned int flags, unsigned int xpos, int ypos, char *msg)
{
	return textwin_additem_msg(tw_update, flags, xpos, ypos, msg, TEXTWIN_LINETYPE_MSG_ALLOCATED);
}

void display_textwin_update_msg(void *tw_update, int itemnum, char *msg)
{
	textwin_window_t *tw = tw_update;
	char *next;

	if(!tw || (itemnum < 0) || (itemnum > tw->nb_items))
		return;

	do {
		textwin_item_t *it = tw->item_first + itemnum;
		next = pds_strchr(msg, '\n');
		if(next)
			it->xsize = next - msg;
		else
			it->xsize = pds_strlen(msg);
		if(it->type == TEXTWIN_LINETYPE_MSG_STATIC) {
			it->data = msg;
		} else if(it->type == TEXTWIN_LINETYPE_MSG_ALLOCATED) {
			if(it->data)
				free(it->data);
			it->data = malloc(it->xsize + 1);
			if(it->data) {
				pds_strncpy((char *)it->data, msg, it->xsize);
				((char *)it->data)[it->xsize] = 0;
			}
		} else
			break;
		if(next)
			msg = next + 1;
		itemnum++;
	} while(next && (itemnum <= tw->nb_items));

	display_textwin_draw_window_items(tw);
}

void display_textwin_additem_editline(void *tw_update, unsigned int iflags, unsigned int eflags, unsigned int xpos, int ypos, unsigned int minxsize, char *strbuf, unsigned int bufsize)
{
	textwin_window_t *tw = tw_update;
	textwin_item_t *it;
	textwin_editline_t *el;

	if(!tw)
		return;
	if(!textwin_itemspace_alloc(tw, 1))
		return;
	if(ypos < 0)
		ypos = tw->item_pos_y;
	if(!textwin_check_ypos(tw, ypos))
		return;

	it = tw->item_first + tw->nb_items;
	it->data = calloc(1, sizeof(struct textwin_editline_t));
	if(!it->data)
		return;
	el = (textwin_editline_t *) it->data;
	it->type = TEXTWIN_LINETYPE_MSG_EDIT;
	it->flags = iflags;
	it->xpos = xpos;
	it->ypos = ypos++;
	it->xsize = minxsize;
	el->flags = eflags;
	el->strbuf = strbuf;
	el->maxdatalen = bufsize;
	el->line_data_len = min(bufsize, pds_strlen(strbuf));
	if(eflags & TEXTWIN_EDITFLAG_STARTEND)
		el->line_data_pos = el->line_data_len;
	else
		el->line_data_pos = 0;
	funcbit_enable(el->flags, TEXTWIN_EDITLINEFLAG_FIRSTKEY);
	tw->nb_items++;
	tw->item_pos_y = max(tw->item_pos_y, ypos);
}

void display_textwin_additem_buttons(void *tw_update, unsigned int flags, unsigned int xpos, int ypos, display_textwin_button_t * buttons, display_textwin_button_t * selected_button)
{
	textwin_window_t *tw = tw_update;
	display_textwin_button_t *bt = buttons;

	if(!tw || !bt)
		return;
	if(!bt->text && !bt->extkey)
		return;

	if(ypos < 0)
		ypos = tw->item_pos_y;

	do {
		textwin_item_t *it;
		if(!textwin_itemspace_alloc(tw, 1))
			break;
		if(!textwin_check_ypos(tw, ypos))
			break;
		it = tw->item_first + tw->nb_items;
		it->type = TEXTWIN_LINETYPE_BUTTON;
		it->flags = flags;
		it->xsize = pds_strlen(bt->text);
		if(it->xsize) {
			it->xpos = xpos;
			it->ypos = ypos;
			if(flags & TEXTWIN_FLAG_VERTICALBUTTONS)
				ypos++;
			else {
				xpos += it->xsize;
				if(bt->extkey)	// !!! space between buttons only if there is extkey
					xpos += TEXTWIN_BUTTONS_GAPSIZE;
			}
		}
		it->data = bt;
		if(bt == selected_button)
			tw->item_selected = tw->nb_items;
		tw->nb_buttons++;
		tw->nb_items++;
		bt++;
	} while(bt->text || bt->extkey);
	tw->item_pos_y = max(tw->item_pos_y, ypos);
}

void display_textwin_additem_separatorline(void *tw_update, int ypos)
{
	textwin_window_t *tw = tw_update;
	textwin_item_t *it;

	if(!tw)
		return;
	if(!textwin_itemspace_alloc(tw, 1))
		return;
	if(ypos < 0)
		ypos = tw->item_pos_y;
	if(!textwin_check_ypos(tw, ypos))
		return;

	it = tw->item_first + tw->nb_items;
	it->type = TEXTWIN_LINETYPE_SEPARATOR;
	it->ypos = ypos++;
	tw->nb_items++;
	tw->item_pos_y = max(tw->item_pos_y, ypos);
}

int display_textwin_additem_switchline(void *tw_update, unsigned int flags, int xpos, int ypos, mpxp_uint32_t * switchvar, unsigned int switchbit, char *switchtext)
{
	textwin_window_t *tw = tw_update;
	textwin_item_t *it;
	textwin_switchline_t *sl;
	unsigned int textlen;

	if(!tw)
		return 0;
	if(!textwin_itemspace_alloc(tw, 1))
		return 0;
	if(ypos < 0)
		ypos = tw->item_pos_y;
	if(!textwin_check_ypos(tw, ypos))
		return 0;

	it = tw->item_first + tw->nb_items;
	it->type = TEXTWIN_LINETYPE_SWITCH;
	it->flags = flags;
	it->xpos = xpos;
	it->ypos = ypos++;
	textlen = pds_strlen(switchtext);
	it->xsize = textlen + sizeof("[x] ") - 1;
	it->data = malloc(sizeof(*sl));
	if(!it->data)
		return 0;
	sl = it->data;
	sl->switchvar = switchvar;
	if(!switchbit)
		switchbit = 1;
	sl->switchbit = switchbit;
	sl->switchtext = malloc(textlen + 1);
	if(sl->switchtext)
		pds_strcpy(sl->switchtext, switchtext);
	tw->nb_items++;
	tw->item_pos_y = max(tw->item_pos_y, ypos);
	return 1;
}

//-------------------------------------------------------------------------
// simple message (closed by caller)
void *display_textwin_openwindow_message(void *tw, char *headtext, char *msg)
{
	const unsigned int flags = TEXTWIN_FLAG_MSGCENTERALIGN;
	tw = display_textwin_allocwindow_items(tw, flags, headtext, NULL, NULL);
	display_textwin_additem_msg_static(tw, flags, 0, -1, msg);
	return display_textwin_openwindow_items(tw, 0, 0, 0);
}

void display_textwin_closewindow_message(void *tw)
{
	display_textwin_closewindow_items(tw);
}

// simple message (closed by a keypress)
void *display_textwin_openwindow_keymessage(unsigned int flags, char *headtext, char *msg)
{
	void *tw;
	funcbit_enable(flags, TEXTWIN_FLAG_KEYCLOSEMSG);
	tw = display_textwin_allocwindow_items(NULL, flags, headtext, NULL, NULL);
	display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
	return display_textwin_openwindow_items(tw, 0, 0, 0);
}

// simple timed message (closed by a keypress or after 3 sec)
void *display_textwin_openwindow_timedmessage(char *msg)
{
	const unsigned int flags = (TEXTWIN_FLAG_TIMEDMESSAGE | TEXTWIN_FLAG_KEYCLOSEMSG | TEXTWIN_FLAG_MSGCENTERALIGN);
	void *tw = display_textwin_allocwindow_items(NULL, flags, NULL, NULL, NULL);
	display_textwin_additem_msg_static(tw, 0, 0, -1, "");
	display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
	display_textwin_additem_msg_static(tw, 0, 0, -1, "");
	return display_textwin_openwindow_items(tw, 0, 0, 0);
}

// simple timed message in red window (closed by a keypress or after 3 sec)
void *display_textwin_openwindow_errormessage(char *msg)
{
	const unsigned int flags = (TEXTWIN_FLAG_TIMEDMESSAGE | TEXTWIN_FLAG_KEYCLOSEMSG | TEXTWIN_FLAG_ERRORMSG | TEXTWIN_FLAG_MSGCENTERALIGN);
	void *tw = display_textwin_allocwindow_items(NULL, flags, NULL, NULL, NULL);
	display_textwin_additem_msg_static(tw, 0, 0, -1, "");
	display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
	display_textwin_additem_msg_static(tw, 0, 0, -1, "");
	return display_textwin_openwindow_items(tw, 0, 0, 0);
}

void *display_textwin_openwindow_errormsg_ok(char *headtext, char *msg)
{
	const unsigned int flags = (TEXTWIN_FLAG_KEYCLOSEMSG | TEXTWIN_FLAG_ERRORMSG | TEXTWIN_FLAG_MSGCENTERALIGN);
	void *tw = display_textwin_allocwindow_items(NULL, flags, headtext, NULL, NULL);
	display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
	display_textwin_additem_buttons(tw, flags, 0, -1, errorhand_button_ok, NULL);
	return display_textwin_openwindow_items(tw, 0, 0, 0);
}

// simple message with confirm (yes/no)
void *display_textwin_openwindow_confirm(unsigned int flags, char *headtext, char *msg, void *exec_func, void *exec_data)
{
	textwin_window_t *tw;
	funcbit_enable(flags, TEXTWIN_FLAG_CONFIRM | TEXTWIN_FLAG_MSGCENTERALIGN);
	tw = display_textwin_allocwindow_items(NULL, flags, headtext, exec_func, exec_data);
	display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
	display_textwin_additem_buttons(tw, TEXTWIN_FLAG_MSGCENTERALIGN, 0, -1, &confirm_buttons_yesno[0], NULL);
	return display_textwin_openwindow_items(tw, 0, 0, 0);
}

void *display_textwin_openwindow_buttons(void *tw, unsigned int flags, char *headtext, char *msg, void *buttonhandfunc, void *buttonhanddata, display_textwin_button_t * buttons)
{
	tw = display_textwin_allocwindow_items(tw, flags, headtext, buttonhandfunc, buttonhanddata);
	if(buttons)
		display_textwin_additem_msg_alloc(tw, flags, 0, -1, msg);
	else
		display_textwin_additem_msg_static(tw, flags, 0, -1, msg);
	display_textwin_additem_buttons(tw, flags, 0, -1, buttons, NULL);
	return display_textwin_openwindow_items(tw, 0, 0, 0);
}

void display_textwin_closewindow_buttons(void *tw)
{
	display_textwin_closewindow_items(tw);
}
