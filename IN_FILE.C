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
//function: in-file handling - main/common routines

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_OUTPUT NULL
#define MPXPLAY_USE_DEBUGMSG 1

#include "in_file.h"
#include "mpxinbuf.h"
#include "au_mixer\au_mixer.h"
#include "au_mixer\mix_func.h"
#include "au_cards\au_cards.h"
#include "decoders\decoders.h"
#include "deparser\tagging.h"
#include "newfunc\dll_load.h"
#include "newfunc\newfunc.h"
#include "display\display.h"

extern frame fr[3];

//parsers
extern struct mpxplay_infile_func_s IN_AAC_funcs, IN_AC3_funcs, IN_APE_funcs;
extern struct mpxplay_infile_func_s IN_CDW_funcs, IN_FLAC_funcs;
extern struct mpxplay_infile_func_s IN_MP2_funcs, IN_MP3_funcs, IN_MPC_funcs;
extern struct mpxplay_infile_func_s IN_OGG_funcs, IN_WAVPACK_funcs;

//containers
extern struct mpxplay_infile_func_s IN_ASF_funcs, IN_AVI_funcs, IN_MP4_funcs;
extern struct mpxplay_infile_func_s IN_WAV_funcs, IN_AIF_funcs;
extern struct mpxplay_infile_func_s IN_FFMPG_funcs;

static struct mpxplay_infile_func_s *all_infile_funcs[] = {
#ifdef MPXPLAY_LINK_INFILE_WAV	// first (it's possible with other filename extensions (ie:MP3))
	&IN_WAV_funcs,
	&IN_AIF_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_ASF
	&IN_ASF_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_AVI
	&IN_AVI_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_MP4
	&IN_MP4_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_FFMPG
	&IN_FFMPG_funcs,
#endif

#ifdef MPXPLAY_LINK_INFILE_AC3
	&IN_AC3_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_APE
	&IN_APE_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_CDW
	&IN_CDW_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_FLAC
	&IN_FLAC_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_MPC
	&IN_MPC_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_OGG
	&IN_OGG_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_WAVPACK
	&IN_WAVPACK_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_MPX
	&IN_MP3_funcs,
	&IN_MP2_funcs,
#endif
#ifdef MPXPLAY_LINK_INFILE_AAC	// lasts (due to the worse autodetection)
	&IN_AAC_funcs,
#endif
	NULL
};

#define LAST_INFILE_FUNCNUM (sizeof(all_infile_funcs)/sizeof(mpxplay_infile_func_s *)-2)

#ifdef MPXPLAY_LINK_DLLLOAD

static struct infiledlltype_s {
	unsigned int dlltype;
	unsigned int modulever;
} infiledlltypes[] = {
	{
	MPXPLAY_DLLMODULETYPE_FILEIN_PARSER, MPXPLAY_DLLMODULEVER_FILEIN_PARSER}, {
	MPXPLAY_DLLMODULETYPE_FILEIN_CONTAINER, MPXPLAY_DLLMODULEVER_FILEIN_CONTAINER}, {
	0, 0}
};

#endif

static unsigned int infile_checkhead_by_ext(struct frame *frp, struct mpxplay_diskdrive_data_s *mdds, struct mpxplay_infile_func_s *infilefuncs, char *filename, char *extension,
											unsigned int *extfound);
static int infile_subdecode(struct frame *frp);

extern char *id3tagset[I3I_MAX + 1];
extern unsigned int crossfadepart, displaymode, desktopmode, videocontrol;
extern unsigned int stream_select_audio, channelmode;

static struct mpxplay_infile_info_s infile_infos[3];
static struct mpxplay_streampacket_info_s audiopacket_infos[3];
static struct mpxplay_audio_decoder_info_s audiodecoder_infos[3];
#ifdef MPXPLAY_LINK_VIDEO
static struct mpxplay_streampacket_info_s videopacket_infos[3];
static struct mpxplay_video_decoder_info_s videodecoder_infos[3];
#endif

//char mpxplay_tag_year[8];

