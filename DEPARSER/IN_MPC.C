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
//function: MPEG-Plus/MusePack (MPC) file handling - main
//requires the dec_mpc\mpcdec.lib & mpcdec.h files
//based on the MPC decoder source

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_MPC

#include "newfunc\newfunc.h"
#include "tagging.h"
#include "decoders\ad_mpc\mpcdec.h"
#include "decoders\ad_mpc\bitstrm.h"
#include "decoders\ad_mpc\requant.h"
#include "decoders\ad_mp3\mp3dec.h"

#define MPC_OUT_T float

extern volatile unsigned int intsoundconfig, intsoundcontrol;

static struct mpxplay_audio_decoder_func_s AD_MPC_funcs;

static void MPC_infile_clearbuffs(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, unsigned int seektype);

static void mpc_save_bitpos(struct mpc_decoder_data *, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds);
static int mpc_seek_helper_init(void);
static void mpc_seek_helper_close(void);
static void mpc_seek_helper_clear(void);

static void MPC_infile_preinit(void)
{
	mpcdec_Huffman_Decoder_SV7_init();
	mpcdec_init_requant();
	mpc_seek_helper_init();
}

static void MPC_infile_deinit(void)
{
	mpc_seek_helper_close();
}

static int MPC_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	struct mpc_decoder_data *mpci;
	int streamversion_major, streamversion_minor;

	if(!fbfs->fopen_read(fbds, filename, 0))
		return MPXPLAY_ERROR_INFILE_FILEOPEN;

	miis->filesize = fbfs->filelength(fbds);
	if(miis->filesize < MPC_HEADERSIZE)
		goto err_out_chk;

	mpci = (struct mpc_decoder_data *)calloc(1, sizeof(struct mpc_decoder_data));
	if(!mpci)
		goto err_out_chk;
	miis->private_data = mpci;

	mpci->bitstream = malloc(MPC_BITSTREAM_BYTESIZE + 64);
	if(!mpci->bitstream)
		goto err_out_chk;

	mpcdec_bitstream_fill(mpci, fbfs->fread, fbds, 32);	//MPC_HEADERSIZE);
	if(pds_strncmp((char *)mpci->bitstream, "MP+", 3) != 0)
		goto err_out_chk;

	mpci->Bitrate = 0;
	mpci->StreamVersion = mpci->bitstream[0] >> 24;
	streamversion_major = mpci->StreamVersion & 0xf;

	if(streamversion_major > 7)
		goto err_out_chk;

	if(streamversion_major == 7) {
		mpci->bs_elementptr++;
		mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
		mpci->OverallFrames = mpcdec_Bitstream_read(mpci, 32);
		mpci->IS_used = mpcdec_Bitstream_read(mpci, 1);
		mpci->MS_used = mpcdec_Bitstream_read(mpci, 1);
		mpci->Max_Band_desired = mpcdec_Bitstream_read(mpci, 6);
		mpcdec_bitstream_forward(mpci, 1 + 3 + 4 + 16 + 8 + 32 + 32 + 32);	// skip non-used bits
	} else {
		mpci->MS_used = ((mpci->bitstream[0] << 10) >> 31);
		mpci->StreamVersion = ((mpci->bitstream[0] << 11) >> 22);
		if(mpci->StreamVersion >= 5)
			mpci->OverallFrames = mpci->bitstream[1];
		else
			mpci->OverallFrames = (mpci->bitstream[1] >> 16);
		if(mpci->StreamVersion < 6 && mpci->OverallFrames)
			mpci->OverallFrames--;
	}
	if(!mpci->OverallFrames)
		mpci->OverallFrames = 1;

	adi->freq = 44100;
	adi->bits = 16;
	adi->filechannels = adi->outchannels = 2;
	if(mpci->IS_used)
		adi->channeltext = "i-Stereo";
	else if(mpci->MS_used)
		adi->channeltext = "msStereo";

	miis->allframes = mpci->OverallFrames;
	adi->pcm_framelen = 1152;
	pds_ftoi((float)miis->filesize / ((1000.0 / 8.0) * (float)(mpci->OverallFrames * 1152) / (float)adi->freq), (long *)&adi->bitrate);	// average bitrate

	//adi->infobits|=ADI_FLAG_OWN_SPECTANAL;
#ifndef MPXDEC_INTEGER_OUTPUT
	funcbit_enable(adi->infobits, (ADI_FLAG_FLOATOUT | ADI_FLAG_FPUROUND_CHOP));
