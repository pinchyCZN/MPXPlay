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
//function: mouse functions

//#define MPXPLAY_USE_DEBUGF 1
//#define MOUSE_DEBUG_OUTPUT NULL

#include "control.h"
#include "cntfuncs.h"
#include "newfunc\newfunc.h"
#include "display\display.h"

static void mpxplay_control_mouse_detect(void);
static void mpxplay_control_mouse_getevent(struct mainvars *mvp);

extern keyconfig kb[];
extern struct desktoppos dtp;

extern unsigned int mouse_on;
extern unsigned int displaymode, desktopmode, textscreen_maxx, textscreen_maxy;
extern unsigned int int08counter, refdisp;
extern unsigned long mpxplay_signal_events;

static unsigned int EnableMouse = 1, mouse_wheel_support;
static unsigned int MouseFuncBtn1Pri, MouseFuncBtn1Sec;	// pri: press, sec:single-click
static unsigned int MouseFuncBtn2Pri, MouseFuncBtn2Sec;
static unsigned int MouseFuncBtn3Pri, MouseFuncBtn3Sec;
static unsigned int MouseFuncWheelUp, MouseFuncWheelDn;

static mpxini_var_s mouse_base_vars[] = {
	{"EnableMouse", &EnableMouse, ARG_NUM},
	{"MouseFuncBtn1Pri", &MouseFuncBtn1Pri, ARG_HEX},
	{"MouseFuncBtn1Sec", &MouseFuncBtn1Sec, ARG_HEX},
	{"MouseFuncBtn2Pri", &MouseFuncBtn2Pri, ARG_HEX},
	{"MouseFuncBtn2Sec", &MouseFuncBtn2Sec, ARG_HEX},
	{"MouseFuncBtn3Pri", &MouseFuncBtn3Pri, ARG_HEX},
	{"MouseFuncBtn3Sec", &MouseFuncBtn3Sec, ARG_HEX},
	{"MouseFuncWheelUp", &MouseFuncWheelUp, ARG_HEX},
	{"MouseFuncWheelDn", &MouseFuncWheelDn, ARG_HEX},
	{NULL, NULL, 0}
};

#define MOUSE_USE_ASM
//#define MOUSE_LOCKMEM 1

#if (INT08_DIVISOR_NEW<12000)
#define MOUSE_CLICK_TIME   3	// (min.) time (length) of single click
#define MOUSE_PRESS_TIME  24	// (min.) time of continuous press (click or press?)
#define MOUSE_PRESS_REPEAT_INTERVAL 3	// ca. 38 char/s
#define MOUSE_DOUBLE_TIME 24	// max time between clicks (double or single click?)
#else							// INT08_DIVISOR_NEW==20000
#define MOUSE_CLICK_TIME   2	// (min.) time (length) of single click
#define MOUSE_PRESS_TIME  12	// (min.) time of continuous press (click or press?)
#define MOUSE_PRESS_REPEAT_INTERVAL 2	// ca. 29 char/s
#define MOUSE_DOUBLE_TIME 12	// max time between clicks (double or single click?)
#endif

#define MOUSE_EVENT_MOVEMENT         1
#define MOUSE_EVENT_LEFT_PRESS       2
#define MOUSE_EVENT_LEFT_RELEASE     4
#define MOUSE_EVENT_RIGHT_PRESS      8
#define MOUSE_EVENT_RIGHT_RELEASE   16
#define MOUSE_EVENT_CENTER_PRESS    32
#define MOUSE_EVENT_CENTER_RELEASE  64
#define MOUSE_EVENT_CENTER2_PRESS   (MOUSE_EVENT_LEFT_PRESS|MOUSE_EVENT_RIGHT_PRESS)
#define MOUSE_EVENT_CENTER2_RELEASE (MOUSE_EVENT_LEFT_RELEASE|MOUSE_EVENT_RIGHT_RELEASE)

