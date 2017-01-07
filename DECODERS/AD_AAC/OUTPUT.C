/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: output.c,v 1.22 2004/02/14 00:00:00 PDSoft Exp $
**/

#include "common.h"
#include "structs.h"
#include "output.h"
#include "decoder.h"

#define DM_MUL (1.0f/(1.0f+M_SQRT2))

static INLINE real_t get_sample(real_t **input, uint8_t channel,
                                uint16_t sample,uint8_t *internal_channel)
{
 if(channel == 0){
   return DM_MUL * (input[internal_channel[1]][sample] +
                input[internal_channel[0]][sample]/M_SQRT2 +
                input[internal_channel[3]][sample]/M_SQRT2 +
                input[internal_channel[5]][sample]);
 }else{
   return DM_MUL * (input[internal_channel[2]][sample] +
                input[internal_channel[0]][sample]/M_SQRT2 +
                input[internal_channel[4]][sample]/M_SQRT2 +
                input[internal_channel[5]][sample]);
 }
}

void output_to_PCM(faacDecHandle hDecoder,
                    real_t **input, void *sample_buffer, uint8_t output_channels,
                    uint16_t frame_len, uint8_t format)
{
 uint8_t *internal_channel=&hDecoder->internal_channel[0];
 float32_t *float_sample_buffer = (float32_t*)sample_buffer;
 uint8_t ch;

 if(hDecoder->downMatrix){
  for (ch = 0; ch < output_channels; ch++){
   float32_t *outchp=float_sample_buffer++;
   uint16_t i;
   for(i = 0; i < frame_len; i++){
    *outchp = (float32_t)get_sample(input,ch,i,internal_channel);
    outchp+=output_channels;
   }
  }
 }else{
  ch=output_channels;
  do{
   uint32_t chstep=output_channels;
   real_t *inchp=input[*internal_channel++];
   float32_t *outchp=float_sample_buffer++;
   uint16_t i=frame_len/4;
   do{
    *outchp = inchp[0];
    outchp+=chstep;
    *outchp = inchp[1];
    outchp+=chstep;
    *outchp = inchp[2];
    outchp+=chstep;
    *outchp = inchp[3];
    outchp+=chstep;
    inchp+=4;
   }while(--i);
  }while(--ch);
 }
}