#endif
	miis->longname = malloc(MPXPLAY_ADITEXTSIZE_LONGNAME + 8);
	if(!miis->longname)
		goto err_out_chk;
	streamversion_minor = (mpci->StreamVersion >> 4) & 0xf;

	if(streamversion_minor < 10)
		sprintf(miis->longname, "MPC v%1.1d.%1.1d", streamversion_major, streamversion_minor);
	else
		sprintf(miis->longname, "MPCv%1.1d.%2.2d", streamversion_major, streamversion_minor);

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_chk:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int MPC_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct mpc_decoder_data *mpci;
	struct mpxsynth_data_s *synthdata;
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	int retcode;

	retcode = MPC_infile_check(fbfs, fbds, filename, miis);
	if(retcode != MPXPLAY_ERROR_INFILE_OK)
		return retcode;

	mpci = miis->private_data;
	if(mpci->StreamVersion != 0x07 && mpci->StreamVersion != 0x17)
		goto err_out_opn;
	mpcdec_bitstream_reset(mpci);
	if(!mpcdec_allocate_datafields(mpci))
		goto err_out_opn;

	synthdata = mpxdec_layer3_synth_alloc(mpci->hybridout);
	if(!synthdata)
		goto err_out_opn;
	mpci->synthdata = synthdata;
	synthdata->outchannels = 2;
	synthdata->granules = 2;
	adi->private_data = mpci;
	miis->audio_decoder_funcs = &AD_MPC_funcs;

	mpci->framebeginbitpos = malloc((mpci->OverallFrames + 1024) * sizeof(unsigned long));
	if(!mpci->framebeginbitpos)
		goto err_out_opn;
	pds_memset(mpci->framebeginbitpos, 0, (mpci->OverallFrames + 1024) * sizeof(unsigned long));
	mpc_seek_helper_clear();

	//mpcdec_Huffman_Decoder_SV7_init();
	mpcdec_initialisiere_Quantisierungstabellen(mpci);

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_opn:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static void MPC_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct mpc_decoder_data *mpci = miis->private_data;

	if(mpci) {
		if(mpci->bitstream)
			free(mpci->bitstream);
		mpxdec_layer3_synth_close(mpci->synthdata);
		mpcdec_free_datafields(mpci);
		if(mpci->framebeginbitpos)
			free(mpci->framebeginbitpos);
		free(mpci);
		if(miis->longname)
			free(miis->longname);
	}
	fbfs->fclose(fbds);
}

static int MPC_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct mpc_decoder_data *mpci = miis->private_data;
	mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	unsigned int framebits = 0;
	unsigned int holdBits;

	if(mpci->decodedframes >= mpci->OverallFrames)
		return MPXPLAY_ERROR_INFILE_EOF;

	if(mpci->resync_counter)
		MPC_infile_clearbuffs(fbfs, fbds, miis, MPX_SEEKTYPE_NORM);
	if(mpci->resync_counter)
		return MPXPLAY_ERROR_INFILE_SYNC_IN;

	if(mpci->bs_forwardbits <= (MPC_BITSTREAM_BYTESIZE2 * 8))
		mpcdec_bitstream_fill(mpci, fbfs->fread, fbds, MPC_BITSTREAM_BYTESIZE2);

	if(mpci->bs_forwardbits > 20) {
		framebits = mpcdec_Bitstream_read(mpci, 20);
		if(mpci->bs_forwardbits >= framebits) {
			mpc_save_bitpos(mpci, fbfs, fbds);

			holdBits = mpcdec_BitsRead(mpci);

			mpcdec_Decode_Bitstream_SV7(mpci);

			if((mpcdec_BitsRead(mpci) - holdBits) == framebits) {
				if(mpci->IS_used) {
					mpcdec_Requantisierung(mpci, (int)mpci->Min_Band - 1, mpci->hybridout);
					mpcdec_Intensity_Stereo_Decode(mpci, mpci->hybridout, mpci->Min_Band, mpci->Max_Band);
				} else {
					mpcdec_Requantisierung(mpci, mpci->Max_Band, mpci->hybridout);
				}
				mpxdec_decode_part2(mpci->synthdata, adi->pcm_bufptr);
				adi->pcm_samplenum = 1152 * 2;
			}
			mpci->decodedframes++;
			return MPXPLAY_ERROR_INFILE_OK;
		} else {
			mpcdec_bitstream_rewind(mpci, 20);
		}
	}

	return MPXPLAY_ERROR_INFILE_NODATA;
}

//-------------------------------------------------------------------------
static struct mpc_decoder_data *seek_helper;

static int mpc_seek_helper_init(void)
{
	if(!seek_helper)
		seek_helper = malloc(sizeof(struct mpc_decoder_data));
	if(!seek_helper)
		return 0;
	pds_memset((void *)seek_helper, 0, sizeof(struct mpc_decoder_data));
	seek_helper->bitstream = malloc(MPC_BITSTREAM_BYTESIZE + 64);
	if(!seek_helper->bitstream) {
		mpc_seek_helper_close();
		return 0;
	}
	return 1;
}

