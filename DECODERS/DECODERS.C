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
//function: main decoder handling

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_OUTPUT stdout

#include "in_file.h"
#include "newfunc\newfunc.h"
#include "decoders.h"
#include "control\control.h"
#include "display\display.h"

#define MPXPLAY_DECODER_PARSING_RETRY_SINGLE      64 // ???
#define MPXPLAY_DECODER_PARSING_RETRY_CONTAINER 1024 // ???

static int  mpxplay_decoders_packet_alloc(struct mpxplay_streampacket_info_s *spi,unsigned int bsbuf_size);
static void mpxplay_decoders_packet_free(struct mpxplay_streampacket_info_s *spi);
static void mpxplay_decoders_packet_reset(struct mpxplay_streampacket_info_s *spi);
static void mpxplay_decoders_packet_clear(struct mpxplay_streampacket_info_s *spi);

static int  mpxplay_decoders_audio_alloc(struct mpxplay_audio_decoder_info_s *adi,struct frame *frp);
static void mpxplay_decoders_audio_free(struct frame *frp);
static void mpxplay_decoders_audio_close(struct mpxplay_infile_info_s *miis,struct frame *frp);
static void mpxplay_decoders_audio_reset(struct mpxplay_audio_decoder_info_s *adi,struct frame *frp);
static void mpxplay_decoders_audio_clear(struct mpxplay_audio_decoder_info_s *adi,struct frame *frp);
static void mpxplay_decoders_audio_setbytespersample(struct mpxplay_audio_decoder_info_s *adi);
static unsigned int mpxplay_decoders_audio_search_waveid(struct mpxplay_audio_decoder_func_s *adf,unsigned long wave_id,struct mpxplay_audio_decoder_info_s *adi);

#ifdef MPXPLAY_LINK_VIDEO
static void mpxplay_decoders_video_close(struct mpxplay_infile_info_s *miis,struct frame *frp);
static void mpxplay_decoders_video_clear(struct mpxplay_video_decoder_info_s *vdi);
#endif

extern unsigned int outmode,channelmode,playcontrol;
extern unsigned int displaymode,analtabnum;
extern unsigned long analtab[DISPLAY_ANALISER_MAXDELAY][DISPLAY_ANALISER_BANDNUM];

extern struct mpxplay_audio_decoder_func_s AD_AAC_funcs;
extern struct mpxplay_audio_decoder_func_s AD_AC3_funcs;
extern struct mpxplay_audio_decoder_func_s AD_ALAC_funcs;
extern struct mpxplay_audio_decoder_func_s AD_MP3_funcs;
extern struct mpxplay_audio_decoder_func_s AD_PCM_funcs;
extern struct mpxplay_audio_decoder_func_s AD_VORBIS_funcs;
extern struct mpxplay_audio_decoder_func_s AD_FFMPG_funcs;

static struct mpxplay_audio_decoder_func_s *audio_decoders[]={
#ifdef MPXPLAY_LINK_DECODER_AAC
 &AD_AAC_funcs,
#endif
#ifdef MPXPLAY_LINK_DECODER_AC3
 &AD_AC3_funcs,
#endif
#ifdef MPXPLAY_LINK_DECODER_ALAC
 &AD_ALAC_funcs,
#endif
#ifdef MPXPLAY_LINK_DECODER_MPX
 &AD_MP3_funcs,
#endif
 &AD_PCM_funcs,
#ifdef MPXPLAY_LINK_DECODER_VORBIS
 &AD_VORBIS_funcs,
#endif
#ifdef MPXPLAY_LINK_DECODER_FFMPG
 &AD_FFMPG_funcs,
#endif
 NULL
};