int mpxplay_infile_decode(struct mpxplay_audioout_info_s *aui)
{
	long retcode_demuxer, retcode_decoder;
	unsigned int fileselect, retry_count_decoder;
	struct frame *frp0 = aui->mvp->frp0, *frp;

	for(fileselect = 0, frp = frp0; fileselect <= ((crossfadepart == CROSS_FADE) ? 1 : 0); fileselect++, frp++) {
		struct mpxplay_audio_decoder_info_s *adi = frp->infile_infos->audio_decoder_infos;
#ifdef MPXPLAY_LINK_VIDEO
		struct mpxplay_video_decoder_info_s *vdi = frp->infile_infos->video_decoder_infos;

		if((displaymode & DISP_GRAPHICAL) && (vdi->flags & VDI_CNTRLBIT_DECODEVIDEO))
			funcbit_enable(vdi->flags, VDI_CNTRLBIT_SHOWVIDEO);
		else
			funcbit_disable(vdi->flags, VDI_CNTRLBIT_SHOWVIDEO);
#endif

		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "infile_decode begin");

		if(frp->infile_infos->seektype) {
			if(frp->infile_funcs && frp->infile_funcs->seek_postprocess)
				frp->infile_funcs->seek_postprocess(frp->filebuf_funcs, frp, frp->infile_infos, frp->infile_infos->seektype);
			mpxplay_decoders_clearbuf(frp->infile_infos, frp, frp->infile_infos->seektype);
			funcbit_smp_disable(frp->infile_infos->seektype, MPX_SEEKTYPES_CLEARBUF);
		}

		if(frp->pcmdec_leftsamples < frp->pcmout_blocksize) {
			if(frp->pcmdec_leftsamples && (frp->pcmdec_storedsamples > frp->pcmdec_leftsamples))
				pds_memcpy(frp->pcmdec_buffer, frp->pcmdec_buffer + (frp->pcmdec_storedsamples - frp->pcmdec_leftsamples) * adi->bytespersample, frp->pcmdec_leftsamples * adi->bytespersample);
			frp->pcmdec_storedsamples = frp->pcmdec_leftsamples;
			retcode_demuxer = MPXPLAY_ERROR_INFILE_OK;
			retry_count_decoder = 32;	// if decoder doesn't give back correct retcode (if it always give back OK)
			do {
				// fails at end of wav files...
				//if((frp->pcmout_blocksize==(adi->pcm_framelen*adi->outchannels)) && !frp->pcmdec_storedsamples) // !!! can we trust in pcm_framelen?
				// adi->pcm_bufptr=frp->pcmout_buffer; // to avoid an extra memcpy in MIXER_conversion
				//else
				adi->pcm_bufptr = frp->pcmdec_buffer + frp->pcmdec_storedsamples * adi->bytespersample;
				adi->pcm_samplenum = 0;
				mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "infile_decode demux");
				if(retcode_demuxer == MPXPLAY_ERROR_INFILE_OK)
					retcode_demuxer = infile_subdecode(frp);
				mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "infile_decode decode");
				retcode_decoder = mpxplay_decoders_decode(frp->infile_infos);
				frp->pcmdec_storedsamples += adi->pcm_samplenum;
				mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "decode ss:%4d bps:%d fs:%4d ba:%d %8.8X %8.8X ", frp->pcmdec_storedsamples,
							   adi->bytespersample, adi->pcm_framelen, frp->infile_infos->audio_stream->block_align, frp->pcmdec_buffer, adi->pcm_bufptr);
				if(frp->pcmdec_storedsamples >= frp->pcmout_blocksize)
					break;
				if((adi->infobits & ADI_CNTRLBIT_BITSTREAMOUT) && adi->pcm_samplenum)
					break;
			} while((--retry_count_decoder) && ((retcode_demuxer == MPXPLAY_ERROR_INFILE_OK) || (retcode_decoder == MPXPLAY_ERROR_INFILE_OK)));

			frp->pcmdec_leftsamples = frp->pcmdec_storedsamples;

			if(!frp->pcmdec_storedsamples) {
				if(fileselect && ((retcode_demuxer == MPXPLAY_ERROR_INFILE_EOF) || (retcode_demuxer == MPXPLAY_ERROR_INFILE_NODATA))) {	// if crossfade
					mpxplay_decoders_reset(frp->infile_infos, frp);
					crossfade_part_step(aui->mvp);
				} else if((retcode_demuxer == MPXPLAY_ERROR_INFILE_EOF) || (retcode_demuxer == MPXPLAY_ERROR_INFILE_NODATA) || (retcode_demuxer == MPXPLAY_ERROR_INFILE_SYNC_IN)
						  || (retcode_demuxer == MPXPLAY_ERROR_INFILE_RESYNC)) {
					funcbit_disable(aui->card_controlbits, AUINFOS_CARDCNTRLBIT_DMADONTWAIT);
					return retcode_demuxer;
				}
			}
		}

		if(frp->pcmdec_leftsamples) {
			//if(adi->pcm_bufptr==frp->pcmout_buffer) // at matched blocksizes (in==out)
			// frp->pcmdec_leftsamples-=MIXER_conversion(aui,adi,frp,(PCM_CV_TYPE_S *)(adi->pcm_bufptr),frp->pcmdec_leftsamples);
			//else
			frp->pcmdec_leftsamples -=
				MIXER_conversion(aui, adi, frp, (PCM_CV_TYPE_S *) (frp->pcmdec_buffer + (frp->pcmdec_storedsamples - frp->pcmdec_leftsamples) * adi->bytespersample), frp->pcmdec_leftsamples);

			frp->frameNum++;
			frp->framecounter++;

			mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "get    ss:%4d ls:%4d bsc:%4d %8.8X ", frp->pcmout_storedsamples, frp->pcmdec_leftsamples, frp->pcmout_blocksize, frp->pcmout_buffer);
		}
	}

	MIXER_main(aui, frp0->infile_infos->audio_decoder_infos, frp0);

	AU_writedata(aui);

	return 0;
}

