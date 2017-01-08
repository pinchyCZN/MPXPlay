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
//function: ID3v1,ID3v2,APETag read/write

#include "in_file.h"
#include "newfunc\newfunc.h"
#include "playlist\playlist.h"
#include <string.h>
#include <malloc.h>

//--------------------------------------------------------------------------
//common
static unsigned int tagging_convert_chars_out(char **extradatas, char *id3p, struct mpxplay_textconv_func_s *mtf, unsigned int do_utf8)
{
	char *ed, *tmp, *dest;
	unsigned int len;

	len = pds_strlen(id3p);
	if(!len)
		return 0;
	len++;						// we count the terminating null too

	if(do_utf8) {
		ed = (char *)malloc(len * 3);	// *3 = max utf8 expansion
		if(!ed)
			return 0;
		*extradatas = ed;
		tmp = (char *)malloc(len);
		if(!tmp)
			return 0;
		dest = mtf->char_to_all(tmp, id3p);
		len = mtf->char_to_utf8(ed, dest, len * 3);
		free(tmp);
	} else {
		ed = (char *)malloc(len);
		if(!ed)
			return 0;
		*extradatas = ed;
		dest = mtf->char_to_all(ed, id3p);
		if(dest != ed)
			pds_strcpy(ed, dest);
	}

	return len;
}

//--------------------------------------------------------------------------
//ID3v1

static char *id3v1genres[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
	"Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
	"Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
	"Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient",
	"Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
	"Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
	"AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative",
	"Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
	"Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
	"Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap",
	"Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
	"Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal",
	"Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll",
	"Hard Rock", "Folk", "Folk/Rock", "National Folk", "Swing", "Fast-Fusion",
	"Bebob", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde",
	"Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock",
	"Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour",
	"Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony",
	"Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam", "Club",
	"Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul",
	"Freestyle", "Duet", "Punk Rock", "Drum Solo", "A capella", "Euro-House",
	"Dance Hall", "Goa", "Drum & Bass", "Club House", "Hardcore", "Terror",
	"Indie", "BritPop", "NegerPunk", "Polsk Punk", "Beat", "Christian Gangsta",
	"Heavy Metal", "Black Metal", "Crossover", "Contemporary C",
	"Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop",
	"SynthPop"
};

#define MAX_ID3GENRENUM (sizeof(id3v1genres)/sizeof(char *))

static unsigned int id3v10_partlens[5] = { 30, 30, 30, 4, 30 };

unsigned int mpxplay_tagging_id3v1_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	char tag[4];
	if(fbfs->fseek(fbds, -128, SEEK_END) < 0)
		return 0;
	if(fbfs->fread(fbds, tag, 3) != 3)
		return 0;
	if(tag[0] == 'T' && tag[1] == 'A' && tag[2] == 'G')
		return 1;
	return 0;
}

//always call check_id3tag_v1 before this
char *mpxplay_tagging_id3v1_get(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char **id3ip, char *id3p, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	unsigned int i, partlen, datalen;
	char *readp, readtmp[128];

	readp = &readtmp[0];
	if(fbfs->fread(fbds, readp, 128 - 3) != (128 - 3))
		return id3p;

	for(i = 0; i < 5; i++) {
		partlen = id3v10_partlens[i];
		if(!id3ip[i]) {			// maybe we've got it from ID3v2
			pds_strncpy(id3p, readp, partlen);
			if(i == I3I_COMMENT) {
				if(id3p[partlen - 1] < 32)	// tracknumber check
					id3p[partlen - 1] = 0;
			}
			id3p[partlen] = 0;
			datalen = pds_strcutspc(id3p);
			if(datalen) {
				id3ip[i] = id3p;
				datalen = mpxplay_textconv_funcs->all_to_char(id3p, datalen, 0);
				id3p += datalen + 1;
			}
		}
		readp += partlen;
	}
	//tracknumber from id3v1.1
	if(!id3ip[I3I_TRACKNUM]) {	// maybe we've got it from ID3v2
		i = (unsigned int)readp[-1];
		if(i && !readp[-2]) {
			id3ip[I3I_TRACKNUM] = id3p;
			id3p += sprintf(id3p, "%d", i) + 1;
		}
	}
	//genre
	if(!id3ip[I3I_GENRE]) {		// maybe we've got it from ID3v2
		i = (unsigned int)readp[0];
		if(i < MAX_ID3GENRENUM)
			id3ip[I3I_GENRE] = id3v1genres[i];
	}
	return id3p;
}

char *mpxplay_tagging_id3v1_index_to_genre(unsigned int i)
{
	if(i < MAX_ID3GENRENUM)
		return id3v1genres[i];
	return NULL;
}

static unsigned int tagging_id3v1_genre_to_index(char *genrename)
{
	unsigned int i;

	if(genrename)
		for(i = 0; i < MAX_ID3GENRENUM; i++)
			if(pds_stricmp(id3v1genres[i], genrename) == 0)
				return i;

	return 255;
}

int mpxplay_tagging_id3v1_put(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	int error = MPXPLAY_ERROR_INFILE_WRITETAG_UNKNOWN;
	long i, fileoffset;
	char *cvdata[I3I_MAX + 1], strtemp[192];
	const unsigned char cvflag[I3I_MAX + 1] = { 1, 1, 1, 0, 1, 0, 0 };

	if(mpxplay_tagging_id3v1_check(fbfs, fbds))	// is file tagged already?
		fileoffset = fbfs->fseek(fbds, -128, SEEK_END);
	else
		fileoffset = fbfs->fseek(fbds, 0, SEEK_END);
	if(fileoffset > 0) {		// successfull file positioning
		pds_memset(cvdata, 0, sizeof(cvdata));
		for(i = 0; i <= I3I_MAX; i++)
			if(cvflag[i])
				tagging_convert_chars_out(&cvdata[i], id3ip[i], mpxplay_textconv_funcs, 0);
		if(id3ip[I3I_TRACKNUM] && id3ip[I3I_TRACKNUM][0]) {	// ID3v1.1
			unsigned int tracknum = pds_atol(id3ip[I3I_TRACKNUM]);
			if(tracknum > 255)
				tracknum = 255;
			sprintf(strtemp, "TAG%-30.30s%-30.30s%-30.30s%-4.4s%-28.28s%c%c%c",
					(cvdata[I3I_TITLE]) ? cvdata[I3I_TITLE] : "",
					(cvdata[I3I_ARTIST]) ? cvdata[I3I_ARTIST] : "",
					(cvdata[I3I_ALBUM]) ? cvdata[I3I_ALBUM] : "",
					(id3ip[I3I_YEAR]) ? id3ip[I3I_YEAR] : "", (cvdata[I3I_COMMENT]) ? cvdata[I3I_COMMENT] : "", 0, tracknum, tagging_id3v1_genre_to_index(id3ip[I3I_GENRE]));
		} else {				// ID3v1.0
			sprintf(strtemp, "TAG%-30.30s%-30.30s%-30.30s%-4.4s%-30.30s%c",
					(cvdata[I3I_TITLE]) ? cvdata[I3I_TITLE] : "",
					(cvdata[I3I_ARTIST]) ? cvdata[I3I_ARTIST] : "",
					(cvdata[I3I_ALBUM]) ? cvdata[I3I_ALBUM] : "",
					(id3ip[I3I_YEAR]) ? id3ip[I3I_YEAR] : "", (cvdata[I3I_COMMENT]) ? cvdata[I3I_COMMENT] : "", tagging_id3v1_genre_to_index(id3ip[I3I_GENRE]));
		}
		if(fbfs->fwrite(fbds, strtemp, 128) == 128)	// successfull TAG writing
			error = MPXPLAY_ERROR_INFILE_OK;
		else
			error = MPXPLAY_ERROR_FILEHAND_CANTWRITE;
		for(i = 0; i <= I3I_MAX; i++)
			if(cvdata[i])
				free(cvdata[i]);
	} else
		error = MPXPLAY_ERROR_FILEHAND_CANTSEEK;
	return error;
}

