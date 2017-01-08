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
//function: APE file handling
//requires the dec_ape\apedec.lib file (and include files)

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_APE

#include "newfunc\newfunc.h"
#include "tagging.h"
#include "decoders\ad_ape\apetypes.h"
#include "decoders\ad_ape\all.h"
#include "decoders\ad_ape\maclib.h"
#include "decoders\ad_ape\apeinfo.h"

#define APEDEC_SEEKBLOCKNUM_INVALID 0xffffffff

typedef struct ape_decoder_data {
	struct IAPEDecompress_data_s *spAPEDecompress;
	unsigned long seek_blocknum;
	unsigned int firstseek;
} ape_decoder_data;

static int ape_assign_values(struct ape_decoder_data *apei, struct mpxplay_infile_info_s *miis)
{
	int (*GetInfo) (struct CAPEInfo_data_s *, enum APE_DECOMPRESS_FIELDS Field, int nParam1, int nParam2);
	struct CAPEInfo_data_s *apeinfo_datas = apei->spAPEDecompress->apeinfo_datas;
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	int fileversion;

	GetInfo = apei->spAPEDecompress->apeinfo_datas->apeinfo_funcs->GetInfo;

	miis->filesize = apei->spAPEDecompress->fileio_funcs->filelength(apei->spAPEDecompress->fileio_datas);
	adi->freq = GetInfo(apeinfo_datas, APE_INFO_SAMPLE_RATE, 0, 0);
	adi->filechannels = adi->outchannels = GetInfo(apeinfo_datas, APE_INFO_CHANNELS, 0, 0);
	adi->bits = GetInfo(apeinfo_datas, APE_INFO_BITS_PER_SAMPLE, 0, 0);
	miis->timemsec = GetInfo(apeinfo_datas, APE_INFO_LENGTH_MS, 0, 0);

	apei->seek_blocknum = APEDEC_SEEKBLOCKNUM_INVALID;
	apei->firstseek = 1;

	miis->longname = malloc(MPXPLAY_ADITEXTSIZE_LONGNAME + 8);
	adi->bitratetext = malloc(MPXPLAY_ADITEXTSIZE_BITRATE + 8);
	if(!miis->longname || !adi->bitratetext)
		return 0;
	fileversion = GetInfo(apeinfo_datas, APE_INFO_FILE_VERSION, 0, 0);
	sprintf(miis->longname, "APEv%1.1d.%2.2d", fileversion / 1000, (fileversion / 10) % 100);
	sprintf(adi->bitratetext, "%2d/%2.1f%%", adi->bits, 100.0 * (float)GetInfo(apeinfo_datas, APE_INFO_AVERAGE_BITRATE, 0, 0)
			/ (float)GetInfo(apeinfo_datas, APE_INFO_DECOMPRESSED_BITRATE, 0, 0));

	return 1;
}

static int APE_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct ape_decoder_data *apei;

	apei = (struct ape_decoder_data *)calloc(1, sizeof(struct ape_decoder_data));
	if(!apei)
		goto err_out_check;
	miis->private_data = apei;

	apei->spAPEDecompress = IAPEDecompress_check(fbfs, fbds, filename);
	if(!apei->spAPEDecompress)
		goto err_out_check;

	if(!ape_assign_values(apei, miis))
		goto err_out_check;

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_check:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int APE_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct ape_decoder_data *apei;
	long allblocks;

	apei = (struct ape_decoder_data *)calloc(1, sizeof(struct ape_decoder_data));
	if(!apei)
		goto err_out_open;
	miis->private_data = apei;

	apei->spAPEDecompress = IAPEDecompress_open(fbfs, fbds, filename);
	if(!apei->spAPEDecompress)
		goto err_out_open;

	if(!ape_assign_values(apei, miis))
		goto err_out_open;

	allblocks = IAPEDecompress_GetInfo(apei->spAPEDecompress, APE_DECOMPRESS_TOTAL_BLOCKS, 0, 0);
	if(allblocks < 1)
		goto err_out_open;

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_open:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static void APE_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct ape_decoder_data *apei = miis->private_data;
	if(apei) {
		struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
		if(apei->spAPEDecompress) {
			apei->spAPEDecompress->fileio_datas = fbds;	// update always
			IAPEDecompress_close(apei->spAPEDecompress);
		}
		if(miis->longname)
			free(miis->longname);
		if(adi->bitratetext)
			free(adi->bitratetext);
		free(apei);
	}
	fbfs->fclose(fbds);
}

static int APE_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct ape_decoder_data *apei = miis->private_data;
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	int nBlocksDecoded = -1, nRetVal;

	apei->spAPEDecompress->fileio_datas = fbds;	// update allways

	nRetVal = IAPEDecompress_GetData(apei->spAPEDecompress, (char *)adi->pcm_bufptr, adi->pcm_framelen, &nBlocksDecoded);

	if(nRetVal == APEDEC_ERROR_IO_EOF)
		return MPXPLAY_ERROR_INFILE_EOF;

	if(nRetVal != APEDEC_ERROR_SUCCESS || nBlocksDecoded < 1)
		return MPXPLAY_ERROR_INFILE_NODATA;

	adi->pcm_samplenum = (nBlocksDecoded * IAPEDecompress_GetInfo(apei->spAPEDecompress, APE_INFO_BLOCK_ALIGN, 0, 0)) / (adi->bits >> 3);

	return MPXPLAY_ERROR_INFILE_OK;
}

