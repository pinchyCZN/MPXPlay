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
//function: AVI demuxing  (based on the FFMPEG lib)

#include "in_file.h"

#ifdef MPXPLAY_LINK_INFILE_AVI

#include "newfunc\newfunc.h"
#include <malloc.h>

#define AVIFLAG_IDX1_KEYFRAME 0x10

#define AST_SEEKFLAG_KEYFRAME 0x01

typedef struct avi_stream_data_s {
	unsigned int streamtype;
	mpxp_uint32_t codec_tag;
	unsigned int need_parsing;

	unsigned int stream_index;

	long start;
	long duration;
	long nb_frames;
	unsigned long bit_rate;
	int scale;
	int rate;
	unsigned long bufsize;

	mpxp_uint8_t *extradata;
	unsigned long extradata_size;

	// audio
	unsigned int channels;
	unsigned long sample_rate;
	unsigned int block_align;
	unsigned int bits_per_sample;
	int sample_size;

	//video
	unsigned long video_res_x;
	unsigned long video_res_y;
	unsigned int video_bpp;

	//mpxp_int64_t frame_offset; // current frame (video) or byte (audio) counter

	unsigned long seek_entries;
	mpxp_uint32_t *seek_table;
	mpxp_uint8_t *seek_flags;
} avi_stream_data_s;

typedef struct avi_demuxer_data_s {
	mpxp_uint32_t riff_end;
	mpxp_uint32_t movi_end;
	mpxp_uint32_t movi_list;
	int nb_streams;
	int is_odml;
	//int non_interleaved;

	struct avi_stream_data_s *avs;	// all streams
	struct avi_stream_data_s *ast_audio;
	struct avi_stream_data_s *ast_video;
	struct avi_stream_data_s *ast_subtitle;
} avi_demuxer_data_s;

static unsigned int avi_read_header(struct avi_demuxer_data_s *avii, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis);
static unsigned int avi_read_wavheader(avi_stream_data_s * ast, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, int size);
static unsigned int avi_load_index(struct avi_demuxer_data_s *avi, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds);
static struct avi_stream_data_s *avi_get_stream(struct avi_demuxer_data_s *avi, unsigned int streamtype, struct mpxplay_streampacket_info_s *spi);
static void avi_assign_audio(struct avi_demuxer_data_s *avi, struct mpxplay_infile_info_s *miis, unsigned int full_load);
static void avi_assign_video(struct avi_demuxer_data_s *avi, struct mpxplay_infile_info_s *miis, unsigned int full_load);

static struct avi_demuxer_data_s *avi_open_and_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis, unsigned int full_load)
{
	struct avi_demuxer_data_s *avii = NULL;

	if(!fbfs->fopen_read(fbds, filename, 2048))	//((full_load)? 0:2048)))
		return NULL;

	miis->filesize = fbfs->filelength(fbds);
	if(miis->filesize < 16)
		return NULL;

	avii = (struct avi_demuxer_data_s *)calloc(1, sizeof(struct avi_demuxer_data_s));
	if(!avii)
		return NULL;
	miis->private_data = avii;

	if(!avi_read_header(avii, fbfs, fbds, miis))
		return NULL;

	avii->ast_audio = avi_get_stream(avii, MPXPLAY_SPI_STREAMTYPE_AUDIO, miis->audio_stream);
#ifdef MPXPLAY_LINK_VIDEO
	avii->ast_video = avi_get_stream(avii, MPXPLAY_SPI_STREAMTYPE_VIDEO, miis->video_stream);
#endif

	if(!avii->ast_audio && !avii->ast_video)
		return NULL;

	if(full_load)
		if(!avi_load_index(avii, fbfs, fbds))
			return NULL;

	avi_assign_audio(avii, miis, full_load);
#ifdef MPXPLAY_LINK_VIDEO
	avi_assign_video(avii, miis, full_load);
#endif

	return avii;
}