//---------------------------------------------------------------------
//ID3v2

#define ID3V2_HEADSIZE 10
#define ID3V2_FOOTERSIZE 10
#define ID3V2_FRAMEHEADSIZE 10
#define ID3V2_MAX_DATALEN 1048576
#define ID3V2_MAX_ID3LEN  256

#define ID3V2_USE_TEXTENC 1		// use utf16,utf8 at text encoding (it's always enabled at decoding)
#define ID3V2_TEXTENCTYPE_NONE    0
#define ID3V2_TEXTENCTYPE_UTF16LE 1
#define ID3V2_TEXTENCTYPE_UTF16BE 2
#define ID3V2_TEXTENCTYPE_UTF8    3

#define ID3V2_FLAG_UNSYNCHRONISATION 0x80
#define ID3V2_FLAG_EXTENDED_HEADER   0x40
#define ID3V2_FLAG_EXPERIMENTAL      0x20
#define ID3V2_FLAG_FOOTER_PRESENT    0x10

#define ID3V2_FRAMENUM_ALLOC 32

#define ID3V2_CONTROL_READ     0
#define ID3V2_CONTROL_WRITECHK 1
#define ID3V2_CONTROL_WRITE    2

typedef struct id3v2x_one_frame_data_s {
	char frameid[8];			// same like in one_frame_handler
	char text_enc_type;
	char language_type[3];
	unsigned int flags;
	void *framebuf;				// if we want to write it back without modification
	unsigned long total_framelen;	// incl. header
} id3v2x_one_frame_data_s;

typedef struct id3v2x_one_frame_handler_s {
	char *frameid;
	unsigned int i3i_index;
	long (*frame_reader) (struct id3v2x_main_data_s * imds, struct id3v2x_one_frame_data_s * framedata, struct mpxplay_filehand_buffered_func_s * fbfs, void *fbds, long datalen, char **id3ip,
						  struct mpxplay_textconv_func_s * mpxplay_textconv_funcs);
	long (*frame_writer) (struct id3v2x_main_data_s * imds, struct id3v2x_one_frame_data_s * framedata, struct mpxplay_filehand_buffered_func_s * fbfs, void *fbds, char *id3ip,
						  struct mpxplay_textconv_func_s * mpxplay_textconv_funcs);
} id3v2x_one_frame_handler_s;

typedef struct id3v2x_one_version_info_s {
	unsigned char version_number;
	unsigned long (*frame_getsize) (char *bufp);
	void (*frame_putsize) (char *bufp, unsigned long framesize);
	id3v2x_one_frame_handler_s *supported_frames;
} id3v2x_one_version_info_s;

typedef struct id3v2x_main_data_s {
	unsigned int control;
	char *id3p;					// !!!
	char global_textenc_type;

	unsigned int version;
	unsigned int flags;
	unsigned long totalsize;
	unsigned long filepos;

	struct id3v2x_one_version_info_s *version_handler;

	unsigned int nb_frames;
	struct id3v2x_one_frame_data_s *frame_datas;
	unsigned int nb_allocated_frames;

	char footer[ID3V2_FOOTERSIZE];
} id3v2x_main_data_s;

static unsigned int tagging_id3v2_framespace_alloc(struct id3v2x_main_data_s *imds)
{
	if(imds->nb_frames >= imds->nb_allocated_frames) {
		unsigned int alloc_framenum = imds->nb_allocated_frames + ID3V2_FRAMENUM_ALLOC;
		struct id3v2x_one_frame_data_s *fds = (struct id3v2x_one_frame_data_s *)calloc(alloc_framenum, sizeof(struct id3v2x_one_frame_data_s));
		if(!fds)
			return 0;
		if(imds->frame_datas) {
			if(imds->nb_frames)
				pds_memcpy((void *)fds, (void *)imds->frame_datas, (imds->nb_frames * sizeof(struct id3v2x_one_frame_data_s)));
			free(imds->frame_datas);
		}
		imds->frame_datas = fds;
		imds->nb_allocated_frames = alloc_framenum;
	}
	return 1;
}

static void tagging_id3v2_framespace_dealloc(struct id3v2x_main_data_s *imds)
{
	if(imds->frame_datas) {
		struct id3v2x_one_frame_data_s *fds = imds->frame_datas;
		while(imds->nb_allocated_frames) {
			if(fds->framebuf)
				free(fds->framebuf);
			fds++;
			imds->nb_allocated_frames--;
		}
		free(imds->frame_datas);
		imds->frame_datas = NULL;
		imds->nb_frames = 0;
	}
}

static id3v2x_one_frame_handler_s *tagging_id3v2_search_frameid(id3v2x_one_frame_handler_s * f, char *framehead)
{
	while(f->frameid) {
		if(pds_strncmp(framehead, f->frameid, 4) == 0)
			return f;
		f++;
	}
	return NULL;
}

static id3v2x_one_frame_handler_s *tagging_id3v2_search_i3i(id3v2x_one_frame_handler_s * f, unsigned int i3i)
{
	while(f->frameid) {
		if(f->i3i_index == i3i)
			return f;
		f++;
	}
	return NULL;
}

static struct id3v2x_one_frame_data_s *tagging_id3v2_search_framedata(struct id3v2x_main_data_s *imds, char *frameid)
{
	struct id3v2x_one_frame_data_s *fds = imds->frame_datas;
	unsigned int i = imds->nb_frames;
	if(!fds || !i)
		return NULL;
	do {
		if(PDS_GETB_LE32(&fds->frameid[0]) == PDS_GETB_LE32(frameid))
			return fds;
		fds++;
	} while(--i);
	return NULL;
}

