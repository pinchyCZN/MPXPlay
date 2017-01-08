//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2006 by PDSoft (Attila Padar)                *
//*                    http://mpxplay.cjb.net                              *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Audio MPEG Layer II decoding
//based on the MPG123 MPEG decoder source (+updates from MPlayer)

#include <math.h>
#include <stdlib.h>
#include "mp3dec.h"

#ifdef USE_80_HYBRIDOUT
typedef double MP2_FLOAT_T;
#else
typedef float MP2_FLOAT_T;
#endif

static al_table alloc_0[] = {
	{4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511},
	{11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},
	{4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511},
	{11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},
	{4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511},
	{11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767}
};

static al_table alloc_1[] = {
	{4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511},
	{11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},
	{4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511},
	{11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},
	{4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511},
	{11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767},
	{2, 0}, {5, 3}, {7, 5}, {16, -32767}
};

static al_table alloc_2[] = {
	{4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255},
	{10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},
	{4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255},
	{10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}
};

static al_table alloc_3[] = {
	{4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255},
	{10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},
	{4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255},
	{10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}
};

static al_table alloc_4[] = {
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},
	{4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127},
	{9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9},
	{2, 0}, {5, 3}, {7, 5}, {10, 9}
};

static MP2_FLOAT_T muls[27][64];
static unsigned int grp_3tab[32 * 3];
static unsigned int grp_5tab[128 * 3];
static unsigned int grp_9tab[1024 * 3];

extern int mpxdec_bitindex;
extern unsigned char *mpxdec_wordpointer;
extern unsigned int MIXER_var_usehq;

