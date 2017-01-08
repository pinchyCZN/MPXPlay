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
//function: Audio MPEG Layer III decoding
//based on the MPG123 MPEG decoder source

//#define MPXPLAY_USE_DEBUGF 1
#define MPXDEC_DEBUG_OUTPUT stdout

#include "newfunc\newfunc.h"
#include "mp3dec.h"
#include "display\display.h"
#include "malloc.h"

static struct gr_info_s si[2][2];

static int mpxdec_tabsel_123[2][3][16] = {
	{{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,},
	 {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384,},
	 {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320,}},

	{{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256,},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,}}
};

static long mpxdec_freqs[10] = { 44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000, 0 };

struct bandInfoStruct {
	int longIdx[23];
	int longDiff[22];
	int shortIdx[14];
	int shortDiff[13];
};

static struct bandInfoStruct bandInfo[9] = {

/* MPEG 1.0 */
	{{0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418, 576},
	 {4, 4, 4, 4, 4, 4, 6, 6, 8, 8, 10, 12, 16, 20, 24, 28, 34, 42, 50, 54, 76, 158},
	 {0, 4 * 3, 8 * 3, 12 * 3, 16 * 3, 22 * 3, 30 * 3, 40 * 3, 52 * 3, 66 * 3, 84 * 3, 106 * 3, 136 * 3, 192 * 3},
	 {4, 4, 4, 4, 6, 8, 10, 12, 14, 18, 22, 30, 56}},

	{{0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384, 576},
	 {4, 4, 4, 4, 4, 4, 6, 6, 6, 8, 10, 12, 16, 18, 22, 28, 34, 40, 46, 54, 54, 192},
	 {0, 4 * 3, 8 * 3, 12 * 3, 16 * 3, 22 * 3, 28 * 3, 38 * 3, 50 * 3, 64 * 3, 80 * 3, 100 * 3, 126 * 3, 192 * 3},
	 {4, 4, 4, 4, 6, 6, 10, 12, 14, 16, 20, 26, 66}},

	{{0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550, 576},
	 {4, 4, 4, 4, 4, 4, 6, 6, 8, 10, 12, 16, 20, 24, 30, 38, 46, 56, 68, 84, 102, 26},
	 {0, 4 * 3, 8 * 3, 12 * 3, 16 * 3, 22 * 3, 30 * 3, 42 * 3, 58 * 3, 78 * 3, 104 * 3, 138 * 3, 180 * 3, 192 * 3},
	 {4, 4, 4, 4, 6, 8, 12, 16, 20, 26, 34, 42, 12}},

/* MPEG 2.0 */
	{{0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576},
	 {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54},
	 {0, 4 * 3, 8 * 3, 12 * 3, 18 * 3, 24 * 3, 32 * 3, 42 * 3, 56 * 3, 74 * 3, 100 * 3, 132 * 3, 174 * 3, 192 * 3},
	 {4, 4, 4, 6, 6, 8, 10, 14, 18, 26, 32, 42, 18}},
	/* docs: 332. mpg123: 330 */
	{{0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 114, 136, 162, 194, 232, 278, 332, 394, 464, 540, 576},
	 {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 18, 22, 26, 32, 38, 46, 54, 62, 70, 76, 36},
	 //{ {0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278,330,394,464,540,576},
	 //  {6,6,6,6,6,6,8,10,12,14,16,18,22,26,32,38,46,52,64,70,76,36 } ,
	 {0, 4 * 3, 8 * 3, 12 * 3, 18 * 3, 26 * 3, 36 * 3, 48 * 3, 62 * 3, 80 * 3, 104 * 3, 136 * 3, 180 * 3, 192 * 3},
	 {4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 32, 44, 12}},

	{{0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576},
	 {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54},
	 {0, 4 * 3, 8 * 3, 12 * 3, 18 * 3, 26 * 3, 36 * 3, 48 * 3, 62 * 3, 80 * 3, 104 * 3, 134 * 3, 174 * 3, 192 * 3},
	 {4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18}},

	/* MPEG 2.5 */
	{{0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576},
	 {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54},
	 {0, 12, 24, 36, 54, 78, 108, 144, 186, 240, 312, 402, 522, 576},
	 {4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18}},
	{{0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576},
	 {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54},
	 {0, 12, 24, 36, 54, 78, 108, 144, 186, 240, 312, 402, 522, 576},
	 {4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18}},
	{{0, 12, 24, 36, 48, 60, 72, 88, 108, 132, 160, 192, 232, 280, 336, 400, 476, 566, 568, 570, 572, 574, 576},
	 {12, 12, 12, 12, 12, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 76, 90, 2, 2, 2, 2, 2},
	 {0, 24, 48, 72, 108, 156, 216, 288, 372, 480, 486, 492, 498, 576},
	 {8, 8, 8, 12, 16, 20, 24, 28, 36, 2, 2, 2, 26}},
};

static unsigned char pretab0[10] = { 1, 1, 1, 1, 2, 2, 3, 3, 3, 2 };
static unsigned char pretab1[40] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0 };
static unsigned char pretab2[40] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int mapbuf0[9][152];
static int mapbuf1[9][156];
static int mapbuf2[9][32];
static int *map[9][3];

static unsigned int n_slen2[512];
static unsigned int i_slen2[256];

static FLOAT_QV tan1_1[16], tan2_1[16], tan1_2[16], tan2_2[16];
static FLOAT_QV pow1_1[2][16], pow2_1[2][16], pow1_2[2][16], pow2_2[2][16];

extern FLOAT_QV mpxdec_l3deq_gainpow2_2[512], mpxdec_l3deq_gainpow2_4[512];
extern FLOAT_QV mpxdec_l3deq_ispow32_2[512 * 32], mpxdec_l3deq_ispow32_4[512 * 32];

static unsigned char scalefac_curr[4][39];
static FLOAT_QD deq_outdata[MPXDEC_CHANNELS_MAX * DEQ_SBLIMIT * SSLIMIT + 128];
static hybridout_t mpx_hybridout[MPXDEC_GRANULES_MAX * MPXDEC_CHANNELS_MAX * SSLIMIT * SBLIMIT + 128];

int mpxdec_bitindex;
unsigned char *mpxdec_wordpointer;

static unsigned int mpxdec_layer3_decode_part1(struct mp3_decoder_data *mp3d, unsigned int chan_mode);
#ifdef MPXDEC_ENABLE_CRC_CHECK
static unsigned int CRC_checkheader(unsigned long head, unsigned char *bsbufp, unsigned int len);
#endif

//-----------------------------------------------------------------------
//API

struct mp3_decoder_data *mpxdec_init(void)
{
	struct mp3_decoder_data *mp3d;

	mp3d = (struct mp3_decoder_data *)calloc(1, sizeof(struct mp3_decoder_data));
	if(!mp3d)
		return mp3d;

	mp3d->bitstream_a = (unsigned char *)calloc(2 * MPXDEC_FRAMEBUFFER_SIZE + 32, sizeof(unsigned char));
	if(!mp3d->bitstream_a)
		goto err_out_init;
	mp3d->bitstreamp = mp3d->bitstream_a + MPXDEC_BACKSTEP_MAX;
	mpxdec_reset_bitstream(mp3d);

	//static temp datafields
	mp3d->scalefacs = &scalefac_curr[0][0];
	mp3d->deq_outdata = &deq_outdata[0];
	mp3d->hybridp = &mpx_hybridout[0];
	pds_memset(mp3d->scalefacs, 0, 4 * 39 * sizeof(*mp3d->scalefacs));
	pds_memset(mp3d->deq_outdata, 0, (MPXDEC_CHANNELS_MAX * DEQ_SBLIMIT * SSLIMIT + 128) * sizeof(*mp3d->deq_outdata));
	pds_memset(mp3d->hybridp, 0, (MPXDEC_GRANULES_MAX * MPXDEC_CHANNELS_MAX * SSLIMIT * SBLIMIT + 128) * sizeof(*mp3d->hybridp));

	//dynamic (separated) datafields
	mp3d->block_a = (block_t *) calloc(2 * MPXDEC_CHANNELS_MAX * SBLIMIT * SSLIMIT + 32, sizeof(*mp3d->block_a));
	if(!mp3d->block_a)
		goto err_out_init;

	mp3d->blockptr = mp3d->block_a + 1;

