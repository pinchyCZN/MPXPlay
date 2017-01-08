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
//function: definitions

#ifndef mpxplay_h
#define mpxplay_h

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <conio.h>
#include <io.h>
#include <math.h>
#include <dos.h>

#ifndef MPXPLAY
#define MPXPLAY 1
#endif

#if defined(WIN32) || defined(__WINDOWS_386__) || defined(__NT__)
#define MPXPLAY_WIN32 1
#define _WIN32_WINDOWS 0x0500
#include <windows.h>
#endif

#ifdef _WINDOWS_

#else
#define MPXPLAY_LINK_FULL     1
#endif

#ifdef __DOS__
 //#define MPXPLAY_LINK_SWSCK32  1    // with SWSCK32 lib
 //#define MPXPLAY_LINK_WATTCP32 1    // with WATTCP32 lib
#endif

#if defined(MPXPLAY_WIN32) || defined(MPXPLAY_LINK_SWSCK32) || defined(MPXPLAY_LINK_WATTCP32)
#define MPXPLAY_LINK_TCPIP 1
#endif
//#define USE_LSA_SCV 1            // show/use soundcard volume on desktop (instead of software volume)
//#define MPXPLAY_IS_UTF8          // not implemented yet (later at GUI)

// out pcm defs
#define PCM_OUTSAMPLES    1152	// at 44100Hz
#define PCM_MIN_CHANNELS  1
#ifdef MPXPLAY_WIN32
#define PCM_MAX_CHANNELS  8		// au_mixer output (au_card input) limit
#else
#define PCM_MAX_CHANNELS  2		// au_mixer output (au_card input) limit
#endif
#define PCM_MIN_BITS      1
#define PCM_MAX_BITS      32
#define PCM_MIN_FREQ      512
#define PCM_MAX_FREQ      192000	// program can play higher freq too
#define PCM_MAX_SAMPLES   (((PCM_OUTSAMPLES*PCM_MAX_FREQ)+22050)/44100*PCM_MAX_CHANNELS)	// only the pcm buffer is limited (in one frame)
#define PCM_MAX_BYTES     (PCM_MAX_SAMPLES*(PCM_MAX_BITS/8))	// in one frame
#define PCM_BUFFER_SIZE   (2*PCM_MAX_BYTES)	// *2 : speed control expansion

// outmodes
#define OUTMODE_TYPE_NULL      0	// ie:-is,-iw
#define OUTMODE_TYPE_TEST      1	// testmode (-t)
#define OUTMODE_TYPE_AUDIO     2	// audio mode
#define OUTMODE_TYPE_FILE      4	// write output into file (-o)
#define OUTMODE_TYPE_MASK      (OUTMODE_TYPE_NULL|OUTMODE_TYPE_TEST|OUTMODE_TYPE_AUDIO|OUTMODE_TYPE_FILE)
#define OUTMODE_CONTROL_FILE_BITSTREAMOUT 16	// -obs
#define OUTMODE_CONTROL_FILE_FLOATOUT     32	// -obf
#define OUTMODE_CONTROL_FILE_TAGLFN       64	// -oni
//#define OUTMODE_CONTROL_MASK (OUTMODE_CONTROL_FILE_BITSTREAMOUT|OUTMODE_CONTROL_FILE_FLOATOUT|OUTMODE_CONTROL_FILE_TAGLFN)

//adone values
#define ADONE_EOF     1			// end of file
#define ADONE_RESTART 2			// open new file

//playcontrol flags (status and control)
#define PLAYC_PAUSEALL         1	// -ppa (pause before each song)
#define PLAYC_HIGHSCAN         2	// -phs
#define PLAYC_PAUSENEXT        4	// -ppn (pause at first/next song)
#define PLAYC_AUTOGFX          8	// switch to video mode if file has video stream
#define PLAYC_CONTINUOUS_SEEK 16	//
#define PLAYC_STARTNEXT       32	// start playing at next song
#define PLAYC_RUNNING         64	// playing is running
#define PLAYC_BEGINOFSONG    128	// begin of song (while no sound)
#define PLAYC_ABORTNEXT      256	// skip to next song is aborted
#define PLAYC_EXITENDLIST    512	// exit at end of list
#define PLAYC_SAVEMASK       (PLAYC_PAUSEALL|PLAYC_PAUSENEXT|PLAYC_AUTOGFX)