#ifdef ID3V2_USE_TEXTENC
static void tagging_id3v2_globaltextenc_get(struct id3v2x_main_data_s *imds)
{
	struct id3v2x_one_frame_data_s *fds = imds->frame_datas;
	unsigned int i = imds->nb_frames;

	if(!fds || !i)
		return;

	do {
		if(fds->text_enc_type && (fds->text_enc_type <= 3)) {
			imds->global_textenc_type = fds->text_enc_type;
			break;
		}
		fds++;
	} while(--i);
}
#endif

//-------------------------------------------------------------------------
//read side

static unsigned long mpx_id3v2x_get_framesize_4x7(char *bufp)
{
	unsigned long framesize = 0;

	if(!(bufp[1] & 0x80))
		framesize += (bufp[0] & 0x7f) << 21;
	if(!(bufp[2] & 0x80))
		framesize += (bufp[1] & 0x7f) << 14;
	if(!(bufp[3] & 0x80))
		framesize += (bufp[2] & 0x7f) << 7;
	framesize += (bufp[3] & 0x7f) << 0;

	return framesize;
}

static unsigned long mpx_id3v2x_get_framesize_4x8(char *bufp)
{
	return PDS_GETB_BE32(bufp);
}

static long mpx_id3v2x_textconv_utf_to_char(char *databuf, long datalen, unsigned int text_enc_type, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	if(((*(mpxplay_textconv_funcs->control)) & ID3TEXTCONV_UTF_AUTO) && databuf) {
		switch (text_enc_type) {
		case ID3V2_TEXTENCTYPE_UTF16LE:
			datalen = mpxplay_textconv_funcs->utf16LE_to_char(databuf, datalen);
			mpxplay_textconv_funcs->convdone = ID3TEXTCONV_UTF16;
			break;
		case ID3V2_TEXTENCTYPE_UTF16BE:
			datalen = mpxplay_textconv_funcs->utf16BE_to_char(databuf, datalen);
			mpxplay_textconv_funcs->convdone = ID3TEXTCONV_UTF16;
			break;
		case ID3V2_TEXTENCTYPE_UTF8:
			datalen = mpxplay_textconv_funcs->utf8_to_char(databuf, datalen);
			mpxplay_textconv_funcs->convdone = ID3TEXTCONV_UTF8;
			break;
		}
	}
	return datalen;
}

static long mpx_id3v2x_tagread_text(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, long datalen, char **id3ip,
									struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	if(datalen < 2)
		return 0;
	if(fbfs->fread(fbds, &framedata->text_enc_type, 1) != 1)
		return -1;
	datalen--;
	if(imds->id3p) {
		if(fbfs->fread(fbds, imds->id3p, datalen) != datalen)
			return -1;
		imds->id3p[datalen] = 0;
		datalen = mpx_id3v2x_textconv_utf_to_char(imds->id3p, datalen, framedata->text_enc_type, mpxplay_textconv_funcs);
	} else {
		if(fbfs->fseek(fbds, datalen, SEEK_CUR) < 0)
			return -1;
	}
	return datalen;
}

static long mpx_id3v2x_tagend_text(struct id3v2x_main_data_s *imds, int datalen, char **id3ip, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	if((datalen > 0) && imds->id3p) {
		id3ip[0] = imds->id3p;
		imds->id3p[datalen] = 0;
		datalen = pds_strcutspc(imds->id3p);
		datalen = mpxplay_textconv_funcs->all_to_char(imds->id3p, datalen, mpxplay_textconv_funcs->convdone);
		imds->id3p += datalen + 1;
	}
	return datalen;
}

static long mpx_id3v2x_tagread_txxx(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, long datalen, char **id3ip,
									struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	datalen = mpx_id3v2x_tagread_text(imds, framedata, fbfs, fbds, datalen, id3ip, mpxplay_textconv_funcs);
	datalen = mpx_id3v2x_tagend_text(imds, datalen, id3ip, mpxplay_textconv_funcs);
	return datalen;
}

static long mpx_id3v2x_tagread_tcon(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, long datalen, char **id3ip,
									struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	datalen = mpx_id3v2x_tagread_text(imds, framedata, fbfs, fbds, datalen, id3ip, mpxplay_textconv_funcs);
	if((datalen > 0) && imds->id3p) {
		char *id3p = imds->id3p;
		int gennum;
		if(id3p[0] == '(')
			id3p++;
		gennum = pds_atol(id3p);
		if((gennum || id3p[0] == '0') && (gennum < MAX_ID3GENRENUM))
			id3ip[0] = id3v1genres[gennum];
		else
			datalen = mpx_id3v2x_tagend_text(imds, datalen, id3ip, mpxplay_textconv_funcs);
	}
	return datalen;
}

static long mpx_id3v2x_tagread_comm(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, long datalen, char **id3ip,
									struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	unsigned int content_desc_len;

	if(datalen <= 5) {			// commentlen == 0
		if(fbfs->fseek(fbds, datalen, SEEK_CUR) < 0)	// skip frame
			return -1;
		return 0;
	}
	if(fbfs->fread(fbds, &framedata->text_enc_type, 1) != 1)	// text encoding type (1 byte)
		return -1;
	if(fbfs->fread(fbds, &framedata->language_type, 3) != 3)	// language           (3 bytes)
		return -1;
	datalen -= 4;

	if(imds->id3p) {
		if(fbfs->fread(fbds, imds->id3p, datalen) != datalen)	// content_descriptor 00h comment
			return -1;

		datalen = mpx_id3v2x_textconv_utf_to_char(imds->id3p, datalen, framedata->text_enc_type, mpxplay_textconv_funcs);

		content_desc_len = pds_strlen(imds->id3p) + 1;
		if(datalen <= content_desc_len)	// there's no comment data
			return 0;
		datalen -= content_desc_len;
		pds_memcpy(imds->id3p, (imds->id3p + content_desc_len), datalen);	// remove content desc data from our datafield (we don't use it)

		datalen = mpx_id3v2x_tagend_text(imds, datalen, id3ip, mpxplay_textconv_funcs);
	} else {
		if(fbfs->fseek(fbds, datalen, SEEK_CUR) < 0)
			return -1;
	}

	return datalen;
}

