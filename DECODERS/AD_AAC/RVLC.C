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
** $Id: rvlc.c,v 1.6 2003/09/20 00:00:00 PDSoft Exp $
**/

/* RVLC scalefactor decoding
 *
 * RVLC works like this:
 *  1. Only symmetric huffman codewords are used
 *  2. Total length of the scalefactor data is stored in the bitsream
 *  3. Scalefactors are DPCM coded
 *  4. Next to the starting value for DPCM the ending value is also stored
 *
 * With all this it is possible to read the scalefactor data from 2 sides.
 * If there is a bit error in the scalefactor data it is possible to start
 * decoding from the other end of the data, to find all but 1 scalefactor.
 */

#include "common.h"

#ifdef ERROR_RESILIENCE

#include "structs.h"
#include "syntax.h"
#include "bits.h"
#include "rvlc.h"

static uint8_t rvlc_decode_sf_forward(ic_stream *ics,
                                      bitfile *ld_sf,
                                      bitfile *ld_esc);
static int8_t rvlc_huffman_sf(bitfile *ld_sf, bitfile *ld_esc);
static int8_t rvlc_huffman_esc(bitfile *ld_esc);

//#define PRINT_RVLC

uint8_t rvlc_scale_factor_data(ic_stream *ics, bitfile *ld)
{
 uint8_t bits = 9;

 ics->sf_concealment = faad_get1bit(ld
        DEBUGVAR(1,149,"rvlc_scale_factor_data(): sf_concealment"));
 ics->rev_global_gain = faad_bits_read24(ld, 8
        DEBUGVAR(1,150,"rvlc_scale_factor_data(): rev_global_gain"));

 if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
  bits = 11;

 ics->length_of_rvlc_sf = faad_bits_read24(ld, bits
        DEBUGVAR(1,151,"rvlc_scale_factor_data(): length_of_rvlc_sf"));

 if (ics->noise_used){
  ics->dpcm_noise_nrg = faad_bits_read24(ld, 9
            DEBUGVAR(1,152,"rvlc_scale_factor_data(): dpcm_noise_nrg"));

  ics->length_of_rvlc_sf -= 9;
 }

 ics->sf_escapes_present = faad_get1bit(ld
        DEBUGVAR(1,153,"rvlc_scale_factor_data(): sf_escapes_present"));

 if (ics->sf_escapes_present){
  ics->length_of_rvlc_escapes = faad_bits_read24(ld, 8
            DEBUGVAR(1,154,"rvlc_scale_factor_data(): length_of_rvlc_escapes"));
 }

 if (ics->noise_used){
  ics->dpcm_noise_last_position = faad_bits_read24(ld, 9
            DEBUGVAR(1,155,"rvlc_scale_factor_data(): dpcm_noise_last_position"));
 }

 return 0;
}

uint8_t rvlc_decode_scale_factors(ic_stream *ics, bitfile *ld)
{
 bitfile ld_rvlc_sf, ld_rvlc_esc;

 if(ics->length_of_rvlc_sf > 0)
  memcpy(&ld_rvlc_sf,ld,sizeof(bitfile));

 if(ics->sf_escapes_present)
  memcpy(&ld_rvlc_esc,ld,sizeof(bitfile));

 return rvlc_decode_sf_forward(ics,&ld_rvlc_sf,&ld_rvlc_esc);
}

static uint8_t rvlc_decode_sf_forward(ic_stream *ics,bitfile *ld_sf,bitfile *ld_esc)
{
    int8_t g, sfb;
    int8_t t = 0;
    int8_t error = 0;
    int8_t noise_pcm_flag = 1;

    int16_t scale_factor = ics->global_gain;
    int16_t is_position = 0;
    int16_t noise_energy = ics->global_gain - 90 - 256;

#ifdef PRINT_RVLC
    printf("\nglobal_gain: %d\n", ics->global_gain);
#endif

    for (g = 0; g < ics->num_window_groups; g++)
    {
        for (sfb = 0; sfb < ics->max_sfb; sfb++)
        {
            if (error)
            {
                ics->scale_factors[g][sfb] = 0;
            } else {
                switch (ics->sfb_cb[g][sfb])
                {
                case ZERO_HCB: /* zero book */
                    ics->scale_factors[g][sfb] = 0;
                    break;
                case INTENSITY_HCB: /* intensity books */
                case INTENSITY_HCB2:

                    /* decode intensity position */
                    t = rvlc_huffman_sf(ld_sf, ld_esc);

                    is_position += t;
                    ics->scale_factors[g][sfb] = is_position;

                    break;
                case NOISE_HCB: /* noise books */

                    /* decode noise energy */
                    if (noise_pcm_flag)
                    {
                        int16_t n = ics->dpcm_noise_nrg;
                        noise_pcm_flag = 0;
                        noise_energy += n;
                    } else {
                        t = rvlc_huffman_sf(ld_sf, ld_esc);
                        noise_energy += t;
                    }

                    ics->scale_factors[g][sfb] = noise_energy;

                    break;
                default: /* spectral books */

                    /* decode scale factor */
                    t = rvlc_huffman_sf(ld_sf, ld_esc);

                    scale_factor += t;
                    if (scale_factor < 0)
                        return 4;

                    ics->scale_factors[g][sfb] = scale_factor;

                    break;
                }
#ifdef PRINT_RVLC
                printf("%3d:%4d%4d\n", sfb, ics->sfb_cb[g][sfb],
                    ics->scale_factors[g][sfb]);
#endif
                if (t == 99)
                {
                    error = 1;
                }
            }
        }
    }
#ifdef PRINT_RVLC
    printf("\n\n");
#endif

    return 0;
}


