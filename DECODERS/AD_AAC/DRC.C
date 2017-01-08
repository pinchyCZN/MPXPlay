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
** $Id: drc.c,v 1.14 2003/09/21 00:00:00 PDSoft Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>
#include "syntax.h"
#include "drc.h"

drc_info *drc_init(real_t cut, real_t boost)
{
	drc_info *drc = (drc_info *) malloc(sizeof(drc_info));
	if(!drc)
		return drc;

	memset(drc, 0, sizeof(drc_info));

	drc->ctrl1 = cut;
	drc->ctrl2 = boost;

	drc->num_bands = 1;
	drc->band_top[0] = 1024 / 4 - 1;
	drc->dyn_rng_sgn[0] = 1;
	drc->dyn_rng_ctl[0] = 0;

	return drc;
}

void drc_end(drc_info * drc)
{
	if(drc)
		free(drc);
}

void drc_decode(drc_info * drc, real_t * spec)
{
	uint32_t i, bd, top;
	uint32_t bottom = 0;

	if(drc->num_bands == 1)
		drc->band_top[0] = 1024 / 4 - 1;

	for(bd = 0; bd < drc->num_bands; bd++) {
		real_t factor, exp;

		top = 4 * (drc->band_top[bd] + 1);

		/* Decode DRC gain factor */
		if(drc->dyn_rng_sgn[bd])	/* compress */
			exp = -drc->ctrl1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) / 24.0;
		else					/* boost */
			exp = drc->ctrl2 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) / 24.0;

		//factor = (real_t)pow(2.0, exp);
		factor = (real_t) pow2(exp);

		/* Apply gain factor */
		for(i = bottom; i < top; i++)
			spec[i] *= factor;

		bottom = top;
	}
}