void mpxdec_layer2_getstuff(struct mp3_decoder_data *mp3d)
{
	static unsigned char translate[9][2][16] = { {{0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 1, 1, 1, 1, 1, 0},	/*44.1 stereo */
												  {0, 2, 2, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}},	/*44.1 mono */
	{{0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/*48 stereo */
	 {0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},	/*48 mono */
	{{0, 3, 3, 3, 3, 3, 3, 0, 0, 0, 1, 1, 1, 1, 1, 0},	/*32 stereo */
	 {0, 3, 3, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}},	/*32 mono */
	{{2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0},	/*22.05 stereo */
	 {2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0}},	/*22.05 mono */
	{{2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0},	/*24 stereo */
	 {2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0}},	/*24 mono */
	{{2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0},	/*16 stereo */
	 {2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0}},	/*16 mono */
	{{2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0},	/*11.025 stereo */
	 {2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0}},	/*11.025 mono */
	{{2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0},	/*12 stereo */
	 {2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0}},	/*12 mono */
	{{2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0},	/*8 stereo */
	 {2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0}}	/*8 mono */
/*       0  48  64  96 128 192 256 384 */
/*        32  56  80 112 160 224 320  XX*/
	};
	static unsigned int sblims[5] = { 27, 30, 8, 12, 30 };
	static al_table *tables[5] = { alloc_0, alloc_1, alloc_2, alloc_3, alloc_4 };
	int table;

	if(mp3d->lsf)				// does this work?
		table = 4;
	else
		table = translate[mp3d->frequency_index][2 - mp3d->filechannels][mp3d->bitrate_index];
	mp3d->II_alloc = tables[table];
	mp3d->II_sblimit = sblims[table];
	mp3d->II_jsbound = (mp3d->mpg_chmode == MPG_MD_JOINT_STEREO) ? ((mp3d->mpg_chmode_ext << 2) + 4) : mp3d->II_sblimit;
}

void mpxdec_layer2_init(void)
{
	static MP2_FLOAT_T mulmul[27] = {
		0.0, -2.0 / 3.0, 2.0 / 3.0,
		2.0 / 7.0, 2.0 / 15.0, 2.0 / 31.0, 2.0 / 63.0, 2.0 / 127.0, 2.0 / 255.0,
		2.0 / 511.0, 2.0 / 1023.0, 2.0 / 2047.0, 2.0 / 4095.0, 2.0 / 8191.0,
		2.0 / 16383.0, 2.0 / 32767.0, 2.0 / 65535.0,
		-4.0 / 5.0, -2.0 / 5.0, 2.0 / 5.0, 4.0 / 5.0,
		-8.0 / 9.0, -4.0 / 9.0, -2.0 / 9.0, 2.0 / 9.0, 4.0 / 9.0, 8.0 / 9.0
	};
	static int base[3][9] = {
		{1, 0, 2,},
		{17, 18, 0, 19, 20,},
		{21, 1, 22, 23, 0, 24, 25, 2, 26}
	};
	static unsigned int tablen[3] = { 3, 5, 9 };
	static unsigned int *tables[3] = { grp_3tab, grp_5tab, grp_9tab };
	int i, j, k, l, len;
	MP2_FLOAT_T *table;
	unsigned int *itable;

	for(i = 0; i < 3; i++) {
		itable = tables[i];
		len = tablen[i];
		for(j = 0; j < len; j++)
			for(k = 0; k < len; k++)
				for(l = 0; l < len; l++) {
					*itable++ = base[i][l];
					*itable++ = base[i][k];
					*itable++ = base[i][j];
				}
	}

	for(k = 0; k < 27; k++) {
		const MP2_FLOAT_T m = mulmul[k];
		table = muls[k];
		for(j = 3, i = 0; i < 63; i++, j--)
			*table++ = pow(2.0, (double)j / 3.0) * m;
		*table = 0.0;
	}
}

#ifdef WIN32
#define getbits_one(x) _getbits_one(loc_bitindex,loc_wordpointer,x)
unsigned int _getbits_one(int *loc_bitindex, int *loc_wordpointer, int x)
{
	int val;
	__asm {
		mov ebx, x mov edi, loc_bitindex mov eax,[edi]
		mov ecx, eax shr eax, 3 mov edi, loc_wordpointer mov edx,[edi]
		add eax, edx mov eax, dword ptr[eax]
		and ecx, 7 bswap eax shl eax, cl mov cl, 32 sub cl, bl shr eax, cl mov edi, loc_bitindex mov eax,[edi]
	add eax, ebx mov[edi], eax} return val;
}
#else
unsigned int getbits_one(int);
#endif
static void II_step_one(struct mp3_decoder_data *mp3d, unsigned int *bit_alloc, int *scale)
{
	int sblimit = mp3d->II_sblimit;
	int jsbound = mp3d->II_jsbound;
	int stereo = mp3d->filechannels - 1;
	int sblimit2 = mp3d->II_sblimit << stereo;
	al_table *alloc1 = mp3d->II_alloc;
	int i;
	unsigned int scfsi_buf[64];
	register unsigned int *scfsi, *bita;
	register int sc, step;
	int loc_bitindex = mpxdec_bitindex;
	unsigned char *loc_wordpointer = mpxdec_wordpointer;

#pragma aux getbits_one=\
	"mov  eax,loc_bitindex"\
	"mov  ecx,eax"\
	"shr  eax,3"\
	"add  eax,loc_wordpointer"\
	"mov  eax,dword ptr [eax]"\
	"and  ecx,7"\
	"bswap eax"\
	"shl  eax,cl"\
	"mov  cl,32"\
	"sub  cl,bl"\
	"shr  eax,cl"\
	"add  loc_bitindex,ebx"\
	parm [ebx] value [eax] modify [ecx];

#define getbits_fast(x) getbits_one(x)

	bita = bit_alloc;
	if(stereo) {
		for(i = jsbound; i; i--) {
			step = alloc1->bits;
			alloc1 += (1 << step);
			bita[0] = getbits_one(step);
			bita[1] = getbits_one(step);
			bita += 2;
		}
		for(i = sblimit - jsbound; i; i--) {
			step = alloc1->bits;
			bita[0] = bita[1] = getbits_one(step);
			bita += 2;
			alloc1 += (1 << step);
		}
		bita = bit_alloc;
		scfsi = scfsi_buf;
		for(i = sblimit2; i; i--) {
			if(*bita++)
				*scfsi++ = getbits_fast(2);
		}
	} else {
		for(i = sblimit; i; i--) {
			step = alloc1->bits;
			*bita++ = getbits_one(step);
			alloc1 += (1 << step);
		}
		bita = bit_alloc;
		scfsi = scfsi_buf;
		for(i = sblimit; i; i--) {
			if(*bita++)
				*scfsi++ = getbits_fast(2);
		}
	}
	bita = bit_alloc;
	scfsi = scfsi_buf;
	for(i = sblimit2; i; i--) {
		if(*bita++) {
			switch (*scfsi) {
			case 0:
				scale[0] = getbits_fast(6);
				scale[1] = getbits_fast(6);
				scale[2] = getbits_fast(6);
				break;
			case 1:
				sc = getbits_fast(6);
				scale[0] = sc;
				scale[1] = sc;
				scale[2] = getbits_fast(6);
				break;
			case 2:
				sc = getbits_fast(6);
				scale[0] = sc;
				scale[1] = sc;
				scale[2] = sc;
				break;
			default:
				scale[0] = getbits_fast(6);
				sc = getbits_fast(6);
				scale[1] = sc;
				scale[2] = sc;
				break;
			}
			scale += 3;
			scfsi++;
		}
	}
	mpxdec_bitindex = loc_bitindex;
}

#ifdef WIN32
#define getbits_two(x) _getbits_one(loc_bitindex,loc_wordpointer,x)
#else
unsigned int getbits_two(int);
#endif

static void II_step_two(struct mp3_decoder_data *mp3d, unsigned int *bita, int *scale, hybridout_t * hout_i)
{
	static unsigned int *table[] = { 0, 0, 0, grp_3tab, 0, grp_5tab, 0, 0, 0, grp_9tab };
	register hybridout_t *hout0, *hout1;
	int sb, ch, k, d1;
	int channels = mp3d->filechannels;
	int sblimit = mp3d->II_sblimit;
	int jsbound = mp3d->II_jsbound;
	al_table *alloc2, *alloc1 = mp3d->II_alloc;
	int loc_bitindex = mpxdec_bitindex, lastnonzero[2];
	unsigned char *loc_wordpointer = mpxdec_wordpointer;

#pragma aux getbits_two=\
	"mov  eax,loc_bitindex"\
	"mov  ecx,eax"\
	"shr  eax,3"\
	"add  eax,loc_wordpointer"\
	"mov  eax,dword ptr [eax]"\
	"and  ecx,7"\
	"bswap eax"\
	"shl  eax,cl"\
	"mov  cl,32"\
	"sub  cl,bl"\
	"shr  eax,cl"\
	"add  loc_bitindex,ebx"\
	parm [ebx] value [eax] modify [ecx];

	lastnonzero[0] = lastnonzero[1] = 0;
	hout0 = hout_i;
	for(sb = 0; sb < jsbound; sb++) {
		hout1 = hout0;
		for(ch = 0; ch < channels; ch++) {
			if(bita[0]) {
				k = (alloc2 = alloc1 + bita[0])->bits;
				if((d1 = alloc2->d) < 0) {
					const MP2_FLOAT_T cm = muls[k][scale[0]];
					MPXDEC_PUT_HOUT(&hout1[0], ((MP2_FLOAT_T) ((int)getbits_two(k) + d1)) * cm);
					MPXDEC_PUT_HOUT(&hout1[32], ((MP2_FLOAT_T) ((int)getbits_two(k) + d1)) * cm);
					MPXDEC_PUT_HOUT(&hout1[64], ((MP2_FLOAT_T) ((int)getbits_two(k) + d1)) * cm);
				} else {
					unsigned int idx, *tab, m;
					idx = getbits_two(k);
					tab = table[d1] + idx + idx + idx;
					m = scale[0];
					MPXDEC_PUT_HOUT(&hout1[0], muls[tab[0]][m]);
					MPXDEC_PUT_HOUT(&hout1[32], muls[tab[1]][m]);
					MPXDEC_PUT_HOUT(&hout1[64], muls[tab[2]][m]);
				}
				lastnonzero[ch] = sb;
				scale += 3;
			} else {
				MPXDEC_PUT_HOUT(&hout1[0], 0.0);
				MPXDEC_PUT_HOUT(&hout1[32], 0.0);
				MPXDEC_PUT_HOUT(&hout1[64], 0.0);
			}
			bita++;
			hout1 += 576;
		}
		alloc1 += (1 << alloc1->bits);
		hout0++;
	}
	hout1 = hout0 + 576;
	bita++;
	for(sb = jsbound; sb < sblimit; sb++) {
		if(bita[0]) {
			k = (alloc2 = alloc1 + bita[0])->bits;
			if((d1 = alloc2->d) < 0) {
				const MP2_FLOAT_T cm0 = muls[k][scale[0]];
				const MP2_FLOAT_T cm1 = muls[k][scale[3]];
				int b;
				b = getbits_two(k) + d1;
				MPXDEC_PUT_HOUT(&hout0[0], ((MP2_FLOAT_T) b) * cm0);
				MPXDEC_PUT_HOUT(&hout1[0], ((MP2_FLOAT_T) b) * cm1);
				b = getbits_two(k) + d1;
				MPXDEC_PUT_HOUT(&hout0[32], ((MP2_FLOAT_T) b) * cm0);
				MPXDEC_PUT_HOUT(&hout1[32], ((MP2_FLOAT_T) b) * cm1);
				b = getbits_two(k) + d1;
				MPXDEC_PUT_HOUT(&hout0[64], ((MP2_FLOAT_T) b) * cm0);
				MPXDEC_PUT_HOUT(&hout1[64], ((MP2_FLOAT_T) b) * cm1);
			} else {
				unsigned int idx, *tab, m1, m2;
				idx = getbits_two(k);
				tab = table[d1] + idx + idx + idx;
				m1 = scale[0];
				m2 = scale[3];
				MPXDEC_PUT_HOUT(&hout0[0], muls[tab[0]][m1]);
				MPXDEC_PUT_HOUT(&hout1[0], muls[tab[0]][m2]);
				MPXDEC_PUT_HOUT(&hout0[32], muls[tab[1]][m1]);
				MPXDEC_PUT_HOUT(&hout1[32], muls[tab[1]][m2]);
				MPXDEC_PUT_HOUT(&hout0[64], muls[tab[2]][m1]);
				MPXDEC_PUT_HOUT(&hout1[64], muls[tab[2]][m2]);
			}
			scale += 6;
			lastnonzero[0] = lastnonzero[1] = sb;
		} else {
			MPXDEC_PUT_HOUT(&hout0[0], 0);
			MPXDEC_PUT_HOUT(&hout0[32], 0);
			MPXDEC_PUT_HOUT(&hout0[64], 0);
			MPXDEC_PUT_HOUT(&hout1[0], 0);
			MPXDEC_PUT_HOUT(&hout1[32], 0);
			MPXDEC_PUT_HOUT(&hout1[64], 0);
		}
		bita += 2;
		alloc1 += (1 << alloc1->bits);
		hout0++;
		hout1++;
	}
	if(MIXER_var_usehq) {
		for(ch = 0; ch < channels; ch++) {
			if(lastnonzero[ch] >= 16) {
				hout0 = hout_i + (576 * ch) + lastnonzero[ch];
				hout1 = hout0 + 1;
				k = 1;
				for(sb = lastnonzero[ch] + 1; sb < SBLIMIT; sb++) {
					const MP2_FLOAT_T cm = (MP2_FLOAT_T) (k++) * (MP2_FLOAT_T) (1.4);
					MPXDEC_PUT_HOUT(&hout1[0], MPXDEC_GET_HOUT(&hout0[0]) / cm);
					MPXDEC_PUT_HOUT(&hout1[32], MPXDEC_GET_HOUT(&hout0[32]) / cm);
					MPXDEC_PUT_HOUT(&hout1[64], MPXDEC_GET_HOUT(&hout0[64]) / cm);
					hout1++;
				}
			} else {
				hout1 = hout_i + (576 * ch) + sblimit;
				for(sb = sblimit; sb < SBLIMIT; sb++) {
					MPXDEC_PUT_HOUT(&hout1[0], 0);
					MPXDEC_PUT_HOUT(&hout1[32], 0);
					MPXDEC_PUT_HOUT(&hout1[64], 0);
					hout1++;
				}
			}
		}
	} else {
		for(ch = 0; ch < channels; ch++) {
			hout1 = hout0;
			for(sb = sblimit; sb < SBLIMIT; sb++) {
				MPXDEC_PUT_HOUT(&hout1[0], 0);
				MPXDEC_PUT_HOUT(&hout1[32], 0);
				MPXDEC_PUT_HOUT(&hout1[64], 0);
				hout1++;
			}
			hout0 += 576;
		}
	}
	mpxdec_bitindex = loc_bitindex;
}

void mpxdec_layer2_decode_part1(struct mp3_decoder_data *mp3d)
{
	int i;
	hybridout_t *hybridout = mp3d->hybridp;
	unsigned int bit_alloc[64];
	int scale[192];

	II_step_one(mp3d, bit_alloc, scale);
	for(i = 0; i < 6; i++) {
		II_step_two(mp3d, bit_alloc, &scale[i >> 2], hybridout);
		hybridout += 3 * SBLIMIT;
	}
	hybridout += (18 * SBLIMIT);
	for(; i < 12; i++) {
		II_step_two(mp3d, bit_alloc, &scale[i >> 2], hybridout);
		hybridout += 3 * SBLIMIT;
	}
}
