//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: visual plugin API

#ifndef mpxplay_visualpi_h
#define mpxplay_visualpi_h

//#include "newfunc\newfunc.h"

typedef struct display_visual_data_v0153_s {
	short *pcm_data;			// interlaced stereo (2ch), 16-bit samples
	unsigned long pcm_samplenum;	// number of samples in pcm_data (usually 1152*2=2304)
	unsigned long pcm_freq;		// sample frequency
	unsigned int channelnum;	// now it's 2 (PCM_MAX_CHANNELS) in Mpxplay
	unsigned long *soundvolumes;	// soundvolumes[channelnum]
	unsigned int bandnum;		// now it's 32 in Mpxplay
	unsigned long *analbands;	// analbands[bandnum] (spectrum analiser bands)
	unsigned long anal_freq_range;	// now it's 22050 (constant, it's correct at 44100Hz output only)
	unsigned int displaymode;	// displaymode
	unsigned long reserved_09;	// desktopmode?
	unsigned long reserved_10;	// refdisp?
	unsigned long reserved_11;	// timemode?
	unsigned long reserved_12;	// mouse_on?
	unsigned long reserved_13;
	unsigned long reserved_14;
	unsigned long reserved_15;
	unsigned long textscreen_linear_address;	// 0xb8000 by default (can redirect)
	unsigned long textscreen_resolution_x;
	unsigned long textscreen_resolution_y;
	unsigned long textscreen_bytes_per_char;	// 2
	unsigned long videoscreen_linear_address;	// got from vesabios
	unsigned long videoscreen_resolution_x;
	unsigned long videoscreen_resolution_y;
	unsigned long videoscreen_bits_per_pixel;
	unsigned long reserved_24;
	unsigned long reserved_25;
	unsigned long reserved_26;
	unsigned long reserved_27;
	unsigned long reserved_28;
	unsigned long reserved_29;
	unsigned long reserved_30;
	unsigned long reserved_31;
} display_visual_data_v0153_s;

typedef display_visual_data_v0153_s display_visual_data_s;

typedef struct display_visual_func__v0153_s {
	void *(*init) (display_visual_data_s * vds);	// returns private_data
	void (*close) (void *private_data);
	int (*start) (void *private_data);	// note: currently init/start and stop/close are called together
	void (*stop) (void *private_data);
	void (*draw) (void *private_data);
	unsigned int fps;			// refresh in 1 sec (call draw() N times/sec) (if it's 0, then a default built-in value is used (currently 18))
} display_visual_func_v0153_s;

typedef display_visual_func_v0153_s display_visual_func_s;

extern void display_visualpi_init(display_visual_data_s * vds);
extern void display_visualpi_close(void);
extern void display_visualplugin_stop(display_visual_data_s * vds);

#endif							// mpxplay_visualpi_h
