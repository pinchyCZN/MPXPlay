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
//function: videoout main functions

#include "mpxplay.h"
#include "newfunc\newfunc.h"
#include "newfunc\dll_load.h"
#include "videoout.h"

extern mpxplay_videoout_func_s VESA_videoout_funcs;

void mpxplay_videoout_init(struct mpxplay_videoout_info_s *voi)
{
 char sout[100];

 if(pds_strlicmp(voi->config_screenhandler_name,"NONE")==0)
  return;

 if(!voi->config_res_x)
  voi->config_res_x=VIDEOSCREEN_DEFAULT_RESX;
 if(!voi->config_bpp)
  voi->config_bpp=VIDEOSCREEN_DEFAULT_BITS;

 if(pds_strlicmp(voi->config_screenhandler_name,"AUTO")==0)
  voi->config_screenhandler_name=NULL;

 if((pds_strlicmp(voi->config_screenhandler_name,"VESA")==0) || !voi->config_screenhandler_name){
  voi->screen_handler=&VESA_videoout_funcs;
  if(!voi->screen_handler->init || !voi->screen_handler->init(voi)){
   voi->screen_handler=NULL;
   if(voi->config_screenhandler_name){
    sprintf(sout,"Couldn't initialize %s video output",voi->config_screenhandler_name);
    pds_textdisplay_printf(sout);
    mpxplay_close_program(0);
   }
  }
 }

#ifdef MPXPLAY_LINK_DLLLOAD
 if(!voi->screen_handler){
  if(voi->config_screenhandler_name){
   mpxplay_module_entry_s *dll_videoout=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_VIDEOOUT,0,voi->config_screenhandler_name,NULL);
   if(dll_videoout){
    if(dll_videoout->module_structure_version==MPXPLAY_DLLMODULEVER_VIDEOOUT){ // !!!
     voi->screen_handler=(struct mpxplay_videoout_func_s *)dll_videoout->module_callpoint;
    }else{
     sprintf(sout,"Cannot handle videoout DLL module (old structure) : %s",voi->config_screenhandler_name);
     pds_textdisplay_printf(sout);
     mpxplay_close_program(0);
    }
   }else{
    sprintf(sout,"Unknown videoout module name : %s",voi->config_screenhandler_name);
    pds_textdisplay_printf(sout);
    mpxplay_close_program(0);
   }
   if(!voi->screen_handler->init || !voi->screen_handler->init(voi)){
    sprintf(sout,"Couldn't initialize %s video output",voi->config_screenhandler_name);
    pds_textdisplay_printf(sout);
    mpxplay_close_program(0);
   }
  }else{
   mpxplay_module_entry_s *dll_videoout=NULL;
   do{
    dll_videoout=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_VIDEOOUT,0,NULL,dll_videoout);
    if(dll_videoout){
     if(dll_videoout->module_structure_version==MPXPLAY_DLLMODULEVER_VIDEOOUT){ // !!!
      voi->screen_handler=(struct mpxplay_videoout_func_s *)dll_videoout->module_callpoint;
      if(voi->screen_handler->init && voi->screen_handler->init(voi))
       break;
      newfunc_dllload_disablemodule(0,0,NULL,dll_videoout);
     }
    }
   }while(dll_videoout);
  }
 }
#endif
}

void mpxplay_videoout_close(struct mpxplay_videoout_info_s *voi)
{
 if(voi->screen_handler && voi->screen_handler->close)
  voi->screen_handler->close(voi);
}

void mpxplay_videoout_listmodes(struct mpxplay_videoout_info_s *voi)
{
 char sout[100];

 if(!voi->screen_handler)
  mpxplay_videoout_init(voi);

 if(!voi->screen_handler){
  pds_textdisplay_printf("Videoout driver not found (or couldn't initialize)!");
  return;
 }
 if(!voi->screen_handler->listmodes){
  sprintf(sout,"Cannot list video modes (no such function in %s the driver)!",voi->screen_handler->name);
  pds_textdisplay_printf(sout);
  return;
 }
 voi->screen_handler->listmodes(voi);
}
