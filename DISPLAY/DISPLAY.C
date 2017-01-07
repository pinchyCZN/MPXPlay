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
//function: display-screen handling

#include "newfunc\newfunc.h"
#include "display.h"
#include "au_cards\au_cards.h"
#include "control\cntfuncs.h"
#include "playlist\playlist.h"
#include "visualpi.h"
#include <mpxinbuf.h>

extern struct buttons dk[];
extern struct desktoppos dtp;

static struct editor_s ed[PLAYLIST_MAX_SIDES];

static void fullscreeninit(void);
static void draw_info_borders(void);
static void draw_button_boxes(void);
static void draw_button_intext(void);
static void draw_analiser_dots(void);
static void draw_editor_borders(void);
static void draw_songpos_line(void);
static void draw_listpos_line(void);
static void refresh_display(struct mainvars *mvp);
       void refresh_desktop(struct mainvars *mvp);
static void draw_songpos_head(struct mainvars *);
static void display_allframes(struct mainvars *);
static void display_timepos(struct mainvars *);
static void display_option_vol(struct mainvars *);
static void display_options(struct mainvars *);
static void display_fileinfos(struct mainvars *mvp);
static void display_id3infos(struct mainvars *mvp);
static void display_id3info_timer_init(void);
static void draw_one_id3info_line(struct playlist_entry_info *pei,unsigned int lastid,unsigned int col,unsigned int endcol,unsigned int color,unsigned int y,unsigned int showfilenameonly);
static void draw_volume_nofe(void);
static void draw_volume_fe(unsigned int);
static void clear_analiser_peeks(void);
static void draw_spectrum_analiser(unsigned long *analpt);
static void setmousecursorpalette(void);
static void init_editorpos(struct mainvars *,unsigned int force_reset);
static void display_editor_close(void);
static void draw_elevator_pos(struct playlist_side_info *,unsigned int side);
static unsigned int set_editorline_color(struct playlist_side_info *,struct playlist_entry_info *);

extern int MIXER_var_volume;
extern int MIXER_var_swapchan,MIXER_var_autovolume,MIXER_var_balance;
extern unsigned int intsoundcontrol,prebuffertype,playreplay,playrand,playcontrol;
extern unsigned int crossfadepart,editorsideborder,id3textconv;
extern unsigned int refdisp,displaymode,desktopmode,textscreen_maxx,textscreen_maxy,oldposrow;
extern unsigned int timemode;
extern unsigned long allcputime,allcpuusage,mpxplay_signal_events;
extern unsigned int analtabnum,mouse_on,lastmousebox;

static const char editorchars[10]="Ä³É»È¼ÂÁÌ¹";
//static const char editorchars[10]="Ä³Ú¿ÀÙÂÁÃ´";
//static const char editorchars[10]="ÍºÉ»È¼ËÊÌ¹";
static unsigned int lastmousec,lastmousex,lastmousey,lastsongpos;
static unsigned int allsongnum,currsongnum;
static unsigned int analtabdelay,lockid3window;
static char souttext[512];
static unsigned long vol_save[2];
unsigned long volnum[DISPLAY_ANALISER_MAXDELAY][2];
unsigned long analtab[DISPLAY_ANALISER_MAXDELAY][DISPLAY_ANALISER_BANDNUM];

//**************************************************************************
// desktop (display) init and close routines
//**************************************************************************
static unsigned int disp_init_ok;

void mpxplay_display_init(struct mainvars *mvp)
{
 unsigned int i;
 struct editor_s *edp;
 struct playlist_side_info *psi;
 display_visual_data_s *vds;

 pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_HIDDEN);  // disable cursor
 if(displaymode&DISP_FULLSCREEN){
  pds_textdisplay_vidmem_save();
  if(displaymode&DISP_50LINES)
   pds_textdisplay_setresolution(50);
 }else{
  if(displaymode&DISP_VERBOSE){
   oldposrow+=2;
   if(oldposrow>=textscreen_maxy){
    pds_textdisplay_scrollup(2);
    oldposrow=textscreen_maxy-1;
   }
  }
  dtp.timepos_fe=oldposrow;
 }
 analtabdelay=(mvp->aui->card_controlbits&AUINFOS_CARDCNTRLBIT_DOUBLEDMA)? 7:3;

 psi=mvp->psi0;
 edp=&ed[0];

 for(i=0;i<PLAYLIST_MAX_SIDES;i++,edp++,psi++)
  edp->from=psi->firstentry;

 mpxplay_display_buttons_init();

 mpxplay_timer_addfunc(&refresh_display,mvp,MPXPLAY_TIMERTYPE_REPEAT,mpxplay_timer_secs_to_counternum(1)/DISPLAY_REFRESH_FPS);
 mpxplay_timer_addfunc(&refresh_desktop,mvp,MPXPLAY_TIMERTYPE_REPEAT,1);

 display_id3info_timer_init();

 vds=mvp->vds;
 vds->channelnum=PCM_MAX_CHANNELS;
 vds->soundvolumes=&vol_save[0];
 vds->bandnum=32;
 vds->analbands=&analtab[0][0];
 vds->anal_freq_range=22050;
 vds->displaymode=displaymode;

 display_visualpi_init(vds);

 disp_init_ok=1;
}

void mpxplay_display_close(struct mainvars *mvp)
{
 struct mpxplay_videoout_info_s *voi;
 if(disp_init_ok){
  disp_init_ok=0;
  display_visualpi_close();
  voi=mvp->voi;
  if(voi->screen_handler && voi->screen_handler->reset) //
   if(voi->screen_handler->reset(voi))                  // !!!
    pds_textdisplay_setlastmode();
  mpxplay_videoout_close(voi);
  display_textwin_close();
  display_editor_close();
  if(displaymode&DISP_FULLSCREEN){
   pds_textdisplay_resetcolorpalette();
   pds_textdisplay_vidmem_restore();
  }else{
   if(displaymode&DISP_VERBOSE)
    pds_textdisplay_setcursor_position(0,max(oldposrow-1,dtp.timepos_fe));
  }
  pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_NORMAL);    // enable cursor
 }
}

void mpxplay_display_switch_to_textmode(struct mainvars *mvp)
{
 if(displaymode&DISP_GRAPHICAL){
  struct mpxplay_videoout_info_s *voi=mvp->voi;
  if(voi->screen_handler && voi->screen_handler->reset)
   voi->screen_handler->reset(voi);
  if(pds_textdisplay_setlastmode()){
   if(displaymode&DISP_50LINES)
    pds_textdisplay_setresolution(50);
   funcbit_disable(displaymode,DISP_GRAPHICAL);
  }
 }
}

void mpxplay_display_switch_to_graphmode(struct mainvars *mvp)
{
 if(!(displaymode&DISP_GRAPHICAL)){
  struct mpxplay_videoout_info_s *voi=mvp->voi;
  if(voi->screen_handler && voi->screen_handler->set)
   if(voi->screen_handler->set(voi,(playcontrol&PLAYC_RUNNING)))
    funcbit_enable(displaymode,DISP_GRAPHICAL);
 }
}

static void dtp_init_values(void)
{
 if(!(displaymode&DISP_FULLSCREEN))
  return;
 lastsongpos=1;
 pds_textdisplay_clrscr();
 if(displaymode&DISP_NOFULLEDIT){
  if((displaymode&DISP_50LINES) || (textscreen_maxy>50)){ // textscreen_maxy>50 : more than 50 lines (set externally, use -fs)
   dtp.endofbuttonsy=14;
   dtp.volline=14;
  }else{
   dtp.endofbuttonsy=10;
   dtp.volline=9;
  }
  clear_analiser_peeks();
 }else{
  if(displaymode&DISP_VERBOSE)
   dtp.timepos_fe=2;
  else
   dtp.timepos_fe=0;
  dtp.volline=dtp.timepos_fe;
  dtp.endofbuttonsy=dtp.timepos_fe-1;
  if(displaymode&DISP_TIMEPOS)
   dtp.endofbuttonsy++;
 }
 dtp.songposline_y=dtp.endofbuttonsy+1+dtp.relative_songposline;
 dtp.editorbegin=dtp.songposline_y;
 if(desktopmode&DTM_SONGPOS)
  dtp.editorbegin++;

 dtp.editorend=textscreen_maxy-1;
 if(desktopmode&DTM_LISTPOS){
  dtp.listposline_y=dtp.editorend;
  dtp.editorend--;
 }
 if(mouse_on){
  mpxplay_control_mouse_setrange((textscreen_maxx-1),textscreen_maxy-1);
  mpxplay_control_mouse_getpos(&lastmousex,&lastmousey);
  lastmousec=pds_textdisplay_getbkcolorxy(lastmousex,lastmousey);
  pds_textdisplay_setbkcolorxy(CL_MOUSECURSOR,lastmousex,lastmousey);
 }
 pds_textdisplay_setcursorshape(TEXTCURSORSHAPE_HIDDEN);
 lastmousebox=LASTMOUSEBOX_INVALID;
 display_editor_resize_y(0);
}

static void dtp_init_analiser_browser(void)
{
 draw_analiser_dots();
 generate_browserboxes();
}

static void dtp_init_browser(void)
{
 generate_browserboxes();
}

static void dtp_init_buttons(void)
{
 if((displaymode&DISP_FULLSCREEN) && (displaymode&DISP_NOFULLEDIT)){
  draw_info_borders();
  draw_button_boxes();
  draw_button_intext();
 }
}

static void dtp_init_editor(struct mainvars *mvp,unsigned int force_reset)
{
 if(displaymode&DISP_FULLSCREEN){
  init_editorpos(mvp,force_reset);
  draw_editor_borders();
  draw_listpos_line();
  draw_songpos_line();
 }
}

static void dtp_reset_editor(void)
{
 unsigned int i;
 struct editor_s *edp=&ed[0];
 for(i=0;i<PLAYLIST_MAX_SIDES;i++,edp++)
  edp->from_prev=NULL;
}

void display_editorside_reset(struct playlist_side_info *psi)
{
 int side=psi-psi->mvp->psi0;
 if((side<0) || (side>=PLAYLIST_MAX_SIDES))
  return;
 ed[side].from_prev=NULL;
}

void refresh_desktop(struct mainvars *mvp)
{
 if(!refdisp)
  return;
 if(refdisp&(RDT_HEADER|RDT_VOL|RDT_OPTIONS|RDT_EDITOR)){
  LCD_refresh_once(refdisp);
 }
 if(intsoundcontrol&INTSOUND_DOSSHELL)
  return;
 if(refdisp&RDT_INIT_FULL){
  if(refdisp&RDT_INIT_VALUES)
   dtp_init_values();
  if(refdisp&RDT_INIT_ANABRO)
   dtp_init_analiser_browser();
  if(refdisp&RDT_INIT_BROWS)
   dtp_init_browser();
  if(refdisp&RDT_INIT_BTN)
   dtp_init_buttons();
  if(refdisp&RDT_INIT_EDIT)
   dtp_init_editor(mvp,(refdisp&RDT_INIT_VALUES)); // !!!
  if(refdisp&RDT_RESET_EDIT)
   dtp_reset_editor();
  refdisp&=~RDT_INIT_FULL;
 }

 if(refdisp&RDT_HEADER){
  struct mpxplay_audioout_info_s *aui=mvp->aui;
  if(aui && aui->card_bytespersign && aui->freq_card){
   analtabdelay=(DISPLAY_REFRESH_FPS*aui->card_dmasize+(aui->card_bytespersign*aui->freq_card/2))/(aui->card_bytespersign*aui->freq_card);
   if(analtabdelay<1)
    analtabdelay=1;
   if(analtabdelay>=DISPLAY_ANALISER_MAXDELAY)
    analtabdelay=DISPLAY_ANALISER_MAXDELAY-1;
  }

  refdisp&=~RDT_HEADER;
  display_fileinfos(mvp);
  display_allframes(mvp);
 }
 if(refdisp&RDT_ID3INFO){
  refdisp&=~RDT_ID3INFO;
  display_id3infos(mvp);
 }
 if(refdisp&RDT_VOL){
  refdisp&=~RDT_VOL;
  display_option_vol(mvp);
 }
 if(refdisp&RDT_OPTIONS){
  refdisp&=~RDT_OPTIONS;
  display_options(mvp);
 }
 if(refdisp&RDT_BROWSER){
  struct playlist_side_info *psip=mvp->psip;
  refdisp&=~RDT_BROWSER;
  allsongnum=psip->lastentry-psip->firstsong+1;
  currsongnum=playlist_getsongcounter(mvp);
  playlist_fulltime_getelapsed(mvp,1);
  draw_listpos_line();
  drawbrowser(mvp);
  draw_mouse_desktoppos(mvp,lastmousex,lastmousey);
 }
 if(refdisp&RDT_EDITOR){
  refdisp&=~RDT_EDITOR;
  draweditor(mvp);
 }
}

//------------------------------------------------------------------------
// draw desktop (init)

static void draw_info_borders(void)
{
 static char infoborders[11][49]={
 {"ÚÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÂÄÄÄ¿"},
 {"³       ³        ³        ³       ³        ³   ³"},
 {"ÃÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÁÄÄÄ´"},
 {"³                                              ³"},
 {"³                                              ³"},
 {"ÃÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄ´"},
 {"³         ³             ³             ³        ³"},
 {"ÀÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÄÄÄÄÄ´        ³"},
 {"                                      ³        ³"},
 {"                                      ³        ³"},
 {"                                      ÀÄÄÄÄÄÄÄÄÙ"},
 };

 struct buttons *dp=&dk[0];
 unsigned int i;

 for(i=0;i<11;i++)
  pds_textdisplay_textxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos,dp->ypos+i,infoborders[i]);
}