//--------------------------------------------------------------------------
// is extension supported?
unsigned int mpxplay_infile_check_extension(char *filename, struct mpxplay_diskdrive_data_s *mdds)
{
	unsigned int i, j;
	char *extension;

	extension = pds_strrchr(filename, '.');
	if(extension && extension[1]) {
		extension++;
		for(i = 0; i <= LAST_INFILE_FUNCNUM; i++) {
			j = 0;
			while(all_infile_funcs[i]->file_extensions[j]) {
				if(pds_stricmp(all_infile_funcs[i]->file_extensions[j], extension) == 0)
					return 1;
				j++;
			}
		}
#ifdef MPXPLAY_LINK_DLLLOAD		// search file-extension in DLLs
		i = 0;
		do {
			mpxplay_module_entry_s *dll_infile = NULL;
			do {
				dll_infile = newfunc_dllload_getmodule(infiledlltypes[i].dlltype, 0, NULL, dll_infile);
				if(dll_infile) {
					if(dll_infile->module_structure_version == infiledlltypes[i].modulever) {	// !!!
						struct mpxplay_infile_func_s *inf = (struct mpxplay_infile_func_s *)dll_infile->module_callpoint;
						if(inf->file_extensions[0]) {
							j = 0;
							do {
								if(pds_stricmp(inf->file_extensions[j], extension) == 0)
									return 1;
								j++;
							} while(inf->file_extensions[j]);
						} else {
							if(infile_checkhead_by_ext(&fr[3], mdds, inf, filename, extension, &j))
								return 1;
						}
					}
				}
			} while(dll_infile);
			i++;
		} while(infiledlltypes[i].dlltype);
#endif
	}
	return 0;
}

unsigned int mpxplay_infile_get_samplenum_per_frame(unsigned int freq)
{
	unsigned int samplenum;
	if(freq >= 32000 && freq <= 48000)	// match with MPEG 1.0 and AC3 freqs (samplenum match at crossfade)
		samplenum = PCM_OUTSAMPLES;
	else if((freq >= 16000 && freq <= 24000) || (freq >= 8000 && freq <= 12000))	// match with MPEG 2.x freqs
		samplenum = PCM_OUTSAMPLES / 2;
	else
		samplenum = ((PCM_OUTSAMPLES * freq) + 22050) / 44100;

	if(samplenum > PCM_MAX_SAMPLES)
		samplenum = PCM_MAX_SAMPLES;

	// fixme: round it to get matched samplenums at -cf after freq conversion

	return samplenum;
}

