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
//function: MP2/MP3 file handling (with MP3 decoder parsing)

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_MPX

#include "in_rawau.h"
#include "tagging.h"
#include "newfunc\newfunc.h"
#include "decoders\ad_mp3\mp3dec.h"

#define XING_USE_TOC 1 // unuseful (low precision)

#define INMP3_READHEAD_DETECTSIZE 2048
#define INMP3_READHEAD_CHECKSIZE  262144

#define INMP3_BSBUF_SIZE      256
#ifdef XING_USE_TOC
 #define INMP3_XING_HEADSIZE   192
#else
 #define INMP3_XING_HEADSIZE   64
#endif
#define XING_FRAMES_FLAG 0x0001
#define XING_BYTES_FLAG  0x0002
#define XING_TOC_FLAG    0x0004
#define XING_NUMTOCENTRIES 100

typedef struct mp3_demux_data_s{
 struct mpxplay_bitstreambuf_s *bs;
 unsigned long databegin;  // ie: end of ID3v2
 unsigned long datalen;
 struct mp3_decoder_data mp3d;
#ifdef XING_USE_TOC
 unsigned long xing_totalbytes;
 mpxp_uint8_t xing_tocdata[XING_NUMTOCENTRIES];
#endif
}mp3_demux_data_s;

static unsigned long mpx_check_xingheader(struct mp3_demux_data_s *mp3i,unsigned char *headbufp)
{
 struct mp3_decoder_data *mp3d=&mp3i->mp3d;
 int xing_flags,offs;
 unsigned long total_frames=0;

 if(mpxdec_syncinfo(mp3d,headbufp)<0)
  return 0;

 if(!mp3d->lsf){ // mpeg1
  if(mp3d->mpg_chmode!=MPG_MD_MONO)
   offs=(32+4);
  else
   offs=(17+4);
 }else{          // mpeg2
  if(mp3d->mpg_chmode!=MPG_MD_MONO)
   offs=(17+4);
  else
   offs=(9+4);
 }

 headbufp+=offs;
 if( PDS_GETB_LE32(headbufp)==PDS_GET4C_LE32('X','i','n','g')
  // || PDS_GETB_LE32(headbufp)==PDS_GET4C_LE32('I','n','f','o') // file is not VBR -> we don't need the infos
 ){
  headbufp+=4;
  xing_flags=PDS_GETB_BE32(headbufp);headbufp+=4;
  if(xing_flags&XING_FRAMES_FLAG){
   total_frames=PDS_GETB_BE32(headbufp);headbufp+=4;
  }
#ifdef XING_USE_TOC
  if(xing_flags&XING_BYTES_FLAG){
   mp3i->xing_totalbytes=PDS_GETB_BE32(headbufp);headbufp+=4;
  }
  if(xing_flags&XING_TOC_FLAG)
   pds_memcpy(&mp3i->xing_tocdata[0],headbufp,XING_NUMTOCENTRIES);
  else
   mp3i->xing_totalbytes=0; // we use it for (to sign) TOC
#endif
 }else{
  headbufp-=offs;
  headbufp+=4+32;
  if(PDS_GETB_LE32(headbufp)==PDS_GET4C_LE32('V','B','R','I')){
   headbufp+=4;
   if(PDS_GETB_BE16(headbufp)==1){
    headbufp+=2+8; // skip tag version, delay, quality, total bytes
    total_frames=PDS_GETB_BE32(headbufp);
   }
  }
 }

 return total_frames;
}

