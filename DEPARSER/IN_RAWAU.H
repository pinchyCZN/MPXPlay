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
//function: in-file raw stream handling (common mp2,mp3,ac3,dts routines)

#ifndef mpxplay_in_rawau_h
#define mpxplay_in_rawau_h

#include "in_file.h"

extern int  INRAWAU_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis);
extern void INRAWAU_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis);
extern int  INRAWAU_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis);
extern long INRAWAU_fseek(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,long newframenum);

#endif