void miis_to_frp(struct mpxplay_infile_info_s *miis, struct frame *frp)
{
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	long samplenum_frame;

	frp->filetype = HFT_FILE_INT;
	frp->filesize = miis->filesize;

	// calculate samplenum
	if((adi->infobits & ADI_FLAG_BITSTREAMOUT) && adi->pcm_framelen)
		samplenum_frame = adi->pcm_framelen;
	else {
		samplenum_frame = mpxplay_infile_get_samplenum_per_frame(adi->freq);
		if(!adi->pcm_framelen)	// ie: pcm decoder don't give back a specified length, we configure it
			adi->pcm_framelen = samplenum_frame;
	}

	// calculate allframes (if parser/demuxer didn't set)
	if(miis->allframes <= 1) {
		if(miis->timemsec)		// containers (AVI,ASF,MP4,OGG,WAV)
			miis->allframes = (long)((float)miis->timemsec / 1000.0 * (float)adi->freq / (float)samplenum_frame);
		else if(adi->bitrate)	// pure bitstreams (AC3,DTS,MP3,etc.)
			miis->allframes = (long)((float)frp->filesize / (float)adi->bitrate * (float)adi->freq / (1000.0 / 8.0) / (float)samplenum_frame);
		if(miis->allframes < 1)
			miis->allframes = 1;
	}
	frp->allframes = miis->allframes;

	// calculate timesec (if parser/demuxer didn't set)
	if(!miis->timemsec) {
		if(miis->allframes)		// MP3-VBR,MPC (from header); AC3,DTS (calculated from bitrate)
			miis->timemsec = 1000.0 * (float)miis->allframes * (float)samplenum_frame / (float)adi->freq;
		else					// ???
		if(adi->bitrate)
			miis->timemsec = (float)frp->filesize * 8.0 / (float)adi->bitrate;
	}

	if(adi->infobits & ADI_FLAG_FLOATOUT)
		adi->bytespersample = sizeof(PCM_CV_TYPE_F);
	else if(!adi->bytespersample)
		adi->bytespersample = (adi->bits + 7) >> 3;
}

#ifdef MPXPLAY_LINK_VIDEO
void mpxplay_infile_video_config_open(struct mpxplay_videoout_info_s *voi, struct mpxplay_video_decoder_info_s *vdi)
{
	if(videocontrol & MPXPLAY_VIDEOCONTROL_DECODEVIDEO)
		funcbit_enable(vdi->flags, VDI_CNTRLBIT_DECODEVIDEO);
	vdi->picture = voi->screen_linear_ptr;	// !!!
	vdi->screen_res_x = voi->screen_res_x;
	vdi->screen_res_y = voi->screen_res_y;
}
#endif

static unsigned long infile_get_header_autodetect(struct frame *frp, struct mpxplay_diskdrive_data_s *mdds, char *filename)
{
	unsigned int i;
	int retcode;

	for(i = 0; i <= LAST_INFILE_FUNCNUM; i++) {
		if(all_infile_funcs[i]->detect) {
			mpxplay_infile_close(frp);
			frp->mdds = mdds;
			frp->infile_funcs = all_infile_funcs[i];
			mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "chkaut: %4.4s ", frp->infile_funcs->file_extensions[0]);
			if(frp->infile_funcs->own_filehand_funcs)
				frp->filehand_funcs = frp->infile_funcs->own_filehand_funcs;
			retcode = frp->infile_funcs->detect(frp->filebuf_funcs, frp, filename, frp->infile_infos);
			switch (retcode) {
			case MPXPLAY_ERROR_INFILE_FILEOPEN:
				return 0;
			case MPXPLAY_ERROR_INFILE_OK:
				mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "infile: %4.4s ", frp->infile_funcs->file_extensions[0]);
				if(mpxplay_decoders_open(frp->infile_infos, frp) == MPXPLAY_ERROR_INFILE_OK) {
					mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "decopen: %4.4s ", frp->infile_funcs->file_extensions[0]);
					miis_to_frp(frp->infile_infos, frp);	// !!!
					return 1;
				}
			}
		}
	}
	mpxplay_infile_close(frp);
	return 0;
}

static unsigned int infile_checkhead_by_ext(struct frame *frp, struct mpxplay_diskdrive_data_s *mdds, struct mpxplay_infile_func_s *infilefuncs, char *filename, char *extension,
											unsigned int *extfound)
{
	unsigned int j = 0;
	do {
		if(!infilefuncs->file_extensions[0] || pds_stricmp(infilefuncs->file_extensions[j], extension) == 0) {
			if(infilefuncs->file_extensions[j])
				*extfound = 1;
			if(infilefuncs->check) {
				mpxplay_infile_close(frp);
				frp->mdds = mdds;
				frp->infile_funcs = infilefuncs;
				if(frp->infile_funcs->own_filehand_funcs)
					frp->filehand_funcs = frp->infile_funcs->own_filehand_funcs;
				if(frp->infile_funcs->check(frp->filebuf_funcs, frp, filename, frp->infile_infos) == MPXPLAY_ERROR_INFILE_OK) {
					mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "oke: %4.4s ", frp->infile_funcs->file_extensions[j]);
					if(mpxplay_decoders_open(frp->infile_infos, frp) == MPXPLAY_ERROR_INFILE_OK) {
						miis_to_frp(frp->infile_infos, frp);	// !!!
						return 1;
					}
				}
			}
		}
		if(!infilefuncs->file_extensions[0])
			break;
		j++;
	} while(infilefuncs->file_extensions[j]);
	mpxplay_infile_close(frp);
	return 0;
}

