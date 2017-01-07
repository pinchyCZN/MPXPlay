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
//function: AAC file handling (direct connection to dec_aac)
//requires the dec_aac\aacdec.lib and faad.h files

//#define MPXPLAY_USE_DEBUGF 1
//#define MPXPLAY_DEBUG_OUTPUT stdout

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_AAC

#include "newfunc\newfunc.h"
#include "tagging.h"
#include "control\control.h"
#include "decoders\ad_aac\faad.h"
#include <string.h>

#define AAC_MAX_CHANNELS         6          // 5.1
#define AAC_BASE_FRAMENUM        20000      // a song is usually less
#define AAC_ADTS_READHEAD_FRAMES 40         // check (count) 40 frames only (not the whole file)
#define AAC_SEEKFRAME_INVALID    0xffffffff //
#define AAC_DECODE_RETRY         4          //

#define AAC_SPECTRUM_ANALISER 1

typedef struct aac_decoder_data{
 faacDecFrameInfo *frameInfo;
 faacDecHandle hDecoder;

 int  outchannels;
 long framecounter;

 unsigned char *bsbuffer;
 unsigned long bsbuf_size;

 int pcmoutsaved;
 AAC_PCMOUT_T *pcmoutdata;
 AAC_PCMOUT_T *pcmoutptr;

 long newframepos;
 faacDecHandle seek_hdecoder;
 unsigned char *seek_bsbuff;

 long seek_tabsize;
 long *seek_table;
 long seek_lastvalidframe;

 long bitrate;
 long timelength_ms;
 int  headertype;
}aac_decoder_data;

extern unsigned int intsoundconfig,intsoundcontrol;

static faacDecHandle faaddec_init_frame(unsigned char *bsbuffer,unsigned int bsbytes,faacDecFrameInfo *hInfo,int bitstream_alloc_only);
static unsigned long ADTS_seek(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci,long newaacframe);
static unsigned long RAW_AAC_seek(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci,long newaacframe);
static unsigned long RAW_AAC_check(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci,long newaacframe);
static unsigned int get_AAC_format(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci);

static int aac_assign_values(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci,struct mpxplay_infile_info_s *miis)
{
 struct mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 faacDecFrameInfo *frameInfo=aaci->frameInfo;

 miis->filesize=fbfs->filelength(fbds);
 adi->freq=frameInfo->samplerate;
 adi->filechannels=frameInfo->channels;
 if(adi->filechannels>2)
  adi->outchannels=2;
 else
  adi->outchannels=adi->filechannels;
 aaci->outchannels=adi->outchannels;
 adi->bits=16;
 adi->bitrate=aaci->bitrate/1000;
 miis->timemsec=aaci->timelength_ms;

 adi->infobits|=ADI_FLAG_FLOATOUT;
#ifdef AAC_SPECTRUM_ANALISER
 adi->infobits|=ADI_FLAG_OWN_SPECTANAL;
#endif

 miis->longname=malloc(MPXPLAY_ADITEXTSIZE_LONGNAME+8);
 if(!miis->longname)
  return 0;

 strcpy(miis->longname,(aaci->headertype==ADTS)? "ADTS":
                      (aaci->headertype==ADIF)? "ADIF":"RAW ");
 strcat(miis->longname,(frameInfo->sbr_present_flag)? "/HEA":"/AAC");

 /*strcpy(miis->longname,"AAC/");
 strcat(miis->longname,(frameInfo->object_type==MAIN)?  "MAIN":
                      (frameInfo->object_type==LC)?    "LC  ":
                      (frameInfo->object_type==SSR)?   "SSR ":
                      (frameInfo->object_type==LTP)?   "LTP ":
                      (frameInfo->object_type==ER_LC)? "ERLC":
                      (frameInfo->object_type==ER_LTP)?"ERLT":
                      (frameInfo->object_type==LD)?    "LD  ":
                      (frameInfo->object_type==DRM_ER_LC)? "DRM ":"????");*/

 adi->wave_id=MPXPLAY_WAVEID_AAC;
 adi->shortname="AAC";
 if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
  adi->infobits|=ADI_FLAG_BITSTREAMOUT;
  adi->pcm_framelen=frameInfo->frameLength;
 }

 return 1;
}

