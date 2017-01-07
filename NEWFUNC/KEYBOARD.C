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
//function: keyboard handling (by bios memory area)

//#define MPXPLAY_USE_DEBUGF 1
//#define KEYBOARD_DEBUG_OUTPUT NULL
#include "newfunc.h"
#include <mpxplay.h>

#ifdef MPXPLAY_WIN32
 // mpxplay_control_mouse_winconsole_getevent() puts the scancodes to our biostmp datafield
 #define KEYBOARD_USE_BIOSMEM 1
 static char fakebiostmp[0x80];
 #define BIOSMEM_ADDRESS (&fakebiostmp[0])
 #define KEYBUF_STARTOFFSET 0x008

 #define KEYBUF_HEADPTR 0x000
 #define KEYBUF_TAILPTR 0x002
 #define KEYBUF_STARTPTR 0x004
 #define KEYBUF_ENDPTR  0x006

 #include <control\cntfuncs.h>

#elif defined(__DOS__)
 #define KEYBOARD_USE_BIOSMEM 1
 #define BIOSMEM_ADDRESS 0
 #define KEYBUF_STARTOFFSET 0x400

 #define KEYBUF_HEADPTR 0x41a
 #define KEYBUF_TAILPTR 0x41c
 #define KEYBUF_STARTPTR 0x480
 #define KEYBUF_ENDPTR  0x482

#endif
#ifndef KEYBOARD_USE_BIOSMEM
 #include <conio.h>
#endif

void newfunc_keyboard_init(void)
{
#ifdef MPXPLAY_WIN32
 char *biosmem=(char *)BIOSMEM_ADDRESS;
 *((unsigned short *)&biosmem[KEYBUF_ENDPTR])=sizeof(fakebiostmp)-KEYBUF_STARTOFFSET;
#endif
}

unsigned int pds_kbhit(void)
{
#ifdef KEYBOARD_USE_BIOSMEM
 char *biosmem=(char *)BIOSMEM_ADDRESS;
#ifdef MPXPLAY_WIN32
 mpxplay_control_mouse_winconsole_getevent();
#endif
 if(*((unsigned short *)&biosmem[KEYBUF_HEADPTR])==*((unsigned short *)&biosmem[KEYBUF_TAILPTR]))
  return 0;
 return 1;
#else
 return kbhit();
#endif
}

#include <stdio.h>

unsigned int pds_extgetch(void)
{
#ifdef KEYBOARD_USE_BIOSMEM
 char *biosmem=(char *)BIOSMEM_ADDRESS;
 unsigned int keybufferpos;
 while(!pds_kbhit());
 keybufferpos=KEYBUF_STARTOFFSET+*((unsigned short *)&biosmem[KEYBUF_HEADPTR]);
 *((unsigned short *)&biosmem[KEYBUF_HEADPTR])+=2;
 if(*((unsigned short *)&biosmem[KEYBUF_HEADPTR])>=*((unsigned short *)&biosmem[KEYBUF_ENDPTR]))
  *((unsigned short *)&biosmem[KEYBUF_HEADPTR])=*((unsigned short *)&biosmem[KEYBUF_STARTPTR]);
 mpxplay_debugf(KEYBOARD_DEBUG_OUTPUT,"getch: %4.4X",(unsigned long)*((unsigned short *)&biosmem[keybufferpos]));
 return *((unsigned short *)&biosmem[keybufferpos]);
#else
 //char sout[50];
 int keycode=getch(); // ??? possibly bad for Mpxplay
 if(!keycode){
  keycode=getch();
  keycode<<=8;
 }else
  keycode=newfunc_keyboard_char_to_extkey(keycode);
 //sprintf(sout,"%8.8X",keycode);
 //display_message(1,0,sout);
 return keycode;
#endif
}

unsigned int pds_look_extgetch(void)
{
 if(pds_kbhit()){
#ifdef KEYBOARD_USE_BIOSMEM
  char *biosmem=(char *)BIOSMEM_ADDRESS;
  unsigned int keybufferpos=KEYBUF_STARTOFFSET+*((unsigned short *)&biosmem[KEYBUF_HEADPTR]);
  mpxplay_debugf(KEYBOARD_DEBUG_OUTPUT,"lookch: %4.4X",(unsigned long)*((unsigned short *)&biosmem[keybufferpos]));
  return *((unsigned short *)&biosmem[keybufferpos]);
#else
  unsigned int keycode=pds_extgetch();
  pds_pushkey(keycode);
  return keycode;
#endif
 }
 return 0;
}