unsigned long mpxplay_infile_get_header_by_ext(struct frame *frp, struct mpxplay_diskdrive_data_s *mdds, char *filename)
{
	unsigned int i, extfound = 0;
	char *extension;

	mpxplay_infile_close(frp);
	frp->mdds = mdds;

	extension = pds_strrchr(filename, '.');
	if(extension && extension[1]) {
		extension++;

#ifdef MPXPLAY_LINK_DLLLOAD		// !!! dll can override internal file parser
		// check DLLs (first)
		i = 0;
		do {
			mpxplay_module_entry_s *dll_infile = NULL;
			do {
				dll_infile = newfunc_dllload_getmodule(infiledlltypes[i].dlltype, 0, NULL, dll_infile);
				if(dll_infile) {
					if(dll_infile->module_structure_version == infiledlltypes[i].modulever) {	// !!!
						if(infile_checkhead_by_ext(frp, mdds, (struct mpxplay_infile_func_s *)dll_infile->module_callpoint, filename, extension, &extfound)) {
							frp->filetype = HFT_FILE_DLL;
							//frp->infile_funcs=(struct mpxplay_infile_func_s *)dll_infile;
							//newfunc_dllload_disablemodule(0,0,NULL,dll_infile);
							return 1;
						}
						//newfunc_dllload_disablemodule(0,0,NULL,dll_infile);
					}
				}
			} while(dll_infile);
			i++;
		} while(infiledlltypes[i].dlltype);
#endif
		//check builtin infiles (last)
		for(i = 0; i <= LAST_INFILE_FUNCNUM; i++)
			if(infile_checkhead_by_ext(frp, mdds, all_infile_funcs[i], filename, extension, &extfound))
				return 1;
	}

	if(!extfound && !funcbit_test(desktopmode, DTM_EDIT_ALLFILES))	// unknown or missing file extension
		if(infile_get_header_autodetect(frp, mdds, filename))
			return 1;

	return 0;
}

static void infile_assign_funcs(struct frame *frp)
{
	if(frp->infile_funcs) {
		/*if(frp->filetype==HFT_FILE_DLL){
		   mpxplay_module_entry_s *dll_infile=(mpxplay_module_entry_s *)frp->infile_funcs;
		   if(newfunc_dllload_reloadmodule(dll_infile))
		   frp->infile_funcs=(struct mpxplay_infile_func_s *)dll_infile->module_callpoint;
		   else
		   frp->infile_funcs=NULL;
		   }
		   if(frp->infile_funcs) */
		if(frp->infile_funcs->own_filehand_funcs)
			frp->filehand_funcs = frp->infile_funcs->own_filehand_funcs;
	}
}

char *mpxplay_infile_get_id3tag(struct frame *frp, char **id3ip, char *id3p)
{
	struct mpxplay_filehand_buffered_func_s *fbfs = frp->filebuf_funcs;
	struct mpxplay_infile_info_s *miis = frp->infile_infos;
	struct mpxplay_textconv_func_s *mpxplay_textconv_funcs = &mpxplay_playlist_textconv_funcs;

	infile_assign_funcs(frp);
	if(frp->infile_funcs) {
		miis->standard_id3tag_support = frp->infile_funcs->flags & MPXPLAY_TAGTYPE_FUNCMASK;
		if(MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support)) {	// common/standard id3tag write
			if(MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support) & MPXPLAY_TAGTYPE_ID3V2) {
				if(mpxplay_tagging_id3v2_check(fbfs, frp)) {
					id3p = mpxplay_tagging_id3v2_get(fbfs, frp, miis, id3ip, id3p, mpxplay_textconv_funcs);
					MPXPLAY_TAGTYPE_SET_FOUND(miis->standard_id3tag_support, MPXPLAY_TAGTYPE_ID3V2);
				}
			}
			if(MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support) & MPXPLAY_TAGTYPE_ID3V1) {
				if(mpxplay_tagging_id3v1_check(fbfs, frp)) {
					id3p = mpxplay_tagging_id3v1_get(fbfs, frp, id3ip, id3p, mpxplay_textconv_funcs);
					MPXPLAY_TAGTYPE_SET_FOUND(miis->standard_id3tag_support, MPXPLAY_TAGTYPE_ID3V1);
				}
			}
			if((MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support) & MPXPLAY_TAGTYPE_APETAG) && !(MPXPLAY_TAGTYPE_GET_FOUND(miis->standard_id3tag_support) & MPXPLAY_TAGTYPE_ID3V1)) {
				if(mpxplay_tagging_apetag_check(fbfs, frp)) {
					id3p = mpxplay_tagging_apetag_get(fbfs, frp, miis, id3ip, id3p, mpxplay_textconv_funcs);
					MPXPLAY_TAGTYPE_SET_FOUND(miis->standard_id3tag_support, MPXPLAY_TAGTYPE_APETAG);
				}
			}

		} else {
			if(frp->infile_funcs->get_id3tag)
				id3p = frp->infile_funcs->get_id3tag(frp->filebuf_funcs, frp, frp->infile_infos, id3ip, id3p, &mpxplay_playlist_textconv_funcs);
		}
	}
	return id3p;
}