#define MOUSE_EVENT_PRESS   (MOUSE_EVENT_LEFT_PRESS|MOUSE_EVENT_RIGHT_PRESS|MOUSE_EVENT_CENTER_PRESS)
#define MOUSE_EVENT_RELEASE (MOUSE_EVENT_LEFT_RELEASE|MOUSE_EVENT_RIGHT_RELEASE|MOUSE_EVENT_CENTER_RELEASE)

#define MOUSE_BUTTON_LEFT   1
#define MOUSE_BUTTON_RIGHT  2
#define MOUSE_BUTTON_CENTER 4
#define MOUSE_BUTTON_WHEEL  8

#define MOUSE_CLICKTYPE_SINGLE  1
#define MOUSE_CLICKTYPE_DOUBLE  2
#define MOUSE_CLICKTYPE_PRESS   4

static struct callback_data {
	unsigned short mouse_ax;
	unsigned short mouse_bx;
	unsigned short mouse_cx;
	unsigned short mouse_dx;
	signed short mouse_si;
	signed short mouse_di;

	unsigned int irq_running;
	unsigned int mouse_event;
	unsigned int readbutton;
	unsigned int mousebutton;
	unsigned int lastbutton;
	unsigned int clicktype;
	int wheel_movement;
	unsigned int buttonpresstime_begin;
	unsigned int buttonpresstime_last;
	unsigned int buttonreleasetime;
	unsigned int mousex;
	unsigned int mousey;
	unsigned int lastx;
	unsigned int lasty;
	unsigned int press_startpos_x;
	unsigned int press_startpos_y;
} cbd;

static unsigned int mouse_check_func(unsigned int button)
{
	switch (button) {
	case MOUSE_BUTTON_LEFT:
		if(MouseFuncBtn1Pri || MouseFuncBtn1Sec)
			return 1;
		break;
	case MOUSE_BUTTON_CENTER:
		if(MouseFuncBtn2Pri || MouseFuncBtn2Sec)
			return 1;
		break;
	case MOUSE_BUTTON_RIGHT:
		if(MouseFuncBtn3Pri || MouseFuncBtn3Sec)
			return 1;
		break;
	}
	return 0;
}

