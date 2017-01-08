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
// video output and mixer (postprocess) definitions

#ifndef videoout_h
#define videoout_h

#define VIDEOSCREEN_DEFAULT_RESX 1024
#define VIDEOSCREEN_DEFAULT_BITS 32

#define MPXPLAY_PIC_FMT_NONE  0
#define MPXPLAY_PIC_FMT_8BIT  1
#define MPXPLAY_PIC_FMT_12BIT 2
#define MPXPLAY_PIC_FMT_16BIT 3
#define MPXPLAY_PIC_FMT_24BIT 4
#define MPXPLAY_PIC_FMT_32BIT 5
#define MPXPLAY_PIC_FMT_BITS  15
#define MPXPLAY_PIC_FMT_RGB   16
#define MPXPLAY_PIC_FMT_YUV   32

#define MPXPLAY_PIC_FMT_YV12  (MPXPLAY_PIC_FMT_YUV|MPXPLAY_PIC_FMT_12BIT)
#define MPXPLAY_PIC_FMT_YUY2  (MPXPLAY_PIC_FMT_YUV|MPXPLAY_PIC_FMT_16BIT)
#define MPXPLAY_PIC_FMT_YUV24 (MPXPLAY_PIC_FMT_YUV|MPXPLAY_PIC_FMT_24BIT)
#define MPXPLAY_PIC_FMT_RGB24 (MPXPLAY_PIC_FMT_RGB|MPXPLAY_PIC_FMT_24BIT)
#define MPXPLAY_PIC_FMT_RGB32 (MPXPLAY_PIC_FMT_RGB|MPXPLAY_PIC_FMT_32BIT)

typedef struct mpxplay_videoout_func_s;

// common structure for
typedef struct mpxplay_videoout_info_s {
	unsigned char *video_picture;
	unsigned long video_bytes;
	unsigned long video_res_x;
	unsigned long video_res_y;
	unsigned long video_bpp;
	unsigned long video_pixeltype;

	char *config_screenhandler_name;
	unsigned long config_mode;
	unsigned long config_res_x;
	unsigned long config_res_y;
	unsigned long config_bpp;

	struct mpxplay_videoout_func_s *screen_handler;
	unsigned long screen_hwmix_capables;
	void *screen_private_data;
	unsigned char *screen_linear_ptr;
	unsigned long screen_size;
	unsigned long screen_res_x;
	unsigned long screen_res_y;
	unsigned long screen_bpp;
	unsigned long screen_pixeltype;
} mpxplay_videoout_info_s;

typedef struct mpxplay_videoout_func_s {
	char *name;
	unsigned long infobits;
	unsigned int (*init) (struct mpxplay_videoout_info_s *);	// init/alloc/check
	void (*close) (struct mpxplay_videoout_info_s *);	// close/dealloc
	void (*listmodes) (struct mpxplay_videoout_info_s *);	// list available modes
	unsigned int (*searchmode) (struct mpxplay_videoout_info_s *, unsigned int x, unsigned int y, unsigned int bpp);	// search mode for res/bpp
	unsigned int (*set) (struct mpxplay_videoout_info_s *, unsigned int clearmem);	// set graph mode
	unsigned int (*reset) (struct mpxplay_videoout_info_s *);	// restore non-graph mode
	unsigned int (*write) (struct mpxplay_videoout_info_s *, unsigned long x, unsigned long y, unsigned char *data);
} mpxplay_videoout_func_s;

#include "au_mixer\au_mixer.h"	// inittype & setmode values are the same

typedef struct mpxplay_videomix_func_s {
	char *name;
	char *cmdlineopt;
	unsigned long infobits;
	unsigned int (*init) (struct mpxplay_videoout_info_s *, unsigned int inittype);
	unsigned int (*config) (struct mpxplay_videoout_info_s *, unsigned int setmode, int value);
	unsigned int (*checkfunc) (struct mpxplay_videoout_info_s *);
	void (*process) (struct mpxplay_videoout_info_s *, unsigned long screen_beginpos, unsigned long screen_bytes);
} mpxlay_videomix_func_s;

extern void mpxplay_videoout_init(struct mpxplay_videoout_info_s *voi);
extern void mpxplay_videoout_close(struct mpxplay_videoout_info_s *voi);
extern void mpxplay_videoout_listmodes(struct mpxplay_videoout_info_s *voi);

#endif							// videoout_h