/* index == 99 means not allowed codeword */
static rvlc_huff_table book_rvlc[] = {
    /*index  length  codeword */
    {  0, 1,   0 }, /*         0 */
    { -1, 3,   5 }, /*       101 */
    {  1, 3,   7 }, /*       111 */
    { -2, 4,   9 }, /*      1001 */
    { -3, 5,  17 }, /*     10001 */
    {  2, 5,  27 }, /*     11011 */
    { -4, 6,  33 }, /*    100001 */
    { 99, 6,  50 }, /*    110010 */
    {  3, 6,  51 }, /*    110011 */
    { 99, 6,  52 }, /*    110100 */
    { -7, 7,  65 }, /*   1000001 */
    { 99, 7,  96 }, /*   1100000 */
    { 99, 7,  98 }, /*   1100010 */
    {  7, 7,  99 }, /*   1100011 */
    {  4, 7, 107 }, /*   1101011 */
    { -5, 8, 129 }, /*  10000001 */
    { 99, 8, 194 }, /*  11000010 */
    {  5, 8, 195 }, /*  11000011 */
    { 99, 8, 212 }, /*  11010100 */
    { 99, 9, 256 }, /* 100000000 */
    { -6, 9, 257 }, /* 100000001 */
    { 99, 9, 426 }, /* 110101010 */
    {  6, 9, 427 }, /* 110101011 */
    { 99, 10,  0 } /* Shouldn't come this far */
};

static rvlc_huff_table book_escape[] = {
    /*index  length  codeword */
    { 1, 2, 0 },
    { 0, 2, 2 },
    { 3, 3, 2 },
    { 2, 3, 6 },
    { 4, 4, 14 },
    { 7, 5, 13 },
    { 6, 5, 15 },
    { 5, 5, 31 },
    { 11, 6, 24 },
    { 10, 6, 25 },
    { 9, 6, 29 },
    { 8, 6, 61 },
    { 13, 7, 56  },
    { 12, 7, 120 },
    { 15, 8, 114 },
    { 14, 8, 242 },
    { 17, 9, 230 },
    { 16, 9, 486 },
    { 19, 10, 463  },
    { 18, 10, 974  },
    { 22, 11, 925  },
    { 20, 11, 1950 },
    { 21, 11, 1951 },
    { 23, 12, 1848 },
    { 25, 13, 3698 },
    { 24, 14, 7399 },
    { 26, 15, 14797 },
    { 49, 19, 236736 },
    { 50, 19, 236737 },
    { 51, 19, 236738 },
    { 52, 19, 236739 },
    { 53, 19, 236740 },
    { 27, 20, 473482 },
    { 28, 20, 473483 },
    { 29, 20, 473484 },
    { 30, 20, 473485 },
    { 31, 20, 473486 },
    { 32, 20, 473487 },
    { 33, 20, 473488 },
    { 34, 20, 473489 },
    { 35, 20, 473490 },
    { 36, 20, 473491 },
    { 37, 20, 473492 },
    { 38, 20, 473493 },
    { 39, 20, 473494 },
    { 40, 20, 473495 },
    { 41, 20, 473496 },
    { 42, 20, 473497 },
    { 43, 20, 473498 },
    { 44, 20, 473499 },
    { 45, 20, 473500 },
    { 46, 20, 473501 },
    { 47, 20, 473502 },
    { 48, 20, 473503 },
    { 99, 21,  0 } /* Shouldn't come this far */
};

static int8_t rvlc_huffman_sf(bitfile *ld_sf, bitfile *ld_esc)
{
 uint8_t i, j;
 int16_t index;
 uint32_t cw;
 rvlc_huff_table *h = book_rvlc;

 i = h->len;
 cw = faad_bits_read24(ld_sf, i DEBUGVAR(1,0,""));

 while ((cw != h->cw) && (i < 10)){
  h++;
  j = h->len-i;
  i += j;
  cw <<= j;
  cw |= faad_getbits(ld_sf, j DEBUGVAR(1,0,""));
 }

 index = h->index;

 if (index == +ESC_VAL){
  int8_t esc = rvlc_huffman_esc(ld_esc);
  if (esc == 99)
   return 99;
  index += esc;
#ifdef PRINT_RVLC
  printf("esc: %d - ", esc);
#endif
 }
 if (index == -ESC_VAL){
  int8_t esc = rvlc_huffman_esc(ld_esc);
  if (esc == 99)
   return 99;
  index -= esc;
#ifdef PRINT_RVLC
  printf("esc: %d - ", esc);
#endif
 }

 return index;
}

static int8_t rvlc_huffman_esc(bitfile *ld)
{
 uint8_t i, j;
 uint32_t cw;
 rvlc_huff_table *h = book_escape;

 i = h->len;
 cw = faad_bits_read24(ld, i DEBUGVAR(1,0,""));

 while ((cw != h->cw) && (i < 21)){
  h++;
  j = h->len-i;
  i += j;
  cw <<= j;
  cw |= faad_getbits(ld, j DEBUGVAR(1,0,""));
 }

 return h->index;
}

#endif // ERROR_RESILIENCE