// search and open a decoder (at file open) (via wave_id or fourcc)
int mpxplay_decoders_open(struct mpxplay_infile_info_s *miis,struct frame *frp)
{
 struct mpxplay_audio_decoder_func_s **ads,*adf;
 struct mpxplay_audio_decoder_info_s *adi;
 struct mpxplay_streampacket_info_s *spi;
 int retcode=MPXPLAY_ERROR_INFILE_OK,opened=0,failed=0,retry;
 char sout[100];

 spi=miis->audio_stream;

 if(spi->flags&MPXPLAY_SPI_FLAG_NEED_DECODER){
  opened++;

  if(!spi->wave_id)
   goto adecoder_open_failed;

  adi=miis->audio_decoder_infos;

#ifdef MPXPLAY_LINK_DLLLOAD // !!! dll can override internal decoder
  adf=NULL;
  {
   mpxplay_module_entry_s *dll_decoder=NULL;
   do{
    dll_decoder=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_DECODER_AUDIO,0,NULL,dll_decoder);
    if(dll_decoder){
     if(dll_decoder->module_structure_version==MPXPLAY_DLLMODULEVER_DECODER_AUDIO){ // !!!
      if(mpxplay_decoders_audio_search_waveid((struct mpxplay_audio_decoder_func_s *)dll_decoder->module_callpoint,spi->wave_id,adi)){
       adf=(struct mpxplay_audio_decoder_func_s *)dll_decoder->module_callpoint;
       break;
      }
     }
    }
   }while(dll_decoder);
  }
  if(!adf)
#endif
  {
   ads=&audio_decoders[0];
   adf=*ads;
   do{
    if(mpxplay_decoders_audio_search_waveid(adf,spi->wave_id,adi))
     break;
    ads++;
    adf=*ads;
   }while(adf);
  }

  if(!adf)
   goto adecoder_open_failed;

  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"wid:%4.4X adf:%8.8X dt:%s",spi->wave_id,adf,adf->decodertypes[0].wave_name);

  miis->audio_decoder_funcs=adf;

  mpxplay_decoders_audio_setbytespersample(adi);

  if(adf->open){
   retcode=adf->open(adi,spi);
   if(retcode!=MPXPLAY_ERROR_INFILE_OK)
    goto adecoder_open_failed;
  }

  mpxplay_decoders_audio_setbytespersample(adi);

  if(spi->extradata && adf->parse_extra){
   retcode=adf->parse_extra(adi,spi);
   if(retcode==MPXPLAY_ERROR_INFILE_OK)
    funcbit_disable(spi->flags,MPXPLAY_SPI_FLAG_NEED_PARSING); // !!!
  }

  retcode=mpxplay_decoders_packet_alloc(spi,max(adf->maxbsframesize,spi->bs_framesize));
  if(retcode!=MPXPLAY_ERROR_INFILE_OK)
   goto adecoder_open_failed;

  if(spi->bs_framesize) // set by demuxer
   spi->bs_readsize=spi->bs_framesize;
  else
   if(adf->maxbsframesize) // decoder max
    spi->bs_readsize=adf->maxbsframesize;
   else
    spi->bs_readsize=MPXPLAY_SPI_MAXBSREADSIZE_AUDIO; // Mpxplay default max

  if(spi->flags&MPXPLAY_SPI_FLAG_NEED_PARSING){
   if(!adf->parse_frame)        // ???
    goto adecoder_open_succeed; //
   if(frp->infile_funcs->fseek)
    frp->infile_funcs->fseek(frp->filebuf_funcs,frp,miis,0);
   if(frp->infile_funcs->seek_postprocess)
    frp->infile_funcs->seek_postprocess(frp->filebuf_funcs,frp,miis,MPX_SEEKTYPE_BOF);

   retry=(spi->flags&MPXPLAY_SPI_FLAG_CONTAINER)? MPXPLAY_DECODER_PARSING_RETRY_CONTAINER:MPXPLAY_DECODER_PARSING_RETRY_SINGLE;
   do{
    int retval=frp->infile_funcs->decode(frp->filebuf_funcs,frp,miis);
    if((retval!=MPXPLAY_ERROR_INFILE_OK) && (retval!=MPXPLAY_ERROR_INFILE_SYNC_IN))
     goto adecoder_open_failed;
    mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"lb:%d %8.8X",spi->bs_leftbytes,(*(long *)spi->bitstreambuf));
    if(miis->audio_decoder_funcs->parse_frame(adi,spi)==MPXPLAY_ERROR_INFILE_OK)
     break;

    if(spi->bs_usedbytes){
     if(spi->bs_leftbytes>spi->bs_usedbytes)
      spi->bs_leftbytes-=spi->bs_usedbytes;
     else
      spi->bs_leftbytes=0;
     if(spi->bs_leftbytes)
      pds_memcpy(spi->bitstreambuf,spi->bitstreambuf+spi->bs_usedbytes,spi->bs_leftbytes);
     spi->bs_usedbytes=0;
    }
    if(pds_look_extgetch()==KEY_ESC){
     funcbit_enable(playcontrol,PLAYC_ABORTNEXT);
     goto adecoder_open_failed;
    }
   }while(--retry);

   if(!retry)
    goto adecoder_open_failed;
  }

