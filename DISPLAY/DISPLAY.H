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
//function: display functions

#ifndef mpxplay_display_h
#define mpxplay_display_h

#include "mpxplay.h"
#include "control\control.h"

// refresh_display types (refdisp variable)
#define RDT_ID3INFO      1  // ID3 window
#define RDT_HEADER       2  // time/frame values,head infos
#define RDT_VOL          4  // only the volume modified (and displayed)
#define RDT_OPTIONS      8  // crossfade,swapchan,volume,surround,speed, etc.
#define RDT_EDITOR      16  // editor,LCD
#define RDT_BROWSER     32  // browser,mousepos
#define RDT_INIT_VALUES 64  // init values   (ie:alt-F9)
#define RDT_INIT_ANABRO 128 // init analiser ('a') and browser
#define RDT_INIT_BROWS  256 // init browser  ('a' or alt-up/down)
#define RDT_INIT_BTN    512 // init buttons  (shift-F9)
#define RDT_INIT_EDIT   1024// init editor   (alt-up/down)
#define RDT_RESET_EDIT  2048// reset editor  (currently for -idl only (new playlist/resize editor))
#define RDT_INIT_FULL   (RDT_INIT_VALUES|RDT_INIT_ANABRO|RDT_INIT_BTN|RDT_INIT_EDIT|RDT_RESET_EDIT)

// displaymodes
#define DISP_FRAMES     0x00000001  // only frames is displayed
#define DISP_TIMEPOS    0x00000002  // timepos is displayed
#define DISP_VERBOSE    0x00000004  // id3info & header are displayed
#define DISP_FULLSCREEN 0x00000008  // use fullscreen
#define DISP_NOFULLEDIT 0x00000010  // full editor not used
#define DISP_ANALISER   0x00000020  // spectrum analiser enabled
#define DISP_50LINES    0x00000040  // 50 lines display
#define DISP_TEXTMODES  0x000000ff  // mask
#define DISP_GRAPHICAL  0x00000100  // non-text mode used

// desktopmode flags
#define DTM_SONGPOS         0x00000001  // draw songpos line
#define DTM_LISTPOS         0x00000002  // draw listpos line
#define DTM_EDIT_ALLFILES   0x00000004  // display all filenames in editor (not only supported audio/container files)
#define DTM_EDIT_ELEVATOR   0x00000008  // draw elevator
#define DTM_EDIT_VERTICAL   0x00000010  // playlist editor vertical border
#define DTM_EDIT_FULLPATH   0x00000020  // (long)filename with fullpath at the top of editor
#define DTM_EDIT_FULLTIME   0x00000040  // fulltime in editor (in the bottom-right corner)
#define DTM_EDIT_SONGTIME   0x00000080  // song times in editor (in every lines)
#define DTM_EDIT_SONGNUM    0x00000100  // song counter in editor (in every lines)
#define DTM_EDIT_DRIVES     0x00000200  // drive letters in editor
#define DTM_EDIT_FILENAMES  0x00000400  // show filenames only in editor
#define DTM_EDIT_FOLLOWBROWSER 0x00000800 // move editorhighline by mouse in songbrowser
#define DTM_EDIT_MOUSESONGSHIFT 0x00001000  // shift playlist entry or scroll editorside up or down by mouse (drag & move)
#define DTM_EDIT_MAGNETFOLLOWSKIP 0x00002000 // editorhighline follows song-skip only if it stands on the current/played file
#define DTM_EDIT_MDCE_DISABLE 0x00004000    // disable center_editorhighline function
#define DTM_ID3WINSCROLL_DISABLE 0x00008000 // disable id3-window scroll
#define DTM_CONFIRM_EXIT    0x00010000  // confirm exit
#define DTM_EDIT_NOLISTWARNINGS 0x00020000 // disable playlist warnings
#define DTM_EDIT_LOADLIST   0x00040000  // load playlist to playlistside (else playlists are always sublists)
#define DTM_EDIT_SHOWDIRDATE 0x00080000 // display date of directories (and filenames at DTM_EDIT_ALLFILES)
//#define DTM_EDIT_TOTALS   0x00100000  // total songnum & time
#define DTM_MASK_COMMANDER  (DTM_EDIT_ALLFILES|DTM_EDIT_FILENAMES|DTM_EDIT_SHOWDIRDATE)
#define DTM_MASK_LOCKMPXINI (DTM_CONFIRM_EXIT) // lock (don't modify) mpxplay.ini setting of these flags