static int mpx_id3v2_read_frame(struct id3v2x_main_data_s *imds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char **id3ip, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	unsigned int i3i_select;
	long datalen;
	id3v2x_one_frame_handler_s *frameselect;
	struct id3v2x_one_frame_data_s *framedata = NULL;
	char readtmp[16];

	if(fbfs->fread(fbds, &readtmp[0], ID3V2_FRAMEHEADSIZE) != ID3V2_FRAMEHEADSIZE)
		return -1;

	imds->filepos += ID3V2_FRAMEHEADSIZE;

	datalen = imds->version_handler->frame_getsize(&readtmp[4]);
	if(!datalen)
		return 0;

	imds->filepos += datalen;
	if(imds->filepos > imds->totalsize)
		return -1;
	if(!tagging_id3v2_framespace_alloc(imds))
		goto err_out_seek;

	framedata = imds->frame_datas + (imds->nb_frames++);
	pds_memcpy((void *)&framedata->frameid[0], (void *)&readtmp[0], 4);
	framedata->total_framelen = datalen + ID3V2_FRAMEHEADSIZE;
	framedata->flags = PDS_GETB_LE16(&readtmp[8]);	// ???

	frameselect = tagging_id3v2_search_frameid(imds->version_handler->supported_frames, &readtmp[0]);

	if((frameselect == NULL) || (datalen > ID3V2_MAX_ID3LEN)) {	// save frame data if we can't handle it
		if(imds->control != ID3V2_CONTROL_READ) {
			framedata->framebuf = malloc(framedata->total_framelen);
			if(framedata->framebuf) {
				pds_memcpy(framedata->framebuf, (void *)&readtmp[0], ID3V2_FRAMEHEADSIZE);
				if(fbfs->fread(fbds, ((char *)framedata->framebuf) + ID3V2_FRAMEHEADSIZE, datalen) != datalen) {
					free(framedata->framebuf);
					framedata->framebuf = NULL;
				}
			}
		}
		goto err_out_seek;
	}

	i3i_select = frameselect->i3i_index;

	if(id3ip[i3i_select] && (imds->control == ID3V2_CONTROL_READ))
		goto err_out_seek;

	mpxplay_textconv_funcs->convdone = 0;
	datalen = frameselect->frame_reader(imds, framedata, fbfs, fbds, datalen, &id3ip[i3i_select], mpxplay_textconv_funcs);
	if(datalen != 0)
		return datalen;

  err_out_seek:
	if(fbfs->fseek(fbds, imds->filepos, SEEK_SET) < 0)
		return -1;
	return 0;
}

static unsigned long mpx_id3v2x_read_header(struct id3v2x_main_data_s *imds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	unsigned long tsize;
	char header[ID3V2_HEADSIZE];

	if(fbfs->fseek(fbds, 0, SEEK_SET) < 0)
		return 0;
	if(fbfs->fread(fbds, header, ID3V2_HEADSIZE) != ID3V2_HEADSIZE)
		return 0;

	if(PDS_GETB_LE24(&header[0]) != PDS_GET4C_LE32('I', 'D', '3', 0))
		return 0;
	imds->version = (unsigned char)header[3];
	imds->flags = (unsigned char)header[5];

	tsize = mpx_id3v2x_get_framesize_4x7(&header[6]);
	if(!tsize)
		return 0;
	tsize += ID3V2_HEADSIZE;
	if(imds->flags & ID3V2_FLAG_FOOTER_PRESENT)
		tsize += ID3V2_FOOTERSIZE;
	if(tsize >= fbfs->filelength(fbds))
		return 0;
	imds->totalsize = tsize;

	return ID3V2_HEADSIZE;
}

static unsigned long mpx_id3v2_read_extended_header(struct id3v2x_main_data_s *imds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	unsigned long ehsize;
	char readtmp[4];

	if(fbfs->fread(fbds, &readtmp[0], 4) != 4)
		return 0;
	ehsize = imds->version_handler->frame_getsize(&readtmp[0]);
	if(ehsize < 4 || (imds->totalsize + ehsize) >= fbfs->filelength(fbds))
		return 0;
	imds->filepos += ehsize;
	if(fbfs->fseek(fbds, ehsize - 4, SEEK_CUR) < 0)	// skip only, not read
		return 0;
	return 1;
}

//--------------------------------------------------------------------------
//write side

static void mpx_id3v2x_put_framesize_4x7(char *bufp, unsigned long framesize)
{
	bufp[3] = (framesize >> 0) & 0x7f;
	bufp[2] = (framesize >> 7) & 0x7f;
	bufp[1] = (framesize >> 14) & 0x7f;
	bufp[0] = (framesize >> 21) & 0x7f;
}

static void mpx_id3v2x_put_framesize_4x8(char *bufp, unsigned long framesize)
{
	PDS_PUTB_BE32(bufp, framesize);
}

static int mpx_id3v2x_write_header(struct id3v2x_main_data_s *imds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	unsigned long totalsize;
	char header[ID3V2_HEADSIZE] = "ID3";

	header[3] = imds->version;
	header[4] = 0;
	header[5] = imds->flags;
	totalsize = imds->totalsize - ID3V2_HEADSIZE;	// !!! because we add them
	if(imds->flags & ID3V2_FLAG_FOOTER_PRESENT)	// to imds->totalsize in read_header
		totalsize -= ID3V2_FOOTERSIZE;	//

	mpx_id3v2x_put_framesize_4x7(&header[6], totalsize);

	if(fbfs->fseek(fbds, 0, SEEK_SET) < 0)
		return MPXPLAY_ERROR_FILEHAND_CANTSEEK;
	if(fbfs->fwrite(fbds, header, ID3V2_HEADSIZE) != ID3V2_HEADSIZE)
		return MPXPLAY_ERROR_FILEHAND_CANTWRITE;

	imds->filepos = ID3V2_HEADSIZE;	// !!!

	return MPXPLAY_ERROR_INFILE_OK;
}

static unsigned int mpx_id3v2x_textconv_char_to_utf(char *destbuf, unsigned int buflen, char *srcbuf, unsigned int datalen, unsigned int text_enc_type,
													struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	if((*(mpxplay_textconv_funcs->control)) & ID3TEXTCONV_UTF_AUTO) {
		switch (text_enc_type) {
		case ID3V2_TEXTENCTYPE_UTF16LE:
			destbuf[0] = 0xff;	// !!! we put utf header before every string/tag
			destbuf[1] = 0xfe;	//
			destbuf += 2;
			buflen -= 2;
			datalen = 2;
			datalen += mpxplay_textconv_funcs->char_to_utf16LE(destbuf, srcbuf, buflen);
			// !!! trailing zeroes are not written
			break;
		case ID3V2_TEXTENCTYPE_UTF16BE:
			destbuf[0] = 0xfe;	// !!!
			destbuf[1] = 0xff;	//
			destbuf += 2;
			buflen -= 2;
			datalen = 2;
			datalen += mpxplay_textconv_funcs->char_to_utf16BE(destbuf, srcbuf, buflen);
			// !!! trailing zeroes are not written
			break;
		case ID3V2_TEXTENCTYPE_UTF8:
			//destbuf[0]=0xef; // ???
			//destbuf[1]=0xbb; //
			//destbuf[2]=0xbf; //
			//destbuf+=3;buflen-=3;datalen=3;
			datalen = mpxplay_textconv_funcs->char_to_utf8(destbuf, srcbuf, buflen);
			// !!! trailing zeroes are not written
			break;
		default:
			pds_memcpy(destbuf, srcbuf, datalen);
			break;
		}
	} else
		pds_memcpy(destbuf, srcbuf, datalen);
	return datalen;
}

