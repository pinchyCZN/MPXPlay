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
//function: visual plugin (DLL) handling

#include "mpxplay.h"
#include "newfunc\newfunc.h"
#include "newfunc\dll_load.h"
#include "visualpi.h"

#ifdef MPXPLAY_LINK_DLLLOAD

#include "display.h"

#define MPXPLAY_VISUALPI_DEFAULT_FPS 18

typedef struct plugin_info_s{
 mpxplay_module_entry_s *visual_module;
 display_visual_func_s *funcs;
 void *private_data;
}plugin_info_s;

static void visualplugin_start(display_visual_data_s *vds);
static void visualplugin_draw(struct plugin_info_s *pis);

extern unsigned long textscreen_linear_address;
extern unsigned int refdisp,displaymode;

static struct plugin_info_s plugin_infos;
static mpxplay_module_entry_s *dllmod;
static unsigned long save_displaymode,save_tsla;//,save_vsla;
static unsigned long last_displaymode;//,last_tsla,last_vsla;

#endif // MPXPLAY_LINK_DLLLOAD

char *display_visual_plugin_selectname;       // -vps
unsigned int display_visual_plugin_starttime; // -vpt secs

void display_visualpi_init(display_visual_data_s *vds)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 if(display_visual_plugin_starttime){
  mpxplay_timer_addfunc(&visualplugin_start,vds,MPXPLAY_TIMERTYPE_WAKEUP,mpxplay_timer_secs_to_counternum(display_visual_plugin_starttime));
  mpxplay_timer_addfunc(&display_visualplugin_stop,vds,MPXPLAY_TIMERTYPE_SIGNAL|MPXPLAY_TIMERTYPE_REPEAT,MPXPLAY_SIGNALTYPE_USER);
 }
#endif
}

void display_visualpi_close(void)
{
#ifdef MPXPLAY_LINK_DLLLOAD
 mpxplay_timer_deletefunc(&visualplugin_start,NULL);
 mpxplay_timer_deletefunc(&display_visualplugin_stop,NULL);
 display_visualplugin_stop(NULL); // close and no restart
#endif
}

#ifdef MPXPLAY_LINK_DLLLOAD

static void visualplugin_start(display_visual_data_s *vds)
{
 struct plugin_info_s *pis=&plugin_infos;
 unsigned int retry=2;

 if(displaymode&DISP_GRAPHICAL) // !!!
  return;

 do{
  dllmod=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_DISPLAY_VISUALPI,0,display_visual_plugin_selectname,dllmod);
  if(dllmod){
   if(dllmod->module_structure_version==MPXPLAY_DLLMODULEVER_DISPLAY_VISUALPI){
    pis->visual_module=dllmod;
    pis->funcs=(display_visual_func_s *)dllmod->module_callpoint;
    break;
   }
  }else
   retry--;
 }while(retry && !display_visual_plugin_selectname); // if last+1 then dllmod==NULL, restart loading from first module (to cycle modules sequentially)

 if(pis->funcs){
  display_visual_func_s *pif=pis->funcs;
  if(pif->init){

   // !!!
   save_displaymode=displaymode;
   save_tsla=textscreen_linear_address;
   //save_vsla=videoscreen_linear_address;

   vds->displaymode=displaymode;
   vds->textscreen_linear_address=textscreen_linear_address;
   //vds->videoscreen_linear_address=videoscreen_linear_address;

   pis->private_data=pif->init(vds);
   if(pis->private_data){
    unsigned int ticks;
    if(pif->start)
     if(pif->start(pis->private_data)>0){
      ticks=mpxplay_timer_secs_to_counternum(1);
      if(pif->fps)
       ticks/=pif->fps;
      else
       ticks/=MPXPLAY_VISUALPI_DEFAULT_FPS;
      mpxplay_timer_addfunc(&visualplugin_draw,pis,MPXPLAY_TIMERTYPE_REPEAT,ticks);

      // !!!
      displaymode=vds->displaymode;
      if(vds->textscreen_linear_address)
       textscreen_linear_address=vds->textscreen_linear_address;
      //if(vds->videoscreen_linear_address)
      // videoscreen_linear_address=vds->videoscreen_linear_address;
     }
   }
   last_displaymode=displaymode;
   //last_tsla=textscreen_linear_address;
   //last_vsla=videoscreen_linear_address;
  }
 }
}

void display_visualplugin_stop(display_visual_data_s *vds)
{
 struct plugin_info_s *pis=&plugin_infos;

 mpxplay_timer_deletefunc(&visualplugin_draw,pis);

 if(pis->funcs){
  if(pis->private_data){
   if(pis->funcs->stop)
    pis->funcs->stop(pis->private_data);
   if(pis->funcs->close)
    pis->funcs->close(pis->private_data);
   pis->private_data=NULL;

   // !!!
   if(displaymode!=last_displaymode){
    unsigned long diffbits=displaymode^last_displaymode;
    funcbit_disable(save_displaymode,diffbits);
    save_displaymode|=(displaymode&diffbits);
    last_displaymode=displaymode;
   }
   // ???
   //if(textscreen_linear_address!=last_tsla)
   // last_tsla=save_tsla=textscreen_linear_address;
   //if(videoscreen_linear_address!=last_vsla)
   // last_vsla=save_vsla=videoscreen_linear_address;

   displaymode=save_displaymode;
   textscreen_linear_address=save_tsla;
   //videoscreen_linear_address=save_vsla;

   refdisp|=RDT_INIT_FULL|RDT_BROWSER|RDT_EDITOR|RDT_OPTIONS|RDT_HEADER|RDT_ID3INFO; // refresh full desktop
  }
  pis->funcs=NULL;
 }

 if(pis->visual_module){
  if(!display_visual_plugin_selectname)
   newfunc_dllload_disablemodule(MPXPLAY_DLLMODULETYPE_DISPLAY_VISUALPI,0,NULL,pis->visual_module);
  pis->visual_module=NULL;
 }

 if(vds) // re-start timing (called from timer, else called from display_visualpi_close())
  display_visualpi_init(vds);
}

static void visualplugin_draw(struct plugin_info_s *pis)
{
 if(displaymode!=last_displaymode){
  unsigned long diffbits=displaymode^last_displaymode;
  funcbit_disable(save_displaymode,diffbits);
  funcbit_copy(save_displaymode,displaymode,diffbits);
  last_displaymode=displaymode;
 }
 //???
 //if(textscreen_linear_address!=last_tsla)
 // last_tsla=save_tsla=textscreen_linear_address;
 //if(videoscreen_linear_address!=last_vsla)
 // last_vsla=save_vsla=videoscreen_linear_address;

 if(pis->funcs->draw && pis->private_data)
  pis->funcs->draw(pis->private_data);
}

#endif // MPXPLAY_LINK_DLLLOAD