static int AVI_infile_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct avi_demuxer_data_s *avii = NULL;

	avii = avi_open_and_check(fbfs, fbds, filename, miis, 0);
	if(!avii)
		goto err_out_check;

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_check:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static int AVI_infile_open(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *filename, struct mpxplay_infile_info_s *miis)
{
	struct avi_demuxer_data_s *avii;

	avii = avi_open_and_check(fbfs, fbds, filename, miis, 1);
	if(!avii)
		goto err_out_open;

	return MPXPLAY_ERROR_INFILE_OK;

  err_out_open:
	return MPXPLAY_ERROR_INFILE_CANTOPEN;
}

static void AVI_infile_close(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct avi_demuxer_data_s *avii = (struct avi_demuxer_data_s *)miis->private_data;
	unsigned int i;
	if(avii) {
		if(avii->avs) {
			for(i = 0; i < avii->nb_streams; i++) {
				avi_stream_data_s *ast = &avii->avs[i];
				if(ast->extradata)
					free(ast->extradata);
				if(ast->seek_table)
					free(ast->seek_table);
				if(ast->seek_flags)
					free(ast->seek_flags);
			}
			free(avii->avs);
		}
		free(avii);
	}
	fbfs->fclose(fbds);
}

static int AVI_infile_decode(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	struct avi_demuxer_data_s *avi = (struct avi_demuxer_data_s *)miis->private_data;
	//int first=0;
	mpxp_uint32_t framepos;
	mpxp_uint8_t d[8];

  resync:

	framepos = fbfs->ftell(fbds);
	if(fbfs->fread(fbds, &d[0], 8) != 8) {
		if(fbfs->fseek(fbds, framepos, SEEK_SET) != framepos)
			return MPXPLAY_ERROR_INFILE_RESYNC;
		return MPXPLAY_ERROR_INFILE_NODATA;
	}
	//fprintf(stderr,"%c%c%c%c %d\n",d[0],d[1],d[2],d[3],PDS_GETB_LE32(&d[4]));

	do {
		mpxp_uint32_t framesize = PDS_GETB_LE32(&d[4]);
		mpxp_uint32_t streamnum;

		if(framepos >= avi->movi_end) {
			if(!avi->is_odml)
				return MPXPLAY_ERROR_INFILE_NODATA;
			if(framepos < avi->riff_end) {
				if(fbfs->fseek(fbds, avi->riff_end, SEEK_SET) != avi->riff_end)
					return MPXPLAY_ERROR_INFILE_RESYNC;
				goto resync;
			}
			if((framepos + framesize) >= fbfs->filelength(fbds))
				goto sync_skip;
		} else {
			if(((framepos + framesize) > avi->movi_end))
				goto sync_skip;
		}

		//fprintf(stderr,"%c%c%c%c fs:%9d fe:%9d me:%9d re:%9d %d\n",d[0],d[1],d[2],d[3],(unsigned long)framesize,(unsigned long)(framepos+framesize),(unsigned long)avi->movi_end,(unsigned long)avi->riff_end,avi->is_odml);

		if(PDS_GETB_LE32(&d[0]) == PDS_GET4C_LE32('J', 'U', 'N', 'K')) {
			if(fbfs->fseek(fbds, framesize, SEEK_CUR) < 0)
				return MPXPLAY_ERROR_INFILE_RESYNC;
			goto resync;
		}

		if((d[0] == 'i') && (d[1] == 'x')) {
			if((d[2] >= '0') && (d[2] <= '9') && (d[3] >= '0') && (d[3] <= '9')) {
				streamnum = ((unsigned int)d[2] - '0') * 10 + ((unsigned int)d[3] - '0');
				if(streamnum < avi->nb_streams) {
					if(fbfs->fseek(fbds, framesize, SEEK_CUR) < 0)
						return MPXPLAY_ERROR_INFILE_RESYNC;
					goto resync;
				}
			}
			goto sync_skip;
		}

		if((d[0] < '0') || (d[0] > '9') || (d[1] < '0') || (d[1] > '9'))
			goto sync_skip;

		streamnum = ((unsigned int)d[0] - '0') * 10 + ((unsigned int)d[1] - '0');

		if(streamnum < avi->nb_streams) {
			avi_stream_data_s *ast = &avi->avs[streamnum];
			struct mpxplay_streampacket_info_s *spi;
			if(framesize <= ast->bufsize) {
				switch (ast->streamtype) {
				case MPXPLAY_SPI_STREAMTYPE_AUDIO:
					if(d[2] != 'w' || d[3] != 'b')
						goto sync_skip;
					spi = miis->audio_stream;
					break;
#ifdef MPXPLAY_LINK_VIDEO
				case MPXPLAY_SPI_STREAMTYPE_VIDEO:
					if(d[2] != 'd' || d[3] != 'c')
						goto sync_skip;
					spi = miis->video_stream;
					break;
#endif
				default:
					if(fbfs->fseek(fbds, framesize, SEEK_CUR) < 0)
						return MPXPLAY_ERROR_INFILE_RESYNC;
					goto resync;
				}
				spi->bs_leftbytes = fbfs->fread(fbds, spi->bitstreambuf, framesize);
				if(spi->bs_leftbytes != framesize) {
					spi->bs_leftbytes = 0;
					if(fbfs->fseek(fbds, framepos, SEEK_SET) != framepos)
						return MPXPLAY_ERROR_INFILE_RESYNC;
					return MPXPLAY_ERROR_INFILE_NODATA;
				}
				if(framesize & 1)
					fbfs->get_byte(fbds);	// ??? word align
				if(miis->audio_stream->bs_leftbytes)
					break;
			}
		}

	  sync_skip:
		/*if(first<3){
		   if(streamnum<avi->nb_streams){
		   avi_stream_data_s *ast=&avi->avs[streamnum];
		   fprintf(stderr,"%c%c%c%c fs:%d bs:%d n:%d t:%d\n",d[0],d[1],d[2],d[3],framesize,ast->bufsize,streamnum,ast->streamtype);
		   }else
		   fprintf(stderr,"%c%c%c%c fs:%d n:%d\n",d[0],d[1],d[2],d[3],framesize,streamnum);
		   first++;
		   } */
		pds_memcpy(&d[0], &d[1], 7);
		if(fbfs->fread(fbds, &d[7], 1) != 1)
			return MPXPLAY_ERROR_INFILE_NODATA;
		framepos++;
	} while(1);

	return MPXPLAY_ERROR_INFILE_OK;
}

