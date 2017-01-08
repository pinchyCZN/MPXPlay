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
//function: joystick handler functions

#include "control.h"
#include "newfunc\newfunc.h"
#include <conio.h>

static void mpxplay_control_joy_getevent(void);

static unsigned int Joy1Port, Joy1FuncLeft, Joy1FuncRight, Joy1FuncUp;
static unsigned int Joy1FuncDown, Joy1FuncBtn1, Joy1FuncBtn2;
static unsigned int Joy2Port, Joy2FuncLeft, Joy2FuncRight, Joy2FuncUp;
static unsigned int Joy2FuncDown, Joy2FuncBtn1, Joy2FuncBtn2;
static unsigned int JoyAxisPreDelay, JoyAxisDelay, JoyButtonPreDly;
static unsigned int JoyButtonDelay, JoySensitivity;

static mpxini_var_s joy_base_vars[] = {
	{"Joy1Port", &Joy1Port, ARG_HEX},
	{"Joy1FuncLeft", &Joy1FuncLeft, ARG_HEX},
	{"Joy1FuncRight", &Joy1FuncRight, ARG_HEX},
	{"Joy1FuncUp", &Joy1FuncUp, ARG_HEX},
	{"Joy1FuncDown", &Joy1FuncDown, ARG_HEX},
	{"Joy1FuncBtn1", &Joy1FuncBtn1, ARG_HEX},
	{"Joy1FuncBtn2", &Joy1FuncBtn2, ARG_HEX},
	{"Joy2Port", &Joy2Port, ARG_HEX},
	{"Joy2FuncLeft", &Joy2FuncLeft, ARG_HEX},
	{"Joy2FuncRight", &Joy2FuncRight, ARG_HEX},
	{"Joy2FuncUp", &Joy2FuncUp, ARG_HEX},
	{"Joy2FuncDown", &Joy2FuncDown, ARG_HEX},
	{"Joy2FuncBtn1", &Joy2FuncBtn1, ARG_HEX},
	{"Joy2FuncBtn2", &Joy2FuncBtn2, ARG_HEX},
	{"JoyAxisPreDelay", &JoyAxisPreDelay, ARG_NUM},
	{"JoyAxisDelay", &JoyAxisDelay, ARG_NUM},
	{"JoyButtonPreDly", &JoyButtonPreDly, ARG_NUM},
	{"JoyButtonDelay", &JoyButtonDelay, ARG_NUM},
	{"JoySensitivity", &JoySensitivity, ARG_NUM},
	{NULL, NULL, 0}
};

#define JOY_EVENTFLAG_JOY1_AXISX 0x01
#define JOY_EVENTFLAG_JOY1_AXISY 0x02
#define JOY_EVENTFLAG_JOY2_AXISX 0x04
#define JOY_EVENTFLAG_JOY2_AXISY 0x08
#define JOY_EVENTMASK_JOY1_AXIS  (JOY_EVENTFLAG_JOY1_AXISX|JOY_EVENTFLAG_JOY1_AXISY)
#define JOY_EVENTMASK_JOY2_AXIS  (JOY_EVENTFLAG_JOY2_AXISX|JOY_EVENTFLAG_JOY2_AXISY)
#define JOY_EVENTFLAG_BUTTON1    0x10
#define JOY_EVENTFLAG_BUTTON2    0x20
#define JOY_EVENTFLAG_BUTTON3    0x40
#define JOY_EVENTFLAG_BUTTON4    0x80
#define JOY_EVENTMASK_BUTTON (JOY_EVENTFLAG_BUTTON1|JOY_EVENTFLAG_BUTTON2|JOY_EVENTFLAG_BUTTON3|JOY_EVENTFLAG_BUTTON4)

#define JOY_AXIS_RELEASE_DELAY   3
#define JOY_BUTTON_RELEASE_DELAY 1

static unsigned int getjoy1pos(unsigned int, unsigned int *, unsigned int *, unsigned int *);
static unsigned int getjoy2pos(unsigned int, unsigned int *, unsigned int *, unsigned int *);

typedef struct joyhand_s {
	int port;
	int lowx, highx, lowy, highy, lastx, lasty;
	int axislastevent, axiscount, axisdelay;
	int buttonlastevent, buttoncount, buttondelay;
} joyhand_s;

static joyhand_s joy1;
static joyhand_s joy2;

static void joy_initvals(struct joyhand_s *joys, unsigned int joyx, unsigned int joyy)
{
	if(joys->port) {
		unsigned int sens = JoySensitivity;
		joys->lowx = joyx - (joyx >> sens);	// Joy1FuncLeft is activated if joyx is less than lowx
		joys->highx = joyx + (joyx >> sens);	// Joy1FuncRight is activated if joyx is more than highx
		joys->lowy = joyy - (joyy >> sens);	// Joy1FuncUp is activated if joyy is less than lowy
		joys->highy = joyy + (joyy >> sens);	// Joy1FuncDown is activated if joyy is more than highy
		joys->lastx = joyx;
		joys->lasty = joyy;
	}
}