#ifdef __DOS__
static void mouse_button_handler(struct callback_data *cbp)
{
	cbp->mousex = cbp->mouse_cx >> 3;
	cbp->mousey = cbp->mouse_dx >> 3;

	if((cbp->mouse_ax & (MOUSE_EVENT_CENTER_PRESS | MOUSE_EVENT_CENTER_RELEASE))
	   || ((cbp->mouse_ax & MOUSE_EVENT_CENTER2_PRESS) == MOUSE_EVENT_CENTER2_PRESS)
	   || ((cbp->mouse_ax & MOUSE_EVENT_CENTER2_RELEASE) == MOUSE_EVENT_CENTER2_RELEASE)) {
		cbp->readbutton = MOUSE_BUTTON_CENTER;
	} else {
		if(cbp->mouse_ax & (MOUSE_EVENT_LEFT_PRESS | MOUSE_EVENT_LEFT_RELEASE))
			cbp->readbutton = MOUSE_BUTTON_LEFT;
		else if(cbp->mouse_ax & (MOUSE_EVENT_RIGHT_PRESS | MOUSE_EVENT_RIGHT_RELEASE))
			cbp->readbutton = MOUSE_BUTTON_RIGHT;
		else if(mouse_wheel_support && (cbp->mouse_bx & 0xff00)) {
			cbp->readbutton = cbp->buttonpresstime_begin = cbp->buttonpresstime_last = cbp->buttonreleasetime = 0;
			cbp->mousebutton = MOUSE_BUTTON_WHEEL;
			cbp->wheel_movement += (short)cbp->mouse_bx / 256;
		}						//else mouse pointer move (possibly drag and move)
	}

	if(cbp->readbutton) {
		cbp->mousebutton = 0;

		if(cbp->mouse_ax & MOUSE_EVENT_PRESS) {
			// generate 1 single click in fullscreen mode (pre-click for desktop)
			if(!cbp->buttonpresstime_begin && (!cbp->buttonreleasetime || (int08counter - cbp->buttonreleasetime) > MOUSE_CLICK_TIME)
			   && !mouse_check_func(cbp->readbutton)) {
				cbp->mousebutton = cbp->readbutton;
				cbp->clicktype = MOUSE_CLICKTYPE_SINGLE;
			}
			//double click check (for desktop)
			if(cbp->buttonreleasetime) {
				if(((int08counter - cbp->buttonreleasetime) < MOUSE_DOUBLE_TIME)
				   && (cbp->readbutton == cbp->lastbutton) && (cbp->lastx == cbp->mousex) && (cbp->lasty == cbp->mousey)
				   && (displaymode & DISP_FULLSCREEN) && !mouse_check_func(cbp->readbutton)) {
					cbp->mousebutton = cbp->readbutton;
					cbp->clicktype = MOUSE_CLICKTYPE_DOUBLE;
				}
			}
			cbp->buttonpresstime_begin = cbp->buttonpresstime_last = int08counter;
			cbp->buttonreleasetime = 0;
		} else {
			if(cbp->mouse_ax & MOUSE_EVENT_RELEASE) {
				// single click / end of press check (post-click for mousebutton functions)
				if(cbp->buttonpresstime_begin) {
					unsigned int timelen = int08counter - cbp->buttonpresstime_begin;
					if((timelen >= MOUSE_CLICK_TIME) && (timelen < MOUSE_PRESS_TIME) && !cbp->mouse_event && mouse_check_func(cbp->readbutton)) {
						cbp->mousebutton = cbp->readbutton;
						cbp->clicktype = MOUSE_CLICKTYPE_SINGLE;
					}
					cbp->buttonpresstime_begin = cbp->buttonpresstime_last = 0;
				}
				cbp->buttonreleasetime = int08counter;
				cbp->readbutton = 0;
			}
		}

		if(cbp->mousebutton) {
			cbp->lastbutton = cbp->mousebutton;
			cbp->lastx = cbp->mousex;
			cbp->lasty = cbp->mousey;
		}
	}

	funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_MOUSE);

	cbp->mouse_event = 1;
}

void _loadds far mouse_click_handler(int r_ax, int r_bx, int r_cx, int r_dx, int r_si, int r_di)
{
#pragma aux mouse_click_handler parm [EAX] [EBX] [ECX] [EDX] [ESI] [EDI]

	if(!cbd.irq_running) {
		cbd.irq_running = 1;
		cbd.mouse_ax = (unsigned short)r_ax;
		cbd.mouse_bx = (unsigned short)r_bx;
		cbd.mouse_cx = (unsigned short)r_cx;
		cbd.mouse_dx = (unsigned short)r_dx;
		cbd.mouse_si = (signed short)r_si;
		cbd.mouse_di = (signed short)r_di;

		mouse_button_handler(&cbd);
		cbd.irq_running = 0;
	}
}

#ifdef MOUSE_LOCKMEM
void mouse_clickhand_endptr(void)
{
}
#endif
#endif							// __DOS__

#ifdef MPXPLAY_WIN32
#include <malloc.h>
#include <wincon.h>

static HANDLE hConsoleInput;

