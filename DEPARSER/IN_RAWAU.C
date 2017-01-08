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
//function: raw-audio streams (ac3,dts,mp2,mp3) common routines

#include "in_file.h"
#include "newfunc\newfunc.h"

#define MPXPLAY_INRAWAU_MIN_FILESIZE 16

int INRAWAU_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;

	if(!fbfs->fopen_read(fbds, filename, 0))
		return MPXPLAY_ERROR_INFILE_FILEOPEN;

	miis->filesize = fbfs->filelength(fbds);
	if(miis->filesize < MPXPLAY_INRAWAU_MIN_FILESIZE)
		goto err_out_open;

	spi->streamtype = MPXPLAY_SPI_STREAMTYPE_AUDIO;
	funcbit_enable(spi->flags, (MPXPLAY_SPI_FLAG_NEED_DECODER | MPXPLAY_SPI_FLAG_NEED_PARSING));

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_open:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

void INRAWAU_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	fbfs->fclose(fbds);
}

int INRAWAU_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	long bytes;

	if(!spi->bs_leftbytes) {
		bytes = fbfs->fread(fbds, spi->bitstreambuf, spi->bs_readsize);
		if(bytes <= 0)
			return MPXPLAY_ERROR_INFILE_NODATA;

		spi->bs_leftbytes = bytes;
	}

	return MPXPLAY_ERROR_INFILE_OK;
}

long INRAWAU_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newframenum)
{
	mpxp_filesize_t newfilepos = (mpxp_filesize_t) ((float)newframenum * (float)miis->filesize / (float)miis->allframes);
	if(fbfs->fseek(fbds, newfilepos, SEEK_SET) < 0)
		return MPXPLAY_ERROR_INFILE_EOF;
	return newframenum;
}
