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
//function: software tone (low and treble only)

#include "au_mixer.h"
#include "newfunc\newfunc.h"
#include "mix_func.h"
#include "decoders\decoders.h"
#include "display\display.h"

#define TONE_EQ_BANDS  10

#define TONE_SYNCFLAG_STOP  0 // eq stopped
#define TONE_SYNCFLAG_BEGIN 1 // synchronize at start
#define TONE_SYNCFLAG_END   2 // synchronize at stop
#define TONE_SYNCFLAG_RUN   3 // eq running

#define EQdB  15

static unsigned long eq_freqs[TONE_EQ_BANDS]={44,87,173,345,690,1379,2757,5513,11025,16538};
static float eq_band_powers[TONE_EQ_BANDS];

one_mixerfunc_info MIXER_FUNCINFO_tone_bass;
one_mixerfunc_info MIXER_FUNCINFO_tone_treble;

static unsigned int sync_flag;

static void calculate_eqgain(struct mpxplay_audioout_info_s *aui)
{
 int i,bass,data[TONE_EQ_BANDS];

 bass=aui->card_mixer_values[AU_MIXCHAN_BASS];
 data[0]=bass;
 data[1]=bass;
 //data[1]=(bass-50)/2+50;

 for(i=2;i<TONE_EQ_BANDS-3;i++)
  data[i]=50;

 data[TONE_EQ_BANDS-3]=(aui->card_mixer_values[AU_MIXCHAN_TREBLE]-50)/4+50;
 data[TONE_EQ_BANDS-2]=(aui->card_mixer_values[AU_MIXCHAN_TREBLE]-50)/2+50;
 data[TONE_EQ_BANDS-1]=aui->card_mixer_values[AU_MIXCHAN_TREBLE];

 for(i=0;i<TONE_EQ_BANDS;i++)
  if(data[i]>50)
   eq_band_powers[i] = pow(10,(float)(data[i]-50)/50.0*EQdB/20.0); // logarithmic
  else
   eq_band_powers[i] = (float)((data[i]))/50.f; // linear to zero

 mpxplay_decoders_audio_eq_config(aui->mvp->frp0,TONE_EQ_BANDS,&eq_freqs[0],&eq_band_powers[0]);
}

static unsigned int mixer_tone_check_decoder_eq(struct mpxplay_audioout_info_s *aui)
{
 struct frame *frp=aui->mvp->frp0;
 struct mpxplay_infile_info_s *miis=frp->infile_infos;
 struct playlist_entry_info *pei;
 char *e,sout[100];


 if(!miis || mpxplay_decoders_audio_eq_exists(frp))
  return 1;

 pei=aui->mvp->pei0;
 if(miis && miis->audio_decoder_infos && miis->audio_decoder_infos->shortname)
  e=miis->audio_decoder_infos->shortname;
 else if(frp->infile_funcs && frp->infile_funcs->file_extensions[0])
  e=frp->infile_funcs->file_extensions[0];
 else if(pei && pei->filename){
  e=pds_strrchr(pei->filename,'.');
  if(e)
   e++;
 }else
  e=NULL;

 snprintf(sout,sizeof(sout),"No decoder EQ %s%s! Use MX_SUPEQ plugin!",((e)? "for ":""),((e)? e:""));
 display_timed_message(sout);
 return 0;
}

static int mixer_tone_init(struct mpxplay_audioout_info_s *aui,int inittype)
{
 switch(inittype){
  case MIXER_INITTYPE_INIT:
        break;
  case MIXER_INITTYPE_REINIT:
        mpxplay_decoders_audio_eq_config(aui->mvp->frp0,TONE_EQ_BANDS,&eq_freqs[0],&eq_band_powers[0]);
        break;
  case MIXER_INITTYPE_START:
        calculate_eqgain(aui);
  case MIXER_INITTYPE_RESET:
        if(sync_flag==TONE_SYNCFLAG_END) // to avoid on-off-on
         sync_flag=TONE_SYNCFLAG_RUN;
        else
         sync_flag=TONE_SYNCFLAG_BEGIN;
        break;
  case MIXER_INITTYPE_CLOSE:
        break;
 }
 return 1;
}

static void mixer_tone_hq(struct mpxplay_audioout_info_s *aui)
{
 if(sync_flag==TONE_SYNCFLAG_END){
  sync_flag=TONE_SYNCFLAG_STOP;
  MIXER_checkfunc_setflags("MIX_TONE_BASS");
 }else{
  if(sync_flag==TONE_SYNCFLAG_BEGIN)
   sync_flag=TONE_SYNCFLAG_RUN;
 }
}