static unsigned int inmpx_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis,unsigned long checksize)
{
 struct mp3_demux_data_s *mp3i;
 unsigned long id3v2size,allframes,retcode,samplenum_per_frame,bytes,i;
 mpxp_uint8_t *bufpos;

 retcode=INRAWAU_infile_open(fbfs,fbds,filename,miis);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;

 mp3i=(struct mp3_demux_data_s *)calloc(1,sizeof(mp3_demux_data_s));
 if(!mp3i)
  goto err_out_open;
 miis->private_data=mp3i;

 mp3i->bs=mpxplay_bitstream_alloc(INMP3_BSBUF_SIZE);
 if(!mp3i->bs)
  goto err_out_open;

 // skip zeroes at the begin of file (if there are)
 bytes=0;
 do{
  if(!(bytes&(INMP3_BSBUF_SIZE-1))){
   mpxplay_bitstream_reset(mp3i->bs);
   if(mpxplay_bitstream_fill(mp3i->bs,fbfs,fbds,INMP3_BSBUF_SIZE)!=MPXPLAY_ERROR_MPXINBUF_OK)
    goto err_out_open;
   bufpos=mpxplay_bitstream_getbufpos(mp3i->bs);
  }
  if(PDS_GETB_LE32(bufpos))
   break;
  bufpos+=4;bytes+=4;
 }while(bytes<checksize);

 if(bytes>checksize)
  goto err_out_open;

 for(i=0;i<3;i++){
  if(PDS_GETB_8U(bufpos))
   break;
  bufpos++;bytes++;
 }
 mp3i->databegin=bytes;
 bytes&=(INMP3_BSBUF_SIZE-1);
 mpxplay_bitstream_skipbytes(mp3i->bs,bytes);

 // get size of ID3V2 header and skip it
 if(mpxplay_bitstream_fill(mp3i->bs,fbfs,fbds,MPXPLAY_TAGGING_ID3V2_HEADSIZE)!=MPXPLAY_ERROR_MPXINBUF_OK)
  goto err_out_open;

 id3v2size=mpxplay_tagging_id3v2_totalsize(mpxplay_bitstream_getbufpos(mp3i->bs));
 if(id3v2size){
  if(id3v2size<=mpxplay_bitstream_leftbytes(mp3i->bs)){
   mpxplay_bitstream_skipbytes(mp3i->bs,id3v2size);
   mp3i->databegin+=id3v2size;
  }else
   if(fbfs->fseek(fbds,(mp3i->databegin+id3v2size),SEEK_SET)>0){ // id3v2size may be wrong
    mpxplay_bitstream_reset(mp3i->bs);
    mp3i->databegin+=id3v2size;
   }
 }

 mp3i->datalen=fbfs->filelength(fbds)-mp3i->databegin; // ??? - ID3v1 (128)

 // get xing-vbr header and calculate bitrate and length
 if(mpxplay_bitstream_fill(mp3i->bs,fbfs,fbds,INMP3_XING_HEADSIZE)!=MPXPLAY_ERROR_MPXINBUF_OK)
  goto err_out_open;

 allframes=mpx_check_xingheader(mp3i,mpxplay_bitstream_getbufpos(mp3i->bs));
 if(allframes && mp3i->mp3d.freq){ // VBR
  struct mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
  miis->allframes=allframes;
  //miis->audio_stream->bs_framesize=mp3i->datalen/allframes+1; // average (not good, if the allframes is wrong)
  miis->audio_stream->bs_framesize=MPXDEC_FRAMESIZE_MAX/2;
  samplenum_per_frame=mpxplay_infile_get_samplenum_per_frame(mp3i->mp3d.freq);
  miis->timemsec=(long)(1000.0*(float)(allframes*samplenum_per_frame)/(float)mp3i->mp3d.freq);
  if(miis->timemsec){
   adi->bitrate=(long)((float)mp3i->datalen*8.0/(float)miis->timemsec);
   adi->bitratetext=malloc(MPXPLAY_ADITEXTSIZE_BITRATE+4+8);
   if(adi->bitratetext){
    if(adi->bitrate<100)
     sprintf(adi->bitratetext," %2d kbit/s VBR",adi->bitrate);
    else
     sprintf(adi->bitratetext,"%-4dkbit/s VBR",adi->bitrate);
   }
  }
 }

 return MPXPLAY_ERROR_INFILE_OK;

err_out_open:
 return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static void INMP3_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis)
{
 struct mp3_demux_data_s *mp3i=miis->private_data;
 struct mpxplay_audio_decoder_info_s *adi=miis->audio_decoder_infos;
 if(mp3i){
  mpxplay_bitstream_free(mp3i->bs);
  free(mp3i);
  if(adi->bitratetext)
   free(adi->bitratetext);
 }
 INRAWAU_infile_close(fbfs,fbds,miis);
}

