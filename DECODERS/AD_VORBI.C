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
//function: Vorbis audio handling
//requires the ad_vorbi\vorbis.lib file (and include files)

#include "in_file.h"

#ifdef MPXPLAY_LINK_DECODER_VORBIS

#include "newfunc\newfunc.h"
#include "ad_vorbi\codec.h"
#include "ad_vorbi\backends.h"
#include "ad_vorbi\codecint.h"

#define VORBIS_OUT_T float
#define VORBIS_BITSTREAM_BUFSIZE 32768
#define VORBIS_MAX_CHANNELS 6

typedef struct vorbis_decoder_data_s {
 unsigned int current_decoder_part;
 unsigned int parse_header_part;
 unsigned long pcmoutcount;

 ogg_packet       ops;

 vorbis_info      vis;
 vorbis_comment   vcs;
 vorbis_dsp_state vds;
 vorbis_block     vbs;

}vorbis_decoder_data_s;

/*static mpxp_uint8_t vorbis_mapping0_channel_matrix[VORBIS_MAX_CHANNELS-2][VORBIS_MAX_CHANNELS]={
//{MPXPLAY_ADICHANTYPE_FRONT_LEFT,0,0,0,0,0},
//{MPXPLAY_ADICHANTYPE_FRONT_LEFT,MPXPLAY_ADICHANTYPE_FRONT_RIGHT,0,0,0,0},
{MPXPLAY_ADICHANTYPE_FRONT_LEFT,MPXPLAY_ADICHANTYPE_FRONT_CENTER,MPXPLAY_ADICHANTYPE_FRONT_RIGHT,0,0,0},
{MPXPLAY_ADICHANTYPE_FRONT_LEFT,MPXPLAY_ADICHANTYPE_FRONT_RIGHT,MPXPLAY_ADICHANTYPE_REAR_LEFT,MPXPLAY_ADICHANTYPE_REAR_RIGHT,0,0},
{MPXPLAY_ADICHANTYPE_FRONT_LEFT,MPXPLAY_ADICHANTYPE_FRONT_CENTER,MPXPLAY_ADICHANTYPE_FRONT_RIGHT,MPXPLAY_ADICHANTYPE_REAR_LEFT,MPXPLAY_ADICHANTYPE_REAR_RIGHT,0},
{MPXPLAY_ADICHANTYPE_FRONT_LEFT,MPXPLAY_ADICHANTYPE_FRONT_CENTER,MPXPLAY_ADICHANTYPE_FRONT_RIGHT,MPXPLAY_ADICHANTYPE_REAR_LEFT,MPXPLAY_ADICHANTYPE_REAR_RIGHT,MPXPLAY_ADICHANTYPE_LFE}
};*/

static mpxp_uint8_t vorbis_mapping0_channel_matrix[VORBIS_MAX_CHANNELS-2][VORBIS_MAX_CHANNELS]={
//{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,  MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
//{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,  MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_CENTER,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,  MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,  MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_CENTER,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,  MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_CENTER,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,MPXPLAY_PCMOUTCHAN_LFE}
};

static int get_vorbis_outdata(struct vorbis_decoder_data_s *omip,VORBIS_OUT_T *pcm_outdata,unsigned int samplenum_request,int flushdata);

static int AD_VORBIS_open(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct vorbis_decoder_data_s *omip;
 ogg_packet *op;

 omip=(struct vorbis_decoder_data_s *)calloc(1,sizeof(struct vorbis_decoder_data_s));
 if(!omip)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 adi->private_data=omip;

 op=&omip->ops;
 op->b_o_s=1;
 op->packet=calloc(VORBIS_BITSTREAM_BUFSIZE,sizeof(*op->packet));
 if(!op->packet)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 vorbis_info_init(&(omip->vis));
 vorbis_comment_init(&(omip->vcs));

 adi->bits=16;
 funcbit_enable(adi->infobits,ADI_FLAG_FLOATOUT);

 return MPXPLAY_ERROR_INFILE_OK;
}

static int ad_vorbis_assign_values(struct vorbis_decoder_data_s *omip,mpxplay_audio_decoder_info_s *adi)
{
 vorbis_info *vi=&omip->vis;

 if(!vi->rate)
  return 0;
 if(!vi->channels)
  return 0;

 adi->freq=vi->rate;
 adi->filechannels=vi->channels;

 if(adi->channelcfg==CHM_LEFT)
  adi->outchannels=vi->outchannels=1;
 else
  adi->outchannels=vi->outchannels=vi->channels;

 if(adi->outchannels>VORBIS_MAX_CHANNELS)
  adi->outchannels=VORBIS_MAX_CHANNELS;

 if(adi->outchannels>PCM_MAX_CHANNELS)
  adi->outchannels=PCM_MAX_CHANNELS;

 if(adi->filechannels==2){
  codec_setup_info *ci=vi->codec_setup;
  if(ci){
   vorbis_info_mapping0 *info=ci->map_param[0];
   if(info){
    if(info->coupling_steps){
     adi->channeltext="c-Stereo";
    }
   }
  }
 }else if(adi->outchannels>2){
  adi->chanmatrix=calloc(VORBIS_MAX_CHANNELS,sizeof(*adi->chanmatrix));
  if(adi->chanmatrix)
   pds_memcpy(adi->chanmatrix,&vorbis_mapping0_channel_matrix[adi->outchannels-1-2][0],VORBIS_MAX_CHANNELS*sizeof(*adi->chanmatrix));
 }

 return 1;
}

