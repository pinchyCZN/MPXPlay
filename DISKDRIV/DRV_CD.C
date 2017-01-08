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
//function: drive handling for CDs (CD ripping)

#include "in_file.h"
#include "newfunc\newfunc.h"
#include "cd_drive.h"

#define CDDRIVE_DISKTYPE_DATA  1
#define CDDRIVE_DISKTYPE_AUDIO 2
#define CDDRIVE_DISKTYPE_MIXED (CDDRIVE_DISKTYPE_AUDIO|CDDRIVE_DISKTYPE_DATA)

#define CDW_CONTROLFLAG_JITTER  1

typedef struct cddrive_info_s {
	unsigned int infobits;
	unsigned int drivenum;
	struct cddrive_lowlevel_func_s *lowlevel_funcs;
	void *lowlevel_datas;

	unsigned int current_read_trackindex;
	unsigned int current_read_frame;
	char *readbuffer;			// required by jitter + gapless playing
	long readbuf_leftbytes;		// to put these here
	int cd_jitter;				//

	unsigned int disktype;
	struct cddrive_disk_info_s diskinfos;
	struct cddrive_track_info_s *trackinfos;

	pds_find_t *ffbl_root;
	unsigned int ff_type_base, ff_type;	// used DISKTYPE
	unsigned int ff_currtrack;
	unsigned int ff_endtrack;
} cddrive_info_s;

#pragma pack(push,2)			// not really needed

typedef struct wavhead_s {
	unsigned long riff_riffID;
	unsigned long riff_rLen;
	unsigned long wave_waveID;
	unsigned long fmt_fmtID;
	unsigned long fmt_fLen;
	unsigned short fmt_wTag;
	unsigned short fmt_wChannel;
	unsigned long fmt_nSample;
	unsigned long fmt_nByte;
	unsigned short fmt_align;
	unsigned short fmt_sample;
	unsigned long data_dataID;
	unsigned long data_dLen;
} wavehead_s;

#pragma pack(pop)

#define CDFILEINFO_WAVHEAD_SIZE sizeof(struct wavhead_s)

static void cddrive_drive_unmount(void *drivehand_data);
static void cdw_clear_trackinfos(struct cddrive_info_s *cdis);
static int cdw_extract_tracknumber_from_filename(struct cddrive_info_s *cdis, char *filename);
static unsigned int cdw_create_ffblk(struct cddrive_info_s *cdis, struct pds_find_t *ffblk);
static unsigned int cdw_load_trackinfos_all(struct cddrive_info_s *cdis);
static unsigned int cdw_set_speed(struct cddrive_info_s *cdis, unsigned int speed);
static mpxp_filesize_t cddrive_file_length(void *filehand_data);

extern unsigned int cdw_control_flags, cdw_control_speed;

#ifdef MPXPLAY_WIN32
extern struct cddrive_lowlevel_func_s WINASPICD_lowlevel_funcs;
#endif
#ifdef __DOS__
extern struct cddrive_lowlevel_func_s MSCDEX_lowlevel_funcs;
#endif

static unsigned int cddrive_drive_check(char *pathname)
{
	int drivenum = pds_getdrivenum_from_path(pathname);
	if(drivenum < 0)
		return 0;
#ifdef MPXPLAY_WIN32
	return WINASPICD_lowlevel_funcs.drive_check(drivenum);
#elif defined(__DOS__)
	return MSCDEX_lowlevel_funcs.drive_check(drivenum);
#else
	return 0;
#endif
}

static long cddrive_drive_config(void *drivehand_data, unsigned long funcnum, void *argp1, void *argp2)
{
	struct cddrive_info_s *cdis = drivehand_data;
	int retcode;

	if(!cdis)
		return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;

	switch (funcnum) {
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_CMD_MEDIAEJECTLOAD:
		if(!cdis->lowlevel_funcs || !cdis->lowlevel_funcs->load_unload_media || !cdis->lowlevel_datas)
			return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;
		retcode = cdis->lowlevel_funcs->load_unload_media(cdis->lowlevel_datas, cdis->drivenum);
		if(retcode != CDDRIVE_RETCODE_OK)
			return 0;
		cdw_set_speed(cdis, cdw_control_speed);
		return 1;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVLETTERSTR:
		if(!argp1 || !argp2)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		snprintf((char *)argp1, *((unsigned long *)argp2), "%c:", ('A' + cdis->drivenum));
		return 1;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVTYPENAME:
		if(!argp1 || !argp2)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		snprintf((char *)argp1, *((unsigned long *)argp2), "%s", "<CD_ROM>");
		return 1;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIREXISTS:
		if(pds_dir_exists((char *)argp1))
			return 1;
		if(pds_strlen((char *)argp1) < sizeof(PDS_DIRECTORY_ROOTDIR_STR))
			if(cdw_load_trackinfos_all(cdis))	// ??? faster way?
				return 1;
		return 0;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_CHKBUFBLOCKBYTES:
		if(funcbit_test(cdis->ff_type_base, CDDRIVE_DISKTYPE_AUDIO))
			return CDFILEINFO_WAVHEAD_SIZE;
		return -2;
		//case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_PREREADBUFBYTES:
		// return 65536; // !!! ???
	}
	return MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC;
}