static long AVI_infile_fseek(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, long newmpxframenum)
{
	struct avi_demuxer_data_s *avi = (struct avi_demuxer_data_s *)miis->private_data;
	avi_stream_data_s *ast;
	long newfilepos = 0, filesize;
	long aviframe;
	float avgbytes_per_aviframe;

#ifdef MPXPLAY_LINK_VIDEO
	if(miis->seektype & MPX_SEEKTYPE_VIDEO) {
		ast = avi->ast_video;
		if(!ast)
			ast = avi->ast_audio;
	} else
#endif
	{
		ast = avi->ast_audio;
		if(!ast)
			ast = avi->ast_video;
	}

	if(!ast)
		return MPXPLAY_ERROR_INFILE_EOF;

	aviframe = (long)((float)ast->nb_frames * (float)newmpxframenum / (float)miis->allframes);
	if(aviframe >= ast->nb_frames)
		return MPXPLAY_ERROR_INFILE_EOF;

	if(aviframe < ast->seek_entries) {
		newfilepos = avi->movi_list + ast->seek_table[aviframe];
	} else {
		filesize = fbfs->filelength(fbds);
		if(ast->seek_entries && avi->is_odml) {
			avgbytes_per_aviframe = (float)(filesize - avi->riff_end) / (float)(ast->nb_frames - ast->seek_entries);
			newfilepos = (long)(avgbytes_per_aviframe * (float)(aviframe - ast->seek_entries)) + avi->riff_end;
		} else {
			avgbytes_per_aviframe = (float)(avi->movi_end - avi->movi_list);
			if(avi->is_odml)
				avgbytes_per_aviframe += filesize - avi->riff_end;
			avgbytes_per_aviframe /= ast->nb_frames;
			newfilepos = (long)(avgbytes_per_aviframe * (float)aviframe) + avi->movi_list;
			if(newfilepos >= avi->movi_end)
				newfilepos += (avi->riff_end - avi->movi_end);
		}
	}

	//fprintf(stderr,"af:%6d nbf:%6d sen:%6d nfp:%10d re:%10d avg:%5.0f\n",aviframe,ast->nb_frames,ast->seek_entries,newfilepos,(long)avi->riff_end,avgbytes_per_aviframe);

	if(fbfs->fseek(fbds, newfilepos, SEEK_SET) < 0)
		return MPXPLAY_ERROR_INFILE_EOF;

	return newmpxframenum;
}