//hi-lite scan defaults
#define PLAYC_HS_STARTSONG  1
#define PLAYC_HS_STARTTIME  "1:00"
#define PLAYC_HS_TIMECOUNT  "0:10"

//playreplay values
#define REPLAY_SONG   1
#define REPLAY_LIST   2

//crossfadepart values
#define CROSS_CLEAR   0
#define CROSS_OUT     1			// fade-out part running
#define CROSS_LOAD    5			// new file loading
#define CROSS_FADE    3			// crossfade
#define CROSS_IN      2			// fade-in part running

//crossfadetype values
#define CFT_FADEOUT   1			// fade out enabled
#define CFT_FADEIN    2			// fade out disabled

//intsoundconfig and intsoundcontrol function bits
#define INTSOUND_NONE      0	// no interrupt functions
#define INTSOUND_DECODER   1	// interrupt decoder
#define INTSOUND_TSR       2	// tsr mode
#define INTSOUND_FUNCTIONS (INTSOUND_DECODER|INTSOUND_TSR)
#define INTSOUND_NOINTDEC  4	// disable interrupt decoder (intsoundconfig)
#define INTSOUND_NOINT08   8	// disable int08 (intsoundconfig)
#define INTSOUND_DOSSHELL  16	// intsoundconfig:-xs intsoundcontrol:ctrl-d
#define INTSOUND_INT08RUN  1024	// int08 process is running (has not finished)

//timer settings
#define MPXPLAY_TIMER_INT      0x08
#define INT08_DIVISOR_DEFAULT  65536
#define INT08_CYCLES_DEFAULT   (1000.0/55.0)	// 18.181818
#ifdef __DOS__
#define INT08_DIVISOR_NEW      10375	// = 18.181818*65536 / (3 * 44100/1152)
//#define INT08_DIVISOR_NEW      1194   // for 1ms (1/1000 sec) refresh
#else
#define INT08_DIVISOR_NEW      20750	// 60fps (XP requires this low value)
#endif
#define INT08_CYCLES_NEW       (INT08_CYCLES_DEFAULT*INT08_DIVISOR_DEFAULT/INT08_DIVISOR_NEW)	// = 114.8

#define REFRESH_DELAY_JOYMOUSE (INT08_CYCLES_NEW/36)	// 38 char/s

#ifdef MPXPLAY_WIN32
#define MPXPLAY_INTSOUNDDECODER_DISALLOW \
  newfunc_newhandler08_waitfor_threadend();\
  InterlockedExchange((LONG *)&intsoundcntrl_save,(LONG)intsoundcontrol);\
  funcbit_smp_disable(intsoundcontrol,INTSOUND_DECODER);
#define MPXPLAY_INTSOUNDDECODER_ALLOW \
  if(funcbit_smp_test(intsoundconfig,INTSOUND_DECODER)) \
   funcbit_smp_copy(intsoundcontrol,intsoundcntrl_save,INTSOUND_DECODER);
#else
#define MPXPLAY_INTSOUNDDECODER_DISALLOW intsoundcntrl_save=intsoundcontrol;funcbit_disable(intsoundcontrol,INTSOUND_DECODER);
#define MPXPLAY_INTSOUNDDECODER_ALLOW    if(intsoundconfig&INTSOUND_DECODER) funcbit_copy(intsoundcontrol,intsoundcntrl_save,INTSOUND_DECODER);
#endif

//Mpxplay error codes
#define MPXERROR_SNDCARD           8
#define MPXERROR_XMS_MEM           9
#define MPXERROR_CONVENTIONAL_MEM 10
#define MPXERROR_NOFILE           11
#define MPXERROR_CANTWRITEFILE    12
#define MPXERROR_DIV0             13
#define MPXERROR_EXCEPTION        14

