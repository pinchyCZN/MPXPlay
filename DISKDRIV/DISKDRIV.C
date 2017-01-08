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
//function: drive handling

#include "in_file.h"
#include "newfunc\newfunc.h"
#include "control\control.h"
#include "diskdriv.h"
#include <malloc.h>

extern struct mpxplay_drivehand_func_s CDDRIVE_drivehand_funcs;
extern struct mpxplay_drivehand_func_s HDDRIVE_drivehand_funcs;
#ifdef MPXPLAY_LINK_TCPIP
extern struct mpxplay_drivehand_func_s FTPDRIVE_drivehand_funcs;
#endif

static struct mpxplay_drivehand_func_s *all_drivehand_funcs[] = {
#ifdef MPXPLAY_LINK_TCPIP
	&FTPDRIVE_drivehand_funcs,
#endif
	&CDDRIVE_drivehand_funcs,
	&HDDRIVE_drivehand_funcs,	// put it last!
	NULL
};

extern unsigned int id3textconv;

struct mpxplay_drivehand_func_s *mpxplay_diskdrive_search_driver(char *pathname)
{
	struct mpxplay_drivehand_func_s *mdfs, **mdfp = &all_drivehand_funcs[0];

	if(!pathname || !pathname[0])
		return NULL;

/*#ifdef MPXPLAY_LINK_DLLLOAD
 {
  mpxplay_module_entry_s *dll_decoder=NULL;
  do{
   dll_decoder=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_DRIVEHAND,0,NULL,dll_decoder);
   if(dll_decoder){
    if(dll_decoder->module_structure_version==MPXPLAY_DLLMODULEVER_DRIVEHAND){ // !!!
     mdfs=(struct mpxplay_drivehand_func_s *)dll_decoder->module_callpoint;
     if(mdfs && mdfs->drive_name_check && mdfs->drive_name_check(pathname))
      return mdfs;
    }
   }
  }while(dll_decoder);
 }
#endif*/

	mdfs = *mdfp;
	do {
		if(mdfs->drive_name_check && mdfs->drive_name_check(pathname))
			return mdfs;
		mdfp++;
		mdfs = *mdfp;
	} while(mdfs);
	return NULL;
}

void mpxplay_diskdrive_alldrives_close(void)
{
	struct mpxplay_drivehand_func_s *mdfs, **mdfp = &all_drivehand_funcs[0];
	mdfs = *mdfp;
	do {
		if(mdfs->drive_unmount)
			mdfs->drive_unmount(NULL);
		mdfp++;
		mdfs = *mdfp;
	} while(mdfs);
}

long mpxplay_diskdrive_drive_config(struct mpxplay_diskdrive_data_s *mdds, unsigned int funcnum, void *argp1, void *argp2)
{
	if(!mdds || !mdds->mdfs)
		return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;
	if(!mdds->mdfs->drive_config)
		return MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC;
	return mdds->mdfs->drive_config(mdds->drive_data, funcnum, argp1, argp2);	// !!! drive data may be NULL (pre-config)
}

unsigned int mpxplay_diskdrive_drive_mount(struct mpxplay_diskdrive_data_s *mdds, char *pathname)
{
	long utftype;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->drive_mount || !pathname || !pathname[0])
		return 0;
	mdds->drive_data = mdds->mdfs->drive_mount(pathname);
	if(!mdds->drive_data)
		return 0;
	// win32 based server also says "UNIX" type ...
	//if(mpxplay_diskdrive_drive_config(mdds,MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISFILESYSUNX,NULL,NULL)>0)
	// funcbit_smp_enable(mdds->infobits,MPXPLAY_DISKDRIVEDATA_INFOBIT_SYSUNIX);
	utftype = mpxplay_diskdrive_drive_config(mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_UTFTYPE, NULL, NULL);
	if(utftype > 0)
		mdds->utftype = utftype;
	return 1;
}

void mpxplay_diskdrive_drive_unmount(struct mpxplay_diskdrive_data_s *mdds)
{
	if(!mdds || !mdds->mdfs)
		return;
	if(mdds->mdfs->drive_unmount && mdds->drive_data)
		mdds->mdfs->drive_unmount(mdds->drive_data);
	mdds->drive_data = NULL;
	mdds->mdfs = NULL;
}

unsigned int mpxplay_diskdrive_findfirst(struct mpxplay_diskdrive_data_s *mdds, char *pathname, unsigned int attrib, struct pds_find_t *ffblk)
{
	if(!mdds || !mdds->mdfs || !mdds->mdfs->findfirst || !mdds->drive_data || !pathname || !pathname[0] || !ffblk)
		return 1;
	pds_memset(ffblk, 0, sizeof(*ffblk));
	return mdds->mdfs->findfirst(mdds->drive_data, pathname, attrib, ffblk);
}

unsigned int mpxplay_diskdrive_findnext(struct mpxplay_diskdrive_data_s *mdds, struct pds_find_t *ffblk)
{
	if(!mdds || !mdds->mdfs || !mdds->mdfs->findnext || !mdds->drive_data || !ffblk)
		return 1;
	return mdds->mdfs->findnext(mdds->drive_data, ffblk);
}

void mpxplay_diskdrive_findclose(struct mpxplay_diskdrive_data_s *mdds, struct pds_find_t *ffblk)
{
	if(!mdds || !mdds->mdfs || !mdds->mdfs->findclose || !mdds->drive_data || !ffblk)
		return;
	mdds->mdfs->findclose(mdds->drive_data, ffblk);
}