//----------------------------------------------------------------

static unsigned int avi_read_header(struct avi_demuxer_data_s *avi, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis)
{
	avi_stream_data_s *ast;
	mpxp_uint32_t tag, tag1;
	int codec_type, stream_index, frame_period, bit_rate;
	unsigned int size;

	if(fbfs->get_le32(fbds) != PDS_GET4C_LE32('R', 'I', 'F', 'F'))
		return 0;

	avi->riff_end = fbfs->get_le32(fbds);	// RIFF chunk size
	avi->riff_end += fbfs->ftell(fbds);	// RIFF chunk end
	tag = fbfs->get_le32(fbds);
	if((tag != PDS_GET4C_LE32('A', 'V', 'I', ' ')) && (tag != PDS_GET4C_LE32('A', 'V', 'I', 'X')))
		return 0;

	stream_index = -1;
	codec_type = -1;
	frame_period = 0;

	do {
		if(fbfs->eof(fbds))
			goto err_out_aviheader;
		tag = fbfs->get_le32(fbds);
		size = fbfs->get_le32(fbds);

		switch (tag) {
		case PDS_GET4C_LE32('L', 'I', 'S', 'T'):
			tag1 = fbfs->get_le32(fbds);
			if(tag1 == PDS_GET4C_LE32('m', 'o', 'v', 'i')) {
				avi->movi_list = fbfs->ftell(fbds) - 4;
				if(size)
					avi->movi_end = avi->movi_list + size;
				else
					avi->movi_end = fbfs->filelength(fbds);
				goto end_of_header;
			}
			break;
		case PDS_GET4C_LE32('d', 'm', 'l', 'h'):
			avi->is_odml = 1;
			fbfs->fseek(fbds, size + (size & 1), SEEK_CUR);
			break;
		case PDS_GET4C_LE32('a', 'v', 'i', 'h'):
			frame_period = fbfs->get_le32(fbds);	// using frame_period is bad idea
			bit_rate = fbfs->get_le32(fbds) * 8;
			fbfs->fseek(fbds, 4 * 4, SEEK_CUR);
			avi->nb_streams = fbfs->get_le32(fbds);
			avi->avs = (struct avi_stream_data_s *)calloc(avi->nb_streams, sizeof(struct avi_stream_data_s));
			if(!avi->avs)
				goto err_out_aviheader;
			fbfs->fseek(fbds, size - 7 * 4, SEEK_CUR);
			break;
		case PDS_GET4C_LE32('s', 't', 'r', 'h'):
			stream_index++;
			if(stream_index >= avi->nb_streams) {
				fbfs->fseek(fbds, size - 8, SEEK_CUR);
				break;
			}
			ast = &avi->avs[stream_index];
			ast->stream_index = stream_index;

			tag1 = fbfs->get_le32(fbds);
			ast->codec_tag = fbfs->get_le32(fbds);
			if(tag1 == PDS_GET4C_LE32('i', 'a', 'v', 's') || tag1 == PDS_GET4C_LE32('i', 'v', 'a', 's')) {
				fbfs->fseek(fbds, 3 * 4, SEEK_CUR);
				ast->scale = fbfs->get_le32(fbds);
				ast->rate = fbfs->get_le32(fbds);
				fbfs->fseek(fbds, size - 7 * 4, SEEK_CUR);
				break;
			}

			fbfs->get_le32(fbds);	// flags
			fbfs->get_le16(fbds);	// priority
			fbfs->get_le16(fbds);	// language
			fbfs->get_le32(fbds);	// initial frame

			ast->scale = fbfs->get_le32(fbds);
			ast->rate = fbfs->get_le32(fbds);
			if(!ast->scale || !ast->rate) {
				if(frame_period) {
					ast->rate = 1000000;
					ast->scale = frame_period;
				} else {
					ast->rate = 25;
					ast->scale = 1;
				}
			}

			ast->start = fbfs->get_le32(fbds);
			ast->duration = fbfs->get_le32(fbds);

			ast->bufsize = fbfs->get_le32(fbds);	// buffer size
			fbfs->get_le32(fbds);	// quality
			ast->sample_size = fbfs->get_le32(fbds);	// sample ssize

			switch (tag1) {
			case PDS_GET4C_LE32('v', 'i', 'd', 's'):
				codec_type = MPXPLAY_SPI_STREAMTYPE_VIDEO;
				ast->sample_size = 0;
				break;
			case PDS_GET4C_LE32('a', 'u', 'd', 's'):
				codec_type = MPXPLAY_SPI_STREAMTYPE_AUDIO;
				break;
			case PDS_GET4C_LE32('t', 'x', 't', 's'):
				codec_type = MPXPLAY_SPI_STREAMTYPE_DATA;	//CODEC_TYPE_SUB ?  FIXME
				break;
			case PDS_GET4C_LE32('p', 'a', 'd', 's'):
				codec_type = MPXPLAY_SPI_STREAMTYPE_UNKNOWN;
				stream_index--;
				break;
			default:
				goto err_out_aviheader;
			}

			fbfs->fseek(fbds, size - 12 * 4, SEEK_CUR);
			break;
		case PDS_GET4C_LE32('s', 't', 'r', 'f'):
			if(stream_index >= avi->nb_streams || (stream_index < 0)) {
				fbfs->fseek(fbds, size, SEEK_CUR);
			} else {
				ast = &avi->avs[stream_index];
				switch (codec_type) {
#ifdef MPXPLAY_LINK_VIDEO
				case MPXPLAY_SPI_STREAMTYPE_VIDEO:
					fbfs->get_le32(fbds);	// size
					ast->video_res_x = fbfs->get_le32(fbds);
					ast->video_res_y = fbfs->get_le32(fbds);
					fbfs->get_le16(fbds);	// panes
					ast->video_bpp = fbfs->get_le16(fbds);	// depth
					tag1 = fbfs->get_le32(fbds);
					fbfs->get_le32(fbds);	// ImageSize
					fbfs->get_le32(fbds);	// XPelsPerMeter
					fbfs->get_le32(fbds);	// YPelsPerMeter
					fbfs->get_le32(fbds);	// ClrUsed
					fbfs->get_le32(fbds);	// ClrImportant

					if((size > (10 * 4)) && (size < (1 << 30))) {
						ast->extradata_size = size - 10 * 4;
						ast->extradata = malloc(ast->extradata_size + MPXPLAY_SPI_EXTRADATA_PADDING);
						if(!ast->extradata)
							goto err_out_aviheader;
						if(fbfs->fread(fbds, ast->extradata, ast->extradata_size) != ast->extradata_size)
							goto err_out_aviheader;
						pds_memset(ast->extradata + ast->extradata_size, 0, MPXPLAY_SPI_EXTRADATA_PADDING);
					}

					if(ast->extradata_size & 1)
						fbfs->get_byte(fbds);

					ast->streamtype = MPXPLAY_SPI_STREAMTYPE_VIDEO;
					ast->codec_tag = tag1;
					break;
#endif
				case MPXPLAY_SPI_STREAMTYPE_AUDIO:
					if(!avi_read_wavheader(ast, fbfs, fbds, size))
						goto err_out_aviheader;
					ast->need_parsing = 1;
					break;
				default:
					ast->streamtype = MPXPLAY_SPI_STREAMTYPE_DATA;
					ast->codec_tag = 0;
					fbfs->fseek(fbds, size, SEEK_CUR);
					break;
				}
			}
			break;
		default:
			size += (size & 1);
			fbfs->fseek(fbds, size, SEEK_CUR);
			break;
		}
	} while(1);

  end_of_header:
	if(stream_index != (avi->nb_streams - 1)) {
	  err_out_aviheader:
		return 0;
	}

	return 1;
}