// display major positions (y=vertical)
#define POS_FILEINFO_FE   (dtp.timepos_fe-1)
#define POS_ID3INFO_FE    (dtp.timepos_fe-2)
#define POS_INVALID       255

#define EDITOR_SIDE_SIZE_DEFAULT  64
#define EDITOR_SIDE_SIZE_MIN      1 // 13

#define TEXTSCREEN_MAXX     128
#define ID3WINDOW_SIZE_NOFE 46 // id3-window size at non-fulleditor

#define LASTMOUSEBOX_INVALID 65535 // refresh mousepos

// button flags/types in structure desktop
#define BTNF_DISABLED    0   // temporary not used
#define BTNF_NONE        1   // no box, only mousepos
#define BTNF_LEFT        2   // left part of a box
#define BTNF_RGHT        4   // right part of a box
#define BTNF_FULL        8   // full box
#define BTNF_ONLY        16  // no info, only box
#define BTNF_UPTEXT      32  // write text to top of box
#define BTNF_PRESS       64  // 'press' button, otherwise 'click'
#define BTNF_BASE        256 // base position for buttons
#define BTNF_ENDOFBTN    512   // end of buttons
#define BTNF_BROWSER     1024  // button is a song-browser button
#define BTNF_END         65536 // end of browser
#define BTNF_MOUSEPOS    (BTNF_NONE|BTNF_LEFT|BTNF_RGHT|BTNF_FULL|BTNF_BROWSER)
#define BTNF_DRAWBOX     (BTNF_FULL|BTNF_ONLY)

//struct buttons dk[] (desktop keys) defs
#define DK_MAX_SIZE    350 // max number of buttons (control+browser)
#define DK_FILEINFO    1                 // fileinfo buttons begin
#define DK_ID3INFO     (DK_FILEINFO+ 7)  // id3info buttons begin
#define DK_TIMEPOS     (DK_ID3INFO+I3I_MAX+1)  // timepos buttons begin (currsong/allsongs ; curtime/alltime)
#define DK_BUFCPU      (DK_TIMEPOS + 4)  // buf/cpu
#define DK_FUNCTION    (DK_BUFCPU  + 1)  // function
#define DK_MOUSEPOS    (DK_FUNCTION+ 1)  // mousepos
#define DK_CONTROL     (DK_MOUSEPOS+ 1)  // control buttons begin
#define DK_OPTIONS     (DK_CONTROL + 8)  // options buttons begin (vol)
#define DK_SMALLBTNS   (DK_OPTIONS +23)  // swapchan,hq-mixer,autopause
#define DK_BROWSER     (DK_SMALLBTNS+5)  // browser buttons begin in dk[]

// color definitions
#define CL_BASE          7  // base color = gray
#define CLH_BASE        15  // base highlight color = white
#define CLB_BASE         0  // base background color = black
#define CL_BLINK       128  // flashing (as background color at any color settings)
#define CL_MOUSECURSOR   5  // mouse cursor

//tag info colors
#define CL_INFO_BASE   1               // info area base color
#define CLB_INFO_BASE  CLB_BASE        //
#define CL_INFOBORDER  CL_INFO_BASE    // info border
#define CLB_INFOBORDER CLB_INFO_BASE   // info border background
#define CL_INFOTEXT    10              // info text
#define CLH_INFOTEXT   14              // highlighted info text
#define CLH_INFOTEXTB  15              // mousepos shows button
#define CLB_INFOTEXT   CLB_INFO_BASE   // info text background
#define CL_ID3TEXT     7               // id3 text
#define CLH_ID3TEXT    15              // highlighted id3
#define CLB_ID3TEXT    CLB_INFO_BASE   // id3 background
#define CL_FUNCTION    12              // PAUSED,CrosFade, ...
#define CLB_FUNCTION   CLB_INFOTEXT    //

//button colors
#define CL_BTNBOX      CL_INFO_BASE    // buttonbox
#define CLB_BTNBOX     CLB_BASE        // buttonbox background
#define CLH_BTNBOX     9               // highlighted buttonbox (mousepos)
#define CLA_BTNBOX    15               // active button box
#define BTN_FLASHTIME  2               // button flash length