static int mixer_tone_checkvar_bass(struct mpxplay_audioout_info_s *aui)
{
 if(aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE)
  return 0;

 if((aui->card_mixer_values[AU_MIXCHAN_BASS]!=MIXER_FUNCINFO_tone_bass.var_center)
   || (aui->card_mixer_values[AU_MIXCHAN_TREBLE]!=MIXER_FUNCINFO_tone_treble.var_center)){
  return 1;
 }

 if((sync_flag==TONE_SYNCFLAG_RUN) || (sync_flag==TONE_SYNCFLAG_END)){ // to do a post processing
  sync_flag=TONE_SYNCFLAG_END;
  return 1;
 }
 sync_flag=TONE_SYNCFLAG_STOP;

 return 0;
}

static int mixer_tone_checkvar_treble(struct mpxplay_audioout_info_s *aui)
{
 if(aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE)
  return 0;
 if(aui->card_mixer_values[AU_MIXCHAN_TREBLE]!=MIXER_FUNCINFO_tone_treble.var_center)
  return 1;
 return 0;
}

static int tone_setvar(one_mixerfunc_info *infop,int currvalue,unsigned int setmode,int modvalue)
{
 int newvalue;
 switch(setmode){
  case MIXER_SETMODE_RELATIVE:newvalue=currvalue+modvalue*infop->var_step;
                              if((currvalue<infop->var_center && newvalue>infop->var_center) || (currvalue>infop->var_center && newvalue<infop->var_center))
                               newvalue=infop->var_center;
                              break;
  case MIXER_SETMODE_ABSOLUTE:newvalue=modvalue;break;
  case MIXER_SETMODE_RESET   :newvalue=infop->var_center;break;
 }
 if(newvalue<infop->var_min)
  newvalue=infop->var_min;
 else
  if(newvalue>infop->var_max)
   newvalue=infop->var_max;

 return newvalue;
}

static void mixer_tone_setvar_bass(struct mpxplay_audioout_info_s *aui,unsigned int setmode,int value)
{
 if(aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE){
  aui->card_mixer_values[AU_MIXCHAN_BASS]=tone_setvar(&MIXER_FUNCINFO_tone_bass,aui->card_mixer_values[AU_MIXCHAN_BASS],setmode,value);
  AU_setmixer_one(aui,AU_MIXCHAN_BASS,MIXER_SETMODE_ABSOLUTE,aui->card_mixer_values[AU_MIXCHAN_BASS]);
  return;
 }

 if(setmode==MIXER_SETMODE_RESET){
  if(aui->card_mixer_values[AU_MIXCHAN_BASS]==MIXER_FUNCINFO_tone_bass.var_center)
   return;
 }else
  mixer_tone_check_decoder_eq(aui);

 if(funcbit_test(aui->card_infobits,AUINFOS_CARDINFOBIT_PLAYING) && ((sync_flag==TONE_SYNCFLAG_BEGIN) || (sync_flag==TONE_SYNCFLAG_END)))
  return;

 aui->card_mixer_values[AU_MIXCHAN_BASS]=tone_setvar(&MIXER_FUNCINFO_tone_bass,aui->card_mixer_values[AU_MIXCHAN_BASS],setmode,value);

 calculate_eqgain(aui);
}

static void mixer_tone_setvar_treble(struct mpxplay_audioout_info_s *aui,unsigned int setmode,int value)
{
 if(aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE){
  aui->card_mixer_values[AU_MIXCHAN_TREBLE]=tone_setvar(&MIXER_FUNCINFO_tone_treble,aui->card_mixer_values[AU_MIXCHAN_TREBLE],setmode,value);
  AU_setmixer_one(aui,AU_MIXCHAN_TREBLE,MIXER_SETMODE_ABSOLUTE,aui->card_mixer_values[AU_MIXCHAN_TREBLE]);
  return;
 }

 if(setmode==MIXER_SETMODE_RESET){
  if(aui->card_mixer_values[AU_MIXCHAN_TREBLE]==MIXER_FUNCINFO_tone_treble.var_center)
   return;
 }else
  mixer_tone_check_decoder_eq(aui);

 if(funcbit_test(aui->card_infobits,AUINFOS_CARDINFOBIT_PLAYING) && ((sync_flag==TONE_SYNCFLAG_BEGIN) || (sync_flag==TONE_SYNCFLAG_END)))
  return;

 aui->card_mixer_values[AU_MIXCHAN_TREBLE]=tone_setvar(&MIXER_FUNCINFO_tone_treble,aui->card_mixer_values[AU_MIXCHAN_TREBLE],setmode,value);

 calculate_eqgain(aui);
}