static unsigned int avi_read_wavheader(avi_stream_data_s * ast, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, int size)
{
	ast->streamtype = MPXPLAY_SPI_STREAMTYPE_AUDIO;
	ast->codec_tag = fbfs->get_le16(fbds);
	ast->channels = fbfs->get_le16(fbds);
	ast->sample_rate = fbfs->get_le32(fbds);
	ast->bit_rate = fbfs->get_le32(fbds) * 8;
	ast->block_align = fbfs->get_le16(fbds);
	if(size == 14)				// plain vanilla WAVEFORMAT
		ast->bits_per_sample = 8;
	else
		ast->bits_per_sample = fbfs->get_le16(fbds);

	if(size >= 18) {			// WAVEFORMATEX
		ast->extradata_size = fbfs->get_le16(fbds);
		if(ast->extradata_size) {
			//fprintf(stdout,"es:%d %d\n",ast->extradata_size,size);
			if(ast->extradata_size > (size - 18))
				ast->extradata_size = size - 18;
			if(ast->extradata_size) {
				ast->extradata = malloc(ast->extradata_size + MPXPLAY_SPI_EXTRADATA_PADDING);
				if(!ast->extradata)
					return 0;
				if(fbfs->fread(fbds, ast->extradata, ast->extradata_size) != ast->extradata_size)
					return 0;
				pds_memset(ast->extradata + ast->extradata_size, 0, MPXPLAY_SPI_EXTRADATA_PADDING);
			}
		}
		if((size - ast->extradata_size - 18) > 0)	// skip garbage at the end
			fbfs->fseek(fbds, (size - ast->extradata_size - 18), SEEK_CUR);
	}
	//fprintf(stdout,"ba:%d \n",ast->block_align);

	return 1;
}