static void draw_button_boxes(void)
{
 struct buttons *dp=&dk[DK_CONTROL];
 static const char boxstr[6]="Ä³Ú¿ÀÙ";
 //static const char boxstr[6]="ÍºÉ»È¼";
 while(dp->boxflag!=BTNF_ENDOFBTN){
  if((dp->boxflag&BTNF_DRAWBOX) && (dp->ypos<dtp.endofbuttonsy)){
   unsigned int j;
   for(j=0;j<=dp->xsize;j++){
    pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos+j,dp->ypos,boxstr[0]);
    pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos+j,dp->ypos+dp->ysize,boxstr[0]);
   }
   for(j=0;j<=dp->ysize;j++){
    pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos,dp->ypos+j,boxstr[1]);
    pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos+dp->xsize,dp->ypos+j,boxstr[1]);
   }
   pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos          ,dp->ypos,boxstr[2]);
   pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos+dp->xsize,dp->ypos,boxstr[3]);
   pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos,dp->ypos+dp->ysize,boxstr[4]);
   pds_textdisplay_charxybk(CL_BTNBOX,CLB_BTNBOX,dp->xpos+dp->xsize,dp->ypos+dp->ysize,boxstr[5]);
  }
  dp++;
 }
}

static void draw_button_intext(void)
{
 struct buttons *dp=&dk[DK_FILEINFO];
 while(dp->boxflag!=BTNF_ENDOFBTN){
  if((dp->intext!=NULL) && (dp->ypos<dtp.endofbuttonsy)){
   if(dp->boxflag&BTNF_UPTEXT)
    pds_textdisplay_textxybk(CL_BTNUPTEXT,CLB_BTNUPTEXT,dp->xpos+1,dp->ypos,dp->intext);
   else
    if(dp->boxflag>BTNF_NONE)
     pds_textdisplay_textxybk(CL_BTNTEXTCNTRL,CLB_BTNTEXT,dp->xpos+1,dp->ypos+1,dp->intext);
    else
     pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,dp->intext);
  }
  dp++;
 }
}

static void draw_analiser_dots(void)
{
 unsigned int i,j;
 if((displaymode&DISP_NOFULLEDIT) && (displaymode&DISP_ANALISER)){
  for(j=0;j<32;j++)
   for(i=0;i<=dtp.endofbuttonsy;i++)
    pds_textdisplay_charxybk(CL_ANALISER_DOTS,CLB_ANALISER,j,i,250);
  clear_analiser_peeks();
 }
}

static void draw_songpos_line(void)
{
 if((displaymode&DISP_FULLSCREEN) && (desktopmode&DTM_SONGPOS)){
  unsigned int i;
  pds_textdisplay_charxybk(CL_SONGPOS_ARROWS,CLB_SONGPOS,0,dtp.songposline_y,27);
  for(i=1;i<textscreen_maxx;i++)
   pds_textdisplay_charxybk(CL_SONGPOS_DOTS,CLB_SONGPOS,i,dtp.songposline_y,'.');
  pds_textdisplay_charxybk(CL_SONGPOS_HEAD,CLB_SONGPOS,lastsongpos,dtp.songposline_y,2);
  pds_textdisplay_charxybk(CL_SONGPOS_ARROWS,CLB_SONGPOS,textscreen_maxx-1,dtp.songposline_y,26);
 }
}

static void draw_listpos_line(void)
{
 if((displaymode&DISP_FULLSCREEN) && (desktopmode&DTM_LISTPOS)){
  unsigned int i,x;
  pds_textdisplay_charxybk(CL_LISTPOS_ARROWS,CLB_LISTPOS,0,dtp.listposline_y,27);
  for(i=1;i<textscreen_maxx;i++)
   pds_textdisplay_charxybk(CL_LISTPOS_DOTS,CLB_LISTPOS,i,dtp.listposline_y,250);
  pds_textdisplay_charxybk(CL_LISTPOS_ARROWS,CLB_LISTPOS,textscreen_maxx-1,dtp.listposline_y,26);
  if(allsongnum){
   x=(((textscreen_maxx-2)*(currsongnum-1))/allsongnum)+1;
   if(x>(textscreen_maxx-2))
    x=textscreen_maxx-2;
   if(x<1)
    x=1;
   pds_textdisplay_charxybk(CL_LISTPOS_HEAD,CLB_LISTPOS,x,dtp.listposline_y,2);
  }
 }
}

//*************************************************************************
// desktop (display) routines (browser,buttons)
//*************************************************************************

static void refresh_display(struct mainvars *mvp)
{
 unsigned int analpos=(analtabnum+1)%analtabdelay;

 if(playcontrol&PLAYC_RUNNING){
  vol_save[0]=volnum[analpos][0];
  vol_save[1]=volnum[analpos][1];
 }else{
  clear_volnum();
  vol_save[0]=vol_save[1]=0;
 }

 if(!(intsoundcontrol&INTSOUND_DOSSHELL)){
  display_timepos(mvp);
  if(displaymode&DISP_FULLSCREEN){
   mvp->vds->analbands=&analtab[analpos][0];
   draw_spectrum_analiser(mvp->vds->analbands);
   draw_songpos_head(mvp);
   recolor_lastbuttonbox_key(NULL,0);
   setmousecursorpalette();
   //if(!mouse_on)
   // draw_clock();
  }
 }
 analtabnum=analpos;
 LCD_refresh_timer(mvp);
}

static void draw_songpos_head(struct mainvars *mvp)
{
 int x;

 if(desktopmode&DTM_SONGPOS){
  struct frame *frp=mvp->frp0;
  unsigned long index_pos=(frp->frameNum>frp->index_start)? (frp->frameNum-frp->index_start):0;
  x=(((textscreen_maxx-1)*index_pos)/frp->index_len)+1;
  if(x>(textscreen_maxx-2))
   x=textscreen_maxx-2;
  if(x<1)
   x=1;
  if(x!=lastsongpos){
   pds_textdisplay_charxybk(CL_SONGPOS_DOTS,CLB_SONGPOS,lastsongpos,dtp.songposline_y,'.');
   pds_textdisplay_charxybk(CL_SONGPOS_HEAD,CLB_SONGPOS,x,dtp.songposline_y,2);
   lastsongpos=x;
  }
 }
}

//************************************************************************
// time,options,fileinfos display routines
//************************************************************************

static void display_allframes(struct mainvars *mvp)
{
 if(displaymode&DISP_FRAMES){
  pds_textdisplay_spacecxyn(CLB_BASE,0,dtp.timepos_fe,textscreen_maxx);
  sprintf(souttext,"{      /%lu}",mvp->frp0->index_len);
  pds_textdisplay_textxybk(CL_BASE,CLB_BASE,0,dtp.timepos_fe,souttext);
 }
}

static void display_timepos(struct mainvars *mvp)
{
 static unsigned int cpushowdelay,cpupercent;
 struct frame *frp=mvp->frp0;
 long cframe,index_pos=frp->frameNum-frp->index_start;
 unsigned long x,ctime;
 char sout[100];

 if(displaymode&DISP_TIMEPOS){
  if(!cpushowdelay--){
   cpupercent=((100*allcpuusage)/allcputime);
   cpupercent&=511;
   allcpuusage=0;
   allcputime=1;
   cpushowdelay=9;
  }
  ctime=0;
  switch(timemode){
   case 0:cframe=index_pos;break;
   case 1:cframe=frp->index_len-index_pos;break;
   case 2:cframe=index_pos;ctime=playlist_fulltime_getelapsed(mvp,0);break;
   case 3:cframe=frp->index_len-index_pos;
                 if(mvp->psip->fulltimesec)
                  ctime=mvp->psip->fulltimesec-playlist_fulltime_getelapsed(mvp,0)-frp->timesec;
                 break;
   default:cframe=index_pos;
  }
  if(cframe<0)
   cframe=0;

  ctime*=10;
  ctime+=(long)(10.0*(float)cframe*(float)frp->timesec/(float)frp->index_len); // float needed to avoid integer overflow at huge files

  if(displaymode&DISP_NOFULLEDIT){
   struct buttons *dp=&dk[DK_TIMEPOS];
   if(allsongnum<=9999){
    sprintf(sout,"%4d",(timemode==3)? (allsongnum-currsongnum+1):currsongnum);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,sout);
    pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+4,dp->ypos,'/');
    dp++;
    sprintf(sout,"%-4d",allsongnum);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,sout);
    dp++;
   }else{
    sprintf(sout,"%9d",((timemode==3)? (allsongnum-currsongnum+1):currsongnum));
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,sout);
    dp+=2;
   }
   if(timemode<2){
    if((cframe<=999999) && (frp->index_len<=999999)){
     sprintf(sout,"%6d",cframe);
     pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,sout);
     pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+6,dp->ypos,'/');
     sprintf(sout,"%-6d",frp->index_len);
     pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos+7,dp->ypos,sout);
    }else{
     sprintf(sout,"%13d",cframe);
     pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,sout);
    }
    dp++;
    x=dp->xpos;
    if((frp->timesec<6000) && (ctime<60000)){    // < 100 minutes (3 digits)
     sprintf(sout,"%2d",ctime/600);          // mm:ss.t
     pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
     x+=2;
     pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,x,dp->ypos,':');
     x++;
     sprintf(sout,"%2.2d",(ctime/10)%60);
     pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
     x+=2;
     pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,x,dp->ypos,'.');
     x++;
     sprintf(sout,"%1.1d",ctime%10);
     pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
     x++;
     pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,x,dp->ypos,'/');  // /
     x++;
     sprintf(sout,"%2d",frp->timesec/60);      // mm:ss
     pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
     x+=2;
    }else{
     sprintf(sout,"%3d",ctime/600);           // mmm:ss
     pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
     x+=3;
     pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,x,dp->ypos,':');
     x++;
     sprintf(sout,"%2.2d",(ctime/10)%60);
     pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
     x+=2;
     pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,x,dp->ypos,'/');  // /
     x++;
     sprintf(sout,"%3d",frp->timesec/60);      // mmm:ss
     pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
     x+=3;
    }
    pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,x,dp->ypos,':');
    x++;
    sprintf(sout,"%2.2d",frp->timesec%60);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,x,dp->ypos,sout);
   }else{
    sprintf(sout,"%5d",ctime/36000);
    pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,sout);
    pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+5,dp->ypos,':');
    sprintf(sout,"%2.2d",(ctime/600)%60);
    pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,dp->xpos+6,dp->ypos,sout);
    pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+8,dp->ypos,':');
    sprintf(sout,"%2.2d",(ctime/10)%60);
    pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,dp->xpos+9,dp->ypos,sout);
    pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+11,dp->ypos,'.');
    sprintf(sout,"%1.1d",ctime%10);
    pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,dp->xpos+12,dp->ypos,sout);
    dp++;
    sprintf(sout,"%7d",mvp->psip->fulltimesec/3600);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,sout);
    pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+7,dp->ypos,':');
    sprintf(sout,"%2.2d",(mvp->psip->fulltimesec/60)%60);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos+8,dp->ypos,sout);
    pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+10,dp->ypos,':');
    sprintf(sout,"%2.2d",mvp->psip->fulltimesec%60);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos+11,dp->ypos,sout);
   }
   dp++;
   if(!funcbit_test(mvp->frp0->buffertype,PREBUFTYPE_BACK)){
    sprintf(sout,"%3d%%",cpupercent);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos+4,dp->ypos,sout);
   }
   draw_volume_nofe();
  }else{
   sprintf(sout,"{%d/%d}{%5lu/%lu}{",((timemode==3)? (allsongnum-currsongnum+1):currsongnum),allsongnum,cframe,frp->index_len);
   x=pds_textdisplay_textxybk(CL_BASE,CLB_BASE,0,dtp.timepos_fe,sout);
   if(timemode<2){
    sprintf(sout,"%2lu:%2.2lu.%1.1d",ctime/600,(ctime/10)%60,ctime%10);
    x+=pds_textdisplay_textxybk(CLH_BASE,CLB_BASE,x,dtp.timepos_fe,sout);
    sprintf(sout,"/%d:%2.2d}{CPU:%2lu%%}",frp->timesec/60,frp->timesec%60,cpupercent);
   }else{
    sprintf(sout,"%lu:%2.2lu:%2.2lu.%1.1d",ctime/36000,(ctime/600)%60,(ctime/10)%60,ctime%10);
    x+=pds_textdisplay_textxybk(CLH_BASE,CLB_BASE,x,dtp.timepos_fe,sout);
    sprintf(sout,"/%d:%2.2d:%2.2d}",mvp->psip->fulltimesec/3600,(mvp->psip->fulltimesec/60)%60,mvp->psip->fulltimesec%60);
   }
   x+=pds_textdisplay_textxybk(CL_BASE,CLB_BASE,x,dtp.timepos_fe,sout);
   draw_volume_fe(x);
  }
 }else{
  if(displaymode&DISP_FRAMES){
   pds_ltoa(index_pos,sout);
   pds_textdisplay_textxybk(CL_BASE,CLB_BASE,1,dtp.timepos_fe,sout);
  }
 }
}

static void display_option_vol(struct mainvars *mvp)
{
 if(displaymode&DISP_NOFULLEDIT){
  struct buttons *dp=&dk[DK_OPTIONS];
#ifdef USE_LSA_SCV
  sprintf(souttext,"%3d",mvp->aui->card_mixer_values[AU_MIXCHAN_MASTER]);
#else
  sprintf(souttext,"%3d",MIXER_var_volume);
#endif
  pds_textdisplay_textxybk(CL_BTNTEXTMIXER,CLB_BTNTEXT,dp->xpos+1,dp->ypos+1,souttext);
 }
}

char *get_playstatus_string(unsigned int *color)
{
 static struct{
  char *text;
  unsigned int color;
 }statustexts[7]={
   {" PAUSED ",CL_FUNCTION|CL_BLINK|(CLB_FUNCTION<<4)},
   {"Fade-Out",CL_FUNCTION|CL_BLINK|(CLB_FUNCTION<<4)},
   {"Fade--In",CL_FUNCTION|CL_BLINK|(CLB_FUNCTION<<4)},
   {"CrosFade",CL_FUNCTION|CL_BLINK|(CLB_FUNCTION<<4)},
   {"        ",CL_FUNCTION|         (CLB_FUNCTION<<4)},
   {"CrosLoad",CL_FUNCTION|CL_BLINK|(CLB_FUNCTION<<4)},
   {"-=SCAN=-",CL_FUNCTION|CL_BLINK|(CLB_FUNCTION<<4)},
  };
 unsigned int status;

 if(!(playcontrol&PLAYC_RUNNING))
  status=0;
 else{
  if(crossfadepart)
   status=crossfadepart;
  else
   if(playcontrol&PLAYC_HIGHSCAN)
    status=6;
   else
    status=4;
 }
 *color=statustexts[status].color;
 return (statustexts[status].text);
}