//button (in/up)text colors
#define CL_BTNTEXTCNTRL    15     // control button intext (skip,move,play)
#define CL_BTNTEXTOPTION    1     // option button intext (hq,rn,re,cf)
#define CL_BTNTEXTMIXER    10     // mixer button intext (vol,sur,spd)
#define CLA_BTNTEXTOPTION  11     // active button intext (enabled function)
#define CLB_BTNTEXT    CLB_BASE   // button intext background
//#define CLAB_BTNTEXT   CLB_BASE  // active button intext background
#define CL_BTNUPTEXT   CL_BTNBOX  // button uptext (headtext) (must be the same with CL_BTNBOX)
#define CLB_BTNUPTEXT  CLB_BTNBOX // button uptext background

//playlist editor colors
#define CL_EDITOR_BASE        CL_BASE          // base color
#define CLB_EDITOR_BASE       CLB_BASE         // background base color
#define CL_EDITOR_BORDER      CL_EDITOR_BASE   // border
#define CLB_EDITOR_BORDER     CLB_EDITOR_BASE  //
#define CL_EDITOR_FULLPATH    CL_EDITOR_BASE   //
#define CLB_EDITOR_FULLPATH   CLB_EDITOR_BASE  //
#define CL_EDITOR_FULLTIME    11               //
#define CLB_EDITOR_FULLTIME   CLB_EDITOR_BASE  //
#define CL_EDITOR_ELEVATOR    CL_EDITOR_BASE   //
#define CLB_EDITOR_ELEVATOR   CLB_EDITOR_BASE  //
#define CLB_EDITOR_HIGHLINE    1 // highline (bar) background    = blue
#define CL_EDITOR_NEWFILENUM  10 // newfile/nextfile (ctrl-enter)= lightgreen
#define CL_EDITOR_AKTFILENUM  11 // actual file                  = lightcyan
#define CL_EDITOR_PLAYED       7 //                              = gray
#define CL_EDITOR_NOTPLAYED   15 //                              = white
#define CL_EDITOR_AT_SEPARATOR 7 // ':' (artist:title)           = gray
#define CL_EDITOR_SELECTED    14 //                              = yellow

/*#define CL_EDITOR_NEWFILENUM  10 // newfile/nextfile (ctrl-enter)= lightgreen
#define CL_EDITOR_AKTFILENUM  14 // actual file                  = yellow
#define CL_EDITOR_PLAYED       7 //                              = gray
#define CL_EDITOR_NOTPLAYED   15 //                              = white
#define CL_EDITOR_AT_SEPARATOR 7 // ':' (artist:title)           = gray
#define CL_EDITOR_SELECTED     11*/

//browser colors
#define CLB_BROWSER_BASE       CLB_BASE //
#define CL_BROWSER_AKTFILENUM 10        //
#define CL_BROWSER_PLAYED      1        //
#define CL_BROWSER_NOTPLAYED   2        //
#define CL_BROWSER_OVERENTRY   8        //

//songpos/listpos colors
#define CLB_SONGPOS       CLB_BASE   //
#define CL_SONGPOS_DOTS   CL_BASE    //
#define CL_SONGPOS_HEAD   CLH_BASE   //
#define CL_SONGPOS_ARROWS CLH_BASE   //
#define CLB_LISTPOS       CLB_BASE   //
#define CL_LISTPOS_DOTS   CL_BASE    //
#define CL_LISTPOS_HEAD   CLH_BASE   //
#define CL_LISTPOS_ARROWS CLH_BASE   //

//volume indicator colors
#define CLB_VOLUME          CLB_BASE //
#define CL_VOLUME_DOTS       1       // volume indicator dots
#define CL_VOLUME_LEVEL0    10       //
#define CL_VOLUME_LEVEL1    14       //
#define CL_VOLUME_LEVEL2    12       //
#define CL_VOLUME_SEPARATOR CL_VOLUME_LEVEL0  // "--"

//spectrum analiser
#define CLB_ANALISER        CLB_BASE //
#define CL_ANALISER_DOTS     1       // spectrum analiser dots
#define CL_ANALISER_LEVEL0  10       //
#define CL_ANALISER_LEVEL1  14       //
#define CL_ANALISER_LEVEL2  12       //

//display_message
#define DISPLAY_MESSAGE_TIME 3       // in secs

#define DISPLAY_REFRESH_FPS       19 // 38/2 (audio frames/sec / 2) (dynamic setting?)