#define MPXPLAY_ERROR_MPXINBUF_OK          0	// no error
#define MPXPLAY_ERROR_MPXINBUF_READ_EOF -100	// can't read, because EOF
#define MPXPLAY_ERROR_MPXINBUF_READ_BUF -101	// can't read, because no data left in buffer
#define MPXPLAY_ERROR_MPXINBUF_READ_LOW -102	// can't read from file-device (but not eof yet)
#define MPXPLAY_ERROR_MPXINBUF_SEEK_EOF -110	// invalid newfilepos (out of filesize)
#define MPXPLAY_ERROR_MPXINBUF_SEEK_BUF -111	// newfilepos is out of buffer
#define MPXPLAY_ERROR_MPXINBUF_SEEK_LOW -112	// can't seek file-device

//dosshell types
#define DSH_NO        0
#define DSH_CHILD     1
#define DSH_NEWSHELL  2
#define DSH_START     3

//videocontrol
#define MPXPLAY_VIDEOCONTROL_DECODEVIDEO 1

//other
#define IRQ_STACK_SIZE 16384	// size of irq (errorhand and soundcard) stacks

//wave (codec) IDs at input/output
#define MPXPLAY_WAVEID_UNKNOWN   0x0000
#define MPXPLAY_WAVEID_PCM_SLE   0x0001	// signed little endian
#define MPXPLAY_WAVEID_PCM_FLOAT 0x0003	// 32-bit float le
#define MPXPLAY_WAVEID_AAC       0x706D
#define MPXPLAY_WAVEID_AC3       0x2000
#define MPXPLAY_WAVEID_DTS       0x2001
#define MPXPLAY_WAVEID_MP2       0x0050
#define MPXPLAY_WAVEID_MP3       0x0055
#define MPXPLAY_WAVEID_WMAV1     0x0160	// 7.0
#define MPXPLAY_WAVEID_WMAV2     0x0161	// 8.0
#define MPXPLAY_WAVEID_WMAV3     0x0162	// 9.0 (not implemented)
#define MPXPLAY_WAVEID_WMAV4     0x0163	// lossless (not implemented)
#define MPXPLAY_WAVEID_FLAC      0xF1AC
// non-standard (internal) wave-ids
#define MPXPLAY_WAVEID_PCM_SBE   0x00017001	// signed big endian pcm
#define MPXPLAY_WAVEID_PCM_F32BE 0x00017003	// 32-bit float big endian
#define MPXPLAY_WAVEID_PCM_F64LE 0x00017004	// 64-bit float little endian
#define MPXPLAY_WAVEID_PCM_F64BE 0x00017005	// 64-bit float big endian
#define MPXPLAY_WAVEID_VORBIS    0x00018000	// ??? Vorbis has 3 official IDs
#define MPXPLAY_WAVEID_SPEEX     0x00018002
#define MPXPLAY_WAVEID_ALAC      0x00018005

//------------------------------------------------------------------------
//main structure definitions

#include "au_cards\au_cards.h"	// aui
#include "display\visualpi.h"	// display_visual_data_s
#include "in_file.h"			// frp
#include "newfunc\newfunc.h"	// dosmem_t
#include "playlist\playlist.h"	// psi,pei
#include "videoout\videoout.h"	// mpxplay_videout_info_s

typedef struct crossfade_info {
	unsigned int usecrossfade;
	unsigned int crossfadepart;
	unsigned int crossfade_out_len;
	unsigned int crossfade_in_len;
	unsigned int crossfadepoint;
	unsigned int crossfadetype;
	unsigned int crossfadebegin;
	unsigned int crossfadeend;
	unsigned int crossfadelimit;
	unsigned int crosscounter;	// counter of crosswait
	unsigned int crosswait;		// don't start crossfade (ie: at rewind)
} crossfade_info;

