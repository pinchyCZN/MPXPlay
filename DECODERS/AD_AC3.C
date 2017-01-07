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
//function: AC3 (A52) decoder handling
//requires the ad_ac3\a52dec.lib and a52.h files

#include "in_file.h"

#ifdef MPXPLAY_LINK_DECODER_AC3

#include "newfunc\newfunc.h"
#include "ad_ac3\a52.h"

#define A52_OUTCHANNELS    2
#define A52_CHANNELS_MAX   6  // 5+1

#define A52_HEADERSIZE          7
#define A52_FRAMESIZE_MIN      64
#define A52_FRAMESIZE_MAX    1920
#define A52_BLOCKS_PER_FRAME    6
#define A52_BLOCKSAMPLES      256

#define A52_BITSTREAM_BUFSIZE (A52_FRAMESIZE_MAX*2)

#define A52_SYNC_RETRY_BYTE   (A52_FRAMESIZE_MAX*32)
#define A52_SYNC_RETRY_FRAME  64

#define A52_GOODFRAME_LIMIT 2

typedef struct a52_decoder_data_s{
 mpxplay_bitstreambuf_s *bs;
 a52_state_t *a52state;
 int info_flags;
 int sample_rate;
 int bit_rate;

 int config_flags;
 level_t config_level;

 int lastframesize;

 unsigned int goodframe_count;
}a52_decoder_data_s;

static unsigned int a52_channums[11] = { 2, 1, 2, 3, 3, 4, 4, 5 ,1,1,2};

static int ad_a52_sync_frame(struct a52_decoder_data_s *a52i);

static int AD_A52_open(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct a52_decoder_data_s *a52i;

 a52i=calloc(1,sizeof(struct a52_decoder_data_s));
 if(!a52i)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 adi->private_data=a52i;

 a52i->a52state=a52_init(0);
 if(!a52i->a52state)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 a52i->bs=mpxplay_bitstream_alloc(A52_BITSTREAM_BUFSIZE);
 if(!a52i->bs)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 adi->outchannels=A52_OUTCHANNELS;
 adi->bits=16;  // output scale : +32767 ... -32768
 adi->infobits|=ADI_FLAG_FLOATOUT;

 if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
  adi->infobits|=ADI_FLAG_BITSTREAMOUT;
  if(adi->infobits&ADI_CNTRLBIT_BITSTREAMNOFRH)
   adi->infobits|=ADI_FLAG_BITSTREAMNOFRH;
 }

 a52i->config_flags=A52_DOLBY;//|A52_ADJUST_LEVEL;
 a52i->config_level=(level_t)70000.0; // ???

 return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_A52_close(struct mpxplay_audio_decoder_info_s *adi)
{
 struct a52_decoder_data_s *a52i=(struct a52_decoder_data_s *)adi->private_data;
 if(a52i){
  mpxplay_bitstream_free(a52i->bs);
  a52_free(a52i->a52state);
  if(adi->channeltext)
   free(adi->channeltext);
  free(a52i);
 }
}

static int ad_a52_parse_finalize(struct mpxplay_audio_decoder_info_s *adi)
{
 struct a52_decoder_data_s *a52i=(struct a52_decoder_data_s *)adi->private_data;

 adi->freq=a52i->sample_rate;
 adi->filechannels=a52_channums[a52i->info_flags&A52_CHANNEL_MASK];
 if(a52i->info_flags&A52_LFE)
  adi->filechannels++;

 adi->bitrate=a52i->bit_rate/1000;

 adi->channeltext=malloc(MPXPLAY_ADITEXTSIZE_CHANNEL+8);
 if(!adi->channeltext)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 sprintf(adi->channeltext,"%d.%d chan",a52_channums[a52i->info_flags&A52_CHANNEL_MASK],((a52i->info_flags&A52_LFE)? 1:0));

 return MPXPLAY_ERROR_INFILE_OK;
}

/*static int AD_A52_parse_extra(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct a52_decoder_data_s *a52i=(struct a52_decoder_data_s *)adi->private_data;
 int framesize;

 framesize=a52_syncinfo(spi->extradata,&a52i->info_flags,&a52i->sample_rate,&a52i->bit_rate);
 if(framesize<=0)
  return MPXPLAY_ERROR_INFILE_CANTOPEN;

 if(!spi->bs_framesize)         // ???
  spi->bs_framesize=framesize;

 return ad_a52_parse_finalize(adi);
}*/

static int AD_A52_parse_frame(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct a52_decoder_data_s *a52i=(struct a52_decoder_data_s *)adi->private_data;
 int retcode;

 spi->bs_usedbytes=mpxplay_bitstream_putbytes(a52i->bs,spi->bitstreambuf,spi->bs_leftbytes);

 retcode=ad_a52_sync_frame(a52i);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;

 if(!spi->bs_framesize)
  spi->bs_framesize=a52i->lastframesize;

 return ad_a52_parse_finalize(adi);
}