static int AAC_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 struct aac_decoder_data *aaci;

 if(!fbfs->fopen_read(fbds,filename,FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS))
  return MPXPLAY_ERROR_INFILE_FILEOPEN;

 aaci=(struct aac_decoder_data *)calloc(1,sizeof(struct aac_decoder_data));
 if(!aaci)
  goto err_out_check;
 miis->private_data=aaci;

 aaci->frameInfo=calloc(1,sizeof(faacDecFrameInfo));
 if(!aaci->frameInfo)
  goto err_out_check;

 aaci->bsbuf_size=FAAD_MIN_STREAMSIZE*AAC_MAX_CHANNELS;
 aaci->bsbuffer=(unsigned char *)malloc(aaci->bsbuf_size*sizeof(*aaci->bsbuffer));
 if(!aaci->bsbuffer)
  goto err_out_check;

 aaci->seek_bsbuff=(unsigned char *)malloc(aaci->bsbuf_size*sizeof(*aaci->seek_bsbuff));
 if(!aaci->seek_bsbuff)
  goto err_out_check;

 aaci->seek_table=(long *)calloc(AAC_BASE_FRAMENUM,sizeof(*aaci->seek_table));
 if(!aaci->seek_table)
  goto err_out_check;

 aaci->seek_tabsize=AAC_BASE_FRAMENUM;

 if(!get_AAC_format(fbfs,fbds,aaci))
  goto err_out_check;

 if(!aac_assign_values(fbfs,fbds,aaci,miis))
  goto err_out_check;

 return MPXPLAY_ERROR_INFILE_OK;

err_out_check:
 return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int AAC_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 struct aac_decoder_data *aaci;
 long bsbytes,retcode;

 retcode=AAC_infile_check(fbfs,fbds,filename,miis);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;
 aaci=miis->private_data;

 if(fbfs->fseek(fbds,aaci->seek_table[0],SEEK_SET)<0)
  goto err_out_open;

 bsbytes=fbfs->fread(fbds,aaci->bsbuffer,aaci->bsbuf_size);
 if(!bsbytes)
  goto err_out_open;

 aaci->hDecoder=faaddec_init_frame(aaci->bsbuffer,bsbytes,aaci->frameInfo,0);
 if(!aaci->hDecoder)
  goto err_out_open;

 aaci->pcmoutptr=aaci->pcmoutdata=(AAC_PCMOUT_T *)malloc(2*aaci->frameInfo->frameLength*aaci->outchannels*sizeof(AAC_PCMOUT_T));
 if(!aaci->pcmoutdata)
  goto err_out_open;

 aaci->newframepos=AAC_SEEKFRAME_INVALID;

 return MPXPLAY_ERROR_INFILE_OK;

err_out_open:
 return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static void AAC_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis)
{
 struct aac_decoder_data *aaci=miis->private_data;
 if(aaci){
  faacDecClose(aaci->hDecoder);
  faacDecClose(aaci->seek_hdecoder);
  if(aaci->frameInfo)
   free(aaci->frameInfo);
  if(aaci->bsbuffer)
   free(aaci->bsbuffer);
  if(aaci->seek_bsbuff)
   free(aaci->seek_bsbuff);
  if(aaci->seek_table)
   free(aaci->seek_table);
  if(aaci->pcmoutdata)
   free(aaci->pcmoutdata);
  free(aaci);
  if(miis->longname)
   free(miis->longname);
 }
 fbfs->fclose(fbds);
}

//-------------------------------------------------------------------------