// called/refreshed from pds_kbhit()
void mpxplay_control_mouse_winconsole_getevent(void)
{
	DWORD nb_events, n, nbe;
	INPUT_RECORD *inprec;
	MOUSE_EVENT_RECORD *mer;
	struct callback_data *cbp = &cbd;
	//char sout[100];

	if(!hConsoleInput)
		hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
	if(!hConsoleInput)
		return;

	if(!GetNumberOfConsoleInputEvents(hConsoleInput, &nb_events))
		return;
	if(!nb_events || nb_events > 64)
		return;
	nb_events += 8;				// for safe (ReadConsoleInput can freeze if we don't give enough space to store datas (if nbe>nb_events))
	inprec = alloca(nb_events * sizeof(INPUT_RECORD));
	if(!inprec)
		return;

	if(!ReadConsoleInput(hConsoleInput, inprec, nb_events * sizeof(INPUT_RECORD), &nbe))
		return;
	for(n = 0; (n < nbe) && (n < nb_events); n++) {
		switch (inprec[n].EventType) {
		case KEY_EVENT:
			if(inprec[n].Event.KeyEvent.bKeyDown) {
				unsigned int scancode =
					newfunc_keyboard_winkey_to_extkey(inprec[n].Event.KeyEvent.dwControlKeyState, inprec[n].Event.KeyEvent.wVirtualKeyCode, inprec[n].Event.KeyEvent.uChar.AsciiChar);
				if(scancode)
					pds_pushkey(scancode);
				//mpxplay_debugf(MOUSE_DEBUG_OUTPUT,
				/*sprintf(sout,"\scan:%4.4X vk:%4.4X asc:%2.2X sc:%2.2X nbe:%d e:%d n:%d",
				   scancode,
				   (int)inprec[n].Event.KeyEvent.wVirtualKeyCode,
				   (int)inprec[n].Event.KeyEvent.uChar.AsciiChar,
				   (int)inprec[n].Event.KeyEvent.wVirtualScanCode,
				   //(int)inprec[n].Event.KeyEvent.wRepeatCount,
				   nbe,nb_events,n);
				   display_message(1,0,sout); */
			}
			break;
		case MOUSE_EVENT:
			mer = &inprec[n].Event.MouseEvent;
			//sprintf(sout,"nb:%d e:%8.8X b:%8.8X c:%8.8X x:%3d y:%3d",nbe,mer->dwEventFlags,mer->dwButtonState,mer->dwControlKeyState,mer->dwMousePosition.X,mer->dwMousePosition.Y);
			//display_message(1,0,sout);
			cbp->mousex = mer->dwMousePosition.X;
			cbp->mousey = mer->dwMousePosition.Y;
			if(mer->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
				cbp->readbutton = MOUSE_BUTTON_LEFT;
			if(mer->dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED)
				cbp->readbutton = MOUSE_BUTTON_CENTER;
			if(mer->dwButtonState & FROM_LEFT_3RD_BUTTON_PRESSED)
				cbp->readbutton = MOUSE_BUTTON_CENTER;
			if(mer->dwButtonState & RIGHTMOST_BUTTON_PRESSED)
				cbp->readbutton = MOUSE_BUTTON_RIGHT;
			if(mer->dwEventFlags & MOUSE_WHEELED) {
				cbp->readbutton = MOUSE_BUTTON_WHEEL;
				cbp->wheel_movement += (short)cbp->mouse_bx / 256;	// ???
			}
			if(cbp->readbutton) {
				cbp->mousebutton = 0;
				cbp->clicktype = 0;
				if(mer->dwEventFlags & DOUBLE_CLICK) {
					cbp->mousebutton = cbp->readbutton;
					cbp->clicktype = MOUSE_CLICKTYPE_DOUBLE;
				} else {		// generate 1 single click in fullscreen mode (pre-click for desktop)
					if(!cbp->buttonpresstime_begin && !mouse_check_func(cbp->readbutton)) {
						cbp->mousebutton = cbp->readbutton;
						cbp->clicktype = MOUSE_CLICKTYPE_SINGLE;
					}
				}
				if(!cbp->buttonpresstime_begin)
					cbp->buttonpresstime_begin = cbp->buttonpresstime_last = int08counter;
			}
			if(!mer->dwButtonState && cbp->buttonpresstime_begin) {	// post single click
				unsigned int timelen = int08counter - cbp->buttonpresstime_begin;
				if((timelen >= MOUSE_CLICK_TIME) && (timelen < MOUSE_PRESS_TIME) && !cbp->mouse_event && mouse_check_func(cbp->readbutton)) {
					cbp->mousebutton = cbp->readbutton;
					cbp->clicktype = MOUSE_CLICKTYPE_SINGLE;
				}
				cbp->buttonpresstime_begin = cbp->buttonpresstime_last = 0;
				cbp->readbutton = 0;
			}

			if(cbp->mousebutton) {
				cbp->lastbutton = cbp->mousebutton;
				cbp->lastx = cbp->mousex;
				cbp->lasty = cbp->mousey;
			}

			funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_MOUSE);
			cbp->mouse_event = 1;

			break;
		}
	}
}
#endif