//-------------------------------------------------------------------------

static void APE_infile_clearbuffs(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, unsigned int seektype)
{
	struct ape_decoder_data *apei = miis->private_data;
	if(apei) {
		apei->spAPEDecompress->fileio_datas = fbds;
		if(seektype & (MPX_SEEKTYPE_NORM | MPX_SEEKTYPE_BOF) && (apei->seek_blocknum != APEDEC_SEEKBLOCKNUM_INVALID))
			IAPEDecompress_Seek(apei->spAPEDecompress, apei->seek_blocknum);
		apei->seek_blocknum = APEDEC_SEEKBLOCKNUM_INVALID;
	}
}

static long APE_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newmpxframenum)
{
	struct ape_decoder_data *apei = miis->private_data;
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	struct IAPEDecompress_data_s *iapedec = apei->spAPEDecompress;
	unsigned int blocks_per_frame = IAPEDecompress_GetInfo(iapedec, APE_INFO_BLOCKS_PER_FRAME, 0, 0);
	unsigned int allapeframes = IAPEDecompress_GetInfo(iapedec, APE_INFO_TOTAL_FRAMES, 0, 0);
	unsigned int newblock = newmpxframenum * adi->pcm_framelen;
	unsigned int currblock, newapeframe;
	unsigned long newbytepos, beginbytepos;
	//char sout[100];

	if(apei->seek_blocknum != APEDEC_SEEKBLOCKNUM_INVALID) {
		currblock = apei->seek_blocknum;
		apei->seek_blocknum = APEDEC_SEEKBLOCKNUM_INVALID;
	} else
		currblock = IAPEDecompress_GetInfo(iapedec, APE_DECOMPRESS_CURRENT_BLOCK, 0, 0);

	if(newblock != currblock || !newblock) {
		if(newblock > currblock) {
			if(currblock || (newblock - currblock) < blocks_per_frame)	// fast forward seek
				newblock += blocks_per_frame;
			newapeframe = newblock / blocks_per_frame;
			if(newapeframe >= allapeframes)	// eof
				return MPXPLAY_ERROR_INFILE_EOF;
		} else {
			if(newblock > (blocks_per_frame / 2)) {	// rewind in file
				newblock -= blocks_per_frame / 2;
				newapeframe = newblock / blocks_per_frame;
			} else {			// begin of file (newblock==0 && newblock==currblock)
				newapeframe = 0;
			}
		}

		newblock = newapeframe * blocks_per_frame;
		//pds_textdisplay_printf("--------------------------------------------------------------");
		//sprintf(sout,"cb:%d nb:%d nf:%d af:%d fs:%d",currblock,newblock,newapeframe,allapeframes,apei->firstseek);
		//display_message(1,0,sout);
		//pds_textdisplay_printf(sout);
		//getch();
		if(apei->firstseek) {	// first seeking in this file, fill bitstream full
			iapedec->fileio_datas = fbds;
			if(IAPEDecompress_Seek(apei->spAPEDecompress, newblock) < 0)
				return MPXPLAY_ERROR_INFILE_EOF;
			apei->seek_blocknum = APEDEC_SEEKBLOCKNUM_INVALID;
			apei->firstseek = 0;
		} else {
			newbytepos = IAPEDecompress_GetInfo(iapedec, APE_INFO_SEEK_BYTE, newapeframe, 0);
			beginbytepos = IAPEDecompress_GetInfo(iapedec, APE_INFO_SEEK_BYTE, 0, 0);
			newbytepos -= (newbytepos - beginbytepos) % 4;
			if(iapedec->fileio_funcs->fseek(fbds, newbytepos, SEEK_SET) < 0)
				return MPXPLAY_ERROR_INFILE_EOF;
			apei->seek_blocknum = newblock;
		}
		//sprintf(sout,"asb:%d ret:%d ",apei->seek_blocknum,newblock/apei->blocks_per_decode);
		//pds_textdisplay_printf(sout);
		//display_message(1,0,sout);
		//getch();
	}

	return (newblock / adi->pcm_framelen);
}

struct mpxplay_infile_func_s IN_APE_funcs = {
	(MPXPLAY_TAGTYPE_PUT_SUPPORT(MPXPLAY_TAGTYPE_ID3V1 | MPXPLAY_TAGTYPE_ID3V2 | MPXPLAY_TAGTYPE_APETAG)
	 | MPXPLAY_TAGTYPE_PUT_PRIMARY(MPXPLAY_TAGTYPE_APETAG)),
	NULL,
	NULL,
	&APE_infile_check,
	&APE_infile_check,
	&APE_infile_open,
	&APE_infile_close,
	&APE_infile_decode,
	&APE_infile_fseek,
	&APE_infile_clearbuffs,
	NULL,
	NULL,
	NULL,
	{"APE", "MAC", NULL}
};

#endif							// MPXPLAY_LINK_INFILE_APE