static int AAC_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis)
{
 struct aac_decoder_data *aaci=miis->private_data;
 mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 void *sample_buffer;
 unsigned long samplenum_out=0,retry=0,pcmoutblocksize=adi->pcm_framelen*aaci->outchannels;
 MPXPLAY_PCMOUT_FLOAT_T *pcmoutp=(MPXPLAY_PCMOUT_FLOAT_T *)adi->pcm_bufptr;
 faacDecFrameInfo frameInfo;

 if(aaci->newframepos!=AAC_SEEKFRAME_INVALID){
  if(fbfs->fseek(fbds,aaci->seek_table[aaci->newframepos],SEEK_SET)<0)
   return MPXPLAY_ERROR_INFILE_NODATA;
  aaci->framecounter=aaci->newframepos;
  faacDecPostSeekReset(aaci->hDecoder,aaci->newframepos); // ???
  aaci->newframepos=AAC_SEEKFRAME_INVALID;
 }

 if(!(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT)){
  if(aaci->pcmoutsaved){
   samplenum_out=min(aaci->pcmoutsaved,pcmoutblocksize);
   memcpy(pcmoutp,aaci->pcmoutptr,samplenum_out*sizeof(AAC_PCMOUT_T));
   pcmoutp+=samplenum_out;
   aaci->pcmoutptr+=samplenum_out;
   aaci->pcmoutsaved-=samplenum_out;
  }
 }

 while(samplenum_out<pcmoutblocksize){
  long framebeginfilepos=fbfs->ftell(fbds);
  long bsbytes=fbfs->fread(fbds,aaci->bsbuffer,aaci->bsbuf_size);
  if(!bsbytes)
   break;

  memset(&frameInfo,0,sizeof(faacDecFrameInfo));

  if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
   faacDecReadframe(aaci->hDecoder,&frameInfo,aaci->bsbuffer,bsbytes);
   sample_buffer=aaci->bsbuffer;
   frameInfo.samples=frameInfo.bytesconsumed;
  }else
   sample_buffer = faacDecDecode(aaci->hDecoder,&frameInfo,aaci->bsbuffer,bsbytes);

  if(frameInfo.bytesconsumed){
   if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
    memcpy(adi->pcm_bufptr,aaci->bsbuffer,frameInfo.bytesconsumed);
    samplenum_out=frameInfo.bytesconsumed;
   }
   if((aaci->framecounter<aaci->seek_tabsize) && (aaci->framecounter>aaci->seek_lastvalidframe)){
    aaci->seek_table[aaci->framecounter]=framebeginfilepos;
    aaci->seek_lastvalidframe=aaci->framecounter;

    /*if(aaci->headertype!=ADIF){
     float timelen=(float)framecount*(float)frameInfo->frameLength/(float)(frameInfo->samplerate);
     float bitrate=(float)filepos*8.0/timelen;
     pds_ftoi(bitrate,&aaci->bitrate);
     pds_ftoi((float)filelen*8000.0/bitrate,&aaci->timelength_ms);
     adi->bitrate=aaci->bitrate/1000;
     miis->timemsec=aaci->timelength_ms;
    }*/
   }
   fbfs->fseek(fbds,framebeginfilepos+frameInfo.bytesconsumed,SEEK_SET);
   aaci->framecounter++;
  }else{
   if((aaci->framecounter+1)<=aaci->seek_lastvalidframe){
    if(fbfs->fseek(fbds,aaci->seek_table[aaci->framecounter+1],SEEK_SET)<0)
     break;
   }
  }

  if((frameInfo.error!=0) || (frameInfo.samples<1) || !sample_buffer){
   if(++retry>AAC_DECODE_RETRY)
    break;
  }else{
   if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
    break;
   }else{
    unsigned long thisblock=min(frameInfo.samples,pcmoutblocksize-samplenum_out);
    memcpy(pcmoutp,sample_buffer,thisblock*sizeof(AAC_PCMOUT_T));
    pcmoutp+=thisblock;
    samplenum_out+=thisblock;
    if(thisblock<frameInfo.samples){
     aaci->pcmoutsaved=frameInfo.samples-thisblock;
     memcpy(aaci->pcmoutdata,((AAC_PCMOUT_T *)sample_buffer)+thisblock,aaci->pcmoutsaved*sizeof(AAC_PCMOUT_T));
     aaci->pcmoutptr=aaci->pcmoutdata;
    }
   }
  }
 }
 adi->pcm_samplenum=samplenum_out;
 if(!samplenum_out)
  return MPXPLAY_ERROR_INFILE_NODATA;

 return MPXPLAY_ERROR_INFILE_OK;
}

//-------------------------------------------------------------------------

static void AAC_infile_clearbuffs(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,unsigned int seektype)
{
 struct aac_decoder_data *aaci=miis->private_data;
 if(aaci){
  if(seektype&(MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE)){
   aaci->pcmoutsaved=0;
#ifdef AAC_SPECTRUM_ANALISER
   aac_analiser_clear();
#endif
  }
 }
}

