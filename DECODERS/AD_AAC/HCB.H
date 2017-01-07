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
** $Id: hcb.h,v 1.5 2003/07/29 08:20:14 menno Exp $
**/

#ifndef __HCB_H__
#define __HCB_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Optimal huffman decoding for AAC taken from:
 *  "SELECTING AN OPTIMAL HUFFMAN DECODER FOR AAC" by
 *  VLADIMIR Z. MESAROVIC , RAGHUNATH RAO, MIROSLAV V. DOKIC, and SACHIN DEO
 *  AES paper 5436
 *
 *   2 methods are used for huffman decoding:
 *   - binary search
 *   - 2-step table lookup
 *
 *   The choice of the "optimal" method is based on the fact that if the
 *   memory size for the Two-step is exorbitantly high then the decision
 *   is Binary search for that codebook. However, for marginally more memory
 *   size, if Twostep outperforms even the best case of Binary then the
 *   decision is Two-step for that codebook.
 *
 *   The following methods are used for the different tables.
 *   codebook   "optimal" method
 *    HCB_1      2-Step
 *    HCB_2      2-Step
 *    HCB_3      Binary
 *    HCB_4      2-Step
 *    HCB_5      Binary
 *    HCB_6      2-Step
 *    HCB_7      Binary
 *    HCB_8      2-Step
 *    HCB_9      Binary
 *    HCB_10     2-Step
 *    HCB_11     2-Step
 *    HCB_SF     Binary
 *
 */


#define ZERO_HCB       0
#define FIRST_PAIR_HCB 5
#define ESC_HCB        11
#define QUAD_LEN       4
#define PAIR_LEN       2
#define NOISE_HCB      13
#define INTENSITY_HCB2 14
#define INTENSITY_HCB  15

/* 1st step table */
typedef struct
{
    uint8_t offset;
    uint8_t extra_bits;
} hcb;

/* 2nd step table with quadruple data */
typedef struct
{
    uint8_t bits;
    int8_t x;
    int8_t y;
} hcb_2_pair;

typedef struct
{
    uint8_t bits;
    int8_t x;
    int8_t y;
    int8_t v;
    int8_t w;
} hcb_2_quad;

/* binary search table */
typedef struct
{
    uint8_t is_leaf;
    int8_t data[4];
} hcb_bin_quad;

typedef struct
{
    uint8_t is_leaf;
    int8_t data[2];
} hcb_bin_pair;

typedef struct huff_cbtable_s{
 hcb           *hcb_table;
 hcb_2_quad    *quad_table;
 uint32_t      quad_tab_size;
 hcb_2_pair    *pair_table;
 uint32_t      pair_tab_size;
 hcb_bin_pair  *bin_table;
 uint32_t      bin_tab_size;
 uint8_t       N;
 uint8_t       unsigned_cb;
}huff_cbtable_s;

extern huff_cbtable_s huff_cbtables[12];

extern hcb_bin_quad hcb3[];

#ifdef __cplusplus
}
#endif
#endif
