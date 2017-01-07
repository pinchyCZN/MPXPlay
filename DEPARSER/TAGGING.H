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

#ifndef mpxplay_tagging_h
#define mpxplay_tagging_h

#include "in_file.h"

//miis->standard_id3tag_control
#define MPXPLAY_TAGTYPE_ID3V1  1
#define MPXPLAY_TAGTYPE_ID3V2  2
#define MPXPLAY_TAGTYPE_APETAG 4
#define MPXPLAY_TAGTYPES_NUM   3 // number of supported common tagtypes
#define MPXPLAY_TAGTYPES_MASK  255
#define MPXPLAY_TAGTYPE_PUT_SUPPORT(t)  ((t)<< 16) // the file supports these tagtypes (for read/write)
#define MPXPLAY_TAGTYPE_GET_SUPPORT(v)  (((v)>>16)&MPXPLAY_TAGTYPES_MASK)
#define MPXPLAY_TAGTYPE_GET_PRIMARY(v)  (((v)>> 24)&MPXPLAY_TAGTYPES_MASK)
#define MPXPLAY_TAGTYPE_PUT_PRIMARY(t)  ((t)<< 24) // the file supports these tagtypes (for read/write)
#define MPXPLAY_TAGTYPE_SET_FOUND(v,t)  v|=((t)<< 0) // found this tagtype(s) in the file
#define MPXPLAY_TAGTYPE_GET_FOUND(v)    (((v)>> 0)&MPXPLAY_TAGTYPES_MASK)
#define MPXPLAY_TAGTYPE_FUNCMASK ((MPXPLAY_TAGTYPES_MASK<<16)|(MPXPLAY_TAGTYPES_MASK<<24))

#define MPXPLAY_TAGGING_ID3V2_HEADSIZE 10

extern char *mpxplay_tagging_id3v1_index_to_genre(unsigned int i);
extern unsigned int mpxplay_tagging_id3v1_check(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds);
extern char *mpxplay_tagging_id3v1_get(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char **id3ip,char *id3p,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs);
extern int  mpxplay_tagging_id3v1_put(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs);

extern unsigned int mpxplay_tagging_id3v2_check(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds);
extern char *mpxplay_tagging_id3v2_get(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,char *id3p,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs);
extern int mpxplay_tagging_id3v2_put(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs);
extern unsigned long mpxplay_tagging_id3v2_totalsize(char *bufp); // needs 10 bytes in bufp

extern int   mpxplay_tagging_apetag_check(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds);
extern char *mpxplay_tagging_apetag_get(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,char *id3p,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs);
extern int   mpxplay_tagging_apetag_put(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs);

#endif // mpxplay_tagging_h