static void display_options(struct mainvars *mvp) // program control infos
{
 struct buttons *dp;
 unsigned int color;
 char *functext;

 if(displaymode&DISP_NOFULLEDIT){
  display_option_vol(mvp);
  dp=&dk[DK_OPTIONS+3];

  functext=get_playstatus_string(&color);
  pds_textdisplay_textxy(color,dk[DK_FUNCTION].xpos,dk[DK_FUNCTION].ypos,functext);

  if(dp->ypos<dtp.endofbuttonsy){
   color=(playreplay)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
   if(playreplay&1)
    pds_textdisplay_textxy(color,dp->xpos+1,dp->ypos+1,"R1");
   else
    pds_textdisplay_textxy(color,dp->xpos+1,dp->ypos+1,dp->intext);
   dp++;
   color=(playrand)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
   pds_textdisplay_textxy(color,dp->xpos+1,dp->ypos+1,dp->intext);
   dp++;
   color=(MIXER_var_autovolume)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
   pds_textdisplay_textxy(color,dp->xpos+1,dp->ypos+1,dp->intext);
   dp++;
   color=(mvp->cfi->usecrossfade)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
   pds_textdisplay_textxy(color,dp->xpos+1,dp->ypos+1,dp->intext);
   dp++;
   color=(mvp->cfi->crossfadetype&CFT_FADEOUT)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
   pds_textdisplay_textxy(color,dp->xpos+1,dp->ypos+1,"Fo");
   color=(mvp->cfi->crossfadetype&CFT_FADEIN)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
   pds_textdisplay_textxy(color,dp->xpos+1+2,dp->ypos+1,"Fi");
   dp++;
   sprintf(souttext,"%3d",MIXER_getvalue("MIX_SURROUND"));
   pds_textdisplay_textxybk(CL_BTNTEXTMIXER,CLB_BTNTEXT,dp->xpos+1,dp->ypos+1,souttext);
   dp+=3;
   if(mvp->aui->mixer_infobits&AUINFOS_MIXERINFOBIT_SPEED1000)
    sprintf(souttext,"%4d",MIXER_getvalue("MIX_SPEED"));
   else
    sprintf(souttext,"%3d",MIXER_getvalue("MIX_SPEED"));
   pds_textdisplay_textxybk(CL_BTNTEXTMIXER,CLB_BTNTEXT,dp->xpos+1,dp->ypos+1,souttext);
   dp+=3;
   if(MIXER_var_balance!=0){
    sprintf(souttext,"%+2d ",MIXER_var_balance);
    souttext[3]=0;
    //sprintf(souttext,"%c%2d",(MIXER_var_balance<0)? 'L':'R',abs(MIXER_var_balance));
   }else
    pds_strcpy(souttext," 0 ");
   pds_textdisplay_textxybk(CL_BTNTEXTMIXER,CLB_BTNTEXT,dp->xpos+1,dp->ypos+1,souttext);
   dp+=3;
   sprintf(souttext,"%3d",mvp->aui->card_mixer_values[AU_MIXCHAN_BASS]);
   pds_textdisplay_textxybk(CL_BTNTEXTMIXER,CLB_BTNTEXT,dp->xpos+1,dp->ypos+1,souttext);
   dp+=3;
   sprintf(souttext,"%3d",mvp->aui->card_mixer_values[AU_MIXCHAN_TREBLE]);
   pds_textdisplay_textxybk(CL_BTNTEXTMIXER,CLB_BTNTEXT,dp->xpos+1,dp->ypos+1,souttext);
  }
  dp=&dk[DK_BUFCPU];
  if(funcbit_test(intsoundcontrol,INTSOUND_DECODER))
   pds_textdisplay_charxybk(CL_INFOBORDER,CLB_INFOBORDER,dp->xpos+3,dp->ypos,'/');
  else
   pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,"CPU:");
  dp=&dk[DK_SMALLBTNS];
  color=(MIXER_var_swapchan)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
  pds_textdisplay_textxy(color,dp->xpos,dp->ypos,dp->intext);
  dp++;
  color=(MIXER_getstatus("MIX_TONE_LOUDNESS"))? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
  pds_textdisplay_textxy(color,dp->xpos,dp->ypos,dp->intext);
  dp++;
  color=(playcontrol&PLAYC_PAUSEALL)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
  pds_textdisplay_textxy(color,dp->xpos,dp->ypos,dp->intext);
  dp++;
  color=(playcontrol&PLAYC_PAUSENEXT)? (CLA_BTNTEXTOPTION|(CLB_BTNTEXT<<4)):(CL_BTNTEXTOPTION|(CLB_BTNTEXT<<4));
  pds_textdisplay_textxy(color,dp->xpos,dp->ypos,dp->intext);
 }
}

static void display_fileinfos(struct mainvars *mvp)
{
 struct buttons *dp;
 struct frame *frp;
 struct mpxplay_infile_info_s *miis;
 struct mpxplay_audio_decoder_info_s *adi;
 struct playlist_entry_info *pei;
 unsigned int col,basex;
 char *shortfname,**id3ip,*channeltext,channumstr[MPXPLAY_ADITEXTSIZE_CHANNEL+2],filetypestr[MPXPLAY_ADITEXTSIZE_LONGNAME+2];

 if(displaymode&DISP_VERBOSE){
  pei=mvp->pei0;
  shortfname=pds_getfilename_from_fullname(pei->filename);
  if(shortfname==NULL)
   shortfname=pei->filename;

  id3ip=&pei->id3info[0];

  if(id3ip[I3I_ARTIST] || id3ip[I3I_TITLE]){
   snprintf(souttext,sizeof(souttext),"Mpxplay v1.57 - %s : %s ",
    (id3ip[I3I_ARTIST])? id3ip[I3I_ARTIST]:"",(id3ip[I3I_TITLE])? id3ip[I3I_TITLE]:"");
  }else
   snprintf(souttext,sizeof(souttext),"Mpxplay v1.57 - %-40.40s ",shortfname);
  pds_mswin_setapplicationtitle(souttext);

  frp=mvp->frp0;
  miis=frp->infile_infos;
  adi=miis->audio_decoder_infos;

  if(adi->channeltext)
   channeltext=adi->channeltext;
  else{
   switch(adi->filechannels){
    case 1:channeltext="  Mono  ";break;
    case 2:channeltext=" Stereo ";break;
    default:snprintf(channumstr,sizeof(channumstr),"%2d-chan ",adi->filechannels);channeltext=&channumstr[0];break;
   }
  }
  if(displaymode&DISP_NOFULLEDIT){
   if(miis->longname){
    pds_strcpy(filetypestr,miis->longname);
    filetypestr[MPXPLAY_ADITEXTSIZE_LONGNAME]=0;
   }else{
    char *e;
    if(frp->infile_funcs && frp->infile_funcs->file_extensions[0])
     e=frp->infile_funcs->file_extensions[0];
    else{
     e=pds_strrchr(shortfname,'.');
     if(e)
      e++;
    }
    if(e || adi->shortname)
     sprintf(filetypestr,"%3.3s->%3.3s",((e)? e:"???"),(adi->shortname? adi->shortname:"???"));
    else
     pds_strcpy(filetypestr,"  ----  ");
   }
   dp=&dk[DK_FILEINFO+1];
   basex=dk[0].xpos+1;
   pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,filetypestr);
   dp++;
   if(adi->bitratetext)
    pds_textdisplay_textxynbk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,MPXPLAY_ADITEXTSIZE_BITRATE,adi->bitratetext);
   else{
    if(adi->bitrate){
     if(adi->bitrate<100)
      sprintf(souttext," %2d kbit",adi->bitrate);
     else
      sprintf(souttext,"%-4dkbit",adi->bitrate);
    }else
     sprintf(souttext," %2d bit ",adi->bits);
    pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,souttext);
   }
   dp++;
   if(adi->freq<100000)
    sprintf(souttext,"%5dHz",adi->freq);
   else
    if(adi->freq<10000000)
     sprintf(souttext,"%4dkHz",adi->freq/1000);
    else
     sprintf(souttext,"%2.1fMHz",(float)adi->freq/1000000);
   pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,souttext);
   dp++;
   pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,channeltext);
   dp++;
   pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dp->xpos,dp->ypos,mvp->aui->card_handler->shortname);

   draw_listpos_line();

  }else{
   if(miis->longname)
    pds_strcpy(filetypestr,miis->longname);
   else{
    char *e=pds_strrchr(shortfname,'.');
    if(e || adi->shortname)
     sprintf(filetypestr,"%3.3s->%3.3s",((e)? (e+1):"???"),(adi->shortname? adi->shortname:"???"));
    else
     filetypestr[0]=0;
   }
   if(adi->bitratetext){
    snprintf(souttext,sizeof(souttext),"%s, %s, %ld Hz, %s            ",
		      filetypestr,adi->bitratetext,adi->freq,channeltext);
   }else{
    if(adi->bitrate){
     snprintf(souttext,sizeof(souttext),"%s, %d kbit/s, %ld Hz, %s            ",
		      filetypestr,adi->bitrate,adi->freq,channeltext);
    }else{
     snprintf(souttext,sizeof(souttext),"%s, %d bit, %ld Hz, %s            ",
		      filetypestr,adi->bits,adi->freq,channeltext);
    }
   }
   col=pds_textdisplay_textxybk(CL_BASE,CLB_BASE,0,POS_FILEINFO_FE,souttext);
   if(col<textscreen_maxx)
    pds_textdisplay_spacecxyn(CLB_BASE,col,POS_FILEINFO_FE,textscreen_maxx-col);
  }
 }
}

//#define SHOW_TRACKNUM 1
#define LINEBUFFER_LEN 192

static int disp_id3info_shift[2],disp_id3info_linelens[2];

