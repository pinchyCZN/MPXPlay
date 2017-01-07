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
//function: proligic surround effect (for 2 channels only!)
// based on libModPlug v0.8 (http://www.xmms-libmod.com)
#ifndef WIN32
#include <mem.h>
#endif
#include <float.h>
#include "au_mixer\au_mixer.h"
#include "newfunc\newfunc.h"

//#define PDS_FINITE(x) _finite(x) // !!! Watcom (else?)

static int MIXER_var_surround;

//#define MIXER_SURROUND_PROLOGIC_LQ 1 // doesn't work proprely
#define MIXER_SURROUND_VERSION1      1

// !!! to avoud surround or vol-limiter fault, we have to limit the range of input (sux, check later)
// +12dB range (input sign may be out of MIXER_SCALE, we don't cut it here, limiter will correct it)
#define MIXER_SURROUND_RANGE_MIN (MIXER_SCALE_MIN*4)
#define MIXER_SURROUND_RANGE_MAX (MIXER_SCALE_MAX*4)

//#define nDolbyHiFltAttn		6
#define nDolbyHiFltMask		1
//#define DOLBYATTNROUNDUP        31

#define XBASSBUFFERSIZE		64		// 2 ms at 50KHz
#define FILTERBUFFERSIZE	64		// 1.25 ms
#define SURROUNDBUFFERSIZE	((PCM_MAX_FREQ * 100) / 1000)

typedef float surr_float_t; // sizeof(surr_float_t) must be match with sizeof(mpxp_int32_t)

static long nSurroundSize;
static long nSurroundPos;
static long nDolbyDepth;
static long nDolbyLoDlyPos;
static long nDolbyLoFltPos;
static surr_float_t nDolbyLoFltSum;
static long nDolbyHiFltPos;
static surr_float_t nDolbyHiFltSum;
static surr_float_t *DolbyLoFilterBuffer;
static surr_float_t *DolbyLoFilterDelay;
static surr_float_t *DolbyHiFilterBuffer;
static surr_float_t *SurroundBuffer;
//static surr_float_t prev_v;

//static unsigned int m_nProLogicDepth;
static unsigned int m_nProLogicDelay;
static unsigned int plsur_initialized;

static void mixer_plsurr_init(void)
{
 DolbyLoFilterBuffer=(surr_float_t *)malloc(XBASSBUFFERSIZE*sizeof(surr_float_t));
 if(!DolbyLoFilterBuffer) return;
 DolbyLoFilterDelay =(surr_float_t *)malloc(XBASSBUFFERSIZE*sizeof(surr_float_t));
 if(!DolbyLoFilterDelay)  return;
 DolbyHiFilterBuffer=(surr_float_t *)malloc(FILTERBUFFERSIZE*sizeof(surr_float_t));
 if(!DolbyHiFilterBuffer) return;
 SurroundBuffer     =(surr_float_t *)malloc(SURROUNDBUFFERSIZE*sizeof(surr_float_t));
 if(!SurroundBuffer)      return;
 plsur_initialized=1;
}

static void mixer_plsurr_close(void)
{
 if(DolbyLoFilterBuffer) free(DolbyLoFilterBuffer);
 if(DolbyLoFilterDelay)  free(DolbyLoFilterDelay);
 if(DolbyHiFilterBuffer) free(DolbyHiFilterBuffer);
 if(SurroundBuffer)      free(SurroundBuffer);
 plsur_initialized=0;
}

static void mixer_surr_reset(struct mpxplay_audioout_info_s *aui)
{
 if(!plsur_initialized)
  return;
 nSurroundPos = 0;
 nDolbyLoFltPos = 0;
 nDolbyLoFltSum = 0;
 nDolbyLoDlyPos = nDolbyHiFltPos = 0;
 nDolbyHiFltSum = 0;
 memset(DolbyLoFilterBuffer, 0, XBASSBUFFERSIZE*sizeof(surr_float_t));
 memset(DolbyLoFilterDelay,  0, XBASSBUFFERSIZE*sizeof(surr_float_t));
 memset(DolbyHiFilterBuffer, 0, FILTERBUFFERSIZE*sizeof(surr_float_t));
 memset(SurroundBuffer,      0, SURROUNDBUFFERSIZE*sizeof(surr_float_t));
 nSurroundSize = (aui->freq_song * m_nProLogicDelay) / 1000;
 if(nSurroundSize > SURROUNDBUFFERSIZE)
  nSurroundSize = SURROUNDBUFFERSIZE;
 //prev_v=0;
}