typedef struct avi_idx1_tmp_s {
	unsigned int streamnum;
	unsigned long flags;
	unsigned long pos;
	unsigned long len;
} avi_idx1_tmp_s;

static unsigned int avi_read_idx1(struct avi_demuxer_data_s *avi, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, long size)
{
	int nb_index_entries, i;
	unsigned long last_pos;
	struct avi_idx1_tmp_s *indx_tmp = NULL, *indxp;
	unsigned int *entrycounts = (unsigned int *)alloca(avi->nb_streams * sizeof(unsigned int));

	if(!entrycounts)
		return 0;

	nb_index_entries = size / 16;
	if(nb_index_entries <= 0)
		return 0;

	i = nb_index_entries * sizeof(struct avi_idx1_tmp_s);
	//fprintf(stdout,"i:%d size:%d e:%d\n",i,size,nb_index_entries);
	indx_tmp = indxp = (struct avi_idx1_tmp_s *)malloc(i);
	if(!indx_tmp)
		return 0;

	if(fbfs->fread(fbds, indx_tmp, i) != i)
		goto err_out_idx1;

	pds_memset(entrycounts, 0, avi->nb_streams * sizeof(unsigned int));

	i = nb_index_entries;
	do {
		unsigned int streamnum, tag;
		tag = indxp->streamnum;
		tag &= 0xffff;
		tag -= (((unsigned long)'0') << 8) | ((unsigned long)'0');
		streamnum = (((unsigned char)tag) << 3) + (((unsigned char)tag) << 1);	//streamnum  = ((tag & 0xff) - '0') * 10;
		streamnum += ((unsigned char *)&tag)[1];	//streamnum += ((tag >> 8) & 0xff) - '0';

		indxp->streamnum = streamnum;
		if(streamnum < avi->nb_streams)
			entrycounts[streamnum]++;

		indxp++;
	} while(--i);

	//fprintf(stderr,"nbs:%d ec0:%d ec1:%d \n",avi->nb_streams,entrycounts[0],entrycounts[1]);

	for(i = 0; i < avi->nb_streams; i++) {
		if(entrycounts[i]) {
			avi_stream_data_s *ast = &avi->avs[i];
			ast->seek_table = (mpxp_uint32_t *) malloc(entrycounts[i] * sizeof(mpxp_uint32_t));
			if(!ast->seek_table)
				goto err_out_idx1;
			if(ast->streamtype == MPXPLAY_SPI_STREAMTYPE_VIDEO) {
				ast->seek_flags = (mpxp_uint8_t *) malloc(entrycounts[i] * sizeof(mpxp_uint8_t));
				if(!ast->seek_flags)
					goto err_out_idx1;
				pds_memset(ast->seek_flags, 0, entrycounts[i] * sizeof(mpxp_uint8_t));
			}
		}
	}

	indxp = indx_tmp;
	if(indxp->pos > avi->movi_list)
		avi->movi_list = 0;		//FIXME better check

	last_pos = 0;
	i = nb_index_entries;
	do {
		if((indxp->pos > last_pos) && (indxp->streamnum < avi->nb_streams)) {
			avi_stream_data_s *ast = &avi->avs[indxp->streamnum];
			ast->seek_table[ast->seek_entries] = indxp->pos;
			last_pos = indxp->pos;
			if((ast->streamtype == MPXPLAY_SPI_STREAMTYPE_VIDEO) && (indxp->flags & AVIFLAG_IDX1_KEYFRAME))
				ast->seek_flags[ast->seek_entries] = AST_SEEKFLAG_KEYFRAME;
			ast->seek_entries++;
		}						//else
		//fprintf(stderr,"index error pos:%d sn:%d \n",indxp->pos,indxp->streamnum);
		indxp++;
	} while(--i);

	//fprintf(stdout,"se0:%d s0p:%d se1:%d s1p:%d \n",avi->avs[0].seek_entries,avi->avs[0].seek_table[avi->avs[0].seek_entries-1],avi->avs[1].seek_entries,avi->avs[1].seek_table[avi->avs[1].seek_entries-1]);

	free(indx_tmp);

	return 1;

  err_out_idx1:
	if(indx_tmp)
		free(indx_tmp);
	return 0;
}