static void ad_vorbis_spi_to_packet(ogg_packet *op,struct mpxplay_streampacket_info_s *spi)
{
 pds_memcpy(op->packet,spi->bitstreambuf,spi->bs_leftbytes);
 op->bytes=spi->bs_leftbytes;
 spi->bs_usedbytes=spi->bs_leftbytes; // ???
}

/*static void ad_vorbis_spi_to_packet(ogg_packet *op_dec,struct mpxplay_streampacket_info_s *spi)
{
 ogg_packet *op_demux=(ogg_packet *)spi->bitstreambuf;
 void *bsdata=spi->bitstreambuf+sizeof(*op_demux);
 unsigned long bsbytes=spi->bs_leftbytes-sizeof(*op_demux);
 pds_memcpy(op_dec->packet,bsdata,bsbytes);
 op_dec->bytes=bsbytes;
 op_dec->b_o_s=op_demux->b_o_s;
 op_dec->e_o_s=op_demux->e_o_s;
 op_dec->granulepos=op_demux->granulepos;
 op_dec->packetno=op_demux->packetno;
 spi->bs_usedbytes=spi->bs_leftbytes; // ???
}*/

static int AD_VORBIS_parse_frame(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct vorbis_decoder_data_s *omip=(struct vorbis_decoder_data_s *)adi->private_data;
 //char sout[100];

 ad_vorbis_spi_to_packet(&omip->ops,spi);
 //sprintf(sout,"vorbis: %d  %d",omip->parse_header_part,spi->bs_leftbytes);
 //pds_textdisplay_printf(sout);
 if(vorbis_synthesis_headerin(&(omip->vis),&(omip->vcs),&(omip->ops))<0)  // unpack_info
  return MPXPLAY_ERROR_INFILE_CANTOPEN;
 //pds_textdisplay_printf("vorbis: headerin ok");
 if((++omip->parse_header_part)<3)
  return MPXPLAY_ERROR_INFILE_NODATA;

 if(vorbis_synthesis_init(&(omip->vds),&(omip->vis))<0)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 if(vorbis_block_init(&(omip->vds),&(omip->vbs))<0)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 if(!ad_vorbis_assign_values(omip,adi))
  return MPXPLAY_ERROR_INFILE_CANTOPEN;

 omip->ops.b_o_s=0;
 //pds_textdisplay_printf("vorbis: parse ok");
 return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_VORBIS_close(struct mpxplay_audio_decoder_info_s *adi)
{
 struct vorbis_decoder_data_s *omip=adi->private_data;
 if(omip){
  vorbis_block_clear(&(omip->vbs));
  vorbis_dsp_clear(&(omip->vds));
  vorbis_info_clear(&(omip->vis));
  if(omip->ops.packet)
   free(omip->ops.packet);
  free(omip);
 }
}

static int AD_VORBIS_decode(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct vorbis_decoder_data_s *omip=adi->private_data;
 int eos=0;
 //char sout[100];

 do{
  //sprintf(sout,"vorbis: decode start dc:%d bs:%d",omip->current_decoder_part,spi->bs_leftbytes);
  //pds_textdisplay_printf(sout);
  switch(omip->current_decoder_part){
   case 0:if(spi->bs_leftbytes){
           if(spi->bs_usedbytes>=spi->bs_leftbytes){
            eos=1;
            break;
           }
           ad_vorbis_spi_to_packet(&omip->ops,spi);
	   omip->current_decoder_part=1;
	  }else{
           adi->pcm_samplenum=get_vorbis_outdata(omip,(VORBIS_OUT_T *)adi->pcm_bufptr,adi->pcm_framelen,1);
	   if(!adi->pcm_samplenum)
	    eos=1;
	   break;
	  }
   case 1:if(vorbis_synthesis(&(omip->vbs),&(omip->ops))==0){
           vorbis_synthesis_blockin(&(omip->vds),&(omip->vbs));
	   omip->current_decoder_part=2;
	  }else{
	   omip->current_decoder_part=0;
	   break;
	  }
   case 2:adi->pcm_samplenum=get_vorbis_outdata(omip,(VORBIS_OUT_T *)adi->pcm_bufptr,adi->pcm_framelen,0);
	  if(!adi->pcm_samplenum)
	   omip->current_decoder_part=0;
	  break;
  }
 }while(!eos && !adi->pcm_samplenum);
 //sprintf(sout,"vorbis: decode end eos:%d sn:%d",eos,adi->pcm_samplenum);
 //pds_textdisplay_printf(sout);
 if(eos)
  return MPXPLAY_ERROR_INFILE_NODATA;

 return MPXPLAY_ERROR_INFILE_OK;
}

void asm_vorbis_conv(int samples,ogg_double_t *chdata,float *pcmout);

static int get_vorbis_outdata(struct vorbis_decoder_data_s *omip,VORBIS_OUT_T *pcm_outdata,unsigned int samplenum_request,int flushdata)
{
 ogg_double_t **pcm;
 int channel,samples,vich=omip->vis.outchannels;
#ifdef OGG_USE_ASM
 int pcmout_step;
#endif
 //char sout[100];

 samples=vorbis_synthesis_pcmout(&(omip->vds),&pcm);
 //sprintf(sout,"vorbis: synth samp:%d pco:%d sr:%d fd:%d",samples,omip->pcmoutcount,samplenum_request,flushdata);
 //pds_textdisplay_printf(sout);

 if(samples){
  float *pcmout_begin;

  if(vich>PCM_MAX_CHANNELS)
   vich=PCM_MAX_CHANNELS;

#ifdef OGG_USE_ASM
  pcmout_step=vich*sizeof(float);
#endif

  if((omip->pcmoutcount+samples)>samplenum_request)
   samples=samplenum_request-omip->pcmoutcount;

  pcmout_begin=pcm_outdata+(omip->pcmoutcount*vich);
  channel=vich;
  do{
   ogg_double_t *pcmdec_data=*pcm++;
#if defined(OGG_USE_ASM) && !defined(OGGDEC_DOUBLE_PRECISION)
 #ifdef __WATCOMC__
  #pragma aux asm_vorbis_conv=\
   "mov ebx,4"\
   "mov ecx,pcmout_step"\
   "back1:mov eax,dword ptr [edi]"\
    "add edi,ebx"\
    "mov dword ptr [esi],eax"\
    "add esi,ecx"\
    "dec edx"\
   "jnz back1"\
   parm[edx][edi][esi] modify[eax ebx ecx edx edi esi];
   asm_vorbis_conv(samples,pcmdec_data,pcmout_begin);
 #endif // __WATCOMC__
#else // !OGG_USE_ASM || OGGDEC_DOUBLE_PRECISION
   unsigned int j=samples;
   float *pcmout_data=pcmout_begin;
   do{
    float val=*pcmdec_data++;
    *pcmout_data=val;
    pcmout_data+=vich;
   }while(--j);
#endif
   pcmout_begin++;
  }while(--channel);

  vorbis_synthesis_read(&(omip->vds),samples);
  omip->pcmoutcount+=samples;
 }
 samples=0;
 if(omip->pcmoutcount>=samplenum_request || flushdata){
  samples=omip->pcmoutcount*vich;
  omip->pcmoutcount=0;
 }
 return samples;
}

static void AD_VORBIS_clearbuff(struct mpxplay_audio_decoder_info_s *adi,unsigned int seektype)
{
 struct vorbis_decoder_data_s *omip=(struct vorbis_decoder_data_s *)adi->private_data;

 if(seektype&(MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE)){
  omip->current_decoder_part=0;
  vorbis_synthesis_restart(&(omip->vds),&(omip->vis));
  omip->pcmoutcount=0;
#ifdef OGG_SPECTRUM_ANALISER
  ogg_vorbis_analiser_clear();
#endif
 }
}

#ifdef OGG_SPECTRUM_ANALISER
static void AD_VORBIS_get_analiser_bands(struct mpxplay_audio_decoder_info_s *adi,unsigned int bandnum,unsigned long *banddataptr)
{
 ogg_vorbis_analiser_config(bandnum,banddataptr);
}
#endif

struct mpxplay_audio_decoder_func_s AD_VORBIS_funcs={
 0,
 NULL,
 NULL,
 NULL,
 &AD_VORBIS_open,
 &AD_VORBIS_close,
 NULL,
 &AD_VORBIS_parse_frame,
 &AD_VORBIS_decode,
 &AD_VORBIS_clearbuff,
#ifdef OGG_SPECTRUM_ANALISER
 &AD_VORBIS_get_analiser_bands,
#else
 NULL,
#endif
 NULL,
 32768,
 0,
 {{MPXPLAY_WAVEID_VORBIS,"VOR"},{0,NULL}}
};

#endif // MPXPLAY_LINK_DECODER_VORBIS