void pds_pushkey(unsigned int newkey)
{
#ifdef KEYBOARD_USE_BIOSMEM
 char *biosmem=(char *)BIOSMEM_ADDRESS;
 unsigned int keybufferpos=KEYBUF_STARTOFFSET+*((unsigned short *)&biosmem[KEYBUF_TAILPTR]);
 biosmem[keybufferpos]  =newkey&0xff;
 biosmem[keybufferpos+1]=newkey>>8;
 *((unsigned short *)&biosmem[KEYBUF_TAILPTR])+=2;
 if(*((unsigned short *)&biosmem[KEYBUF_TAILPTR])>=*((unsigned short *)&biosmem[KEYBUF_ENDPTR]))
  *((unsigned short *)&biosmem[KEYBUF_TAILPTR])=*((unsigned short *)&biosmem[KEYBUF_STARTPTR]);
#else
 ungetch(newkey); // ???
#endif
}

typedef struct char_to_extkey_t{
 char c;
 unsigned short extkey;
}char_to_extkey_t;

static char_to_extkey_t keycodes[]={
 {0x1b,0x011b},{0x08,0x0e08},{0x09,0x0f09},{0x0d,0x1c0d},{0x1c,0xe00d},
 {0x3b,0x3b00},{0x3c,0x3c00},{0x3d,0x3d00},{0x3e,0x3e00},{0x3f,0x3f00},
 {0x40,0x4000},{0x41,0x4100},{0x42,0x4200},{0x43,0x4300},{0x44,0x4400},
 {0x47,0x47e0},{0x48,0x48e0},{0x49,0x49e0},{0x4b,0x4be0},{0x4d,0x4de0},
 {0x4f,0x4fe0},{0x50,0x50e0},{0x51,0x51e0},{0x52,0x52e0},{0x53,0x53e0},
 {'1',0x0231},{'2',0x0332},{'3',0x0433},{'4',0x0534},{'5',0x0635},
 {'6',0x0736},{'7',0x0837},{'8',0x0938},{'9',0x0A39},{'0',0x0B30},
 {'-',0x0C2D},{'=',0x0D3D},{'q',0x1071},{'w',0x1177},{'e',0x1265},
 {'r',0x1372},{'t',0x1474},{'y',0x1579},{'u',0x1675},{'i',0x1769},
 {'o',0x186F},{'p',0x1970},{'[',0x1A5B},{']',0x1B5D},{'a',0x1E61},
 {'s',0x1F73},{'d',0x2064},{'f',0x2166},{'g',0x2267},{'h',0x2368},
 {'j',0x246A},{'k',0x256B},{'l',0x266C},{';',0x273B},{'\'',0x2827},
 {'`',0x2960},{'\\',0x2B5C},{'z',0x2C7A},{'x',0x2D78},{'c',0x2E63},
 {'v',0x2F76},{'b',0x3062},{'n',0x316E},{'m',0x326D},{',',0x332C},
 {'.',0x342E},{'/',0x352F},{' ',0x3920},{'!',0x0221},{'@',0x0340},
 {'#',0x0423},{'$',0x0524},{'%',0x0625},{'^',0x075E},{'&',0x0826},
 {'*',0x092A},{'(',0x0A28},{')',0x0B29},{'_',0x0C5F},{'+',0x0D2B},
 {'Q',0x1051},{'W',0x1157},{'E',0x1245},{'R',0x1352},{'T',0x1454},
 {'Y',0x1559},{'U',0x1655},{'I',0x1749},{'O',0x184F},{'P',0x1950},
 {'{',0x1A7B},{'}',0x1B7D},{'A',0x1E41},{'S',0x1F53},{'D',0x2044},
 {'F',0x2146},{'G',0x2247},{'H',0x2348},{'J',0x244A},{'K',0x254B},
 {'L',0x264C},{':',0x273A},{'\"',0x2822},{'~',0x297E},{'|',0x2B7C},
 {'Z',0x2C5A},{'X',0x2D58},{'C',0x2E43},{'V',0x2F56},{'B',0x3042},
 {'N',0x314E},{'M',0x324D},{'<',0x333C},{'>',0x343E},{'?',0x353F},
 {'*',0x372A},{'-',0x4A2D},{'+',0x4E2B},{'/',0xE02F}, // gray buttons
 {0,0x0000}};

unsigned int newfunc_keyboard_char_to_extkey(char c)
{
 char_to_extkey_t *table=&keycodes[0];
 do{
  if(table->c==c)
   return table->extkey;
  table++;
 }while(table->c);
 return 0xffff;
}