unsigned int mpxplay_diskdrive_checkdir(struct mpxplay_diskdrive_data_s *mdds, char *dirname)
{
	long retcode = 0;
	struct pds_find_t ffblk;
	char searchname[MAX_PATHNAMELEN];

	if(!dirname || !dirname[0])
		return retcode;

	if(mdds) {
		retcode = mpxplay_diskdrive_drive_config(mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIREXISTS, dirname, NULL);
		if(retcode >= 0)
			return retcode;
		retcode = 0;
	}

	if(mpxplay_diskdrive_findfirst(mdds, dirname, (_A_NORMAL | _A_SUBDIR), &ffblk) == 0) {	// subdir
		if(ffblk.attrib & _A_SUBDIR)
			retcode = 1;
		mpxplay_diskdrive_findclose(mdds, &ffblk);
	} else {					// for rootdir
		pds_filename_assemble_fullname(searchname, dirname, PDS_DIRECTORY_ALLFILE_STR);
		if(mpxplay_diskdrive_findfirst(mdds, searchname, (_A_NORMAL | _A_SUBDIR), &ffblk) == 0) {
			retcode = 1;
			mpxplay_diskdrive_findclose(mdds, &ffblk);
		} else {
			if(pds_access(dirname, F_OK) == 0)
				retcode = 1;
		}
	}
	return retcode;
}

unsigned int mpxplay_diskdrive_isdirroot(struct mpxplay_diskdrive_data_s *mdds, char *dirname)
{
	long retcode = 0;
	unsigned long len;

	if(!dirname || !dirname[0])
		return retcode;

	if(mdds) {
		retcode = mpxplay_diskdrive_drive_config(mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIRROOT, dirname, NULL);
		if(retcode >= 0)
			return retcode;
		retcode = 0;
	}

	len = pds_strlen(dirname);
	if(len < sizeof(PDS_DIRECTORY_ROOTDIR_STR))	// we assume this is the root directory (less than "c:\")
		retcode = 1;
	return retcode;
}

char *mpxplay_diskdrive_getcwd(struct mpxplay_diskdrive_data_s *mdds, char *buf, unsigned int buflen)
{
	char *retptr = NULL;
	buf[0] = 0;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->getcwd || !mdds->drive_data || !buf || !buflen)
		return retptr;
	retptr = mdds->mdfs->getcwd(mdds->drive_data, buf, buflen);
	if(retptr && buf[0])
		pds_strcpy(mdds->lastdir, buf);
	return retptr;
}

int mpxplay_diskdrive_chdir(struct mpxplay_diskdrive_data_s *mdds, char *path)
{
	int retcode = -1;
	unsigned int len;
	char strtmp[MAX_PATHNAMELEN];
	if(!mdds || !mdds->mdfs || !mdds->mdfs->chdir || !mdds->drive_data || !path || !path[0])
		return retcode;
	pds_strcpy(strtmp, path);
	len = pds_filename_remove_relatives(strtmp);
	if(!mpxplay_diskdrive_isdirroot(mdds, strtmp))
		if(strtmp[len - 1] == PDS_DIRECTORY_SEPARATOR_CHAR)
			strtmp[len - 1] = 0;
	retcode = mdds->mdfs->chdir(mdds->drive_data, strtmp);
	if(retcode >= 0)
		pds_strcpy(mdds->lastdir, strtmp);
	return retcode;
}

int mpxplay_diskdrive_mkdir(struct mpxplay_diskdrive_data_s *mdds, char *path)
{
	char stmp[MAX_PATHNAMELEN * 3];
	if(!mdds || !mdds->mdfs || !mdds->mdfs->mkdir || !mdds->drive_data || !path || !path[0])
		return MPXPLAY_ERROR_FILEHAND_CANTCREATE;
	if(mdds->utftype) {
		mpxplay_playlist_textconv_selected_back(stmp, sizeof(stmp), path, mdds->utftype);
		path = &stmp[0];
	}
	if(mdds->mdfs->mkdir(mdds->drive_data, path) < 0)
		return MPXPLAY_ERROR_FILEHAND_CANTCREATE;
	return MPXPLAY_ERROR_FILEHAND_OK;
}

int mpxplay_diskdrive_rmdir(struct mpxplay_diskdrive_data_s *mdds, char *path)
{
	if(!mdds || !mdds->mdfs || !mdds->mdfs->rmdir || !mdds->drive_data || !path || !path[0])
		return MPXPLAY_ERROR_FILEHAND_REMOVEDIR;
	if(mdds->mdfs->rmdir(mdds->drive_data, path) < 0)
		return MPXPLAY_ERROR_FILEHAND_REMOVEDIR;
	return MPXPLAY_ERROR_FILEHAND_OK;
}

int mpxplay_diskdrive_rename(struct mpxplay_diskdrive_data_s *mdds, char *oldfilename, char *newfilename)
{
	char stmp_oldname[MAX_PATHNAMELEN * 3], stmp_newname[MAX_PATHNAMELEN * 3];
	if(!mdds || !mdds->mdfs || !mdds->mdfs->rename || !mdds->drive_data || !oldfilename || !oldfilename[0] || !newfilename || !newfilename[0])
		return MPXPLAY_ERROR_FILEHAND_RENAME;
	if(mdds->utftype) {
		mpxplay_playlist_textconv_selected_back(stmp_oldname, sizeof(stmp_oldname), oldfilename, mdds->utftype);
		oldfilename = &stmp_oldname[0];
		mpxplay_playlist_textconv_selected_back(stmp_newname, sizeof(stmp_newname), newfilename, mdds->utftype);
		newfilename = &stmp_newname[0];
	}
	if(mdds->mdfs->rename(mdds->drive_data, oldfilename, newfilename) < 0)
		return MPXPLAY_ERROR_FILEHAND_RENAME;
	return MPXPLAY_ERROR_FILEHAND_OK;
}

