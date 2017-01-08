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
//

#ifndef diskdriv_h
#define diskdriv_h

#include "in_file.h"

#define MPXPLAY_DISKDRIVEDATA_INFOBIT_INTMDDS 1	// internally allocated mdds
#define MPXPLAY_DISKDRIVEDATA_INFOBIT_SYSUNIX 2	// unix (remote) file system (for text files and file-pathes)

// should match with ID3TEXTCONV_ , later modify ID3TEXTCONV_ (add 16LE,16BE to API)
#define MPXPLAY_DISKDRIVEDATA_UTFTYPE_INVALID   0xffffffff
#define MPXPLAY_DISKDRIVEDATA_UTFTYPE_NONE      0
#define MPXPLAY_DISKDRIVEDATA_UTFTYPE_CODEPAGE  ID3TEXTCONV_CODEPAGE
#define MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8      ID3TEXTCONV_UTF8
#define MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE   ID3TEXTCONV_UTF16
#define MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16BE   (MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE|0x10000000)
#define MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF_ALL (MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8|MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16LE|MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF16BE)

typedef struct mpxplay_diskdrive_data_s {
	struct mpxplay_drivehand_func_s *mdfs;
	void *drive_data;
	void *file_data;
	mpxp_int32_t infobits;
	mpxp_uint32_t utftype;
	char lastdir[MAX_PATHNAMELEN];	// to save current (last) directory of the drive(s)
} mpxplay_diskdrive_data_s;

#define MPXPLAY_DRIVEHANDFUNC_INFOBIT_SLOWACCESS 1	// like ftp

//funcnums for drive_config()

//--- with drive_data (mdds)
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_ABOUT                   0	// open an "about" dialog
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GUICFG                  1	// open a gui config (not implemented)
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_CMD_RESETDRIVE       1000	// ie: clear dir cache
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_CMD_MEDIAEJECTLOAD   1001	// load/unload media
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISFILESYSUNX     2000	// is filesystem unix style?
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_UTFTYPE          2001	// filename text encoding
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_CHKBUFBLOCKBYTES 2010	// before open (>0:value,0:nobuff,<0:set by Mpxplay)
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_PREREADBUFBYTES  2012	// fills up the prebuffer before start playing after open (>0:value,0:nobuff,<0:set by Mpxplay)
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_PREREADBUFMSEC   2013	// same in msec (not implemented yet)
//argp1: char *dirname
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIREXISTS      2100
//argp1: char *dirname
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIRROOT        2101
//argp1: char *dest, argp2: unsigned long *maxdestlen
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVLETTERSTR     2200
//argp1: char *dest, argp2: unsigned long *maxdestlen
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVTYPENAME      2201
//argp1: char *dest, argp2: unsigned long *maxdestlen
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVOPENNAME      2202	// for ctrl-f
//argp1: char *dest, argp2: char *src
#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_REALLYFULLPATH   2300	// ie: ftp://user:pasw@server/dir/filename
//argp1: char *dest, argp2: unsigned long *maxdestlen
//#define MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ERRORLASTTEXT    2500

//--- with filehand_data (mdfs)
//argp1: unsigned long *blocksize
#define MPXPLAY_DISKFILE_CFGFUNCNUM_SET_FILEBLOCKSIZE   6000	// after open
//argp1: unsigned long *readwaitenable
#define MPXPLAY_DISKFILE_CFGFUNCNUM_SET_READWAIT        6010	// wait at read (1) or don't (0)

//--- textfile config
#define MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_SRC   8000
#define MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_GET_TEXTCODETYPE_DEST  8001
//argp1: unsigned long *textcodetype
#define MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_TEXTCODETYPE_SRC   8500
#define MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_TEXTCODETYPE_DEST  8501
#define MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_SEARCHSTRZ_ENABLE  8510
#define MPXPLAY_DISKTEXTFILE_CFGFUNCNUM_SET_SEARCHSTRZ_DISABLE 8511

//note: if a GET functions returns a number, it retuns that in the error code
#define MPXPLAY_DISKDRIV_CFGERROR_SET_OK           1
#define MPXPLAY_DISKDRIV_CFGERROR_NOBACKVALUE      0	//
#define MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE   -1
#define MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC      -2	// not supported function
#define MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING -3	// missing argument (for setting)

typedef struct mpxplay_drivehand_func_s {
	char *drivername;
	mpxp_int32_t infobits;
	long (*drive_config) (void *drive_data, unsigned long funcnum, void *argp1, void *argp2);	// pre-config without argument
	unsigned int (*drive_name_check) (char *pathname);	// check pathname, return true (1) if 'path' is supported by this drive-handler
	void *(*drive_mount) (char *pathname);	// returns drivehand_data
	void (*drive_unmount) (void *drivehand_data);
	unsigned int (*findfirst) (void *drivehand_data, char *pathname, unsigned int attrib, struct pds_find_t * ffblk);
	unsigned int (*findnext) (void *drivehand_data, struct pds_find_t * ffblk);
	void (*findclose) (void *drivehand_data, struct pds_find_t * ffblk);
	char *(*getcwd) (void *drivehand_data, char *buf, unsigned int buflen);
	int (*chdir) (void *drivehand_data, char *path);
	int (*mkdir) (void *drivehand_data, char *path);
	int (*rmdir) (void *drivehand_data, char *path);
	int (*rename) (void *drivehand_data, char *oldfilename, char *newfilename);
	int (*unlink) (void *drivehand_data, char *filename);
	void *r15;
	void *r16;
	void *r17;
	void *r18;
	void *r19;
	void *r20;

	unsigned int (*file_name_check) (void *drivehand_data, char *filename);
	void *(*file_open) (void *drivehand_data, char *filename, unsigned long openmode);	// returns filehand_data
	void (*file_close) (void *filehand_data);
	long (*file_read) (void *filehand_data, char *buf, unsigned int len);
	long (*file_write) (void *filehand_data, char *buf, unsigned int len);
	 mpxp_filesize_t(*file_seek) (void *filehand_data, mpxp_filesize_t offset, int fromwhere);
	 mpxp_filesize_t(*file_tell) (void *filehand_data);
	 mpxp_filesize_t(*file_length) (void *filehand_data);
	int (*file_eof) (void *filehand_data);
	int (*file_chsize) (void *filehand_data, mpxp_filesize_t offset);
	void *r31;
} mpxplay_drivehand_func_s;		// 32*4=128 bytes