static long AAC_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,long newmpxframenum)
{
 struct aac_decoder_data *aaci=miis->private_data;
 mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 long newaacframe;

 pds_ftoi((float)newmpxframenum*(float)adi->pcm_framelen/(float)aaci->frameInfo->frameLength,&newaacframe);

 if(newaacframe<=aaci->seek_lastvalidframe){
  aaci->newframepos=newaacframe;
 }else{
  switch(aaci->headertype){
   case ADTS:aaci->newframepos=ADTS_seek(fbfs,fbds,aaci,newaacframe);
             if(!aaci->newframepos)
              return MPXPLAY_ERROR_INFILE_EOF;
             break;
   default  :aaci->newframepos=RAW_AAC_seek(fbfs,fbds,aaci,newaacframe);
             if(!aaci->newframepos)
              return MPXPLAY_ERROR_INFILE_EOF;
  }
  pds_ftoi((float)aaci->newframepos/(float)adi->pcm_framelen*(float)aaci->frameInfo->frameLength,&newmpxframenum);
 }

 if(fbfs->fseek(fbds,aaci->seek_table[aaci->newframepos],SEEK_SET)<0)
  return MPXPLAY_ERROR_INFILE_EOF;

 return newmpxframenum;
}

//------------------------------------------------------------------------
static faacDecHandle faaddec_init_frame(unsigned char *bsbuffer,unsigned int bsbytes,faacDecFrameInfo *hInfo,int bitstream_alloc_only)
{
 faacDecHandle hDecoder;
 faacDecConfigurationPtr config;
 faacDecFrameInfo frameInfo;

 hDecoder=faacDecOpen();
 if(!hDecoder)
  return hDecoder;

 config = faacDecGetCurrentConfiguration(hDecoder);
 config->defObjectType    = hInfo->object_type;
 config->defSBRpresentflag= hInfo->sbr_present_flag;
 config->defChannels      = hInfo->channels;
 if(hInfo->samplerate)
  config->defSampleRate   = hInfo->samplerate;

 config->outputFormat    = FAAD_FMT_FLOAT;
 config->downMatrix      = 1;

 if(faacDecInit_frame(hDecoder,bsbuffer,bsbytes,&frameInfo)!=0)
  goto err_out_initfr;

 memcpy(hInfo,(void *)&frameInfo,sizeof(faacDecFrameInfo));

 if(bitstream_alloc_only){
  if(!faacDecInitField_bs(hDecoder))
   goto err_out_initfr;
 }else{
  if(!faacDecInitFields(hDecoder))
   goto err_out_initfr;
 }

 return hDecoder;

err_out_initfr:
 faacDecClose(hDecoder);
 return NULL;
}

//------------------------------------------------------------------------

#define ADIF_MAX_SIZE 30

static int adts_sample_rates[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,7350,0,0,0};

static unsigned int ADIF_read_header(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci)
{
 faacDecFrameInfo *frameInfo=aaci->frameInfo;
 unsigned int bitstream,sf_idx,bitrate,object_type,sampling_rate;
 unsigned char buff[17],*bufp=&buff[0];

 if(fbfs->fread(fbds,bufp,17)!=17)
  return 0;

 if(bufp[0] & 0x80)
  bufp+=9;

 bitstream = bufp[0] & 0x10;
 bitrate = ((unsigned int)(bufp[0] & 0x0F)<<19)|
           ((unsigned int) bufp[1]<<11)|
           ((unsigned int) bufp[2]<<3)|
           ((unsigned int) bufp[3] & 0xE0);

 if(bitstream == 0){
  object_type = ((bufp[6]&0x01)<<1)|((bufp[7]&0x80)>>7);
  sf_idx = (bufp[7]&0x78)>>3;
 }else{
  object_type = (bufp[4] & 0x18)>>3;
  sf_idx = ((bufp[4] & 0x07)<<1)|((bufp[5] & 0x80)>>7);
 }
 sampling_rate=adts_sample_rates[sf_idx];
 frameInfo->object_type=object_type;
 frameInfo->samplerate=sampling_rate;

 //to get channels, sbr_present_flag, sampling_rate
 if(!RAW_AAC_check(fbfs,fbds,aaci,AAC_ADTS_READHEAD_FRAMES/2))
  return 0;

 aaci->headertype = ADIF;
 aaci->bitrate=bitrate;

 return 1;
}