adecoder_open_succeed:
  mpxplay_decoders_audio_search_waveid(adf,spi->wave_id,adi); // if the decoder modifies wave_id -> correct shortname
  mpxplay_decoders_audio_setbytespersample(adi);
  if(!adi->pcm_framelen)
   adi->pcm_framelen=adf->maxpcmblocksamplenum;
  if(!spi->bs_framesize)
   spi->bs_framesize=adf->maxbsframesize;
  spi->bs_readsize=spi->bs_framesize;
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"audio parsing succeed");
  goto adecoder_open_end;
adecoder_open_failed:
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"audio parsing failed");
  if(!funcbit_test(playcontrol,PLAYC_ABORTNEXT)){
   //if(adf)
   // sprintf(sout,"Can't open audio decoder: %s %d",((adi->shortname)? adi->shortname:adf->decodertypes[0].wave_name),spi->flags);
   //else
   if(!adf){
    if(miis->longname)
     sprintf(sout,"No audio decoder for %s (wid:%8.8X)!",miis->longname,spi->wave_id);
    else
     sprintf(sout,"No audio decoder for %s -> %8.8X !",frp->infile_funcs->file_extensions[0],spi->wave_id);
    display_timed_message(sout);
   }
  }
  mpxplay_decoders_audio_close(miis,frp);
  failed++;
 }
adecoder_open_end:

 //-------------------------------------------------------------------

#ifdef MPXPLAY_LINK_VIDEO
 spi=miis->video_stream;
 if(spi->flags&MPXPLAY_SPI_FLAG_NEED_DECODER){
  opened++;

  retcode=mpxplay_decoders_packet_alloc(spi,spi->bs_framesize);
  if(retcode!=MPXPLAY_ERROR_INFILE_OK)
   goto vdecoder_open_failed;

vdecoder_open_succeed:
  if(!spi->bs_framesize)
   spi->bs_framesize=adf->maxbsframesize;
  spi->bs_readsize=spi->bs_framesize;
  goto vdecoder_open_end;
vdecoder_open_failed:
  mpxplay_decoders_video_close(miis,frp);
  failed++;
 }
 vdecoder_open_end:
#endif

 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT,"opened:%d failed:%d",opened,failed);

 if(failed && (failed==opened)){
  if(retcode!=MPXPLAY_ERROR_INFILE_OK)
   return retcode;
  return MPXPLAY_ERROR_INFILE_CANTOPEN;
 }

 return MPXPLAY_ERROR_INFILE_OK;
}

// close the decoder (at the close of file)
void mpxplay_decoders_close(struct mpxplay_infile_info_s *miis,struct frame *frp)
{
 mpxplay_decoders_audio_close(miis,frp);
#ifdef MPXPLAY_LINK_VIDEO
 mpxplay_decoders_video_close(miis,frp);
#endif
}