static void *cddrive_drive_mount(char *pathname)
{
	struct cddrive_info_s *cdis;
	int drivenum = pds_getdrivenum_from_path(pathname);
	if(drivenum < 0)
		return NULL;

	cdis = (struct cddrive_info_s *)calloc(1, sizeof(*cdis));
	if(!cdis)
		return cdis;

	cdis->drivenum = drivenum;
#ifdef MPXPLAY_WIN32
	cdis->lowlevel_funcs = &WINASPICD_lowlevel_funcs;
#elif defined(__DOS__)
	cdis->lowlevel_funcs = &MSCDEX_lowlevel_funcs;
#else
	goto err_out_mount;
#endif
	cdis->lowlevel_datas = cdis->lowlevel_funcs->drive_mount(drivenum);

	if(cdis->lowlevel_datas) {
		cdis->readbuffer = calloc(1, CD_READBUF_SIZE + CD_SYNC_SIZE + 32);
		if(!cdis->readbuffer)
			goto err_out_mount;
	}

	return cdis;

  err_out_mount:
	cddrive_drive_unmount(cdis);
	return NULL;
}

static void cddrive_drive_unmount(void *drivehand_data)
{
	struct cddrive_info_s *cdis = drivehand_data;
	if(cdis) {
		if(cdis->lowlevel_funcs && cdis->lowlevel_funcs->drive_unmount)	// && cdis->lowlevel_datas)
			cdis->lowlevel_funcs->drive_unmount(cdis->lowlevel_datas);
		cdw_clear_trackinfos(cdis);
		if(cdis->readbuffer)
			free(cdis->readbuffer);
		free(cdis);
	}
}

static unsigned int cddrive_findfirst(void *drivehand_data, char *searchname, unsigned int attrib, struct pds_find_t *ffblk)
{
	struct cddrive_info_s *cdis = drivehand_data;
	struct cddrive_disk_info_s *diskinfos;
	unsigned int error = 1;
	int track;
	char path[MAX_PATHNAMELEN];

	if(!cdis)
		return error;

	error = pds_findfirst(searchname, attrib, ffblk);

	pds_getpath_from_fullname(path, searchname);

	if(pds_strlen(path) < sizeof(PDS_DIRECTORY_ROOTDIR_STR)) {	// root dir

		if(!error)
			funcbit_enable(cdis->ff_type, CDDRIVE_DISKTYPE_DATA);

		if(attrib == _A_NORMAL) {

			track = cdw_extract_tracknumber_from_filename(cdis, searchname);

			if(track != 0) {
				if(!cdw_load_trackinfos_all(cdis))
					goto err_no_cda;
				diskinfos = &cdis->diskinfos;
				if(track < 0) {
					cdis->ff_currtrack = diskinfos->firsttrack;
					cdis->ff_endtrack = diskinfos->lasttrack;
				} else
					cdis->ff_currtrack = cdis->ff_endtrack = track;
				if(!(cdis->ff_type & CDDRIVE_DISKTYPE_DATA))
					if(!cdw_create_ffblk(cdis, ffblk))
						goto err_no_cda;
				funcbit_enable(cdis->ff_type, CDDRIVE_DISKTYPE_AUDIO);
			}
		  err_no_cda:
			cdw_set_speed(cdis, cdw_control_speed);	// sets the speed in the root-dir only
		}

		if(cdis->ff_type) {
			error = 0;
			cdis->ffbl_root = ffblk;	// !!! assumed it's static in caller
		} else
			cdis->ffbl_root = NULL;

		cdis->ff_type_base = cdis->ff_type;
	}

	return error;
}

// returns 0 at no error (non-zero at error/finished)
static unsigned int cddrive_findnext(void *drivehand_data, struct pds_find_t *ffblk)
{
	struct cddrive_info_s *cdis = drivehand_data;
	if(!cdis)
		return 1;
	if((cdis->ff_type & CDDRIVE_DISKTYPE_DATA) || (ffblk != cdis->ffbl_root)) {
		if(pds_findnext(ffblk) == 0)
			return 0;
		if(ffblk != cdis->ffbl_root)
			return 1;
		funcbit_disable(cdis->ff_type, CDDRIVE_DISKTYPE_DATA);
	}
	if(cdis->ff_type & CDDRIVE_DISKTYPE_AUDIO) {
		if(cdw_create_ffblk(cdis, ffblk))
			return 0;
		funcbit_disable(cdis->ff_type, CDDRIVE_DISKTYPE_AUDIO);
	}
	return 1;
}