static unsigned int ADTS_read_header(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci)
{
 faacDecFrameInfo *frameInfo=aaci->frameInfo;
 int object_type,sr_idx,channels;

 if(fbfs->fread(fbds,&aaci->seek_bsbuff[4],2)!=2)
  return 0;

 if(!((aaci->seek_bsbuff[0] == 0xFF)&&((aaci->seek_bsbuff[1] & 0xF6) == 0xF0)))
  return 0;

 object_type = (aaci->seek_bsbuff[2]&0xC0)>>6;
 sr_idx      = (aaci->seek_bsbuff[2]&0x3C)>>2;
 channels    = ((aaci->seek_bsbuff[2]&0x01)<<2)|((aaci->seek_bsbuff[3]&0xC0)>>6);
 frameInfo->object_type = object_type;
 frameInfo->samplerate  = adts_sample_rates[sr_idx];
 frameInfo->channels    = channels;

 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"o:%d c:%d si:%d sr:%d \n",object_type,channels,sr_idx,frameInfo->samplerate);

 //to get sbr_present_flag, sampling rate
 if(!RAW_AAC_check(fbfs,fbds,aaci,AAC_ADTS_READHEAD_FRAMES))
  return 0;

 //we don't need it for ADTS
 faacDecClose(aaci->seek_hdecoder);
 aaci->seek_hdecoder=NULL;

 aaci->headertype   = ADTS;
 frameInfo->channels= max(channels,frameInfo->channels);
 frameInfo->object_type  = object_type;

 return 1;
}

static unsigned long ADTS_seek(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci,long newaacframe)
{
 unsigned long framecount,beginframe,filepos,filelen,eof_flag;
 unsigned int  intsoundcntrl_save;
 unsigned char buff[8];

 filelen=fbfs->filelength(fbds);
 beginframe=framecount=aaci->seek_lastvalidframe;
 filepos=aaci->seek_table[framecount];
 if(fbfs->fseek(fbds,filepos,SEEK_SET)<0)
  return 0;
 eof_flag=0;

 do{
  if(fbfs->fread(fbds,&buff[0],6)!=6){
   eof_flag=1;
   break;
  }

  if(!((buff[0] == 0xFF)&&((buff[1] & 0xF6) == 0xF0))){
   eof_flag=2;
   break;
  }

  if(framecount>=aaci->seek_tabsize){
   MPXPLAY_INTSOUNDDECODER_DISALLOW;
   pds_ftoi((float)(aaci->seek_tabsize+2048)*(float)filelen/(float)filepos,&aaci->seek_tabsize);
   aaci->seek_table=realloc(aaci->seek_table,aaci->seek_tabsize*sizeof(*aaci->seek_table));
   MPXPLAY_INTSOUNDDECODER_ALLOW;
   if(!aaci->seek_table){
    aaci->seek_tabsize=0;
    aaci->seek_lastvalidframe=0;
    return 0;
   }
  }
  aaci->seek_table[framecount]=filepos;
  aaci->seek_lastvalidframe=framecount;

  if(framecount>=newaacframe)
   break;
  if(pds_look_extgetch()==KEY_ESC){
   pds_extgetch();
   break;
  }

  filepos+=((((unsigned long)buff[3] & 0x3)) << 11) // frame_len
          | (((unsigned long)buff[4]) << 3)
          | (((unsigned long)buff[5]) >> 5);
  if(fbfs->fseek(fbds,filepos,SEEK_SET)<0){
   eof_flag=3;
   break;
  }
  framecount++;
 }while(1);

 //hack
 if(eof_flag){
  if(framecount<(beginframe+100)) // seeking forward
   return 0;
  framecount-=50;                 // seeking rewind
 }

 return framecount;
}

static unsigned int rawaac_init_seek_hdecoder(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci)
{
 if(!aaci->seek_hdecoder){
  faacDecFrameInfo *frameInfo=aaci->frameInfo;
  long bsbytes;
  if(fbfs->fseek(fbds,aaci->seek_table[0],SEEK_SET)<0)
   return 0;
  bsbytes=fbfs->fread(fbds,aaci->seek_bsbuff,aaci->bsbuf_size);
  if(!bsbytes)
   return 0;
  aaci->seek_hdecoder=faaddec_init_frame(aaci->seek_bsbuff,bsbytes,aaci->frameInfo,1);
  if(!aaci->seek_hdecoder)
   return 0;
  aaci->seek_table[0]+=frameInfo->bytesconsumed;
 }
 return 1;
}