int mpxplay_decoders_decode(struct mpxplay_infile_info_s *miis)
{
 int retcode=MPXPLAY_ERROR_INFILE_EOF;

 if(miis->audio_decoder_funcs){
  struct mpxplay_streampacket_info_s *spi=miis->audio_stream;
  struct mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;

  if(miis->audio_decoder_funcs->get_analiser_bands)
   if((displaymode&DISP_ANALISER) && (displaymode&DISP_NOFULLEDIT))
    miis->audio_decoder_funcs->get_analiser_bands(adi,DISPLAY_ANALISER_BANDNUM,&analtab[analtabnum][0]);
   else
    miis->audio_decoder_funcs->get_analiser_bands(adi,0,NULL);

  if(miis->audio_decoder_funcs->decode)
   retcode=miis->audio_decoder_funcs->decode(adi,spi);

  if(spi->bs_usedbytes){
   if(spi->bs_leftbytes>spi->bs_usedbytes)
    spi->bs_leftbytes-=spi->bs_usedbytes;
   else
    spi->bs_leftbytes=0;
   if(spi->bs_leftbytes)
    pds_memcpy(spi->bitstreambuf,spi->bitstreambuf+spi->bs_usedbytes,spi->bs_leftbytes);
   spi->bs_usedbytes=0;
  }
 }

 return retcode;
}

void mpxplay_decoders_clearbuf(struct mpxplay_infile_info_s *miis,struct frame *frp,unsigned int seektype)
{
 struct mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 if(miis->audio_decoder_funcs){
  if(miis->audio_decoder_funcs->clearbuf)
   miis->audio_decoder_funcs->clearbuf(miis->audio_decoder_infos,seektype);
  mpxplay_decoders_packet_reset(miis->audio_stream);
 }
 if(seektype&(MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE)){ // clear the whole buffer
  funcbit_smp_pointer_put(adi->pcm_bufptr,frp->pcmdec_buffer);
  funcbit_smp_value_put(frp->pcmdec_storedsamples,0);
  funcbit_smp_value_put(frp->pcmdec_leftsamples,0);
  funcbit_smp_value_put(frp->pcmout_storedsamples,0);
 }else{ // keep one block
  if(frp->pcmdec_leftsamples>frp->pcmout_blocksize)
   funcbit_smp_value_put(frp->pcmdec_leftsamples,frp->pcmout_blocksize);
 }
}

/*void mpxplay_decoders_setpos(struct mpxplay_infile_info_s *miis,long newmpxframenum)
{
 if(miis->audio_decoder_funcs){
  struct mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
  adi->pcmdatapos=(mpxp_int64_t)((float)newmpxframenum*(float)adi->pcmdatalen/(float)miis->allframes);
 }
 if(miis->video_decoder_funcs){
  struct mpxplay_video_decoder_info_s *vdi=miis->video_decoder_infos;
  vdi->video_framepos=(long)((float)newmpxframenum*(float)vdi->video_frames/(float)miis->allframes);
 }
}*/

// at program init
void mpxplay_decoders_preinit(void)
{
 struct mpxplay_audio_decoder_func_s **ads,*adf;

 ads=&audio_decoders[0];
 adf=*ads;

 do{
  if(adf->preinit)
   adf->preinit();
  ads++;
  adf=*ads;
 }while(adf);

#ifdef MPXPLAY_LINK_DLLLOAD
 {
  mpxplay_module_entry_s *dll_decoder=NULL;
  do{
   dll_decoder=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_DECODER_AUDIO,0,NULL,dll_decoder);
   if(dll_decoder){
    if(dll_decoder->module_structure_version==MPXPLAY_DLLMODULEVER_DECODER_AUDIO){ // !!!
     adf=(struct mpxplay_audio_decoder_func_s *)dll_decoder->module_callpoint;
     if(adf->preinit)
      adf->preinit();
    }
   }
  }while(dll_decoder);
 }
#endif
}

