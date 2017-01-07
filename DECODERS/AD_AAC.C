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
//function: AAC decoder handling
//requires ad_aac\aacdec.lib and faad.h

#include "in_file.h"

#ifdef MPXPLAY_LINK_DECODER_AAC

#include "newfunc\newfunc.h"
#include "ad_aac\faad.h"

#define AAC_MAX_CHANNELS   6          // 5.1
#define AAC_OUTCHANNELS    2
#define AAC_FRAMESIZE_MAX  (AAC_MAX_CHANNELS*FAAD_MIN_STREAMSIZE)
#define AAC_BITSTREAM_BUFSIZE (AAC_FRAMESIZE_MAX*2)
#define AAC_SAMPLES_PER_FRAME_MAX 2048 // ???

#define AAC_SPECTRUM_ANALISER 1

typedef struct aac_decoder_data_s{
 mpxplay_bitstreambuf_s *bs;

 faacDecHandle hDecoder;
 faacDecFrameInfo *frameInfo;

 unsigned long pcmsamples_per_aacframe;
}aac_decoder_data_s;

static faacDecHandle faaddec_init_dsi(unsigned char *bsbuffer,unsigned int bsbytes,faacDecFrameInfo *hInfo);
static faacDecHandle faaddec_init_frame(unsigned char *bsbuffer,unsigned int bsbytes,faacDecFrameInfo *hInfo,int bitstream_alloc_only);
static unsigned int MakeAdtsHeader(struct faacDecFrameInfo *frameInfo,unsigned char *data,unsigned int framesize);

static int AD_AAC_open(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct aac_decoder_data_s *aaci;

 aaci=(struct aac_decoder_data_s *)calloc(1,sizeof(struct aac_decoder_data_s));
 if(!aaci)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 adi->private_data=aaci;

 aaci->frameInfo=(faacDecFrameInfo *)calloc(1,sizeof(faacDecFrameInfo));
 if(!aaci->frameInfo)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 aaci->bs=mpxplay_bitstream_alloc(AAC_BITSTREAM_BUFSIZE);
 if(!aaci->bs)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 adi->bits=16; // scalebits in AAC decoder
 adi->outchannels=AAC_OUTCHANNELS;

 adi->infobits|=ADI_FLAG_FLOATOUT;
#ifdef AAC_SPECTRUM_ANALISER
 adi->infobits|=ADI_FLAG_OWN_SPECTANAL;
#endif

 if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
  adi->infobits|=ADI_FLAG_BITSTREAMOUT;
  if(adi->infobits&ADI_CNTRLBIT_BITSTREAMNOFRH)
   adi->infobits|=ADI_FLAG_BITSTREAMNOFRH;
 }

 return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_AAC_close(struct mpxplay_audio_decoder_info_s *adi)
{
 struct aac_decoder_data_s *aaci=(struct aac_decoder_data_s *)adi->private_data;
 if(aaci){
  mpxplay_bitstream_free(aaci->bs);
  if(aaci->frameInfo)
   free(aaci->frameInfo);
  if(aaci->hDecoder)
   faacDecClose(aaci->hDecoder);
  free(aaci);
 }
}

static void ad_aac_assign_values(struct mpxplay_audio_decoder_info_s *adi,faacDecFrameInfo *frameInfo)
{
 adi->filechannels=frameInfo->channels;
 if(adi->filechannels>2)
  adi->outchannels=2;
 else
  adi->outchannels=adi->filechannels;
 adi->freq=frameInfo->samplerate;
 adi->pcm_framelen=frameInfo->frameLength;
}

static int AD_AAC_parse_extra(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct aac_decoder_data_s *aaci=(struct aac_decoder_data_s *)adi->private_data;

 if(aaci->hDecoder)             // ??? should not happen
  faacDecClose(aaci->hDecoder);

 aaci->hDecoder=faaddec_init_dsi(spi->extradata,spi->extradata_size,aaci->frameInfo);
 if(!aaci->hDecoder)
  return MPXPLAY_ERROR_INFILE_CANTOPEN;

 /*if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
  faacDecClose(aaci->hDecoder);
  aaci->hDecoder=NULL;
 }*/

 ad_aac_assign_values(adi,aaci->frameInfo);

 return MPXPLAY_ERROR_INFILE_OK;
}