extern struct mpxplay_drivehand_func_s *mpxplay_diskdrive_search_driver(char *pathname);
extern void mpxplay_diskdrive_alldrives_close(void);
extern long mpxplay_diskdrive_drive_config(struct mpxplay_diskdrive_data_s *mdds, unsigned int funcnum, void *argp1, void *argp2);
extern unsigned int mpxplay_diskdrive_drive_mount(struct mpxplay_diskdrive_data_s *mdds, char *pathname);
extern void mpxplay_diskdrive_drive_unmount(struct mpxplay_diskdrive_data_s *mdds);
extern unsigned int mpxplay_diskdrive_findfirst(struct mpxplay_diskdrive_data_s *mdds, char *pathname, unsigned int attrib, struct pds_find_t *ffblk);
extern unsigned int mpxplay_diskdrive_findnext(struct mpxplay_diskdrive_data_s *mdds, struct pds_find_t *ffblk);
extern void mpxplay_diskdrive_findclose(struct mpxplay_diskdrive_data_s *mdds, struct pds_find_t *ffblk);
extern unsigned int mpxplay_diskdrive_checkdir(struct mpxplay_diskdrive_data_s *mdds, char *dirname);
extern unsigned int mpxplay_diskdrive_isdirroot(struct mpxplay_diskdrive_data_s *mdds, char *dirname);
extern char *mpxplay_diskdrive_getcwd(struct mpxplay_diskdrive_data_s *mdds, char *buf, unsigned int buflen);
extern int mpxplay_diskdrive_chdir(struct mpxplay_diskdrive_data_s *mdds, char *path);
extern int mpxplay_diskdrive_mkdir(struct mpxplay_diskdrive_data_s *mdds, char *path);
extern int mpxplay_diskdrive_rmdir(struct mpxplay_diskdrive_data_s *mdds, char *path);
extern int mpxplay_diskdrive_rename(struct mpxplay_diskdrive_data_s *mdds, char *oldfilename, char *newfilename);
extern int mpxplay_diskdrive_unlink(struct mpxplay_diskdrive_data_s *mdds, char *filename);

extern void *mpxplay_diskdrive_file_open(struct mpxplay_diskdrive_data_s *mdds, char *filename, unsigned long opentype);
extern struct mpxplay_diskdrive_data_s *mpxplay_diskdrive_file_get_mdds(void *mdfp);
extern long mpxplay_diskdrive_file_config(void *mdfd, unsigned int funcnum, void *argp1, void *argp2);
extern void mpxplay_diskdrive_file_close(void *mdfd);
extern long mpxplay_diskdrive_file_read(void *mdfd, char *buf, unsigned int len);
extern long mpxplay_diskdrive_file_write(void *mdfd, char *buf, unsigned int len);
extern mpxp_filesize_t mpxplay_diskdrive_file_seek(void *mdfd, mpxp_filesize_t offset, int fromwhere);
extern mpxp_filesize_t mpxplay_diskdrive_file_tell(void *mdfd);
extern mpxp_filesize_t mpxplay_diskdrive_file_length(void *mdfd);
extern int mpxplay_diskdrive_file_eof(void *mdfd);
extern int mpxplay_diskdrive_file_chsize(void *mdfd, mpxp_filesize_t offset);

extern unsigned int mpxplay_diskdrive_subdirscan_open(struct mpxplay_diskdrive_data_s *mdds, char *path_and_filename, unsigned int attrib, struct pds_subdirscan_t *dsi);
extern int mpxplay_diskdrive_subdirscan_findnextfile(struct mpxplay_diskdrive_data_s *mdds, struct pds_subdirscan_t *dsi);
extern void mpxplay_diskdrive_subdirscan_close(struct mpxplay_diskdrive_data_s *mdds, struct pds_subdirscan_t *dsi);

extern void *mpxplay_diskdrive_textfile_open(struct mpxplay_diskdrive_data_s *mdds, char *filename, unsigned int openmode);	// openmodes: O_RDONLY,O_RDWR|O_CREAT
extern void mpxplay_diskdrive_textfile_close(void *fhp);
extern long mpxplay_diskdrive_textfile_config(void *fhp, unsigned int funcnum, void *argp1, void *argp2);
extern unsigned long mpxplay_diskdrive_textfile_readline(void *fhp, char *readbuf, unsigned long rbuflen);
extern long mpxplay_diskdrive_textfile_writeline(void *fhp, char *str);
extern mpxp_filesize_t mpxplay_diskdrive_textfile_filelength(void *fhp);

#endif							// diskdriv_h