static void cddrive_findclose(void *drivehand_data, struct pds_find_t *ffblk)
{
	struct cddrive_info_s *cdis = drivehand_data;
	if(!cdis || (cdis->ff_type_base & CDDRIVE_DISKTYPE_DATA) || (ffblk != cdis->ffbl_root))
		pds_findclose(ffblk);
	if(cdis && (ffblk == cdis->ffbl_root))
		cdis->ffbl_root = NULL;
}

static char *cddrive_getcwd(void *drivehand_data, char *buf, unsigned int buflen)
{
	struct cddrive_info_s *cdis = drivehand_data;
	if(!cdis)
		return NULL;
	pds_getdcwd(cdis->drivenum + 1, buf);
	return buf;
}

static int cddrive_chdir(void *drivehand_data, char *path)
{
	return pds_chdir(path);
}

//------------------------------------------------------------------------
static void cdw_clear_trackinfos(struct cddrive_info_s *cdis)
{
	struct cddrive_disk_info_s *diskinfos = &cdis->diskinfos;
	struct cddrive_track_info_s *trackinfo = cdis->trackinfos;
	unsigned int i;

	if(trackinfo) {
		for(i = 0; i < diskinfos->nb_tracks; i++) {
			if(trackinfo->artist && (trackinfo->artist != diskinfos->artist_name))	// !!!
				free(trackinfo->artist);
			if(trackinfo->title && (trackinfo->title != diskinfos->album_name))	// !!!
				free(trackinfo->title);
			trackinfo++;
		}
		free(cdis->trackinfos);
		cdis->trackinfos = NULL;
	}
	if(diskinfos->artist_name)
		free(diskinfos->artist_name);
	if(diskinfos->album_name)
		free(diskinfos->album_name);
	pds_memset(diskinfos, 0, sizeof(*diskinfos));
}

static int cdw_extract_tracknumber_from_filename(struct cddrive_info_s *cdis, char *filename)
{
	int track = 0, drive;
	char *ext, *tc, *fn;

	drive = pds_getdrivenum_from_path(filename);
	if(drive < 0)
		drive = cdis->drivenum;
	else if(drive != cdis->drivenum)
		return track;

	ext = pds_strrchr(filename, '.');
	if(!ext)
		return track;

	ext++;

	if(pds_stricmp(ext, "cdw") == 0 || pds_strcmp(ext, "*") == 0) {	// .cdw or .*
		tc = &ext[-2];
		if(tc[0] == '*')		// !!! bad
			track = -1;
		else {
			fn = pds_getfilename_from_fullname(filename);
			if(fn && pds_strncmp(fn + 2, " - ", sizeof(" - ") - 1) == 0)	// lfn format (NN - Artist - Title.CDW)
				track = pds_atol(fn);
			if(track <= 0) {	// ??? not found, try sfn format (TRACKNN.CDW)
				while(tc[-1] >= '0' && tc[-1] <= '9')
					tc--;
				track = pds_atol(tc);
			}
		}
	}
	return track;
}

static int cdw_cdatracknum_to_disktrackindex(struct cddrive_info_s *cdis, unsigned int cdatracknum)
{
	struct cddrive_disk_info_s *diskinfos = &cdis->diskinfos;
	struct cddrive_track_info_s *trackinfo = &cdis->trackinfos[0];
	unsigned int i;

	if(!trackinfo || !cdatracknum)
		return -1;

	for(i = 0; i < diskinfos->nb_tracks; i++) {
		if(trackinfo->tracktype == CDDRIVE_TRACKTYPE_AUDIO) {
			cdatracknum--;
			if(!cdatracknum)
				return i;
		}
		trackinfo++;
	}
	return -1;
}

static unsigned int cdw_disktrackindex_to_cdatracknum(struct cddrive_info_s *cdis, unsigned int disktracknum)
{
	struct cddrive_disk_info_s *diskinfos = &cdis->diskinfos;
	struct cddrive_track_info_s *trackinfo = &cdis->trackinfos[0];
	unsigned int i, cdatracknum = 0;

	if(!trackinfo)
		return cdatracknum;

	for(i = diskinfos->firsttrack; i <= disktracknum; i++) {
		if(trackinfo->tracktype == CDDRIVE_TRACKTYPE_AUDIO)
			cdatracknum++;
		trackinfo++;
	}
	return cdatracknum;
}