one_mixerfunc_info MIXER_FUNCINFO_tone_bass={
 "MIX_TONE_BASS",
 "mxtb",
 NULL,
 MIXER_INFOBIT_PARALLEL_DEPENDENCY|MIXER_INFOBIT_EXTERNAL_DEPENDENCY, // loudness|newfile
 0,100,50,3,
 &mixer_tone_init,
 &mixer_tone_hq,
 &mixer_tone_hq,
 &mixer_tone_checkvar_bass,
 &mixer_tone_setvar_bass
};

one_mixerfunc_info MIXER_FUNCINFO_tone_treble={
 "MIX_TONE_TREBLE",
 "mxtt",
 NULL,
 MIXER_INFOBIT_PARALLEL_DEPENDENCY|MIXER_INFOBIT_EXTERNAL_DEPENDENCY, // loudness|newfile
 0,100,50,3,
 NULL,
 NULL,
 NULL,
 &mixer_tone_checkvar_treble,
 &mixer_tone_setvar_treble
};

//-------------------------------------------------------------------
#define MIXER_TONE_LOUDNESS_DEFAULT_VOL    230
#define MIXER_TONE_LOUDNESS_DEFAULT_SURR   120
#define MIXER_TONE_LOUDNESS_DEFAULT_BASS    70
#define MIXER_TONE_LOUDNESS_DEFAULT_TREBLE  75

static int loudness_enabled;

static int mixer_tone_checkvar_loudness(struct mpxplay_audioout_info_s *aui)
{
 if(!loudness_enabled){
  if( (aui->card_mixer_values[AU_MIXCHAN_BASS]>=MIXER_TONE_LOUDNESS_DEFAULT_BASS)
   && (aui->card_mixer_values[AU_MIXCHAN_TREBLE]>=MIXER_TONE_LOUDNESS_DEFAULT_TREBLE)
   //&& (MIXER_getvalue("MIX_VOLUME")>100)
   //&& (MIXER_getvalue("MIX_SURROUND")>100)
  ){
   loudness_enabled=1; // to correct flag after program restart (this flag is not saved in mpxplay.ini)
  }
 }
 return loudness_enabled;
}

static void mixer_tone_setvar_loudness(struct mpxplay_audioout_info_s *aui,unsigned int setmode,int value)
{
 mixer_tone_check_decoder_eq(aui);

 switch(setmode){
  case MIXER_SETMODE_RESET:
   if(!loudness_enabled)
    break;
  case MIXER_SETMODE_RELATIVE:
   mixer_tone_checkvar_loudness(aui);
   if(!loudness_enabled){
    MIXER_setfunction("MIX_VOLUME",MIXER_SETMODE_ABSOLUTE,MIXER_TONE_LOUDNESS_DEFAULT_VOL);
    MIXER_setfunction("MIX_SURROUND",MIXER_SETMODE_ABSOLUTE,MIXER_TONE_LOUDNESS_DEFAULT_SURR);
    aui->card_mixer_values[AU_MIXCHAN_BASS]=MIXER_TONE_LOUDNESS_DEFAULT_BASS;
    aui->card_mixer_values[AU_MIXCHAN_TREBLE]=MIXER_TONE_LOUDNESS_DEFAULT_TREBLE;
    loudness_enabled=1;
   }else{
    MIXER_setfunction("MIX_VOLUME",MIXER_SETMODE_RESET,0);
    MIXER_setfunction("MIX_SURROUND",MIXER_SETMODE_RESET,0);
    aui->card_mixer_values[AU_MIXCHAN_BASS]=MIXER_FUNCINFO_tone_bass.var_center;
    aui->card_mixer_values[AU_MIXCHAN_TREBLE]=MIXER_FUNCINFO_tone_treble.var_center;
    loudness_enabled=0;
   }
   if(aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE){
    AU_setmixer_one(aui,AU_MIXCHAN_BASS,MIXER_SETMODE_ABSOLUTE,aui->card_mixer_values[AU_MIXCHAN_BASS]);
    AU_setmixer_one(aui,AU_MIXCHAN_TREBLE,MIXER_SETMODE_ABSOLUTE,aui->card_mixer_values[AU_MIXCHAN_TREBLE]);
   }else{
    calculate_eqgain(aui);
   }
 }
}

one_mixerfunc_info MIXER_FUNCINFO_tone_loudness={
 "MIX_TONE_LOUDNESS",
 "mxtl",
 &loudness_enabled,
 MIXER_INFOBIT_SWITCH,
 0,1,0,0,
 NULL,
 NULL,
 NULL,
 &mixer_tone_checkvar_loudness,
 &mixer_tone_setvar_loudness
};