void mpxplay_control_joy_loadini(mpxini_line_t * mpxini_lines, struct mpxini_part_t *mpxini_partp)
{
	mpxplay_control_general_loadini(mpxini_lines, mpxini_partp, joy_base_vars);
}

void mpxplay_control_joy_init(void)
{
	unsigned int joyx, joyy, joyevent;

	if(Joy1Port) {
		joy1.port = getjoy1pos(Joy1Port, &joyx, &joyy, &joyevent);
		joy_initvals(&joy1, joyx, joyy);
	}

	if(Joy2Port) {
		joy2.port = getjoy2pos(Joy2Port, &joyx, &joyy, &joyevent);
		joy_initvals(&joy2, joyx, joyy);
	}

	if(joy1.port || joy2.port)
		mpxplay_timer_addfunc(&mpxplay_control_joy_getevent, NULL, MPXPLAY_TIMERTYPE_REPEAT, mpxplay_timer_secs_to_counternum(1) / 36);
}

static void mpxplay_control_joy_getevent(void)
{
	unsigned int joyx, joyy, joyevent, newkey;
	register joyhand_s *joy;

	// joy1
	joy = &joy1;
	if(joy->port) {
		getjoy1pos(joy->port, &joyx, &joyy, &joyevent);
		if(!joy->axisdelay) {
			joyx = (joy->lastx + joyx) >> 1;
			joy->lastx = joyx;
			joyy = (joy->lasty + joyy) >> 1;
			joy->lasty = joyy;

			newkey = 0;
			if(joyx < joy->lowx) {
				newkey = Joy1FuncLeft;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISX;
			} else if(joyx > joy->highx) {
				newkey = Joy1FuncRight;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISX;
			} else if(joyy < joy->lowy) {
				newkey = Joy1FuncUp;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISY;
			} else if(joyy > joy->highy) {
				newkey = Joy1FuncDown;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISY;
			}

			if((joyevent & JOY_EVENTMASK_JOY1_AXIS) != joy->axislastevent) {
				joy->axislastevent = joyevent & JOY_EVENTMASK_JOY1_AXIS;
				joy->axiscount = 0;
			}
			if(newkey) {		// execute axis function
				pds_pushkey(newkey);
				if(!joy->axiscount) {	// first
					joy->axisdelay = JoyAxisPreDelay;
					joy->axiscount = 1;
				} else			// repeat
					joy->axisdelay = JoyAxisDelay;
			} else {
				joy->axisdelay = JOY_AXIS_RELEASE_DELAY;
				joy->axiscount = 0;
			}
		} else
			joy->axisdelay--;

		if(!joy->buttondelay || (joyevent & JOY_EVENTMASK_BUTTON) != joy->buttonlastevent) {
			if((joyevent & JOY_EVENTMASK_BUTTON) != joy->buttonlastevent) {
				joy->buttonlastevent = joyevent & JOY_EVENTMASK_BUTTON;
				joy->buttoncount = 0;
			}
			newkey = 0;
			if(!(joyevent & JOY_EVENTFLAG_BUTTON1))
				newkey = Joy1FuncBtn1;
			else if(!(joyevent & JOY_EVENTFLAG_BUTTON2))
				newkey = Joy1FuncBtn2;
			else if(!(joyevent & JOY_EVENTFLAG_BUTTON3))
				newkey = Joy2FuncBtn1;
			else if(!(joyevent & JOY_EVENTFLAG_BUTTON4))
				newkey = Joy2FuncBtn2;

			if(newkey) {		// execute button function
				pds_pushkey(newkey);
				if(!joy->buttoncount) {	// first
					joy->buttondelay = JoyButtonPreDly;
					joy->buttoncount = 1;
				} else			// repeat
					joy->buttondelay = JoyButtonDelay;
			} else {
				joy->buttondelay = JOY_BUTTON_RELEASE_DELAY;
				joy->buttoncount = 0;
			}
		} else
			joy->buttondelay--;
	}
	//joy2
	joy = &joy2;
	if(joy->port) {
		getjoy1pos(joy->port, &joyx, &joyy, &joyevent);
		if(!joy->axisdelay) {
			joyx = (joy->lastx + joyx) >> 1;
			joy->lastx = joyx;
			joyy = (joy->lasty + joyy) >> 1;
			joy->lasty = joyy;

			newkey = 0;
			if(joyx < joy->lowx) {
				newkey = Joy2FuncLeft;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISX;
			} else if(joyx > joy->highx) {
				newkey = Joy2FuncRight;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISX;
			} else if(joyy < joy->lowy) {
				newkey = Joy2FuncUp;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISY;
			} else if(joyy > joy->highy) {
				newkey = Joy2FuncDown;
				joyevent |= JOY_EVENTFLAG_JOY1_AXISY;
			}

			if((joyevent & JOY_EVENTMASK_JOY1_AXIS) != joy->axislastevent) {
				joy->axislastevent = joyevent & JOY_EVENTMASK_JOY1_AXIS;
				joy->axiscount = 0;
			}
			if(newkey) {		// execute axis function
				pds_pushkey(newkey);
				if(!joy->axiscount) {	// first
					joy->axisdelay = JoyAxisPreDelay;
					joy->axiscount = 1;
				} else			// repeat
					joy->axisdelay = JoyAxisDelay;
			} else {
				joy->axisdelay = JOY_AXIS_RELEASE_DELAY;
				joy->axiscount = 0;
			}
		} else
			joy->axisdelay--;

		if(!joy->buttondelay || (joyevent & JOY_EVENTMASK_BUTTON) != joy->buttonlastevent) {
			if((joyevent & JOY_EVENTMASK_BUTTON) != joy->buttonlastevent) {
				joy->buttonlastevent = joyevent & JOY_EVENTMASK_BUTTON;
				joy->buttoncount = 0;
			}
			newkey = 0;
			if(!(joyevent & JOY_EVENTFLAG_BUTTON1))
				newkey = Joy1FuncBtn1;
			else if(!(joyevent & JOY_EVENTFLAG_BUTTON2))
				newkey = Joy1FuncBtn2;
			else if(!(joyevent & JOY_EVENTFLAG_BUTTON3))
				newkey = Joy2FuncBtn1;
			else if(!(joyevent & JOY_EVENTFLAG_BUTTON4))
				newkey = Joy2FuncBtn2;

			if(newkey) {		// execute button function
				pds_pushkey(newkey);
				if(!joy->buttoncount) {	// first
					joy->buttondelay = JoyButtonPreDly;
					joy->buttoncount = 1;
				} else			// repeat
					joy->buttondelay = JoyButtonDelay;
			} else {
				joy->buttondelay = JOY_BUTTON_RELEASE_DELAY;
				joy->buttoncount = 0;
			}
		} else
			joy->buttondelay--;
	}
}