int mpxplay_infile_write_id3tag(struct frame *frp, char *filename, char **id3ip)
{
	struct mpxplay_filehand_buffered_func_s *fbfs = frp->filebuf_funcs;
	struct mpxplay_infile_info_s *miis = frp->infile_infos;
	struct mpxplay_textconv_func_s *mpxplay_textconv_funcs = &mpxplay_playlist_textconv_funcs;
	int error = MPXPLAY_ERROR_INFILE_CANTOPEN;
	unsigned int i, is_id3v1, is_id3v2, is_apetag;
	int id3v1_retcode, id3v2_retcode, apetag_retcode;

	if(frp->filetype == HFT_DFT)
		return MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE;

	infile_assign_funcs(frp);

	if(frp->infile_funcs) {
		miis->standard_id3tag_support = frp->infile_funcs->flags & MPXPLAY_TAGTYPE_FUNCMASK;

		if(frp->infile_funcs->write_id3tag || MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support)) {

			pds_fileattrib_reset(filename, _A_RDONLY);

			if(!fbfs->fopen_write(frp, filename))
				return error;

			for(i = 0; i <= I3I_MAX; i++)
				if(id3tagset[i])
					id3ip[i] = id3tagset[i];

			//if(!id3ip[I3I_YEAR])
			// id3ip[I3I_YEAR]=&mpxplay_tag_year[0];

			if(MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support)) {	// common/standard id3tag write
				is_id3v1 = 0;
				is_id3v2 = 0;
				is_apetag = 0;
				id3v1_retcode = 0;
				id3v2_retcode = 0;
				apetag_retcode = 0;

				if(MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support) & MPXPLAY_TAGTYPE_ID3V2) {
					if(mpxplay_tagging_id3v2_check(fbfs, frp)) {
						id3v2_retcode = mpxplay_tagging_id3v2_put(fbfs, frp, miis, id3ip, mpxplay_textconv_funcs);
						if(id3v2_retcode == MPXPLAY_ERROR_INFILE_OK)	// !!!
							is_id3v2 = 1;	// hack to put id3v1 if v2 failed
					}
				}
				if(MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support) & MPXPLAY_TAGTYPE_ID3V1) {
					if(mpxplay_tagging_id3v1_check(fbfs, frp)) {
						id3v1_retcode = mpxplay_tagging_id3v1_put(fbfs, frp, miis, id3ip, mpxplay_textconv_funcs);
						is_id3v1 = 1;
					}
				}
				if(MPXPLAY_TAGTYPE_GET_SUPPORT(miis->standard_id3tag_support) & MPXPLAY_TAGTYPE_APETAG) {
					if(mpxplay_tagging_apetag_check(fbfs, frp)) {
						apetag_retcode = mpxplay_tagging_apetag_put(fbfs, frp, miis, id3ip, mpxplay_textconv_funcs);
						is_apetag = 1;
					}
				}
				if(!is_id3v1 && !is_id3v2 && !is_apetag) {	// create new tag
					switch (MPXPLAY_TAGTYPE_GET_PRIMARY(miis->standard_id3tag_support)) {
					case MPXPLAY_TAGTYPE_ID3V1:
						id3v1_retcode = mpxplay_tagging_id3v1_put(fbfs, frp, miis, id3ip, mpxplay_textconv_funcs);
						break;
					case MPXPLAY_TAGTYPE_ID3V2:
						id3v2_retcode = mpxplay_tagging_id3v2_put(fbfs, frp, miis, id3ip, mpxplay_textconv_funcs);
						break;
					case MPXPLAY_TAGTYPE_APETAG:
						apetag_retcode = mpxplay_tagging_apetag_put(fbfs, frp, miis, id3ip, mpxplay_textconv_funcs);
						break;
					default:
						error = MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE;
					}
				}

				error = min(id3v1_retcode, id3v2_retcode);
				error = min(error, apetag_retcode);

			} else {			// special / not standard id3tag write (no such currently)
				error = frp->infile_funcs->write_id3tag(fbfs, frp, frp->infile_infos, id3ip, &mpxplay_playlist_textconv_funcs);
			}

			fbfs->fclose(frp);
		} else {
			error = MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE;
		}
	}

	return error;
}