static void display_id3infos(struct mainvars *mvp)
{
#ifdef SHOW_TRACKNUM
 static char *id3mask[7]={"%s. ","%.50s","%s","%.30s ","(%4s) ","[%s] ","%.30s"};
 static unsigned int id3order_nofe[7]={I3I_TRACKNUM,I3I_ARTIST,I3I_TITLE,I3I_ALBUM,I3I_YEAR,I3I_GENRE,I3I_COMMENT};
#else
 static char *id3mask[6]={"%.50s","%s","%.30s ","(%4s) ","[%s] ","%.30s"};
 static unsigned int id3order_nofe[6]={I3I_ARTIST,I3I_TITLE,I3I_ALBUM,I3I_YEAR,I3I_GENRE,I3I_COMMENT};
#endif
 struct buttons *dp;
 struct playlist_entry_info *pei=mvp->pei0;
 int len;
 unsigned int i,col,basex,basey,color,shift;
 char *shortfname,**id3ip,linetmp[LINEBUFFER_LEN+1];
 unsigned short *linep,linebuffer[2][LINEBUFFER_LEN+1];

 if(lockid3window)
  return;

 if(!(displaymode&DISP_NOFULLEDIT)){
  if(displaymode&DISP_VERBOSE)
   draw_one_id3info_line(pei,6,0,textscreen_maxx,CLH_BASE|(CLB_BASE<<4),POS_ID3INFO_FE,0);
  return;
 }

 basex=dk[0].xpos+1;
 shift=disp_id3info_shift[0];

 shortfname=pds_getfilename_from_fullname(pei->filename);
 if(shortfname==NULL)
  shortfname=pei->filename;

 id3ip=&pei->id3info[0];
 linep=&linebuffer[0][0];
 pds_memset(linep,0,sizeof(linebuffer));

#ifdef SHOW_TRACKNUM
 dp=&dk[DK_ID3INFO];
#else
 dp=&dk[DK_ID3INFO+1];
#endif
 col=0;
 color=CLH_ID3TEXT|(CLB_ID3TEXT<<4);
#ifndef MPX_DEBUG_SERIAL
 basey=dp->ypos;
 pds_textdisplay_spacecxyn(CLB_ID3TEXT,basex,dp->ypos,ID3WINDOW_SIZE_NOFE);
#ifdef SHOW_TRACKNUM
 for(i=0;i<7;i++){
#else
 for(i=0;i<6;i++){
#endif
  unsigned int index=id3order_nofe[i];
  if(index==I3I_ALBUM){ // then new line
   disp_id3info_linelens[0]=col;
   if(shift && (col<=ID3WINDOW_SIZE_NOFE))
    disp_id3info_shift[0]=0;
   col=0;
   color=CL_ID3TEXT|(CLB_ID3TEXT<<4);
   linep=&linebuffer[1][0];
   shift=disp_id3info_shift[1];
   pds_textdisplay_spacecxyn(CLB_ID3TEXT,basex,dp->ypos,ID3WINDOW_SIZE_NOFE);
  }
  if(id3ip[index] && (col<=(LINEBUFFER_LEN-3))){
   if((index==I3I_TITLE) && id3ip[I3I_ARTIST])
    col+=pds_textdisplay_text2bufbk(CL_ID3TEXT,CLB_ID3TEXT,&linep[col],LINEBUFFER_LEN-col," - ");
   pds_strncpy(linetmp,id3ip[index],LINEBUFFER_LEN-col);
   linetmp[LINEBUFFER_LEN-col]=0;
   pds_strcutspc(linetmp);
   len=snprintf(souttext,sizeof(souttext),id3mask[i],linetmp);
   if((len>0) && (col<LINEBUFFER_LEN)){
    if(((col<shift) || ((col-shift)<ID3WINDOW_SIZE_NOFE)) && ((col+len)>shift)){
     int xpos=(int)col-(int)shift;
     dp->xsize=len-1;
     if(xpos<0){
      dp->xsize+=xpos;
      xpos=0;
     }
     if((xpos+dp->xsize)>ID3WINDOW_SIZE_NOFE)
      dp->xsize=ID3WINDOW_SIZE_NOFE-xpos-1;
     dp->xpos=basex+xpos;
    }else{
     dp->xsize=0;
     dp->xpos=POS_INVALID;
    }
    col+=pds_textdisplay_text2buf(color,&linep[col],LINEBUFFER_LEN-col,souttext);
   }
  }else{
   if((index==I3I_TITLE) && !id3ip[I3I_ARTIST]){
    len=pds_textdisplay_text2buf(color,&linep[col],LINEBUFFER_LEN,shortfname);
    dp->xsize=len;
    dp->xpos=basex;
    col+=len;
   }else{
    dp->xsize=0;
    dp->xpos=POS_INVALID;
   }
  }
  dp++;
 }
 disp_id3info_linelens[1]=col;
 if(shift && (col<=ID3WINDOW_SIZE_NOFE))
  disp_id3info_shift[1]=0;
 pds_textdisplay_textbufxyn(basex,basey,&linebuffer[0][disp_id3info_shift[0]],ID3WINDOW_SIZE_NOFE);
 pds_textdisplay_textbufxyn(basex,basey+1,&linebuffer[1][disp_id3info_shift[1]],ID3WINDOW_SIZE_NOFE);
#endif
 lastmousebox=LASTMOUSEBOX_INVALID;
}

static unsigned int disp_id3info_scroll_waitcount;
static int disp_id3info_scroll_direction;

static void display_id3info_timer_scroll(void)
{
 if((desktopmode&DTM_ID3WINSCROLL_DISABLE) || !(displaymode&DISP_NOFULLEDIT) || (ID3WINDOW_SIZE_NOFE>=disp_id3info_linelens[0]))
  return;

 if(disp_id3info_scroll_waitcount)
  disp_id3info_scroll_waitcount--;
 else{
  disp_id3info_shift[0]+=disp_id3info_scroll_direction;
  if((disp_id3info_shift[0]<0) || (disp_id3info_shift[0]>(disp_id3info_linelens[0]-ID3WINDOW_SIZE_NOFE))){
   disp_id3info_scroll_direction=-disp_id3info_scroll_direction;
   disp_id3info_scroll_waitcount=7;
   disp_id3info_shift[0]+=disp_id3info_scroll_direction;
  }
  refdisp|=RDT_ID3INFO;
 }
}

// at new file
static void display_id3info_timer_reset(void)
{
 disp_id3info_shift[0]=disp_id3info_shift[1]=0;
 disp_id3info_scroll_waitcount=10;
 disp_id3info_scroll_direction=1;
 refdisp|=RDT_ID3INFO;
}

static void display_id3info_timer_resume(void)
{
 disp_id3info_scroll_waitcount=3;
}

static void display_id3info_timer_suspend(void)
{
 disp_id3info_scroll_waitcount=999999999;
 mpxplay_timer_addfunc(&display_id3info_timer_resume,NULL,MPXPLAY_TIMERTYPE_SIGNAL,MPXPLAY_SIGNALTYPE_CLEARMESSAGE);
}

// at start of program
static void display_id3info_timer_init(void)
{
 if(displaymode&DISP_FULLSCREEN){
  mpxplay_timer_addfunc(&display_id3info_timer_scroll,NULL,MPXPLAY_TIMERTYPE_REPEAT,mpxplay_timer_secs_to_counternum(1)/5);
  mpxplay_timer_addfunc(&display_id3info_timer_reset, NULL,MPXPLAY_TIMERTYPE_SIGNAL|MPXPLAY_TIMERTYPE_REPEAT,MPXPLAY_SIGNALTYPE_NEWFILE);
  mpxplay_timer_addfunc(&display_id3info_timer_suspend,NULL,MPXPLAY_TIMERTYPE_SIGNAL|MPXPLAY_TIMERTYPE_REPEAT,MPXPLAY_SIGNALTYPE_DISPMESSAGE);
 }
}

static void draw_one_id3info_line(struct playlist_entry_info *pei,unsigned int lastid,
                                  unsigned int col,unsigned int endcol,
	                          unsigned int color,unsigned int y,unsigned int showfilenameonly)
{
 static unsigned int id3order_fe[6]={I3I_ARTIST,I3I_TITLE,I3I_GENRE,I3I_ALBUM,I3I_YEAR,I3I_COMMENT};
 static unsigned int id3textlen[6]={1,0,7,6,9,7};
 static char *id3text[6]={":",""," Album:"," Year:"," Comment:"," Genre:"};
 char *shortfname,**id3ip=&pei->id3info[0];
 unsigned int i,k,len,showseparator=0,lengthcol,bkcolor;

 if(col>=endcol)
  return;

 lengthcol=endcol-col;
 bkcolor=color>>4;
 if(showfilenameonly || (GET_HFT(pei->entrytype)==HFT_DFT) || (!id3ip[I3I_ARTIST] && !id3ip[I3I_TITLE])){
  len=pds_strcpy(souttext,pei->filename);
  if((pei->entrytype&(DFTM_DFT|DFTM_DRIVE))==(DFTM_DFT|DFTM_DRIVE))
   shortfname=&souttext[0];
  else{
   if(!(id3textconv&ID3TEXTCONV_FILENAME) && (id3textconv&(ID3TEXTCONV_CODEPAGE|ID3TEXTCONV_UTF_AUTO)))
    mpxplay_playlist_textconv_selected_do(souttext,len,(id3textconv|ID3TEXTCONV_UTF8),0); // note: UTF8 is required for FTP filenames
   shortfname=pds_getfilename_from_fullname(souttext);
   len=pds_strlen(shortfname);
  }
  pds_strcat(shortfname,"             ");
  shortfname[lengthcol]=0;
  pds_textdisplay_textxy(color,col,y,shortfname);
  if(len<13)
   col+=13;
  else
   col+=len+2;
 }
 if(!showfilenameonly || (GET_HFT(pei->entrytype)==HFT_DFT)){
  for(k=0;k<lastid;k++){
   i=id3order_fe[k];
   if(id3ip[i]!=NULL){
    len=pds_strlen(id3ip[i]);
    if((col+id3textlen[i])>endcol)
     break;
    if(showseparator && id3textlen[i]){
     pds_textdisplay_textxybk(CL_EDITOR_AT_SEPARATOR,bkcolor,col,y,(char *)id3text[i]);
     col+=id3textlen[i];
    }
    if((col+len)>endcol){
     pds_strcpy(souttext,id3ip[i]);
     souttext[endcol-col]=0;
     pds_textdisplay_textxy(color,col,y,souttext);
     col=endcol;
     break;
    }else{
     pds_textdisplay_textxy(color,col,y,id3ip[i]);
     col+=len;
    }
    showseparator=1;
   }
   if(k==1)
    showseparator=1;
  }
 }
 if(col<endcol)
  pds_textdisplay_spacecxyn(bkcolor,col,y,endcol-col);
}

/*static void draw_clock(void)
{
 unsigned int hextime=pds_gettime();
 sprintf(souttext,"%2d:%2.2d:%2.2d",(hextime>>16),(hextime>>8)&0xff,hextime&0xff);
 pds_textdisplay_textxybk(CLH_INFOTEXT,CLB_INFOTEXT,dk[DK_MOUSEPOS].xpos,dk[DK_MOUSEPOS].ypos,souttext);
}*/

//**************************************************************************
// spectrum analiser & volume meter
//**************************************************************************

static unsigned int log1[32]={100,200,400,
			700,980,1221,1520,
			1893,2358,2936,3656,4552,5669,7059,8790,10945,13629,16971,
			21132,26314,32767,36000,40000,45000,50000,55000,
			60000,65000,70000,75000,80000,85000};
static unsigned int peekvol[2],peeknum[2],peekhold[2];

#ifdef MPXPLAY_WIN32

static void draw_volume_nofe(void)
{
 static struct vol_nofe{
  unsigned int voldiv;
  unsigned int lines;
  unsigned int linelen;
  unsigned int xpos_base;
  unsigned int ypos_base;
  unsigned int xpos_rel[2];
  unsigned int ypos_rel[2];
  unsigned int istep[2];
  unsigned int xlen;
  unsigned int part[2];
  char drawchars[2];
 }sv[2]={ {  16,1,48,32,0,{23-1,23+2},{0,0},{-1,1},23,{10,19},{'º',250} },
	  {  7 ,2,32,0, 0,{   0,   0},{0,1},{ 1,1},32,{12,26},{'#',250} }};

 unsigned char  line_text[2][23*2+2];
 unsigned short line_attribs[2][23*2+2];

 unsigned int i,ch,cvol[2];
 struct vol_nofe *svp;

 if(!dtp.volline)
  return;
 if(dtp.volline<dtp.endofbuttonsy){
  if(displaymode&DISP_ANALISER)
   return;
  svp=(struct vol_nofe *)&sv[1];
  svp->ypos_base=dtp.volline;
 }else{
  svp=(struct vol_nofe *)&sv[0];
  svp->ypos_base=dtp.volline;
  line_text[0][svp->xpos_rel[0]+1]=line_text[0][svp->xpos_rel[0]+2]='-';
  line_attribs[0][svp->xpos_rel[0]+1]=line_attribs[0][svp->xpos_rel[0]+2]=CL_VOLUME_SEPARATOR|(CLB_VOLUME<<4);
 }
 cvol[0]=vol_save[0]/svp->voldiv;
 cvol[1]=vol_save[1]/svp->voldiv;
 for(ch=0;ch<2;ch++){
  if(peekvol[ch]<cvol[ch]){
   peekvol[ch]=cvol[ch];
   peeknum[ch]=32;
   peekhold[ch]=19;
  }else
   if(peekhold[ch]--==0){
    peekvol[ch]=0;
    peeknum[ch]=32;
   }
  for(i=0;i<svp->xlen;i++){
   if(peekvol[ch]<log1[i] && peeknum[ch]==32)
    peeknum[ch]=i-1;
   if(cvol[ch]>log1[i] || (i==peeknum[ch] && peeknum[ch]!=0)){
    unsigned int color=(i<svp->part[0])? (CL_VOLUME_LEVEL0|(CLB_VOLUME<<4))
		      :(i<svp->part[1])? (CL_VOLUME_LEVEL1|(CLB_VOLUME<<4))
		      :                  (CL_VOLUME_LEVEL2|(CLB_VOLUME<<4));
    line_text[svp->ypos_rel[ch]][svp->xpos_rel[ch]+(i*svp->istep[ch])]=svp->drawchars[0];
    line_attribs[svp->ypos_rel[ch]][svp->xpos_rel[ch]+(i*svp->istep[ch])]=color;
   }else{
    line_text[svp->ypos_rel[ch]][svp->xpos_rel[ch]+(i*svp->istep[ch])]=svp->drawchars[1];
    line_attribs[svp->ypos_rel[ch]][svp->xpos_rel[ch]+(i*svp->istep[ch])]=CL_VOLUME_DOTS|(CLB_VOLUME<<4);
   }
  }
 }

 for(i=0;i<svp->lines;i++)
  pds_textdisplay_textxyan(svp->xpos_base,svp->ypos_base+svp->ypos_rel[i],&line_text[i][0],&line_attribs[i][0],svp->linelen);
}

#else

static void draw_volume_nofe(void)
{
 static struct vol_nofe{
  unsigned int voldiv;
  unsigned int xpos[2];
  unsigned int istep[2];
  unsigned int xlen;
  unsigned int part[2];
  char drawchars[2];
 }sv[2]={ {  16,{32+23-1,32+23+2},{-1,1},23,{10,19},{'º',250} },
	  {  7 ,{      0,      0},{ 1,1},32,{12,26},{'#',250} }};

 unsigned int i,ch,cvol[2],y[2];
 struct vol_nofe *svp;

 if(!dtp.volline)
  return;
 if(dtp.volline<dtp.endofbuttonsy){
  if(displaymode&DISP_ANALISER)
   return;
  svp=(struct vol_nofe *)&sv[1];
  y[0]=dtp.volline;
  y[1]=y[0]+1;
 }else{
  svp=(struct vol_nofe *)&sv[0];
  y[0]=y[1]=dtp.volline;
  pds_textdisplay_textxybk(CL_VOLUME_SEPARATOR,CLB_VOLUME,svp->xpos[0]+1,y[0],"--");
 }
 cvol[0]=vol_save[0]/svp->voldiv;
 cvol[1]=vol_save[1]/svp->voldiv;
 for(ch=0;ch<2;ch++){
  if(peekvol[ch]<cvol[ch]){
   peekvol[ch]=cvol[ch];
   peeknum[ch]=32;
   peekhold[ch]=19;
  }else
   if(peekhold[ch]--==0){
    peekvol[ch]=0;
    peeknum[ch]=32;
   }
  for(i=0;i<svp->xlen;i++){
   if(peekvol[ch]<log1[i] && peeknum[ch]==32)
    peeknum[ch]=i-1;
   if(cvol[ch]>log1[i] || (i==peeknum[ch] && peeknum[ch]!=0)){
    unsigned int color=(i<svp->part[0])? (CL_VOLUME_LEVEL0|(CLB_VOLUME<<4))
		      :(i<svp->part[1])? (CL_VOLUME_LEVEL1|(CLB_VOLUME<<4))
		      :                  (CL_VOLUME_LEVEL2|(CLB_VOLUME<<4));
    pds_textdisplay_charxy(color,svp->xpos[ch]+(i*svp->istep[ch]),y[ch],svp->drawchars[0]);
   }else
    pds_textdisplay_charxybk(CL_VOLUME_DOTS,CLB_VOLUME,svp->xpos[ch]+(i*svp->istep[ch]),y[ch],svp->drawchars[1]);
  }
 }
}
#endif

static void draw_volume_fe(unsigned int x) // -fe -fl
{
 unsigned int i,cvol,color;

 cvol=(vol_save[0]+vol_save[1])/52;
 if(peekvol[0]<cvol){
  peekvol[0]=cvol;
  peeknum[0]=32;
  peekhold[0]=19;
 }else
  if(peekhold[0]--==0){
   peekvol[0]=0;
   peeknum[0]=32;
  }
 for(i=0;i<20;i++){
  if(peekvol[0]<log1[i] && peeknum[0]==32)
   peeknum[0]=i-1;
  color=(i< 7)? (CL_VOLUME_LEVEL0|(CLB_VOLUME<<4))
       :(i<17)? (CL_VOLUME_LEVEL1|(CLB_VOLUME<<4))
       :        (CL_VOLUME_LEVEL2|(CLB_VOLUME<<4));
  if(cvol>log1[i] || (i==peeknum[0] && peeknum[0]!=0))
   pds_textdisplay_charxy(color,x,dtp.timepos_fe,'#');
  else
   pds_textdisplay_charxy(color,x,dtp.timepos_fe,'.');
  x++;
 }
 if(x<textscreen_maxx)
  pds_textdisplay_spacecxyn(CLB_BASE,x,dtp.timepos_fe,textscreen_maxx-x);
}

void clear_volnum(void)
{
 pds_memset(&volnum[0][0],0,DISPLAY_ANALISER_MAXDELAY*2*sizeof(unsigned long));
 pds_memset(&analtab[0][0],0,DISPLAY_ANALISER_MAXDELAY*32*sizeof(unsigned long));
}

// table for 25 lines display mode (12 analiser lines)
static unsigned int log25[12]={0,100,400,
			980,1520,
			2358,3656,5669,8790,13629,
			21132,32767};

// table for 50 lines display mode (16 analiser lines)
static unsigned int log50[16]={0,100,200,400,
			       700,1024,
			       1448,2048,2896,4096,5792,8192,11585,16384,
			       23170,32768};
static long analpeekvol[32],analpeeknum[32],analpeekhold[32];
static unsigned int anal_endline,*anal_logp;

#ifdef MPXPLAY_WIN32
static unsigned short sa_attribs[15][32];
static char sa_chars[15][32],sa_prev_chars[15][32];

static void draw_spectrum_analiser(unsigned long *analpt)
{
 unsigned int i,k,y,endline,endcol,cvol,*logp,color;
 long *analpv,*analpn,*analph;

 if(!(displaymode&DISP_ANALISER) || !(displaymode&DISP_NOFULLEDIT))
  return;

 analpv=&analpeekvol[0];
 analpn=&analpeeknum[0];
 analph=&analpeekhold[0];
 endline=anal_endline;
 logp=anal_logp;
 endcol=32;
 analpt[0]>>=3;
 analpt[1]>>=1;
 analpt[2]>>=1;
 for(k=0;k<endcol;k++){
  cvol=analpt[0];
  if(analph[0])
   analph[0]--;
  else{
   if(analpn[0]){
    sa_attribs[endline-analpn[0]][k]=CL_ANALISER_DOTS|(CLB_ANALISER<<4);
    sa_chars[endline-analpn[0]][k]=250;
    analpn[0]--;
    analpv[0]=logp[analpn[0]];
   }
  }
  if(analpv[0]<cvol && cvol>50){
   for(i=0;i<endline;i++){
    sa_attribs[i][k]=CL_ANALISER_DOTS|(CLB_ANALISER<<4);
    sa_chars[i][k]=250;
   }
   analpn[0]=0;
   for(i=1;i<=endline;i++){
    if(cvol<logp[i]){
     analpn[0]=i;
     break;
    }
   }
   analph[0]=20;
   analpv[0]=logp[analpn[0]];
  }
  y=analpn[0];
  if(y){
   color=(y< 5)? (CL_ANALISER_LEVEL0|(CLB_ANALISER<<4))
	:(y<15)? (CL_ANALISER_LEVEL1|(CLB_ANALISER<<4))
	:        (CL_ANALISER_LEVEL2|(CLB_ANALISER<<4));
   sa_attribs[endline-y][k]=color;
   sa_chars[endline-y][k]=(analph[0])? 22:31;
  }else
   y=endline+1;
  for(i=1;i<y;i++){
   if(cvol>logp[i]){
    color=(i< 5)? (CL_ANALISER_LEVEL0|(CLB_ANALISER<<4))
	 :(i<15)? (CL_ANALISER_LEVEL1|(CLB_ANALISER<<4))
	 :        (CL_ANALISER_LEVEL2|(CLB_ANALISER<<4));
    sa_attribs[endline-i][k]=color;
    sa_chars[endline-i][k]=186;
   }else{
    sa_attribs[endline-i][k]=CL_ANALISER_DOTS|(CLB_ANALISER<<4);
    sa_chars[endline-i][k]=250;
   }
  }
  analpt++;analpv++;analpn++;analph++;
 }
 for(y=0;y<anal_endline;y++){
  int x,beginx=-1,endx=-1;
  char *sapc=&sa_prev_chars[y][0],*sac=&sa_chars[y][0];
  for(x=0;x<endcol;x++,sapc++,sac++){
   char c=*sac;
   if(*sapc!=c){
    *sapc=c;
    if(beginx<0)
     beginx=x;
    else
     endx=x;
   }
  }
  if(beginx>=0){
   if(endx<0)
    endx=beginx;
   pds_textdisplay_textxyan(beginx,y,&sa_chars[y][beginx],&sa_attribs[y][beginx],(endx-beginx+1));
  }
 }
}

#else

static void draw_spectrum_analiser(unsigned long *analpt)
{
 unsigned int i,k,y,endline,endcol,cvol,*logp,color;
 long *analpv,*analpn,*analph;

 if(!(displaymode&DISP_ANALISER) || !(displaymode&DISP_NOFULLEDIT))
  return;

 analpv=&analpeekvol[0];
 analpn=&analpeeknum[0];
 analph=&analpeekhold[0];
 endline=anal_endline;
 logp=anal_logp;
 endcol=32;
 analpt[0]>>=3;
 analpt[1]>>=1;
 analpt[2]>>=1;
 for(k=0;k<endcol;k++){
  cvol=analpt[0];
  if(analph[0])
   analph[0]--;
  else{
   if(analpn[0]){
    pds_textdisplay_charxybk(CL_ANALISER_DOTS,CLB_ANALISER,k,endline-analpn[0],250);
    analpn[0]--;
    analpv[0]=logp[analpn[0]];
   }
  }
  if(analpv[0]<cvol && cvol>50){
   for(i=0;i<endline;i++)
    pds_textdisplay_charxybk(CL_ANALISER_DOTS,CLB_ANALISER,k,i,250);
   analpn[0]=0;
   for(i=1;i<=endline;i++){
    if(cvol<logp[i]){
     analpn[0]=i;
     break;
    }
   }
   analph[0]=20;
   analpv[0]=logp[analpn[0]];
  }
  y=analpn[0];
  if(y){
   color=(y< 5)? (CL_ANALISER_LEVEL0|(CLB_ANALISER<<4))
	:(y<15)? (CL_ANALISER_LEVEL1|(CLB_ANALISER<<4))
	:        (CL_ANALISER_LEVEL2|(CLB_ANALISER<<4));
   pds_textdisplay_charxy(color,k,endline-y,(analph[0])? 22:31);
  }else
   y=endline+1;
  for(i=1;i<y;i++){
   if(cvol>logp[i]){
    color=(i< 5)? (CL_ANALISER_LEVEL0|(CLB_ANALISER<<4))
	 :(i<15)? (CL_ANALISER_LEVEL1|(CLB_ANALISER<<4))
	 :        (CL_ANALISER_LEVEL2|(CLB_ANALISER<<4));
    pds_textdisplay_charxy(color,k,endline-i,186);
   }else
    pds_textdisplay_charxybk(CL_ANALISER_DOTS,CLB_ANALISER,k,endline-i,250);
  }
  analpt++;analpv++;analpn++;analph++;
 }
}
#endif

static void clear_analiser_peeks(void) // 25 <-> 50 lines change and analyser on/off
{
 unsigned int i;
 for(i=0;i<32;i++)
  analpeekvol[i]=analpeeknum[i]=analpeekhold[i]=0;
 if((displaymode&DISP_50LINES) || (textscreen_maxy>50)){
  anal_endline=15;
  anal_logp=(unsigned int *)&log50[0];
 }else{
  anal_endline=11;
  anal_logp=(unsigned int *)&log25[0];
 }
#ifdef MPXPLAY_WIN32
 pds_memset(&sa_prev_chars,0,sizeof(sa_prev_chars));
#endif
}

static void setmousecursorpalette(void)
{
#ifdef __DOS__
 static unsigned int palettec=0;
 unsigned char *ppal;
 static unsigned char palette[18][3]={{63,30,30},{63,41,30},{63,52,30},{63,63,30},
			    {52,63,30},{41,63,30},{30,63,30},{30,63,41},
			    {30,63,52},{30,63,63},{30,52,63},{30,41,63},
			    {30,30,63},{41,30,63},{52,30,63},{63,30,63},
			    {63,30,52},{63,30,41}};

 if(mouse_on){
  ppal=(char *)(&palette[palettec][0]);
  outp(0x03c8,CL_MOUSECURSOR);
  outp(0x03c9,ppal[0]);
  ppal++;
  outp(0x03c9,ppal[0]);
  ppal++;
  outp(0x03c9,ppal[0]);
  palettec++;
  if(palettec>=18)
   palettec=0;
 }
#endif
}

//*************************************************************************
// playlist editor routines
//*************************************************************************
#ifdef MPXPLAY_WIN32
#define EDITOR_USE_TEMPBUF 1
#endif

#ifdef EDITOR_USE_TEMPBUF
 static unsigned int editor_bufsize,editor_lastsize;
 static mpxp_uint16_t *editor_dispbuf,*editor_prevbuf;
 #define editor_xy_to_dispbuf(x,y) &editor_dispbuf[((y)-dtp.editorbegin)*textscreen_maxx+(x)]
 #define editor_textdisplay_charxybk(color,bkcolor,x,y,c) pds_textdisplay_char2bufbk(color,bkcolor,editor_xy_to_dispbuf(x,y),c)
 #define editor_textdisplay_textxy(color,x,y,s) pds_textdisplay_text2field(color,editor_xy_to_dispbuf(x,y),s)
 #define editor_textdisplay_textxybk(color,bkcolor,x,y,s) pds_textdisplay_text2fieldbk(color,bkcolor,editor_xy_to_dispbuf(x,y),s)

static void editor_draw_one_id3info_editor_line(struct playlist_entry_info *pei,unsigned int lastid,
                                  unsigned int col,unsigned int endcol,
	                          unsigned int color,unsigned int y,unsigned int showfilenameonly)
{
 static unsigned int id3order_fe[6]={I3I_ARTIST,I3I_TITLE,I3I_GENRE,I3I_ALBUM,I3I_YEAR,I3I_COMMENT};
 static unsigned int id3textlen[6]={1,0,7,6,9,7};
 static char *id3text[6]={":",""," Album:"," Year:"," Comment:"," Genre:"};
 char *shortfname,**id3ip=&pei->id3info[0];
 unsigned int i,k,len,showseparator=0,lengthcol,bkcolor;

 if(col>=endcol)
  return;

 lengthcol=endcol-col;
 bkcolor=color>>4;
 if(showfilenameonly || (GET_HFT(pei->entrytype)==HFT_DFT) || (!id3ip[I3I_ARTIST] && !id3ip[I3I_TITLE])){
  len=pds_strcpy(souttext,pei->filename);
  if((pei->entrytype&(DFTM_DFT|DFTM_DRIVE))==(DFTM_DFT|DFTM_DRIVE))
   shortfname=&souttext[0];
  else{
   if(!(id3textconv&ID3TEXTCONV_FILENAME) && (id3textconv&(ID3TEXTCONV_CODEPAGE|ID3TEXTCONV_UTF_AUTO)))
    mpxplay_playlist_textconv_selected_do(souttext,len,(id3textconv|ID3TEXTCONV_UTF8),0); // note: UTF8 is required for FTP filenames
   shortfname=pds_getfilename_from_fullname(souttext);
   len=pds_strlen(shortfname);
  }
  pds_strcat(shortfname,"             ");
  shortfname[lengthcol]=0;
  editor_textdisplay_textxy(color,col,y,shortfname);
  if(len<13)
   col+=13;
  else
   col+=len+2;
 }
 if(!showfilenameonly || (GET_HFT(pei->entrytype)==HFT_DFT)){
  for(k=0;k<lastid;k++){
   i=id3order_fe[k];
   if(id3ip[i]!=NULL){
    len=pds_strlen(id3ip[i]);
    if((col+id3textlen[i])>endcol)
     break;
    if(showseparator && id3textlen[i]){
     editor_textdisplay_textxybk(CL_EDITOR_AT_SEPARATOR,bkcolor,col,y,(char *)id3text[i]);
     col+=id3textlen[i];
    }
    if((col+len)>endcol){
     pds_strcpy(souttext,id3ip[i]);
     souttext[endcol-col]=0;
     editor_textdisplay_textxy(color,col,y,souttext);
     col=endcol;
     break;
    }else{
     editor_textdisplay_textxy(color,col,y,id3ip[i]);
     col+=len;
    }
    showseparator=1;
   }
   if(k==1)
    showseparator=1;
  }
 }
 if(col<endcol)
  for(k=col;k<endcol;k++)
   editor_textdisplay_charxybk(CL_EDITOR_BASE,bkcolor,k,y,' ');
}

static void editor_draw_difference(unsigned int lineposy)
{
 mpxp_uint16_t *dp=editor_dispbuf,*pp=editor_prevbuf;
 unsigned int x,y,beginy,endy;
 if(lineposy){
  beginy=endy=lineposy;
  x=(lineposy-dtp.editorbegin)*textscreen_maxx;
  dp+=x;pp+=x;
 }else{
  beginy=dtp.editorbegin;
  endy=dtp.editorend;
 }
 for(y=beginy;y<=endy;y++){
  int beginx=-1,endx=-1;
  for(x=0;x<textscreen_maxx;x++){
   if(*pp!=*dp){
    *pp=*dp;
    if(beginx<0)
     beginx=x;
    else
     endx=x;
   }
   dp++;pp++;
  }
  if(beginx>=0){
   if(endx<0)
    endx=beginx;
   pds_textdisplay_textbufxyn(beginx,y,&editor_dispbuf[textscreen_maxx*(y-dtp.editorbegin)+beginx],endx-beginx+1);
  }
 }
}

#else
 #define editor_textdisplay_charxybk(color,bkcolor,x,y,c) pds_textdisplay_charxybk(color,bkcolor,x,y,c)
 #define editor_textdisplay_textxy(color,x,y,s) pds_textdisplay_textxy(color,x,y,s)
 #define editor_textdisplay_textxybk(color,bkcolor,x,y,s) pds_textdisplay_textxybk(color,bkcolor,x,y,s)
 #define editor_draw_one_id3info_editor_line draw_one_id3info_line
 #define editor_draw_difference()
#endif

static void init_editorpos(struct mainvars *mvp,unsigned int force_reset)
{
#ifdef EDITOR_USE_TEMPBUF
 unsigned int newsize;
#endif
 if(desktopmode&DTM_EDIT_VERTICAL){
  if(editorsideborder<EDITOR_SIDE_SIZE_MIN)
   editorsideborder=EDITOR_SIDE_SIZE_MIN;
  if(editorsideborder>(textscreen_maxx-EDITOR_SIDE_SIZE_MIN-3))
   editorsideborder=textscreen_maxx-EDITOR_SIDE_SIZE_MIN-3;
  ed[0].begincol=1;
  if(mvp->psie==mvp->psi0)
   ed[1].begincol=ed[0].begincol+editorsideborder+1;
  else
   ed[1].begincol=textscreen_maxx-1-editorsideborder-ed[0].begincol;
  ed[0].lengthcol=ed[1].begincol-ed[0].begincol-1;
  ed[1].lengthcol=textscreen_maxx-ed[1].begincol-1;
  ed[0].beginline=ed[1].beginline=dtp.editorbegin+1;
  ed[0].lengthline=ed[1].lengthline=dtp.editorend-ed[0].beginline;
 }else{
  int editorysize=dtp.editorend-dtp.editorbegin;
  int pageysize=editorysize/2-1;
  if(pageysize<=0)
   pageysize=1;
  ed[0].begincol=ed[1].begincol=1;
  ed[0].lengthcol=ed[1].lengthcol=textscreen_maxx-2;
  ed[0].beginline=dtp.editorbegin+1;
  ed[0].lengthline=pageysize;
  ed[1].beginline=ed[0].beginline+pageysize+1;
  if(ed[1].beginline<dtp.editorend)
   ed[1].lengthline=dtp.editorend-ed[1].beginline;
  else
   ed[1].lengthline=0;
 }
#ifdef EDITOR_USE_TEMPBUF
 newsize=(dtp.editorend-dtp.editorbegin+1)*textscreen_maxx;
 if(editor_lastsize!=newsize || force_reset){
  editor_lastsize=newsize;
  if(editor_bufsize<newsize){
   editor_bufsize=newsize;
   if(editor_dispbuf)
    free(editor_dispbuf);
   if(editor_prevbuf)
    free(editor_prevbuf);
   editor_dispbuf=(mpxp_uint16_t *)calloc(1,editor_bufsize*sizeof(*editor_dispbuf));
   editor_prevbuf=(mpxp_uint16_t *)calloc(1,editor_bufsize*sizeof(*editor_prevbuf));
   if(!editor_dispbuf || !editor_prevbuf)
    mpxplay_close_program(MPXERROR_XMS_MEM);
  }else{
   pds_memset(editor_dispbuf,0,editor_bufsize*sizeof(*editor_dispbuf));
   pds_memset(editor_prevbuf,0,editor_bufsize*sizeof(*editor_prevbuf));
  }
 }
#endif
}

static void display_editor_close(void)
{
#ifdef EDITOR_USE_TEMPBUF
 if(editor_dispbuf)
  free(editor_dispbuf);
 if(editor_prevbuf)
  free(editor_prevbuf);
#endif
}

static void draw_editor_borders(void)
{
 unsigned int j;
 if(displaymode&DISP_FULLSCREEN){
  for(j=1;j<(textscreen_maxx-1);j++){
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,dtp.editorbegin,editorchars[0]);
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,dtp.editorend,editorchars[0]);
  }
  for(j=(dtp.editorbegin+1);j<dtp.editorend;j++){
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,0,j,editorchars[1]);
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,textscreen_maxx-1,j,editorchars[1]);
  }
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,0,dtp.editorbegin,editorchars[2]);
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,textscreen_maxx-1,dtp.editorbegin,editorchars[3]);
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,0,dtp.editorend,editorchars[4]);
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,textscreen_maxx-1,dtp.editorend,editorchars[5]);
  if(!funcbit_test(refdisp,RDT_EDITOR))
   editor_draw_difference(0);
 }
}