int mpxplay_diskdrive_unlink(struct mpxplay_diskdrive_data_s *mdds, char *filename)
{
	if(!mdds || !mdds->mdfs || !mdds->mdfs->unlink || !mdds->drive_data || !filename || !filename[0])
		return MPXPLAY_ERROR_FILEHAND_DELETE;
	if(mdds->mdfs->unlink(mdds->drive_data, filename) < 0)
		return MPXPLAY_ERROR_FILEHAND_DELETE;
	return MPXPLAY_ERROR_FILEHAND_OK;
}

//----------------------------------------------------------------------------
typedef struct mpxplay_diskfile_data_s {
	struct mpxplay_diskdrive_data_s *mdds;
	void *filehand_datas;
} mpxplay_diskfile_data_s;

static struct mpxplay_drivehand_func_s *mpxplay_diskdrive_filehandler_search(char *filename)
{
	struct mpxplay_drivehand_func_s *mdfs, **mdfp = &all_drivehand_funcs[0];

	if(!filename || !filename[0])
		return NULL;

	mdfs = *mdfp;
	do {
		if(mdfs->file_name_check && mdfs->file_name_check(NULL, filename))
			return mdfs;
		mdfp++;
		mdfs = *mdfp;
	} while(mdfs);
	return NULL;
}

static struct mpxplay_diskdrive_data_s *mpxplay_diskdrive_filehandler_alloc(char *filename)
{
	struct mpxplay_drivehand_func_s *mdfs = NULL;
	struct mpxplay_diskdrive_data_s *mdds_h = NULL, *mdds_a;

	if(!filename || !filename[0])
		return NULL;

	mdds_h = playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(filename));
	if(!mdds_h) {
		mdfs = mpxplay_diskdrive_filehandler_search(filename);
		if(!mdfs)
			return NULL;
	}
	mdds_a = calloc(1, sizeof(*mdds_a));
	if(!mdds_a)
		return NULL;
	if(mdds_h)
		pds_memcpy(mdds_a, mdds_h, sizeof(*mdds_a));
	else
		mdds_a->mdfs = mdfs;
	return mdds_a;
}

static void mpxplay_diskdrive_filehandler_free(struct mpxplay_diskdrive_data_s *mdds)
{
	if(mdds)
		free(mdds);
}

void *mpxplay_diskdrive_file_open(struct mpxplay_diskdrive_data_s *mdds, char *filename, unsigned long openmode)
{
	struct mpxplay_diskfile_data_s *mdfd;
	char stmp[MAX_PATHNAMELEN * 3];

	mdfd = calloc(1, sizeof(*mdfd));
	if(!mdfd)
		return mdfd;

	if(!mdds) {
		mdds = mpxplay_diskdrive_filehandler_alloc(filename);
		if(!mdds)
			goto err_out_open;
		funcbit_enable(mdds->infobits, MPXPLAY_DISKDRIVEDATA_INFOBIT_INTMDDS);
	}
	if(mdds->utftype) {
		mpxplay_playlist_textconv_selected_back(stmp, sizeof(stmp), filename, mdds->utftype);
		filename = &stmp[0];
	}
	mdfd->mdds = mdds;
	mdfd->filehand_datas = mdds->mdfs->file_open(mdds->drive_data, filename, openmode);
	if(!mdfd->filehand_datas)
		goto err_out_open;
	return mdfd;

  err_out_open:
	mpxplay_diskdrive_file_close(mdfd);
	return NULL;
}

struct mpxplay_diskdrive_data_s *mpxplay_diskdrive_file_get_mdds(void *mdfp)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	if(!mdfd)
		return NULL;
	return mdfd->mdds;
}

long mpxplay_diskdrive_file_config(void *mdfp, unsigned int funcnum, void *argp1, void *argp2)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	if(!mdfd)
		return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs)
		return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;
	if(!mdds->mdfs->drive_config)
		return MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC;
	return mdds->mdfs->drive_config(mdfd->filehand_datas, funcnum, argp1, argp2);	// !!! file-data may be NULL (pre-config)
}

void mpxplay_diskdrive_file_close(void *mdfp)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	if(mdfd) {
		struct mpxplay_diskdrive_data_s *mdds = mdfd->mdds;
		if(mdds && mdds->mdfs && mdds->mdfs->file_close && mdfd->filehand_datas)
			mdds->mdfs->file_close(mdfd->filehand_datas);
		if(funcbit_test(mdds->infobits, MPXPLAY_DISKDRIVEDATA_INFOBIT_INTMDDS))
			mpxplay_diskdrive_filehandler_free(mdds);
		free(mdfd);
	}
}

long mpxplay_diskdrive_file_read(void *mdfp, char *buf, unsigned int len)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	long bytes;
	if(!mdfd || !buf || !len)
		return 0;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->file_read || !mdfd->filehand_datas)
		return 0;
	bytes = mdds->mdfs->file_read(mdfd->filehand_datas, buf, len);
	if(bytes < 0)
		bytes = 0;
	return bytes;
}