// at program close
void mpxplay_decoders_deinit(void)
{
 struct mpxplay_audio_decoder_func_s **ads,*adf;

 ads=&audio_decoders[0];
 adf=*ads;

 do{
  if(adf->deinit)
   adf->deinit();
  ads++;
  adf=*ads;
 }while(adf);

#ifdef MPXPLAY_LINK_DLLLOAD
 {
  mpxplay_module_entry_s *dll_decoder=NULL;
  do{
   dll_decoder=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_DECODER_AUDIO,0,NULL,dll_decoder);
   if(dll_decoder){
    if(dll_decoder->module_structure_version==MPXPLAY_DLLMODULEVER_DECODER_AUDIO){ // !!!
     adf=(struct mpxplay_audio_decoder_func_s *)dll_decoder->module_callpoint;
     if(adf->deinit)
      adf->deinit();
    }
   }
  }while(dll_decoder);
 }
#endif
}

// at program start and at every new file
int mpxplay_decoders_alloc(struct frame *frp,unsigned int full)
{
 struct mpxplay_infile_info_s *miis;
 int retcode;
 if(!frp)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 miis=frp->infile_infos;
 if(!miis)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 retcode=mpxplay_decoders_packet_alloc(miis->audio_stream,MPXPLAY_SPI_MAXBSREADSIZE_AUDIO);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;
#ifdef MPXPLAY_LINK_VIDEO
 retcode=mpxplay_decoders_packet_alloc(miis->video_stream,MPXPLAY_SPI_MAXBSREADSIZE_VIDEO);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;
#endif
 if(full){
  retcode=mpxplay_decoders_audio_alloc(miis->audio_decoder_infos,frp);
  if(retcode!=MPXPLAY_ERROR_INFILE_OK)
   return retcode;
 }
 return MPXPLAY_ERROR_INFILE_OK;
}

// at close of the program
void mpxplay_decoders_free(struct mpxplay_infile_info_s *miis,struct frame *frp)
{
 if(miis){
  mpxplay_decoders_packet_free(miis->audio_stream);
#ifdef MPXPLAY_LINK_VIDEO
  mpxplay_decoders_packet_free(miis->video_stream);
#endif
 }
 mpxplay_decoders_audio_free(frp);
}

// at crossfade + eof (pre file close)
void mpxplay_decoders_reset(struct mpxplay_infile_info_s *miis,struct frame *frp)
{
 if(!miis)
  return;
 mpxplay_decoders_audio_reset(miis->audio_decoder_infos,frp);
}

// at close of file
void mpxplay_decoders_clear(struct mpxplay_infile_info_s *miis,struct frame *frp)
{
 if(!miis)
  return;
 mpxplay_decoders_packet_clear(miis->audio_stream);
 mpxplay_decoders_audio_clear(miis->audio_decoder_infos,frp);
#ifdef MPXPLAY_LINK_VIDEO
 mpxplay_decoders_packet_clear(miis->video_stream);
#endif
}

//-------------------------------------------------------------------------
// at the program start and at every new file
static int mpxplay_decoders_packet_alloc(struct mpxplay_streampacket_info_s *spi,unsigned int max_framesize)
{
 if(!spi)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 if(max_framesize>spi->bs_bufsize){
  mpxp_uint8_t *newbuf=(mpxp_uint8_t *)malloc(max_framesize+32);
  if(!newbuf)
   return MPXPLAY_ERROR_INFILE_MEMORY;

  if(spi->bitstreambuf){
   pds_memcpy(newbuf,spi->bitstreambuf,spi->bs_bufsize);
   free(spi->bitstreambuf);
  }
  funcbit_smp_pointer_put(spi->bitstreambuf,newbuf);
  funcbit_smp_value_put(spi->bs_bufsize,max_framesize);
 }

 mpxplay_decoders_packet_reset(spi);

 return MPXPLAY_ERROR_INFILE_OK;
}

// at end of program only
static void mpxplay_decoders_packet_free(struct mpxplay_streampacket_info_s *spi)
{
 if(!spi)
  return;
 if(spi->bitstreambuf){
  free(spi->bitstreambuf);
  funcbit_smp_pointer_put(spi->bitstreambuf,NULL);
 }
 funcbit_smp_value_put(spi->bs_bufsize,0);
}