static int AD_A52_decode(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct a52_decoder_data_s *a52i=(struct a52_decoder_data_s *)adi->private_data;
 MPXPLAY_PCMOUT_FLOAT_T *pcmout=(MPXPLAY_PCMOUT_FLOAT_T *)adi->pcm_bufptr;
 sample_t *a52out;
 unsigned int i,ch,b;
 int retcode;

 if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
  spi->bs_usedbytes=mpxplay_bitstream_putbytes(a52i->bs,spi->bitstreambuf,spi->bs_leftbytes);
  retcode=ad_a52_sync_frame(a52i);
  if(retcode!=MPXPLAY_ERROR_INFILE_OK)
   return retcode;
  if(adi->infobits&ADI_CNTRLBIT_BITSTREAMNOFRH)
   mpxplay_bitstream_skipbytes(a52i->bs,2);
  mpxplay_bitstream_readbytes(a52i->bs,adi->pcm_bufptr,a52i->lastframesize);
  adi->pcm_samplenum=a52i->lastframesize;
  return MPXPLAY_ERROR_INFILE_OK;
 }

 if(a52i->lastframesize){
  mpxplay_bitstream_skipbytes(a52i->bs,a52i->lastframesize);
  a52i->lastframesize=0;
 }
 spi->bs_usedbytes=mpxplay_bitstream_putbytes(a52i->bs,spi->bitstreambuf,spi->bs_leftbytes);
 retcode=ad_a52_sync_frame(a52i);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;

 b=A52_BLOCKS_PER_FRAME;
 do{
  MPXPLAY_PCMOUT_FLOAT_T *pcmptr;
  if(a52_block(a52i->a52state)<0){
   a52i->goodframe_count=0;
   return MPXPLAY_ERROR_INFILE_RESYNC;
  }

  a52out=a52_samples(a52i->a52state);
  pcmptr=pcmout;

  for(ch=0;ch<A52_OUTCHANNELS;ch++){
   for(i=0;i<A52_BLOCKSAMPLES;i++){
    *pcmptr=(MPXPLAY_PCMOUT_FLOAT_T)(*a52out);
    pcmptr+=A52_OUTCHANNELS;
    a52out++;
   }
   pcmptr-=(A52_BLOCKSAMPLES*A52_OUTCHANNELS-1);
  }
  pcmout+=A52_BLOCKSAMPLES*A52_OUTCHANNELS;
  adi->pcm_samplenum+=A52_BLOCKSAMPLES*A52_OUTCHANNELS;
 }while(--b);

 return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_A52_clearbuff(struct mpxplay_audio_decoder_info_s *adi,unsigned int seektype)
{
 struct a52_decoder_data_s *a52i=(struct a52_decoder_data_s *)adi->private_data;
 mpxplay_bitstream_reset(a52i->bs);
 a52i->lastframesize=0;
 if(seektype&MPX_SEEKTYPE_BOF)
  a52i->goodframe_count=A52_GOODFRAME_LIMIT;
 else
  a52i->goodframe_count=0;
 if(seektype&(MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE))
  a52_reset(a52i->a52state);
}

//------------------------------------------------------------------------
static int ad_a52_sync_frame(struct a52_decoder_data_s *a52i)
{
 unsigned int retry_frame=A52_SYNC_RETRY_FRAME;
 unsigned int retry_byte=A52_SYNC_RETRY_BYTE;
 do{
  int framesize;
  if(mpxplay_bitstream_leftbytes(a52i->bs)<A52_HEADERSIZE)
   return MPXPLAY_ERROR_INFILE_NODATA;

  framesize=a52_syncinfo(mpxplay_bitstream_getbufpos(a52i->bs),&a52i->info_flags,&a52i->sample_rate,&a52i->bit_rate);
  if(framesize<=0){
   a52i->goodframe_count=0;
   mpxplay_bitstream_skipbytes(a52i->bs,1);
   if(!(--retry_byte))
    break;
   continue;
  }

  if(mpxplay_bitstream_leftbytes(a52i->bs)<framesize)
   return MPXPLAY_ERROR_INFILE_NODATA;

  if(a52_frame(a52i->a52state,mpxplay_bitstream_getbufpos(a52i->bs),&a52i->config_flags,&a52i->config_level,384,framesize)==0){
   a52i->goodframe_count++;
   if(a52i->goodframe_count>=A52_GOODFRAME_LIMIT){
    //a52_dynrng(a52i->a52state,NULL,NULL);
    a52i->lastframesize=framesize;
    return MPXPLAY_ERROR_INFILE_OK;
   }else{
    mpxplay_bitstream_skipbytes(a52i->bs,framesize);
    continue;
   }
  }

  a52i->goodframe_count=0;
  mpxplay_bitstream_skipbytes(a52i->bs,framesize); // skips frame on error

  if(!(--retry_frame))
   break;

 }while(1);

 return MPXPLAY_ERROR_INFILE_EOF;
}

struct mpxplay_audio_decoder_func_s AD_AC3_funcs={
 0,
 NULL,
 NULL,
 NULL,
 &AD_A52_open,
 &AD_A52_close,
 NULL,//&AD_A52_parse_extra,
 &AD_A52_parse_frame,
 &AD_A52_decode,
 &AD_A52_clearbuff,
 NULL,
 NULL,
 A52_FRAMESIZE_MAX,
 A52_BLOCKSAMPLES*A52_BLOCKS_PER_FRAME,
 {{MPXPLAY_WAVEID_AC3,"AC3"},{0,NULL}}
};

#endif //MPXPLAY_LINK_DECODER_AC3
