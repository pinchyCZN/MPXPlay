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
//function: PCM (WAV) "decoding"

#include "in_file.h"
#include "newfunc\newfunc.h"
#include "au_mixer\mix_func.h"

typedef struct pcm_decoder_data_s{
 mpxplay_bitstreambuf_s *bs;
 unsigned int bytespersample;
}pcm_decoder_data_s;

static int AD_PCM_open(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct pcm_decoder_data_s *pcmi;

 if(!adi->bytespersample)
  return MPXPLAY_ERROR_INFILE_CANTOPEN;

 pcmi=(struct pcm_decoder_data_s *)calloc(1,sizeof(struct pcm_decoder_data_s));
 if(!pcmi)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 adi->private_data=pcmi;

 pcmi->bytespersample=adi->bytespersample;
 if((spi->wave_id==MPXPLAY_WAVEID_PCM_FLOAT) || (spi->wave_id==MPXPLAY_WAVEID_PCM_F32BE)){
  funcbit_enable(adi->infobits,ADI_FLAG_FLOATOUT);
  adi->bits=1; // sample scale bits
  adi->bitratetext="32-float";
  pcmi->bytespersample=sizeof(mpxp_float_t);
 }
 if((spi->wave_id==MPXPLAY_WAVEID_PCM_F64LE) || (spi->wave_id==MPXPLAY_WAVEID_PCM_F64BE)){
  funcbit_enable(adi->infobits,ADI_FLAG_FLOATOUT);
  adi->bits=1; // sample scale bits
  adi->bitratetext="64-float";
  pcmi->bytespersample=sizeof(mpxp_double_t);
 }

 if(!spi->block_align)
  spi->block_align=adi->outchannels*pcmi->bytespersample;

 pcmi->bs=mpxplay_bitstream_alloc(spi->block_align);
 if(!pcmi->bs)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 adi->pcm_framelen=spi->bs_framesize/adi->bytespersample/adi->outchannels;

 funcbit_disable(spi->flags,MPXPLAY_SPI_FLAG_NEED_PARSING); // !!!

 return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_PCM_close(struct mpxplay_audio_decoder_info_s *adi)
{
 struct pcm_decoder_data_s *pcmi=(struct pcm_decoder_data_s *)adi->private_data;
 if(pcmi){
  mpxplay_bitstream_free(pcmi->bs);
  free(pcmi);
 }
}

static int AD_PCM_decode(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct pcm_decoder_data_s *pcmi=(struct pcm_decoder_data_s *)adi->private_data;
 unsigned long savedbytes=mpxplay_bitstream_leftbytes(pcmi->bs);
 unsigned long newbytes=spi->bs_leftbytes;
 unsigned long allbytes=savedbytes+newbytes,i;

 if(allbytes<spi->block_align){
  spi->bs_usedbytes=mpxplay_bitstream_putbytes(pcmi->bs,spi->bitstreambuf,spi->bs_leftbytes);
  return MPXPLAY_ERROR_INFILE_NODATA;
 }

 if(savedbytes)
  mpxplay_bitstream_readbytes(pcmi->bs,adi->pcm_bufptr,savedbytes);

 if(newbytes){
  unsigned long savebytes;
  allbytes-=allbytes%spi->block_align;
  newbytes=allbytes-savedbytes;
  pds_memcpy(adi->pcm_bufptr+savedbytes,spi->bitstreambuf,newbytes);
  savebytes=spi->bs_leftbytes-newbytes;
  mpxplay_bitstream_putbytes(pcmi->bs,spi->bitstreambuf+newbytes,savebytes);
  spi->bs_usedbytes=spi->bs_leftbytes;
 }

 adi->pcm_samplenum=(savedbytes+newbytes)/pcmi->bytespersample;

 if((spi->wave_id==MPXPLAY_WAVEID_PCM_F32BE) || (spi->wave_id==MPXPLAY_WAVEID_PCM_F64BE) || ((spi->wave_id==MPXPLAY_WAVEID_PCM_SBE) && (adi->bits>8))){
  char *bufptr=adi->pcm_bufptr;
  for(i=0;i<adi->pcm_samplenum;i++,bufptr+=pcmi->bytespersample)
   pds_mem_reverse(bufptr,pcmi->bytespersample); // !!! slow
 }
 if((spi->wave_id==MPXPLAY_WAVEID_PCM_F64LE) || (spi->wave_id==MPXPLAY_WAVEID_PCM_F64BE))
  aumixer_cvbits_float64le_to_float32le((PCM_CV_TYPE_S *)adi->pcm_bufptr,adi->pcm_samplenum);

 return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_PCM_clearbuff(struct mpxplay_audio_decoder_info_s *adi,unsigned int seektype)
{
 struct pcm_decoder_data_s *pcmi=(struct pcm_decoder_data_s *)adi->private_data;
 mpxplay_bitstream_reset(pcmi->bs);
}

struct mpxplay_audio_decoder_func_s AD_PCM_funcs={
 0,
 NULL,
 NULL,
 NULL,
 &AD_PCM_open,
 &AD_PCM_close,
 NULL,
 NULL,
 &AD_PCM_decode,
 &AD_PCM_clearbuff,
 NULL,
 NULL,
 0,
 0,
 {{MPXPLAY_WAVEID_PCM_SLE,"PCM"},{MPXPLAY_WAVEID_PCM_SBE,"PCM"},
 {MPXPLAY_WAVEID_PCM_FLOAT,"PCM"},{MPXPLAY_WAVEID_PCM_F32BE,"PCM"},
 {MPXPLAY_WAVEID_PCM_F64LE,"PCM"},{MPXPLAY_WAVEID_PCM_F64BE,"PCM"},
 {0,NULL}}
};