static long INMP3_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,long newframenum)
{
 struct mp3_demux_data_s *mp3i=miis->private_data;
 long newfilepos;
#ifdef XING_USE_TOC
 long toc_index,tocpos_low,tocpos_high,filepos_low,filepos_high,framepos_low,framepos_high;
 //char sout[100];
#endif

 if(!mp3i)
  return 0;

#ifdef XING_USE_TOC
 if(mp3i->xing_totalbytes && (newframenum<miis->allframes)){
  toc_index=(long)floor(100.0*(float)newframenum/(float)miis->allframes);
  if(toc_index>=XING_NUMTOCENTRIES)
   toc_index=XING_NUMTOCENTRIES-1;
  tocpos_low=(unsigned long)mp3i->xing_tocdata[toc_index];
  if((toc_index<1) || tocpos_low){
   pds_ftoi((float)tocpos_low/256.0*(float)mp3i->xing_totalbytes,&filepos_low);
   pds_ftoi((float)toc_index/100.0*(float)miis->allframes,&framepos_low);
   if((toc_index+1)<XING_NUMTOCENTRIES){
    tocpos_high=(unsigned long)mp3i->xing_tocdata[toc_index+1];
    pds_ftoi((float)tocpos_high/256.0*(float)mp3i->xing_totalbytes,&filepos_high);
    pds_ftoi((float)(toc_index+1)/100.0*(float)miis->allframes,&framepos_high);
   }else{
    filepos_high=mp3i->xing_totalbytes;
    framepos_high=miis->allframes;
   }
   pds_ftoi(((float)filepos_high-(float)filepos_low)*((float)newframenum-(float)framepos_low)/((float)framepos_high-(float)framepos_low),&newfilepos);
   //sprintf(sout,"%d %d %d %d",toc_index,filepos_low,filepos_high,newfilepos);
   //display_message(1,0,sout);
   newfilepos+=filepos_low;
   goto skip_newframenum;
  }
 }
#endif

 pds_ftoi((float)newframenum*(float)mp3i->datalen/(float)miis->allframes,&newfilepos);

#ifdef XING_USE_TOC
skip_newframenum:
#endif

 newfilepos+=mp3i->databegin;

 if(fbfs->fseek(fbds,newfilepos,SEEK_SET)<0)
  return MPXPLAY_ERROR_INFILE_EOF;

 return newframenum;
}

//------------------------------------------------------------------
static int INMP2_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 int retcode=inmpx_infile_open(fbfs,fbds,filename,miis,INMP3_READHEAD_CHECKSIZE);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;

 miis->audio_stream->wave_id=MPXPLAY_WAVEID_MP2;
 miis->longname="Layer II";

 return MPXPLAY_ERROR_INFILE_OK;
}

struct mpxplay_infile_func_s IN_MP2_funcs={
 (MPXPLAY_TAGTYPE_PUT_SUPPORT(MPXPLAY_TAGTYPE_ID3V1|MPXPLAY_TAGTYPE_ID3V2|MPXPLAY_TAGTYPE_APETAG)
 |MPXPLAY_TAGTYPE_PUT_PRIMARY(MPXPLAY_TAGTYPE_ID3V1)),
 NULL,
 NULL,
 NULL, // same way than INMP3_detect
 &INMP2_infile_open,
 &INMP2_infile_open,
 &INMP3_infile_close,
 &INRAWAU_infile_decode,
 &INMP3_infile_fseek,
 NULL,
 NULL,
 NULL,
 NULL,
 {"MP2",NULL}
};

static int inmp3_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis,unsigned long checksize)
{
 int retcode=inmpx_infile_open(fbfs,fbds,filename,miis,checksize);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;

 miis->audio_stream->wave_id=MPXPLAY_WAVEID_MP3;
 miis->longname="LayerIII";

 return MPXPLAY_ERROR_INFILE_OK;
}

static int INMP3_infile_detect(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 return inmp3_infile_open(fbfs,fbds,filename,miis,INMP3_READHEAD_DETECTSIZE);
}

static int INMP3_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 return inmp3_infile_open(fbfs,fbds,filename,miis,INMP3_READHEAD_CHECKSIZE);
}

struct mpxplay_infile_func_s IN_MP3_funcs={
 (MPXPLAY_TAGTYPE_PUT_SUPPORT(MPXPLAY_TAGTYPE_ID3V1|MPXPLAY_TAGTYPE_ID3V2|MPXPLAY_TAGTYPE_APETAG)
 |MPXPLAY_TAGTYPE_PUT_PRIMARY(MPXPLAY_TAGTYPE_ID3V1)),
 NULL,
 NULL,
 &INMP3_infile_detect,
 &INMP3_infile_open,
 &INMP3_infile_open,
 &INMP3_infile_close,
 &INRAWAU_infile_decode,
 &INMP3_infile_fseek,
 NULL,
 NULL,
 NULL,
 NULL,
 {"MP3",NULL}
};

#endif // MPXPLAY_LINK_INFILE_MPX