//------------------------------------------------------------------------

#if defined(MOUSE_LOCKMEM) && defined(__DOS__)
static int lock_region(void *address, unsigned length)
{
	union REGS regs;
	unsigned linear;

	linear = (unsigned)address;

	regs.w.ax = 0x600;			/* DPMI Lock Linear Region */
	regs.w.bx = (unsigned short)(linear >> 16);	/* Linear address in BX:CX */
	regs.w.cx = (unsigned short)(linear & 0xFFFF);
	regs.w.si = (unsigned short)(length >> 16);	/* Length in SI:DI */
	regs.w.di = (unsigned short)(length & 0xFFFF);
	int386(0x31, &regs, &regs);
	return (!regs.w.cflag);		/* Return 0 if can't lock */
}

static void unlock_region(void *address, unsigned length)
{
	union REGS regs;
	unsigned linear;

	linear = (unsigned)address;

	regs.w.ax = 0x601;			/* DPMI Lock Linear Region */
	regs.w.bx = (unsigned short)(linear >> 16);	/* Linear address in BX:CX */
	regs.w.cx = (unsigned short)(linear & 0xFFFF);
	regs.w.si = (unsigned short)(length >> 16);	/* Length in SI:DI */
	regs.w.di = (unsigned short)(length & 0xFFFF);
	int386(0x31, &regs, &regs);
}
#endif

static void control_mouse_clickhand_init(void)
{
#ifdef MPXPLAY_WIN32
	hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
	FlushConsoleInputBuffer(hConsoleInput);
#elif defined(__DOS__)
	struct SREGS sregs;
	union REGS regs;
	void (far * function_ptr) ();

	segread(&sregs);

	regs.w.ax = 0x0C;
	if(mouse_wheel_support)
		regs.w.cx = 0x00ff;
	else
		regs.w.cx = 0x007f;
	function_ptr = mouse_click_handler;
	regs.x.edx = FP_OFF(function_ptr);
	sregs.es = FP_SEG(function_ptr);
	int386x(0x33, &regs, &regs, &sregs);
#endif
}

void mpxplay_control_mouse_loadini(mpxini_line_t * mpxini_lines, struct mpxini_part_t *mpxini_partp)
{
	mpxplay_control_general_loadini(mpxini_lines, mpxini_partp, mouse_base_vars);
}

void mpxplay_control_mouse_init(void)
{
	if(EnableMouse)
		mpxplay_control_mouse_detect();

	if(mouse_on) {
#ifdef MOUSE_LOCKMEM
		if(!lock_region(&cbd, sizeof(cbd)) || !lock_region((void near *)mouse_click_handler, (char *)mouse_clickhand_endptr - (char near *)mouse_click_handler))
			mouse_on = 0;
		else
#endif
		{
			control_mouse_clickhand_init();
			mpxplay_timer_addfunc(&mpxplay_control_mouse_getevent, NULL, MPXPLAY_TIMERTYPE_REPEAT | MPXPLAY_TIMERFLAG_MVPDATA, mpxplay_timer_secs_to_counternum(1) / 36);
		}
	}
}

void mpxplay_control_mouse_close(void)
{
	if(mouse_on) {
#ifdef MOUSE_LOCKMEM
		unlock_region(&cbd, sizeof(cbd));
		unlock_region((void near *)mouse_click_handler, (char *)mouse_clickhand_endptr - (char near *)mouse_click_handler);
#endif
		mpxplay_control_mouse_detect();	// reset
	}
}