char newfunc_keyboard_extkey_to_char(unsigned short extkey)
{
 char_to_extkey_t *table=&keycodes[0];
 do{
  if(table->extkey==extkey)
   return table->c;
  table++;
 }while(table->c);
 return 0xff;
}

//------------------------------------------------------------------
#ifdef MPXPLAY_WIN32

#define CTRLKEY_ALT   (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED)
#define CTRLKEY_CTRL  (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)
#define CTRLKEY_SHIFT (SHIFT_PRESSED|CAPSLOCK_ON)
#define CTRLKEY_ALL   (CTRLKEY_ALT|CTRLKEY_CTRL|CTRLKEY_SHIFT)

#ifndef VK_PGUP
#define VK_PGUP 0x21
#endif
#ifndef VK_PGDN
#define VK_PGDN 0x22
#endif

typedef struct winkey_to_extkey_t{
 unsigned short control;
 unsigned char virtualkey;
 char ascii;
 unsigned short extkey;
}winkey_to_extkey_t;

static winkey_to_extkey_t wincodes[]={
 {0,VK_ESCAPE,0,0x011b},
 {0,'1','1',0x0231},
 {0,'2','2',0x0332},
 {0,'3','3',0x0433},
 {0,'4','4',0x0534},
 {0,'5','5',0x0635},
 {0,'6','6',0x0736},
 {0,'7','7',0x0837},
 {0,'8','8',0x0938},
 {0,'9','9',0x0A39},
 {0,'0','0',0x0B30},
 {0,0xbd,'-',0x0C2D},
 {0,0xbb,'=',0x0D3D},
 {0,VK_BACK,0,0x0e08},
 {0,VK_TAB,0,0x0f09},
 {0,'Q','q',0x1071},
 {0,'W','w',0x1177},
 {0,'E','e',0x1265},
 {0,'R','r',0x1372},
 {0,'T','t',0x1474},
 {0,'Y','y',0x1579},
 {0,'U','u',0x1675},
 {0,'I','i',0x1769},
 {0,'O','o',0x186F},
 {0,'P','p',0x1970},
 {0,0xdb,'[',0x1A5B},
 {0,0xdd,']',0x1B5D},
 {0,VK_RETURN,0,0x1c0d},
 {0,'A','a',0x1E61},
 {0,'S','s',0x1F73},
 {0,'D','d',0x2064},
 {0,'F','f',0x2166},
 {0,'G','g',0x2267},
 {0,'H','h',0x2368},
 {0,'J','j',0x246A},
 {0,'K','k',0x256B},
 {0,'L','l',0x266C},
 {0,0xba,';',0x273B},
 {0,0xde,'\'',0x2827},
 {0,0xc0,'`',0x2960},
 {0,0xdc,'\\',0x2B5C},
 {0,'Z','z',0x2C7A},
 {0,'X','x',0x2D78},
 {0,'C','c',0x2E63},
 {0,'V','v',0x2F76},
 {0,'B','b',0x3062},
 {0,'N','n',0x316E},
 {0,'M','m',0x326D},
 {0,0xbc,',',0x332C},
 {0,0xbe,'.',0x342E},
 //{0,0xbf,'/',0x352F},          // removed white, preferred gray
 {0,VK_MULTIPLY,'*',0x372A},
 {0,' ',' ',0x3920},
 {0,VK_F1,0,0x3b00},
 {0,VK_F2,0,0x3c00},
 {0,VK_F3,0,0x3d00},
 {0,VK_F4,0,0x3e00},
 {0,VK_F5,0,0x3f00},
 {0,VK_F6,0,0x4000},
 {0,VK_F7,0,0x4100},
 {0,VK_F8,0,0x4200},
 {0,VK_F9,0,0x4300},
 {0,VK_F10,0,0x4400},
 {0,VK_F11,0,0x8500},
 {0,VK_F12,0,0x8600},
 {0,VK_NUMPAD7,0,0x4700},
 {NUMLOCK_ON,VK_NUMPAD7,'7',0x0837},
 {0,VK_NUMPAD8,0,0x4800},
 {NUMLOCK_ON,VK_NUMPAD8,'8',0x0938},
 {0,VK_NUMPAD9,0,0x4900},
 {NUMLOCK_ON,VK_NUMPAD9,'9',0x0A39},
 {0,VK_SUBTRACT,'-',0x4a2d},
 {0,VK_NUMPAD4,0,0x4b00},
 {NUMLOCK_ON,VK_NUMPAD4,'4',0x0534},
 {0,VK_NUMPAD5,0,0x4c00},
 {NUMLOCK_ON,VK_NUMPAD5,'5',0x0635},
 {0,VK_NUMPAD6,0,0x4d00},
 {NUMLOCK_ON,VK_NUMPAD6,'6',0x0736},
 {0,VK_ADD,'+',0x4e2b},
 {0,VK_NUMPAD1,0,0x4f00},
 {NUMLOCK_ON,VK_NUMPAD1,'1',0x0231},
 {0,VK_NUMPAD2,0,0x5000},
 {NUMLOCK_ON,VK_NUMPAD2,'2',0x0332},
 {0,VK_NUMPAD3,0,0x5100},
 {NUMLOCK_ON,VK_NUMPAD3,'3',0x0433},
 {0,VK_NUMPAD0,0,0x5200},
 {NUMLOCK_ON,VK_NUMPAD0,'0',0x0B30},
 {0,VK_DECIMAL,0,0x5300},
 {0,VK_RETURN,0,0xe00d},                 // gray - same
 {0,0xbf,'/',0xe02f},                    // ??? VK_DIVIDE is false in win98se
 {0,VK_DIVIDE,'/',0xe02f},
 {0,VK_HOME,0,0x47e0},
 {0,VK_UP,0,0x48e0},
 {0,VK_PGUP,0,0x49e0},
 {0,VK_LEFT,0,0x4be0},
 {0,VK_RIGHT,0,0x4de0},
 {0,VK_END,0,0x4fe0},
 {0,VK_DOWN,0,0x50e0},
 {0,VK_PGDN,0,0x51e0},
 {0,VK_INSERT,0,0x52e0},
 {0,VK_DELETE,0,0x53e0},

 {CTRLKEY_CTRL,VK_ESCAPE,0,0x011b},      // same
 {CTRLKEY_CTRL,'1',0,0xffff},
 {CTRLKEY_CTRL,'2',0,0x0300},
 {CTRLKEY_CTRL,'3',0,0xffff},
 {CTRLKEY_CTRL,'4',0,0xffff},
 {CTRLKEY_CTRL,'5',0,0xffff},
 {CTRLKEY_CTRL,'6',0,0x071e},
 {CTRLKEY_CTRL,'7',0,0xffff},
 {CTRLKEY_CTRL,'8',0,0xffff},
 {CTRLKEY_CTRL,'9',0,0xffff},
 {CTRLKEY_CTRL,'0',0,0xffff},
 {CTRLKEY_CTRL,0xbd,0,0x0C1f},
 {CTRLKEY_CTRL,0xbb,0,0xffff},
 {CTRLKEY_CTRL,VK_BACK,0,0x0e7f},
 {CTRLKEY_CTRL,VK_TAB,0,0x9400},
 {CTRLKEY_CTRL,'Q',0,0x1011},
 {CTRLKEY_CTRL,'W',0,0x1117},
 {CTRLKEY_CTRL,'E',0,0x1205},
 {CTRLKEY_CTRL,'R',0,0x1312},
 {CTRLKEY_CTRL,'T',0,0x1414},
 {CTRLKEY_CTRL,'Y',0,0x1519},
 {CTRLKEY_CTRL,'U',0,0x1615},
 {CTRLKEY_CTRL,'I',0,0x1709},
 {CTRLKEY_CTRL,'O',0,0x180f},
 {CTRLKEY_CTRL,'P',0,0x1910},
 {CTRLKEY_CTRL,0xdb,0,0x1A1B},
 {CTRLKEY_CTRL,0xdd,0,0x1B1D},
 {CTRLKEY_CTRL,VK_RETURN,0,0x1c0a},
 {CTRLKEY_CTRL,'A',0,0x1E01},
 {CTRLKEY_CTRL,'S',0,0x1F13},
 {CTRLKEY_CTRL,'D',0,0x2004},
 {CTRLKEY_CTRL,'F',0,0x2106},
 {CTRLKEY_CTRL,'G',0,0x2207},
 {CTRLKEY_CTRL,'H',0,0x2308},
 {CTRLKEY_CTRL,'J',0,0x240A},
 {CTRLKEY_CTRL,'K',0,0x250B},
 {CTRLKEY_CTRL,'L',0,0x260C},
 {CTRLKEY_CTRL,0xba,0,0xffff},
 {CTRLKEY_CTRL,0xde,0,0xffff},
 {CTRLKEY_CTRL,0xc0,0,0xffff},
 {CTRLKEY_CTRL,0xdc,0,0x2B1C},
 {CTRLKEY_CTRL,'Z',0,0x2C1A},
 {CTRLKEY_CTRL,'X',0,0x2D18},
 {CTRLKEY_CTRL,'C',0,0x2E03},
 {CTRLKEY_CTRL,'V',0,0x2F16},
 {CTRLKEY_CTRL,'B',0,0x3002},
 {CTRLKEY_CTRL,'N',0,0x310E},
 {CTRLKEY_CTRL,'M',0,0x320D},
 {CTRLKEY_CTRL,0xbc,0,0xffff},
 {CTRLKEY_CTRL,0xbe,0,0xffff},
 //{CTRLKEY_CTRL,0xbf,0,0xffff},        // removed white, preferred gray
 {CTRLKEY_CTRL,VK_MULTIPLY,0,0x9600},
 {CTRLKEY_CTRL,' ',0,0x3920},           // same
 {CTRLKEY_CTRL,VK_F1,0,0x5e00},
 {CTRLKEY_CTRL,VK_F2,0,0x5f00},
 {CTRLKEY_CTRL,VK_F3,0,0x6000},
 {CTRLKEY_CTRL,VK_F4,0,0x6100},
 {CTRLKEY_CTRL,VK_F5,0,0x6200},
 {CTRLKEY_CTRL,VK_F6,0,0x6300},
 {CTRLKEY_CTRL,VK_F7,0,0x6400},
 {CTRLKEY_CTRL,VK_F8,0,0x6500},
 {CTRLKEY_CTRL,VK_F9,0,0x6600},
 {CTRLKEY_CTRL,VK_F10,0,0x6700},
 {CTRLKEY_CTRL,VK_F11,0,0x8900},
 {CTRLKEY_CTRL,VK_F12,0,0x8a00},
 {CTRLKEY_CTRL,VK_NUMPAD7,0,0x7700},
 {CTRLKEY_CTRL,VK_NUMPAD8,0,0x8d00},
 {CTRLKEY_CTRL,VK_NUMPAD9,0,0x8400},
 {CTRLKEY_CTRL,VK_SUBTRACT,0,0x8e00},
 {CTRLKEY_CTRL,VK_NUMPAD4,0,0x7300},
 {CTRLKEY_CTRL,VK_NUMPAD5,0,0x8f00},
 {CTRLKEY_CTRL,VK_NUMPAD6,0,0x7400},
 {CTRLKEY_CTRL,VK_ADD,0,0x9000},
 {CTRLKEY_CTRL,VK_NUMPAD1,0,0x7500},
 {CTRLKEY_CTRL,VK_NUMPAD2,0,0x9100},
 {CTRLKEY_CTRL,VK_NUMPAD3,0,0x7600},
 {CTRLKEY_CTRL,VK_NUMPAD0,0,0x9200},
 {CTRLKEY_CTRL,VK_DECIMAL,0,0x9300},
 {CTRLKEY_CTRL,VK_RETURN,0,0xe00a},    // gray - same like white
 {CTRLKEY_CTRL,0xbf,0,0x9500},         // ??? VK_DIVIDE is false in win98se
 {CTRLKEY_CTRL,VK_DIVIDE,0,0x9500},
 {CTRLKEY_CTRL,VK_HOME,0,0x77e0},
 {CTRLKEY_CTRL,VK_UP,0,0x8de0},
 {CTRLKEY_CTRL,VK_PGUP,0,0x84e0},
 {CTRLKEY_CTRL,VK_LEFT,0,0x73e0},
 {CTRLKEY_CTRL,VK_RIGHT,0,0x74e0},
 {CTRLKEY_CTRL,VK_END,0,0x75e0},
 {CTRLKEY_CTRL,VK_DOWN,0,0x91e0},
 {CTRLKEY_CTRL,VK_PGDN,0,0x76e0},
 {CTRLKEY_CTRL,VK_INSERT,0,0x92e0},
 {CTRLKEY_CTRL,VK_DELETE,0,0x93e0},

 {CTRLKEY_ALT,VK_ESCAPE,0,0x0100},
 {CTRLKEY_ALT,'1','1',0x7800},
 {CTRLKEY_ALT,'2','2',0x7900},
 {CTRLKEY_ALT,'3','3',0x7a00},
 {CTRLKEY_ALT,'4','4',0x7b00},
 {CTRLKEY_ALT,'5','5',0x7c00},
 {CTRLKEY_ALT,'6','6',0x7d00},
 {CTRLKEY_ALT,'7','7',0x7e00},
 {CTRLKEY_ALT,'8','8',0x7f00},
 {CTRLKEY_ALT,'9','9',0x8000},
 {CTRLKEY_ALT,'0','0',0x8100},
 {CTRLKEY_ALT,0xbd,'-',0x8200},
 {CTRLKEY_ALT,0xbb,'=',0x8300},
 {CTRLKEY_ALT,VK_BACK,0,0x0e00},
 //{CTRLKEY_ALT,VK_TAB,0,0xa500}, // windows special key
 {CTRLKEY_ALT,'Q','q',0x1000},
 {CTRLKEY_ALT,'W','w',0x1100},
 {CTRLKEY_ALT,'E','e',0x1200},
 {CTRLKEY_ALT,'R','r',0x1300},
 {CTRLKEY_ALT,'T','t',0x1400},
 {CTRLKEY_ALT,'Y','y',0x1500},
 {CTRLKEY_ALT,'U','u',0x1600},
 {CTRLKEY_ALT,'I','i',0x1700},
 {CTRLKEY_ALT,'O','o',0x1800},
 {CTRLKEY_ALT,'P','p',0x1900},
 {CTRLKEY_ALT,0xdb,'[',0x1A00},
 {CTRLKEY_ALT,0xdd,']',0x1B00},
 //{CTRLKEY_ALT,VK_RETURN,0,0x1c00}, // windows special key
 {CTRLKEY_ALT,'A','a',0x1E00},
 {CTRLKEY_ALT,'S','s',0x1F00},
 {CTRLKEY_ALT,'D','d',0x2000},
 {CTRLKEY_ALT,'F','f',0x2100},
 {CTRLKEY_ALT,'G','g',0x2200},
 {CTRLKEY_ALT,'H','h',0x2300},
 {CTRLKEY_ALT,'J','j',0x2400},
 {CTRLKEY_ALT,'K','k',0x2500},
 {CTRLKEY_ALT,'L','l',0x2600},
 {CTRLKEY_ALT,0xba,';',0x2700},
 {CTRLKEY_ALT,0xde,'\'',0x2800},
 {CTRLKEY_ALT,0xc0,'`',0x2900},
 {CTRLKEY_ALT,0xdc,'\\',0x2B00},
 {CTRLKEY_ALT,'Z','z',0x2C00},
 {CTRLKEY_ALT,'X','x',0x2D00},
 {CTRLKEY_ALT,'C','c',0x2E00},
 {CTRLKEY_ALT,'V','v',0x2F00},
 {CTRLKEY_ALT,'B','b',0x3000},
 {CTRLKEY_ALT,'N','n',0x3100},
 {CTRLKEY_ALT,'M','m',0x3200},
 {CTRLKEY_ALT,0xbc,',',0x33f0},
 {CTRLKEY_ALT,0xbe,'.',0x34f0},
 //{CTRLKEY_ALT,0xbf,'/',0x35F0},        // removed white, preferred gray
 {CTRLKEY_ALT,VK_MULTIPLY,'*',0x37F0},
 {CTRLKEY_ALT,' ',' ',0x3920},           // same
 {CTRLKEY_ALT,VK_F1,0,0x6800},
 {CTRLKEY_ALT,VK_F2,0,0x6900},
 {CTRLKEY_ALT,VK_F3,0,0x6A00},
 {CTRLKEY_ALT,VK_F4,0,0x6B00},
 {CTRLKEY_ALT,VK_F5,0,0x6C00},
 {CTRLKEY_ALT,VK_F6,0,0x6D00},
 {CTRLKEY_ALT,VK_F7,0,0x6E00},
 {CTRLKEY_ALT,VK_F8,0,0x6F00},
 {CTRLKEY_ALT,VK_F9,0,0x7000},
 {CTRLKEY_ALT,VK_F10,0,0x7100},
 {CTRLKEY_ALT,VK_F11,0,0x8B00},
 {CTRLKEY_ALT,VK_F12,0,0x8C00},
 //{CTRLKEY_ALT,VK_NUMPAD7,0,0x4700},   // spec keys
 //{CTRLKEY_ALT,VK_NUMPAD8,0,0x4800},
 //{CTRLKEY_ALT,VK_NUMPAD9,0,0x4900},
 {CTRLKEY_ALT,VK_SUBTRACT,'-',0x4AF0},
 //{CTRLKEY_ALT,VK_NUMPAD4,0,0x4b00},
 //{CTRLKEY_ALT,VK_NUMPAD5,0,0x4c00},
 //{CTRLKEY_ALT,VK_NUMPAD6,0,0x4d00},
 {CTRLKEY_ALT,VK_ADD,'+',0x4EF0},
 //{CTRLKEY_ALT,VK_NUMPAD1,0,0x4f00},
 //{CTRLKEY_ALT,VK_NUMPAD2,0,0x5000},
 //{CTRLKEY_ALT,VK_NUMPAD3,0,0x5100},
 //{CTRLKEY_ALT,VK_NUMPAD0,0,0x5200},
 {CTRLKEY_ALT,VK_DECIMAL,0,0xffff},
 {CTRLKEY_ALT,VK_RETURN,0,0xA600},
 {CTRLKEY_ALT,0xbf,'/',0xA400},         // ??? VK_DIVIDE is false in win98se
 {CTRLKEY_ALT,VK_DIVIDE,'/',0xA400},
 {CTRLKEY_ALT,VK_HOME,0,0x9700},
 {CTRLKEY_ALT,VK_UP,0,0x9800},
 {CTRLKEY_ALT,VK_PGUP,0,0x9900},
 {CTRLKEY_ALT,VK_LEFT,0,0x9B00},
 {CTRLKEY_ALT,VK_RIGHT,0,0x9D00},
 {CTRLKEY_ALT,VK_END,0,0x9F00},
 {CTRLKEY_ALT,VK_DOWN,0,0xA000},
 {CTRLKEY_ALT,VK_PGDN,0,0xA100},
 {CTRLKEY_ALT,VK_INSERT,0,0xA200},
 {CTRLKEY_ALT,VK_DELETE,0,0xA300},

 // shift is last, else alt & ctrl keycodes are not handled well if capslock is on
 {SHIFT_PRESSED,VK_ESCAPE,0,0x011b},     // same
 {SHIFT_PRESSED,'1','!',0x0221},
 {SHIFT_PRESSED,'2','@',0x0340},
 {SHIFT_PRESSED,'3','#',0x0423},
 {SHIFT_PRESSED,'4','$',0x0524},
 {SHIFT_PRESSED,'5','%',0x0625},
 {SHIFT_PRESSED,'6','^',0x075E},
 {SHIFT_PRESSED,'7','&',0x0826},
 {SHIFT_PRESSED,'8','*',0x092A},
 {SHIFT_PRESSED,'9','(',0x0A28},
 {SHIFT_PRESSED,'0',')',0x0B29},
 {SHIFT_PRESSED,0xbd,'_',0x0C5F},
 {SHIFT_PRESSED,0xbb,'+',0x0D2B},
 {CTRLKEY_SHIFT,'Q','Q',0x1051},
 {CTRLKEY_SHIFT,'W','W',0x1157},
 {CTRLKEY_SHIFT,'E','E',0x1245},
 {CTRLKEY_SHIFT,'R','R',0x1352},
 {CTRLKEY_SHIFT,'T','T',0x1454},
 {CTRLKEY_SHIFT,'Y','Y',0x1559},
 {CTRLKEY_SHIFT,'U','U',0x1655},
 {CTRLKEY_SHIFT,'I','I',0x1749},
 {CTRLKEY_SHIFT,'O','O',0x184F},
 {CTRLKEY_SHIFT,'P','P',0x1950},
 {SHIFT_PRESSED,0xdb,'{',0x1A7B},
 {SHIFT_PRESSED,0xdd,'}',0x1B7D},
 {CTRLKEY_SHIFT,'A','A',0x1E41},
 {CTRLKEY_SHIFT,'S','S',0x1F53},
 {CTRLKEY_SHIFT,'D','D',0x2044},
 {CTRLKEY_SHIFT,'F','F',0x2146},
 {CTRLKEY_SHIFT,'G','G',0x2247},
 {CTRLKEY_SHIFT,'H','H',0x2348},
 {CTRLKEY_SHIFT,'J','J',0x244A},
 {CTRLKEY_SHIFT,'K','K',0x254B},
 {CTRLKEY_SHIFT,'L','L',0x264C},
 {SHIFT_PRESSED,0xba,':',0x273A},
 {SHIFT_PRESSED,0xde,'\"',0x2822},
 {SHIFT_PRESSED,0xc0,'~',0x297E},
 {SHIFT_PRESSED,0xdc,'|',0x2B7C},
 {CTRLKEY_SHIFT,'Z','Z',0x2C5A},
 {CTRLKEY_SHIFT,'X','X',0x2D58},
 {CTRLKEY_SHIFT,'C','C',0x2E43},
 {CTRLKEY_SHIFT,'V','V',0x2F56},
 {CTRLKEY_SHIFT,'B','B',0x3042},
 {CTRLKEY_SHIFT,'N','N',0x314E},
 {CTRLKEY_SHIFT,'M','M',0x324D},
 {SHIFT_PRESSED,0xbc,'<',0x333C},
 {SHIFT_PRESSED,0xbe,'>',0x343E},
 {SHIFT_PRESSED,0xbf,'/',0x353F},        // !!! '?' char (shift-white-'/')
 {SHIFT_PRESSED,VK_MULTIPLY,'*',0x372A}, // same
 {SHIFT_PRESSED,' ',' ',0x3920},         // same
 {SHIFT_PRESSED,VK_F1,0,0x5400},
 {SHIFT_PRESSED,VK_F2,0,0x5500},
 {SHIFT_PRESSED,VK_F3,0,0x5600},
 {SHIFT_PRESSED,VK_F4,0,0x5700},
 {SHIFT_PRESSED,VK_F5,0,0x5800},
 {SHIFT_PRESSED,VK_F6,0,0x5900},
 {SHIFT_PRESSED,VK_F7,0,0x5a00},
 {SHIFT_PRESSED,VK_F8,0,0x5b00},
 {SHIFT_PRESSED,VK_F9,0,0x5c00},
 {SHIFT_PRESSED,VK_F10,0,0x5d00},
 {SHIFT_PRESSED,VK_F11,0,0x8700},
 {SHIFT_PRESSED,VK_F12,0,0x8800},
 {SHIFT_PRESSED,VK_NUMPAD7,0,0x4737},
 {SHIFT_PRESSED,VK_NUMPAD8,0,0x4838},
 {SHIFT_PRESSED,VK_NUMPAD9,0,0x4939},
 {SHIFT_PRESSED,VK_SUBTRACT,'-',0x4a2d}, // same
 {SHIFT_PRESSED,VK_NUMPAD4,0,0x4b34},
 {SHIFT_PRESSED,VK_NUMPAD5,0,0x4c35},
 {SHIFT_PRESSED,VK_NUMPAD6,0,0x4d36},
 {SHIFT_PRESSED,VK_ADD,'+',0x4e2b},      // same
 {SHIFT_PRESSED,VK_NUMPAD1,0,0x4f31},
 {SHIFT_PRESSED,VK_NUMPAD2,0,0x5032},
 {SHIFT_PRESSED,VK_NUMPAD3,0,0x5133},
 {SHIFT_PRESSED,VK_NUMPAD0,0,0x5230},
 {SHIFT_PRESSED,VK_DECIMAL,0,0x532e},
 {SHIFT_PRESSED,VK_RETURN,0,0xe00d},     // same like without SHIFT
 //{SHIFT_PRESSED,0xbf,'/',0xe02f},        // preffered '?' (VK_DIVIDE is false in win98se)
 {SHIFT_PRESSED,VK_DIVIDE,'/',0xe02f},   // same
 {SHIFT_PRESSED,VK_HOME,0,0x47e0},       // same
 {SHIFT_PRESSED,VK_UP,0,0x48e0},         // same
 {SHIFT_PRESSED,VK_PGUP,0,0x49e0},       // same
 {SHIFT_PRESSED,VK_LEFT,0,0x4be0},       // same
 {SHIFT_PRESSED,VK_RIGHT,0,0x4de0},      // same
 {SHIFT_PRESSED,VK_END,0,0x4fe0},        // same
 {SHIFT_PRESSED,VK_DOWN,0,0x50e0},       // same
 {SHIFT_PRESSED,VK_PGDN,0,0x51e0},       // same
 {SHIFT_PRESSED,VK_INSERT,0,0x52e0},     // same
 {SHIFT_PRESSED,VK_DELETE,0,0x53e0},     // same

 {0,0,0,0x0000}};

unsigned int newfunc_keyboard_winkey_to_extkey(unsigned int control,unsigned int virtkeycode,char asciicode)
{
 winkey_to_extkey_t *table;
 if(!virtkeycode && !asciicode)
  return 0;
 if((control&NUMLOCK_ON) && !(control&(CTRLKEY_CTRL|CTRLKEY_ALT)) && (asciicode>='0') && (asciicode<='9')) // hack
  control&=NUMLOCK_ON;
 else
  control&=CTRLKEY_ALL;

retry:
 table=&wincodes[0];
 do{
  if(!control || (table->control&control)){
   if(virtkeycode){
    if(table->virtualkey==virtkeycode)
     return table->extkey;
   }else
    if(asciicode && table->ascii==asciicode)
     return table->extkey;
  }
  table++;
 }while(table->extkey);

 if(control&CAPSLOCK_ON){               // if entry not found with CAPSLOCK ON
  funcbit_disable(control,CAPSLOCK_ON); //
  goto retry;                           // better idea?
 }

 return 0;
}

#endif // MPXPLAY_WIN32