static unsigned long RAW_AAC_seek(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci,long newaacframe)
{
 long beginframe,framecount=0,filepos=0,eof_flag=0;
 long filelen=fbfs->filelength(fbds);
 unsigned int intsoundcntrl_save,channels;
 faacDecFrameInfo *frameInfo=aaci->frameInfo;

 if(!rawaac_init_seek_hdecoder(fbfs,fbds,aaci))
  return 0;

 framecount=aaci->seek_lastvalidframe;
 filepos=aaci->seek_table[framecount];
 if(fbfs->fseek(fbds,filepos,SEEK_SET)<0)
  return 0;
 faacDecPostSeekReset(aaci->seek_hdecoder,framecount);
 channels=0;
 beginframe=framecount;
 pds_memset(frameInfo,0,sizeof(*frameInfo));
 do{
  long bsbytes=fbfs->fread(fbds,aaci->seek_bsbuff,aaci->bsbuf_size);
  if(!bsbytes){
   eof_flag=1;
   break;
  }
  faacDecReadframe(aaci->seek_hdecoder,frameInfo,aaci->seek_bsbuff,bsbytes);
  if(!frameInfo->bytesconsumed){
   eof_flag=2;
   break;
  }
  if(frameInfo->error==0){
   if(framecount>=aaci->seek_tabsize){
    MPXPLAY_INTSOUNDDECODER_DISALLOW;
    pds_ftoi((float)(aaci->seek_tabsize+2048)*(float)filelen/(float)filepos,&aaci->seek_tabsize);
    aaci->seek_table=realloc(aaci->seek_table,aaci->seek_tabsize*sizeof(*aaci->seek_table));
    MPXPLAY_INTSOUNDDECODER_ALLOW;
    if(!aaci->seek_table){
     aaci->seek_tabsize=0;
     aaci->seek_lastvalidframe=0;
     return 0;
    }
   }
   aaci->seek_table[framecount]=filepos;
   aaci->seek_lastvalidframe=framecount;
  }
  if(framecount>=newaacframe)
   break;
  if(pds_look_extgetch()==KEY_ESC){
   pds_extgetch();
   break;
  }
  filepos+=frameInfo->bytesconsumed;
  if(fbfs->fseek(fbds,filepos,SEEK_SET)<0){
   eof_flag=3;
   break;
  }
  framecount++;
 }while(1);

 if(filepos && framecount && (frameInfo->error==0) && frameInfo->samplerate && channels && (aaci->seek_lastvalidframe==framecount)){
  float timelen=(float)framecount*(float)frameInfo->frameLength/(float)(frameInfo->samplerate);
  float bitrate=(float)filepos*8.0/timelen;
  pds_ftoi(bitrate,&aaci->bitrate);
  pds_ftoi((float)filelen*8000.0/bitrate,&aaci->timelength_ms);
  //mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"f:%d c:%d t:%d b:%d eof:%d\n",frameInfo->samplerate,channels,aaci->timelength_ms/1000,aaci->bitrate,eof_flag);
 }

 //hack
 if(eof_flag){
  if(framecount<(beginframe+100)) // seeking forward
   return 0;
  framecount-=50;                 // seeking rewind
 }

 return framecount;
}

static unsigned long RAW_AAC_check(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci,long framelimit)
{
 long framecount=0,filepos=0,eof_flag=0,goodframes=0;
 long filelen=fbfs->filelength(fbds);
 unsigned int channels=0;
 faacDecFrameInfo *frameInfo=aaci->frameInfo;

 if(!rawaac_init_seek_hdecoder(fbfs,fbds,aaci))
  return 0;

 filepos=aaci->seek_table[0];
 if(fbfs->fseek(fbds,filepos,SEEK_SET)<0)
  return 0;
 faacDecPostSeekReset(aaci->seek_hdecoder,0);
 pds_memset(frameInfo,0,sizeof(*frameInfo));
 do{
  long bsbytes=fbfs->fread(fbds,aaci->seek_bsbuff,aaci->bsbuf_size);
  if(!bsbytes){
   eof_flag=1;
   break;
  }
  faacDecReadframe(aaci->seek_hdecoder,frameInfo,aaci->seek_bsbuff,bsbytes);
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"bc:%d er:%d ch:%d sr:%d\n",frameInfo->bytesconsumed,frameInfo->error,frameInfo->channels,frameInfo->samplerate);
  if(!frameInfo->bytesconsumed){
   eof_flag=2;
   break;
  }
  if((frameInfo->error==0) && frameInfo->channels){
   channels=max(channels,frameInfo->channels);
   goodframes++;
  }else{
   channels=0;
   goodframes=0;
  }

  if(framecount>=framelimit)
   break;
  if(pds_look_extgetch()==KEY_ESC){
   pds_extgetch();
   break;
  }
  filepos+=frameInfo->bytesconsumed;
  if(fbfs->fseek(fbds,filepos,SEEK_SET)<0){
   eof_flag=3;
   break;
  }
  framecount++;
 }while(1);

 if(filepos && ((goodframes>1) || (goodframes && (eof_flag==1 || eof_flag==3))) && (frameInfo->error==0) && frameInfo->samplerate && channels){
  float timelen=(float)framecount*(float)frameInfo->frameLength/(float)(frameInfo->samplerate);
  float bitrate=(float)filepos*8.0/timelen;
  pds_ftoi(bitrate,&aaci->bitrate);
  pds_ftoi((float)filelen*8000.0/bitrate,&aaci->timelength_ms);
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"fr:%d ch:%d ti:%d br:%d eof:%d gfc:%d\n",frameInfo->samplerate,channels,aaci->timelength_ms/1000,aaci->bitrate,eof_flag,goodframes);
  return 1;
 }
 return 0;
}