static void draw_elevator_pos(struct playlist_side_info *psi,unsigned int side)
{
 static unsigned int last_elevatorpos;
 struct editor_s *edp=&ed[side];
 struct mainvars *mvp=psi->mvp;

 if((desktopmode&DTM_EDIT_ELEVATOR) && (psi==mvp->psie) && (edp->lengthline>1)){
  unsigned int listlines=(psi->lastentry-psi->firstentry)+1;
  unsigned int lfrom=edp->from-psi->firstentry;
  unsigned int lines=edp->lengthline,elevatorpos;
  int remainlines=listlines-lines;

  lines--;
  if(remainlines>0)
   elevatorpos=((lfrom*lines)/remainlines);
  else
   elevatorpos=0;

  elevatorpos+=edp->beginline;
  if(last_elevatorpos>dtp.editorbegin)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,textscreen_maxx-1,last_elevatorpos,editorchars[1]);
  if(elevatorpos<edp->beginline)
   elevatorpos=edp->beginline;
  if(elevatorpos>(edp->beginline+edp->lengthline))
   elevatorpos=edp->beginline+edp->lengthline;
  editor_textdisplay_charxybk(CL_EDITOR_ELEVATOR,CLB_EDITOR_ELEVATOR,textscreen_maxx-1,elevatorpos,18);
  last_elevatorpos=elevatorpos;
 }
}