typedef struct mainvars {
	struct frame *frp0;
	struct mpxplay_audioout_info_s *aui;
	struct mpxplay_videoout_info_s *voi;

	struct playlist_side_info *psi0;	// side 0 info
	struct playlist_side_info *psie;	// editorside info
	struct playlist_side_info *psip;	// playside info
	struct playlist_side_info *psil;	// playlist side info

	struct playlist_entry_info *pei0;	// 0. side 0. entry (base entry and pointers of current song)
	struct playlist_entry_info *aktfilenum;	// currsong entry (aktfilenum)
	struct playlist_entry_info *newfilenum;	// nextsong entry (newfilenum)

	struct playlist_entry_info *newsong;	// skip to newsong absolute

	long seek_absolute;			// ie: with mouse on songposline
	long seek_relative;			// rewind/forward
	unsigned int seek_frames;	// rewind/forward framenum
	int step;					// step (skip) -1/+1 song relative
	int direction;				// direction of step (skip)

	int idone;					// end of decoding
	unsigned int fdone;			// end of file
	unsigned int adone;			// end of file + end of decoding
	unsigned int sndempty;		// no sound (above soundlimit)
	unsigned int partselect;	// open newfile or decoding
	unsigned int foundfile;		// found 1 playable audio file (at least) in the playlist
	struct crossfade_info *cfi;
	display_visual_data_s *vds;
} mainvars;

//card and mixer data
typedef struct mpxplay_audioout_info_s {
	short *pcm_sample;
	short *pcm16;
	unsigned int samplenum;
	unsigned int mixer_function_flags;
	unsigned char mixer_infobits;
	unsigned char bytespersample_mixer;
	unsigned char bytespersample_card;
	unsigned int freq_set;
	unsigned int freq_song;
	unsigned int freq_card;
	unsigned int chan_set;
	unsigned char chan_song;
	unsigned char chan_card;
	unsigned int bits_set;
	unsigned char bits_song;
	unsigned char bits_card;

	unsigned int card_wave_id;	// 0x0001,0x0003,0x0055,0x2000,etc.
	char *card_wave_name;		// currently file extension for -obs ("MP3","AC3")
	unsigned long card_controlbits;	// card control flags
	unsigned long card_infobits;	// card info flags
	unsigned long card_outbytes;	// samplenum*bytespersample_card
	unsigned long card_dmasize;
	unsigned long card_dmalastput;
	unsigned long card_dmaspace;
	unsigned long card_dmafilled;
	unsigned long card_dma_lastgoodpos;
	unsigned int card_bytespersign;	// bytespersample_card*chan_card
	unsigned int card_dmaout_under_int08;
	unsigned short int08_decoder_cycles;
	dosmem_t *card_dma_dosmem;
	char *card_DMABUFF;

	char *card_selectname;		// select card by name
	one_sndcard_info *card_handler;	// function structure of the card
	void *card_private_data;	// extra private datas can be pointed here (with malloc)
	unsigned short card_type;
	unsigned short card_port;
	unsigned char card_irq;
	unsigned char card_isa_dma;
	unsigned char card_isa_hidma;

	struct mainvars *mvp;
	struct playlist_entry_info *pei;	// for encoders
	unsigned int card_dma_buffer_size;
	//unsigned int card_mixer_values[AU_MIXCHANS];
	int card_master_volume;
	int card_mixer_values[AU_MIXCHANS_NUM];	// -1, 0-100
	//int card_mixer_values[AU_MIXCHANS_NUM][AU_MIXCHANFUNCS_NUM]; // -1, 0-100
} mpxplay_audioout_info_s;

//----------------------------------------------------------------------

//mpxplay.c
extern long mpxplay_calculate_timesec_to_framenum(struct frame *frp, char *pst);
extern long mpxplay_calculate_index_start_end(struct frame *frp, struct mainvars *mvp, struct playlist_entry_info *pei);	// return index_end
extern void mpxplay_stop_and_clear(struct mainvars *, unsigned int);
extern unsigned int mpxplay_check_buffers_full(struct mainvars *mvp);
extern void crossfade_part_step(struct mainvars *);
extern void crossfade_reset(struct mainvars *);
extern void mpxplay_close_program(unsigned int errorcode);

#endif