static void mpxplay_control_mouse_getevent(struct mainvars *mvp)
{
	struct callback_data *cbp;
	unsigned int newkey;

	if(!mouse_on)
		return;

	cbp = &cbd;
	if(!cbp->mouse_event && !cbp->readbutton && !cbp->mousebutton)
		goto getevent_end;

	//check continuous press
	if(!cbp->mousebutton) {
		if(cbp->readbutton && cbp->buttonpresstime_begin) {
			if((int08counter - cbp->buttonpresstime_begin) >= MOUSE_PRESS_TIME) {
				if((int08counter - cbp->buttonpresstime_last) >= MOUSE_PRESS_REPEAT_INTERVAL) {
					cbp->mousebutton = cbp->readbutton;
					cbp->clicktype = MOUSE_CLICKTYPE_PRESS;
					cbp->buttonpresstime_last += MOUSE_PRESS_REPEAT_INTERVAL;
				}
			}
		}
	}

	if(cbp->clicktype != MOUSE_CLICKTYPE_PRESS) {
		cbp->press_startpos_x = cbp->mousex;
		cbp->press_startpos_y = cbp->mousey;
	}

	newkey = 0;

	if(cbp->mousebutton == MOUSE_BUTTON_LEFT) {	// left button
		if(cbp->clicktype & MOUSE_CLICKTYPE_PRESS)
			newkey = MouseFuncBtn1Pri;	// defined in mpxplay.ini
		else if(cbp->clicktype & MOUSE_CLICKTYPE_SINGLE)
			newkey = MouseFuncBtn1Sec;	// defined in mpxplay.ini

		if(!MouseFuncBtn1Pri && !MouseFuncBtn1Sec) {
			if(!display_textwin_handle_mousepos(cbp->mousex, cbp->mousey, 1)) {

				if(cbp->clicktype & MOUSE_CLICKTYPE_SINGLE) {
					if(cbp->mousex && cbp->mousex < (textscreen_maxx - 1)) {
						if((cbp->mousey == dtp.songposline_y) && (desktopmode & DTM_SONGPOS)) {	// songposline one click
							struct frame *frp = mvp->frp0;
							mvp->seek_absolute = frp->index_start + (frp->index_len * (cbp->mousex - 1)) / (textscreen_maxx - 2) + 1;
						}
						if((cbp->mousey == dtp.listposline_y) && (desktopmode & DTM_LISTPOS))	// listposline one click
							mvp->newsong = (((mvp->psip->lastentry - mvp->psip->firstsong) + 1) * cbp->mousex) / (textscreen_maxx - 2) + mvp->psip->firstsong;
					}
					if((cbp->mousey == dtp.listposline_y) && (desktopmode & DTM_LISTPOS)) {	// listposline one click
						if(cbp->mousex == 0)
							newkey = kb[20].c;	// step back
						if(cbp->mousex == (textscreen_maxx - 1))
							newkey = kb[21].c;	// step forward
					}
				}
				{				// desktop buttons/browser
					struct buttons *dp = mpxplay_control_mouse_xy_to_dp(mvp, cbp->mousex, cbp->mousey);
					if(dp) {
						if((dp->boxflag & BTNF_PRESS && (cbp->clicktype == MOUSE_CLICKTYPE_PRESS)) || (cbp->clicktype & MOUSE_CLICKTYPE_SINGLE) || (cbp->clicktype & MOUSE_CLICKTYPE_DOUBLE)) {
							if(dp->boxflag & BTNF_BROWSER)
								mvp->newsong = mvp->psip->firstsong + (unsigned int)(dp->keycode);
							else if(dp->keycode)
								newkey = *(dp->keycode);
						}
					}
				}

				if((cbp->mousey == dtp.songposline_y) && (desktopmode & DTM_SONGPOS)) {	// songposline continuous press
					if(cbp->mousex == 0)
						newkey = kb[0].c;	// fast rewind
					if(cbp->mousex == (textscreen_maxx - 1))
						newkey = kb[2].c;	// fast forward
				}
				if((cbp->mousey > dtp.editorbegin) && (cbp->mousey < dtp.editorend)) {
					if(cbp->mousex && (cbp->mousex < (textscreen_maxx - 1))) {	// editor
						struct playlist_entry_info *ehls = mvp->psie->editorhighline;
						set_mousepos_on_editor(mvp, cbp->mousex, cbp->mousey);
						if(cbp->clicktype & MOUSE_CLICKTYPE_DOUBLE)
							newkey = kb[160].c;	// start new song (enter)
						else if(cbp->clicktype & MOUSE_CLICKTYPE_PRESS) {	// move up or down with mouse
							if((cbp->press_startpos_y > dtp.editorbegin) && (cbp->press_startpos_y < dtp.editorend)) {
								if(desktopmode & DTM_EDIT_MOUSESONGSHIFT)
									playlist_editlist_mouse_shiftfile(mvp, ehls);
								else
									scroll_editorside(mvp, ehls);
							} else {
								cbp->press_startpos_y = cbp->mousey;	// to correct starting point if it was out of editor
							}
						}
					}
					if(cbp->mousex == (textscreen_maxx - 1))	// elevator
						set_mousepos_on_elevator(mvp, cbp->mousey);
				}

			}
		}
	}

	if(cbp->mousebutton == MOUSE_BUTTON_CENTER) {
		if(cbp->clicktype & MOUSE_CLICKTYPE_PRESS)
			newkey = MouseFuncBtn2Pri;
		else if(cbp->clicktype & MOUSE_CLICKTYPE_SINGLE)
			newkey = MouseFuncBtn2Sec;
	}

	if(cbp->mousebutton == MOUSE_BUTTON_RIGHT) {
		if(cbp->clicktype & MOUSE_CLICKTYPE_PRESS)
			newkey = MouseFuncBtn3Pri;
		else if(cbp->clicktype & MOUSE_CLICKTYPE_SINGLE)
			newkey = MouseFuncBtn3Sec;

		if(!MouseFuncBtn3Pri && !MouseFuncBtn3Sec && !(cbp->clicktype & MOUSE_CLICKTYPE_PRESS)) {	// select/unselect song with right mouse button
			if((cbp->mousey > dtp.editorbegin) && (cbp->mousey < dtp.editorend)) {
				if(cbp->mousex && (cbp->mousex < (textscreen_maxx - 1))) {	// editor
					struct playlist_side_info *psi;
					struct playlist_entry_info *pei;
					set_mousepos_on_editor(mvp, cbp->mousex, cbp->mousey);
					psi = mvp->psie;
					pei = psi->editorhighline;
					if(pei->infobits & PEIF_SELECTED) {
						funcbit_disable(pei->infobits, PEIF_SELECTED);
						if(psi->selected_files)
							psi->selected_files--;
					} else {
						funcbit_enable(pei->infobits, PEIF_SELECTED);
						psi->selected_files++;
					}
				}
			}
		}
	}

	if(cbp->mousebutton == MOUSE_BUTTON_WHEEL) {
		if(cbp->wheel_movement < 0) {	// mousewheel up
			if(MouseFuncWheelUp)
				newkey = MouseFuncWheelUp;
			else {
				playlist_editorhighline_seek(mvp->psie, cbp->wheel_movement, SEEK_CUR);
				mpxplay_display_center_editorhighline(mvp->psie, 255);
				refdisp |= RDT_EDITOR;
			}
		} else {				// mousewheel down
			if(MouseFuncWheelDn)
				newkey = MouseFuncWheelDn;
			else {
				playlist_editorhighline_seek(mvp->psie, cbp->wheel_movement, SEEK_CUR);
				mpxplay_display_center_editorhighline(mvp->psie, 255);
				refdisp |= RDT_EDITOR;
			}
		}
	}

	cbp->mouse_event = cbp->mousebutton = cbp->clicktype = cbp->wheel_movement = 0;

	if(newkey)
		pds_pushkey(newkey);

  getevent_end:
	draw_mouse_desktoppos(mvp, cbp->mousex, cbp->mousey);
}