static long mpx_id3v2x_tagwrite_text(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *id3ip,
									 struct mpxplay_textconv_func_s *mpxplay_textconv_funcs, unsigned int comment)
{
	unsigned int datalen, buflen, framelen, ccd_len;
	char *cpinbuf, *cpoutbuf, *utfbuf, text_enc_type;
	char header[ID3V2_FRAMEHEADSIZE + 4];

	datalen = pds_strlen(id3ip);
	if(!datalen)
		return 0;
	cpinbuf = alloca(datalen + 1);
	if(!cpinbuf)
		return MPXPLAY_ERROR_INFILE_MEMORY;
	buflen = datalen * 2 + 8;
	utfbuf = alloca(buflen);
	if(!utfbuf)
		return MPXPLAY_ERROR_INFILE_MEMORY;

#ifdef ID3V2_USE_TEXTENC
	text_enc_type = framedata->text_enc_type;
#else
	text_enc_type = ID3V2_TEXTENCTYPE_NONE;
#endif

	cpoutbuf = mpxplay_textconv_funcs->char_to_all(cpinbuf, id3ip);
	datalen = mpx_id3v2x_textconv_char_to_utf(utfbuf, buflen, cpoutbuf, datalen, text_enc_type, mpxplay_textconv_funcs);
	if(!datalen)
		return MPXPLAY_ERROR_INFILE_MEMORY;
	framelen = 1 + datalen;		// sizeof text_enc + datalen
	if(comment) {
		ccd_len = ((text_enc_type == ID3V2_TEXTENCTYPE_UTF16LE) || (text_enc_type == ID3V2_TEXTENCTYPE_UTF16BE)) ? 2 : 1;
		framelen += 3 + ccd_len;	// sizeof language + comment_content_descriptor
	}

	if(imds->control == ID3V2_CONTROL_WRITE) {
		pds_memcpy(&header[0], &framedata->frameid[0], 4);
		imds->version_handler->frame_putsize(&header[4], framelen);
		PDS_PUTB_LE16(&header[8], framedata->flags);

		if(fbfs->fwrite(fbds, header, ID3V2_FRAMEHEADSIZE) != ID3V2_FRAMEHEADSIZE)
			return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
		if(fbfs->fwrite(fbds, &text_enc_type, 1) != 1)	// text_enc
			return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
		if(comment) {
			if(fbfs->fwrite(fbds, &framedata->language_type[0], 3) != 3)	// language
				return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
			header[0] = header[1] = 0;
			if(fbfs->fwrite(fbds, &header[0], ccd_len) != ccd_len)	// comment content descriptor (closed with a 00h)
				return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
		}
		if(fbfs->fwrite(fbds, utfbuf, datalen) != datalen)
			return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
	}
	return (framelen + ID3V2_FRAMEHEADSIZE);	// total bytes written
}

static long mpx_id3v2x_tagwrite_txxx(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *id3ip,
									 struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	return mpx_id3v2x_tagwrite_text(imds, framedata, fbfs, fbds, id3ip, mpxplay_textconv_funcs, 0);
}

static long mpx_id3v2x_tagwrite_tcon(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *id3ip,
									 struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	unsigned int genrenum = tagging_id3v1_genre_to_index(id3ip);
	if(genrenum < MAX_ID3GENRENUM) {	// standard id3v1 genre-string
		char gennumstr[8];
		sprintf(gennumstr, "(%d)", genrenum);	// we write the index only
		return mpx_id3v2x_tagwrite_text(imds, framedata, fbfs, fbds, &gennumstr[0], mpxplay_textconv_funcs, 0);
	}
	return mpx_id3v2x_tagwrite_text(imds, framedata, fbfs, fbds, id3ip, mpxplay_textconv_funcs, 0);
}

static long mpx_id3v2x_tagwrite_comm(struct id3v2x_main_data_s *imds, struct id3v2x_one_frame_data_s *framedata, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char *id3ip,
									 struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	return mpx_id3v2x_tagwrite_text(imds, framedata, fbfs, fbds, id3ip, mpxplay_textconv_funcs, 1);
}

//--------------------------------------------------------------------------