void set_mousepos_on_elevator(struct mainvars *mvp,unsigned int mousey)
{
 if(desktopmode&DTM_EDIT_ELEVATOR){
  struct playlist_side_info *psi=mvp->psie;
  struct editor_s *edp=&ed[psi - mvp->psi0];
  unsigned int listlines=(psi->lastentry-psi->firstentry)+1;
  unsigned int elevatorpos=mousey-edp->beginline,fe;
  int lines=edp->lengthline;
  int remainlines=listlines-lines;
  lines--;

  if(lines>0 && remainlines>0){
   struct playlist_entry_info *pei;
   if(elevatorpos)
    fe=(elevatorpos*remainlines)/lines+1;
   else
    fe=0;
   pei=psi->firstentry+fe;
   edp->from=pei;
   if(psi->editorhighline<pei)
    playlist_editorhighline_set_nocenter(psi,pei);
   else
    if(psi->editorhighline>(pei+lines))
     playlist_editorhighline_set_nocenter(psi,pei+lines);
   refdisp|=RDT_EDITOR;
  }
 }
}

void set_mousepos_on_editor(struct mainvars *mvp,unsigned int mousex,unsigned int mousey)
{
 struct playlist_side_info *psi=mvp->psi0;
 struct editor_s *edp=&ed[0];
 unsigned int side;

 for(side=0;side<PLAYLIST_MAX_SIDES;side++,psi++,edp++){
  if(mousex>=edp->begincol && mousex<(edp->begincol+edp->lengthcol)
     && mousey>=edp->beginline && mousey<(edp->beginline+edp->lengthline))
   break;
 }
 if(side>=PLAYLIST_MAX_SIDES)
  return;

 if(psi->editsidetype&PLT_ENABLED){
  if(psi!=mvp->psie)
   playlist_change_editorside(mvp);
  psi=mvp->psie;
  playlist_editorhighline_set_nocenter(psi,edp->from+mousey-edp->beginline);
  refdisp|=RDT_EDITOR;
 }
}

static void draw_editor_head_foot(unsigned int tcolor,unsigned int tpos,char *text,unsigned int tlen,unsigned int y,char vsep)
{
 unsigned int j,vspos=ed[1].begincol-1;

 if((desktopmode&DTM_EDIT_VERTICAL) && (vspos<tpos)){
  for(j=1;j<vspos;j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,y,editorchars[0]);
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,vspos,y,vsep);
  for(j=(vspos+1);j<tpos;j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,y,editorchars[0]);
 }else{
  for(j=1;j<tpos;j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,y,editorchars[0]);
 }
 editor_textdisplay_textxy(tcolor,tpos,y,text);

 j=tpos+tlen;
 if((desktopmode&DTM_EDIT_VERTICAL) && (vspos>=j)){
  for(;j<vspos;j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,y,editorchars[0]);
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,vspos,y,vsep);
  for(j=(vspos+1);j<(textscreen_maxx-1);j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,y,editorchars[0]);
 }else{
  for(;j<(textscreen_maxx-1);j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,y,editorchars[0]);
 }
}