//--------------------------------------------------------------------------
static void clear_infile_infos(struct mpxplay_infile_info_s *miis, struct frame *frp)
{
	if(miis) {					// it can be NULL at 'soundcard init failed'
		miis->filesize = 0;
		miis->timemsec = 0;
		miis->allframes = 1;
		miis->private_data = NULL;
		miis->longname = NULL;
		miis->seektype = 0;
		miis->standard_id3tag_support = 0;

		miis->audio_decoder_funcs = NULL;
		miis->video_decoder_funcs = NULL;

		mpxplay_decoders_clear(miis, frp);
	}
}

static void clear_frame(struct frame *frp)
{
	frp->filetype = 0;
	frp->mpxfilept = 0;
	frp->filesize = 0;
	frp->filepos = 0;
	frp->frameNum = 0;
	frp->framecounter = 0;
	frp->index_start = 0;
	frp->index_end = 0;
	frp->index_len = 1;			//
	frp->allframes = 1;			// division by zero bugfix
	frp->timesec = 0;

	frp->buffertype = 0;
	frp->prebuffergetp = 0;
	frp->prebufferputp = 0;
	frp->prebufferbytes_rewind = 0;
	frp->prebufferbytes_forward = 0;
	frp->prebuffer_seek_retry = PREBUFFER_SEEKRETRY_INVALID;

	frp->mdds = NULL;
	frp->filehand_funcs = NULL;
	frp->filehand_datas = NULL;

	frp->infile_funcs = NULL;

	frp->pcmout_storedsamples = 0;
}

//------------------------------------------------------------------------

int mpxplay_infile_open(struct frame *frp, char *filename)
{
	infile_assign_funcs(frp);

	if(frp->infile_funcs && frp->infile_funcs->open) {
		if(frp->infile_funcs->open(frp->filebuf_funcs, frp, filename, frp->infile_infos) == MPXPLAY_ERROR_INFILE_OK) {
			if(mpxplay_decoders_open(frp->infile_infos, frp) == MPXPLAY_ERROR_INFILE_OK) {
				miis_to_frp(frp->infile_infos, frp);	// !!!
				return 1;
			}
		}
		mpxplay_infile_close(frp);
	}
	return 0;
}

void mpxplay_infile_reset(struct frame *frp)
{
	frp->infile_infos->seektype = 0;
	frp->prebufferbytes_forward = 0;
	frp->prebufferbytes_rewind = 0;
}

void mpxplay_infile_close(struct frame *frp)
{
	mpxplay_timer_deletefunc(mpxplay_infile_close, frp);
	mpxplay_infile_reset(frp);
	if(frp->infile_funcs && frp->infile_funcs->close)
		frp->infile_funcs->close(frp->filebuf_funcs, frp, frp->infile_infos);
	mpxplay_decoders_close(frp->infile_infos, frp);
	clear_infile_infos(frp->infile_infos, frp);
	clear_frame(frp);
}

static int infile_subdecode(struct frame *frp)	// demux (AVI,ASF) or direct decoding (APE,WAV)
{
	int retcode = MPXPLAY_ERROR_INFILE_EOF;

	if(frp->infile_funcs && frp->infile_funcs->decode) {
		if(frp->infile_infos->audio_stream->bs_leftbytes)
			retcode = MPXPLAY_ERROR_INFILE_OK;
		else
			retcode = frp->infile_funcs->decode(frp->filebuf_funcs, frp, frp->infile_infos);
	}

	return retcode;
}

long mpxplay_infile_fseek(struct frame *frp, long framenum_set)
{
	if(frp->infile_funcs && frp->infile_funcs->fseek)
		framenum_set = frp->infile_funcs->fseek(frp->filebuf_funcs, frp, frp->infile_infos, framenum_set);
	funcbit_disable(frp->infile_infos->seektype, MPX_SEEKTYPES_FSEEK);

	//mpxplay_decoders_setpos(frp->infile_infos,framenum_set);

	return framenum_set;
}

//-------------------------------------------------------------------------
static unsigned int infile_initialized;