long mpxplay_diskdrive_file_write(void *mdfp, char *buf, unsigned int len)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	if(!mdfd || !buf || !len)
		return 0;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->file_write || !mdfd->filehand_datas)
		return 0;
	return mdds->mdfs->file_write(mdfd->filehand_datas, buf, len);
}

mpxp_filesize_t mpxplay_diskdrive_file_seek(void *mdfp, mpxp_filesize_t offset, int fromwhere)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	if(!mdfd)
		return MPXPLAY_ERROR_MPXINBUF_SEEK_LOW;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->file_seek || !mdfd->filehand_datas)
		return MPXPLAY_ERROR_MPXINBUF_SEEK_LOW;
	return mdds->mdfs->file_seek(mdfd->filehand_datas, offset, fromwhere);
}

mpxp_filesize_t mpxplay_diskdrive_file_tell(void *mdfp)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	if(!mdfd)
		return 0;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->file_tell || !mdfd->filehand_datas)
		return 0;
	return mdds->mdfs->file_tell(mdfd->filehand_datas);
}

mpxp_filesize_t mpxplay_diskdrive_file_length(void *mdfp)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	if(!mdfd)
		return 0;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->file_length || !mdfd->filehand_datas)
		return 0;
	return mdds->mdfs->file_length(mdfd->filehand_datas);
}

int mpxplay_diskdrive_file_eof(void *mdfp)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	if(!mdfd)
		return 1;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->file_eof || !mdfd->filehand_datas)
		return 1;
	return mdds->mdfs->file_eof(mdfd->filehand_datas);
}

int mpxplay_diskdrive_file_chsize(void *mdfp, mpxp_filesize_t offset)
{
	struct mpxplay_diskfile_data_s *mdfd = mdfp;
	struct mpxplay_diskdrive_data_s *mdds;
	if(!mdfd)
		return 0;
	mdds = mdfd->mdds;
	if(!mdds || !mdds->mdfs || !mdds->mdfs->file_chsize || !mdfd->filehand_datas)
		return 0;
	return mdds->mdfs->file_chsize(mdfd->filehand_datas, offset);
}

//--------------------------------------------------------------------------

#define MAX_DIRECTORY_DEPTH  32
#define DIRSCAN_LOCFLAG_SCANSUBDIRS  32	// else scan one directory only
#define DIRSCAN_LOCFLAG_LOADALLDIRS  64	// else load files from the deepest level
#define DIRSCAN_LOCFLAG_NEXTDIR      128

unsigned int mpxplay_diskdrive_subdirscan_open(struct mpxplay_diskdrive_data_s *mdds, char *path_and_filename, unsigned int attrib, struct pds_subdirscan_t *dsi)
{
	unsigned int i;
	mpxp_char_t *sd;
	char currdir[MAX_PATHNAMELEN];

	pds_memset(dsi, 0, sizeof(*dsi));

	dsi->ff = calloc(1, sizeof(struct pds_find_t));
	if(!dsi->ff)
		goto err_out_open;
	dsi->subdir_ff_datas = calloc(MAX_DIRECTORY_DEPTH, sizeof(struct pds_find_t));
	if(!dsi->subdir_ff_datas)
		goto err_out_open;
	dsi->subdir_names = calloc(MAX_DIRECTORY_DEPTH, sizeof(mpxp_char_t *));
	if(!dsi->subdir_names)
		goto err_out_open;
	for(i = 0; i < MAX_DIRECTORY_DEPTH; i++) {
		dsi->subdir_names[i] = calloc(300, sizeof(mpxp_char_t));
		if(!dsi->subdir_names[i])
			goto err_out_open;
	}
	dsi->subdir_masks = calloc(MAX_DIRECTORY_DEPTH, sizeof(mpxp_char_t *));
	if(!dsi->subdir_masks)
		goto err_out_open;

	dsi->subdir_level = 0;
	dsi->subdir_reach = -1;
	dsi->subfile_reach = -1;

	dsi->scan_attrib = attrib;
	pds_strcpy(dsi->subdirmasks, path_and_filename);
	pds_strcpy(dsi->startdir, path_and_filename);

	sd = &dsi->subdirmasks[0];
	if(pds_getdrivenum_from_path(sd) >= 0)
		sd += 2;
	if(sd[0] == PDS_DIRECTORY_SEPARATOR_CHAR)
		sd++;

	do {
		mpxp_char_t *p = pds_strchr(sd, PDS_DIRECTORY_SEPARATOR_CHAR);
		if(p) {
			unsigned int len = p - sd;
			if(len && (dsi->nb_subdirmasks || pds_strnchr(sd, '*', len) || pds_strnchr(sd, '?', len))) {
				if(!dsi->nb_subdirmasks) {	// first '\\'
					len = sd - (&dsi->subdirmasks[0]);
					dsi->startdir[len] = 0;	// end of startdir
					if(len >= (sizeof(PDS_DIRECTORY_ROOTDIR_STR)) && (dsi->startdir[len - 1] == PDS_DIRECTORY_SEPARATOR_CHAR))
						dsi->startdir[len - 1] = 0;
				}
				dsi->subdir_masks[dsi->nb_subdirmasks++] = sd;	// begin of subdirmask
				*p = 0;			// end
				funcbit_enable(dsi->flags, DIRSCAN_LOCFLAG_NEXTDIR);
				funcbit_enable(dsi->flags, DIRSCAN_LOCFLAG_SCANSUBDIRS);
			}
			sd = p + 1;
		} else {
			pds_strcpy(dsi->scan_names, sd);
			if(!dsi->nb_subdirmasks) {
				if(sd > (&dsi->subdirmasks[0]))
					pds_getpath_from_fullname(dsi->startdir, dsi->startdir);
				else
					mpxplay_diskdrive_getcwd(mdds, dsi->startdir, sizeof(dsi->startdir));	// no path, only filename or mask
			}
			break;
		}
	} while(1);

	if(dsi->nb_subdirmasks && pds_strcmp(dsi->subdir_masks[dsi->nb_subdirmasks - 1], "*.*") == 0) {
		funcbit_enable(dsi->flags, (DIRSCAN_LOCFLAG_SCANSUBDIRS | DIRSCAN_LOCFLAG_LOADALLDIRS));
		funcbit_disable(dsi->flags, DIRSCAN_LOCFLAG_NEXTDIR);
	}

	if(!pds_filename_check_absolutepath(dsi->startdir)) {
		mpxplay_diskdrive_getcwd(mdds, currdir, sizeof(currdir));
		pds_filename_build_fullpath(dsi->currdir, currdir, dsi->startdir);
		pds_strcpy(dsi->startdir, dsi->currdir);
	} else
		pds_strcpy(dsi->currdir, dsi->startdir);

	return 0;

  err_out_open:
	mpxplay_diskdrive_subdirscan_close(mdds, dsi);
	return 1;
}

