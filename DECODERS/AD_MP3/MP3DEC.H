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
//function:mp2,mp3 decoder definitions

#ifndef mp3dec_h
#define mp3dec_h

#define MPXDEC_CHANNELS_MAX  2
#define MPXDEC_GRANULES_MAX  2
//#define MPXDEC_INTEGER_OUTPUT 1 // with synthio.asm

#define SBLIMIT 32
#define SSLIMIT 18
#define DEQ_SBLIMIT (SBLIMIT)

#define MPXDEC_FRAMEBUFFER_SIZE 4096 // 2*4096 (fsize+fsizeold)
#define MPXDEC_BACKSTEP_MAX      512 // 2^9 (main_data_begin)

#define MPG_MD_STEREO       0
#define MPG_MD_JOINT_STEREO 1
#define MPG_MD_DUALCHAN     2
#define MPG_MD_MONO         3

#define MPXDEC_SKIP_BITSTREAM_BUGS 1
//#define MPXDEC_ENABLE_CRC_CHECK  1
//#define MPXDEC_ENABLE_FREEFORMAT 1 // bad

#ifdef MPXDEC_ENABLE_FREEFORMAT
 #define MPXDEC_FRAMESIZE_MAX  (MPXDEC_FRAMEBUFFER_SIZE-MPXDEC_BACKSTEP_MAX-16) // framebuffer_size - max_backstep - protection
 #define MPXDEC_HEADMASK_FREEFORMAT 0xfffffc03
#else
 #define MPXDEC_FRAMESIZE_MAX  1792
#endif

#define MPXDEC_HEADMASK_STANDARD 0xffff0c03
#define MPXDEC_HEADMASK_SYNC     0xffe00000

//decoding error codes
#define MPXDEC_ERROR_OK        0 // have to be zero! (others have to be non-zero)
#define MPXDEC_ERROR_NODATA    1 // eof
#define MPXDEC_ERROR_BITSTREAM 2 // hole or bad data/value
#define MPXDEC_ERROR_DATABEGIN 3 // main_data_begin

//mp3_decoder_info->infobits
#define MP3DI_INFOBIT_FREEFORMAT      1

#ifndef M_PI
 #define M_PI    3.14159265358979323846264338327950288
#endif
#ifndef M_SQRT2
 #define M_SQRT2 1.41421356237
#endif

#ifdef __WATCOMC__
typedef char mpxdec_float80_t[10];
#else
typedef long double mpxdec_float80_t;
#endif

//#define USE_80_ALL 1   // 80-bit l3_deq, mdct, synth

#ifdef USE_80_ALL
 #define USE_80_DEQV 1
 #define USE_80_DEQD 1
 #define USE_80_MDCT1236  1
 #define USE_80_HYBRIDOUT 1
 #define USE_80_SYNTH     1
#endif

#ifdef USE_80_DEQV
 #ifdef __WATCOMC__
  typedef char FLOAT_QV[10];
 #else
  typedef long double FLOAT_QV;
 #endif
#else
 typedef float FLOAT_QV;
#endif

#ifdef USE_80_DEQD
 #ifdef __WATCOMC__
  typedef char FLOAT_QD[10];
 #else
  typedef long double FLOAT_QD;
 #endif
#else
 typedef float FLOAT_QD;
#endif

#ifdef USE_80_MDCT1236
 typedef char block_t[10];
#else
 typedef float block_t;
#endif

#ifdef USE_80_HYBRIDOUT
 #ifdef __WATCOMC__
  typedef char hybridout_t[10];
 #else
  typedef long double hybridout_t;
 #endif
#else
 typedef float hybridout_t;
#endif

#ifdef USE_80_SYNTH
 typedef char synth_rollb_t[10];
#else
 typedef float synth_rollb_t;
#endif

void f80bit_put_asm(mpxdec_float80_t *ptr,float val);
#pragma aux f80bit_put_asm parm[esi][8087] = "fstp tbyte ptr [esi]"
float f80bit_get_asm(mpxdec_float80_t *ptr);
#pragma aux f80bit_get_asm parm[esi] value[8087] = "fld tbyte ptr [esi]"
#define f80bit_put_c(p,v) *(p)=(v)
#define f80bit_get_c(p)   (*(p))

