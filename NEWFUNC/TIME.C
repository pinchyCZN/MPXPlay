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
// time,delay functions

#include <conio.h>
#include <time.h>
#include "newfunc.h"
#include "mpxplay.h"

#ifdef __DOS__
extern void (__far __interrupt *oldint08_handler)();
extern volatile unsigned long int08counter;
#endif

unsigned long pds_gettimeh(void)
{
 return ((unsigned long)clock()*100/CLOCKS_PER_SEC);
}

mpxp_uint64_t pds_gettimem(void)
{
 mpxp_uint64_t val;
#ifdef __DOS__
 unsigned long tsc;
 if(oldint08_handler){
  _disable();
  outp(0x43,0x04);
  tsc=inp(0x40);
  tsc+=inp(0x40)<<8;
  _enable();
  if(tsc<INT08_DIVISOR_NEW)
   tsc=INT08_DIVISOR_NEW-tsc;
  else
   tsc=0;
  val=(mpxp_int64_t)(((float)int08counter+(float)tsc/(float)INT08_DIVISOR_NEW)*1000.0/(float)INT08_CYCLES_NEW);
  //fprintf(stderr,"val:%d \n",val);
 }else
#endif
  val=((mpxp_uint64_t)clock()*1000/CLOCKS_PER_SEC);

 return val;
}

mpxp_uint64_t pds_gettimeu(void)
{
#ifdef __DOS__
 mpxp_int64_t val;
 unsigned long tsc;
 if(oldint08_handler){
  _disable();
  outp(0x43,0x04);
  tsc=inp(0x40);
  tsc+=inp(0x40)<<8;
  _enable();
  if(tsc<INT08_DIVISOR_NEW)
   tsc=INT08_DIVISOR_NEW-tsc;
  else
   tsc=0;
  val=(mpxp_int64_t)(((float)int08counter+(float)tsc/(float)INT08_DIVISOR_NEW)*1000000.0/(float)INT08_CYCLES_NEW);
  //fprintf(stderr,"val:%d \n",(long)val);
 }else
  val=((mpxp_uint64_t)clock()*1000000/CLOCKS_PER_SEC);

 return val;
#else
 return ((mpxp_uint64_t)clock()*1000000/CLOCKS_PER_SEC); // 1ms precision
#endif
}

unsigned long pds_gettime(void)
{
 unsigned long timeval;
 time_t timer;
 struct tm *t;
 timer=time(NULL);
 t=localtime(&timer);
 timeval=(t->tm_sec&63)|((t->tm_min&63)<<8)|((t->tm_hour&31)<<16);
 return timeval; // 0x00HHMMSS
}

unsigned long pds_getdate(void)
{
 unsigned long dateval;
 time_t timer;
 struct tm *t;
 timer=time(NULL);
 t=localtime(&timer);
 dateval=(t->tm_mday&31)|(((t->tm_mon+1)&15)<<8)|(((t->tm_year+1900)&65535)<<16);
 return dateval; // 0xYYYYMMDD
}

// "hh:mm:ss" to 0x00hhmmss
unsigned long pds_strtime_to_hextime(char *timestr,unsigned int houralign)
{
 unsigned long hextime=0,i=0;
 char tmp[300];

 if(!pds_strcpy(tmp,timestr))
  return 0;

 timestr=&tmp[0];
 do{
  char *p=pds_strchr(timestr,':');
  if(p)
   *p++=0;
  hextime<<=8;
  hextime|=pds_atol(timestr)&0xff;
  timestr=p;
 }while(timestr && (++i<3));

 if(houralign){
  if(i<2)
   hextime<<=8*(2-i);
 }

 return hextime;
}

// "hh:mm:ss.nn" to 0xhhmmssnn
unsigned long pds_strtime_to_hexhtime(char *timestr)
{
 static char separators[4]="::.";
 unsigned long hextime=0,i=0,val;
 char *next,tmp[300];

 if(!pds_strcpy(tmp,timestr))
  return 0;

 timestr=&tmp[0];
 do{
  char *p=pds_strchr(timestr,separators[i]);
  if(p)
   *p++=0;
  if(next){
   hextime<<=8;
   val=pds_atol(timestr)&0xff;
   if(i==3 && val<10 && timestr[0]!='0')
    val*=10;
   hextime|=val;
  }else{
   if(i==3)
    hextime<<=8;
  }
  next=p;
  if(p)
   timestr=p;
 }while(++i<4);

 return hextime;
}

void pds_delay_10us(unsigned int ticks) //each tick is 10us
{
#ifdef __DOS__
 unsigned int divisor=(oldint08_handler)? INT08_DIVISOR_NEW:INT08_DIVISOR_DEFAULT; // ???
 unsigned int i,oldtsc, tsctemp, tscdif;

 for(i=0;i<ticks;i++){
  _disable();
  outp(0x43,0x04);
  oldtsc=inp(0x40);
  oldtsc+=inp(0x40)<<8;
  _enable();

  do{
   _disable();
   outp(0x43,0x04);
   tsctemp=inp(0x40);
   tsctemp+=inp(0x40)<<8;
   _enable();
   if(tsctemp<=oldtsc)
    tscdif=oldtsc-tsctemp; // handle overflow
   else
    tscdif=divisor+oldtsc-tsctemp;
  }while(tscdif<12); //wait for 10us  (12/(65536*18) sec)
 }
#else
 unsigned int oldclock=clock();
 while(oldclock==clock()){} // 1ms not 0.01ms (10us)
#endif
}