static void cdw_create_filename(struct cddrive_info_s *cdis, char *filename, unsigned int maxnamelen, struct cddrive_track_info_s *t)
{
	unsigned int cdatracknum = cdw_disktrackindex_to_cdatracknum(cdis, t->tracknum);
	if(t->artist || t->title) {
		char *p;
		snprintf(filename, maxnamelen, "%2.2d - %.100s%s%.100s.cdw", cdatracknum, ((t->artist) ? t->artist : ""), ((t->artist && t->title) ? " - " : ""), ((t->title) ? t->title : ""));
		p = filename;
		do {
			char c = *p;
			switch (c) {		// convert not-allowed chars in filename
			case '/':
			case '\\':
				c = '&';
				break;
			case '?':
			case '*':
			case '\"':
			case '|':
				c = ' ';
				break;
			case '<':
				c = '[';
				break;
			case '>':
				c = ']';
				break;
			}
			*p++ = c;
		} while(*p);
	} else
		snprintf(filename, maxnamelen, "TRACK%2.2d.CDW", cdatracknum);
}

static unsigned long cdw_get_trackfilelen(struct cddrive_track_info_s *trackinfo)
{
	unsigned long filelen;
	filelen = trackinfo->framepos_end - trackinfo->framepos_begin + 1;
	filelen *= CD_FRAME_SIZE;
	filelen += CDFILEINFO_WAVHEAD_SIZE;
	return filelen;
}

static unsigned int cdw_create_ffblk(struct cddrive_info_s *cdis, struct pds_find_t *ffblk)
{
	struct cddrive_track_info_s *trackinfo = &cdis->trackinfos[cdis->ff_currtrack - cdis->diskinfos.firsttrack];

	while(cdis->ff_currtrack <= cdis->ff_endtrack) {
		cdis->ff_currtrack++;
		if(trackinfo->tracktype == CDDRIVE_TRACKTYPE_AUDIO) {
			cdw_create_filename(cdis, ffblk->name, sizeof(ffblk->name), trackinfo);
			ffblk->attrib = _A_NORMAL;
			ffblk->size = cdw_get_trackfilelen(trackinfo);
			return 1;
		}
		trackinfo++;
	}
	return 0;
}

static unsigned int cdw_load_trackinfos_all(struct cddrive_info_s *cdis)
{
	struct cddrive_disk_info_s *diskinfos = &cdis->diskinfos;
	struct cddrive_track_info_s *trackinfos;
	unsigned int i, retcode;

	cdw_clear_trackinfos(cdis);
	retcode = cdis->lowlevel_funcs->get_diskinfo(cdis->lowlevel_datas, cdis->drivenum, diskinfos);
	if(retcode != CDDRIVE_RETCODE_OK)
		return 0;

	if(diskinfos->nb_tracks) {	// aspi style
		diskinfos->firsttrack = 1;
		diskinfos->lasttrack = diskinfos->nb_tracks;
	} else {					// mscdex style
		if(!diskinfos->firsttrack || !diskinfos->lasttrack || (diskinfos->firsttrack > diskinfos->lasttrack))
			return 0;
		diskinfos->nb_tracks = diskinfos->lasttrack - diskinfos->firsttrack + 1;
	}
	if(diskinfos->nb_tracks > 1000)	// probably impossible
		return 0;

	cdis->trackinfos = trackinfos = (struct cddrive_track_info_s *)calloc(diskinfos->nb_tracks + 4, sizeof(*cdis->trackinfos));
	if(!trackinfos)
		return 0;
	retcode = cdis->lowlevel_funcs->get_trackinfos(cdis->lowlevel_datas, cdis->drivenum, trackinfos);
	if(retcode != CDDRIVE_RETCODE_OK)
		return 0;
	for(i = 0; i < (diskinfos->nb_tracks - 1); i++)
		trackinfos[i].framepos_end = trackinfos[i + 1].framepos_begin - 1;
	trackinfos[diskinfos->nb_tracks - 1].framepos_end = (diskinfos->nb_frames) ? diskinfos->nb_frames : trackinfos[diskinfos->nb_tracks - 1].framepos_begin;
	return 1;
}

static unsigned int cdw_load_trackinfo_one(struct cddrive_info_s *cdis, unsigned int tracknum, struct cddrive_track_info_s *trackinfo)
{
	int trackindex;
	if(!cdis->trackinfos)
		if(!cdw_load_trackinfos_all(cdis))
			return 0;

	trackindex = cdw_cdatracknum_to_disktrackindex(cdis, tracknum);
	if(trackindex < 0)
		return 0;
	pds_memcpy(trackinfo, &cdis->trackinfos[trackindex], sizeof(*trackinfo));
	return 1;
}