void draweditor(struct mainvars *mvp)
{
 unsigned int j,listlines,editor_rows,displayed_rows,len,x;
 unsigned int colpos,side,begincol,endcol,endcol_id3,lengthcol;
 unsigned int color,bkcolor,lineposy,showfilenameonly;
 struct playlist_side_info *psi=mvp->psie;
 struct playlist_entry_info *pei;
 struct editor_s *edp;
 char stmp[MAX_PATHNAMELEN];

 playlist_chkfile_start_ehline(psi,psi->editorhighline);

 if(!(displaymode&DISP_FULLSCREEN))
  return;

 if(desktopmode&DTM_EDIT_VERTICAL){
  for(j=ed[0].beginline;j<(ed[0].beginline+ed[0].lengthline);j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,ed[1].begincol-1,j,editorchars[1]);
 }else{
  for(j=1;j<(textscreen_maxx-1);j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,ed[1].beginline-1,editorchars[0]);
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,0,ed[1].beginline-1,editorchars[8]);
  editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,textscreen_maxx-1,ed[1].beginline-1,editorchars[9]);
 }

 pei=psi->editorhighline;
 if(desktopmode&DTM_EDIT_FULLPATH){
  if((psi->editsidetype&PLT_ENABLED) && (psi->lastentry>=psi->firstentry)){
   if(!(id3textconv&ID3TEXTCONV_FILENAME) && (id3textconv&(ID3TEXTCONV_CODEPAGE|ID3TEXTCONV_UTF_AUTO))){
    len=pds_strcpy(stmp,pei->filename);
    mpxplay_playlist_textconv_selected_do(stmp,len,(id3textconv|ID3TEXTCONV_UTF8),0); // note: UTF8 is required for FTP filenames
    len=sprintf(souttext,"%s (%d)",stmp,pei-psi->firstentry+1);
   }else
    len=sprintf(souttext,"%s (%d)",pei->filename,pei-psi->firstentry+1);
  }else{
   if(psi->currdir[0])
    len=sprintf(souttext," %s ",psi->currdir);
   else
    len=pds_strcpy(souttext," No directory infos (try ctrl-r) ");
  }
  if(len<(textscreen_maxx-2)){
   x=(textscreen_maxx>>1)-(len>>1);
   draw_editor_head_foot(CL_EDITOR_FULLPATH|(CLB_EDITOR_FULLPATH<<4),x,souttext,len,dtp.editorbegin,editorchars[6]);
  }else{
   sprintf(stmp,"%c%c%c...%s",souttext[0],souttext[1],souttext[2],&souttext[len-(textscreen_maxx-9)]);
   editor_textdisplay_textxybk(CL_EDITOR_FULLPATH,CLB_EDITOR_FULLPATH,2,dtp.editorbegin,stmp);
  }
 }else{
  for(j=1;j<(textscreen_maxx-1);j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,dtp.editorbegin,editorchars[0]);
 }
 //if(desktopmode&DTM_EDIT_TOTALS){
 // len=sprintf(souttext,"[Total songs: %d  Time: %d:%2.2d:%2.2d]",allsongnum,mvp->psip->fulltimesec/3600,(mvp->psip->fulltimesec/60)%60,mvp->psip->fulltimesec%60);
 // x=textscreen_maxx-len-1;
 // draw_editor_head_foot(CL_EDITOR_FULLTIME|(CLB_EDITOR_BORDER<<4),x,souttext,len,dtp.editorend,editorchars[7]);
 //}else
 if(desktopmode&DTM_EDIT_FULLTIME){
  unsigned int fulltimesec=mvp->psip->fulltimesec;
  len=sprintf(souttext,"[%d:%2.2d:%2.2d]",fulltimesec/3600,(fulltimesec/60)%60,fulltimesec%60);
  x=textscreen_maxx-len-1;
  draw_editor_head_foot(CL_EDITOR_FULLTIME|(CLB_EDITOR_BORDER<<4),x,souttext,len,dtp.editorend,editorchars[7]);
 }else{
  for(j=1;j<(textscreen_maxx-1);j++)
   editor_textdisplay_charxybk(CL_EDITOR_BORDER,CLB_EDITOR_BORDER,j,dtp.editorend,editorchars[0]);
 }

 psi=mvp->psi0;
 edp=&ed[0];
 for(side=0;side<PLAYLIST_MAX_SIDES;side++,psi++,edp++){
  begincol=edp->begincol;
  lengthcol=edp->lengthcol;
  endcol=begincol+lengthcol; //-1;

  lineposy=edp->beginline;
  editor_rows=edp->lengthline-1;

  j=0;
  if((psi->editsidetype&PLT_ENABLED) && (psi->lastentry>=psi->firstentry)){
   listlines=psi->lastentry-psi->firstentry;
   if(listlines<=editor_rows){
    displayed_rows=listlines;
    pei=psi->firstentry;
   }else{
    displayed_rows=editor_rows;
    pei=edp->from;
    if(pei>=(psi->lastentry-editor_rows))
     pei=psi->lastentry-editor_rows;
    else
     if(pei<psi->firstentry) // ???
      pei=psi->firstentry;
   }
   if(pei<(psi->editorhighline-editor_rows)){
    pei=psi->editorhighline-editor_rows;
   }else{
    if(pei>psi->editorhighline)
     pei=psi->editorhighline;
   }
   edp->from=pei;
   if(edp->from_prev!=pei){
    edp->from_prev=pei;
    playlist_chkfile_start_disp(psi,pei,pei+displayed_rows);
   }
   draw_elevator_pos(psi,side);
   for(;j<=displayed_rows;j++,lineposy++,pei++){
    colpos=begincol;
    color=set_editorline_color(psi,pei);
    if((psi==mvp->psie) && (pei==psi->editorhighline))
     bkcolor=CLB_EDITOR_HIGHLINE;
    else
     bkcolor=CLB_EDITOR_BASE;
    color|=bkcolor<<4;
    if(lengthcol && (desktopmode&DTM_EDIT_SONGNUM) && (GET_HFT(pei->entrytype)!=HFT_DFT) && ((pei->entrytype>=DFT_AUDIOFILE) || !funcbit_test(desktopmode,DTM_EDIT_ALLFILES))){ // show filenumber
     unsigned int en=pei-psi->firstsong+1;
     if(en<100)
      len=sprintf(souttext,"%2.2d.  ",en);
     else
      len=sprintf(souttext,"%3d. ",en);
     if(lengthcol<7) // 7=max. length of filenum (9999.  ) // ???
      souttext[lengthcol]=0;
     editor_textdisplay_textxy(color,colpos,lineposy,souttext);
     colpos+=max(5,len); // artist:title begins on a fixed position
    }
    endcol_id3=endcol;
    if((desktopmode&DTM_EDIT_SONGTIME) && (lengthcol>20) && (pei->infobits&PEIF_ENABLED)){
     unsigned long timesec=(playlist_entry_get_timemsec(pei)+500)/1000;
     len=sprintf(souttext,"(%d:%2.2d)",timesec/60,timesec%60);
     if(len<=lengthcol){
      endcol_id3-=len;
      editor_textdisplay_textxy(color,endcol_id3,lineposy,souttext);
     }
    }else if((desktopmode&DTM_EDIT_SHOWDIRDATE) && (lengthcol>29) && ((pei->entrytype==DFT_SUBDIR) || (pei->entrytype==DFT_SUBLIST) || (pei->entrytype<DFTM_DFT) || (pei->entrytype>=DFT_AUDIOFILE))){
     pds_fdate_t *d=&pei->filedate;
     len=sprintf(souttext,"%4d.%2.2d.%2.2d %2d:%2.2d",d->year+1980,d->month,d->day,d->hours,d->minutes);
     if(len<=lengthcol){
      endcol_id3-=len;
      editor_textdisplay_textxy(color,endcol_id3,lineposy,souttext);
      endcol_id3--;
      editor_textdisplay_charxybk(CL_EDITOR_BASE,bkcolor,endcol_id3,lineposy,' ');//editorchars[1]);
     }
     if(lengthcol>35){
      if((pei->entrytype!=DFT_SUBDIR) || pei->filesize){
       mpxp_filesize_t f=pei->filesize;
       char *s=NULL;
       if(f>=10240000){
        f=(f+524288)/1048576;
        s="M";
       }else if(f>=100000){
        f=(f+512)/1024;
        s="k";
       }
       if(s)
        len=sprintf(souttext,"%4d%s",f,s);
       else
        len=sprintf(souttext,"%5d",f);
       endcol_id3-=len;
       editor_textdisplay_textxy(color,endcol_id3,lineposy,souttext);
       endcol_id3--;
       editor_textdisplay_charxybk(CL_EDITOR_BASE,bkcolor,endcol_id3,lineposy,' ');//editorchars[1]);
      }
     }
    }
    showfilenameonly=((desktopmode&DTM_EDIT_FILENAMES) || ((pei->entrytype<DFTM_DFT) && (desktopmode&DTM_EDIT_ALLFILES)));
    editor_draw_one_id3info_editor_line(pei,2,colpos,endcol_id3,color,lineposy,showfilenameonly);
   }
  }
#ifdef EDITOR_USE_TEMPBUF
  for(;j<=editor_rows;j++,lineposy++)
   for(x=begincol;x<endcol;x++)
    editor_textdisplay_charxybk(CL_EDITOR_BASE,CLB_EDITOR_BASE,x,lineposy,' ');
  editor_draw_difference(0);
#else
  for(;j<=editor_rows;j++,lineposy++)
   pds_textdisplay_spacecxyn(CLB_EDITOR_BASE,begincol,lineposy,lengthcol);
#endif
 }
}

void display_draw_editor_oneline(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 struct mainvars *mvp=psi->mvp;
 unsigned int colpos,color,bkcolor,len,endcol,lengthcol,lineposy,endcol_id3,editor_rows,showfilenameonly;
 int side;
 struct editor_s *edp;

 if(desktopmode&DTM_EDIT_FULLTIME){
  unsigned int fulltimesec=mvp->psip->fulltimesec;
  len=sprintf(souttext,"[%d:%2.2d:%2.2d]",fulltimesec/3600,(fulltimesec/60)%60,fulltimesec%60);
  editor_textdisplay_textxy(CL_EDITOR_FULLTIME|(CLB_EDITOR_BORDER<<4),textscreen_maxx-len-1,dtp.editorend,souttext);
  editor_draw_difference(dtp.editorend);
 }

 side=psi-mvp->psi0;
 if(side<0 || side>PLAYLIST_MAX_SIDES)
  return;
 edp=&ed[side];
 editor_rows=edp->lengthline-1;
 if((pei<edp->from) || (pei>(edp->from+editor_rows)))
  return;
 lineposy=edp->beginline+(pei-edp->from);
 colpos=edp->begincol;
 lengthcol=edp->lengthcol;
 endcol=colpos+lengthcol; //-1;
 color=set_editorline_color(psi,pei);
 if((psi==mvp->psie) && (pei==psi->editorhighline))
  bkcolor=CLB_EDITOR_HIGHLINE;
 else
  bkcolor=CLB_EDITOR_BASE;
 color|=bkcolor<<4;
 if(lengthcol && (desktopmode&DTM_EDIT_SONGNUM) && (GET_HFT(pei->entrytype)!=HFT_DFT) && ((pei->entrytype>=DFT_AUDIOFILE) || !funcbit_test(desktopmode,DTM_EDIT_ALLFILES))){ // show filenumber
  unsigned int en=pei-psi->firstsong+1;
  if(en<100)
   len=sprintf(souttext,"%2.2d.  ",en);
  else
   len=sprintf(souttext,"%3d. ",en);
  if(lengthcol<7) // 7=max. length of filenum (9999.  ) // ???
   souttext[lengthcol]=0;
  editor_textdisplay_textxy(color,colpos,lineposy,souttext);
  colpos+=max(5,len); // artist:title begins on a fixed position
 }
 endcol_id3=endcol;
 if((desktopmode&DTM_EDIT_SONGTIME) && (lengthcol>20) && (pei->infobits&PEIF_ENABLED)){
  unsigned long timesec=(playlist_entry_get_timemsec(pei)+500)/1000;
  len=sprintf(souttext,"(%d:%2.2d)",timesec/60,timesec%60);
  if(len<=lengthcol){
   endcol_id3-=len;
   editor_textdisplay_textxy(color,endcol_id3,lineposy,souttext);
  }
 }else if((desktopmode&DTM_EDIT_SHOWDIRDATE) && (lengthcol>29) && ((pei->entrytype==DFT_SUBDIR) || (pei->entrytype==DFT_SUBLIST) || (pei->entrytype<DFTM_DFT) || (pei->entrytype>=DFT_AUDIOFILE))){
  pds_fdate_t *d=&pei->filedate;
  len=sprintf(souttext,"%4d.%2.2d.%2.2d %2d:%2.2d",d->year+1980,d->month,d->day,d->hours,d->minutes);
  if(len<=lengthcol){
   endcol_id3-=len;
   editor_textdisplay_textxy(color,endcol_id3,lineposy,souttext);
   endcol_id3--;
   editor_textdisplay_charxybk(CL_EDITOR_BASE,bkcolor,endcol_id3,lineposy,' ');//editorchars[1]);
  }
  if(lengthcol>35){
   if((pei->entrytype!=DFT_SUBDIR) || pei->filesize){
    mpxp_filesize_t f=pei->filesize;
    char *s=NULL;
    if(f>=10240000){
     f=(f+524288)/1048576;
     s="M";
    }else if(f>=100000){
     f=(f+512)/1024;
     s="k";
    }
    if(s)
     len=sprintf(souttext,"%4d%s",f,s);
    else
     len=sprintf(souttext,"%5d",f);
    endcol_id3-=len;
    editor_textdisplay_textxy(color,endcol_id3,lineposy,souttext);
    endcol_id3--;
    editor_textdisplay_charxybk(CL_EDITOR_BASE,bkcolor,endcol_id3,lineposy,' ');//editorchars[1]);
   }
  }
 }
 showfilenameonly=((desktopmode&DTM_EDIT_FILENAMES) || ((pei->entrytype<DFTM_DFT) && (desktopmode&DTM_EDIT_ALLFILES)));
 editor_draw_one_id3info_editor_line(pei,2,colpos,endcol_id3,color,lineposy,showfilenameonly);
 editor_draw_difference(lineposy);
}

static unsigned int set_editorline_color(struct playlist_side_info *psi,struct playlist_entry_info *pei)
{
 unsigned int color=CL_EDITOR_PLAYED; // gray
 struct mainvars *mvp=psi->mvp;

 if(pei->infobits&PEIF_SELECTED)
  color=CL_EDITOR_SELECTED;     // light magenta
 else
  if((psi==mvp->psip) && (GET_HFT(pei->entrytype)!=HFT_DFT) && (pei->infobits&PEIF_ENABLED)){
   if(pei==mvp->newfilenum)
    color=CL_EDITOR_NEWFILENUM;   // light green
   else if(pei==mvp->aktfilenum)
    color=CL_EDITOR_AKTFILENUM;   // yellow
   else if((pei->entrytype>=DFT_AUDIOFILE) || (pei->entrytype==DFT_NOTCHECKED)){
    if(playrand){
     if(!(pei->infobits&PEIF_RNDPLAYED))
      color=CL_EDITOR_NOTPLAYED; // white
    }else{
     if(pei>=mvp->aktfilenum)
      color=CL_EDITOR_NOTPLAYED; // white
    }
   }
  }
 return color;
}

unsigned int display_editor_resize_x(struct mainvars *mvp,int direction)
{
 struct playlist_side_info *psie=mvp->psie;
 if((psie==mvp->psi0 && direction<0) || (psie!=mvp->psi0 && direction>0)){
  if(editorsideborder>EDITOR_SIDE_SIZE_MIN){
   editorsideborder--;
   return 1;
  }
 }else{
  if(editorsideborder<(textscreen_maxx-EDITOR_SIDE_SIZE_MIN-3)){
   editorsideborder++;
   return 1;
  }
 }
 return 0;
}

void display_editor_resize_y(int direction)
{
 int maxline=dtp.editorend-dtp.endofbuttonsy;

 maxline-=(desktopmode&DTM_EDIT_VERTICAL)? 4:6;

 dtp.relative_songposline+=direction;

 if(dtp.relative_songposline<0)
  dtp.relative_songposline=0;
 if(dtp.relative_songposline>maxline)
  dtp.relative_songposline=maxline;

 dtp.relative_songposline-=dtp.relative_songposline%3;

 dtp.songposline_y=dtp.editorbegin=dtp.endofbuttonsy+1+dtp.relative_songposline;
 if(desktopmode&DTM_SONGPOS)
  dtp.editorbegin++;
}

void scroll_editorside(struct mainvars *mvp,struct playlist_entry_info *ehls)
{
 struct playlist_side_info *psi=mvp->psie;
 struct editor_s *edp=&ed[psi - mvp->psi0];
 unsigned int listlines=(psi->lastentry-psi->firstentry)+1;
 int editor_rows=edp->lengthline;
 int remainlines=listlines-editor_rows;
 struct playlist_entry_info *newfrom;
 editor_rows--;

 if(editor_rows>0 && remainlines>0){
  int diff=ehls-psi->editorhighline;
  newfrom=edp->from+diff;
  if(newfrom>=(psi->lastentry-editor_rows))
   newfrom=psi->lastentry-editor_rows;
  if(newfrom<psi->firstentry)
   newfrom=psi->firstentry;
  edp->from=newfrom;
  playlist_editorhighline_set_nocenter(psi,ehls);
  refdisp|=RDT_EDITOR;
 }
}

#define MDCE_VISIBLE_LINES 4 // give a high value to keep the cursor on the middle of editorside

void mpxplay_display_center_editorhighline(struct playlist_side_info *psi,unsigned int visible_lines)
{
 if(!(desktopmode&DTM_EDIT_MDCE_DISABLE)){
  struct mainvars *mvp=psi->mvp;
  struct editor_s *edp=&ed[psi - mvp->psi0];
  unsigned int listlines=(psi->lastentry-psi->firstentry)+1;
  int editor_rows=edp->lengthline;
  struct playlist_entry_info *newfrom;
  editor_rows--;
  if(!visible_lines)
   visible_lines=MDCE_VISIBLE_LINES;

  if((editor_rows>0) && (listlines>editor_rows)){
   if(editor_rows<(visible_lines*2))
    newfrom=psi->editorhighline-(editor_rows/2);
   else if(edp->from>(psi->editorhighline-visible_lines))
    newfrom=psi->editorhighline-visible_lines;
   else if(edp->from<(psi->editorhighline-editor_rows+visible_lines))
    newfrom=psi->editorhighline-editor_rows+visible_lines;
   else
    newfrom=edp->from;
   if(newfrom>=(psi->lastentry-editor_rows))
    newfrom=psi->lastentry-editor_rows;
   if(newfrom<psi->firstentry)
    newfrom=psi->firstentry;
   edp->from=newfrom;
   refdisp|=RDT_EDITOR;
  }
 }
}