//----------------------------------------------------------------------
#if defined(MOUSE_USE_ASM) && defined(__WATCOMC__) && defined(__DOS__)
void asm_mouse_detect(void);
void asm_getmousepos(unsigned int *x, unsigned int *y);
void asm_setmouserange(unsigned int x, unsigned int y);
#endif

static void mpxplay_control_mouse_detect(void)
{
#ifdef MPXPLAY_WIN32
	mouse_on = 1;
	mouse_wheel_support = 1;
#elif defined(__DOS__)
#if defined(MOUSE_USE_ASM) && defined(__WATCOMC__)

#pragma aux asm_mouse_detect=\
 "mov ax,0"\
 "int 0x33"\
 "cmp ax,0xffff"\
 "jne nomouse"\
 "mov dword ptr mouse_on,1"\
 "mov ax,0x11"\
 "xor ecx,ecx"\
 "int 0x33"\
 "cmp ax,'WM'"\
 "jne nomouse"\
 "test cl,1"\
 "jz nomouse"\
 "mov dword ptr mouse_wheel_support,1"\
 "nomouse:"\
 modify[eax ebx ecx edx edi esi];
	asm_mouse_detect();

#else
	union REGS regs;

	//detect mouse
	pds_memset(&regs, 0, sizeof(union REGS));
	regs.w.ax = 0;
	int386(0x33, &regs, &regs);
	if(regs.w.ax != 0xffff)
		return;
	mouse_on = 1;

	//check mouse wheel
	pds_memset(&regs, 0, sizeof(union REGS));
	regs.w.ax = 0x11;
	int386(0x33, &regs, &regs);
	if(regs.w.ax != 'WM' || !(regs.w.cx & 1))
		return;
	mouse_wheel_support = 1;
#endif
#endif							// __DOS__
}