static unsigned int cdw_set_speed(struct cddrive_info_s *cdis, unsigned int speed)
{
	if(!cdis->lowlevel_funcs->set_speed)
		return CDDRIVE_RETCODE_FAILED;
	if(!speed)
		return CDDRIVE_RETCODE_OK;
	return cdis->lowlevel_funcs->set_speed(cdis->lowlevel_datas, speed);
}

//------------------------------------------------------------------------
#define CDFILEINFO_INFOBIT_STARTREAD_OK     1
#define CDFILEINFO_INFOBIT_STARTREAD_FAILED 2
#define CDFILEINFO_INFOBIT_SEEK             16	// at jitter

#define CDFILE_ERROR_LIMIT   32
#define CDFILE_MAX_RETRIES   6

typedef struct cdfile_info_s {
	unsigned long infobits;
	int drivenumber_r;
	int filehand;				// normal file
	int tracknumber;			// audio track
	long track_beginpos;
	long track_endpos;
	long track_currpos;
	long track_len;
	long filepos;
	unsigned long checknum;
	struct cddrive_info_s *cdis;
	struct cddrive_track_info_s trackinfo;
	struct wavhead_s wh;
} cdfile_info_s;

static void cddrive_file_close(void *filehand_data);
static void cdw_wav_header_create(struct cdfile_info_s *cdfi);

static unsigned int cddrive_drivenum_read;
static unsigned long cddrive_file_checknum;

static unsigned int cddrive_file_check(void *drivehand_data, char *filename)
{
	if(!drivehand_data)
		return 0;
	if(cdw_extract_tracknumber_from_filename(drivehand_data, filename) > 0)
		return 1;
	return 0;
}

static void *cddrive_file_open(void *drivehand_data, char *filename, unsigned long openmode)
{
	struct cddrive_info_s *cdis = drivehand_data;
	struct cdfile_info_s *cdfi;
	//char trackname[MAX_PATHNAMELEN];

	if(!cdis || (openmode & (O_RDWR | O_WRONLY | O_CREAT)))
		return NULL;

	cdfi = calloc(1, sizeof(*cdfi));
	if(!cdfi)
		return cdfi;
	cdfi->cdis = cdis;
	cdfi->drivenumber_r = cdis->drivenum;
	cdfi->tracknumber = cdw_extract_tracknumber_from_filename(cdis, filename);
	if(cdfi->tracknumber > 0) {
		if(!cdw_load_trackinfo_one(cdis, cdfi->tracknumber, &cdfi->trackinfo))
			goto err_out_open;

		/*cdw_create_filename(cdis,trackname,sizeof(trackname),&cdfi->trackinfo); //
		   if(pds_stricmp(pds_getfilename_from_fullname(filename),trackname)!=0) // !!!
		   goto err_out_open;                    // */

		cdfi->track_beginpos = cdfi->track_currpos = cdfi->trackinfo.framepos_begin;
		cdfi->track_endpos = cdfi->trackinfo.framepos_end + 1;
		cdfi->track_len = cdfi->track_endpos - cdfi->track_beginpos;

		cdw_wav_header_create(cdfi);

		funcbit_enable(cdis->ff_type_base, CDDRIVE_DISKTYPE_AUDIO);	// ???
	} else {
		cdfi->filehand = pds_open_read(filename, openmode);
		if(cdfi->filehand <= 0)
			goto err_out_open;
	}

	return cdfi;

  err_out_open:
	cddrive_file_close(cdfi);
	return NULL;
}

static void cddrive_file_close(void *filehand_data)
{
	struct cdfile_info_s *cdfi = filehand_data;
	if(cdfi) {
		if(cdfi->filehand)
			pds_close(cdfi->filehand);
		free(cdfi);
	}
}

static void cdw_wav_header_create(struct cdfile_info_s *cdfi)
{
	struct wavhead_s *wh = &cdfi->wh;

	wh->riff_riffID = PDS_GET4C_LE32('R', 'I', 'F', 'F');
	wh->riff_rLen = cddrive_file_length(cdfi) - 8;	// filelen-sizeof(struct RIFF);
	wh->wave_waveID = PDS_GET4C_LE32('W', 'A', 'V', 'E');
	wh->fmt_fmtID = PDS_GET4C_LE32('f', 'm', 't', ' ');
	wh->fmt_fLen = 16;			// sizeof(struct FORMAT)-8;
	wh->fmt_wTag = MPXPLAY_WAVEID_PCM_SLE;
	wh->fmt_wChannel = 2;
	wh->fmt_nSample = 44100;
	wh->fmt_nByte = 44100 * 2 * 2;
	wh->fmt_align = 2 * 2;
	wh->fmt_sample = 16;
	wh->data_dataID = PDS_GET4C_LE32('d', 'a', 't', 'a');
	wh->data_dLen = wh->riff_rLen - (4 + 24 + 8);	// -(sizeof(struct WAVE)+sizeof(struct FORMAT)+sizeof(struct DATA));
}