// at open of file and at seek
static void mpxplay_decoders_packet_reset(struct mpxplay_streampacket_info_s *spi)
{
 if(!spi)
  return;
 funcbit_smp_value_put(spi->bs_leftbytes,0);
 funcbit_smp_value_put(spi->bs_usedbytes,0);
}

// at close of file
static void mpxplay_decoders_packet_clear(struct mpxplay_streampacket_info_s *spi)
{
 if(!spi)
  return;
 funcbit_smp_value_put(spi->flags,0);
 funcbit_smp_value_put(spi->wave_id,0);
 funcbit_smp_value_put(spi->streamtype,0);
 funcbit_smp_value_put(spi->nb_streams,0);
 funcbit_smp_value_put(spi->bs_framesize,0);
 funcbit_smp_pointer_put(spi->extradata,NULL);
 funcbit_smp_value_put(spi->extradata_size,0);
 mpxplay_decoders_packet_reset(spi);
}

//-----------------------------------------------------------------------
static int mpxplay_decoders_audio_alloc(struct mpxplay_audio_decoder_info_s *adi,struct frame *frp)
{
 long newpcmbufsize;

 if(!adi)
  return MPXPLAY_ERROR_INFILE_MEMORY;

 newpcmbufsize =adi->pcm_framelen;
 newpcmbufsize+=mpxplay_infile_get_samplenum_per_frame(adi->freq); // frp->pcmout blocksamples
 newpcmbufsize*=adi->outchannels;
 newpcmbufsize*=sizeof(MPXPLAY_PCMOUT_FLOAT_T); // in bytes (!!! 4-bytes align to clear_pcmout)

 if(!newpcmbufsize)
  newpcmbufsize=PCM_BUFSIZE_DECODER;

 if(newpcmbufsize>frp->pcmdec_bufsize){
  frp->pcmdec_bufsize=newpcmbufsize;
  if(frp->pcmdec_buffer)
   pds_free(frp->pcmdec_buffer);
  frp->pcmdec_buffer=pds_malloc(frp->pcmdec_bufsize);
  if(!frp->pcmdec_buffer){
   frp->pcmdec_bufsize=0;
   return MPXPLAY_ERROR_INFILE_MEMORY;
  }
 }
 funcbit_smp_pointer_put(adi->pcm_bufptr,frp->pcmdec_buffer);
 funcbit_smp_value_put(frp->pcmdec_storedsamples,0);
 funcbit_smp_value_put(frp->pcmdec_leftsamples,0);
 if(adi->replaygain<0.0 || adi->replaygain>8192.0) // ???
  adi->replaygain=1.0;

 return MPXPLAY_ERROR_INFILE_OK;
}

static void mpxplay_decoders_audio_free(struct frame *frp)
{
 if(frp->pcmdec_buffer){
  free(frp->pcmdec_buffer);
  funcbit_smp_pointer_put(frp->pcmdec_buffer,NULL);
 }
 funcbit_smp_value_put(frp->pcmdec_bufsize,0);
}

static void mpxplay_decoders_audio_close(struct mpxplay_infile_info_s *miis,struct frame *frp)
{
 if(miis->audio_decoder_funcs){
  if(miis->audio_decoder_funcs->close)
   miis->audio_decoder_funcs->close(miis->audio_decoder_infos);
  funcbit_smp_pointer_put(miis->audio_decoder_funcs,NULL);
  funcbit_smp_pointer_put(miis->audio_decoder_infos->private_data,NULL);
 }
 mpxplay_decoders_packet_clear(miis->audio_stream);
 mpxplay_decoders_audio_clear(miis->audio_decoder_infos,frp);
}

static void clear_pcmout(struct frame *frp)
{
 int i,*p=(int *)frp->pcmdec_buffer;
 if(p)
  for(i=(frp->pcmdec_bufsize/sizeof(int));i;i--)
   funcbit_smp_value_put((*p++),0);
}