static void subdirscan_rebuild_currdir(struct pds_subdirscan_t *dsi)
{
	unsigned int i, len;

	pds_strcpy(dsi->prevdir, dsi->currdir);
	len = pds_strcpy(dsi->currdir, dsi->startdir);

	for(i = 0; i < dsi->subdir_level; i++) {
		if(dsi->currdir[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR)
			len += pds_strcpy(&dsi->currdir[len], PDS_DIRECTORY_SEPARATOR_STR);
		len += pds_strcpy(&dsi->currdir[len], dsi->subdir_names[i]);
	}
}

int mpxplay_diskdrive_subdirscan_findnextfile(struct mpxplay_diskdrive_data_s *mdds, struct pds_subdirscan_t *dsi)
{
	unsigned int len, fferror;
	char searchfilename[300], searchdirname[300];

	if(!dsi->ff || !dsi->subdir_ff_datas || !dsi->subdir_names)
		return -1;

	funcbit_disable(dsi->flags, (SUBDIRSCAN_FLAG_SUBDIR | SUBDIRSCAN_FLAG_UPDIR));

	if(dsi->flags & DIRSCAN_LOCFLAG_NEXTDIR) {	// skip to next directory
		struct pds_find_t *ffd = &dsi->subdir_ff_datas[dsi->subdir_level];
		if(dsi->subdir_reach < dsi->subdir_level) {	// we were not here yet (in this directory), begin a new dirname search
			dsi->subdir_reach = dsi->subdir_level;
			len = pds_strcpy(searchdirname, dsi->currdir);
			if(searchdirname[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR)
				len += pds_strcpy(&searchdirname[len], PDS_DIRECTORY_SEPARATOR_STR);
			fferror = 0;
			if(dsi->subdir_level < dsi->nb_subdirmasks)
				pds_strcpy(&searchdirname[len], dsi->subdir_masks[dsi->subdir_level]);
			else if(funcbit_test(dsi->flags, DIRSCAN_LOCFLAG_LOADALLDIRS))
				pds_strcpy(&searchdirname[len], "*.*");
			else
				fferror = 1;
			if(!fferror) {
				fferror = mpxplay_diskdrive_findfirst(mdds, searchdirname, _A_SUBDIR, ffd);
				while((!(ffd->attrib & _A_SUBDIR)
					   || (pds_strcmp("..", ffd->name) == 0)
					   || (pds_strcmp(".", ffd->name) == 0))
					  && !fferror) {
					fferror = mpxplay_diskdrive_findnext(mdds, ffd);
				}
			}
		} else {				// continue a dirname search
			do {
				fferror = mpxplay_diskdrive_findnext(mdds, ffd);
			} while((!(ffd->attrib & _A_SUBDIR)
					 || (pds_strcmp("..", ffd->name) == 0)
					 || (pds_strcmp(".", ffd->name) == 0))
					&& !fferror);
		}
		if(!fferror) {			// go deeper
			if(dsi->subdir_level < (MAX_DIRECTORY_DEPTH - 1)) {
				unsigned long fileload_boundary;
				pds_strcpy(dsi->subdir_names[dsi->subdir_level++], ffd->name);
				subdirscan_rebuild_currdir(dsi);
				fileload_boundary = dsi->nb_subdirmasks;
				if(funcbit_test(dsi->flags, DIRSCAN_LOCFLAG_LOADALLDIRS))
					fileload_boundary--;
				if(dsi->subdir_level >= fileload_boundary) {
					funcbit_disable(dsi->flags, DIRSCAN_LOCFLAG_NEXTDIR);	// read filenames from next dir
					funcbit_enable(dsi->flags, SUBDIRSCAN_FLAG_SUBDIR);
				}
			}
			return 1;
		} else {
			mpxplay_diskdrive_findclose(mdds, ffd);
			if(dsi->subdir_level > 0) {	// go higher
				dsi->subdir_level--;
				dsi->subdir_reach--;
				dsi->subfile_reach = dsi->subdir_reach;
				subdirscan_rebuild_currdir(dsi);
				funcbit_enable(dsi->flags, (DIRSCAN_LOCFLAG_NEXTDIR | SUBDIRSCAN_FLAG_UPDIR));
				return 1;
			} else
				return -1;
		}
	}
	// read filenames from the directory
	if(dsi->subfile_reach < dsi->subdir_level) {
		dsi->subfile_reach = dsi->subdir_level;
		len = pds_strcpy(searchfilename, dsi->currdir);
		if(searchfilename[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR)
			len += pds_strcpy(&searchfilename[len], PDS_DIRECTORY_SEPARATOR_STR);
		pds_strcpy(&searchfilename[len], dsi->scan_names);
		fferror = mpxplay_diskdrive_findfirst(mdds, searchfilename, dsi->scan_attrib, dsi->ff);
	} else
		fferror = mpxplay_diskdrive_findnext(mdds, dsi->ff);

	if(fferror)
		goto err_out_nextdir;

	if(dsi->ff->attrib & _A_SUBDIR)
		if(pds_strcmp(dsi->ff->name, "..") == 0 || pds_strcmp(dsi->ff->name, ".") == 0)
			return 1;
	//if(dsi->scan_attrib && !(dsi->ff->attrib&dsi->scan_attrib))
	// return 1;

	len = pds_strcpy(dsi->fullname, dsi->currdir);
	if(dsi->fullname[len - 1] != PDS_DIRECTORY_SEPARATOR_CHAR)
		len += pds_strcpy(&dsi->fullname[len], PDS_DIRECTORY_SEPARATOR_STR);
	pds_strcpy(&dsi->fullname[len], dsi->ff->name);

	return 0;

  err_out_nextdir:
	mpxplay_diskdrive_findclose(mdds, dsi->ff);
	if(!funcbit_test(dsi->flags, DIRSCAN_LOCFLAG_SCANSUBDIRS))
		return -1;
	funcbit_enable(dsi->flags, DIRSCAN_LOCFLAG_NEXTDIR);
	return 1;
}

void mpxplay_diskdrive_subdirscan_close(struct mpxplay_diskdrive_data_s *mdds, struct pds_subdirscan_t *dsi)
{
	unsigned int i;
	if(dsi->ff) {
		free(dsi->ff);
		dsi->ff = NULL;
	}
	if(dsi->subdir_ff_datas) {
		for(i = 0; i <= dsi->subdir_level; i++)
			mpxplay_diskdrive_findclose(mdds, &dsi->subdir_ff_datas[i]);
		free(dsi->subdir_ff_datas);
		dsi->subdir_ff_datas = NULL;
	}
	if(dsi->subdir_names) {
		for(i = 0; i < MAX_DIRECTORY_DEPTH; i++)
			if(dsi->subdir_names[i])
				free(dsi->subdir_names[i]);
		free(dsi->subdir_names);
		dsi->subdir_names = NULL;
	}
	if(dsi->subdir_masks) {
		free(dsi->subdir_masks);
		dsi->subdir_masks = NULL;
	}
}

//------------------------------------------------------------------------
//buffered file handling (usually for playlist files)

#define MPXPLAY_DISKDRIVE_TEXTFILE_BUFSIZE 16384
#define MPXPLAY_DISKDRIVE_TEXTFILE_KBHITCHK_LINECOUNT 1023

#define MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_FILEBEGIN  1
#define MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_SEARCHSTRZ 2	// search for trailing zero (instead of eol) at O_BINARY too

#define MPXPLAY_DISKDRIVE_TEXTFILE_USEWRITEBUF 1

typedef struct diskdrive_textfile_s {
	struct mpxplay_diskfile_data_s *filehand;
	char *buffer;
	char *bufptr;
	char *bufend;
	unsigned int openmode;
	unsigned int utftype_src;
	unsigned int utftype_dest;
	mpxp_uint32_t flags;
	unsigned long linecount;
	struct mpxplay_diskdrive_data_s *mdds;
} diskdrive_textfile_s;

void *mpxplay_diskdrive_textfile_open(struct mpxplay_diskdrive_data_s *mdds, char *filename, unsigned int openmode)
{
	struct diskdrive_textfile_s *fp;

	fp = (struct diskdrive_textfile_s *)calloc(1, sizeof(*fp));
	if(!fp)
		return fp;
	fp->buffer = (char *)malloc(MPXPLAY_DISKDRIVE_TEXTFILE_BUFSIZE + 16);	// where is the overflow?
	if(!fp->buffer)
		goto err_out_opent;
	fp->bufptr = fp->bufend = fp->buffer;

	if(funcbit_test(openmode, O_TEXT))
		funcbit_enable(fp->flags, MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_SEARCHSTRZ);

	funcbit_disable(openmode, O_TEXT);	// !!! we handle files in non-text mode (we convert the cr/lf) (ftp ascii mode problem: it't doesn't convert lf to cr/lf)
	funcbit_enable(fp->flags, MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_FILEBEGIN);

	fp->filehand = mpxplay_diskdrive_file_open(mdds, filename, (openmode | O_BINARY));	// !!!
	if(!fp->filehand)
		goto err_out_opent;
	fp->openmode = openmode;
	fp->mdds = fp->filehand->mdds;
	if(fp->mdds)				//
		fp->utftype_dest = fp->mdds->utftype;	// ???
	else
		fp->utftype_dest = MPXPLAY_DISKDRIVEDATA_UTFTYPE_INVALID;

	if((id3textconv & ID3TEXTCONV_UTF_AUTO) && !(openmode & (O_WRONLY | O_RDWR | O_APPEND | O_CREAT | O_TRUNC | O_BINARY))) {	// !!! bullshit
		unsigned long readbytes;
		readbytes = mpxplay_diskdrive_file_read(fp->filehand, fp->buffer, 2);
		if(!readbytes)
			goto err_out_opent;
		if((fp->buffer[0] == 0xff) && (fp->buffer[1] == 0xfe))
			fp->utftype_src = MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE;
		else if((fp->buffer[0] == 0xfe) && (fp->buffer[1] == 0xff))
			fp->utftype_src = MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16BE;
		else if((fp->buffer[0] == 0xef) && (fp->buffer[1] == 0xbb)) {
			readbytes += mpxplay_diskdrive_file_read(fp->filehand, (fp->buffer + 2), 1);
			if(readbytes < 3)
				goto err_out_opent;
			if(fp->buffer[2] == 0xbf)
				fp->utftype_src = MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8;
			else
				fp->bufend = fp->buffer + readbytes;
		} else
			fp->bufend = fp->buffer + readbytes;
	}

	return fp;

  err_out_opent:
	mpxplay_diskdrive_textfile_close(fp);
	return NULL;
}

void mpxplay_diskdrive_textfile_close(void *fhp)
{
	struct diskdrive_textfile_s *fp = fhp;
	if(fp) {
#ifdef MPXPLAY_DISKDRIVE_TEXTFILE_USEWRITEBUF
		if(fp->openmode & (O_WRONLY | O_RDWR | O_APPEND) && fp->buffer && fp->filehand) {
			unsigned long bytes = fp->bufptr - fp->buffer;
			if(bytes && (bytes <= MPXPLAY_DISKDRIVE_TEXTFILE_BUFSIZE))
				mpxplay_diskdrive_file_write(fp->filehand, fp->buffer, bytes);
		}
#endif
		if(fp->filehand)
			mpxplay_diskdrive_file_close(fp->filehand);
		if(fp->buffer)
			free(fp->buffer);
		free(fp);
	}
}

long mpxplay_diskdrive_textfile_config(void *fhp, unsigned int funcnum, void *argp1, void *argp2)
{
	struct diskdrive_textfile_s *fp = fhp;
	if(!fp || !fp->filehand)
		return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;
	switch (funcnum) {
	case MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_SRC:
		return fp->utftype_src;
	case MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_DEST:
		return fp->utftype_dest;
	case MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_TEXTCODETYPE_SRC:
		if(!argp1)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		fp->utftype_src = *((unsigned long *)argp1);
		return MPXPLAY_DISKDRIV_CFGERROR_SET_OK;
	case MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_TEXTCODETYPE_DEST:
		if(funcbit_test(fp->flags, MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_FILEBEGIN)) {
			unsigned int len = 0;
			unsigned long newutftype;
			char str[4];
			funcbit_disable(fp->flags, MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_FILEBEGIN);
			if(argp1) {
				newutftype = *((unsigned long *)argp1);
				if((fp->utftype_dest == MPXPLAY_DISKDRIVEDATA_UTFTYPE_INVALID) || (newutftype > MPXPLAY_DISKDRIVEDATA_UTFTYPE_NONE))
					fp->utftype_dest = newutftype;
			}
			switch (fp->utftype_dest) {
			case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE:
				str[0] = 0xff;
				str[1] = 0xfe;
				len = 2;
				break;
			case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16BE:
				str[0] = 0xfe;
				str[1] = 0xff;
				len = 2;
				break;
			case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8:
				str[0] = 0xef;
				str[1] = 0xbb;
				str[2] = 0xbf;
				len = 3;
				break;
			default:
				return MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC;
			}
			if(len)
				if(mpxplay_diskdrive_file_write(fp->filehand, str, len) != len)
					return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;
		}
		return MPXPLAY_DISKDRIV_CFGERROR_SET_OK;
	case MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_SEARCHSTRZ_ENABLE:
		funcbit_enable(fp->flags, MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_SEARCHSTRZ);
		return MPXPLAY_DISKDRIV_CFGERROR_SET_OK;
	case MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_SEARCHSTRZ_DISABLE:
		funcbit_disable(fp->flags, MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_SEARCHSTRZ);
		return MPXPLAY_DISKDRIV_CFGERROR_SET_OK;
	}
	return MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC;
}

unsigned long mpxplay_diskdrive_textfile_readline(void *fhp, char *readbuf, unsigned long rbuflen)
{
	struct diskdrive_textfile_s *fp = fhp;
	unsigned long textbytes = 0;

	if(!fp)
		return textbytes;
	if(!fp->filehand)
		return textbytes;
	if(!(fp->linecount & MPXPLAY_DISKDRIVE_TEXTFILE_KBHITCHK_LINECOUNT))
		if(pds_look_extgetch() == KEY_ESC) {
			pds_extgetch();
			return textbytes;
		}
	fp->linecount++;

	do {
		if(fp->bufptr >= fp->bufend) {
			unsigned long readbytes;
			readbytes = mpxplay_diskdrive_file_read(fp->filehand, fp->buffer, MPXPLAY_DISKDRIVE_TEXTFILE_BUFSIZE);
			if(!readbytes)
				break;
			switch (fp->utftype_src) {
			case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE:
				readbytes = mpxplay_playlist_textconv_funcs.utf16LE_to_char(fp->buffer, readbytes);
				break;
			case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16BE:
				readbytes = mpxplay_playlist_textconv_funcs.utf16BE_to_char(fp->buffer, readbytes);
				break;
			case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8:
				readbytes = mpxplay_playlist_textconv_funcs.utf8_to_char(fp->buffer, readbytes);
				break;
			}
			fp->bufptr = fp->buffer;
			fp->bufend = fp->buffer + readbytes;
		}
		if(!funcbit_test(fp->openmode, O_BINARY)) {
			if(fp->bufptr[0] == '\r') {	// !!! skips '\r'
				fp->bufptr++;
				continue;
			}
			if(fp->bufptr[0] == '\n') {	// !!! removes '\n'
				fp->bufptr++;
				break;
			}
		}
		if(funcbit_test(fp->flags, MPXPLAY_DISKDRIVE_TEXTFILE_FLAG_SEARCHSTRZ)) {
			if(!fp->bufptr[0]) {
				*readbuf = *(fp->bufptr++);	// close of string at O_BINARY
				if(textbytes)	// !!! we don't return if the length of string is zero
					break;
				continue;
			}
		}
		*readbuf = fp->bufptr[0];
		readbuf++;
		fp->bufptr++;
		textbytes++;
	} while(textbytes < rbuflen);
	if(!funcbit_test(fp->openmode, O_BINARY))
		*readbuf = 0;
	return textbytes;
}

long mpxplay_diskdrive_textfile_writeline(void *fhp, char *str)
{
	struct diskdrive_textfile_s *fp = fhp;
	unsigned long len, bytes = 0;
	char *locbuf, *utfbuf;

	if(!fp)
		return MPXPLAY_ERROR_FILEHAND_MEMORY;
	if(!fp->filehand)
		return MPXPLAY_ERROR_FILEHAND_MEMORY;
#ifndef MPXPLAY_DISKDRIVE_TEXTFILE_USEWRITEBUF
	fp->bufptr = fp->bufend = fp->buffer;	// clears readbuffer
#endif

	len = pds_strlen(str);
	if(!len)
		return bytes;
	if(!funcbit_test(fp->openmode, O_BINARY)) {
		locbuf = (char *)alloca(len + 4);
		if(!locbuf)
			return MPXPLAY_ERROR_FILEHAND_MEMORY;
		len = pds_strcpy(locbuf, str);
		if(locbuf[len - 1] == '\n')
			len--;
		if(!fp->mdds || !funcbit_test(fp->mdds->infobits, MPXPLAY_DISKDRIVEDATA_INFOBIT_SYSUNIX))
			locbuf[len++] = '\r';
		locbuf[len++] = '\n';
		locbuf[len] = 0;
		str = locbuf;
		if((fp->utftype_dest != fp->utftype_src) || (fp->utftype_dest & MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF_ALL)) {
			unsigned int buflen = 2 * len + 4;
			utfbuf = (char *)alloca(buflen);
			if(!utfbuf)
				return MPXPLAY_ERROR_FILEHAND_MEMORY;
			str = utfbuf;
			if(fp->utftype_dest == fp->utftype_src) {
				len = mpxplay_playlist_textconv_selected_back(utfbuf, buflen, locbuf, fp->utftype_dest);
			} else {
				switch (fp->utftype_dest) {
				case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE:
					len = mpxplay_playlist_textconv_funcs.char_to_utf16LE(utfbuf, locbuf, buflen);
					break;
				case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16BE:
					len = mpxplay_playlist_textconv_funcs.char_to_utf16BE(utfbuf, locbuf, buflen);
					break;
				case MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8:
					len = mpxplay_playlist_textconv_funcs.char_to_utf8(utfbuf, locbuf, buflen);
					break;
				default:
					len = mpxplay_playlist_textconv_selected_do(locbuf, len, fp->utftype_src, 0);
					str = locbuf;
					break;
				}
			}
		}
	}
#ifdef MPXPLAY_DISKDRIVE_TEXTFILE_USEWRITEBUF
	bytes = fp->bufptr - fp->buffer;
	if(bytes && ((bytes + len) >= MPXPLAY_DISKDRIVE_TEXTFILE_BUFSIZE)) {
		fp->bufptr = fp->buffer;
		if(mpxplay_diskdrive_file_write(fp->filehand, fp->buffer, bytes) != bytes)
			return MPXPLAY_ERROR_FILEHAND_CANTWRITE;
	}
	if(len >= MPXPLAY_DISKDRIVE_TEXTFILE_BUFSIZE)
		bytes = mpxplay_diskdrive_file_write(fp->filehand, str, len);
	else {
		pds_memcpy(fp->bufptr, str, len);
		fp->bufptr += len;
		bytes = len;
	}
#else
	bytes = mpxplay_diskdrive_file_write(fp->filehand, str, len);
#endif
	return bytes;
}

mpxp_filesize_t mpxplay_diskdrive_textfile_filelength(void *fhp)
{
	struct diskdrive_textfile_s *fp = fhp;
	if(!fp)
		return 0;
	return mpxplay_diskdrive_file_length(fp->filehand);
}
