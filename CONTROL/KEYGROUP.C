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
//function: keygroup - execute multiply (more) key-functions with one button

#include "newfunc\newfunc.h"
#include "control.h"
#include "cntfuncs.h"

#define KEYGROUP_MAX_GROUPS 32
#define KEYGROUP_QUEUE_LEN  16

typedef struct{
 unsigned short buttoncode;                       // real keyboard code
 unsigned short queue_elements;                   // number of key-functions in the queue
 unsigned short keyfuncqueue[KEYGROUP_QUEUE_LEN]; // stored key-functions
}keygroup_func_t;

static keygroup_func_t keygroup_funcs[KEYGROUP_MAX_GROUPS];
static unsigned int keygroupnum;
static int group_select,keyfunc_select;

void mpxplay_control_keygroup_loadini(mpxini_line_t *mpxini_lines,struct mpxini_part_t *mpxini_partp)
{
 unsigned int i;
 mpxini_line_t *linep;
 keygroup_func_t *groupfuncp;

 linep=mpxini_lines+mpxini_partp->partbegin_linenum;
 groupfuncp=&keygroup_funcs[0];

 for(i=0;(i<mpxini_partp->partlinenum) && (keygroupnum<KEYGROUP_MAX_GROUPS);i++,linep++){
  if(linep->varnamep){
   groupfuncp->buttoncode=pds_atol16(linep->varnamep);
   if(groupfuncp->buttoncode){
    char *curr_queue_element=linep->valuep;
    while(curr_queue_element){
     char *next_queue_element;
     if(groupfuncp->queue_elements>=(KEYGROUP_QUEUE_LEN-1))
      break;
     next_queue_element=pds_strchr(curr_queue_element,',');
     if(next_queue_element)
      *next_queue_element++=0;
     groupfuncp->keyfuncqueue[groupfuncp->queue_elements]=pds_atol16(curr_queue_element);
     groupfuncp->queue_elements++;
     curr_queue_element=next_queue_element;
    }
    groupfuncp++;
    keygroupnum++;
   }
  }
 }
 group_select=-1;
}

int mpxplay_control_keygroup_getgroup(unsigned int extkey,struct mainvars *mvp)
{
 unsigned int i;
 keygroup_func_t *groupfuncp;

 group_select=-1;
 groupfuncp=&keygroup_funcs[0];

 for(i=0;i<keygroupnum;i++){
  if((unsigned int)groupfuncp->buttoncode==extkey){
   group_select=i;
   keyfunc_select=0;
   break;
  }
  groupfuncp++;
 }
 return group_select;
}

int mpxplay_control_keygroup_getnextfunc(void)
{
 keygroup_func_t *groupfuncp;

 if(group_select<0)
  return -1;

 groupfuncp=&keygroup_funcs[group_select];
 if(keyfunc_select>=groupfuncp->queue_elements){
  group_select=-1;
  return -1;
 }

 return ((unsigned int)groupfuncp->keyfuncqueue[keyfunc_select++]);
}