	mp3d->synthdata = mpxdec_layer3_synth_alloc(mp3d->hybridp);
	if(!mp3d->synthdata)
		goto err_out_init;

#if defined(USE_80_MDCT1236) && !defined(USE_80_DEQD)
	mp3d->cv_deq_dct = (mpxdec_float80_t *) malloc((MPXDEC_CHANNELS_MAX * DEQ_SBLIMIT * SSLIMIT + 128) * sizeof(*mp3d->cv_deq_dct));
	if(!mp3d->cv_deq_dct)
		goto err_out_init;
#endif

	return mp3d;

  err_out_init:
	mpxdec_close(mp3d);
	return NULL;
}

void mpxdec_close(struct mp3_decoder_data *mp3d)
{
	if(mp3d) {
		if(mp3d->bitstream_a)
			free(mp3d->bitstream_a);
		if(mp3d->block_a)
			free(mp3d->block_a);
		mpxdec_layer3_synth_close(mp3d->synthdata);
#if defined(USE_80_MDCT1236) && !defined(USE_80_DEQD)
		if(mp3d->cv_deq_dct)
			free(mp3d->cv_deq_dct);
#endif
		free(mp3d);
	}
}

void mpxdec_reset_bitstream(struct mp3_decoder_data *mp3d)
{
	mp3d->fsize_curr = mp3d->fsize_prev = mp3d->fsize_next = mp3d->bsnum = 0;
	mp3d->bsbuf = mp3d->bitstreamp + MPXDEC_FRAMEBUFFER_SIZE;
}

void mpxdec_reset_decoding(struct mp3_decoder_data *mp3d)
{
	pds_memset(mp3d->scalefacs, 0, 4 * 39 * sizeof(*mp3d->scalefacs));
	pds_memset(mp3d->block_a, 0, (2 * MPXDEC_CHANNELS_MAX * SBLIMIT * SSLIMIT + 1) * sizeof(*mp3d->block_a));
}

void mpxdec_reset_synth(struct mp3_decoder_data *mp3d)
{
	mpxdec_layer3_synth_clear(mp3d->synthdata);
}

int mpxdec_syncinfo(struct mp3_decoder_data *mp3d, unsigned char *bsbuf)
{
	register unsigned long newhead;

	newhead = PDS_GETB_BE32(bsbuf);

	if(mp3d->firsthead) {
#ifdef MPXDEC_ENABLE_FREEFORMAT
		if(mp3d->infobits & MP3DI_INFOBIT_FREEFORMAT) {
			if((newhead & MPXDEC_HEADMASK_FREEFORMAT) != mp3d->firsthead)
				return -1;
		} else
#endif
		if((newhead & MPXDEC_HEADMASK_STANDARD) != mp3d->firsthead)
			return -1;
	} else {
		if((newhead & MPXDEC_HEADMASK_SYNC) != MPXDEC_HEADMASK_SYNC)
			return -1;
	}

	mp3d->bitrate_index = ((newhead >> 12) & 0xf);

#ifndef MPXDEC_ENABLE_FREEFORMAT
	if(!mp3d->bitrate_index)
		return -1;
#endif
	if(mp3d->bitrate_index == 15)
		return -1;
	//mpxplay_debugf(MPXDEC_DEBUG_OUTPUT,"syncinfo begin %8.8X",PDS_GETB_BE32(bsbuf));
	mp3d->frequency_index = ((newhead >> 10) & 0x3);
	if(mp3d->frequency_index > 2)
		return -1;
	mp3d->lay = 4 - ((newhead >> 17) & 0x3);

	if(newhead & (1 << 20))
		mp3d->lsf = (newhead & (1 << 19)) ? 0 : 3;
	else
		mp3d->lsf = 6;
	mp3d->frequency_index += mp3d->lsf;
	if(mp3d->lsf) {
		mp3d->lsf = 1;
		mp3d->granules = 1;
	} else
		mp3d->granules = 2;
	if(mp3d->synthdata)			// else called from deparser
		mp3d->synthdata->granules = mp3d->granules;
	mp3d->error_protection = ((newhead >> 16) & 0x1) ^ 0x1;
#ifndef MPXDEC_ENABLE_FREEFORMAT
	mp3d->padding = ((newhead >> 9) & 0x1);
#endif
	mp3d->mpg_chmode = ((newhead >> 6) & 0x3);
	mp3d->mpg_chmode_ext = ((newhead >> 4) & 0x3);
	if(mp3d->mpg_chmode == MPG_MD_JOINT_STEREO) {
		mp3d->ms_stereo = mp3d->mpg_chmode_ext & 0x2;
		mp3d->i_stereo = mp3d->mpg_chmode_ext & 0x1;
	} else
		mp3d->ms_stereo = mp3d->i_stereo = 0;

	mp3d->filechannels = (mp3d->mpg_chmode == MPG_MD_MONO) ? 1 : 2;

	//mpxplay_debugf(MPXDEC_DEBUG_OUTPUT,"lay :%d freq:%d lsf:%d gr:%d",mp3d->lay,mp3d->freq,mp3d->lsf,mp3d->granules);
	switch (mp3d->lay) {
	case 2:
		mpxdec_layer2_getstuff(mp3d);
		break;
	case 3:
		if(mp3d->lsf)
			mp3d->ssize = (mp3d->filechannels == 1) ? 9 : 17;
		else
			mp3d->ssize = (mp3d->filechannels == 1) ? 17 : 32;
		if(mp3d->error_protection)
			mp3d->ssize += 2;
		break;
	default:
		return -1;
	}

	mp3d->freq = mpxdec_freqs[mp3d->frequency_index];

#ifdef MPXDEC_ENABLE_FREEFORMAT
	if(mp3d->bitrate_index) {
		mp3d->padding = ((newhead >> 9) & 0x1);
		mp3d->bitrate = mpxdec_tabsel_123[mp3d->lsf][mp3d->lay - 1][mp3d->bitrate_index];
		mp3d->fsize_next = (long)mp3d->bitrate * 144000 / (mp3d->freq << mp3d->lsf) + mp3d->padding;
	} else {
		mp3d->fsize_next = mp3d->fsize_curr - mp3d->padding;	// !!! bad
		mp3d->padding = ((newhead >> 9) & 0x1);	// needs an initial fsize_curr
		mp3d->fsize_next += mp3d->padding;	//
	}
#else
	mp3d->bitrate = mpxdec_tabsel_123[mp3d->lsf][mp3d->lay - 1][mp3d->bitrate_index];
	mp3d->fsize_next = (long)mp3d->bitrate * 144000 / (mp3d->freq << mp3d->lsf) + mp3d->padding;
#endif

	if(mp3d->fsize_next > MPXDEC_FRAMESIZE_MAX)
		return -1;

	if(!mp3d->firsthead) {
#ifdef MPXDEC_ENABLE_FREEFORMAT
		if(!mp3d->bitrate_index)
			funcbit_enable(mp3d->infobits, MP3DI_INFOBIT_FREEFORMAT);
		if(mp3d->infobits & MP3DI_INFOBIT_FREEFORMAT)
			mp3d->firsthead = newhead & MPXDEC_HEADMASK_FREEFORMAT;
		else
#endif
			mp3d->firsthead = newhead & MPXDEC_HEADMASK_STANDARD;
	}
#if defined(MPXDEC_ENABLE_FREEFORMAT) || defined(MPXDEC_ENABLE_CRC_CHECK)
	mp3d->newhead = newhead;
#endif

	return mp3d->fsize_next;
}

int mpxdec_read_frame(struct mp3_decoder_data *mp3d, unsigned char *bsbuf)
{
	mp3d->bsbufold = mp3d->bsbuf;
	mp3d->bsbuf = mp3d->bitstreamp + ((mp3d->bsnum) ? MPXDEC_FRAMEBUFFER_SIZE : 0);
	mp3d->bsnum ^= 1;

	pds_memcpy(mp3d->bsbuf, bsbuf, mp3d->fsize_next);

	mpxdec_wordpointer = mp3d->bsbuf + 4;	// the bitstream contains the 4-bytes header too (for -obs)

	if(mp3d->error_protection) {
#ifdef MPXDEC_ENABLE_CRC_CHECK
		if(mp3d->lay == 3)
			if(!CRC_checkheader(mp3d->newhead, mpxdec_wordpointer, mp3d->ssize))
				return -1;
#endif
		mpxdec_wordpointer += 2;
	}

	mpxdec_bitindex = 0;

	mp3d->fsize_prev = mp3d->fsize_curr;
	mp3d->fsize_curr = mp3d->fsize_next;

	return 0;
}