static unsigned int avi_load_index(struct avi_demuxer_data_s *avi, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	mpxp_uint32_t tag, size;
	mpxp_uint64_t pos = fbfs->ftell(fbds);

	fbfs->fseek(fbds, avi->movi_end, SEEK_SET);

	for(;;) {
		if(fbfs->eof(fbds))
			break;
		tag = fbfs->get_le32(fbds);
		size = fbfs->get_le32(fbds);
		switch (tag) {
		case PDS_GET4C_LE32('i', 'd', 'x', '1'):
			if(!avi_read_idx1(avi, fbfs, fbds, size))
				goto skip;
			else
				goto the_end;
			break;
		default:
		  skip:
			size += (size & 1);
			fbfs->fseek(fbds, size, SEEK_CUR);
			break;
		}
	}
  the_end:
	//avi->non_interleaved |= guess_ni_flag(s);
	fbfs->fseek(fbds, pos, SEEK_SET);
	return 1;
}

//------------------------------------------------------------------

static struct avi_stream_data_s *avi_get_stream(struct avi_demuxer_data_s *avi, unsigned int streamtype, struct mpxplay_streampacket_info_s *spi)
{
	unsigned int i, streamtype_count = 0;
	struct avi_stream_data_s *ast = avi->avs, *found_stream = NULL;

	for(i = 0; i < avi->nb_streams; i++) {
		if(ast->streamtype == streamtype) {
			if(streamtype_count <= spi->stream_select)
				found_stream = ast;
			streamtype_count++;
		}
		ast++;
	}
	spi->nb_streams = streamtype_count;
	return found_stream;
}