static int AD_AAC_parse_frame(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct aac_decoder_data_s *aaci=(struct aac_decoder_data_s *)adi->private_data;

 if(!aaci->hDecoder){
  aaci->hDecoder=faaddec_init_frame(spi->bitstreambuf,spi->bs_leftbytes,aaci->frameInfo,0);
  if(!aaci->hDecoder)
   return MPXPLAY_ERROR_INFILE_CANTOPEN;
 }

 spi->bs_usedbytes=mpxplay_bitstream_putbytes(aaci->bs,spi->bitstreambuf,spi->bs_leftbytes);

 faacDecReadframe(aaci->hDecoder,aaci->frameInfo,mpxplay_bitstream_getbufpos(aaci->bs),mpxplay_bitstream_leftbytes(aaci->bs));

 if(aaci->frameInfo->bytesconsumed>0){
  mpxplay_bitstream_skipbytes(aaci->bs,aaci->frameInfo->bytesconsumed);

  if((aaci->frameInfo->error==0) && aaci->frameInfo->samplerate && aaci->frameInfo->channels){
   ad_aac_assign_values(adi,aaci->frameInfo);
   return MPXPLAY_ERROR_INFILE_OK;
  }
 }else
  mpxplay_bitstream_reset(aaci->bs);

 return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int AD_AAC_decode(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct aac_decoder_data_s *aaci=(struct aac_decoder_data_s *)adi->private_data;
 void *sample_buffer;
 int retcode,leftbytes;
 faacDecFrameInfo frameInfo;

 spi->bs_usedbytes=mpxplay_bitstream_putbytes(aaci->bs,spi->bitstreambuf,spi->bs_leftbytes);

 leftbytes=mpxplay_bitstream_leftbytes(aaci->bs);
 if(leftbytes<=0)
  retcode=MPXPLAY_ERROR_INFILE_NODATA;
 else
  retcode=MPXPLAY_ERROR_INFILE_RESYNC;

 if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
  /*if(spi->bs_leftbytes){
   unsigned int headersize=0;
   if(!(adi->infobits&ADI_CNTRLBIT_BITSTREAMNOFRH))
    headersize=MakeAdtsHeader(aaci->frameInfo,adi->pcm_bufptr,spi->bs_leftbytes);
   pds_memcpy(adi->pcm_bufptr+headersize,spi->bitstreambuf,spi->bs_leftbytes);
   adi->pcm_samplenum=headersize+spi->bs_leftbytes;
   spi->bs_usedbytes=spi->bs_leftbytes;
  }*/

  //faacDecReadframe(aaci->hDecoder,&frameInfo,spi->bitstreambuf,spi->bs_leftbytes);
  sample_buffer=faacDecDecode(aaci->hDecoder,&frameInfo,mpxplay_bitstream_getbufpos(aaci->bs),leftbytes);
  if(frameInfo.bytesconsumed>0){
   mpxplay_bitstream_skipbytes(aaci->bs,frameInfo.bytesconsumed);
   if(sample_buffer && (frameInfo.error==0) && frameInfo.samplerate && frameInfo.channels){
    unsigned int headersize=0;
    if(!(adi->infobits&ADI_CNTRLBIT_BITSTREAMNOFRH))
     headersize=MakeAdtsHeader(aaci->frameInfo,adi->pcm_bufptr,frameInfo.bytesconsumed);
    pds_memcpy(adi->pcm_bufptr+headersize,spi->bitstreambuf,frameInfo.bytesconsumed);
    adi->pcm_samplenum=headersize+frameInfo.bytesconsumed;
    retcode=MPXPLAY_ERROR_INFILE_OK;
   }
  }else
   mpxplay_bitstream_reset(aaci->bs);
  return retcode;
 }

 sample_buffer = faacDecDecode(aaci->hDecoder,&frameInfo,mpxplay_bitstream_getbufpos(aaci->bs),leftbytes);
 if(frameInfo.bytesconsumed>0){
  mpxplay_bitstream_skipbytes(aaci->bs,frameInfo.bytesconsumed);
  if(sample_buffer && (frameInfo.error==0) && (frameInfo.bytesconsumed>0) && (frameInfo.samples>=adi->outchannels)){
   pds_memcpy(adi->pcm_bufptr,sample_buffer,frameInfo.samples*sizeof(MPXPLAY_PCMOUT_FLOAT_T));
   adi->pcm_samplenum=frameInfo.samples;
   retcode=MPXPLAY_ERROR_INFILE_OK;
  }
 }else
  mpxplay_bitstream_reset(aaci->bs);

 return retcode;
}