static id3v2x_one_frame_handler_s id3v23_supported_frames[] = { {"TPE1", I3I_ARTIST, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TIT2", I3I_TITLE, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TALB", I3I_ALBUM, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TYER", I3I_YEAR, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TCON", I3I_GENRE, &mpx_id3v2x_tagread_tcon, &mpx_id3v2x_tagwrite_tcon},
{"COMM", I3I_COMMENT, &mpx_id3v2x_tagread_comm, &mpx_id3v2x_tagwrite_comm},
{"TRCK", I3I_TRACKNUM, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{NULL, 0, NULL, NULL}
};

static id3v2x_one_frame_handler_s id3v24_supported_frames[] = { {"TPE1", I3I_ARTIST, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TIT2", I3I_TITLE, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TALB", I3I_ALBUM, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TYER", I3I_YEAR, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TDRC", I3I_YEAR, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{"TCON", I3I_GENRE, &mpx_id3v2x_tagread_tcon, &mpx_id3v2x_tagwrite_tcon},
{"COMM", I3I_COMMENT, &mpx_id3v2x_tagread_comm, &mpx_id3v2x_tagwrite_comm},
{"TRCK", I3I_TRACKNUM, &mpx_id3v2x_tagread_txxx, &mpx_id3v2x_tagwrite_txxx},
{NULL, 0, NULL, NULL}
};

//2.3 and 2.4 are supported
static id3v2x_one_version_info_s id3v2_all_version_infos[] = { {0x03, mpx_id3v2x_get_framesize_4x8, mpx_id3v2x_put_framesize_4x8, &id3v23_supported_frames[0]},
{0x04, mpx_id3v2x_get_framesize_4x7, mpx_id3v2x_put_framesize_4x7, &id3v24_supported_frames[0]},
{0x00, NULL, NULL, NULL}
};

static id3v2x_one_version_info_s *mpx_id3v2_version_selector(unsigned int version)
{
	id3v2x_one_version_info_s *v = &id3v2_all_version_infos[0];
	while(v->frame_getsize) {
		if(v->version_number == version)
			return v;
		v++;
	}
	return NULL;
}

static unsigned int mpx_id3v2_check_header(struct id3v2x_main_data_s *imds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	imds->filepos = mpx_id3v2x_read_header(imds, fbfs, fbds);
	if(!imds->filepos)
		return 0;

	imds->version_handler = mpx_id3v2_version_selector(imds->version);
	if(!imds->version_handler)
		return 0;

	if(imds->flags & ID3V2_FLAG_EXTENDED_HEADER)
		if(!mpx_id3v2_read_extended_header(imds, fbfs, fbds))
			return 0;

	return 1;
}

static unsigned int mpx_id3v2_read_main(struct id3v2x_main_data_s *imds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char **id3ip,
										struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	if(!mpx_id3v2_check_header(imds, fbfs, fbds))
		return 0;

	if((imds->filepos + ID3V2_FRAMEHEADSIZE) >= imds->totalsize) {	// no frames (tags) in the ID3V2
		fbfs->fseek(fbds, imds->totalsize, SEEK_SET);
		return 1;
	}

	do {
		if(mpx_id3v2_read_frame(imds, fbfs, fbds, id3ip, mpxplay_textconv_funcs) < 0)
			break;
	} while(imds->filepos < (imds->totalsize - ID3V2_FRAMEHEADSIZE));

	if(imds->filepos != imds->totalsize)
		fbfs->fseek(fbds, imds->totalsize, SEEK_SET);

	return 1;
}

static int mpx_id3v2_write_main(struct id3v2x_main_data_s *imds, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, char **id3ip, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	unsigned long totalsize = ID3V2_HEADSIZE;
	int i, retcode, framelen;
	struct id3v2x_one_frame_data_s *fds;
	struct id3v2x_one_frame_data_s fd_tmp;
	char writetmp[128];

	if(totalsize > imds->totalsize)
		return MPXPLAY_ERROR_INFILE_WRITETAG_IV2_SPACE;

	retcode = mpx_id3v2x_write_header(imds, fbfs, fbds);
	if(retcode != MPXPLAY_ERROR_INFILE_OK)
		return retcode;

	for(i = 0; i <= I3I_MAX; i++) {	// first write the supported frames
		id3v2x_one_frame_handler_s *frameselect = tagging_id3v2_search_i3i(imds->version_handler->supported_frames, i);
		if(frameselect) {
			fds = tagging_id3v2_search_framedata(imds, frameselect->frameid);
			if(!fds) {			// frame not exists in the file yet, create new framedata
				fds = &fd_tmp;
				pds_memset((void *)fds, 0, sizeof(struct id3v2x_one_frame_data_s));
				pds_memcpy((void *)&fds->frameid[0], (void *)&frameselect->frameid[0], 4);
				fds->text_enc_type = imds->global_textenc_type;
			}
			imds->control = ID3V2_CONTROL_WRITECHK;
			framelen = frameselect->frame_writer(imds, fds, fbfs, fbds, id3ip[i], mpxplay_textconv_funcs);
			if(framelen < 0)	// error
				return framelen;
			if(framelen == 0)	// datalen=0
				continue;
			if((totalsize + framelen) > imds->totalsize) {	// not enough space, skip frame
				retcode = MPXPLAY_ERROR_INFILE_WRITETAG_IV2_SPACE;
				continue;
			}
			imds->control = ID3V2_CONTROL_WRITE;
			framelen = frameselect->frame_writer(imds, fds, fbfs, fbds, id3ip[i], mpxplay_textconv_funcs);
			if(framelen < 0)	// error
				return framelen;
			if((totalsize + framelen) > imds->totalsize) {	// not enough space, skip frame
				retcode = MPXPLAY_ERROR_INFILE_WRITETAG_IV2_SPACE;
				continue;
			}
			totalsize += framelen;
		}
	}

	i = imds->nb_frames;		// write the other frames
	fds = imds->frame_datas;
	if(i && fds) {
		do {
			if(fds->framebuf && fds->total_framelen) {
				if((totalsize + fds->total_framelen) <= imds->totalsize) {
					long bytes = fbfs->fwrite(fbds, fds->framebuf, fds->total_framelen);
					totalsize += bytes;
					if(bytes != fds->total_framelen)
						return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
				} else
					retcode = MPXPLAY_ERROR_INFILE_WRITETAG_IV2_SPACE;
			}
			fds++;
		} while(--i);
	}

	pds_memset(writetmp, 0, sizeof(writetmp));
	while(totalsize < imds->totalsize) {	// fill up the id3v2 with zeroes
		i = min((imds->totalsize - totalsize), sizeof(writetmp));
		if(fbfs->fwrite(fbds, writetmp, i) != i)
			return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
		totalsize += i;
	}

	return retcode;
}

unsigned int mpxplay_tagging_id3v2_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	struct id3v2x_main_data_s imds;
	pds_memset((void *)&imds, 0, sizeof(imds));
	imds.control = ID3V2_CONTROL_READ;
	return mpx_id3v2_check_header(&imds, fbfs, fbds);
}

char *mpxplay_tagging_id3v2_get(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, char *id3p,
								struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	struct id3v2x_main_data_s imds;
	pds_memset((void *)&imds, 0, sizeof(imds));
	imds.control = ID3V2_CONTROL_READ;
	imds.id3p = id3p;
	mpx_id3v2_read_main(&imds, fbfs, fbds, id3ip, mpxplay_textconv_funcs);
	tagging_id3v2_framespace_dealloc(&imds);
	return imds.id3p;
}

int mpxplay_tagging_id3v2_put(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	int retcode;
	struct id3v2x_main_data_s imds;
	pds_memset((void *)&imds, 0, sizeof(imds));
	imds.control = ID3V2_CONTROL_WRITE;
	if(mpx_id3v2_read_main(&imds, fbfs, fbds, id3ip, mpxplay_textconv_funcs)) {
#ifdef ID3V2_USE_TEXTENC
		tagging_id3v2_globaltextenc_get(&imds);	// use the same (utf) text encoding at the new tags
#endif
		retcode = mpx_id3v2_write_main(&imds, fbfs, fbds, id3ip, mpxplay_textconv_funcs);
	} else
		retcode = MPXPLAY_ERROR_INFILE_WRITETAG_IV2_SPACE;
	tagging_id3v2_framespace_dealloc(&imds);
	return retcode;
}

//-------------------------------------------------------------------------
// requires 10 bytes in bufp !
unsigned long mpxplay_tagging_id3v2_totalsize(char *bufp)
{
	unsigned long id3v2totalsize;
	unsigned int flags;

	if(PDS_GETB_LE24(bufp) != PDS_GET4C_LE32('I', 'D', '3', 0))
		return 0;

	bufp += 3;					// ID3
	bufp += 2;					// version

	flags = bufp[0];
	bufp += 1;					// flags

	id3v2totalsize = ID3V2_HEADSIZE + mpx_id3v2x_get_framesize_4x7(bufp);
	if(flags & ID3V2_FLAG_FOOTER_PRESENT)
		id3v2totalsize += ID3V2_FOOTERSIZE;

	return id3v2totalsize;
}

//--------------------------------------------------------------------
//APEtag

typedef struct APETag_s {
	unsigned char ID[8];		// should equal 'APETAGEX'
	unsigned char Version[4];	// 1000 or 2000
	unsigned char Length[4];	// the complete size of the tag, including this footer
	unsigned char TagCount[4];	// the number of fields in the tag
	unsigned char Flags[4];		// the tag flags (none currently defined)
	unsigned char Reserved[8];	// reserved for later use
} APETag_s;

typedef struct tagtype_s {
	char *name;
	unsigned int i3i_index;
} tagtype_s;

static tagtype_s tagtypes[] = {
	{"Artist", I3I_ARTIST}, {"Title", I3I_TITLE}, {"Album", I3I_ALBUM},
	{"Year", I3I_YEAR}, {"Comment", I3I_COMMENT}, {"Genre", I3I_GENRE},
	{"Track", I3I_TRACKNUM}
};

#define TAGTYPENUM (sizeof(tagtypes)/sizeof(struct tagtype_s))

static int ape_tag_check_apetag(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct APETag_s *apetag)
{
	int version, taglen;
	long filesize = fbfs->filelength(fbds);
	if(filesize <= ((long)sizeof(struct APETag_s)))
		return 0;
	if(fbfs->fseek(fbds, -((long)sizeof(struct APETag_s)), SEEK_END) < 0)
		return 0;
	if(fbfs->fread(fbds, (char *)apetag, sizeof(struct APETag_s)) != sizeof(struct APETag_s))
		return 0;
	if(pds_strncmp(apetag->ID, "APETAGEX", sizeof(apetag->ID)) != 0)
		return 0;
	version = PDS_GETB_LE32(apetag->Version);
	if(version != 1000 && version != 2000)
		return 0;
	taglen = PDS_GETB_LE32(apetag->Length);
	if(taglen <= sizeof(struct APETag_s) || (taglen >= filesize))
		return 0;
	if(!PDS_GETB_LE32(apetag->TagCount))
		return 0;
	if(fbfs->fseek(fbds, -taglen, SEEK_END) < 0)
		return 0;
	return 1;
}

int mpxplay_tagging_apetag_check(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds)
{
	struct APETag_s apetag;
	return ape_tag_check_apetag(fbfs, fbds, &apetag);
}

static char *ape_tag_read_apetag(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct APETag_s *apetag, struct mpxplay_infile_info_s *miis, char **id3ip, char *id3p,
								 struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	long tt, version, taglen, tagcount;
	long namelen, datalen, dataflags;
	char *tagbuff, *tbp, *tname, *tdata;

	taglen = PDS_GETB_LE32(apetag->Length);
	tagbuff = malloc(taglen);
	if(!tagbuff)
		return id3p;
	if(fbfs->fread(fbds, tagbuff, taglen - sizeof(struct APETag_s)) != (taglen - sizeof(struct APETag_s)))
		goto err_out_tagread;
	tbp = tagbuff;
	tagbuff[taglen - sizeof(struct APETag_s)] = 0;
	version = PDS_GETB_LE32(apetag->Version);
	tagcount = PDS_GETB_LE32(apetag->TagCount);
	for(; *tbp && tagcount--;) {
		datalen = PDS_GETB_LE32(tbp);
		tbp += 4;
		dataflags = PDS_GETB_LE32(tbp);
		tbp += 4;
		namelen = pds_strlen(tbp) + 1;
		if(namelen > 128)		// impossible data
			break;
		tname = tbp;
		tbp += namelen;
		tdata = tbp;
		tbp += datalen;
		if(tbp > (&tagbuff[taglen - sizeof(struct APETag_s)]))	// bad data -> overflow
			break;
		if((namelen > 1) && ((version == 1000 && datalen > 1) || (version == 2000 && datalen > 0 && !(dataflags & 2)))) {
			/*if(pds_stricmp(tname,"replaygain_track_gain")==0){// || pds_stricmp(tname,"replaygain_album_gain")==0)
			   miis->audio_decoder_infos->replaygain=atof(tdata);
			   //fprintf(stdout,"%s %s %2.2f df:%d\n",tname,tdata,miis->audio_decoder_infos->replaygain,dataflags);
			   }else */  {
				for(tt = 0; tt < TAGTYPENUM; tt++) {
					int i3iindex = tagtypes[tt].i3i_index;
					if(!id3ip[i3iindex] && pds_stricmp(tname, tagtypes[tt].name) == 0) {
						id3ip[i3iindex] = id3p;
						pds_memcpy(id3p, tdata, datalen);
						if(version == 1000) {
							id3p += mpxplay_textconv_funcs->all_to_char(id3p, datalen - 1, 0) + 1;	// datalen includes the terminating null in v1
						} else {
							id3p[datalen] = 0;
							if((*(mpxplay_textconv_funcs->control)) & ID3TEXTCONV_UTF_AUTO)
								datalen = mpxplay_textconv_funcs->utf8_to_char(id3p, datalen);	// allways in v2
							id3p += mpxplay_textconv_funcs->all_to_char(id3p, datalen, ID3TEXTCONV_UTF8) + 1;	// datalen does not include the terminating null in v2
						}
						break;
					}
				}
			}
		}
	}
  err_out_tagread:
	free(tagbuff);
	return id3p;
}

char *mpxplay_tagging_apetag_get(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, char *id3p,
								 struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	struct APETag_s apetag;

	if(ape_tag_check_apetag(fbfs, fbds, &apetag))
		return ape_tag_read_apetag(fbfs, fbds, &apetag, miis, id3ip, id3p, mpxplay_textconv_funcs);

	return id3p;
}

int mpxplay_tagging_apetag_put(struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, struct mpxplay_infile_info_s *miis, char **id3ip, struct mpxplay_textconv_func_s *mpxplay_textconv_funcs)
{
	int tt, i, extratags = 0, cutfile = 0, retcode = MPXPLAY_ERROR_INFILE_OK;
	long alltagsize = 0, tagcount = 0, newalltagsize, *tagdlens = NULL, *tagflags = NULL;
	mpxp_int32_t tagnamelen, tagdatalen, flags = 0, version;
	char *tagbuff = NULL, *tbp, **tagnames = NULL, **tagdatas = NULL, *extradatas[TAGTYPENUM];
	struct APETag_s apetag;

	if(mpxplay_tagging_id3v1_check(fbfs, fbds))
		return mpxplay_tagging_id3v1_put(fbfs, fbds, miis, id3ip, mpxplay_textconv_funcs);

	pds_memset(extradatas, 0, sizeof(extradatas));

	if(ape_tag_check_apetag(fbfs, fbds, &apetag)) {	// APE file already has APETAGEX
		// load all tagdatas, update and write back
		version = PDS_GETB_LE32(apetag.Version);
		if((version != 1000) && (version != 2000))
			return MPXPLAY_ERROR_INFILE_WRITETAG_TAGTYPE;
		alltagsize = PDS_GETB_LE32(apetag.Length);
		tbp = tagbuff = malloc(alltagsize);
		tagcount = PDS_GETB_LE32(apetag.TagCount);
		tagnames = (char **)malloc((tagcount + TAGTYPENUM) * sizeof(char *));
		tagdatas = (char **)malloc((tagcount + TAGTYPENUM) * sizeof(char *));
		tagdlens = (long *)malloc((tagcount + TAGTYPENUM) * sizeof(long));
		tagflags = (long *)malloc((tagcount + TAGTYPENUM) * sizeof(long));
		if(!tagbuff || !tagnames || !tagdatas || !tagdlens || !tagflags) {
			retcode = MPXPLAY_ERROR_INFILE_MEMORY;
			goto close_tagput;
		}
		//load
		if(fbfs->fread(fbds, tagbuff, alltagsize - sizeof(struct APETag_s)) != (alltagsize - sizeof(struct APETag_s))) {
			retcode = MPXPLAY_ERROR_INFILE_EOF;
			goto close_tagput;
		}

		for(i = 0; i < tagcount && (tbp < (tagbuff + alltagsize - sizeof(struct APETag_s))); i++) {
			tagdlens[i] = PDS_GETB_LE32(tbp);
			tbp += 4;
			tagflags[i] = PDS_GETB_LE32(tbp);
			tbp += 4;
			tagnames[i] = tbp;
			tbp += pds_strlen(tbp) + 1;
			tagdatas[i] = tbp;
			tbp += tagdlens[i];
		}
		tagcount = i;			// correction in the case of a wrong ape-tag
		//update
		for(tt = 0; tt < TAGTYPENUM; tt++) {
			int i3iindex = tagtypes[tt].i3i_index;
			for(i = 0; i < tagcount; i++) {
				if(!(tagflags[i] & 2)) {
					if(pds_stricmp(tagnames[i], tagtypes[tt].name) == 0) {	// found, update
						unsigned int len = tagging_convert_chars_out(&extradatas[tt], id3ip[i3iindex], mpxplay_textconv_funcs, ((version == 2000) ? 1 : 0));
						if(len) {
							tagdatas[i] = extradatas[tt];
							tagdlens[i] = len + ((version == 2000) ? 0 : 1);	// v2 does not count terminating null
						} else {
							tagdatas[i] = "";
							tagdlens[i] = (version == 2000) ? 0 : 1;
						}
						break;
					}
				}
			}
			if(i == tagcount) {	// not found, new
				unsigned int len = tagging_convert_chars_out(&extradatas[tt], id3ip[i3iindex], mpxplay_textconv_funcs, ((version == 2000) ? 1 : 0));
				if(len) {
					tagnames[tagcount + extratags] = tagtypes[tt].name;
					tagdatas[tagcount + extratags] = extradatas[tt];
					tagdlens[tagcount + extratags] = len + ((version == 2000) ? 0 : 1);
					tagflags[tagcount + extratags] = 0;
					extratags++;
				}
			}
		}
		//write back
		if(fbfs->fseek(fbds, -alltagsize, SEEK_END) < 0) {
			retcode = MPXPLAY_ERROR_INFILE_EOF;
			goto close_tagput;
		}
		newalltagsize = 0;
		tagcount += extratags;
		for(i = 0; i < tagcount; i++) {
			if((version == 1000 && tagdlens[i] > 1) || (version == 2000 && tagdlens[i])) {
				tagnamelen = pds_strlen(tagnames[i]) + 1;
				fbfs->fwrite(fbds, &tagdlens[i], sizeof(tagdlens[i]));
				fbfs->fwrite(fbds, &tagflags[i], sizeof(tagflags[i]));
				fbfs->fwrite(fbds, tagnames[i], tagnamelen);
				fbfs->fwrite(fbds, tagdatas[i], tagdlens[i]);
				newalltagsize += sizeof(tagdlens[i]) + sizeof(tagflags[i]) + tagnamelen + tagdlens[i];
			}
		}
		if(newalltagsize < alltagsize)
			cutfile = 1;
		alltagsize = newalltagsize;
	} else {
		// write an absolute new APEv2 tag
		version = 2000;
		if(fbfs->fseek(fbds, 0, SEEK_END) < 0)
			return MPXPLAY_ERROR_INFILE_EOF;
		for(tt = 0; tt < TAGTYPENUM; tt++) {
			int i3iindex = tagtypes[tt].i3i_index;
			if(id3ip[i3iindex]) {
				unsigned int len = tagging_convert_chars_out(&extradatas[0], id3ip[i3iindex], mpxplay_textconv_funcs, ((version == 2000) ? 1 : 0));
				if(len) {
					tagnamelen = pds_strlen(tagtypes[tt].name) + 1;
					tagdatalen = len + ((version == 2000) ? 0 : 1);
					fbfs->fwrite(fbds, &tagdatalen, sizeof(tagdatalen));
					fbfs->fwrite(fbds, &flags, sizeof(flags));
					fbfs->fwrite(fbds, tagtypes[tt].name, tagnamelen);
					fbfs->fwrite(fbds, extradatas[0], tagdatalen);
					alltagsize += sizeof(tagdatalen) + sizeof(flags) + tagnamelen + tagdatalen;
					tagcount++;
					free(extradatas[0]);	// ???
					extradatas[0] = NULL;
				}
			}
		}
	}
	//finish/close the tag
	memcpy(apetag.ID, "APETAGEX", sizeof("APETAGEX"));
	PDS_PUTB_LE32(apetag.Version, version);
	PDS_PUTB_LE32(apetag.Length, alltagsize + sizeof(struct APETag_s));
	PDS_PUTB_LE32(apetag.TagCount, tagcount);
	PDS_PUTB_LE32(apetag.Flags, 0);
	memset(apetag.Reserved, 0, sizeof(apetag.Reserved));
	fbfs->fwrite(fbds, (void *)&apetag, sizeof(struct APETag_s));

	if(cutfile)
		if(!fbfs->chsize(fbds, fbfs->ftell(fbds)))
			retcode = MPXPLAY_ERROR_INFILE_EOF;

  close_tagput:
	if(tagflags)
		free(tagflags);
	if(tagdlens)
		free(tagdlens);
	if(tagdatas)
		free(tagdatas);
	if(tagnames)
		free(tagnames);
	if(tagbuff)
		free(tagbuff);
	for(tt = 0; tt < TAGTYPENUM; tt++)
		if(extradatas[tt])
			free(extradatas[tt]);
	return retcode;
}