static void avi_assign_audio(struct avi_demuxer_data_s *avi, struct mpxplay_infile_info_s *miis, unsigned int full_load)
{
	struct mpxplay_audio_decoder_info_s *adi = miis->audio_decoder_infos;
	struct mpxplay_streampacket_info_s *spi = miis->audio_stream;
	struct avi_stream_data_s *ast = avi->ast_audio;
	//char sout[200];

	if(!ast)
		return;

	spi->streamtype = ast->streamtype;
	spi->wave_id = ast->codec_tag;
	spi->block_align = ast->block_align;
	spi->bs_framesize = ast->bufsize;
	if(full_load) {
		funcbit_enable(spi->flags, MPXPLAY_SPI_FLAG_NEED_DECODER);
		if(ast->need_parsing)
			funcbit_enable(spi->flags, MPXPLAY_SPI_FLAG_NEED_PARSING);
	}
	funcbit_enable(spi->flags, MPXPLAY_SPI_FLAG_CONTAINER);
	spi->extradata = ast->extradata;
	spi->extradata_size = ast->extradata_size;

	adi->filechannels = adi->outchannels = ast->channels;
	adi->freq = ast->sample_rate;
	adi->bits = ast->bits_per_sample;
	if((spi->wave_id != MPXPLAY_WAVEID_PCM_SLE) && (spi->wave_id != MPXPLAY_WAVEID_PCM_FLOAT))
		adi->bitrate = (ast->bit_rate + 500) / 1000;

	if(ast->scale <= 1)			// !!! hack
		ast->nb_frames = ast->seek_entries;	// ??? better idea?
	else
		ast->nb_frames = ast->duration;

	miis->timemsec = (float)ast->duration * 1000.0 / (float)ast->rate * (float)ast->scale;	// ???

	/*if(!ast->bufsize){
	   if((spi->wave_id==MPXPLAY_WAVEID_PCM_SLE) || (spi->wave_id==MPXPLAY_WAVEID_PCM_FLOAT))
	   ast->bufsize=(float)2*miis->timemsec*(float)ast->sample_rate*(float)ast->sample_size/(float)ast->nb_frames; // ???
	   }
	   spi->bs_framesize=ast->bufsize; */

	/*fprintf(stderr,"st:%d wd:%4.4X fs:%d c:%d f:%d b:%d br:%d d:%d r:%d s:%d ss:%d ba:%d\n",
	   spi->streamtype,spi->wave_id,spi->bs_framesize,adi->filechannels,adi->freq,
	   adi->bits,adi->bitrate,ast->duration,ast->rate,ast->scale,ast->sample_size,spi->block_align);
	   //pds_textdisplay_printf(sout);
	   //getch();
	   fprintf(stderr,"%8.8X %d %2.2X%2.2X%2.2X%2.2X%2.2X%2.2X \n",spi->extradata,spi->extradata_size,
	   spi->extradata[0],spi->extradata[1],spi->extradata[2],spi->extradata[3],spi->extradata[4],spi->extradata[5]); */
}

#ifdef MPXPLAY_LINK_VIDEO
static void avi_assign_video(struct avi_demuxer_data_s *avi, struct mpxplay_infile_info_s *miis, unsigned int full_load)
{
	struct mpxplay_video_decoder_info_s *vdi = miis->video_decoder_infos;
	struct mpxplay_streampacket_info_s *spi = miis->video_stream;
	struct avi_stream_data_s *ast = avi->ast_video;
	//char sout[100];

	if(!ast)
		return;

	spi->streamtype = ast->streamtype;
	spi->wave_id = ast->codec_tag;
	spi->bs_framesize = ast->bufsize;
	if(full_load) {
		funcbit_enable(spi->flags, MPXPLAY_SPI_FLAG_NEED_DECODER);
		//if(ast->need_parsing)
		// funcbit_enable(spi->flags,MPXPLAY_SPI_FLAG_NEED_PARSING);
	}
	funcbit_enable(spi->flags, MPXPLAY_SPI_FLAG_CONTAINER);
	spi->extradata = ast->extradata;
	spi->extradata_size = ast->extradata_size;

	vdi->video_res_x = ast->video_res_x;
	vdi->video_res_y = ast->video_res_y;
	vdi->video_bitrate = (ast->bit_rate + 500) / 1000;

	ast->nb_frames = ast->duration;
	vdi->video_frames = ast->nb_frames;
	vdi->video_fps = ast->rate / ast->scale;

	/*fprintf(stderr,"x:%d y:%d fps:%d d:%d sc:%d ra:%d br:%d bs:%d\n",ast->video_res_x,ast->video_res_y,vdi->video_fps,
	   ast->duration,ast->scale,ast->rate,vdi->video_bitrate,ast->bufsize);
	   //pds_textdisplay_printf(sout); */
}
#endif

struct mpxplay_infile_func_s IN_AVI_funcs = {
	0,
	NULL,
	NULL,
	&AVI_infile_check,
	&AVI_infile_check,
	&AVI_infile_open,
	&AVI_infile_close,
	&AVI_infile_decode,
	&AVI_infile_fseek,
	NULL,
	NULL,
	NULL,
	NULL,
	{"AVI", NULL}
};

#endif							// MPXPLAY_LINK_INFILE_AVI