void AD_AAC_clearbuff(struct mpxplay_audio_decoder_info_s *adi,unsigned int seektype)
{
 struct aac_decoder_data_s *aaci=(struct aac_decoder_data_s *)adi->private_data;
 mpxplay_bitstream_reset(aaci->bs);
 if(seektype&(MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE)){
#ifdef AAC_SPECTRUM_ANALISER
  aac_analiser_clear();
#endif
 }
}

//------------------------------------------------------------------
static faacDecHandle faaddec_init_dsi(unsigned char *bsbuffer,unsigned int bsbytes,faacDecFrameInfo *hInfo)
{
 faacDecHandle hDecoder;
 faacDecConfigurationPtr config;

 hDecoder=faacDecOpen();
 if(!hDecoder)
  return hDecoder;

 config = faacDecGetCurrentConfiguration(hDecoder);
 config->outputFormat  = FAAD_FMT_FLOAT;
 config->downMatrix    = 1;

 if(faacDecInit_dsi(hDecoder,bsbuffer,bsbytes,hInfo)!=0)
  goto err_out_dsi;

 if(!faacDecInitFields(hDecoder))
  goto err_out_dsi;

 return hDecoder;

err_out_dsi:
 faacDecClose(hDecoder);
 return NULL;
}

//init the decoder using the first frame(s)
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

static unsigned int MakeAdtsHeader(struct faacDecFrameInfo *frameInfo,unsigned char *data,unsigned int framesize)
{
 int profile;

 profile = frameInfo->object_type;
 //if(profile)
 // profile--;
 profile&=3;

 pds_memset(data,0,7);

 framesize+=7;

 data[0]  = 0xFF; // 8b: syncword
 data[1]  = 0xF0; // 4b: syncword
                  // 1b: mpeg id = 0
                  // 2b: layer = 0
 data[1] |= 0x01; // 1b: protection absent

 data[2]  = ((profile << 6) & 0xC0);             // 2b: profile
 data[2] |= ((frameInfo->sf_index << 2) & 0x3C); // 4b: sampling_frequency_index
                                                 // 1b: private = 0
 data[2] |= ((frameInfo->channels >> 2) & 0x1);  // 1b: channel_configuration

 data[3]  = ((frameInfo->channels << 6) & 0xC0); // 2b: channel_configuration
                                       // 1b: original
                                       // 1b: home
                                       // 1b: copyright_id
                                       // 1b: copyright_id_start
 data[3] |= ((framesize >> 11) & 0x3); // 2b: aac_frame_length

 data[4]  = ((framesize >> 3) & 0xFF); // 8b: aac_frame_length

 data[5]  = ((framesize << 5) & 0xE0); // 3b: aac_frame_length
 data[5] |= ((0x7FF >> 6) & 0x1F);     // 5b: adts_buffer_fullness

 data[6]  = ((0x7FF << 2) & 0x3F);     // 6b: adts_buffer_fullness
                                       // 2b: num_raw_data_blocks
 return 7;
}

//----------------------------------------------------------------------

struct mpxplay_audio_decoder_func_s AD_AAC_funcs={
 0,
 NULL,
 NULL,
 NULL,
 &AD_AAC_open,
 &AD_AAC_close,
 &AD_AAC_parse_extra,
 &AD_AAC_parse_frame,
 &AD_AAC_decode,
 &AD_AAC_clearbuff,
 NULL,
 NULL,
 AAC_FRAMESIZE_MAX,
 AAC_SAMPLES_PER_FRAME_MAX,
 {{MPXPLAY_WAVEID_AAC,"AAC"},{0,NULL}}
};

#endif // MPXPLAY_LINK_DECODER_AAC