#define DISPLAY_ANALISER_BANDNUM  32
#define DISPLAY_ANALISER_MAXDELAY (DISPLAY_REFRESH_FPS/2) // 500ms

typedef struct desktoppos{
  int volline;
  int timepos_fe;
  int endofbuttonsy;
  int relative_songposline; // relative to endofbuttonsy
  int songposline_y;
  int listposline_y;
  int editorbegin;
  int editorend;
}desktoppos;

typedef struct buttons{
 unsigned int boxflag;
 unsigned int xpos;    // relative to base xpos (dk[0].xpos)
 unsigned int ypos;    // relative to base ypos (dk[0].ypos)
 unsigned int xsize;   // contains real xsize - 1
 unsigned int ysize;   // contains real ysize - 1
 unsigned int *keycode;
 unsigned int *keycode2;
 char *intext;
 char *mousepostext;
}buttons;

typedef struct editor_s{
  struct playlist_entry_info *from;
  struct playlist_entry_info *from_prev;
  int begincol;
  int lengthcol;
  int beginline;
  int lengthline;
}editor_s;

//display.c
extern void mpxplay_display_init(struct mainvars *mvp);
extern void mpxplay_display_close(struct mainvars *mvp);
extern void mpxplay_display_switch_to_textmode(struct mainvars *mvp);
extern void mpxplay_display_switch_to_graphmode(struct mainvars *mvp);

extern char *get_playstatus_string(unsigned int *color);
extern void display_editorside_reset(struct playlist_side_info *psi);
extern void refresh_desktop(struct mainvars *);

extern void clear_volnum(void);

extern void set_mousepos_on_elevator(struct mainvars *,unsigned int mousey);
extern void set_mousepos_on_editor(struct mainvars *,unsigned int,unsigned int);
extern void draweditor(struct mainvars *);
extern void display_draw_editor_oneline(struct playlist_side_info *psi,struct playlist_entry_info *pei);

extern unsigned int display_editor_resize_x(struct mainvars *,int direction);
extern void display_editor_resize_y(int);
extern void scroll_editorside(struct mainvars *,struct playlist_entry_info *ehls);
extern void mpxplay_display_center_editorhighline(struct playlist_side_info *psi,unsigned int visible_lines);

extern void clear_mousepos_text(void);
extern void draw_mousepos_text(unsigned int,char *);
extern void draw_mouse_listpos(struct mainvars *,unsigned int);
extern void draw_mouse_desktoppos(struct mainvars *,unsigned int x,unsigned int y);
extern void draw_mouse_restorelastmousec(void);

extern void display_message(unsigned int,unsigned int,char *);
extern void clear_message(void);
extern void display_clear_timed_message(void);
extern void display_timed_message(char *message); // till DISPLAY_MESSAGE_TIME
extern void display_static_message(unsigned int linepos,unsigned int blink,char *msg);
extern void clear_static_message(void);
extern void display_warning_message(char *message);
extern void display_bufpos_int08(struct mainvars *mvp);
extern void display_help_window(void);

//buttons.c
extern void mpxplay_display_buttons_init(void);
extern struct buttons *mpxplay_control_mouse_xy_to_dp(struct mainvars *,unsigned int,unsigned int);
extern void color_one_buttonbox(struct buttons *,unsigned int);
extern void recolor_lastbuttonbox_key(struct buttons *,unsigned int);
extern void recolor_lastbuttonbox_mousepos(struct mainvars *,struct buttons *);
extern void color_buttonbox_key(unsigned int);

extern void generate_browserboxes(void);
extern void drawbrowser(struct mainvars *);
extern void draw_browserbox(struct mainvars *,struct playlist_entry_info *);

//lcd.c
extern void mpxplay_display_lcd_loadini(mpxini_line_t *,struct mpxini_part_t *);
extern void mpxplay_display_lcd_init(void);
extern void mpxplay_display_lcd_close(void);

extern void LCD_refresh_once(unsigned int);
extern void LCD_refresh_timer(struct mainvars *);
extern void LCD_page_select(struct mainvars *,int select);