int mpxdec_decode_part1(struct mp3_decoder_data *mp3d, unsigned int chan_mode)
{
	unsigned int retcode;
	switch (mp3d->lay) {
	case 2:
		mpxdec_layer2_decode_part1(mp3d);
		retcode = MPXDEC_ERROR_OK;
		break;
	case 3:
		retcode = mpxdec_layer3_decode_part1(mp3d, chan_mode);
		break;
	}
	return retcode;
}

#ifdef MPXDEC_ENABLE_CRC_CHECK

//based on the LAME encoder
//(it works, but if the frame has wrong checksum, then we cannot play the file)

#define CRC16_POLYNOMIAL 0x8005

static unsigned int CRC_update(unsigned int crc, unsigned int value)
{
	unsigned int i = 8;
	value <<= 8;
	do {
		value <<= 1;
		crc <<= 1;
		if((crc ^ value) & 0x10000)
			crc ^= CRC16_POLYNOMIAL;
	} while(--i);
	return crc;
}

static unsigned int CRC_checkheader(unsigned long head, unsigned char *bsbufp, unsigned int len)
{
	unsigned int crc = 0xffff;
	unsigned int checksum = ((unsigned int)bsbufp[0] << 8) | ((unsigned int)bsbufp[1]);

	if(len > 2) {
		crc = CRC_update(crc, (head >> 8) & 0xff);
		crc = CRC_update(crc, (head) & 0xff);
		bsbufp += 2;			// skip the checksum itself
		len -= 2;
		do {
			crc = CRC_update(crc, (unsigned int)(*bsbufp++));
		} while(--len);
	}

	if((crc & 0xffff) == checksum)
		return 1;

	return 0;
}

#endif

//----------------------------------------------------------------------
void mpxdec_layer3_init(void)
{
	int i, j, k, l;

	/*for(i=0;i<(256+118+4);i++){
	   unsigned int indx2=((i&1)<<8)+(i>>1);
	   unsigned int indx4=((i&3)<<7)+(i>>2);
	   FLOAT_QV fg,isg,p;
	   MPXDEC_PUT_DEQV(&fg,pow(2.0,-0.25*(double)(i-46)));
	   //const double fg=exp(log(2)* -0.25*(double)(i-46));
	   MPXDEC_PUT_DEQV(&gainpow2_2[indx2],MPXDEC_GET_DEQV(&fg));
	   MPXDEC_PUT_DEQV(&gainpow2_4[indx4],MPXDEC_GET_DEQV(&fg));
	   indx2<<=5;
	   indx2+=16;
	   indx4<<=5;
	   indx4+=16;
	   for(j=1;j<16;j++){
	   MPXDEC_PUT_DEQV(&p,pow((double)j,4.0/3.0));
	   MPXDEC_PUT_DEQV(&isg,(MPXDEC_GET_DEQV(&p)*MPXDEC_GET_DEQV(&fg)));
	   //const FLOAT_QV isg=exp(log((double)j)*4.0/3.0)*fg;
	   MPXDEC_PUT_DEQV(&ispow32_2[indx2+j],MPXDEC_GET_DEQV(&isg));
	   MPXDEC_PUT_DEQV(&ispow32_4[indx4+j],MPXDEC_GET_DEQV(&isg));
	   MPXDEC_PUT_DEQV(&ispow32_2[indx2-j],-MPXDEC_GET_DEQV(&isg));
	   MPXDEC_PUT_DEQV(&ispow32_4[indx4-j],-MPXDEC_GET_DEQV(&isg));
	   }
	   } */

	/*for(i=0;i<(256+118+4);i++){
	   unsigned int indx2=((i&1)<<8)+(i>>1);
	   unsigned int indx4=((i&3)<<7)+(i>>2);
	   const double fg=pow(2.0,-0.25*(double)(i-46));
	   //const double fg=exp(log(2)* -0.25*(double)(i-46));
	   MPXDEC_PUT_DEQV(&gainpow2_2[indx2],fg);
	   MPXDEC_PUT_DEQV(&gainpow2_4[indx4],fg);
	   indx2<<=5;
	   indx2+=16;
	   indx4<<=5;
	   indx4+=16;
	   for(j=1;j<16;j++){
	   const double isg=pow((double)j,4.0/3.0)*fg;
	   //const FLOAT_QV isg=exp(log((double)j)*4.0/3.0)*fg;
	   MPXDEC_PUT_DEQV(&ispow32_2[indx2+j],isg);
	   MPXDEC_PUT_DEQV(&ispow32_4[indx4+j],isg);
	   MPXDEC_PUT_DEQV(&ispow32_2[indx2-j],-isg);
	   MPXDEC_PUT_DEQV(&ispow32_4[indx4-j],-isg);
	   }
	   } */

	//IS MPEG 1.0
	for(i = 0; i < 7; i++) {
		const double s = sin(i * M_PI / 12.0);
		const double c = cos(i * M_PI / 12.0);
		double tan2 = (c / (s + c)), tan1 = (s / (s + c));
		MPXDEC_PUT_DEQV(&tan1_1[i], tan1);
		MPXDEC_PUT_DEQV(&tan1_1[i + 8], tan1);
		tan1 *= sqrt(2.0);
		MPXDEC_PUT_DEQV(&tan1_2[i], tan1);
		MPXDEC_PUT_DEQV(&tan1_2[i + 8], tan1);
		MPXDEC_PUT_DEQV(&tan2_1[i], tan2);
		MPXDEC_PUT_DEQV(&tan2_1[i + 8], tan2);
		tan2 *= sqrt(2.0);
		MPXDEC_PUT_DEQV(&tan2_2[i], tan2);
		MPXDEC_PUT_DEQV(&tan2_2[i + 8], tan2);
	}
	MPXDEC_PUT_DEQV(&tan1_1[7], 1.0);
	MPXDEC_PUT_DEQV(&tan2_1[7], 1.0);
	MPXDEC_PUT_DEQV(&tan1_2[7], 1.0);
	MPXDEC_PUT_DEQV(&tan2_2[7], 1.0);
	MPXDEC_PUT_DEQV(&tan1_1[15], 1.0);
	MPXDEC_PUT_DEQV(&tan2_1[15], 1.0);
	MPXDEC_PUT_DEQV(&tan1_2[15], 1.0);
	MPXDEC_PUT_DEQV(&tan2_2[15], 1.0);

	//IS MPEG 2.x (LSF)
	for(i = 0; i < 16; i++) {
		for(j = 0; j < 2; j++) {
			double base = pow(2.0, -0.25 * (j + 1.0));
			double p1 = 1.0, p2 = 1.0;
			if(i > 0) {
				if(i & 1)
					p1 = pow(base, (i + 1.0) * 0.5);
				else
					p2 = pow(base, i * 0.5);
			}
			MPXDEC_PUT_DEQV(&pow1_1[j][i], p1);
			p1 *= sqrt(2.0);
			MPXDEC_PUT_DEQV(&pow1_2[j][i], p1);
			MPXDEC_PUT_DEQV(&pow2_1[j][i], p2);
			p2 *= sqrt(2.0);
			MPXDEC_PUT_DEQV(&pow2_2[j][i], p2);
		}
	}

	//bt=2
	for(j = 0; j < 9; j++) {
		struct bandInfoStruct *bi = (struct bandInfoStruct *)&bandInfo[j];
		int *mp;
		int cb, lwin;
		int *bdf;

		mp = map[j][0] = mapbuf0[j];
		bdf = bi->longDiff;
		for(i = 0, cb = 1; cb < 9; cb++, i += *bdf++) {
			*mp++ = (*bdf) >> 1;
			*mp++ = i * sizeof(FLOAT_QD);	// asm (xrpnt)
			*mp++ = 3 * sizeof(unsigned int);	// asm adressing in deq_bt2 (full_gain)
			*mp++ = cb;
		}
		bdf = bi->shortDiff + 3;
		for(cb = 4; cb < 14; cb++) {
			int l = (*bdf++) >> 1;
			for(lwin = 0; lwin < 3; lwin++) {
				*mp++ = l;
				*mp++ = (i + lwin) * sizeof(FLOAT_QD);	// asm
				*mp++ = lwin * sizeof(unsigned int);	// asm
				*mp++ = cb;
			}
			i += 6 * l;
		}

		mp = map[j][1] = mapbuf1[j];
		bdf = bi->shortDiff + 0;
		for(i = 0, cb = 1; cb < 14; cb++) {
			int l = (*bdf++) >> 1;
			for(lwin = 0; lwin < 3; lwin++) {
				*mp++ = l;
				*mp++ = (i + lwin) * sizeof(FLOAT_QD);	// asm
				*mp++ = lwin * sizeof(unsigned int);	// asm
				*mp++ = cb;
			}
			i += 6 * l;
		}

		mp = map[j][2] = mapbuf2[j];
		bdf = bi->longDiff;
		for(cb = 0; cb < 22; cb++) {
			*mp++ = (*bdf++) >> 1;
		}
	}

	//MPEG 2.x (LSF)
	for(i = 0; i < 5; i++)
		for(j = 0; j < 6; j++)
			for(k = 0; k < 6; k++) {
				int n = k + j * 6 + i * 36;
				i_slen2[n] = i | (j << 3) | (k << 6) | (3 << 12);
			}

	for(i = 0; i < 4; i++)
		for(j = 0; j < 4; j++)
			for(k = 0; k < 4; k++) {
				int n = k + j * 4 + i * 16;
				i_slen2[n + 180] = i | (j << 3) | (k << 6) | (4 << 12);
			}

	for(i = 0; i < 4; i++)
		for(j = 0; j < 3; j++) {
			int n = j + i * 3;
			i_slen2[n + 244] = i | (j << 3) | (5 << 12);
			n_slen2[n + 500] = i | (j << 3) | (2 << 12) | (1 << 15);
		}

	for(i = 0; i < 5; i++)
		for(j = 0; j < 5; j++)
			for(k = 0; k < 4; k++)
				for(l = 0; l < 4; l++) {
					int n = l + k * 4 + j * 16 + i * 80;
					n_slen2[n] = i | (j << 3) | (k << 6) | (l << 9) | (0 << 12);
				}

	for(i = 0; i < 5; i++)
		for(j = 0; j < 5; j++)
			for(k = 0; k < 4; k++) {
				int n = k + j * 4 + i * 20;
				n_slen2[n + 400] = i | (j << 3) | (k << 6) | (1 << 12);
			}

	mpxdec_l3deq_init();
	mpxdec_l3mdct_init();
}