#if defined(USE_80_DEQV) && defined(__WATCOMC__)
 #define MPXDEC_PUT_DEQV f80bit_put_asm
 #define MPXDEC_GET_DEQV f80bit_get_asm
#else
 #define MPXDEC_PUT_DEQV f80bit_put_c
 #define MPXDEC_GET_DEQV f80bit_get_c
#endif

#if defined(USE_80_DEQD) && defined(__WATCOMC__)
 #define MPXDEC_PUT_DEQD f80bit_put_asm
 #define MPXDEC_GET_DEQD f80bit_get_asm
#else
 #define MPXDEC_PUT_DEQD f80bit_put_c
 #define MPXDEC_GET_DEQD f80bit_get_c
#endif

#if defined(USE_80_HYBRIDOUT) && defined(__WATCOMC__)
 #define MPXDEC_PUT_HOUT f80bit_put_asm
 #define MPXDEC_GET_HOUT f80bit_get_asm
#else
 #define MPXDEC_PUT_HOUT f80bit_put_c
 #define MPXDEC_GET_HOUT f80bit_get_c
#endif

typedef struct
{
 short bits;
 short d;
}al_table;

typedef struct mpxsynth_data_s{
 hybridout_t *hybridp;          // mp3i->hybridp
 unsigned int granules;
 unsigned int outchannels;
 synth_rollb_t *synth_rollbuff; // [MPXDEC_CHANNELS_MAX][2][256+16]
 unsigned int synth_bo;         // block-offset of rollbuff

 unsigned long analiser_bandnum;
 unsigned long *analiser_banddata;

 unsigned int eq_bandnum;
 unsigned long *eq_freqs;
 float *eq_powers;

 unsigned int eq_sync_counter;
 hybridout_t *eq_mdct_save;     // [MPXDEC_CHANNELS_MAX][FIR_BANDS][EQ_MDCTSAVE_SIZE]
}mpxsynth_data_s;

typedef struct gr_info_s{
 int scfsi;
 unsigned int part2_3_length;
 unsigned int scalefac_compress;
 unsigned int block_type;
 unsigned int mixed_block_flag;
 unsigned int table_select[3];
 unsigned int subblock_gain[3];
 unsigned int maxband[3];
 unsigned int maxbandl;
 unsigned int maxb;
 unsigned int regions[4];
 unsigned int preflag;
 unsigned int scalefac_scale;
 unsigned int count1table_select;
 unsigned int full_gain[3];
 FLOAT_QV *pow2gain;
 FLOAT_QV *ispow32_base;
 int *mapp;
}gr_info_s;

typedef struct mp3_decoder_data{
 unsigned char granules;
 unsigned char lay;
 unsigned char filechannels;
 unsigned char lsf;

 unsigned char bitrate_index;
 unsigned char frequency_index;
 unsigned char mpg_chmode;
 unsigned char mpg_chmode_ext;

 unsigned int  ssize;
 unsigned int  fsize_curr;
 unsigned int  fsize_prev;
 unsigned int  fsize_next;
 unsigned int  bsnum;

#if defined(MPXDEC_ENABLE_FREEFORMAT) || defined(MPXDEC_ENABLE_CRC_CHECK)
 unsigned long newhead;
#endif
 unsigned long firsthead;
 unsigned char *bsbuf;
 unsigned char *bsbufold;
 unsigned int  freq;

 unsigned int  infobits;

 unsigned char error_protection;
 unsigned char padding;
 unsigned char ms_stereo;
 unsigned char i_stereo;

 unsigned short bitrate;
 unsigned char outchannels;
 unsigned char gr;
 unsigned int  part2begin;

 //dynamic data fields (allocated to every file separately)
 unsigned char *bitstream_a; // alloc ptr [2][MPXDEC_FRAMEBUFFER_SIZE]
 unsigned char *bitstreamp;  // work ptr
 block_t *block_a;        // alloc-ptr and block-select-switch (blc)
 block_t *blockptr;       // [2][MPXDEC_CHANNELS_MAX][SBLIMIT][SSLIMIT]
 struct mpxsynth_data_s *synthdata;

 //static (temp) data fields (all files use the same memory field(s))
 struct gr_info_s *gr_info;
 FLOAT_QD *deq_outdata;

#if defined(USE_80_MDCT1236) && !defined(USE_80_DEQD)
 mpxdec_float80_t *cv_deq_dct;
#endif

 unsigned char *scalefacs;
 hybridout_t *hybridp; // [MPXDEC_GRANULES_MAX][MPXDEC_CHANNELS_MAX][SSLIMIT][SBLIMIT]

 unsigned int II_sblimit;
 unsigned int II_jsbound;
 al_table    *II_alloc;

}mp3_decoder_data;

