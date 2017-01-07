/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: single-block PCM synthesis
 last mod: $Id: synthesis.c,v 1.31 2007/02/12 00:00:00 PDSoft Exp $

 ********************************************************************/

#include <stdio.h>
#include "ogg.h"
#include "codec.h"
#include "codecint.h"
#include "registry.h"
#include "os.h"

int vorbis_synthesis(vorbis_block *vb,ogg_packet *op)
{
 vorbis_dsp_state     *vd=vb->vd;
 backend_lookup_state *b=vd->backend_state;
 vorbis_info          *vi=vd->vi;
 codec_setup_info     *ci=vi->codec_setup;
 oggpack_buffer       *opb=&vb->opb;
 int                   type,mode;

 oggpack_readinit(opb,op->packet,op->bytes);

 if(oggpack_read1(opb)!=0)
  return(OV_ENOTAUDIO);

 mode=oggpack_read32(opb,b->modebits);
 if((mode==-1) || (mode>=ci->modes))
  return(OV_EBADPACKET);

 vb->mode=mode;
 vb->W=ci->mode_param[mode]->blockflag;
 if(vb->W){
  vb->lW=oggpack_read1(opb);
  vb->nW=oggpack_read1(opb);
  if(vb->nW==-1)
   return(OV_EBADPACKET);
 }else{
  vb->lW=0;
  vb->nW=0;
 }

 vb->granulepos=op->granulepos;
 vb->sequence=op->packetno-3;
 vb->eofflag=op->e_o_s;

 vb->pcmend=ci->blocksizes[vb->W];

 type=ci->map_type[ci->mode_param[mode]->mapping];

 return(_mapping_P[type]->inverse(vb,ci->map_param[ci->mode_param[mode]->mapping]));
}


//------------------------------------------------------------------------
// registry

extern vorbis_func_floor     floor0_exportbundle;
extern vorbis_func_floor     floor1_exportbundle;
extern vorbis_func_residue   residue0_exportbundle;
extern vorbis_func_residue   residue1_exportbundle;
extern vorbis_func_residue   residue2_exportbundle;
extern vorbis_func_mapping   mapping0_exportbundle;

vorbis_func_floor     *_floor_P[]={
  &floor0_exportbundle,
  &floor1_exportbundle,
};

vorbis_func_residue   *_residue_P[]={
  &residue0_exportbundle,
  &residue1_exportbundle,
  &residue2_exportbundle,
};

vorbis_func_mapping   *_mapping_P[]={
  &mapping0_exportbundle,
};