void mpxdec_preinit(void)
{
	mpxdec_synth_init(65536);
	mpxdec_layer2_init();
	mpxdec_layer3_init();
}
//--------------------------------------------------------------

// number of bits must be between 1 and 24, you have to check this before calling
unsigned int getbits_24(int);
#pragma aux getbits_24=\
	"mov  eax,mpxdec_bitindex"\
	"mov  ecx,eax"\
	"add  mpxdec_bitindex,ebx"\
	"shr  eax,3"\
	"add  eax,mpxdec_wordpointer"\
	"mov  eax,dword ptr [eax]"\
	"and  ecx,7"\
	"bswap eax"\
	"shl  eax,cl"\
	"mov  cl,32"\
	"sub  cl,bl"\
	"shr  eax,cl"\
	parm [ebx] value [eax] modify [ecx];

// number of bits must be between 0 and 8
unsigned int getbits_8(int);
#pragma aux getbits_8=\
	"mov  eax,mpxdec_bitindex"\
	"mov  ecx,eax"\
	"add  mpxdec_bitindex,ebx"\
	"shr  eax,3"\
	"add  eax,mpxdec_wordpointer"\
	"movzx eax,word ptr [eax]"\
	"and  ecx,7"\
	"xchg al,ah"\
	"shl  ax,cl"\
	"mov  cl,16"\
	"sub  cl,bl"\
	"shr  eax,cl"\
	parm [ebx] value [eax] modify [ecx];

unsigned int get1bit(void);
#pragma aux get1bit=\
	"mov  eax,mpxdec_bitindex"\
	"mov  ecx,eax"\
	"inc  mpxdec_bitindex"\
	"shr  eax,3"\
	"add  eax,dword ptr mpxdec_wordpointer"\
	"and  ecx,7"\
	"movzx eax,byte ptr [eax]"\
	"shl  eax,cl"\
	"and  eax,128"\
	value [eax] modify [ecx];


static void III_correct_regions(struct gr_info_s *gr_info)
{
	int region1 = gr_info->regions[0];
	int bv = gr_info->regions[2];

	if(bv <= region1) {
		gr_info->regions[0] = bv;
		gr_info->regions[1] = 0;
		gr_info->regions[2] = 0;
	} else {
		int region2 = gr_info->regions[1];
		if(bv <= region2) {
			gr_info->regions[1] = bv - region1;
			gr_info->regions[2] = 0;
		} else {
			if(region2 < region1)	// ???
				region2 = region1;
			gr_info->regions[1] = region2 - region1;
			gr_info->regions[2] = bv - region2;
		}
	}
	gr_info->regions[3] = (288 - bv) >> 1;
}

static unsigned int III_get_side_info_1(struct gr_info_s *gr_info, int stereo, int ms_stereo, long sfreq)
{
	unsigned int gainpow2p = 256 + ((ms_stereo) ? 2 : 0), grch;

	if(stereo & 2) {
		unsigned int i;
		mpxdec_bitindex += 3;
		(gr_info)->scfsi = 0;
		(gr_info + 1)->scfsi = 0;
		i = getbits_8(8);
		(gr_info + 2)->scfsi = i >> 4;
		i &= 0xf;
		(gr_info + 3)->scfsi = i;
		grch = 4;
	} else {
		mpxdec_bitindex += 5;
		(gr_info)->scfsi = 0;
		(gr_info + 2)->scfsi = getbits_8(4);
		grch = 2;
	}

	do {
		register unsigned int i, pow2gain;
		i = getbits_24(21);
		gr_info->part2_3_length = i >> 9;	// 12 bits
		i &= 511;
		if(i > 288)
			return MPXDEC_ERROR_BITSTREAM;
		gr_info->regions[2] = i;	// 9 bits
		i = getbits_24(13);
		pow2gain = gainpow2p - (i >> 5);	// 8 bits
		gr_info->scalefac_compress = (i >> 1) & 15;	// 4 bits
		if(i & 1) {				// window-switching flag==1 for block_Type!=0 ; window-sw-flag = 0 for block_type==0
			i = getbits_24(13);
			gr_info->table_select[2] = 0;
			gr_info->table_select[1] = i & 31;
			i >>= 5;			// 5 bits
			gr_info->table_select[0] = i & 31;
			i >>= 5;			// 5 bits
			gr_info->mixed_block_flag = i & 1;
			i >>= 1;			// 1 bit
			gr_info->block_type = i & 3;	// 2 bits
			if(gr_info->block_type == 2) {
				if(gr_info->mixed_block_flag)
					gr_info->mapp = map[sfreq][0];
				else
					gr_info->mapp = map[sfreq][1];
			} else
				gr_info->mapp = map[sfreq][2];
			i = getbits_24(12);
			gr_info->count1table_select = i & 1;
			i >>= 1;
			if(i & 1) {			// gr_info->scalefac_scale
				unsigned int index = ((pow2gain & 3) << 7) + (pow2gain >> 2);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_4[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_4[(index << 5) + 16]);
				i >>= 1;
				gr_info->full_gain[2] = (i & 14);
				gr_info->preflag = i & 1;
				i >>= 3;
				gr_info->full_gain[1] = (i & 14);
				i >>= 3;
				gr_info->full_gain[0] = (i & 14);
			} else {
				unsigned int index = ((pow2gain & 1) << 8) + (pow2gain >> 1);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_2[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_2[(index << 5) + 16]);
				gr_info->full_gain[2] = (i & 28);
				gr_info->preflag = i & 2;
				i >>= 3;
				gr_info->full_gain[1] = (i & 28);
				i >>= 3;
				gr_info->full_gain[0] = (i & 28);
			}
			gr_info->regions[0] = (36 >> 1);
			gr_info->regions[1] = (576 >> 1);
		} else {
			register int r1c;
			gr_info->mapp = map[sfreq][2];
			i = getbits_24(15);
			gr_info->table_select[2] = i & 31;
			i >>= 5;
			gr_info->table_select[1] = i & 31;
			i >>= 5;
			gr_info->table_select[0] = i & 31;
			i = getbits_24(10);
			gr_info->count1table_select = i & 1;
			if(i & 2) {			// gr_info->scalefac_scale
				unsigned int index = ((pow2gain & 3) << 7) + (pow2gain >> 2);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_4[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_4[(index << 5) + 16]);
			} else {
				unsigned int index = ((pow2gain & 1) << 8) + (pow2gain >> 1);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_2[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_2[(index << 5) + 16]);
			}
			i >>= 2;
			gr_info->preflag = i & 1;
			i >>= 1;
			r1c = (i & 7) + 1;
			i >>= 3;
			i = (i & 15) + 1;
			gr_info->regions[0] = bandInfo[sfreq].longIdx[i] >> 1;
			gr_info->regions[1] = bandInfo[sfreq].longIdx[i + r1c] >> 1;
			gr_info->block_type = 0;
			gr_info->mixed_block_flag = 0;
		}
		III_correct_regions(gr_info);
		gr_info++;
		if(!(stereo & 2))
			gr_info++;
	} while(--grch);
	return MPXDEC_ERROR_OK;
}