// API  (in layer3.c)
extern void mpxdec_preinit(void);
extern struct mp3_decoder_data *mpxdec_init(void);
extern void mpxdec_close(struct mp3_decoder_data *mp3d);
extern void mpxdec_reset_bitstream(struct mp3_decoder_data *mp3d);
extern void mpxdec_reset_decoding(struct mp3_decoder_data *mp3d);
extern void mpxdec_reset_synth(struct mp3_decoder_data *mp3d);
extern int  mpxdec_syncinfo(struct mp3_decoder_data *mp3d,unsigned char *bsbuf);
extern int  mpxdec_read_frame(struct mp3_decoder_data *mp3d,unsigned char *bsbuf);
extern int  mpxdec_decode_part1(struct mp3_decoder_data *mp3d,unsigned int chan_mode);

// layer2.c
extern void mpxdec_layer2_init(void);
extern void mpxdec_layer2_getstuff(struct mp3_decoder_data *);
extern void mpxdec_layer2_decode_part1(struct mp3_decoder_data *);

// common mp2,mp3,mpc
extern struct mpxsynth_data_s *mpxdec_layer3_synth_alloc(hybridout_t *hybridp);
extern void mpxdec_layer3_synth_close(struct mpxsynth_data_s *synthdata);
extern void mpxdec_layer3_synth_clear(struct mpxsynth_data_s *synthdata);
extern void mpxdec_decode_part2(struct mpxsynth_data_s *synthdata,unsigned char *pcm_mpxout_p);

extern unsigned int mpxdec_layer3_eq_init(struct mpxsynth_data_s *synthdata);
extern void mpxdec_layer3_eq_close(struct mpxsynth_data_s *synthdata);
extern void mpxdec_layer3_eq_clear(struct mpxsynth_data_s *synthdata);
extern void mpxdec_layer3_eq_config(struct mpxsynth_data_s *synthdata);

//l3_deq.asm
extern void mpxdec_l3deq_init(void);
extern void mpxdec_l3deq_dequantize(FLOAT_QD *,unsigned char *,struct gr_info_s *,int);
extern void mpxdec_l3deq_dequantize_ms(FLOAT_QD *,unsigned char *,struct gr_info_s *,int);
extern void mpxdec_l3deq_dequantize_bt2(FLOAT_QD *,unsigned char *,struct gr_info_s *,int);
extern void mpxdec_l3deq_dequantize_ms_bt2(FLOAT_QD *,unsigned char *,struct gr_info_s *,int);
extern void mpxdec_l3deq_dequantize_bt2_js(FLOAT_QD *,unsigned char *,struct gr_info_s *,int);
extern void mpxdec_l3deq_dequantize_ms_bt2_js(FLOAT_QD *,unsigned char *,struct gr_info_s *,int);

//l3_mdct.asm
extern void mpxdec_l3mdct_init(void);
#if defined(USE_80_MDCT1236)
extern void mpxdec_l3mdct_hybrid(mpxdec_float80_t *,hybridout_t *,block_t *,struct gr_info_s *);
#else
extern void mpxdec_l3mdct_hybrid(FLOAT_QD *,hybridout_t *,block_t *,struct gr_info_s *);
#endif

//synth.asm
#ifdef MPXDEC_INTEGER_OUTPUT
extern short *mpxdec_synth_granule(hybridout_t *bandPtr,unsigned int bo8_channels,short *pcm_out,void *synth_rollbuff);
#else
extern float *mpxdec_synth_granule(hybridout_t *bandPtr,unsigned int bo8_channels,float *pcm_out,void *synth_rollbuff);
#endif
extern void mpxdec_synth_init(int scale);

#endif // mp3dec_h