static void mpxplay_decoders_audio_reset(struct mpxplay_audio_decoder_info_s *adi,struct frame *frp)
{
 if(adi)
  funcbit_smp_pointer_put(adi->pcm_bufptr,frp->pcmdec_buffer);
 funcbit_smp_value_put(frp->pcmdec_storedsamples,0);
 funcbit_smp_value_put(frp->pcmdec_leftsamples,0);
 clear_pcmout(frp); // ???
}

static void mpxplay_decoders_audio_clear(struct mpxplay_audio_decoder_info_s *adi,struct frame *frp)
{
 mpxplay_decoders_audio_reset(adi,frp);
 if(!adi)
  return;
 funcbit_smp_pointer_put(adi->private_data,NULL);
 adi->infobits=0;
 if(outmode&OUTMODE_CONTROL_FILE_BITSTREAMOUT)
  funcbit_enable(adi->infobits,ADI_CNTRLBIT_BITSTREAMOUT);
 adi->wave_id=0;
 adi->freq=0;
 adi->filechannels=0;
 adi->outchannels=0;
 if(adi->chanmatrix){
  free(adi->chanmatrix);
  adi->chanmatrix=NULL;
 }
 adi->bits=0;
 adi->bytespersample=0;
 adi->bitrate=0;
 adi->pcm_framelen=0;
 adi->replaygain=1.0;
 adi->shortname=NULL;
 adi->bitratetext=NULL;
 adi->freqtext=NULL;
 adi->channeltext=NULL;
 adi->channelcfg=channelmode;
}

static void mpxplay_decoders_audio_setbytespersample(struct mpxplay_audio_decoder_info_s *adi)
{
 if(adi->infobits&ADI_FLAG_FLOATOUT)
  adi->bytespersample=sizeof(PCM_CV_TYPE_F);
 else
  //if(!adi->bytespersample)
  adi->bytespersample=(adi->bits+7) >> 3;
}

static unsigned int mpxplay_decoders_audio_search_waveid(struct mpxplay_audio_decoder_func_s *adf,unsigned long wave_id,struct mpxplay_audio_decoder_info_s *adi)
{
 mpxplay_audio_decoder_type_s *adt=&(adf->decodertypes[0]);
 do{
  if(adt->wave_id==wave_id){
   adi->wave_id=wave_id;
   adi->shortname=adt->wave_name;
   return 1;
  }
  adt++;
 }while(adt->wave_id || adt->wave_name);
 return 0;
}

unsigned int mpxplay_decoders_audio_eq_exists(struct frame *frp)
{
 if(frp){
  struct mpxplay_infile_info_s *miis=frp->infile_infos;
  if(miis && miis->audio_decoder_funcs && miis->audio_decoder_funcs->set_eq && miis->audio_decoder_infos)
   return 1;
 }
 return 0;
}

void mpxplay_decoders_audio_eq_config(struct frame *frp,unsigned int bandnum,unsigned long *band_freqs,float *band_powers)
{
 if(mpxplay_decoders_audio_eq_exists(frp)){
  struct mpxplay_infile_info_s *miis=frp->infile_infos;
  miis->audio_decoder_funcs->set_eq(miis->audio_decoder_infos,bandnum,band_freqs,band_powers);
 }
 /*if(frp){
  struct mpxplay_infile_info_s *miis=frp->infile_infos;
  if(miis && miis->audio_decoder_funcs && miis->audio_decoder_funcs->set_eq && miis->audio_decoder_infos)
   miis->audio_decoder_funcs->set_eq(miis->audio_decoder_infos,bandnum,band_freqs,band_powers);
 }*/
}

//-----------------------------------------------------------------------

#ifdef MPXPLAY_LINK_VIDEO

static void mpxplay_decoders_video_close(struct mpxplay_infile_info_s *miis,struct frame *frp)
{
 mpxplay_decoders_packet_clear(miis->video_stream);
 mpxplay_decoders_video_clear(miis->video_decoder_infos);
}

static void mpxplay_decoders_video_clear(struct mpxplay_video_decoder_info_s *vdi)
{
 if(!vdi)
  return;
 pds_memset(vdi,0,sizeof(struct mpxplay_video_decoder_info_s));
}
#endif