static unsigned int III_get_side_info_2(struct gr_info_s *gr_info, int stereo, int ms_stereo, long sfreq)
{
	unsigned int gainpow2p = 256 + ((ms_stereo) ? 2 : 0);

	mpxdec_bitindex += stereo;

	do {
		register unsigned int i, pow2gain;
		i = getbits_24(21);
		gr_info->part2_3_length = i >> 9;	// 12 bits
		i &= 511;
		if(i > 288)
			return MPXDEC_ERROR_BITSTREAM;
		gr_info->regions[2] = i;	// 9 bits
		i = getbits_24(18);
		pow2gain = gainpow2p - (i >> 10);	// 8 bits
		gr_info->scalefac_compress = (i >> 1) & 511;	// 9 bits
		if(i & 1) {				// window-switching flag==1 for block_Type!=0 ; window-sw-flag = 0 for block_type==0
			i = getbits_24(13);
			gr_info->block_type = (i >> 11) & 3;	// 2 bits
			gr_info->mixed_block_flag = (i >> 10) & 1;	// 1 bit
			gr_info->table_select[0] = (i >> 5) & 31;	// 5 bits
			gr_info->table_select[1] = (i) & 31;	// 5 bits
			gr_info->table_select[2] = 0;
			i = getbits_24(11);
			gr_info->count1table_select = i & 1;
			if(i & 2) {			// gr_info->scalefac_scale
				unsigned int index = ((pow2gain & 3) << 7) + (pow2gain >> 2);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_4[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_4[(index << 5) + 16]);
				i >>= 1;
				gr_info->full_gain[2] = (i & 14);
				i >>= 3;
				gr_info->full_gain[1] = (i & 14);
				i >>= 3;
				gr_info->full_gain[0] = (i & 14);
			} else {
				unsigned int index = ((pow2gain & 1) << 8) + (pow2gain >> 1);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_2[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_2[(index << 5) + 16]);
				gr_info->full_gain[2] = (i & 28);
				i >>= 3;
				gr_info->full_gain[1] = (i & 28);
				i >>= 3;
				gr_info->full_gain[0] = (i & 28);
			}
			if(gr_info->block_type == 2) {
				gr_info->regions[0] = 36 >> 1;
				if(gr_info->mixed_block_flag)
					gr_info->mapp = map[sfreq][0];
				else
					gr_info->mapp = map[sfreq][1];
			} else {
				gr_info->mapp = map[sfreq][2];
				if(sfreq == 8)
					gr_info->regions[0] = 108 >> 1;
				else
					gr_info->regions[0] = 54 >> 1;
			}
			gr_info->regions[1] = (576 >> 1);
		} else {
			register unsigned int r1c;
			i = getbits_24(15);
			gr_info->table_select[2] = i & 31;
			i >>= 5;
			gr_info->table_select[1] = i & 31;
			i >>= 5;
			gr_info->table_select[0] = i & 31;
			i = getbits_24(9);
			gr_info->count1table_select = i & 1;
			if(i & 2) {			// gr_info->scalefac_scale
				unsigned int index = ((pow2gain & 3) << 7) + (pow2gain >> 2);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_4[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_4[(index << 5) + 16]);
			} else {
				unsigned int index = ((pow2gain & 1) << 8) + (pow2gain >> 1);
				gr_info->pow2gain = (&mpxdec_l3deq_gainpow2_2[index]);
				gr_info->ispow32_base = (&mpxdec_l3deq_ispow32_2[(index << 5) + 16]);
			}
			i >>= 2;
			r1c = (i & 7) + 1;
			i >>= 3;
			i = (i & 15) + 1;
			gr_info->regions[0] = bandInfo[sfreq].longIdx[i] >> 1;
			gr_info->regions[1] = bandInfo[sfreq].longIdx[i + r1c] >> 1;
			gr_info->mapp = map[sfreq][2];
			gr_info->block_type = 0;
			gr_info->mixed_block_flag = 0;
		}
		III_correct_regions(gr_info);
		gr_info++;
	} while(--stereo);
	return MPXDEC_ERROR_OK;
}

static void III_get_scale_factors_1(unsigned char *scf, struct gr_info_s *gr_info)
{
	static unsigned char slen[2][16] = {
		{0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4},
		{0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3}
	};
	unsigned int num0 = (unsigned int)slen[0][gr_info->scalefac_compress];
	unsigned int num1 = (unsigned int)slen[1][gr_info->scalefac_compress];

	if(gr_info->block_type == 2) {
		unsigned int i;

		i = 18 - gr_info->mixed_block_flag;
		do {
			*scf++ = getbits_8(num0);
		} while(--i);

		i = 18;
		do {
			*scf++ = getbits_8(num1);
		} while(--i);

		scf[1] = 0;
		scf[2] = 0;
	} else {
		unsigned int i;
		const unsigned int scfsi = gr_info->scfsi;

		i = 6;
		if(scfsi & 0x8)
			scf += i;
		else {
			do {
				*scf++ = getbits_8(num0);
			} while(--i);
		}

		i = 5;
		if(scfsi & 0x4)
			scf += i;
		else {
			do {
				*scf++ = getbits_8(num0);
			} while(--i);
		}

		if(gr_info->preflag) {
			register unsigned char *pretab = pretab0;

			i = 5;
			if(scfsi & 0x2) {
				do {			// maybe the preflag has changed but the scf doesn't
					scf[0] = scf[2 * 39] + *pretab++;	// we use the saved scf
					scf++;
				} while(--i);
			} else {
				do {
					unsigned int b = getbits_8(num1);	// scf changing
					scf[2 * 39] = b;	// we save the scf
					scf[0] = b + *pretab++;
					scf++;
				} while(--i);
			}
			i = 5;
			if(scfsi & 0x1) {
				do {
					scf[0] = scf[2 * 39] + *pretab++;
					scf++;
				} while(--i);
			} else {
				do {
					unsigned int b = getbits_8(num1);
					scf[2 * 39] = b;
					scf[0] = b + *pretab++;
					scf++;
				} while(--i);
			}
		} else {
			i = 5;
			if(scfsi & 0x2) {
				do {			// maybe the preflag has changed but the scf doesn't
					scf[0] = scf[2 * 39];	// we use the saved scf
					scf++;
				} while(--i);
			} else {
				do {			// scf changing
					scf[2 * 39] = scf[0] = getbits_8(num1);	// we save the scf
					scf++;
				} while(--i);
			}
			i = 5;
			if(scfsi & 0x1) {
				do {
					scf[0] = scf[2 * 39];
					scf++;
				} while(--i);
			} else {
				do {
					scf[2 * 39] = scf[0] = getbits_8(num1);
					scf++;
				} while(--i);
			}
		}
	}
	scf[0] = 0;
}

static void III_get_scale_factors_2(unsigned char *scf, struct gr_info_s *gr_info, int i_stereo)
{
	unsigned char *pnt, *pretab;
	unsigned int slen;
	int i, j, n;

	static unsigned char stab[3][6][4] = {
		{{6, 5, 5, 5}, {6, 5, 7, 3}, {11, 10, 0, 0},
		 {7, 7, 7, 0}, {6, 6, 6, 3}, {8, 8, 5, 0}},
		{{9, 9, 9, 9}, {9, 9, 12, 6}, {18, 18, 0, 0},
		 {12, 12, 12, 0}, {12, 9, 9, 6}, {15, 12, 9, 0}},
		{{6, 9, 9, 9}, {6, 9, 12, 6}, {15, 18, 0, 0},
		 {6, 15, 12, 0}, {6, 12, 9, 6}, {6, 18, 9, 0}}
	};

	if(i_stereo)
		slen = i_slen2[gr_info->scalefac_compress >> 1];
	else
		slen = n_slen2[gr_info->scalefac_compress];

	gr_info->preflag = (slen >> 15) & 0x1;
	pretab = (gr_info->preflag) ? pretab1 : pretab2;

	n = 0;
	if(gr_info->block_type == 2) {
		n++;
		if(gr_info->mixed_block_flag)
			n++;
	}

	pnt = stab[n][(slen >> 12) & 0x7];

	for(i = 0; i < 4; i++) {
		int num = slen & 0x7;
		slen >>= 3;
		if(num) {
			for(j = 0; j < (int)pnt[i]; j++)
				*scf++ = getbits_8(num) + (*pretab++);
		} else {
			for(j = 0; j < (int)pnt[i]; j++) {
				*scf++ = *pretab++;
			}
		}
	}
	n = (n << 1) + 1;
	for(i = 0; i < n; i++)
		*scf++ = *pretab++;
}