void mpxplay_control_mouse_getpos(unsigned int *mousex, unsigned int *mousey)
{
#ifdef __DOS__
#if defined(MOUSE_USE_ASM) && defined(__WATCOMC__)

#pragma aux asm_getmousepos=\
 "push edx"\
 "push eax"\
 "mov ax,3"\
 "xor ebx,ebx"\
 "xor ecx,ecx"\
 "xor edx,edx"\
 "int 0x33"\
 "pop eax"\
 "shr ecx,3"\
 "mov dword ptr [eax],ecx"\
 "pop eax"\
 "shr edx,3"\
 "mov dword ptr [eax],edx"\
 parm [eax][edx] modify[eax ebx ecx edx edi esi];
	asm_getmousepos(mousex, mousey);

#else
	union REGS regs;

	pds_memset(&regs, 0, sizeof(union REGS));
	regs.w.ax = 3;
	int386(0x33, &regs, &regs);
	*mousex = regs.w.cx >> 3;
	*mousey = regs.w.dx >> 3;
#endif
#else
	*mousex = textscreen_maxx / 2;	// !!! after screen reset
	*mousey = textscreen_maxy / 2;
#endif
	cbd.lastx = cbd.mousex = *mousex;
	cbd.lasty = cbd.mousey = *mousey;
}

void mpxplay_control_mouse_setrange(unsigned int x, unsigned int y)
{
#ifdef __DOS__
#if defined(MOUSE_USE_ASM) && defined(__WATCOMC__)

#pragma aux asm_setmouserange=\
 "push edx"\
 "shl eax,3"\
 "mov edx,eax"\
 "xor ecx,ecx"\
 "mov ax,7"\
 "int 0x33"\
 "pop edx"\
 "shl edx,3"\
 "xor ecx,ecx"\
 "mov ax,8"\
 "int 0x33"\
 parm[eax][edx] modify[eax ecx edx ebx edi esi];
	asm_setmouserange(x, y);

#else
	union REGS regs;

	pds_memset(&regs, 0, sizeof(union REGS));
	regs.w.ax = 7;
	regs.w.dx = x << 3;
	int386(0x33, &regs, &regs);
	pds_memset(&regs, 0, sizeof(union REGS));
	regs.w.ax = 8;
	regs.w.dx = y << 3;
	int386(0x33, &regs, &regs);
#endif
#endif							// __DOS__
}