static unsigned int cdw_read_sectors_cda(struct cdfile_info_s *cdfi, unsigned long sectorpos, unsigned long sectornum)
{
	if(cdfi->cdis->lowlevel_funcs->read_sectors_cda)
		return cdfi->cdis->lowlevel_funcs->read_sectors_cda(cdfi->cdis->lowlevel_datas, cdfi->cdis->readbuffer, sectorpos, sectornum);
	return 0;
}

static unsigned int cdw_start_read(struct cdfile_info_s *cdfi)
{
	struct cddrive_info_s *cdis;
	long diff;
	unsigned int retcode = CDDRIVE_RETCODE_OK;

	cdis = cdfi->cdis;

	if(cdis->lowlevel_funcs->start_read) {
		retcode = cdis->lowlevel_funcs->start_read(cdis->lowlevel_datas, cdis->drivenum, &cdfi->trackinfo);
		if(retcode == CDDRIVE_RETCODE_FAILED)
			return retcode;
	}

	if(cdfi->drivenumber_r == cddrive_drivenum_read)	// we don't read paralell on the same disk/drive (it's too slow to do it)
		cddrive_file_checknum++;
	else
		cddrive_drivenum_read = cdfi->drivenumber_r;

	cdfi->checknum = cddrive_file_checknum;

	diff = cdfi->track_currpos - cdis->current_read_frame;
	if(diff < 0)
		diff = -diff;

	if((retcode == CDDRIVE_RETCODE_RESET_READ) || (diff > 100)) {	// drive has changed or not-next-track
		cdis->current_read_frame = cdfi->track_currpos;
		cdis->readbuf_leftbytes = 0;
		funcbit_enable(cdfi->infobits, CDFILEINFO_INFOBIT_SEEK);
	} else
		cdfi->track_currpos = cdis->current_read_frame;

	return CDDRIVE_RETCODE_OK;
}