static void III_i_stereo(FLOAT_QD * xr_buf, unsigned char *scalefac, struct gr_info_s *gr_info, int sfreq, int ms_stereo, int lsf)
{
	FLOAT_QD(*xr)[DEQ_SBLIMIT * SSLIMIT] = (FLOAT_QD(*)[DEQ_SBLIMIT * SSLIMIT]) xr_buf;
	struct bandInfoStruct *bi = (struct bandInfoStruct *)&bandInfo[sfreq];
	FLOAT_QV *tab1, *tab2;

	if(lsf) {
		int p = gr_info->scalefac_compress & 0x1;
		if(ms_stereo) {
			tab1 = pow1_2[p];
			tab2 = pow2_2[p];
		} else {
			tab1 = pow1_1[p];
			tab2 = pow2_1[p];
		}
	} else {
		if(ms_stereo) {
			tab1 = tan1_2;
			tab2 = tan2_2;
		} else {
			tab1 = tan1_1;
			tab2 = tan2_1;
		}
	}

	if(gr_info->block_type == 2) {
		int lwin, do_l = 0;

		if(gr_info->mixed_block_flag)
			do_l = 1;

		for(lwin = 0; lwin < 3; lwin++) {
			int is_p, sb, idx, sfb = gr_info->maxband[lwin];

			if(sfb > 3)
				do_l = 0;
			for(; sfb < 12; sfb++) {
				is_p = (scalefac[sfb * 3 + lwin - gr_info->mixed_block_flag]);
				if(is_p != 7) {
					FLOAT_QV *t1 = &tab1[is_p];
					FLOAT_QV *t2 = &tab2[is_p];
					sb = bi->shortDiff[sfb];
					idx = bi->shortIdx[sfb] + lwin;
					for(; sb > 0; sb--, idx += 3) {
						MPXDEC_PUT_DEQD(&xr[1][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t2));
						MPXDEC_PUT_DEQD(&xr[0][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t1));
					}
				}
			}
			is_p = (scalefac[11 * 3 + lwin - gr_info->mixed_block_flag]);
			sb = bi->shortDiff[12];
			idx = bi->shortIdx[12] + lwin;
			if(is_p != 7) {
				FLOAT_QV *t1 = &tab1[is_p];
				FLOAT_QV *t2 = &tab2[is_p];
				for(; sb > 0; sb--, idx += 3) {
					MPXDEC_PUT_DEQD(&xr[1][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t2));
					MPXDEC_PUT_DEQD(&xr[0][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t1));
				}
			}
		}
		if(do_l) {
			int sfb = gr_info->maxbandl;
			int idx = bi->longIdx[sfb];

			for(; sfb < 8; sfb++) {
				int sb = bi->longDiff[sfb], is_p = scalefac[sfb];
				if(is_p != 7) {
					FLOAT_QV *t1 = &tab1[is_p];
					FLOAT_QV *t2 = &tab2[is_p];
					for(; sb > 0; sb--, idx++) {
						MPXDEC_PUT_DEQD(&xr[1][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t2));
						MPXDEC_PUT_DEQD(&xr[0][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t1));
					}
				} else
					idx += sb;
			}
		}
	} else {
		int sfb = 22, idx;
		unsigned char *pretab = (gr_info->preflag) ? pretab1 : pretab2;

		while(sfb >= 0 && (gr_info->maxbandl < bi->longIdx[sfb]))
			sfb--;
		sfb++;
		idx = bi->longIdx[sfb];
		scalefac[21] = scalefac[20];
		for(; sfb < 22; sfb++) {
			int is_p = (scalefac[sfb] - pretab[sfb]), sb = bi->longDiff[sfb];
			if(is_p != 7) {
				FLOAT_QV *t1 = &tab1[is_p];
				FLOAT_QV *t2 = &tab2[is_p];
				for(; sb; sb--, idx++) {
					MPXDEC_PUT_DEQD(&xr[1][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t2));
					MPXDEC_PUT_DEQD(&xr[0][idx], MPXDEC_GET_DEQD(&xr[0][idx]) * MPXDEC_GET_DEQV(t1));
				}
			} else
				idx += sb;
		}
	}
}

//--------------------------------------------------------------------------
#if defined(USE_80_MDCT1236) && !defined(USE_80_DEQD)
static void cv_f32_f80(float *src, mpxdec_float80_t * dest, unsigned int len)
{
	do {
		MPXDEC_PUT_HOUT(dest, *src);
		dest++;
		src++;
	} while(--len);
}
#endif

static void set_bsbuf_pointer(unsigned int backstep, struct mp3_decoder_data *mp3d)
{
	mpxdec_wordpointer = mp3d->bsbuf + 4 + mp3d->ssize - backstep;
	if(backstep)
		pds_memcpy(mpxdec_wordpointer, mp3d->bsbufold + mp3d->fsize_prev - backstep, backstep);
	mpxdec_bitindex = 0;
}

