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
** $Id: ms_is.c,v 1.51 2003/09/21 00:00:00 PDSoft Exp $
**/

#include "common.h"
#include "structs.h"

#include "syntax.h"
#include "ms_is.h"
#include "pns.h"

void ms_decode(ic_stream * ics, ic_stream * icsr, real_t * l_spec, real_t * r_spec, uint16_t frame_len)
{
	uint32_t g, b, sfb;
	uint32_t group = 0;
	uint32_t nshort = frame_len / 8;
	uint16_t i;

	if(ics->ms_mask_present >= 1) {
		for(g = 0; g < ics->num_window_groups; g++) {
			for(b = 0; b < ics->window_group_length[g]; b++) {
				for(sfb = 0; sfb < ics->max_sfb; sfb++) {
					if((ics->ms_used[g][sfb] || ics->ms_mask_present == 2) && !is_intensity(icsr, g, sfb) && !is_noise(ics, g, sfb)) {
						for(i = ics->swb_offset[sfb]; i < ics->swb_offset[sfb + 1]; i++) {
							uint32_t k = group + i;
							double tmp = r_spec[k];
							r_spec[k] = l_spec[k] - tmp;
							l_spec[k] = l_spec[k] + tmp;
						}
					}
				}
				group += nshort;
			}
		}
	}
}

/*void ms_decode(ic_stream *ics, ic_stream *icsr, real_t *l_spec, real_t *r_spec,
               uint16_t frame_len)
{
 uint8_t g, b, sfb;
 uint8_t group = 0;
 uint16_t nshort = frame_len/8;

 uint16_t i, k;
 real_t tmp;

 if (ics->ms_mask_present >= 1){
  for (g = 0; g < ics->num_window_groups; g++){
   for (b = 0; b < ics->window_group_length[g]; b++){
    for(sfb = 0; sfb < ics->max_sfb; sfb++){
     if ((ics->ms_used[g][sfb] || ics->ms_mask_present == 2) &&
          !is_intensity(icsr, g, sfb) && !is_noise(ics, g, sfb)){
      for (i = ics->swb_offset[sfb]; i < ics->swb_offset[sfb+1]; i++){
       k = (group*nshort) + i;
       tmp = l_spec[k] - r_spec[k];
       l_spec[k] = l_spec[k] + r_spec[k];
       r_spec[k] = tmp;
      }
     }
    }
    group++;
   }
  }
 }
}*/

void is_decode(ic_stream * ics, ic_stream * icsr, real_t * l_spec, real_t * r_spec, uint16_t frame_len)
{
	uint8_t g, sfb, b;
	uint16_t i, k;
	real_t scale;

	uint16_t nshort = frame_len / 8;
	uint8_t group = 0;

	for(g = 0; g < icsr->num_window_groups; g++) {
		/* Do intensity stereo decoding */
		for(b = 0; b < icsr->window_group_length[g]; b++) {
			for(sfb = 0; sfb < icsr->max_sfb; sfb++) {
				if(is_intensity(icsr, g, sfb)) {
					/* For scalefactor bands coded in intensity stereo the
					   corresponding predictors in the right channel are
					   switched to "off".
					 */
					ics->pred.prediction_used[sfb] = 0;
					icsr->pred.prediction_used[sfb] = 0;

					scale = (real_t) pow2(-(0.25f * (real_t) icsr->scale_factors[g][sfb]));
					//scale = (real_t)pow(0.5, (0.25*icsr->scale_factors[g][sfb]));

					/* Scale from left to right channel,do not touch left channel */
					for(i = icsr->swb_offset[sfb]; i < icsr->swb_offset[sfb + 1]; i++) {
						k = (group * nshort) + i;

						r_spec[k] = MUL(l_spec[k], scale);

						if(is_intensity(icsr, g, sfb) != invert_intensity(ics, g, sfb))
							r_spec[k] = -r_spec[k];
					}
				}
			}
			group++;
		}
	}
}