static long cddrive_file_read(void *filehand_data, char *ptr, unsigned int num)
{
	struct cdfile_info_s *cdfi = filehand_data;
	struct cddrive_info_s *cdis;
	int i, j;
	int jitter_dev, cs, jitter1, jitter2, match, tries;
	int min_errors, curr_errors, max_reached, min_error_jitter;
	unsigned long sectors;

	if(!cdfi)
		return 0;
	if(cdfi->filehand)
		return pds_dos_read(cdfi->filehand, ptr, num);

	i = 0;
	if(cdfi->filepos < CDFILEINFO_WAVHEAD_SIZE) {
		j = min(num, (CDFILEINFO_WAVHEAD_SIZE - cdfi->filepos));
		pds_memcpy(ptr, ((char *)&cdfi->wh) + cdfi->filepos, j);
		num -= j;
		ptr += j;
		i += j;
	}
	if(!num)
		goto end_read;

	if(!(cdfi->infobits & (CDFILEINFO_INFOBIT_STARTREAD_OK | CDFILEINFO_INFOBIT_STARTREAD_FAILED))) {
		if(cdw_start_read(cdfi) == CDDRIVE_RETCODE_OK)
			funcbit_enable(cdfi->infobits, CDFILEINFO_INFOBIT_STARTREAD_OK);
		else
			funcbit_enable(cdfi->infobits, CDFILEINFO_INFOBIT_STARTREAD_FAILED);
	}
	if(funcbit_test(cdfi->infobits, CDFILEINFO_INFOBIT_STARTREAD_FAILED))
		goto end_read;
	if(cdfi->checknum != cddrive_file_checknum)	// we want to read paralell on the same disk
		goto end_read;			// not allowed

	cdis = cdfi->cdis;

	if(cdw_control_flags & CDW_CONTROLFLAG_JITTER) {
		do {
			if(cdis->readbuf_leftbytes > 0) {
				if(num > cdis->readbuf_leftbytes)
					j = cdis->readbuf_leftbytes;
				else
					j = num;
				pds_memcpy(ptr, &cdis->readbuffer[CD_READBUF_SIZE - cdis->readbuf_leftbytes], j);
				ptr += j;
				cdis->readbuf_leftbytes -= j;
				num -= j;
				i += j;
			}
			if(cdfi->track_currpos >= cdfi->track_endpos)
				break;
			if(cdis->readbuf_leftbytes <= 0) {
				sectors = cdfi->track_endpos - cdfi->track_currpos + CD_SYNC_SECTORS;
				if(sectors > CD_READ_SECTORS)
					sectors = CD_READ_SECTORS;
				if((cdfi->track_currpos < CD_SYNC_SECTORS) || (cdfi->infobits & CDFILEINFO_INFOBIT_SEEK)) {
					if(cdw_read_sectors_cda(cdfi, cdfi->track_currpos, sectors) != CDDRIVE_RETCODE_OK)
						break;
					cdis->cd_jitter = 0;
					cdis->readbuf_leftbytes = (sectors - CD_SYNC_SECTORS + 1) * CD_FRAME_SIZE;
					pds_qmemcpyr(&cdis->readbuffer[CD_READBUF_SIZE - cdis->readbuf_leftbytes], &cdis->readbuffer[0], (cdis->readbuf_leftbytes >> 2));	//+1);
					cdfi->track_currpos += sectors;
					funcbit_disable(cdfi->infobits, CDFILEINFO_INFOBIT_SEEK);
				} else {
					pds_qmemcpyr(&cdis->readbuffer[CD_READBUF_SIZE], &cdis->readbuffer[CD_READBUF_SIZE - CD_FRAME_SIZE], (CD_SYNC_SIZE >> 2));	// + 1);

					tries = 0;
					match = 0;
					min_errors = 0;

					while(!match && (tries < CDFILE_MAX_RETRIES)) {
						if(cdw_read_sectors_cda(cdfi, cdfi->track_currpos - CD_SYNC_SECTORS, sectors) != CDDRIVE_RETCODE_OK)
							goto end_read;

						tries++;

						jitter_dev = 0;
						while(1) {
							jitter1 = cdis->cd_jitter + jitter_dev;
							jitter2 = cdis->cd_jitter - jitter_dev;
							if((jitter2 < 0) && (jitter1 >= CD_FRAME_SIZE * CD_SYNC_SECTORS * 2))
								break;
							if(jitter1 < (CD_FRAME_SIZE * CD_SYNC_SECTORS * 2)) {
								for(cs = 0; cs < CD_SYNC_SIZE; cs++)
									if(cdis->readbuffer[jitter1 + cs] != cdis->readbuffer[CD_READBUF_SIZE + cs])
										break;
								if(cs == CD_SYNC_SIZE) {
									cdis->cd_jitter = jitter1;
									match = 1;
									break;
								}
							}
							if(jitter2 >= 0) {
								for(cs = 0; cs < CD_SYNC_SIZE; cs++)
									if(cdis->readbuffer[jitter2 + cs] != cdis->readbuffer[CD_READBUF_SIZE + cs])
										break;
								if(cs == CD_SYNC_SIZE) {
									cdis->cd_jitter = jitter2;
									match = 1;
									break;
								}
							}
							jitter_dev++;
						}
					}

					if(match == 0) {
						min_errors = CDFILE_ERROR_LIMIT;
						max_reached = 0;
						min_error_jitter = cdis->cd_jitter;

						jitter1 = 0;
						while(jitter1 < (CD_FRAME_SIZE * CD_SYNC_SECTORS * 2)) {
							curr_errors = 0;
							for(cs = 0; cs < CD_SYNC_SIZE; cs++)
								if(cdis->readbuffer[jitter1 + cs] != cdis->readbuffer[CD_READBUF_SIZE + cs]) {
									curr_errors++;
									if(curr_errors > CDFILE_ERROR_LIMIT)
										break;
								}
							if(curr_errors < min_errors) {
								min_errors = curr_errors;
								max_reached = cs;
								min_error_jitter = jitter1;
							} else if(curr_errors == min_errors) {
								if(cs > max_reached) {
									max_reached = cs;
									min_error_jitter = jitter1;
								}
							}
							jitter1++;
						}
						cdis->cd_jitter = min_error_jitter;
					}
					cdis->readbuf_leftbytes = sectors * CD_FRAME_SIZE - (cdis->cd_jitter + CD_FRAME_SIZE);
					cdfi->track_currpos += sectors - CD_SYNC_SECTORS;
				}

				// at the end of the track - to get gapless sound
				if(sectors < CD_READ_SECTORS) {
					unsigned int target = CD_READBUF_SIZE - cdis->readbuf_leftbytes;
					unsigned int source = cdis->cd_jitter + CD_FRAME_SIZE;

					if(target > source)	// ??? always have to be
						pds_qmemcpyr(&cdis->readbuffer[target], &cdis->readbuffer[source], (cdis->readbuf_leftbytes >> 2));	//+1);
				}

			}
		} while(num && cdis->readbuf_leftbytes);
	} else {
		do {
			if(cdis->readbuf_leftbytes > 0) {
				if(num > cdis->readbuf_leftbytes)
					j = cdis->readbuf_leftbytes;
				else
					j = num;
				pds_memcpy(ptr, &cdis->readbuffer[CD_READBUF_SIZE - cdis->readbuf_leftbytes], j);
				ptr += j;
				cdis->readbuf_leftbytes -= j;
				num -= j;
				i += j;
			}
			if(cdfi->track_currpos >= cdfi->track_endpos)
				break;
			if(cdis->readbuf_leftbytes <= 0) {
				sectors = cdfi->track_endpos - cdfi->track_currpos;
				if(sectors > CD_READ_SECTORS)
					sectors = CD_READ_SECTORS;
				if(cdw_read_sectors_cda(cdfi, cdfi->track_currpos, sectors) != CDDRIVE_RETCODE_OK)
					break;
				cdfi->track_currpos += sectors;
				cdis->readbuf_leftbytes = sectors * CD_FRAME_SIZE;
				if(cdis->readbuf_leftbytes < CD_READBUF_SIZE)
					pds_qmemcpyr(&cdis->readbuffer[CD_READBUF_SIZE - cdis->readbuf_leftbytes], cdis->readbuffer, (cdis->readbuf_leftbytes >> 2));	//+1);
			}
		} while(num && cdis->readbuf_leftbytes);
	}

	cdis->current_read_frame = cdfi->track_currpos;
	funcbit_disable(cdfi->infobits, CDFILEINFO_INFOBIT_SEEK);

  end_read:
	cdfi->filepos += i;

	return i;
}