static unsigned int mpxdec_layer3_decode_part1(struct mp3_decoder_data *mp3d, unsigned int chan_mode)
{
	mp3d->gr_info = &(si[0][0]);
	{
		unsigned int main_data_begin;
		if(mp3d->lsf) {
			main_data_begin = getbits_8(8);
			if(main_data_begin && !mp3d->fsize_prev)
				return MPXDEC_ERROR_DATABEGIN;
			if(III_get_side_info_2(mp3d->gr_info, mp3d->filechannels, mp3d->ms_stereo, mp3d->frequency_index))
				return MPXDEC_ERROR_BITSTREAM;
		} else {
			main_data_begin = getbits_24(9);
			if(main_data_begin && !mp3d->fsize_prev)
				return MPXDEC_ERROR_DATABEGIN;
			if(III_get_side_info_1(mp3d->gr_info, mp3d->filechannels, mp3d->ms_stereo, mp3d->frequency_index))
				return MPXDEC_ERROR_BITSTREAM;
		}
		set_bsbuf_pointer(main_data_begin, mp3d);
	}
	for(mp3d->gr = 0; mp3d->gr < mp3d->granules; mp3d->gr++) {
		mp3d->part2begin = mpxdec_bitindex;
		if(mp3d->lsf)
			III_get_scale_factors_2(mp3d->scalefacs, mp3d->gr_info, 0);
		else
			III_get_scale_factors_1(mp3d->scalefacs, mp3d->gr_info);
		if(mp3d->gr_info->block_type == 2)
			mpxdec_l3deq_dequantize_bt2(mp3d->deq_outdata, mp3d->scalefacs, mp3d->gr_info, mp3d->part2begin);
		else
			mpxdec_l3deq_dequantize(mp3d->deq_outdata, mp3d->scalefacs, mp3d->gr_info, mp3d->part2begin);

		mp3d->gr_info++;
		if(mp3d->filechannels == 2) {
			mp3d->part2begin = mpxdec_bitindex;
			if(mp3d->lsf)
				III_get_scale_factors_2(&mp3d->scalefacs[39], mp3d->gr_info, mp3d->i_stereo);
			else
				III_get_scale_factors_1(&mp3d->scalefacs[39], mp3d->gr_info);
			if(chan_mode) {
				if(mp3d->gr_info->block_type == 2) {
					if(mp3d->ms_stereo) {
						if(mp3d->i_stereo)
							mpxdec_l3deq_dequantize_ms_bt2_js(mp3d->deq_outdata, &mp3d->scalefacs[39], mp3d->gr_info, mp3d->part2begin);
						else
							mpxdec_l3deq_dequantize_ms_bt2(mp3d->deq_outdata, &mp3d->scalefacs[39], mp3d->gr_info, mp3d->part2begin);
					} else {
						if(mp3d->i_stereo)
							mpxdec_l3deq_dequantize_bt2_js(&mp3d->deq_outdata[DEQ_SBLIMIT * SSLIMIT], &mp3d->scalefacs[39], mp3d->gr_info, mp3d->part2begin);
						else
							mpxdec_l3deq_dequantize_bt2(&mp3d->deq_outdata[DEQ_SBLIMIT * SSLIMIT], &mp3d->scalefacs[39], mp3d->gr_info, mp3d->part2begin);
					}
				} else {
					if(mp3d->ms_stereo)
						mpxdec_l3deq_dequantize_ms(mp3d->deq_outdata, &mp3d->scalefacs[39], mp3d->gr_info, mp3d->part2begin);
					else
						mpxdec_l3deq_dequantize(&mp3d->deq_outdata[DEQ_SBLIMIT * SSLIMIT], &mp3d->scalefacs[39], mp3d->gr_info, mp3d->part2begin);
				}
				if(mp3d->ms_stereo || mp3d->i_stereo || (chan_mode == CHM_DOWNMIX)) {
					if(mp3d->gr_info->maxb > (mp3d->gr_info - 1)->maxb)
						(mp3d->gr_info - 1)->maxb = mp3d->gr_info->maxb;
					else
						mp3d->gr_info->maxb = (mp3d->gr_info - 1)->maxb;
				}
				if(mp3d->i_stereo)
					III_i_stereo(mp3d->deq_outdata, &mp3d->scalefacs[39], mp3d->gr_info, mp3d->frequency_index, mp3d->ms_stereo, mp3d->lsf);

				//this is at wrong place (blocknum of the 2 channels can be different, but this is rare)
				if(chan_mode == CHM_DOWNMIX) {
					register FLOAT_QD *left = (FLOAT_QD *) mp3d->deq_outdata;
					register FLOAT_QD *right = (FLOAT_QD *) & mp3d->deq_outdata[DEQ_SBLIMIT * SSLIMIT];
					register int j = SSLIMIT * mp3d->gr_info->maxb;
					const float half = 0.5;
					do {
						MPXDEC_PUT_DEQD(&left[0], ((MPXDEC_GET_DEQD(&left[0]) + MPXDEC_GET_DEQD(&right[0])) * half));
						left++;
						right++;
						j--;
					} while(j > 0);
				}
			} else {
				mpxdec_bitindex = mp3d->part2begin + mp3d->gr_info->part2_3_length;
			}
		}
#if defined(USE_80_MDCT1236) && !defined(USE_80_DEQD)
		cv_f32_f80(mp3d->deq_outdata, mp3d->cv_deq_dct, 2 * DEQ_SBLIMIT * SSLIMIT);
#endif

		{
#if defined(USE_80_MDCT1236) && !defined(USE_80_DEQD)
			mpxdec_float80_t *deqp = mp3d->cv_deq_dct;
#else
			FLOAT_QD *deqp = mp3d->deq_outdata;
#endif
			hybridout_t *hout = mp3d->hybridp;
			block_t *blockp = mp3d->blockptr;
			struct gr_info_s *gr_info2 = mp3d->gr_info - 1;
			unsigned int ch = mp3d->outchannels;
			if(mp3d->gr)
				hout += 2 * SBLIMIT * SSLIMIT;

			do {
				mpxdec_l3mdct_hybrid(deqp, hout, blockp, gr_info2);
				deqp += DEQ_SBLIMIT * SSLIMIT;
				hout += SSLIMIT * SBLIMIT;
				blockp += (SBLIMIT * SSLIMIT + 1);
				gr_info2++;
			} while(--ch);
		}
		mp3d->gr_info++;
	}

	return MPXDEC_ERROR_OK;
}

//------------------------------------------------------------------------

#define EQ_FREQ_RANGE    22050
#define EQSET_BLOCKSIZE  16

#define FIR_BANDS        8
#define FIR_DELAY        6
#define EQ_TAP           (FIR_DELAY*2+1)
#define FIR_LEN          (FIR_DELAY*3+2)
#define EQ_BLOCK_SIZE    (SSLIMIT)
#define TONE_MDCTBANDS   (SBLIMIT)
#define EQ_MDCTSAVE_SIZE (FIR_LEN+EQ_BLOCK_SIZE)
#define EQ_SYNC_DELAY    3		// bass turn-on delay

#define EQSET_RESOLUTION (TONE_MDCTBANDS*EQSET_BLOCKSIZE)

unsigned int mpxdec_layer3_eq_init(struct mpxsynth_data_s *synthdata)
{
	synthdata->eq_mdct_save = calloc(MPXDEC_CHANNELS_MAX * FIR_BANDS * EQ_MDCTSAVE_SIZE, sizeof(hybridout_t));
	if(!synthdata->eq_mdct_save)
		return 0;
	synthdata->eq_sync_counter = 0;
	return 1;
}

void mpxdec_layer3_eq_close(struct mpxsynth_data_s *synthdata)
{
	if(synthdata->eq_mdct_save) {
		free(synthdata->eq_mdct_save);
		synthdata->eq_mdct_save = NULL;
	}
}

void mpxdec_layer3_eq_clear(struct mpxsynth_data_s *synthdata)
{
	pds_memset(synthdata->eq_mdct_save, 0, MPXDEC_CHANNELS_MAX * FIR_BANDS * EQ_MDCTSAVE_SIZE * sizeof(hybridout_t));
	synthdata->eq_sync_counter = 0;
}

static float EQ_gain[TONE_MDCTBANDS - FIR_BANDS];
static float EQ_Filter[FIR_BANDS][FIR_DELAY + 1];

static float set[EQSET_RESOLUTION];
static float mid[TONE_MDCTBANDS];

void mpxdec_layer3_eq_config(struct mpxsynth_data_s *synthdata)
{
	int i, n, k;
	long *eq_setpos;

	if(!synthdata->eq_bandnum) {
		synthdata->eq_sync_counter = 0;
		return;
	}

	eq_setpos = (long *)alloca(synthdata->eq_bandnum);
	if(!eq_setpos)
		return;

	for(i = 0; i < synthdata->eq_bandnum; i++)
		eq_setpos[i] = synthdata->eq_freqs[i] * EQSET_RESOLUTION / EQ_FREQ_RANGE;

	for(n = 0; n < eq_setpos[0]; n++)
		set[n] = synthdata->eq_powers[0];
	for(i = 1; i < synthdata->eq_bandnum; i++) {
		int bw = eq_setpos[i] - eq_setpos[i - 1];
		if(bw == 1)
			set[n++] = synthdata->eq_powers[i];
		else
			for(k = 0; k < bw; k++)
				set[n++] = (synthdata->eq_powers[i - 1] * (bw - k) + synthdata->eq_powers[i] * k) / (float)bw;
	}
	for(; n < EQSET_RESOLUTION; n++)
		set[n] = synthdata->eq_powers[synthdata->eq_bandnum - 1];

	pds_memset(mid, 0, TONE_MDCTBANDS * sizeof(float));
	for(k = FIR_BANDS * EQSET_BLOCKSIZE; k < EQSET_RESOLUTION; k++)
		mid[k / EQSET_BLOCKSIZE] += set[k];

	for(n = FIR_BANDS; n < TONE_MDCTBANDS; n++)
		EQ_gain[n - FIR_BANDS] = mid[n] / (float)EQSET_BLOCKSIZE;

	for(i = 0; i < FIR_BANDS; i++) {
		for(n = 0; n <= FIR_DELAY; n++) {
			double xn = 0.0, win;
			for(k = 0; k < EQSET_BLOCKSIZE; k++) {
				int idx = (i & 1) ? ((i << 4) + (EQSET_BLOCKSIZE - 1) - k) : ((i << 4) + k);
				//int idx = ((i<<4)+k);
				xn += set[idx] * cos(n * (k + 0.5) * M_PI / EQSET_BLOCKSIZE);
			}
			xn = xn / (double)EQSET_BLOCKSIZE;
			//win=cos(n*M_PI/2.0/(double)(FIR_DELAY+1));
			win = cos(n * M_PI / (double)(EQ_TAP));
			win *= win;
			EQ_Filter[i][n] = xn * win;
		}
	}
}

