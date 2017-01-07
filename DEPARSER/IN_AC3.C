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
//function: AC3 file handling (with AC3 decoder parsing)

#include "in_file.h"
#include "in_rawau.h"
#include "tagging.h"

#ifdef MPXPLAY_LINK_INFILE_AC3

static int INAC3_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *miis)
{
 int retcode;

 retcode=INRAWAU_infile_open(fbfs,fbds,filename,miis);
 if(retcode!=MPXPLAY_ERROR_INFILE_OK)
  return retcode;

 miis->audio_stream->wave_id=MPXPLAY_WAVEID_AC3;
 miis->longname="DolbyAC3";

 return MPXPLAY_ERROR_INFILE_OK;
}

struct mpxplay_infile_func_s IN_AC3_funcs={
 (MPXPLAY_TAGTYPE_PUT_SUPPORT(MPXPLAY_TAGTYPE_ID3V1|MPXPLAY_TAGTYPE_APETAG)
 |MPXPLAY_TAGTYPE_PUT_PRIMARY(MPXPLAY_TAGTYPE_APETAG)),
 NULL,
 NULL,
 &INAC3_infile_open,
 &INAC3_infile_open,
 &INAC3_infile_open,
 &INRAWAU_infile_close,
 &INRAWAU_infile_decode,
 &INRAWAU_fseek,
 NULL,
 NULL,
 NULL,
 NULL,
 {"AC3",NULL}
};

#endif // MPXPLAY_LINK_INFILE_AC3