//*************************************************************************
// mousepos functions
//*************************************************************************

void clear_mousepos_text(void)
{
 if(displaymode&DISP_NOFULLEDIT)
  pds_textdisplay_spacecxyn(CLB_INFOTEXT,dk[DK_MOUSEPOS].xpos,dk[DK_MOUSEPOS].ypos,8);
}

void draw_mousepos_text(unsigned int isbutton,char *text)
{
 short color;
 if(displaymode&DISP_NOFULLEDIT){
  if(isbutton)
   color=CLH_INFOTEXTB;
  else
   color=CLH_INFOTEXT;
  text[8]=0; // cut to window
  clear_mousepos_text();
  pds_textdisplay_textxybk(color,CLB_INFOTEXT,dk[DK_MOUSEPOS].xpos,dk[DK_MOUSEPOS].ypos,text);
 }
}

void draw_mouse_listpos(struct mainvars *mvp,unsigned int i)
{
 struct playlist_side_info *psi=mvp->psip;
 struct playlist_entry_info *pei=psi->firstsong+i;

 if(pei>=psi->firstsong && pei<=psi->lastentry){
  pds_getfilename_noext_from_fullname(souttext,pei->filename);
  draw_mousepos_text(1,souttext);
  if((desktopmode&DTM_EDIT_FOLLOWBROWSER) && (psi==mvp->psie)){
   playlist_editorhighline_set(psi,pei);
   refdisp|=RDT_EDITOR;
  }
 }
}

void draw_mouse_desktoppos(struct mainvars *mvp,unsigned int mousex,unsigned int mousey)
{
#ifdef __DOS__
 unsigned int mousec;
#endif
 unsigned int i;

 if(!(displaymode&DISP_FULLSCREEN) || !mouse_on)
  return;
#ifdef __DOS__
 mousec=pds_textdisplay_lowlevel_getbkcolorxy(lastmousex,lastmousey);
 if(mousec!=CL_MOUSECURSOR)
  lastmousec=mousec;
#endif
 if((lastmousex!=mousex) || (lastmousey!=mousey) || (lastmousebox==LASTMOUSEBOX_INVALID)){
  i=LASTMOUSEBOX_INVALID;
  if(!display_textwin_handle_mousepos(mousex,mousey,0)){
   if((desktopmode&DTM_SONGPOS) && (mousey==dtp.songposline_y) && (mousex!=0) && (mousex!=(textscreen_maxx-1))){
    recolor_lastbuttonbox_mousepos(mvp,NULL);
    i=(mvp->frp0->timesec*(mousex-1))/(textscreen_maxx-2);
    sprintf(souttext," %2lu:%2.2lu   ",i/60,i%60);
    draw_mousepos_text(1,souttext);
   }else{
    if((desktopmode&DTM_LISTPOS) && (mousey==dtp.listposline_y) && (mousex!=0) && (mousex!=(textscreen_maxx-1))){
     recolor_lastbuttonbox_mousepos(mvp,NULL);
     draw_mouse_listpos(mvp,allsongnum*mousex/(textscreen_maxx-2));
     i=0;
    }else{
     if((displaymode&(DISP_NOFULLEDIT|DISP_ANALISER))==(DISP_NOFULLEDIT|DISP_ANALISER)
       && (mousey<anal_endline) && (mousex<32)){
      recolor_lastbuttonbox_mousepos(mvp,NULL);
      sprintf(souttext,"%5d Hz",(mousex+1)*22050/32);
      draw_mousepos_text(0,souttext);
      i=0;
     }else{
      if(mpxplay_control_mouse_xy_to_dp(mvp,mousex,mousey)!=NULL)
       i=0;
     }
    }
   }
  }
  if(i==LASTMOUSEBOX_INVALID)
   recolor_lastbuttonbox_mousepos(mvp,NULL);
#ifdef __DOS__
  pds_textdisplay_lowlevel_setbkcolorxy(lastmousec,lastmousex,lastmousey);
  lastmousec=pds_textdisplay_lowlevel_getbkcolorxy(mousex,mousey);
#endif
  lastmousex=mousex;lastmousey=mousey;
  lastmousebox=0;
 }
#ifdef __DOS__
 pds_textdisplay_lowlevel_setbkcolorxy(CL_MOUSECURSOR,lastmousex,lastmousey);
#endif
}

void draw_mouse_restorelastmousec(void)
{
#ifdef __DOS__
 unsigned int mousec=pds_textdisplay_lowlevel_getbkcolorxy(lastmousex,lastmousey);
 if(mousec!=CL_MOUSECURSOR)
  lastmousec=mousec;
 pds_textdisplay_lowlevel_setbkcolorxy(lastmousec,lastmousex,lastmousey);
#endif
}

//************************************************************************
void display_message(unsigned int linepos,unsigned int blink,char *msg)
{
 static char sout[TEXTSCREEN_MAXX+2],msg0[TEXTSCREEN_MAXX/2+2],line[TEXTSCREEN_MAXX+2]; // !!! (to avoid stack overflow if called from irq)

 if(displaymode && lockid3window<2){
  unsigned int color,len,windowlen;

  lockid3window=1;
  color=CLH_ID3TEXT|(CLB_ID3TEXT<<4);
  if(blink)
   color|=CL_BLINK;

  if(displaymode&DISP_NOFULLEDIT)
   windowlen=ID3WINDOW_SIZE_NOFE;
  else if (displaymode&DISP_VERBOSE)
   windowlen=textscreen_maxx;
  else if (displaymode&DISP_TIMEPOS)
   windowlen=textscreen_maxx/2;
  if(windowlen>TEXTSCREEN_MAXX)
   windowlen=TEXTSCREEN_MAXX;

  len=pds_strncpy(sout,msg,windowlen+1);
  if(len<(windowlen+1))
   pds_memset(&sout[len],32,windowlen+1-len);
  sout[windowlen]=0;

  if(displaymode&DISP_NOFULLEDIT){
   pds_textdisplay_textxy(color,dk[0].xpos+1,dk[DK_ID3INFO].ypos+linepos,sout);
  }else{
   if(displaymode&DISP_VERBOSE){
    if(!linepos)
     pds_textdisplay_textxy(color,0,POS_ID3INFO_FE,sout);
    else
     pds_textdisplay_textxy(color,0,POS_FILEINFO_FE,sout);
   }else{
    if(displaymode&DISP_TIMEPOS)
     if(!linepos){
      pds_textdisplay_textxy(color,0,dtp.timepos_fe,sout);
      pds_strcpy(msg0,sout);
     }else{
      pds_strcpy(line,msg0);
      pds_strcat(line,sout);
      line[textscreen_maxx-1]=0;
      pds_textdisplay_textxy(color,0,dtp.timepos_fe,line);
     }
   }
  }
  funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_DISPMESSAGE);
  funcbit_disable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_CLEARMESSAGE); //
  funcbit_disable(refdisp,RDT_ID3INFO);   // !!! hack
 }
}

void clear_message(void)
{
 if(lockid3window<2){
  lockid3window=0;
  refdisp|=RDT_ID3INFO;
  if(!(displaymode&DISP_NOFULLEDIT))
   refdisp|=RDT_HEADER;
  funcbit_enable(mpxplay_signal_events,MPXPLAY_SIGNALTYPE_CLEARMESSAGE);
 }
}

void display_clear_timed_message(void)
{
 if(lockid3window<3){
  lockid3window=0;
  clear_message();
  mpxplay_timer_deletefunc(&display_clear_timed_message,NULL); //
 }
}

void display_timed_message(char *message)
{
 char *line1,line0[128];

 if(lockid3window>2)
  return;
 lockid3window=0;

 line1=pds_strchr(message,'\n');
 if(line1){
  unsigned int len=line1-message;
  line1++;
  if(len){
   if(len>(sizeof(line0)-1))
    len=sizeof(line0)-1;
   pds_strncpy(line0,message,len);
   line0[len]=0;
   display_message(0,0,line0);
  }
  display_message(1,0,line1);
 }else{
  display_message(0,0,message);
  display_message(1,0,"");
 }
 mpxplay_timer_addfunc(&display_clear_timed_message,NULL,MPXPLAY_TIMERTYPE_WAKEUP,mpxplay_timer_secs_to_counternum(DISPLAY_MESSAGE_TIME));
 lockid3window=2;
}

void display_static_message(unsigned int linepos,unsigned int blink,char *msg)
{
 lockid3window=0;
 display_message(linepos,blink,msg);
 lockid3window=3;
 mpxplay_timer_deletefunc(&display_clear_timed_message,NULL);
}

void clear_static_message(void)
{
 lockid3window=0;
 clear_message();
}

void display_warning_message(char *message)
{
 if(displaymode){
  pds_textdisplay_printf(message);
  //delay(2000);
 }
}

void display_bufpos_int08(struct mainvars *mvp)
{
 long i;
 char sout[32];

 if(displaymode&DISP_FULLSCREEN){
  unsigned int color;
  struct frame *frp=mvp->frp0;
  if(!funcbit_test(intsoundcontrol,INTSOUND_DOSSHELL) || !funcbit_test(intsoundcontrol,INTSOUND_TSR)){
   if(funcbit_test(frp->buffertype,PREBUFTYPE_BACK) && !funcbit_test(intsoundcontrol,INTSOUND_DOSSHELL)){
    if(displaymode&DISP_NOFULLEDIT){
     pds_ftoi(100.0*(float)frp->prebufferbytes_forward/(float)frp->prebuffersize,&i);
     if(i>99)
      i=99;
     sprintf(sout,"%2d%%",i); // prebuffer %
     pds_textdisplay_textxybk(CL_INFOTEXT,CLB_INFOTEXT,dk[DK_BUFCPU].xpos+5,dk[DK_BUFCPU].ypos,sout);
    }
    pds_ftoi(100.0*(float)frp->prebufferbytes_rewind/(float)frp->prebuffersize,&i);
    if(i>99)
     i=99;
    sprintf(sout,"%2d%%",i); // backbuffer %
    color=CL_INFOTEXT;
   }else{
    pds_ftoi(100.0*(float)frp->prebufferbytes_forward/(float)frp->prebuffersize,&i);
    if(i>99)
     i=99;
    sprintf(sout,"%2d%%",i); // buffer left in %
    if(i<20)
     color=12;
    else
     color=CL_INFOTEXT;
   }
  }else{
   unsigned long index_pos=(frp->frameNum>frp->index_start)? (frp->frameNum-frp->index_start):0;
   pds_ftoi(100.0*(float)index_pos/(float)frp->index_len,&i);
   if(i>99)
    i=99;
   sprintf(sout,"Pos:%2d%%",i); // song position in %
   color=15;
  }
  if(funcbit_test(intsoundcontrol,INTSOUND_DOSSHELL))
   pds_textdisplay_textxybk(color,1,0,0,sout);
  else
   if(displaymode&DISP_NOFULLEDIT)
    pds_textdisplay_textxybk(color,CLB_INFOTEXT,dk[DK_BUFCPU].xpos,dk[DK_BUFCPU].ypos,sout);
 }
}

static char *keycontrols_help_text="\
 Play controls:                         Audio controls:                 \n\
\n\
  ESC/F10/ctrl-'c' - exit                .      - volume up (dot)\n\
  gray-'-' - skip back one song          ,      - volume down (comma)\n\
  gray-'+' - skip forward one song       '      - surround up\n\
  gray-'/' - skip back one album         ;      - surround down\n\
  gray-'*' - skip forward one album      ]      - speed up \n\
  alt-'/'  - load previous fastlist      [      - speed down\n\
  alt-'*'  - load next fastlist          <      - balance-left\n\
  left/right arrow - seek back/forward   >      - balance-right\n\
  ctrl-left/right  - faster seek         \"      - bass up\n\
  Backspace- jump to begin of song       :      - bass down\n\
   d       - hi-lite scan (demo mode)    }      - treble up\n\
   P       - start/pause playing         {      - treble down\n\
   S       - stop playing                |      - loudness on/off\n\
   N       - random play on/off         alt-'.' - soundcard volume up\n\
   R       - replay (repeat) mode       alt-',' - soundcard volume down\n\
  ctrl-'p' - autopause on/off            C      - crossfader on/off\n\
  ctrl-'s' - pause at next song          F      - cfade out/in select\n\
  ctrl-Enter - select next song          M      - mute (while press)\n\
                                        ctrl-'m'- mute sound (on/off)\n\
 Commander controls:                     V      - auto volume on/off\n\
                                        ctrl-V  - reset mixer functions\n\
  alt-F1/F2 - select drive               X      - swap channels\n\
  alt-F5/F6/F8 - copy/move/del file(s)\n\
  F7     - create directory             List controls:\n\
  F2     - save playlist\n\
  F3     - show file/dir infos           i       - insert index\n\
  F4     - edit id3 tags                ctrl-'i' - remove index\n\
  alt-'+'/'-' - select/unselect files    J       - juke-box mode on/off\n\
                                        ctrl-'r' - reload dir/list\n\
 Display controls:                       Ins     - select entry\n\
                                         F5/F8   - copy/del entry(s)\n\
  A     - analyser on/off               ctrl-Ins - copy all to otherside\n\
  T     - time mode select              ctrl-Del  - clear playlist\n\
 alt-F9  - 25/50 lines text mode        ctrl-F1-F4 - sort playlist\n\
 ctrl-F9 - full screen editor           ctrl-up/dn - shift entry\
";

void display_help_window(void)
{
 display_textwin_openwindow_keymessage(0," Help - keyboard controls ",keycontrols_help_text);
}