static void mpc_seek_helper_close(void)
{
	if(seek_helper) {
		if(seek_helper->bitstream) {
			free(seek_helper->bitstream);
			seek_helper->bitstream = NULL;
		}
		free(seek_helper);
		seek_helper = NULL;
	}
}

static void mpc_seek_helper_clear(void)
{
	if(seek_helper) {
		mpcdec_bitstream_reset(seek_helper);
		seek_helper->decodedframes = seek_helper->resync_counter = 0;
	}
}

static void mpc_copy_seek_to_mpci(struct mpc_decoder_data *mpci)
{
	unsigned int intsoundcntrl_save;
	MPXPLAY_INTSOUNDDECODER_DISALLOW pds_memcpy((char *)mpci->bitstream, (char *)seek_helper->bitstream, MPC_BITSTREAM_BYTESIZE);
	mpci->bs_bitpos = seek_helper->bs_bitpos;
	mpci->bs_elementptr = seek_helper->bs_elementptr;
	mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
	mpci->bs_elemcount = seek_helper->bs_elemcount;
	mpci->bs_rewindbits = seek_helper->bs_rewindbits;
	mpci->bs_forwardbits = seek_helper->bs_forwardbits;
	mpci->bs_putbyteptr = seek_helper->bs_putbyteptr;
	mpci->decodedframes = seek_helper->decodedframes;
	mpci->resync_counter = seek_helper->resync_counter;
	MPXPLAY_INTSOUNDDECODER_ALLOW mpc_seek_helper_clear();
}

static void mpc_copy_mpci_to_seek(struct mpc_decoder_data *mpci)
{
	unsigned int intsoundcntrl_save;
	MPXPLAY_INTSOUNDDECODER_DISALLOW pds_memcpy((char *)seek_helper->bitstream, (char *)mpci->bitstream, MPC_BITSTREAM_BYTESIZE);
	seek_helper->bs_bitpos = mpci->bs_bitpos;
	seek_helper->bs_elementptr = mpci->bs_elementptr;
	seek_helper->bs_dword = seek_helper->bitstream[seek_helper->bs_elementptr];
	seek_helper->bs_elemcount = mpci->bs_elemcount;
	seek_helper->bs_rewindbits = mpci->bs_rewindbits;
	seek_helper->bs_forwardbits = mpci->bs_forwardbits;
	seek_helper->bs_putbyteptr = mpci->bs_putbyteptr;
	seek_helper->decodedframes = mpci->decodedframes;
	seek_helper->OverallFrames = mpci->OverallFrames;
	seek_helper->framebeginbitpos = mpci->framebeginbitpos;	// pointer!!
MPXPLAY_INTSOUNDDECODER_ALLOW}

static void mpc_save_bitpos(struct mpc_decoder_data *mpci, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	if((!mpci->framebeginbitpos[mpci->decodedframes]) && mpci->decodedframes < (mpci->OverallFrames + 1023))
		mpci->framebeginbitpos[mpci->decodedframes] = fbfs->ftell(fbds) * 8 - mpci->bs_forwardbits - 20;
}

static void MPC_infile_clearbuffs(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, unsigned int seektype)
{
	struct mpc_decoder_data *mpci = miis->private_data;
	unsigned int framebits, holdBits;

	if(seektype & MPX_SEEKTYPE_BOF)
		mpcdec_clear_datafields(mpci);

	if(seektype & MPX_SEEKTYPE_NORM) {
		while(mpci->resync_counter) {
			if(mpci->bs_forwardbits <= (MPC_BITSTREAM_BYTESIZE2 * 8))
				mpcdec_bitstream_fill(mpci, fbfs->fread, fbds, MPC_BITSTREAM_BYTESIZE2);
			if(mpci->bs_forwardbits > 20) {
				framebits = mpcdec_Bitstream_read(mpci, 20);
				if(mpci->bs_forwardbits >= framebits) {
					mpc_save_bitpos(mpci, fbfs, fbds);
					holdBits = mpcdec_BitsRead(mpci);
					mpcdec_Decode_Bitstream_SV7(mpci);
					if((mpcdec_BitsRead(mpci) - holdBits) != framebits) {
						mpci->resync_counter = 0;
						break;
					}
				} else {
					mpcdec_bitstream_rewind(mpci, 20);
					break;
				}
			} else
				break;
			mpci->decodedframes++;
			mpci->resync_counter--;
		}
	}

	if(seektype & (MPX_SEEKTYPE_BOF | MPX_SEEKTYPE_PAUSE))
		mpxdec_layer3_synth_clear(mpci->synthdata);
}