static void layer3_eq_set(struct mpxsynth_data_s *synthdata, hybridout_t * hout)
{
	int ch, i, k, n;
	float *eqfi;
	hybridout_t *mdct;

	if(!synthdata->eq_bandnum) {
		synthdata->eq_sync_counter = 0;
		return;
	}

	if(synthdata->eq_sync_counter < EQ_SYNC_DELAY)
		synthdata->eq_sync_counter++;

	mdct = synthdata->eq_mdct_save;

	for(ch = 0; ch < synthdata->outchannels; ch++) {
		eqfi = &EQ_Filter[0][0];

		for(i = 0; i < FIR_BANDS; i++, eqfi += FIR_DELAY + 1, mdct += EQ_MDCTSAVE_SIZE) {
			for(k = 0; k < EQ_BLOCK_SIZE; k++)
				MPXDEC_PUT_HOUT(&mdct[FIR_LEN + k], MPXDEC_GET_HOUT(&hout[k * SBLIMIT + i]));

			if(synthdata->eq_sync_counter >= EQ_SYNC_DELAY) {
				hybridout_t *mk = &mdct[FIR_DELAY];
				for(k = 0; k < EQ_BLOCK_SIZE; k++, mk++) {
					hybridout_t *mnp = mk, *mnn = mk;
					float *e = eqfi;
					float outdata = MPXDEC_GET_HOUT(&mnp[0]) * e[0];
					n = FIR_DELAY;
					do {
						outdata += (MPXDEC_GET_HOUT(++mnp) + MPXDEC_GET_HOUT(--mnn)) * *(++e);
					} while(--n);
					MPXDEC_PUT_HOUT(&hout[k * SBLIMIT + i], outdata);
				}
			}

			for(n = 0; n < FIR_LEN; n++)
				MPXDEC_PUT_HOUT(&mdct[n], MPXDEC_GET_HOUT(&mdct[EQ_BLOCK_SIZE + n]));
		}

		for(i = FIR_BANDS; i < TONE_MDCTBANDS; i++) {
			const float gain = EQ_gain[i - FIR_BANDS];
			if(gain != 1.0f) {
				for(k = 0; k < EQ_BLOCK_SIZE; k++)
					MPXDEC_PUT_HOUT(&hout[k * SBLIMIT + i], MPXDEC_GET_HOUT(&hout[k * SBLIMIT + i]) * gain);
			}
		}

		hout += EQ_BLOCK_SIZE * TONE_MDCTBANDS;
	}
}

//-------------------------------------------------------------------------

struct mpxsynth_data_s *mpxdec_layer3_synth_alloc(hybridout_t * hybridp)
{
	struct mpxsynth_data_s *synthdata;

	synthdata = (struct mpxsynth_data_s *)calloc(1, sizeof(struct mpxsynth_data_s));
	if(!synthdata)
		return synthdata;
	synthdata->synth_rollbuff = calloc(MPXDEC_CHANNELS_MAX * 2 * 0x110, sizeof(*synthdata->synth_rollbuff));
	if(!synthdata->synth_rollbuff)
		goto err_out_synthalloc;
#ifdef USE_80_SYNTH
	synthdata->synth_bo = 1;
#else
	synthdata->synth_bo = 4;
#endif
	synthdata->hybridp = hybridp;
	if(!mpxdec_layer3_eq_init(synthdata))
		goto err_out_synthalloc;

	return synthdata;

  err_out_synthalloc:
	mpxdec_layer3_synth_close(synthdata);
	return NULL;
}

void mpxdec_layer3_synth_close(struct mpxsynth_data_s *synthdata)
{
	if(synthdata) {
		if(synthdata->synth_rollbuff)
			free(synthdata->synth_rollbuff);
		mpxdec_layer3_eq_close(synthdata);
		free(synthdata);
	}
}

void mpxdec_layer3_synth_clear(struct mpxsynth_data_s *synthdata)
{
	pds_memset(synthdata->synth_rollbuff, 0, MPXDEC_CHANNELS_MAX * 2 * 0x110 * sizeof(*synthdata->synth_rollbuff));
#ifdef USE_80_SYNTH
	synthdata->synth_bo = 1;
#else
	synthdata->synth_bo = 4;
#endif
	mpxdec_layer3_eq_clear(synthdata);
}

//-----------------------------------------------------------------------

void asm_mpx_analisercalc(hybridout_t *, unsigned int, unsigned int, unsigned long *, long *);

static void mpx_analisercalc(hybridout_t * hybgr, unsigned int channels, unsigned int bandnum, unsigned long *banddata)
{
	long tmp;
#ifdef __WATCOMC__
#ifdef USE_80_HYBRIDOUT
#pragma aux asm_mpx_analisercalc=\
  "dec ecx"\
  "mov dword ptr [ebx],6800"\
  "fild dword ptr [ebx]"\
  "Lsb:mov dh,cl"\
   "inc dh"\
   "fldz"\
   "Lch:mov ch,18"\
    "LLss:"\
     "fld tbyte ptr [eax]"\
     "add eax,320"\
     "fabs"\
     "dec ch"\
     "fadd"\
    "jnz LLss"\
    "dec dh"\
   "jnz Lch"\
   "fmul st,st(1)"\
   "shr dword ptr [esi],1"\
   "mov ebx,576"\
   "fiadd dword ptr [esi]"\
   "shl ebx,cl"\
   "dec ebx"\
   "mov edi,ebx"\
   "shl ebx,3"\
   "shl edi,1"\
   "add ebx,edi"\
   "fistp dword ptr [esi]"\
   "sub eax,ebx"\
   "add esi,4"\
   "dec dl"\
  "jnz Lsb"\
  "fstp st(0)"\
  parm[eax][ecx][edx][esi][ebx] modify [eax ebx ecx edx edi esi];
#else
#pragma aux asm_mpx_analisercalc=\
  "dec ecx"\
  "mov dword ptr [ebx],6800"\
  "fild dword ptr [ebx]"\
  "Lsb:mov dh,cl"\
   "inc dh"\
   "fldz"\
   "Lch:mov ch,18"\
    "LLss:"\
     "fld dword ptr [eax]"\
     "add eax,128"\
     "fabs"\
     "dec ch"\
     "fadd"\
    "jnz LLss"\
    "dec dh"\
   "jnz Lch"\
   "fmul st,st(1)"\
   "shr dword ptr [esi],1"\
   "mov ebx,576"\
   "fiadd dword ptr [esi]"\
   "shl ebx,cl"\
   "dec ebx"\
   "shl ebx,2"\
   "fistp dword ptr [esi]"\
   "sub eax,ebx"\
   "add esi,4"\
   "dec dl"\
  "jnz Lsb"\
  "fstp st(0)"\
  parm[eax][ecx][edx][esi][ebx] modify [eax ebx ecx edx esi];
#endif
	asm_mpx_analisercalc(hybgr, channels, bandnum, banddata, &tmp);
#endif							// __WATCOMC__
}

#ifndef USE_80_SYNTH
#define SYNTH_FPUC 1			// chop
#endif

#ifdef SYNTH_FPUC
void asm_fpusetround_chop(void);
void asm_fpusetround_near(void);
#endif

void mpxdec_decode_part2(struct mpxsynth_data_s *synthdata, unsigned char *pcm_mpxout_p)
{
#ifdef MPXDEC_INTEGER_OUTPUT
	short *pcm_point = (short *)pcm_mpxout_p;
#else
	float *pcm_point = (float *)pcm_mpxout_p;
#endif
	hybridout_t *hybgr = synthdata->hybridp;
	unsigned int granules = synthdata->granules;
#ifdef SYNTH_FPUC
	int tmp;
#pragma aux asm_fpusetround_chop=\
  "fstcw word ptr tmp"\
  "or word ptr tmp,0x0c00"\
  "fldcw word ptr tmp"\
  modify[];
	asm_fpusetround_chop();
#endif

	do {

		if(synthdata->eq_bandnum)
			layer3_eq_set(synthdata, hybgr);

		if(synthdata->analiser_bandnum && synthdata->analiser_banddata)
			mpx_analisercalc(hybgr, synthdata->outchannels, synthdata->analiser_bandnum, synthdata->analiser_banddata);

		pcm_point = mpxdec_synth_granule(hybgr, (synthdata->synth_bo << 8) | synthdata->outchannels, pcm_point, synthdata->synth_rollbuff);
#ifdef USE_80_SYNTH
		synthdata->synth_bo = (synthdata->synth_bo - SSLIMIT) & 15;
#else
		synthdata->synth_bo = (synthdata->synth_bo - SSLIMIT * sizeof(float)) & (15 * sizeof(float));
#endif

		hybgr += 2 * SBLIMIT * SSLIMIT;
	} while(--granules);

#ifdef SYNTH_FPUC
#pragma aux asm_fpusetround_near=\
  "fstcw word ptr tmp"\
  "and word ptr tmp,0xf3ff"\
  "fldcw word ptr tmp"\
  modify[];
	asm_fpusetround_near();
#endif
}