//textwin.c
#define CL_TEXTWIN_BASE     0        // black
#define CLB_TEXTWIN_BASE    7        // lightgray (inversed text)
#define CL_TEXTWIN_SHADOW   7        // lightgray
#define CLB_TEXTWIN_SHADOW  0        // black
#define CL_TEXTWIN_ERROR    15       // white
#define CLB_TEXTWIN_ERROR   4        // red
#define CL_TEXTWIN_BUTTON   0
#define CLB_TEXTWIN_BUTTON  3        // cyan
#define CL_TEXTWIN_EBUTTON  0
#define CLB_TEXTWIN_EBUTTON 7

#define TEXTWIN_FLAG_MSGCENTERALIGN    1
#define TEXTWIN_FLAG_MSGLEFTALIGN      2
#define TEXTWIN_FLAG_MSGRIGHTALIGN     4
#define TEXTWIN_FLAGS_MSG_ALIGN (TEXTWIN_FLAG_MSGCENTERALIGN|TEXTWIN_FLAG_MSGLEFTALIGN|TEXTWIN_FLAG_MSGRIGHTALIGN)
#define TEXTWIN_FLAG_TIMEDMESSAGE      8
#define TEXTWIN_FLAG_KEYCLOSEMSG      16
#define TEXTWIN_FLAG_ERRORMSG         32 // red window
#define TEXTWIN_FLAG_CONFIRM          64 // enter/esc keys (no buttons)
#define TEXTWIN_FLAG_DONTCLOSE       128 // don't dealloc window after executing
#define TEXTWIN_FLAG_VERTICALBUTTONS 256 // aka roll menu
#define TEXTWIN_FLAG_NOWINMINSIZE    512 // use smallest window

#define TEXTWIN_EDITFLAG_OVERWRITE     1 // else default is inserting
#define TEXTWIN_EDITFLAG_NUMERIC       2 // accept only numbers
#define TEXTWIN_EDITFLAG_STARTEND      4 // start editing at the end of string

typedef struct display_textwin_button_t{
 char *text;
 unsigned int extkey;
}display_textwin_button_t;

extern void display_textwin_draw_window_headtext(void *tw_update,char *headtext);
extern unsigned int display_textwin_handle_mousepos(unsigned int x,unsigned int y,unsigned int click);
extern void *display_textwin_allocwindow_items(void *tw_update,unsigned int flags,char *headtext,void *buttonhandfunc,void *buttonhanddata);
extern void *display_textwin_openwindow_items(void *tw_update,unsigned int xpos,unsigned int ypos,unsigned int minxsize);
extern void display_textwin_closewindow_items(void *tw);
extern int  display_textwin_additem_msg_static(void *tw_update,unsigned int flags,unsigned int xpos,int ypos,char *msg); // returns itemnum
extern int  display_textwin_additem_msg_alloc(void *tw_update,unsigned int flags,unsigned int xpos,int ypos,char *msg);  //
extern void display_textwin_update_msg(void *tw_update,int itemnum,char *msg);
extern void display_textwin_additem_editline(void *tw_update,unsigned int iflags,unsigned int eflags,unsigned int xpos,int ypos,unsigned int minxsize,char *strbuf,unsigned int bufsize);
extern void display_textwin_additem_buttons(void *tw_update,unsigned int flags,unsigned int xpos,int ypos,display_textwin_button_t *buttons,display_textwin_button_t *selected_button);
extern void display_textwin_additem_separatorline(void *tw_update,int ypos);
extern int display_textwin_additem_switchline(void *tw_update,unsigned int flags,int xpos,int ypos,mpxp_uint32_t *switchvar,unsigned int switchbit,char *switchtext);

extern void display_textwin_close(void);
extern void *display_textwin_openwindow_buttons(void *tw_update,unsigned int flags,char *headtext,char *msg,void *buttonhandfunc,void *buttonhanddata,display_textwin_button_t *buttons);
extern void display_textwin_closewindow_buttons(void *tw);
extern void *display_textwin_openwindow_message(void *tw_update,char *headtext,char *msg);
extern void display_textwin_closewindow_message(void *tw);
extern void *display_textwin_openwindow_keymessage(unsigned int flags,char *headtext,char *msg);
extern void *display_textwin_openwindow_timedmessage(char *msg);
extern void *display_textwin_openwindow_errormessage(char *msg);
extern void *display_textwin_openwindow_errormsg_ok(char *headtext,char *msg);
extern void *display_textwin_openwindow_confirm(unsigned int flags,char *headtext,char *msg,void *exec_func,void *exec_data);

#endif // mpxplay_display_h
