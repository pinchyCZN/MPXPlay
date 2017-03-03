/* Converted to D from _wss.c by htod */
module __wss;
/*
#include <stdio.h>
#include <stdlib.h>
#include <i86.h>
#include <conio.h>
#include <dpmi.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>
*/

extern (C):
alias ubyte BYTE;
alias ushort WORD;
alias uint DWORD;

const CLOCK_RATE = 300000000;
DWORD base_reg;
int device_found;

const PCICMD = 0x04;
const MSE = 0x02;

const BME = 0x04;
//HDA PCI CONFIG REGISTERS
//HDA MEM MAP CONFIG REGISTERS
const HDBARL = 0x10;
const GCAP = 0x00;
const GCTL = 0x08;
const STATESTS = 0x0E;
const HDA_IC = 0x60;
const HDA_IR = 0x64;
const HDA_IRS = 0x68;
const OSD0CTL = 0xA0;
const OSD0STS = 0xA3;
const OSD0LPIB = 0xA4;
const OSD0CBL = 0xA8;
const OSD0LVI = 0xAC;
const OSD0FMT = 0xB2;
const OSD0BDPL = 0xB8;

const OSD0BDPU = 0xBC;
//ctrl bits
const SRST = 0x01;
const RUN = 0x02;

const BCIS = 0x04;

const CRST = 0x01;
const ICB = 0x01;

const IRV = 0x02;
const SET_AMP_GAIN = 0x003;
const SET_MUTE = 0x0080;
const SET_LEFT_AMP = 0x2000;
const SET_RIGHT_AMP = 0x1000;

const SET_OUT_AMP = 0x8000;


void  log_msg(char *,...);

DWORD  get_tick_count();
void  udelay(int );

int  get_msec(DWORD );