static unsigned int get_AAC_format(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct aac_decoder_data *aaci)
{
 unsigned long tagsize,file_len;

 file_len = fbfs->filelength(fbds);
 if(file_len<ADIF_MAX_SIZE)
  return 0;

 if(fbfs->fread(fbds,aaci->seek_bsbuff,4)!=4)
  return 0;

 // skip ID3v2
 if(aaci->seek_bsbuff[0]=='I' && aaci->seek_bsbuff[1]=='D' && aaci->seek_bsbuff[2]=='3'){
  if(fbfs->fread(fbds,&aaci->seek_bsbuff[4],6)!=6)
   return 0;
  tagsize = (aaci->seek_bsbuff[6] << 21) | (aaci->seek_bsbuff[7] << 14) |
            (aaci->seek_bsbuff[8] <<  7) | (aaci->seek_bsbuff[9] <<  0);
  tagsize+=10;
  if(file_len<=(tagsize+4))
   return 0;
  if(fbfs->fseek(fbds,tagsize,SEEK_SET)<0)
   return 0;
  if(fbfs->fread(fbds,aaci->seek_bsbuff,4)!=4)
   return 0;
  aaci->seek_table[0]=tagsize;
 }else{
  tagsize=0;
 }

 file_len -= tagsize;

 if(aaci->seek_bsbuff[0]=='A' && aaci->seek_bsbuff[1]=='D' && aaci->seek_bsbuff[2]=='I' && aaci->seek_bsbuff[3]=='F'){
  if(!ADIF_read_header(fbfs,fbds,aaci))
   return 0;
  pds_ftoi((float)file_len*8000.0/((float)aaci->bitrate),&aaci->timelength_ms);
 }else{
  if((aaci->seek_bsbuff[0]==0xFF) && ((aaci->seek_bsbuff[1]&0xF6) == 0xF0)){
   if(!ADTS_read_header(fbfs,fbds,aaci))
    return 0;
  }else{
   if(!RAW_AAC_check(fbfs,fbds,aaci,AAC_ADTS_READHEAD_FRAMES))
    return 0;
   aaci->headertype  = RAW;
  }
 }

 //faacDecClose(aaci->seek_hdecoder);
 //aaci->seek_hdecoder=NULL;

 return 1;
}

struct mpxplay_infile_func_s IN_AAC_funcs={
 (MPXPLAY_TAGTYPE_PUT_SUPPORT(MPXPLAY_TAGTYPE_ID3V1|MPXPLAY_TAGTYPE_ID3V2|MPXPLAY_TAGTYPE_APETAG)
 |MPXPLAY_TAGTYPE_PUT_PRIMARY(MPXPLAY_TAGTYPE_APETAG)),
 NULL,
 NULL,
 NULL,//&AAC_infile_check, // !!! sometimes can crash (on bad files)
 &AAC_infile_check,
 &AAC_infile_open,
 &AAC_infile_close,
 &AAC_infile_decode,
 &AAC_infile_fseek,
 &AAC_infile_clearbuffs,
 NULL,
 NULL,
 NULL,
 {"AAC",NULL}
};

#endif // MPXPLAY_LINK_INFILE_AAC