#ifdef MPXPLAY_WIN32
static const unsigned int btns_win[4] = { JOY_BUTTON1, JOY_BUTTON2, JOY_BUTTON3, JOY_BUTTON4 };
static const unsigned int btns_dos[4] = { JOY_EVENTFLAG_BUTTON1, JOY_EVENTFLAG_BUTTON2, JOY_EVENTFLAG_BUTTON3, JOY_EVENTFLAG_BUTTON4 };

static unsigned int getjoy1pos(unsigned int joyport, unsigned int *joyx, unsigned int *joyy, unsigned int *joyevent)
{
	unsigned int i;
	JOYINFO ji;

	*joyx = 0;
	*joyy = 0;
	*joyevent = 0;

	if(joyGetPos(JOYSTICKID1, &ji) == JOYERR_NOERROR) {
		for(i = 0; i < 4; i++)
			if(ji.wButtons & btns_win[i])
				*joyevent |= btns_dos[i];
		*joyx = ji.wXpos;
		*joyy = ji.wYpos;
		*joyevent |= JOY_EVENTMASK_JOY1_AXIS;
		return joyport;
	}
	return 0;
}

static unsigned int getjoy2pos(unsigned int joyport, unsigned int *joyx, unsigned int *joyy, unsigned int *joyevent)
{
	unsigned int i;
	JOYINFO ji;

	*joyx = 0;
	*joyy = 0;
	*joyevent = 0;

	if(joyGetPos(JOYSTICKID2, &ji) == JOYERR_NOERROR) {
		for(i = 0; i < 4; i++)
			if(ji.wButtons & btns_win[i])
				*joyevent |= btns_dos[i];
		*joyx = ji.wXpos;
		*joyy = ji.wYpos;
		*joyevent |= JOY_EVENTMASK_JOY2_AXIS;
		return joyport;
	}
	return 0;
}

#else							// __DOS__

static unsigned int getjoy1pos(unsigned int joyport, unsigned int *joyx, unsigned int *joyy, unsigned int *joyevent)
{
	unsigned int count, countx, county;

	outp(joyport, 0xff);
	countx = county = 0;
	count = 0xffff;
	do {
		char joybits = inp(joyport);
		if(joybits & JOY_EVENTFLAG_JOY1_AXISX)
			countx++;
		if(joybits & JOY_EVENTFLAG_JOY1_AXISY)
			county++;
		if(!(joybits & JOY_EVENTMASK_JOY1_AXIS))
			break;
	} while(--count);
	if(!count)					// no axis, no joy
		return 0;
	joyx[0] = countx;
	joyy[0] = county;
	joyevent[0] = inp(joyport);
	return joyport;
}

static unsigned int getjoy2pos(unsigned int joyport, unsigned int *joyx2, unsigned int *joyy2, unsigned int *joyevent)
{
	unsigned int count, countx2, county2;

	outp(joyport, 0xff);
	countx2 = county2 = 0;
	count = 0xffff;
	do {
		char joybits = inp(joyport);
		if(joybits & JOY_EVENTFLAG_JOY2_AXISX)
			countx2++;
		if(joybits & JOY_EVENTFLAG_JOY2_AXISY)
			county2++;
		if(!(joybits & JOY_EVENTMASK_JOY2_AXIS))
			break;
	} while(--count);
	if(!count)
		return 0;
	joyx2[0] = countx2;
	joyy2[0] = county2;
	joyevent[0] = inp(joyport);
	return joyport;
}

#endif