static int mixer_surround_init(struct mpxplay_audioout_info_s *aui,int inittype)
{
 switch(inittype){
  case MIXER_INITTYPE_INIT:
        mixer_plsurr_init();
        break;
  case MIXER_INITTYPE_REINIT:
        break;
  case MIXER_INITTYPE_START:
  case MIXER_INITTYPE_RESET:
  case MIXER_INITTYPE_LQHQSW:
        mixer_surr_reset(aui);
        break;
  case MIXER_INITTYPE_CLOSE:
        mixer_plsurr_close();
        break;
 }
 return 1;
}

static void mixer_surround_lq(struct mpxplay_audioout_info_s *aui)
{
 short *pcms=aui->pcm_sample;
 unsigned int i=aui->samplenum,step=aui->chan_card;

 i/=step;

#ifdef MIXER_SURROUND_PROLOGIC_LQ
 if(MIXER_var_surround<=100){
#endif
  do{
   int cadd=  (int)pcms[0]+(int)pcms[1];
   int csub=(((int)pcms[0]-(int)pcms[1])*MIXER_var_surround)/100;
   int c1;

   c1=(cadd+csub)/2;
   if(c1>MIXER_SCALE_MAX)
    c1=MIXER_SCALE_MAX;
   else
    if(c1<MIXER_SCALE_MIN)
     c1=MIXER_SCALE_MIN;
   pcms[0]=c1;

   c1=(cadd-csub)/2;
   if(c1>MIXER_SCALE_MAX)
    c1=MIXER_SCALE_MAX;
   else
    if(c1<MIXER_SCALE_MIN)
     c1=MIXER_SCALE_MIN;
   pcms[1]=c1;

   pcms+=step;
  }while(--i);
#ifdef MIXER_SURROUND_PROLOGIC_LQ
 }else{
  int n = nDolbyLoFltPos;
  do{
   //int v = ((int)pcms[0]+(int)pcms[1]+DOLBYATTNROUNDUP) >> (nDolbyHiFltAttn+1);
   int v=((int)pcms[0]+(int)pcms[1])*MIXER_var_surround/1000;
   int secho,tmp;
   //v *= (int)nDolbyDepth;
   // Low-Pass Filter
   *((mpxp_int32_t *)&nDolbyHiFltSum) -= *((mpxp_int32_t *)&DolbyHiFilterBuffer[nDolbyHiFltPos]);
   *((mpxp_int32_t *)&DolbyHiFilterBuffer[nDolbyHiFltPos]) = v;
   *((mpxp_int32_t *)&nDolbyHiFltSum) += v;
   v = *((mpxp_int32_t *)&nDolbyHiFltSum);
   nDolbyHiFltPos++;
   nDolbyHiFltPos &= nDolbyHiFltMask;
   // Surround
   secho = *((mpxp_int32_t *)&SurroundBuffer[nSurroundPos]);
   *((mpxp_int32_t *)&SurroundBuffer[nSurroundPos]) = v;
   // Delay line and remove low frequencies
   v = *((mpxp_int32_t *)&DolbyLoFilterDelay[nDolbyLoDlyPos]); // v = delayed signal
   *((mpxp_int32_t *)&DolbyLoFilterDelay[nDolbyLoDlyPos]) = secho;	       // secho = signal
   nDolbyLoDlyPos++;
   nDolbyLoDlyPos &= 0x1F;
   *((mpxp_int32_t *)&nDolbyLoFltSum) -= *((mpxp_int32_t *)&DolbyLoFilterBuffer[n]);
   tmp = secho / 64;
   *((mpxp_int32_t *)&DolbyLoFilterBuffer[n]) = tmp;
   *((mpxp_int32_t *)&nDolbyLoFltSum) += tmp;
   v -= *((mpxp_int32_t *)&nDolbyLoFltSum);
   n++;
   n &= 0x3F;
   // Add echo
   pcms[0] += v;
   pcms[1] -= v;
   if (++nSurroundPos >= nSurroundSize)
    nSurroundPos = 0;
   pcms += step;
  }while(--i);
  nDolbyLoFltPos = n;
 }
#endif
}

