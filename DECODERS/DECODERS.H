//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2007 by PDSoft (Attila Padar)                *
//*                    http://mpxplay.cjb.net                              *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: decoders handling inc.

#ifndef mpxplay_decoders_h
#define mpxplay_decoders_h

#include "in_file.h"

#define PCM_BUFSIZE_DECODER 32768	// initial size

extern int mpxplay_decoders_open(struct mpxplay_infile_info_s *miis, struct frame *frp);
extern void mpxplay_decoders_close(struct mpxplay_infile_info_s *miis, struct frame *frp);
extern int mpxplay_decoders_decode(struct mpxplay_infile_info_s *miis);
extern void mpxplay_decoders_clearbuf(struct mpxplay_infile_info_s *miis, struct frame *frp, unsigned int seektype);
extern void mpxplay_decoders_setpos(struct mpxplay_infile_info_s *miis, long newmpxframenum);

extern void mpxplay_decoders_preinit(void);
extern void mpxplay_decoders_deinit(void);
extern int mpxplay_decoders_alloc(struct frame *frp, unsigned int full);
extern void mpxplay_decoders_free(struct mpxplay_infile_info_s *miis, struct frame *frp);
extern void mpxplay_decoders_reset(struct mpxplay_infile_info_s *miis, struct frame *frp);
extern void mpxplay_decoders_clear(struct mpxplay_infile_info_s *miis, struct frame *frp);

extern unsigned int mpxplay_decoders_audio_eq_exists(struct frame *frp);
extern void mpxplay_decoders_audio_eq_config(struct frame *frp, unsigned int bandnum, unsigned long *band_freqs, float *band_powers);

#endif							// mpxplay_decoders_h