void mpxplay_infile_init(struct mainvars *mvp)
{
	struct frame *frp = mvp->frp0;
	unsigned int i = 0;

	do {
		struct mpxplay_infile_info_s *miis = &infile_infos[i];
		frp->infile_infos = miis;
		miis->audio_stream = &audiopacket_infos[i];
		miis->audio_stream->stream_select = stream_select_audio;
		miis->audio_decoder_infos = &audiodecoder_infos[i];
#ifdef MPXPLAY_LINK_VIDEO
		miis->video_stream = &videopacket_infos[i];
		miis->video_decoder_infos = &videodecoder_infos[i];
#endif
		mpxplay_decoders_alloc(frp, ((i < 2) ? 1 : 0));
		clear_infile_infos(miis, frp);
		frp++;
		i++;
	} while(i < 3);

	for(i = 0; i <= LAST_INFILE_FUNCNUM; i++) {
		if(all_infile_funcs[i]->preinit)
			all_infile_funcs[i]->preinit();
	}

	mpxplay_decoders_preinit();

	//i=pds_getdate();
	//sprintf(&mpxplay_tag_year[0],"%4d/%2.2d",i>>16,(i>>8)&0xff);
	infile_initialized = 1;
}

void mpxplay_infile_deinit(void)
{
	unsigned int i;

	if(!infile_initialized)
		return;

	for(i = 0; i < 2; i++) {
		mpxplay_infile_close(&fr[i]);
		mpxplay_decoders_free(fr[i].infile_infos, &fr[i]);
	}

	for(i = 0; i <= LAST_INFILE_FUNCNUM; i++) {
		if(all_infile_funcs[i]->deinit)
			all_infile_funcs[i]->deinit();
	}

	mpxplay_decoders_deinit();
}

struct frame *mpxplay_infile_frame_alloc(struct frame *frp)
{
	struct mpxplay_infile_info_s *miis;

	pds_memset(frp, 0, sizeof(struct frame));
	mpxplay_mpxinbuf_assign_funcs(frp);
	//if(!mpxinbuf_alloc_ringbuffer(frp,PREBUFFERBLOCKSIZE_CHECK)) // ???
	// goto err_out_fa;
	miis = (struct mpxplay_infile_info_s *)calloc(1, sizeof(*frp->infile_infos));
	if(!miis)
		goto err_out_fa;
	frp->infile_infos = miis;
	miis->audio_stream = (struct mpxplay_streampacket_info_s *)calloc(1, sizeof(*miis->audio_stream));
	if(!miis->audio_stream)
		goto err_out_fa;
	miis->audio_stream->stream_select = stream_select_audio;
	miis->audio_decoder_infos = (struct mpxplay_audio_decoder_info_s *)calloc(1, sizeof(*miis->audio_decoder_infos));
	if(!miis->audio_decoder_infos)
		goto err_out_fa;
#ifdef MPXPLAY_LINK_VIDEO
	miis->video_stream = (struct mpxplay_streampacket_info_s *)calloc(1, sizeof(*miis->video_stream));
	if(!miis->video_stream)
		goto err_out_fa;
	miis->video_decoder_infos = (struct mpxplay_video_decoder_info_s *)calloc(1, sizeof(*miis->video_decoder_infos));
	if(!miis->video_decoder_infos)
		goto err_out_fa;
#endif
	mpxplay_decoders_alloc(frp, 1);

	clear_infile_infos(miis, frp);

	return frp;

  err_out_fa:
	mpxplay_infile_frame_free(frp);
	return NULL;
}

void mpxplay_infile_frame_free(struct frame *frp)
{
	struct mpxplay_infile_info_s *miis;
	if(frp) {
		miis = frp->infile_infos;
		if(miis) {
			mpxplay_infile_close(frp);	// ???
			mpxplay_decoders_free(miis, frp);
			if(miis->audio_stream)
				free(miis->audio_stream);
			if(miis->audio_decoder_infos)
				free(miis->audio_decoder_infos);
#ifdef MPXPLAY_LINK_VIDEO
			if(miis->video_stream)
				free(miis->video_stream);
			if(miis->video_decoder_infos)
				free(miis->video_decoder_infos);
#endif
			free(miis);
			frp->infile_infos = NULL;
		}
		if(frp->prebufferbegin)
			free(frp->prebufferbegin);
		if(frp->pcmdec_buffer)
			free(frp->pcmdec_buffer);
		if(frp->pcmout_buffer)
			free(frp->pcmout_buffer);
		pds_memset(frp, 0, sizeof(struct frame));
	}
}