static void mixer_surround_hq(struct mpxplay_audioout_info_s *aui)
{
 PCM_CV_TYPE_F *pcm=(PCM_CV_TYPE_F *)aui->pcm_sample;
 unsigned int i=aui->samplenum,step=aui->chan_card;

 i/=step;
 if(MIXER_var_surround<=100){
  const float cfsur=(float)MIXER_var_surround/200;
  const float half=0.5;
  do{
   const PCM_CV_TYPE_F cadd=(pcm[0]+pcm[1])*half;
   const PCM_CV_TYPE_F csub=(pcm[0]-pcm[1])*cfsur;
   pcm[0]=(cadd+csub);
   pcm[1]=(cadd-csub);
   pcm+=step;
  }while(--i);
 }else{
  int n = nDolbyLoFltPos;
#ifdef MIXER_SURROUND_VERSION1
  const float cfsur=(float)MIXER_var_surround/400.0;
#else
  const float cfsur=(float)MIXER_var_surround/200.0;
#endif
  do{
   PCM_CV_TYPE_F v=pcm[0]+pcm[1];
   PCM_CV_TYPE_F secho,tmp;
   long c;
   //if(PDS_FINITE(v)){
    c=(long)v;
    if(c>MIXER_SURROUND_RANGE_MAX)
     v=(float)MIXER_SURROUND_RANGE_MAX;
    else if(c<MIXER_SURROUND_RANGE_MIN)
     v=(float)MIXER_SURROUND_RANGE_MIN;
    v*=cfsur;
   // prev_v=v;
   //}else
   // v=prev_v;

   // Low-Pass Filter
   nDolbyHiFltSum -= DolbyHiFilterBuffer[nDolbyHiFltPos];
   DolbyHiFilterBuffer[nDolbyHiFltPos] = v;
   nDolbyHiFltSum += v;
   v -= nDolbyHiFltSum/(nDolbyHiFltMask+1);
   //v = nDolbyHiFltSum;
   if((++nDolbyHiFltPos)>nDolbyHiFltMask)
    nDolbyHiFltPos=0;
   // Surround
   secho = SurroundBuffer[nSurroundPos];
   SurroundBuffer[nSurroundPos] = v;
   if (++nSurroundPos >= nSurroundSize)
    nSurroundPos = 0;
#ifdef MIXER_SURROUND_VERSION1
   // Delay line and remove low frequencies
   v = DolbyLoFilterDelay[nDolbyLoDlyPos];	// v = delayed signal
   DolbyLoFilterDelay[nDolbyLoDlyPos] = secho;	// secho = signal
   nDolbyLoDlyPos++;
   nDolbyLoDlyPos &= 0x1F;
   nDolbyLoFltSum -= DolbyLoFilterBuffer[n];
   tmp = secho / 64;
   DolbyLoFilterBuffer[n] = tmp;
   nDolbyLoFltSum += tmp;
   v -= nDolbyLoFltSum;
   n++;
   n &= 0x3F;
   // Add echo
   pcm[0] += v;
   pcm[1] -= v;
#else
   pcm[0] += secho;
   pcm[1] -= secho;
#endif
   pcm += step;
  }while(--i);
  nDolbyLoFltPos = n;
 }
}

static int mixer_surround_checkvar(struct mpxplay_audioout_info_s *aui)
{
 if((MIXER_var_surround!=100) && (aui->chan_card>=2)){
  if(MIXER_var_surround>100){
   unsigned int gain = ((MIXER_var_surround-100) * 10) / 100, nDelay=0;
   if (gain < 1) gain = 1;
   nDolbyDepth=gain;
   nDelay=((MIXER_var_surround-100) * 10) / 100 + 3;
   if (nDelay < 4) nDelay = 4;
   if (nDelay > 23) nDelay = 23;
   m_nProLogicDelay = nDelay;
   nSurroundSize = (aui->freq_song * m_nProLogicDelay) / 1000;
   if(nSurroundSize > SURROUNDBUFFERSIZE)
    nSurroundSize = SURROUNDBUFFERSIZE;
  }
  return 1;
 }
 return 0;
}

one_mixerfunc_info MIXER_FUNCINFO_surround={
 "MIX_SURROUND",
 "mxsr",
 &MIXER_var_surround,
 MIXER_INFOBIT_EXTERNAL_DEPENDENCY, // aui->chan_card
 10,500,100,10,
 &mixer_surround_init,
 &mixer_surround_lq,
 &mixer_surround_hq,
 &mixer_surround_checkvar,
 NULL
};