static mpxp_filesize_t cddrive_file_tell(void *filehand_data)
{
	struct cdfile_info_s *cdfi = filehand_data;
	if(!cdfi)
		return 0;
	if(cdfi->filehand)
		return pds_tell(cdfi->filehand);

	return (cdfi->track_currpos - cdfi->track_beginpos) * CD_FRAME_SIZE;
}

static mpxp_filesize_t cddrive_file_seek(void *filehand_data, mpxp_filesize_t bytepos, int fromwhere)
{
	struct cdfile_info_s *cdfi = filehand_data;
	unsigned long framepos;
	if(!cdfi)
		return 0;
	if(cdfi->filehand)
		return pds_lseek(cdfi->filehand, bytepos, fromwhere);

	if(bytepos < 0)
		bytepos = 0;
	cdfi->filepos = bytepos;
	cdfi->track_currpos = cdfi->track_beginpos;
	if(bytepos >= CDFILEINFO_WAVHEAD_SIZE) {
		framepos = (bytepos - CDFILEINFO_WAVHEAD_SIZE) / CD_FRAME_SIZE;
		if(framepos >= cdfi->track_endpos)
			return MPXPLAY_ERROR_MPXINBUF_SEEK_EOF;
		cdfi->track_currpos += framepos;
	} else
		framepos = 0;

	cdfi->cdis->readbuf_leftbytes = 0;	// this clears/resets the cdis->readbuffer

	if((framepos > 0) || (funcbit_test(cdfi->infobits, CDFILEINFO_INFOBIT_STARTREAD_OK)))
		funcbit_enable(cdfi->infobits, CDFILEINFO_INFOBIT_SEEK);

	return (cddrive_file_tell(filehand_data));
}

static mpxp_filesize_t cddrive_file_length(void *filehand_data)
{
	struct cdfile_info_s *cdfi = filehand_data;
	if(!cdfi)
		return 0;
	if(cdfi->filehand)
		return pds_filelength(cdfi->filehand);
	return ((mpxp_filesize_t) cdfi->track_len * CD_FRAME_SIZE + sizeof(struct wavhead_s));
}

static int cddrive_file_eof(void *filehand_data)
{
	struct cdfile_info_s *cdfi = filehand_data;
	if(!cdfi)
		return 0;
	if(cdfi->filehand)
		return pds_eof(cdfi->filehand);
	return (((cdfi->track_currpos >= cdfi->track_endpos) && !cdfi->cdis->readbuf_leftbytes) ? 1 : 0);
}

//------------------------------------------------------------------------

struct mpxplay_drivehand_func_s CDDRIVE_drivehand_funcs = {
	"CDDRIVE",
	0,							// infobits
	&cddrive_drive_config,
	&cddrive_drive_check,
	&cddrive_drive_mount,
	&cddrive_drive_unmount,
	&cddrive_findfirst,
	&cddrive_findnext,
	&cddrive_findclose,
	&cddrive_getcwd,
	&cddrive_chdir,
	NULL,						// mkdir
	NULL,						// rmdir
	NULL,						// rename
	NULL,						// unlink
	NULL,						// r15
	NULL,						// r16
	NULL,						// r17
	NULL,						// r18
	NULL,						// r19
	NULL,						// r20

	&cddrive_file_check,
	&cddrive_file_open,
	&cddrive_file_close,
	&cddrive_file_read,
	NULL,						// file_write
	&cddrive_file_seek,
	&cddrive_file_tell,
	&cddrive_file_length,
	&cddrive_file_eof,
	NULL,						// file_chsize
	NULL						// r31
};