static long MPC_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newframenum)
{
	struct mpc_decoder_data *mpci = miis->private_data;
	unsigned long fbbp;

	if(!seek_helper)
		return -1;

	if(newframenum > 32) {
		if(!mpci->decodedframes && (mpci->OverallFrames > 100) && (newframenum > (mpci->OverallFrames - 100)))
			newframenum = mpci->OverallFrames - 100;	// 32 is not enough to rewind trough files
		else
			newframenum -= 32;	// 32 frame 'empty' decoding after seeking
	} else
		newframenum = 0;
	mpc_copy_mpci_to_seek(mpci);
	fbbp = seek_helper->framebeginbitpos[newframenum];
	if(fbbp						// skip to the saved position (if possible)
	   // if we jump a too short forward, the filedata may be out from the prebuffer
	   && ((newframenum < seek_helper->decodedframes) || (newframenum > (seek_helper->decodedframes + 100)))) {
		mpc_seek_helper_clear();
		fbfs->fseek(fbds, (fbbp / 8) & (~3), SEEK_SET);	// set elementpos (dword (4 byte) align)
		mpcdec_bitstream_fill(seek_helper, fbfs->fread, fbds, MPC_BITSTREAM_BYTESIZE);
		mpcdec_bitstream_forward(seek_helper, fbbp & 31);	// set bitpos
		seek_helper->decodedframes = newframenum;
		if(newframenum)
			seek_helper->resync_counter = 32;
	} else {
		// if we haven't saved the begin-file-position of the 'newframe'
		if(!seek_helper->decodedframes || (newframenum < seek_helper->decodedframes)) {	// seeking from the begin of file
			mpc_seek_helper_clear();
			fbfs->fseek(fbds, 0, SEEK_SET);
			mpcdec_bitstream_fill(seek_helper, fbfs->fread, fbds, MPC_BITSTREAM_BYTESIZE);
			mpcdec_bitstream_forward(seek_helper, MPC_HEADERSIZE * 8);	// skip header
		}
		if(newframenum) {
			while(seek_helper->decodedframes < newframenum) {	// seeking from the current position
				if(seek_helper->bs_forwardbits < (MPC_BITSTREAM_BYTESIZE2 * 8))
					mpcdec_bitstream_fill(seek_helper, fbfs->fread, fbds, MPC_BITSTREAM_BYTESIZE2);

				if(seek_helper->bs_forwardbits > 20) {
					unsigned long framebits = mpcdec_Bitstream_read(seek_helper, 20);
					if(seek_helper->bs_forwardbits >= framebits) {
						mpc_save_bitpos(seek_helper, fbfs, fbds);
						mpcdec_bitstream_forward(seek_helper, framebits);
					} else {
						mpcdec_bitstream_rewind(seek_helper, 20);
						break;
					}
				} else
					break;
				seek_helper->decodedframes++;
			}
			seek_helper->resync_counter = 32;
		}
	}
	mpc_copy_seek_to_mpci(mpci);
	return (mpci->decodedframes + mpci->resync_counter);	// !!!
}

struct mpxplay_infile_func_s IN_MPC_funcs = {
	(MPXPLAY_TAGTYPE_PUT_SUPPORT(MPXPLAY_TAGTYPE_ID3V1 | MPXPLAY_TAGTYPE_APETAG)
	 | MPXPLAY_TAGTYPE_PUT_PRIMARY(MPXPLAY_TAGTYPE_APETAG)),
	&MPC_infile_preinit,
	&MPC_infile_deinit,
	&MPC_infile_check,
	&MPC_infile_check,
	&MPC_infile_open,
	&MPC_infile_close,
	&MPC_infile_decode,
	&MPC_infile_fseek,
	&MPC_infile_clearbuffs,
	NULL,
	NULL,
	NULL,
	{"MPC", NULL}
};

static void AD_MPC_get_analiser_bands(struct mpxplay_audio_decoder_info_s *adi, unsigned int bandnum, unsigned long *banddataptr)
{
	struct mpc_decoder_data *mpci = (struct mpc_decoder_data *)adi->private_data;
	struct mpxsynth_data_s *sd = mpci->synthdata;
	sd->analiser_bandnum = bandnum;
	sd->analiser_banddata = banddataptr;
}

static void AD_MPC_set_eq(struct mpxplay_audio_decoder_info_s *adi, unsigned int bandnum, unsigned long *band_freqs, float *band_powers)
{
	struct mpc_decoder_data *mpci = (struct mpc_decoder_data *)adi->private_data;
	struct mpxsynth_data_s *sd = mpci->synthdata;
	sd->eq_bandnum = bandnum;
	sd->eq_freqs = band_freqs;
	sd->eq_powers = band_powers;
	mpxdec_layer3_eq_config(sd);
}

static struct mpxplay_audio_decoder_func_s AD_MPC_funcs = {
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&AD_MPC_get_analiser_bands,
	&AD_MPC_set_eq,
	0,
	0,
	{{0, NULL}}
};

#endif							// MPXPLAY_LINK_INFILE_MPC
