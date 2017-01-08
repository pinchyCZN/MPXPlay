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
//function: ftp drive handling (ftp client)

#include "in_file.h"
#include "newfunc\newfunc.h"
#include "display\display.h"
#include "control\cntfuncs.h"	// for mpxplay_control_keyboard_get_topfunc()
#include <malloc.h>

#ifdef MPXPLAY_LINK_TCPIP
#define MPXPLAY_DRVFTP_LINK_FTPCLIENT 1
#endif

//#define MPXPLAY_DRVFTP_DEBUGFILE "d:\\wc\\mpxplay\\ftplog.txt"
//FILE *debug_fp;

// ftpi->flags config
#define DRVFTP_FTPDRIVE_FLAG_PASSIVE_MODE     1
#define DRVFTP_FTPDRIVE_FLAG_AUTO_RECONNECT   2
#define DRVFTP_FTPDRIVE_FLAG_USE_ANONYM_LOGIN 4
#define DRVFTP_FTPDRIVE_FLAG_USE_UTF8         8
#define DRVFTP_FTPDRIVE_FLAG_DEFAULT_CONFIG (DRVFTP_FTPDRIVE_FLAG_PASSIVE_MODE|DRVFTP_FTPDRIVE_FLAG_AUTO_RECONNECT|DRVFTP_FTPDRIVE_FLAG_USE_ANONYM_LOGIN)
// inside control
#define DRVFTP_FTPDRIVE_FLAG_RECONNECT      8192

unsigned long mpxplay_diskdrive_drvftp_config = DRVFTP_FTPDRIVE_FLAG_DEFAULT_CONFIG;

#ifdef MPXPLAY_DRVFTP_LINK_FTPCLIENT

#define DRVFTP_MAX_LOCAL_DRIVES   ('Z'-'A'+1)
#define DRVFTP_MAX_VIRTUAL_DRIVES ('7'-'0'+1)
#define DRVFTP_MAX_SESSIONS       (DRVFTP_MAX_VIRTUAL_DRIVES+1)	// +1 ftpfile
#define DRVFTP_FIRSTDRV_VIRTUAL   DRVFTP_MAX_LOCAL_DRIVES	// 0:
#define DRVFTP_VIRTUALDRV_FTPFILE DRVFTP_MAX_VIRTUAL_DRIVES

#define DRVFTP_IP_LEN 4

#define DRVFTP_DEFAULT_RCVBUFSIZE 65536

#define DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION    2	//
#define DRVFTP_DEFAULT_TIMEOUTRETRY_DATACONN   2	//
#define DRVFTP_DEFAULT_TIMEOUTMS_ACCEPT      100	// in msec
#define DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE  15000
#define DRVFTP_DEFAULT_TIMEOUTMS_LONGWAIT  30000
#define DRVFTP_DEFAULT_TIMEOUTMS_READ       6000
#define DRVFTP_DEFAULT_TIMEOUTMS_EOF        1000	// !!!
#define DRVFTP_DEFAULT_TIMEOUTMS_DISP       1500

// ftp://user:pasw@servername:port/dir
#define DRVFTP_PATHSEPARATOR_USER     '@'
#define DRVFTP_PATHSEPARATOR_PASSWORD ':'
#define DRVFTP_PATHSEPARATOR_PORTNUM  ':'

// dirs and direntries (ftpi->cached_dir_datas)
#define DRVFTP_CACHED_DIRS_INITSTEP    512	// initial and expand-step
#define DRVFTP_CACHED_DIRS_MAX        4096	// to not eat all memory
#define DRVFTP_CACHED_DIRENT_INITSTEP 1024
#define DRVFTP_CACHED_DIRENT_MAX     16384

// ftpi->system_type
#define DRVFTP_FTPDRIVEINFO_SYSTYPE_UNIX  1
#define DRVFTP_FTPDRIVEINFO_SYSTYPE_WINNT 2

// ftpi->file_open_id_num (unlock/lock)
#define DRVFTP_FILEOPENID_FREE 0
#define DRVFTP_FILEOPENID_BUSY -1000

// ftpi->lastdatatype
#define DRVFTP_DATATYPE_ASCII  1
#define DRVFTP_DATATYPE_BINARY 2

// ftpi->server_features
#define DRVFTP_FEATURE_REST      1
#define DRVFTP_FEATURE_SIZE      2
#define DRVFTP_FEATURE_MLSD      4
#define DRVFTP_FEATURE_AUTH_SSL  8
#define DRVFTP_FEATURE_AUTH_TLS 16
#define DRVFTP_FEATURE_UTF8     32
#define DRVFTP_FEATURE_CLNT     64

// respcontrol
#define DRVFTP_RESPCNTR_NONE         0
#define DRVFTP_RESPCNTR_INSTANT      1
#define DRVFTP_RESPCNTR_NORETCODECHK 2
#define DRVFTP_RESPCNTR_LONGWAIT     4

#define DRVFTP_RESPCODE_OPENING_DATACONN   150
#define DRVFTP_RESPCODE_COMMAND_OK         200
#define DRVFTP_RESPCODE_CMD_SIZE_OK        213
#define DRVFTP_RESPCODE_TRANSFER_COMPLETE  226
#define DRVFTP_RESPCODE_ENTERING_PASSIVE   227
#define DRVFTP_RESPCODE_DIRCOMMAND_OK      250
#define DRVFTP_RESPCODE_CMD_PWD_OK         257
#define DRVFTP_RESPCODE_CMD_REST_OK        350
#define DRVFTP_RESPCODE_ERRORCODES         400	// ???
#define DRVFTP_RESPCODE_DATACONN_FAILED    425
#define DRVFTP_RESPCODE_TRANSFER_ABORTED   426
#define DRVFTP_RESPCODE_FILE_EXIST_ALREADY 550
#define DRVFTP_RESPCODE_FILE_NOT_FOUND     550	// or permission denied

// ftfi->flags
#define DRVFTP_FTPFILE_FLAG_SEEK     1
#define DRVFTP_FTPFILE_FLAG_WRITE    2
#define DRVFTP_FTPFILE_FLAG_READWAIT 4

// ftfi->opentype
#define DRVFTP_FTPFILE_OPENTYPE_READ   1
#define DRVFTP_FTPFILE_OPENTYPE_WRITE  2
#define DRVFTP_FTPFILE_OPENTYPE_CREATE 4
#define DRVFTP_FTPFILE_OPENTYPE_TEXT   8

// socketinfo->flags
#define DRVFTP_SOCKINFO_FLAG_SSL_DISABLED  1

// socket_select checkmode
#define DRVFTP_SOCKSELECT_MODE_READ  1
#define DRVFTP_SOCKSELECT_MODE_WRITE 2

typedef mpxp_uint32_t ftpdrive_socket_t;

typedef struct ftpdrive_direntry_info_s {
	char *filename;
	unsigned long attrib;
	mpxp_filesize_t filesize;
	pds_fdate_t fdate;
} ftpdrive_direntry_info_s;

typedef struct ftpdrive_directory_info_s {
	char *dirname;
	unsigned long cached_entries_num;
	unsigned long cached_entries_max;
	struct ftpdrive_direntry_info_s *entrydatas;
} ftpdrive_directory_info_s;

typedef struct ftpdrive_socket_info_s {
	ftpdrive_socket_t socknum;
	mpxp_uint8_t conn_ip_addr[DRVFTP_IP_LEN];
	unsigned int portnum;
	void *sslhand;
	unsigned int flags;
} ftpdrive_socket_info_s;

typedef struct ftpdrive_info_s {
	mpxp_uint32_t flags;
	unsigned int drivenum;
	struct ftpdrive_info_s **ftpdrive_info_ptr;
	long connect_id_num;
	unsigned int system_type;
	unsigned int server_features;
	unsigned int connection_retry;
	struct ftpdrive_lowlevel_func_s *lowfunc;
	struct ftpdrive_socket_info_s socket_info_session;
	struct ftpdrive_socket_info_s socket_info_filehand;	// one filehand (data connect) per session
	long file_open_id_num;		// id of last opened file
	unsigned long socket_bufsize;

	unsigned long cached_dirs_num;
	unsigned long cached_dirs_max;
	struct ftpdrive_directory_info_s *cached_dir_datas;

	unsigned int lastrespcode;
	unsigned int lastdatatype;
	mpxp_uint8_t ip_local[DRVFTP_IP_LEN];
	mpxp_uint8_t ip_remote[DRVFTP_IP_LEN];

	unsigned long message_bufbytes;
	char message_buffer[256];

	char currremotedir_selected[MAX_PATHNAMELEN];
	char currremotedir_real[MAX_PATHNAMELEN];

	char lastresptext[256];

	char servername[256];
	char username[128];
	char password[128];
} ftpdrive_info_s;

typedef struct ftpdrive_filefind_s {
	unsigned long entrynum_curr;
	unsigned long entrynum_end;
	struct ftpdrive_direntry_info_s *entry;
	char searchfilemask[MAX_PATHNAMELEN];
} ftpdrive_filefind_s;

typedef struct ftpfile_info_s {
	mpxp_uint32_t flags;
	unsigned int opentype;
	long connect_id_num;
	long open_id_num;
	unsigned long file_bufsize;
	mpxp_filesize_t filepos;
	mpxp_filesize_t filesize;
	mpxp_uint64_t timeout_at_read;
	struct ftpdrive_info_s **ftpi_ftpdrives_ptr;
	struct ftpdrive_info_s ftpi_infos;
	char remotefilename[MAX_PATHNAMELEN];
	char singleftpfilename[MAX_PATHNAMELEN];
} ftpfile_info_s;

typedef struct ftpdrive_lowlevel_func_s {
	char *name;
	unsigned int def_portnum;
	int (*global_init) (void);
	void (*global_deinit) (void);
	int (*addressinfo_init) (struct ftpdrive_info_s * ftpi);
	int (*login_preprocess) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_session);
	int (*socket_open) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_any, unsigned long bufsize);
	void (*socket_shutdown) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_any);
	void (*socket_close) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_any, unsigned int full);
	int (*socket_select) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_any, unsigned int checkmode);
	int (*socket_connect) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_any);
	int (*socket_ssl_connect) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_any);
	int (*socket_listen) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_filehand);
	int (*socket_accept) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socketinfo_filehand);
	long (*send) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socket_info, char *data, unsigned long bytes_to_send);
	long (*bytes_buffered) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socket_info);
	long (*receive) (struct ftpdrive_info_s * ftpi, struct ftpdrive_socket_info_s * socket_info, char *data, unsigned long buflen);
} ftpdrive_lowlevel_func_s;

static ftpdrive_lowlevel_func_s FTPDRV_lowlevel_funcs;
#ifdef MPXPLAY_WIN32
static ftpdrive_lowlevel_func_s FTPSDRV_lowlevel_funcs;
static ftpdrive_lowlevel_func_s FTPESDRV_lowlevel_funcs;
#endif

static ftpdrive_lowlevel_func_s *ALL_lowlevel_funcs[] = {
	&FTPDRV_lowlevel_funcs,		// 0.
#ifdef MPXPLAY_WIN32
	&FTPSDRV_lowlevel_funcs,	// 1.
	&FTPESDRV_lowlevel_funcs,	// 2.
#endif
	NULL
};

typedef struct ftpdrive_feattable_s {
	char *featname;
	unsigned int featflag;
} ftpdrive_feattable_s;

static ftpdrive_feattable_s ftpdrive_feature_table[] = {
	{"REST", DRVFTP_FEATURE_REST},
	{"SIZE", DRVFTP_FEATURE_SIZE},
	{"MLSD", DRVFTP_FEATURE_MLSD},
	{"AUTH SSL", DRVFTP_FEATURE_AUTH_SSL},
	{"AUTH TLS", DRVFTP_FEATURE_AUTH_TLS},
	{"UTF8", DRVFTP_FEATURE_UTF8},
	{"CLNT", DRVFTP_FEATURE_CLNT},
	{"", 0}
};

typedef struct fptdrive_timeoutmsg_s {
	mpxp_uint64_t endtime_response, begintime_disp, lasttime_disp;
	char msgmask[128];
} ftpdrive_timeoutmsg_s;

static long drvftp_connectid_num;
static struct ftpdrive_info_s *ftpdrives_info_ptrs[DRVFTP_MAX_SESSIONS];

static void drvftp_dircache_dir_dealloc(struct ftpdrive_directory_info_s *diri);
static unsigned int ftpdrive_session_connect(struct ftpdrive_info_s *ftpi);
static void ftpdrive_drive_unmount(void *drivehand_data);
static unsigned int ftpdrive_checkdir(void *drivehand_data, char *dirname);
static int ftpdrive_chdir(void *drivehand_data, char *path);
static void drvftp_str_localname_to_remote(char *remotename, char *pathname);
static int drvftp_cwd(struct ftpdrive_info_s *ftpi, char *remotename);
static void drvftp_message_write_error(char *message);
static void drvftp_message_timeout_init(struct fptdrive_timeoutmsg_s *tos, unsigned long timeoutms, char *msgmask);
static void drvftp_message_timeout_reset(struct fptdrive_timeoutmsg_s *tos, unsigned long timeoutms);
static void drvftp_message_timeout_write(struct fptdrive_timeoutmsg_s *tos);
static void drvftp_message_timeout_close(struct fptdrive_timeoutmsg_s *tos);

//-------------------------------------------------------------------------
//directory cache (filename,attribs,filesize,filedate)
static unsigned int drvftp_dircache_dirs_expand(struct ftpdrive_info_s *ftpi)
{
	struct ftpdrive_directory_info_s *dirdatas;

	if(ftpi->cached_dirs_max >= DRVFTP_CACHED_DIRS_MAX)
		return 0;
	dirdatas = (struct ftpdrive_directory_info_s *)malloc((ftpi->cached_dirs_max + DRVFTP_CACHED_DIRS_INITSTEP) * sizeof(*dirdatas));
	if(!dirdatas)
		return 0;
	if(ftpi->cached_dir_datas) {
		pds_memcpy(dirdatas, ftpi->cached_dir_datas, (ftpi->cached_dirs_max * sizeof(*dirdatas)));
		free(ftpi->cached_dir_datas);
	}
	pds_memset(dirdatas + ftpi->cached_dirs_max, 0, (DRVFTP_CACHED_DIRS_INITSTEP * sizeof(*dirdatas)));
	ftpi->cached_dir_datas = dirdatas;
	ftpi->cached_dirs_max += DRVFTP_CACHED_DIRS_INITSTEP;
	return 1;
}

static struct ftpdrive_directory_info_s *drvftp_dircache_dir_realloc(struct ftpdrive_info_s *ftpi, struct ftpdrive_directory_info_s *dirdatas, char *dirname)
{
	unsigned int step;
	if(dirdatas) {
		drvftp_dircache_dir_dealloc(dirdatas);
		step = 0;
	} else {
		if(ftpi->cached_dirs_num >= ftpi->cached_dirs_max)
			if(!drvftp_dircache_dirs_expand(ftpi))
				return NULL;
		dirdatas = ftpi->cached_dir_datas + ftpi->cached_dirs_num;
		step = 1;
	}
	dirdatas->dirname = malloc(pds_strlen(dirname) + 1);
	if(!dirdatas->dirname)
		return NULL;
	pds_strcpy(dirdatas->dirname, dirname);
	ftpi->cached_dirs_num += step;
	return dirdatas;
}

static void drvftp_dircache_dir_dealloc(struct ftpdrive_directory_info_s *diri)
{
	struct ftpdrive_direntry_info_s *ed;
	unsigned int i;
	if(diri) {
		if(diri->dirname)
			free(diri->dirname);
		ed = diri->entrydatas;
		if(ed) {
			i = diri->cached_entries_num;
			if(i)
				do {
					if(ed->filename)
						free(ed->filename);
					ed++;
				} while(--i);
			free(diri->entrydatas);
		}
		pds_memset(diri, 0, sizeof(*diri));
	}
}

static void drvftp_dircache_alldirs_dealloc(struct ftpdrive_info_s *ftpi)
{
	struct ftpdrive_directory_info_s *diri = ftpi->cached_dir_datas;
	unsigned int i;
	if(diri) {
		i = ftpi->cached_dirs_num;
		if(i)
			do {
				drvftp_dircache_dir_dealloc(diri);
				diri++;
			} while(--i);
		free(ftpi->cached_dir_datas);
		ftpi->cached_dir_datas = NULL;
	}
	ftpi->cached_dirs_num = ftpi->cached_dirs_max = 0;
}

static struct ftpdrive_directory_info_s *drvftp_dircache_dir_searchby_name(struct ftpdrive_info_s *ftpi, char *dirname)
{
	struct ftpdrive_directory_info_s *diri = ftpi->cached_dir_datas;
	unsigned int i;
	if(diri) {
		i = ftpi->cached_dirs_num;
		if(i)
			do {
				if(diri->dirname && (pds_stricmp(diri->dirname, dirname) == 0))
					return diri;
				diri++;
			} while(--i);
	}
	return NULL;
}

static unsigned int drvftp_dircache_entries_expand(struct ftpdrive_directory_info_s *diri)
{
	struct ftpdrive_direntry_info_s *ed;
	if(diri->cached_entries_max >= DRVFTP_CACHED_DIRENT_MAX)
		return 0;
	ed = (struct ftpdrive_direntry_info_s *)malloc((diri->cached_entries_max + DRVFTP_CACHED_DIRENT_INITSTEP) * sizeof(*ed));
	if(!ed)
		return 0;
	if(diri->entrydatas) {
		pds_memcpy(ed, diri->entrydatas, (diri->cached_entries_max * sizeof(*ed)));
		free(diri->entrydatas);
	}
	pds_memset(ed + diri->cached_entries_num, 0, (DRVFTP_CACHED_DIRENT_INITSTEP * sizeof(*ed)));
	diri->entrydatas = ed;
	diri->cached_entries_max += DRVFTP_CACHED_DIRENT_INITSTEP;
	return 1;
}

static struct ftpdrive_direntry_info_s *drvftp_dircache_entry_alloc(struct ftpdrive_directory_info_s *diri, char *filename)
{
	struct ftpdrive_direntry_info_s *ed;
	if(diri->cached_entries_num >= diri->cached_entries_max)
		if(!drvftp_dircache_entries_expand(diri))
			return NULL;
	ed = diri->entrydatas + diri->cached_entries_num;
	ed->filename = malloc(pds_strlen(filename) + 1);
	if(!ed->filename)
		return NULL;
	pds_strcpy(ed->filename, filename);
	diri->cached_entries_num++;
	return ed;
}

static char *drvftp_str_getpath_from_fullname(char *path, char *fullname)
{
	char *filenamep;

	if(!path)
		return NULL;
	if(!fullname) {
		*path = 0;
		return NULL;
	}

	if(path != fullname)
		pds_strcpy(path, fullname);
	filenamep = pds_strrchr(path, PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP);
	if(filenamep) {
		if((filenamep == path) || (path[1] == ':' && filenamep == (path + 2)))	// "\\filename.xxx" or "c:\\filename.xxx"
			filenamep[1] = 0;
		else
			filenamep[0] = 0;	// c:\\subdir\\filename.xxx
		filenamep++;
	} else {
		filenamep = pds_strchr(path, ':');
		if(filenamep)
			*(++filenamep) = 0;
		else {
			filenamep = path;
			*path = 0;
		}
	}
	filenamep = fullname + (filenamep - path);
	return filenamep;
}

static struct ftpdrive_direntry_info_s *drvftp_dircache_entry_searchby_fullname(struct ftpdrive_info_s *ftpi, char *fullname)
{
	struct ftpdrive_directory_info_s *diri;
	struct ftpdrive_direntry_info_s *ed;
	unsigned int en;
	char *filename, dirname[MAX_PATHNAMELEN];

	filename = drvftp_str_getpath_from_fullname(dirname, fullname);

	diri = drvftp_dircache_dir_searchby_name(ftpi, dirname);
	if(!diri)
		goto err_out_remove;
	ed = diri->entrydatas;
	if(!ed)
		goto err_out_remove;
	en = diri->cached_entries_num;
	if(!en)
		goto err_out_remove;
	do {
		if(ed->filename && (pds_stricmp(ed->filename, filename) == 0))
			return ed;
		ed++;
	} while(--en);
  err_out_remove:
	return NULL;
}

static struct ftpdrive_direntry_info_s *drvftp_dircache_entry_removeby_fullname(struct ftpdrive_info_s *ftpi, char *fullname)
{
	struct ftpdrive_direntry_info_s *ed = drvftp_dircache_entry_searchby_fullname(ftpi, fullname);
	if(ed)
		if(ed->filename) {
			free(ed->filename);
			ed->filename = NULL;
		}
	return ed;
}

static struct ftpdrive_direntry_info_s *drvftp_dircache_entry_addby_fullname(struct ftpdrive_info_s *ftpi, char *fullname)
{
	struct ftpdrive_directory_info_s *diri;
	struct ftpdrive_direntry_info_s *ed;
	unsigned int en;
	char *filename, dirname[MAX_PATHNAMELEN];

	filename = drvftp_str_getpath_from_fullname(dirname, fullname);

	diri = drvftp_dircache_dir_searchby_name(ftpi, dirname);
	if(!diri)
		return NULL;
	ed = diri->entrydatas;
	if(!ed)
		goto err_out_add;
	en = diri->cached_entries_max;
	if(!en)
		goto err_out_add;
	do {
		if(!ed->filename) {		// deleted by entry_removeby_fullname
			pds_memset(ed, 0, sizeof(*ed));
			ed->filename = malloc(pds_strlen(filename) + 1);
			if(!ed->filename)
				return NULL;
			pds_strcpy(ed->filename, filename);
			diri->cached_entries_num++;
			return ed;
		}
		ed++;
	} while(--en);
  err_out_add:
	return drvftp_dircache_entry_alloc(diri, filename);
}

static void drvftp_dircache_entry_copyto_ffblk(struct pds_find_t *ffblk, struct ftpdrive_direntry_info_s *ed)
{
	ffblk->attrib = ed->attrib;
	ffblk->size = ed->filesize;
	pds_memcpy(&ffblk->fdate, &ed->fdate, sizeof(ffblk->fdate));
	pds_strcpy(ffblk->name, ed->filename);
}

//----------------------------------------------------------------------
static unsigned int drvftp_cmdctrl_sendcommand(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command)
{
	unsigned int bytes;
	char cmd[MAX_PATHNAMELEN + 32];
	snprintf(cmd, sizeof(cmd), "%s\r\n", command);
	bytes = ftpi->lowfunc->send(ftpi, socketinfo_session, cmd, pds_strlen(cmd));
	if(!bytes && funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_AUTO_RECONNECT))
		funcbit_smp_enable(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT);
#ifdef MPXPLAY_DRVFTP_DEBUGFILE
	if(debug_fp)
		fprintf(debug_fp, "%s\n", command);
#endif
	return bytes;
}

// block based reading
static unsigned int drvftp_cmdctrl_read_respline(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *response, unsigned int respbufsize, unsigned int respcontrol)
{
	long bytes, len, retcode = 0, mesbufmaxsize = sizeof(ftpi->message_buffer) - 4;
	unsigned int loc_rescontrol = respcontrol;
	char *e;
	mpxp_uint64_t endtime = pds_gettimem();
	struct fptdrive_timeoutmsg_s tos;

	if(funcbit_test(respcontrol, DRVFTP_RESPCNTR_LONGWAIT))
		endtime += DRVFTP_DEFAULT_TIMEOUTMS_LONGWAIT;
	else
		endtime += DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE;

	drvftp_message_timeout_init(&tos, endtime, "waiting for server %d sec ...");

	do {
		if(ftpi->message_bufbytes) {
			e = pds_strchr(ftpi->message_buffer, '\n');
			if(e) {
				*e++ = 0;
				len = e - (&ftpi->message_buffer[0]);
				if(len < respbufsize) {
					bytes = pds_strcpy(response, ftpi->message_buffer);
					if(response[bytes - 1] == '\r') {
						bytes--;
						response[bytes] = 0;
					}
					retcode = 1;
#ifdef MPXPLAY_DRVFTP_DEBUGFILE
					if(debug_fp)
						fprintf(debug_fp, "%s\n", response);
#endif
				}
				funcbit_smp_value_put(ftpi->message_bufbytes, pds_strcpy(ftpi->message_buffer, e));
				break;
			} else if(ftpi->message_bufbytes >= mesbufmaxsize) {	// message buffer is full (and eol not found)
				funcbit_smp_value_put(ftpi->message_bufbytes, 0);
			}
		}
		bytes = ftpi->lowfunc->bytes_buffered(ftpi, socketinfo_session);
		if(bytes < 0)
			break;
		if((bytes == 0) && (ftpi->lowfunc->socket_select(ftpi, socketinfo_session, DRVFTP_SOCKSELECT_MODE_READ) != 0) && (ftpi->lowfunc->bytes_buffered(ftpi, socketinfo_session) <= 0))	// required!
			break;
		if(bytes) {
			bytes = min(bytes, (mesbufmaxsize - ftpi->message_bufbytes));
			if(bytes) {			// message_buffer is not full
				long gotbytes = ftpi->lowfunc->receive(ftpi, socketinfo_session, &ftpi->message_buffer[ftpi->message_bufbytes], bytes);
				if(gotbytes != bytes)	// should not happen
					bytes = min(gotbytes, (mesbufmaxsize - ftpi->message_bufbytes));
				funcbit_smp_value_put(ftpi->message_bufbytes, (ftpi->message_bufbytes + bytes));
				funcbit_smp_value_put(ftpi->message_buffer[ftpi->message_bufbytes], 0);
			}
		} else
			drvftp_message_timeout_write(&tos);

		if(loc_rescontrol & DRVFTP_RESPCNTR_INSTANT) {	// read only buffered/started responses (don't wait for new ones)
			if(!ftpi->message_bufbytes)
				break;
			funcbit_disable(loc_rescontrol, DRVFTP_RESPCNTR_INSTANT);
		}
		if((pds_look_extgetch() == KEY_ESC) && (pds_gettimem() > tos.begintime_disp))
			break;
	} while(pds_gettimem() <= endtime);

	if(!retcode) {
		bytes = 0;
		if(!funcbit_test(respcontrol, DRVFTP_RESPCNTR_INSTANT) && funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_AUTO_RECONNECT) && (pds_look_extgetch() != KEY_ESC))
			funcbit_smp_enable(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT);
		if((pds_look_extgetch() == KEY_ESC) && (pds_gettimem() > tos.begintime_disp) && !mpxplay_control_keyboard_get_topfunc())	// !!! bullshit
			pds_extgetch();
	}

	drvftp_message_timeout_close(&tos);

	return bytes;
}

static unsigned int drvftp_cmdctrl_read_response(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *response, unsigned int respbufsize, unsigned int respcontrol)
{
	long retcode = 0;
	do {
		if(!drvftp_cmdctrl_read_respline(ftpi, socketinfo_session, response, respbufsize, respcontrol))
			break;
		retcode = pds_atol(response);
	} while(!(respcontrol & DRVFTP_RESPCNTR_NORETCODECHK) && ((retcode < 100) || (retcode > 999)));
	if(retcode < 100)
		retcode = 0;
	else {
		funcbit_smp_value_put(ftpi->lastrespcode, retcode);
		pds_strncpy(ftpi->lastresptext, response + 4, sizeof(ftpi->lastresptext) - 1);
		ftpi->lastresptext[sizeof(ftpi->lastresptext) - 1] = 0;
	}
	return retcode;
}

static unsigned int drvftp_cmdctrl_read_respcode(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, unsigned int respcontrol)
{
	char response[MAX_PATHNAMELEN + 32];
	return drvftp_cmdctrl_read_response(ftpi, socketinfo_session, response, sizeof(response), respcontrol);
}

static unsigned int drvftp_cmdctrl_check_respcode(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, unsigned int respcontrol, unsigned int expected_respcode)
{
	do {
		unsigned int respcode = drvftp_cmdctrl_read_respcode(ftpi, socketinfo_session, respcontrol);
		if(!respcode)
			break;
		if(respcode == expected_respcode)
			return 1;
		if(respcode >= DRVFTP_RESPCODE_ERRORCODES)
			break;
	} while(1);
	return 0;
}

static unsigned int drvftp_cmdctrl_check_listof_respcodes(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, unsigned int respcontrol, unsigned int *expected_respcodes)
{
	if(!expected_respcodes || !expected_respcodes[0])
		return 0;
	do {
		unsigned int *i, respcode = drvftp_cmdctrl_read_respcode(ftpi, socketinfo_session, respcontrol);
		if(!respcode)
			break;
		i = expected_respcodes;
		do {
			if(respcode == *i)
				return 1;
			i++;
		} while(*i);
		if(respcode >= DRVFTP_RESPCODE_ERRORCODES)
			break;
	} while(1);
	return 0;
}

static void drvftp_cmdctrl_flush_responses(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session)
{
	char tmp[MAX_PATHNAMELEN + 32];
	while(drvftp_cmdctrl_read_response(ftpi, socketinfo_session, tmp, sizeof(tmp), (DRVFTP_RESPCNTR_INSTANT | DRVFTP_RESPCNTR_NORETCODECHK))) {
	}
	funcbit_smp_value_put(ftpi->message_bufbytes, 0);
}

static unsigned int drvftp_cmdctrl_send_command_check_respcode(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command, unsigned int expected_respcode)
{
	drvftp_cmdctrl_flush_responses(ftpi, socketinfo_session);
	if(!drvftp_cmdctrl_sendcommand(ftpi, socketinfo_session, command))
		return 0;
	return drvftp_cmdctrl_check_respcode(ftpi, socketinfo_session, DRVFTP_RESPCNTR_NONE, expected_respcode);
}

static unsigned int drvftp_cmdctrl_send_command_respcntr_check_respcode(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command, unsigned int respcontrol,
																		unsigned int expected_respcode)
{
	drvftp_cmdctrl_flush_responses(ftpi, socketinfo_session);
	if(!drvftp_cmdctrl_sendcommand(ftpi, socketinfo_session, command))
		return 0;
	return drvftp_cmdctrl_check_respcode(ftpi, socketinfo_session, respcontrol, expected_respcode);
}

static unsigned int drvftp_cmdctrl_send_command_get_success(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command)
{
	unsigned int respcode;
	drvftp_cmdctrl_flush_responses(ftpi, socketinfo_session);
	if(!drvftp_cmdctrl_sendcommand(ftpi, socketinfo_session, command))
		return 0;
	respcode = drvftp_cmdctrl_read_respcode(ftpi, socketinfo_session, DRVFTP_RESPCNTR_NONE);
	if((respcode >= 100) && (respcode < DRVFTP_RESPCODE_ERRORCODES))
		return 1;
	return 0;
}

//splits response to a retcode-number and a string (after the retcode)
static int drvftp_cmdctrl_send_command_get_response(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command, char *respbuf, unsigned int buflen,
													unsigned int respcontrol)
{
	int retcode;
	drvftp_cmdctrl_flush_responses(ftpi, socketinfo_session);
	if(!drvftp_cmdctrl_sendcommand(ftpi, socketinfo_session, command))
		return 0;
	if(respbuf && buflen) {
		char *s;
		*respbuf = 0;
		retcode = drvftp_cmdctrl_read_response(ftpi, socketinfo_session, respbuf, buflen, respcontrol);
		s = pds_strchr(respbuf, '\r');
		if(s) {
			*s = 0;
			pds_strcpy(respbuf, &respbuf[4]);
		} else {
			pds_strncpy(respbuf, &respbuf[4], buflen - 4);
			respbuf[buflen - 4] = 0;
		}
	} else
		retcode = drvftp_cmdctrl_read_respcode(ftpi, socketinfo_session, respcontrol);
	return retcode;
}

//with auto reconnect
static unsigned int drvftp_cmdctrl_arc_send_command_check_respcode(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command, unsigned int expected_respcode)
{
	unsigned int retry = 1, respcode;
	do {
		drvftp_cmdctrl_flush_responses(ftpi, socketinfo_session);
		if(!drvftp_cmdctrl_sendcommand(ftpi, socketinfo_session, command))
			break;
		respcode = drvftp_cmdctrl_read_respcode(ftpi, socketinfo_session, DRVFTP_RESPCNTR_NONE);
		if(respcode == expected_respcode)
			return 1;
		if(respcode >= DRVFTP_RESPCODE_ERRORCODES)
			break;
		if(!funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT))
			break;
		if(!retry)
			break;
		retry = 0;
		if(!ftpdrive_session_connect(ftpi))
			break;
	} while(1);
	return 0;
}

static unsigned int drvftp_cmdctrl_arc_send_command_get_success(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command)
{
	unsigned int retry = 1, retcode = 0;
	do {
		retcode = drvftp_cmdctrl_send_command_get_success(ftpi, socketinfo_session, command);
		if(retcode)
			break;
		if(!funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT))
			break;
		if(!retry)
			break;
		retry = 0;
		if(!ftpdrive_session_connect(ftpi))
			break;
	} while(1);
	return retcode;
}

static int drvftp_cmdctrl_arc_send_command_get_response(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, char *command, char *respbuf, unsigned int buflen,
														unsigned int respcontrol)
{
	unsigned int retry = 1, retcode = 0;
	do {
		retcode = drvftp_cmdctrl_send_command_get_response(ftpi, socketinfo_session, command, respbuf, buflen, respcontrol);
		if(retcode)
			break;
		if(!funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT))
			break;
		if(!retry)
			break;
		retry = 0;
		if(!ftpdrive_session_connect(ftpi))
			break;
	} while(1);
	return retcode;
}

//---------------------------------------------------------------------------
static int drvftp_dataconn_send_type(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session, unsigned int type)
{
	char *command;
	if(ftpi->lastdatatype == type)
		return 1;
	switch (type) {
	case DRVFTP_DATATYPE_ASCII:
		command = "TYPE A";
		break;
	case DRVFTP_DATATYPE_BINARY:
		command = "TYPE I";
		break;
	default:
		return 0;
	}
	if(!drvftp_cmdctrl_send_command_check_respcode(ftpi, socketinfo_session, command, DRVFTP_RESPCODE_COMMAND_OK))
		return 0;
	funcbit_smp_value_put(ftpi->lastdatatype, type);
	return 1;
}

static int drvftp_dataconn_open(struct ftpdrive_info_s *ftpi, struct ftpfile_info_s *ftfi, char *command, char *openpathfilename, struct ftpdrive_socket_info_s *socketinfo_filehand,
								unsigned int datatype)
{
	unsigned int success = 0, retry = DRVFTP_DEFAULT_TIMEOUTRETRY_DATACONN, i;
	unsigned long ip_nums[DRVFTP_IP_LEN], port_nums[2];
	mpxp_uint64_t endtime_dataconn;
	char *filename, *ip, cmd[MAX_PATHNAMELEN + 16];
	char newdir[MAX_PATHNAMELEN];

	if(ftfi) {					// filename
		filename = drvftp_str_getpath_from_fullname(newdir, openpathfilename);
		if(!filename)			// should not happen
			goto err_out_getdataa;
	} else {					// directory
		pds_strcpy(newdir, openpathfilename);
		filename = NULL;
	}
	if(newdir[0])
		if(drvftp_cwd(ftpi, newdir) != 0)	// cwd to dir of file (some servers don't like full pathes)
			goto err_out_getdataa;

	do {
		ftpi->lowfunc->socket_close(ftpi, socketinfo_filehand, 0);

		drvftp_cmdctrl_flush_responses(ftpi, &(ftpi->socket_info_session));

		if(!ftpi->lowfunc->socket_open(ftpi, socketinfo_filehand, ((ftpi->socket_bufsize) ? ftpi->socket_bufsize : DRVFTP_DEFAULT_RCVBUFSIZE)))
			break;

		if(!drvftp_dataconn_send_type(ftpi, &(ftpi->socket_info_session), datatype))
			goto err_out_cont;

		if(ftfi && !ftfi->filesize) {
			snprintf(cmd, sizeof(cmd), "SIZE %s", filename);
			if(drvftp_cmdctrl_send_command_check_respcode(ftpi, &(ftpi->socket_info_session), cmd, DRVFTP_RESPCODE_CMD_SIZE_OK))
				ftfi->filesize = pds_atoi64(ftpi->lastresptext);
			else if((ftpi->lastrespcode == DRVFTP_RESPCODE_FILE_NOT_FOUND) && funcbit_test(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_READ)) {
				struct ftpdrive_direntry_info_s *ed = drvftp_dircache_entry_searchby_fullname(ftpi, openpathfilename);
				if(ed && ed->filesize)
					ftfi->filesize = ed->filesize;
				else
					goto err_out_getdataa;
			}
		}

		if(funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_PASSIVE_MODE)) {
			if(!drvftp_cmdctrl_send_command_check_respcode(ftpi, &(ftpi->socket_info_session), "PASV", DRVFTP_RESPCODE_ENTERING_PASSIVE))
				goto err_out_cont;
			ip = pds_strchr(ftpi->lastresptext, '(');
			if(!ip)
				goto err_out_cont;
			pds_memset(&ip_nums[0], 0, sizeof(ip_nums));
			pds_memset(&port_nums[0], 0, sizeof(port_nums));
			sscanf(ip, "(%d,%d,%d,%d,%d,%d)", &ip_nums[0], &ip_nums[1], &ip_nums[2], &ip_nums[3], &port_nums[0], &port_nums[1]);
			socketinfo_filehand->portnum = (port_nums[0] << 8) + port_nums[1];
			if(socketinfo_filehand->portnum >= 65536)
				goto err_out_cont;
			for(i = 0; i < DRVFTP_IP_LEN; i++)
				socketinfo_filehand->conn_ip_addr[i] = ip_nums[i];
		} else {
			if(!ftpi->lowfunc->socket_listen(ftpi, socketinfo_filehand))
				goto err_out_cont;
			sprintf(cmd, "PORT %d,%d,%d,%d,%d,%d", (unsigned int)ftpi->ip_local[0], (unsigned int)ftpi->ip_local[1],
					(unsigned int)ftpi->ip_local[2], (unsigned int)ftpi->ip_local[3], (socketinfo_filehand->portnum >> 8), (socketinfo_filehand->portnum & 0xff));
			if(!drvftp_cmdctrl_send_command_check_respcode(ftpi, &(ftpi->socket_info_session), cmd, DRVFTP_RESPCODE_COMMAND_OK))
				goto err_out_cont;
		}

		if(ftfi && ftfi->filepos) {
			sprintf(cmd, "REST %d", ftfi->filepos);
			if(!drvftp_cmdctrl_send_command_check_respcode(ftpi, &(ftpi->socket_info_session), cmd, DRVFTP_RESPCODE_CMD_REST_OK))
				goto err_out_getdataa;
		}

		snprintf(cmd, sizeof(cmd), "%s%s%s", command, ((filename) ? " " : ""), ((filename) ? filename : ""));

		if(funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_PASSIVE_MODE)) {
			unsigned int rc[3] = { DRVFTP_RESPCODE_OPENING_DATACONN, 125, 0 };
			if(!drvftp_cmdctrl_sendcommand(ftpi, &ftpi->socket_info_session, cmd))
				goto err_out_cont;
			if(!ftpi->lowfunc->socket_connect(ftpi, socketinfo_filehand))
				goto err_out_cont;
			if(!drvftp_cmdctrl_check_listof_respcodes(ftpi, &ftpi->socket_info_session, DRVFTP_RESPCNTR_NONE, &rc[0])) {
				if(ftpi->lastrespcode == DRVFTP_RESPCODE_FILE_NOT_FOUND)
					goto err_out_getdataa;
				goto err_out_cont;
			}
			if(!ftpi->lowfunc->socket_ssl_connect || ftpi->lowfunc->socket_ssl_connect(ftpi, socketinfo_filehand))
				success = 1;
		} else {
			if(!drvftp_cmdctrl_send_command_check_respcode(ftpi, &(ftpi->socket_info_session), cmd, DRVFTP_RESPCODE_OPENING_DATACONN))
				goto err_out_cont;
			endtime_dataconn = pds_gettimem() + DRVFTP_DEFAULT_TIMEOUTMS_ACCEPT;
			do {
				if(ftpi->lowfunc->socket_accept(ftpi, socketinfo_filehand)) {
					success = 1;
					break;
				}
				if(drvftp_cmdctrl_read_respcode(ftpi, &(ftpi->socket_info_session), DRVFTP_RESPCNTR_INSTANT) == DRVFTP_RESPCODE_DATACONN_FAILED)
					break;
			} while(pds_gettimem() <= endtime_dataconn);
		}
	  err_out_cont:
		if(funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT))
			if(!ftpdrive_session_connect(ftpi))
				break;
	} while(!success && (--retry));

	funcbit_smp_value_put(ftpi->socket_bufsize, 0);

	if(success)
		return 1;

	snprintf(cmd, sizeof(cmd), "dataconn (file/dir) open failed!\n%s", ((filename) ? filename : ((openpathfilename) ? openpathfilename : "")));
	drvftp_message_write_error(cmd);

  err_out_getdataa:
	//snprintf(cmd,sizeof(cmd),"%s\n%s",ftpi->lastresptext,((filename)? filename:((openpathfilename)? openpathfilename:"")));
	//drvftp_message_write_error(cmd);
	ftpi->lowfunc->socket_close(ftpi, socketinfo_filehand, 0);
	return 0;
}

static long drvftp_dataconn_read(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_filehand, char *bufptr, unsigned int buflen, unsigned int requested_bytes,
								 mpxp_uint64_t timeoutms_dcr)
{
	long total_bytes_read = 0, readbytes, conn_error = 0;
	mpxp_uint64_t endtime_response;
	struct fptdrive_timeoutmsg_s tos;
#ifdef MPXPLAY_DRVFTP_DEBUGFILE
	char sout[128];
#endif

	if(timeoutms_dcr) {
		endtime_response = pds_gettimem() + timeoutms_dcr;
		drvftp_message_timeout_init(&tos, endtime_response, "read retry %d sec ...");
	}
	if(!requested_bytes)
		requested_bytes = 1;
	do {
		readbytes = 0;
		drvftp_cmdctrl_read_respcode(ftpi, &(ftpi->socket_info_session), DRVFTP_RESPCNTR_INSTANT);
		if(ftpi->lastrespcode >= DRVFTP_RESPCODE_ERRORCODES)
			break;
		if(ftpi->lastrespcode != DRVFTP_RESPCODE_TRANSFER_COMPLETE) {
			readbytes = ftpi->lowfunc->bytes_buffered(ftpi, socketinfo_filehand);
			if(readbytes < 0) {
				conn_error = 1;
				break;
			}
			if(readbytes && timeoutms_dcr) {
				endtime_response = pds_gettimem() + timeoutms_dcr;
				drvftp_message_timeout_reset(&tos, endtime_response);
			}
		}
		if(readbytes || (ftpi->lastrespcode == DRVFTP_RESPCODE_TRANSFER_COMPLETE)) {
			readbytes = ftpi->lowfunc->receive(ftpi, socketinfo_filehand, bufptr, buflen);
			total_bytes_read += readbytes;
			if((ftpi->lastrespcode == DRVFTP_RESPCODE_TRANSFER_COMPLETE) || (total_bytes_read >= requested_bytes))
				break;
			if(readbytes >= buflen)
				break;
			buflen -= readbytes;
			bufptr += readbytes;
		}
		if(!total_bytes_read && (ftpi->lowfunc->socket_select(ftpi, socketinfo_filehand, DRVFTP_SOCKSELECT_MODE_READ) != 0)) {
			readbytes = ftpi->lowfunc->bytes_buffered(ftpi, socketinfo_filehand);
			if(readbytes <= 0) {
				conn_error = 1;
				break;
			}
		}
		if(!timeoutms_dcr || (pds_gettimem() > endtime_response))
			break;
		drvftp_message_timeout_write(&tos);
	} while(1);
	if(timeoutms_dcr)
		drvftp_message_timeout_close(&tos);

#ifdef MPXPLAY_DRVFTP_DEBUGFILE
	sprintf(sout, "read: rb:%d tbr:%d lr:%d ce:%d", requested_bytes, total_bytes_read, ftpi->lastrespcode, conn_error);
	drvftp_message_write_error(sout);
#endif

	return ((total_bytes_read || !conn_error) ? total_bytes_read : -1);
}

static int drvftp_dataconn_write(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_filehand, char *buf, unsigned int len)
{
	return ftpi->lowfunc->send(ftpi, socketinfo_filehand, buf, len);
}

static void drvftp_dataconn_close(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_filehand)
{
	if(socketinfo_filehand->socknum) {
		//if(ftpi->lastrespcode!=DRVFTP_RESPCODE_TRANSFER_COMPLETE)
		// drvftp_cmdctrl_send_command_check_respcode(ftpi,&(ftpi->socket_info_session),"ABOR",DRVFTP_RESPCODE_TRANSFER_ABORTED);
		ftpi->lowfunc->socket_close(ftpi, socketinfo_filehand, 0);
		if(ftpi->lastrespcode != DRVFTP_RESPCODE_TRANSFER_COMPLETE)
			drvftp_cmdctrl_check_respcode(ftpi, &(ftpi->socket_info_session), DRVFTP_RESPCNTR_NONE, DRVFTP_RESPCODE_TRANSFER_COMPLETE);	// !!! handles TRANSFER_ABORTED too by the general error checking, ??? when will this line fail?
		if(funcbit_test(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT))
			ftpdrive_session_connect(ftpi);
	}
}

static void drvftp_get_serverfeatures(struct ftpdrive_info_s *ftpi)
{
	unsigned long srv_features = 0;
	char featline[256];
	if(!drvftp_cmdctrl_send_command_check_respcode(ftpi, &(ftpi->socket_info_session), "FEAT", 211))
		return;
	do {
		long retcode;
		ftpdrive_feattable_s *ft = &ftpdrive_feature_table[0];
		if(!drvftp_cmdctrl_read_respline(ftpi, &(ftpi->socket_info_session), featline, sizeof(featline), 0))
			break;
		retcode = pds_atol(featline);
		if(retcode)				// have to be 211 (end of features)
			break;
		do {
			pds_strcutspc(featline);
			if(pds_strnicmp(featline, ft->featname, pds_strlen(ft->featname)) == 0)
				funcbit_enable(srv_features, ft->featflag);
			ft++;
		} while(ft->featflag);
	} while(1);
	funcbit_smp_value_put(ftpi->server_features, srv_features);
}

//-------------------------------------------------------------------------
static void drvftp_message_write_error(char *message)
{
	char sout[256];
	snprintf(sout, sizeof(sout), "FTP: %s", message);
	display_timed_message(sout);
#ifdef MPXPLAY_DRVFTP_DEBUGFILE
	if(debug_fp)
		fprintf(debug_fp, "%s\n", sout);
#endif
}

static void drvftp_message_timeout_init(struct fptdrive_timeoutmsg_s *tos, unsigned long endtime, char *msgmask)
{
	mpxp_uint64_t currtime = pds_gettimem();
	pds_memset(tos, 0, sizeof(*tos));
	tos->endtime_response = endtime;
	tos->begintime_disp = currtime + DRVFTP_DEFAULT_TIMEOUTMS_DISP;
	pds_strcpy(tos->msgmask, msgmask);
}

static void drvftp_message_timeout_reset(struct fptdrive_timeoutmsg_s *tos, unsigned long endtime)
{
	mpxp_uint64_t currtime = pds_gettimem();
	if(currtime > tos->begintime_disp)
		display_clear_timed_message();
	tos->endtime_response = endtime;
	tos->begintime_disp = currtime + DRVFTP_DEFAULT_TIMEOUTMS_DISP;
}

static void drvftp_message_timeout_write(struct fptdrive_timeoutmsg_s *tos)
{
	mpxp_uint64_t currtime = pds_gettimem(), disptime;
	char sout[256];
	if(currtime > tos->begintime_disp) {
		disptime = (unsigned long)((tos->endtime_response + 500 - currtime) / 1000);
		if(tos->lasttime_disp != disptime) {
			sprintf(sout, (const char *)tos->msgmask, disptime);
			drvftp_message_write_error(sout);
			tos->lasttime_disp = disptime;
		}
	}
#ifdef MPXPLAY_WIN32
	Sleep(0);
#endif
}

static void drvftp_message_timeout_close(struct fptdrive_timeoutmsg_s *tos)
{
	if(pds_gettimem() > tos->begintime_disp)
		display_clear_timed_message();
}

static long ftpdrive_drive_config(void *drive_data, unsigned long funcnum, void *argp1, void *argp2)
{
	struct ftpdrive_info_s *ftpi;
	struct ftpfile_info_s *ftfi;
	unsigned long i, j, k;
	char strtmp[1024], strtm2[64];

	switch (funcnum) {
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVOPENNAME:
		if(!argp1 || !argp2)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		j = DRVFTP_MAX_SESSIONS;
		k = 0;
		for(i = 0; i < DRVFTP_MAX_SESSIONS; i++) {
			ftpi = ftpdrives_info_ptrs[i];
			if(ftpi) {
				if(ftpi->connect_id_num > k) {
					k = ftpi->connect_id_num;
					j = i;
				}
			}
		}
		if(j < DRVFTP_MAX_SESSIONS) {
			ftpi = ftpdrives_info_ptrs[j];
			snprintf((char *)argp1, *((unsigned long *)argp2), "%s//%s%s%s%s%s%s",
					 ftpi->lowfunc->name,
					 ((ftpi->username[0]) ? ftpi->username : ""),
					 ((ftpi->username[0] && ftpi->password[0]) ? ":" : ""),
					 ((ftpi->password[0]) ? ftpi->password : ""), ((ftpi->username[0] && ftpi->password[0]) ? "@" : ""), ftpi->servername, ftpi->currremotedir_selected);
			return 1;
		}
		return 0;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIRROOT:
		if(!argp1)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		drvftp_str_localname_to_remote(strtmp, (char *)argp1);
		i = pds_strlen(strtmp);
		if(!i)
			return 1;
		if((strtmp[0] == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP) && (i == 1))	// '/'
			return 1;
		if((strtmp[0] == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP) && (strtmp[1] == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP) && (i == 2))	// '//'
			return 1;
		if((strtmp[1] == ':') && (i < sizeof(PDS_DIRECTORY_ROOTDIR_STR)))	// "d:/"
			return 1;
		if((strtmp[0] == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP) && (strtmp[2] == ':') && (i <= sizeof(PDS_DIRECTORY_ROOTDIR_STR)))	// "/d:/"
			return 1;
		return 0;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_CHKBUFBLOCKBYTES:
		return 4096;			// ???
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_PREREADBUFBYTES:
		return 65536;			// !!! ???
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_REALLYFULLPATH:
		if(!argp1 || !argp2)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		if(drive_data)
			break;
		drvftp_str_localname_to_remote((char *)argp1, (char *)argp2);
		return 1;
	}

	if(!drive_data)
		return MPXPLAY_DISKDRIV_CFGERROR_INVALID_DRIVE;
	ftpi = drive_data;
	ftfi = drive_data;

	switch (funcnum) {
		// ftpi
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_CMD_RESETDRIVE:
		if(ftpi->socket_info_session.socknum && (ftpi->connection_retry == DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION))
			drvftp_dircache_alldirs_dealloc(ftpi);
		ftpi->currremotedir_real[0] = 0;
		funcbit_smp_value_put(ftpi->connection_retry, DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION);
		return 1;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISFILESYSUNX:
		if(ftpi->system_type == DRVFTP_FTPDRIVEINFO_SYSTYPE_UNIX)
			return 1;
		else
			return 0;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_UTFTYPE:
		if((ftpi->flags & DRVFTP_FTPDRIVE_FLAG_USE_UTF8) && (ftpi->server_features & DRVFTP_FEATURE_UTF8))
			return MPXPLAY_DISKDRIVEDATA_UTFTYPE_UTF8;
		else
			return 0;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ISDIREXISTS:
		return ftpdrive_checkdir(ftpi, (char *)argp1);
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVLETTERSTR:
		if(!argp1 || !argp2)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		snprintf((char *)argp1, *((unsigned long *)argp2), "%d:", (ftpi->drivenum - DRVFTP_FIRSTDRV_VIRTUAL));
		return 1;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_DRVTYPENAME:
		if(!argp1 || !argp2)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		snprintf((char *)argp1, *((unsigned long *)argp2), "%s%s", ftpi->lowfunc->name, ftpi->servername);
		return 1;
	case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_REALLYFULLPATH:
		if(!argp1 || !argp2)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		drvftp_str_localname_to_remote(strtmp, (char *)argp2);
		if(ftpi->socket_info_session.portnum != ftpi->lowfunc->def_portnum)
			sprintf(strtm2, ":%d", ftpi->socket_info_session.portnum);
		else
			strtm2[0] = 0;
		sprintf((char *)argp1, "%s//%s%s%s%s%s%s%s%s", ftpi->lowfunc->name,
				ftpi->username, ((ftpi->password[0]) ? ":" : ""), ftpi->password,
				((ftpi->username[0]) ? "@" : ""), ftpi->servername, strtm2, ((strtmp[0] != PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP) ? PDS_DIRECTORY_SEPARATOR_STR_UNXFTP : ""), strtmp);
		return 1;
		/*case MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_ERRORLASTTEXT:
		   if(!argp1 || !argp2)
		   return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		   if(!ftpi->lastrespcode){
		   ((char *)argp1)[0]=0;
		   return 0;
		   }
		   pds_strncpy((char *)argp1,ftpi->lastresptext,*((unsigned long *)argp2));
		   ((char *)argp1)[*((unsigned long *)argp2)-1]=0;
		   return 1; */

		// ftfi
	case MPXPLAY_DISKFILE_CFGFUNCNUM_SET_FILEBLOCKSIZE:
		if(!argp1)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		funcbit_smp_value_put(ftfi->file_bufsize, (*((unsigned long *)argp1)));
		return 1;
	case MPXPLAY_DISKFILE_CFGFUNCNUM_SET_READWAIT:
		if(!argp1)
			return MPXPLAY_DISKDRIV_CFGERROR_ARGUMENTMISSING;
		if(*((unsigned long *)argp1))
			funcbit_enable(ftfi->flags, DRVFTP_FTPFILE_FLAG_READWAIT);
		else
			funcbit_disable(ftfi->flags, DRVFTP_FTPFILE_FLAG_READWAIT);
		return 1;
	}
	return MPXPLAY_DISKDRIV_CFGERROR_UNSUPPFUNC;
}

static struct ftpdrive_lowlevel_func_s *ftpdrive_drive_getlowfunc_by_name(char *name)
{
	struct ftpdrive_lowlevel_func_s **lowfunc = &ALL_lowlevel_funcs[0];
	do {
		if(pds_strlicmp(name, (*lowfunc)->name) == 0)
			return (*lowfunc);
		lowfunc++;
	} while(*lowfunc);
	return NULL;
}

static unsigned int ftpdrive_drive_check(char *pathname)
{
	if(ftpdrive_drive_getlowfunc_by_name(pathname))
		return 1;
	return 0;
}

static unsigned int ftpdrive_session_connect(struct ftpdrive_info_s *ftpi)
{
	char strtmp[MAX_PATHNAMELEN];

	funcbit_smp_value_put(ftpi->currremotedir_real[0], 0);
	funcbit_smp_disable(ftpi->flags, DRVFTP_FTPDRIVE_FLAG_RECONNECT);
	funcbit_smp_value_put(ftpi->file_open_id_num, DRVFTP_FILEOPENID_FREE);
	funcbit_smp_value_put(ftpi->lastrespcode, 0);
	funcbit_smp_value_put(ftpi->lastdatatype, 0);

	ftpi->lowfunc->socket_close(ftpi, &ftpi->socket_info_session, 1);
	ftpi->lowfunc->socket_close(ftpi, &ftpi->socket_info_filehand, 0);

	if(!ftpi->connection_retry)
		return 0;
	funcbit_smp_value_decrement(ftpi->connection_retry);
	ftpi->lastresptext[0] = 0;

	drvftp_message_write_error("getting ip adresses...");

	if(!ftpi->lowfunc->addressinfo_init(ftpi)) {
		drvftp_message_write_error("getting IP address(es) failed!");
		return 0;
	}

	drvftp_message_write_error("opening socket...");

	if(!ftpi->lowfunc->socket_open(ftpi, &ftpi->socket_info_session, 0)) {
		drvftp_message_write_error("couldn't open socket!");
		return 0;
	}
	drvftp_message_write_error("connecting to server...");
	pds_smp_memcpy(&ftpi->socket_info_session.conn_ip_addr, &ftpi->ip_remote, DRVFTP_IP_LEN);
	if(!ftpi->lowfunc->socket_connect(ftpi, &ftpi->socket_info_session)) {
		pds_strcpy(ftpi->lastresptext, "no connection to the server!");
		goto err_out_sc;
	}
	if(ftpi->lowfunc->socket_ssl_connect && !ftpi->lowfunc->socket_ssl_connect(ftpi, &ftpi->socket_info_session)) {
		pds_strcpy(ftpi->lastresptext, "SSL connect to server failed!");
		goto err_out_sc;
	}
	drvftp_message_write_error("waiting for welcome message...");
	if(drvftp_cmdctrl_read_respcode(ftpi, &ftpi->socket_info_session, DRVFTP_RESPCNTR_LONGWAIT) != 220) {
		if(!ftpi->lastrespcode)
			pds_strcpy(ftpi->lastresptext, "no server response (offline/down)!");
		goto err_out_sc;
	}
	if(ftpi->lowfunc->login_preprocess) {
		if(!ftpi->lowfunc->login_preprocess(ftpi, &ftpi->socket_info_session))
			goto err_out_sc;
	}
	drvftp_message_write_error("logging in...");
	if(ftpi->username[0]) {
		sprintf(strtmp, "USER %s", ftpi->username);
		if(!drvftp_cmdctrl_send_command_respcntr_check_respcode(ftpi, &ftpi->socket_info_session, strtmp, DRVFTP_RESPCNTR_LONGWAIT, 331))
			goto err_out_sc;
		if(ftpi->password[0]) {
			sprintf(strtmp, "PASS %s", ftpi->password);
			if(!drvftp_cmdctrl_send_command_respcntr_check_respcode(ftpi, &ftpi->socket_info_session, strtmp, DRVFTP_RESPCNTR_LONGWAIT, 230))
				goto err_out_sc;
		}
	}

	drvftp_message_write_error("login ok, opening...");

	strtmp[0] = 0;
	drvftp_cmdctrl_send_command_get_response(ftpi, &ftpi->socket_info_session, "SYST", strtmp, sizeof(strtmp), DRVFTP_RESPCNTR_NONE);
	if(pds_strnicmp(strtmp, "UNIX", 4) == 0)
		funcbit_smp_value_put(ftpi->system_type, DRVFTP_FTPDRIVEINFO_SYSTYPE_UNIX);
	//else if(pds_strnicmp(strtmp,"Windows_NT",10)==0)
	// funcbit_smp_value_put(ftpi->system_type,DRVFTP_FTPDRIVEINFO_SYSTYPE_WINNT);

	drvftp_get_serverfeatures(ftpi);

	if(ftpi->server_features & DRVFTP_FEATURE_CLNT)
		drvftp_cmdctrl_send_command_get_success(ftpi, &ftpi->socket_info_session, "CLNT Mpxplay");

	if(ftpi->server_features & DRVFTP_FEATURE_UTF8)
		if(!drvftp_cmdctrl_send_command_check_respcode
		   (ftpi, &ftpi->socket_info_session, ((ftpi->flags & DRVFTP_FTPDRIVE_FLAG_USE_UTF8) ? "OPTS UTF8 ON" : "OPTS UTF8 OFF"), DRVFTP_RESPCODE_COMMAND_OK))
			funcbit_disable(ftpi->server_features, DRVFTP_FEATURE_UTF8);

	if(ftpi->socket_info_session.sslhand) {
		drvftp_cmdctrl_send_command_get_success(ftpi, &ftpi->socket_info_session, "PBSZ 0");
		drvftp_cmdctrl_send_command_get_success(ftpi, &ftpi->socket_info_session, "PROT P");
	}

	if(ftpi->currremotedir_selected[0])
		drvftp_cwd(ftpi, ftpi->currremotedir_selected);

	funcbit_smp_value_put(ftpi->connection_retry, DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION);

	display_clear_timed_message();

	return 1;

  err_out_sc:
	drvftp_message_write_error(ftpi->lastresptext);
	ftpi->lowfunc->socket_close(ftpi, &ftpi->socket_info_session, 1);
	return 0;
}

static unsigned int ftpdrive_extract_logininfos_from_path(struct ftpdrive_info_s *ftpi, char *pathname)
{
	char *hostname, *portnum, *username, *password, *dirname, strtmp[MAX_PATHNAMELEN];

	funcbit_smp_pointer_put(ftpi->lowfunc, ftpdrive_drive_getlowfunc_by_name(pathname));
	if(!ftpi->lowfunc)
		return 0;

	pathname += pds_strlen(ftpi->lowfunc->name);

	while(*pathname == '/')
		pathname++;

	pds_strcpy(strtmp, pathname);
	hostname = pds_strrchr(strtmp, DRVFTP_PATHSEPARATOR_USER);
	if(hostname) {
		*hostname++ = 0;
		username = &strtmp[0];
		password = pds_strchr(strtmp, DRVFTP_PATHSEPARATOR_PASSWORD);
		if(password)
			*password++ = 0;
		pds_strcpy(ftpi->username, username);
		pds_strcpy(ftpi->password, password);
	} else {
		hostname = &strtmp[0];
		if(mpxplay_diskdrive_drvftp_config & DRVFTP_FTPDRIVE_FLAG_USE_ANONYM_LOGIN) {
			pds_strcpy(ftpi->username, "anonymous");
			pds_strcpy(ftpi->password, "anybody@hotmail.com");
		}
	}

	dirname = pds_strchr(hostname, PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP);
	if(dirname) {
		pds_strcpy(ftpi->currremotedir_selected, dirname);
		*dirname = 0;
	}
	portnum = pds_strchr(hostname, DRVFTP_PATHSEPARATOR_PORTNUM);
	if(portnum) {
		*portnum++ = 0;
		funcbit_smp_value_put(ftpi->socket_info_session.portnum, pds_atol(portnum));
	}
	if(!ftpi->socket_info_session.portnum)
		funcbit_smp_value_put(ftpi->socket_info_session.portnum, ftpi->lowfunc->def_portnum);

	pds_strcpy(ftpi->servername, hostname);

	return 1;
}

static unsigned int ftpdrive_compare_logininfos(struct ftpdrive_info_s *ftpi1, struct ftpdrive_info_s *ftpi2)
{
	if(ftpi1->lowfunc != ftpi2->lowfunc)
		return 0;
	if(ftpi1->socket_info_session.portnum != ftpi2->socket_info_session.portnum)
		return 0;
	if(pds_stricmp(ftpi1->servername, ftpi2->servername) != 0)
		return 0;
	if(pds_stricmp(ftpi1->username, ftpi2->username) != 0)
		return 0;
	if(pds_stricmp(ftpi1->password, ftpi2->password) != 0)
		return 0;
	return 1;					// same
}

static void *ftpdrive_drive_connect(char *pathname)
{
	struct ftpdrive_info_s *ftpi;

	ftpi = (struct ftpdrive_info_s *)calloc(1, sizeof(*ftpi));
	if(!ftpi)
		return ftpi;

#ifdef MPXPLAY_DRVFTP_DEBUGFILE
	debug_fp = fopen(MPXPLAY_DRVFTP_DEBUGFILE, "w");
#endif

	if(drvftp_connectid_num >= 0x7fffffff)
		funcbit_smp_value_put(drvftp_connectid_num, 0);

	funcbit_smp_value_increment(drvftp_connectid_num);
	funcbit_smp_value_put(ftpi->connect_id_num, drvftp_connectid_num);
	funcbit_smp_value_put(ftpi->flags, mpxplay_diskdrive_drvftp_config);
	funcbit_smp_value_put(ftpi->connection_retry, DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION);

	if(!ftpdrive_extract_logininfos_from_path(ftpi, pathname))
		goto err_out_mount;

	if(!ftpi->lowfunc->global_init())
		goto err_out_mount;

	if(!ftpdrive_session_connect(ftpi))
		goto err_out_mount;

	return ftpi;

  err_out_mount:
	ftpdrive_drive_unmount(ftpi);
	return NULL;
}

static void *ftpdrive_drive_mount(char *pathname)
{
	unsigned int i;
	struct ftpdrive_info_s *ftpi = NULL, ftpi_infos;

	ftpi = ftpdrives_info_ptrs[DRVFTP_VIRTUALDRV_FTPFILE];
	if(ftpi) {					// closing ftp-file session, using disk-session if they match
		pds_memset(&ftpi_infos, 0, sizeof(ftpi_infos));
		if(ftpdrive_extract_logininfos_from_path(&ftpi_infos, pathname))
			if(ftpdrive_compare_logininfos(&ftpi_infos, ftpi))
				ftpdrive_drive_unmount(ftpi);
	}

	for(i = 0; i < DRVFTP_MAX_VIRTUAL_DRIVES; i++)
		if(!ftpdrives_info_ptrs[i])
			break;

	if(i >= DRVFTP_MAX_VIRTUAL_DRIVES)
		return ftpi;

	ftpi = ftpdrive_drive_connect(pathname);
	if(!ftpi)
		return ftpi;

	funcbit_smp_value_put(ftpi->drivenum, (DRVFTP_FIRSTDRV_VIRTUAL + i));
	funcbit_smp_pointer_put(ftpdrives_info_ptrs[i], ftpi);
	funcbit_smp_pointer_put(ftpi->ftpdrive_info_ptr, &ftpdrives_info_ptrs[i]);

	return ftpi;
}

static void ftpdrive_drive_close(struct ftpdrive_info_s *ftpi)
{
	if(ftpi) {
		ftpi->lowfunc->socket_close(ftpi, &ftpi->socket_info_filehand, 1);
		ftpi->lowfunc->socket_close(ftpi, &ftpi->socket_info_session, 1);
		drvftp_dircache_alldirs_dealloc(ftpi);
		if(ftpi->ftpdrive_info_ptr)
			funcbit_smp_pointer_put(*(ftpi->ftpdrive_info_ptr), NULL);
		free(ftpi);
	}
}

static void ftpdrive_drive_unmount(void *drivehand_data)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	if(ftpi) {
		ftpdrive_drive_close(ftpi);
	} else {					// all flush / global deinit
		struct ftpdrive_lowlevel_func_s **lowfunc;
		unsigned int i;
		for(i = 0; i < DRVFTP_MAX_SESSIONS; i++) {
			ftpi = ftpdrives_info_ptrs[i];
			if(ftpi) {
				ftpdrive_drive_close(ftpi);
				funcbit_smp_pointer_put(ftpdrives_info_ptrs[i], NULL);
			}
		}
		lowfunc = &ALL_lowlevel_funcs[0];
		do {
			if((*lowfunc)->global_deinit)
				(*lowfunc)->global_deinit();
			lowfunc++;
		} while(*lowfunc);
	}
#ifdef MPXPLAY_DRVFTP_DEBUGFILE
	if(debug_fp)
		fclose(debug_fp);
#endif
}

static void drvftp_str_localname_to_remote(char *remotename, char *pathname)
{
	int drivenum = pds_getdrivenum_from_path(pathname);
	if(drivenum >= 0)
		pathname += sizeof(PDS_DIRECTORY_DRIVE_STR) - 1;
	pds_strcpy(remotename, pathname);
#if (PDS_DIRECTORY_SEPARATOR_CHAR!=PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)
	while(*remotename) {		// convert '\' to '/'
		if(*remotename == PDS_DIRECTORY_SEPARATOR_CHAR)
			*remotename = PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP;
		remotename++;
	}
#endif
}

static void drvftp_str_remotename_to_local(struct ftpdrive_info_s *ftpi, char *localname, char *remotename, unsigned int buflen)
{
	if(*remotename == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)	// skip '/'
		remotename++;

	snprintf(localname, buflen, "%d:%c%s", (ftpi->drivenum - DRVFTP_FIRSTDRV_VIRTUAL), PDS_DIRECTORY_SEPARATOR_CHAR, remotename);
#if (PDS_DIRECTORY_SEPARATOR_CHAR!=PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)
	while(*localname) {			// convert '/' to '\'
		if(*localname == PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP)
			*localname = PDS_DIRECTORY_SEPARATOR_CHAR;
		localname++;
	}
#endif
}

static void ftpdrive_close_and_lock_filehand(struct ftpdrive_info_s *ftpi)
{
	drvftp_dataconn_close(ftpi, &ftpi->socket_info_filehand);
	funcbit_smp_value_put(ftpi->file_open_id_num, DRVFTP_FILEOPENID_BUSY);
}

static void ftpdrive_unlock_filehand(struct ftpdrive_info_s *ftpi)
{
	if(ftpi->file_open_id_num == DRVFTP_FILEOPENID_BUSY)
		funcbit_smp_value_put(ftpi->file_open_id_num, DRVFTP_FILEOPENID_FREE);
}

//-----------------------------------------------------------------------
static unsigned int ftpdrive_findnext(void *drivehand_data, struct pds_find_t *ffblk);

#define DRVFTP_LISTLINE_MAXPARTS_WINNT 4
#define DRVFTP_LISTLINE_MAXPARTS_UNIX  9

static char *strMon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL
};

static unsigned int drvftp_str_listline_slice(char *listline, char **parts, unsigned int maxparts)
{
	unsigned int partnum = 0;
	do {
		char *next;
		while(*listline == ' ')
			listline++;
		if(!listline[0])
			break;
		parts[partnum++] = listline;
		if(partnum >= maxparts)
			break;
		next = pds_strchr(listline, ' ');
		if(!next)
			break;
		*next++ = 0;
		listline = next;
	} while(1);
	return partnum;
}

static unsigned int drvftp_str_listline_to_direntry_winnt(char *listline, struct pds_find_t *ffblk)
{
	char *parts[DRVFTP_LISTLINE_MAXPARTS_WINNT + 1], linetmp[MAX_PATHNAMELEN * 2 + 8];

	pds_strcpy(linetmp, listline);

	if(drvftp_str_listline_slice(linetmp, &parts[0], DRVFTP_LISTLINE_MAXPARTS_WINNT) < DRVFTP_LISTLINE_MAXPARTS_WINNT)
		return 0;

	if((parts[0][2] != '-') || (parts[0][5] != '-') || (parts[1][2] != ':'))
		return 0;

	ffblk->fdate.month = pds_atol(&parts[0][0]);
	ffblk->fdate.day = pds_atol(&parts[0][3]);
	ffblk->fdate.year = pds_atol(&parts[0][6]);
	if(!ffblk->fdate.month || (ffblk->fdate.month > 12) || !ffblk->fdate.day || (ffblk->fdate.day > 31) || (ffblk->fdate.year > 99))
		return 0;
	if(ffblk->fdate.year >= 80)
		ffblk->fdate.year -= 80;	// 1980 -> 0  (+1980)
	else
		ffblk->fdate.year += 20;	// 2000 -> 20 (+1980)

	ffblk->fdate.hours = pds_atol(&parts[1][0]);
	ffblk->fdate.minutes = pds_atol(&parts[1][3]);
	if(pds_strnicmp(&parts[1][5], "PM", 2) == 0)
		ffblk->fdate.hours += 12;

	if(pds_stricmp(parts[2], "<DIR>") == 0)
		funcbit_enable(ffblk->attrib, _A_SUBDIR);
	else
		ffblk->size = pds_atol(parts[2]);

	pds_strncpy(ffblk->name, parts[3], sizeof(ffblk->name) - 1);	// filename
	ffblk->name[sizeof(ffblk->name) - 1] = 0;

	return 1;
}

static unsigned int drvftp_str_listline_to_direntry_unix(char *listline, struct pds_find_t *ffblk)
{
	unsigned int i, m, partnum;
	char *lp, *end, *parts[DRVFTP_LISTLINE_MAXPARTS_UNIX + 1], linetmp[MAX_PATHNAMELEN * 2 + 8];

	pds_strcpy(linetmp, listline);

	partnum = drvftp_str_listline_slice(linetmp, &parts[0], DRVFTP_LISTLINE_MAXPARTS_UNIX - 1);
	if(partnum < (DRVFTP_LISTLINE_MAXPARTS_UNIX - 1))
		return 0;

	if(parts[0][0] == 'd')
		funcbit_enable(ffblk->attrib, _A_SUBDIR);
	//else if(parts[0][0]!='-') // ???
	// return 0;          //
	if(parts[0][2] != 'w')
		funcbit_enable(ffblk->attrib, _A_RDONLY);

	for(m = 4; m <= 5; m++) {
		i = 0;
		do {
			if(pds_stricmp(parts[m], strMon[i]) == 0) {
				ffblk->fdate.month = i + 1;
				break;
			}
			i++;
		} while(strMon[i]);
		if(ffblk->fdate.month)
			break;
	}
	if(!ffblk->fdate.month)
		return 0;

	if(m == 5)
		drvftp_str_listline_slice(parts[m + 2], &parts[m + 2], 2);

	ffblk->size = pds_atoi64(parts[m - 1]);
	ffblk->fdate.day = pds_atol(parts[m + 1]);

	if(parts[m + 2][2] == ':') {	// time of file (hh:mm) in current year
		ffblk->fdate.hours = pds_atol(parts[m + 2]);
		ffblk->fdate.minutes = pds_atol(&parts[m + 2][3]);
		ffblk->fdate.year = (pds_getdate() >> 16) - 1980;
	} else {					// year of file
		i = pds_atol(parts[m + 2]);
		if(i >= 1980 && i <= 9999)
			i -= 1980;
		ffblk->fdate.year = i;
	}

	lp = parts[m + 3];
	end = pds_strstr(lp, " ->");	// link
	if(end)
		*end = 0;

	while(*lp == ' ')			// skip spaces
		lp++;
	pds_strncpy(ffblk->name, lp, sizeof(ffblk->name) - 1);	// filename
	ffblk->name[sizeof(ffblk->name) - 1] = 0;
	return 1;
}

// !!! date is GMT! (correct it)
static unsigned int drvftp_str_mlsdline_to_direntry(char *listline, struct pds_find_t *ffblk)
{
	unsigned int year, month, day, hour, minute, second;
	char *end;

	do {
		char *data, *next;
		next = pds_strchr(listline, ';');
		if(!next)
			break;
		*next++ = 0;
		data = pds_strchr(listline, '=');
		if(data) {
			*data++ = 0;
			if(pds_stricmp(listline, "type") == 0) {	// dir or file
				if(pds_stricmp(data, "dir") == 0)
					funcbit_enable(ffblk->attrib, _A_SUBDIR);
			} else if(pds_stricmp(listline, "modify") == 0) {	// !!! date in GMT (need to convert to local time)
				year = month = day = hour = minute = second = 0;
				sscanf(data, "%4d%2d%2d%2d%2d%2d", &year, &month, &day, &hour, &minute, &second);
				if(year >= 1980 && year <= 9999)
					year -= 1980;
				ffblk->fdate.year = year;
				ffblk->fdate.month = month;
				ffblk->fdate.day = day;
				ffblk->fdate.hours = hour;
				ffblk->fdate.minutes = minute;
				ffblk->fdate.twosecs = second / 2;
			} else if(pds_stricmp(listline, "size") == 0) {	// filesize
				ffblk->size = pds_atol(data);
			}
		}
		listline = next;
	} while(1);

	if(!ffblk->fdate.month)
		return 0;

	end = pds_strstr(listline, " ->");	// link
	if(end)
		*end = 0;

	while(*listline == ' ')		// skip spaces
		listline++;
	pds_strncpy(ffblk->name, listline, sizeof(ffblk->name) - 1);	// filename
	ffblk->name[sizeof(ffblk->name) - 1] = 0;
	return 1;
}

static unsigned int ftpdrive_findfirst(void *drivehand_data, char *pathname, unsigned int attrib, struct pds_find_t *ffblk)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	struct ftpdrive_directory_info_s *diri;
	struct ftpdrive_filefind_s *ff_data;
	struct ftpdrive_socket_info_s socketinfo_ff;
	unsigned int retcode = 1, datapos = 0;	//,firstread=1;
	char *fn, remotepath[MAX_PATHNAMELEN];
	char remotename[MAX_PATHNAMELEN], listline[MAX_PATHNAMELEN * 2];
#ifdef MPXPLAY_DRVFTP_DEBUGFILE
	char sout[MAX_PATHNAMELEN];
#endif

	pds_memset((char *)(&socketinfo_ff), 0, sizeof(socketinfo_ff));

	ff_data = (struct ftpdrive_filefind_s *)malloc(sizeof(*ff_data));
	if(!ff_data)
		return retcode;

	drvftp_str_localname_to_remote(remotename, pathname);
	fn = drvftp_str_getpath_from_fullname(remotepath, remotename);
	if(fn)
		pds_strcpy(ff_data->searchfilemask, fn);
	else						// should not happen
		pds_strcpy(ff_data->searchfilemask, PDS_DIRECTORY_ALLFILE_STR);

	diri = drvftp_dircache_dir_searchby_name(ftpi, remotepath);
	if(!diri) {
		ftpdrive_close_and_lock_filehand(ftpi);
		if(drvftp_cwd(ftpi, remotepath) != 0)
			goto err_out_ff;
		drvftp_dataconn_open(ftpi, NULL, ((ftpi->server_features & DRVFTP_FEATURE_MLSD) ? "MLSD" : "LIST"), remotepath, &socketinfo_ff, DRVFTP_DATATYPE_ASCII);
		if(!socketinfo_ff.socknum)
			goto err_out_ff;
		diri = drvftp_dircache_dir_realloc(ftpi, diri, remotepath);
		if(!diri)
			goto err_out_ff;
		ffblk->name[0] = 0;
		do {
			struct ftpdrive_direntry_info_s *ed;
			long bytes_read, linelen = 0;
			if(datapos) {
				char *r, *end = pds_strchr(listline, '\n');
				if(end) {
					*end++ = 0;
					r = pds_strchr(listline, '\r');
					if(r)
						*r = 0;
					linelen = end - &listline[0];
					pds_memset(ffblk, 0, sizeof(*ffblk));
					if(ftpi->server_features & DRVFTP_FEATURE_MLSD)
						drvftp_str_mlsdline_to_direntry(listline, ffblk);
					else if(!drvftp_str_listline_to_direntry_winnt(listline, ffblk))
						drvftp_str_listline_to_direntry_unix(listline, ffblk);
#ifdef MPXPLAY_DRVFTP_DEBUGFILE
					sprintf(sout, "c:\"%s\" dp:%d ln:%d", listline, datapos, linelen);
					drvftp_message_write_error(sout);
#endif
					pds_strcpy(listline, end);
					datapos -= linelen;
					if(!ffblk->name[0])
						continue;
					ed = drvftp_dircache_entry_alloc(diri, ffblk->name);
					if(!ed)
						break;
					ed->attrib = ffblk->attrib;
					ed->filesize = ffblk->size;
					pds_memcpy(&ed->fdate, &ffblk->fdate, sizeof(ed->fdate));
					continue;
				}
			}
			if(datapos >= (sizeof(listline) - 1))
				datapos = 0;
			bytes_read = drvftp_dataconn_read(ftpi, &socketinfo_ff, &listline[datapos], sizeof(listline) - 1 - datapos, 0, DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE);
			//bytes_read=drvftp_dataconn_read(ftpi,&socketinfo_ff,&listline[datapos],sizeof(listline)-1-datapos,0,((datapos || firstread)? DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE:DRVFTP_DEFAULT_TIMEOUTMS_EOF)); // !!!
			if(bytes_read < 0)
				break;
			datapos += bytes_read;
			if(!datapos)
				break;
			listline[datapos] = 0;
			//firstread=0;
		} while(1);
	}

	if(!diri->entrydatas || !diri->cached_entries_num)
		goto err_out_ff;
	ff_data->entrynum_curr = 0;
	ff_data->entrynum_end = diri->cached_entries_num;
	ff_data->entry = diri->entrydatas;
	ffblk->ff_data = ff_data;

	if(ftpdrive_findnext(ftpi, ffblk))
		goto err_out_ff;

	retcode = 0;
	goto end_ff;

  err_out_ff:
	if(ff_data)
		free(ff_data);
	ffblk->ff_data = NULL;
  end_ff:
	drvftp_dataconn_close(ftpi, &socketinfo_ff);
	ftpdrive_unlock_filehand(ftpi);
	return retcode;
}

static unsigned int ftpdrive_findnext(void *drivehand_data, struct pds_find_t *ffblk)
{
	struct ftpdrive_filefind_s *ff_data = ffblk->ff_data;
	unsigned int retcode = 1;
	if(!ff_data)
		return retcode;
	do {
		if(ff_data->entrynum_curr >= ff_data->entrynum_end)
			break;
		if(ff_data->entry->filename && pds_filename_wildchar_cmp(ff_data->entry->filename, ff_data->searchfilemask)) {
			drvftp_dircache_entry_copyto_ffblk(ffblk, ff_data->entry);
			retcode = 0;
		}
		ff_data->entrynum_curr++;
		ff_data->entry++;
	} while(retcode);
	return retcode;
}

static void ftpdrive_findclose(void *drivehand_data, struct pds_find_t *ffblk)
{
	if(ffblk->ff_data) {
		free(ffblk->ff_data);
		ffblk->ff_data = NULL;
	}
}

static unsigned int ftpdrive_checkdir(void *drivehand_data, char *dirname)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	struct ftpdrive_directory_info_s *diri;
	struct ftpdrive_direntry_info_s *ed;
	char remotename[MAX_PATHNAMELEN];
	drvftp_str_localname_to_remote(remotename, dirname);
	if(pds_stricmp(remotename, ftpi->currremotedir_real) == 0)
		return 1;
	diri = drvftp_dircache_dir_searchby_name(ftpi, remotename);
	if(diri)
		return 1;
	ed = drvftp_dircache_entry_searchby_fullname(ftpi, remotename);
	if(ed) {
		if(funcbit_test(ed->attrib, _A_SUBDIR))
			return 1;
		return 0;
	}
	if(drvftp_cwd(ftpi, remotename) == 0)
		return 1;
	return 0;
}

static unsigned int drvftp_getcurrdir_real(struct ftpdrive_info_s *ftpi)
{
	unsigned int retcode = 0, buflen = sizeof(ftpi->currremotedir_real);
	char *remotepath = alloca(buflen + 1);
	if(!remotepath)
		return retcode;
	ftpdrive_close_and_lock_filehand(ftpi);
	*remotepath = 0;
	if(drvftp_cmdctrl_arc_send_command_get_response(ftpi, &ftpi->socket_info_session, "PWD", remotepath, buflen, DRVFTP_RESPCNTR_NONE) == DRVFTP_RESPCODE_CMD_PWD_OK) {
		char *s = pds_strchr(remotepath, '\"');	// search for "dirname"
		if(s) {
			pds_strcpy(remotepath, s + 1);
			s = pds_strchr(remotepath, '\"');
			if(s)
				*s = 0;
		}
		pds_strcpy(ftpi->currremotedir_real, remotepath);
		retcode = 1;
	}
	ftpdrive_unlock_filehand(ftpi);
	return retcode;
}

static char *ftpdrive_getcwd(void *drivehand_data, char *localpathbuf, unsigned int buflen)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	*localpathbuf = 0;
	if(ftpi->currremotedir_selected[0])
		drvftp_str_remotename_to_local(ftpi, localpathbuf, ftpi->currremotedir_selected, buflen);
	else {
		if(drvftp_getcurrdir_real(ftpi)) {
			pds_strcpy(ftpi->currremotedir_selected, ftpi->currremotedir_real);
			drvftp_str_remotename_to_local(ftpi, localpathbuf, ftpi->currremotedir_real, buflen);
		}
	}
	return localpathbuf;
}

static int drvftp_cwd(struct ftpdrive_info_s *ftpi, char *remotename)
{
	int retcode = -1;
	char command[MAX_PATHNAMELEN + 8];
	if(pds_stricmp(remotename, ftpi->currremotedir_real) == 0)
		retcode = 0;
	else {
		ftpdrive_close_and_lock_filehand(ftpi);
		snprintf(command, sizeof(command), "CWD %s", remotename);
		if(drvftp_cmdctrl_arc_send_command_check_respcode(ftpi, &ftpi->socket_info_session, command, DRVFTP_RESPCODE_DIRCOMMAND_OK)) {
			pds_strcpy(ftpi->currremotedir_real, remotename);
			retcode = 0;
		}
		ftpdrive_unlock_filehand(ftpi);
	}
	return retcode;
}

static int ftpdrive_chdir(void *drivehand_data, char *path)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	struct ftpdrive_directory_info_s *diri;
	int retcode = -1;
	char remotename[MAX_PATHNAMELEN];
	ftpi->connection_retry = DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION;
	drvftp_str_localname_to_remote(remotename, path);
	diri = drvftp_dircache_dir_searchby_name(ftpi, remotename);
	if(diri)
		retcode = 0;
	else
		retcode = drvftp_cwd(ftpi, remotename);
	if(retcode == 0)
		pds_strcpy(ftpi->currremotedir_selected, remotename);
	else
		drvftp_message_write_error(ftpi->lastresptext);
	return retcode;
}

static int ftpdrive_mkdir(void *drivehand_data, char *path)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	int retcode = -1;
	char remotename[MAX_PATHNAMELEN], command[MAX_PATHNAMELEN + 8];
	ftpi->connection_retry = DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION;
	ftpdrive_close_and_lock_filehand(ftpi);
	drvftp_str_localname_to_remote(remotename, path);
	snprintf(command, sizeof(command), "MKD %s", remotename);
	if(drvftp_cmdctrl_arc_send_command_get_success(ftpi, &ftpi->socket_info_session, command)) {
		struct ftpdrive_direntry_info_s *edn;
		edn = drvftp_dircache_entry_addby_fullname(ftpi, remotename);
		if(edn)
			edn->attrib = _A_SUBDIR;
		retcode = 0;
	} else
		drvftp_message_write_error(ftpi->lastresptext);
	ftpdrive_unlock_filehand(ftpi);
	return retcode;
}

static int ftpdrive_rmdir(void *drivehand_data, char *path)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	int retcode = -1;
	char remotename[MAX_PATHNAMELEN], command[MAX_PATHNAMELEN + 8];
	ftpi->connection_retry = DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION;
	ftpdrive_close_and_lock_filehand(ftpi);
	drvftp_str_localname_to_remote(remotename, path);
	if(pds_stricmp(remotename, ftpi->currremotedir_real) == 0) {
		if(!drvftp_cmdctrl_arc_send_command_get_success(ftpi, &ftpi->socket_info_session, "CDUP"))
			goto err_out_rmdir;
		drvftp_getcurrdir_real(ftpi);
	}
	snprintf(command, sizeof(command), "RMD %s", remotename);
	if(drvftp_cmdctrl_arc_send_command_get_success(ftpi, &ftpi->socket_info_session, command)) {
		struct ftpdrive_directory_info_s *diri;
		drvftp_dircache_entry_removeby_fullname(ftpi, remotename);
		diri = drvftp_dircache_dir_searchby_name(ftpi, remotename);
		if(diri)
			drvftp_dircache_dir_dealloc(diri);
		retcode = 0;
	}
  err_out_rmdir:
	ftpdrive_unlock_filehand(ftpi);
	if(retcode != 0)
		drvftp_message_write_error(ftpi->lastresptext);
	return retcode;
}

static int ftpdrive_rename(void *drivehand_data, char *oldfilename, char *newfilename)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	int retcode = -1;
	char oldremotename[MAX_PATHNAMELEN], newremotename[MAX_PATHNAMELEN], command[MAX_PATHNAMELEN + 8];
	ftpi->connection_retry = DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION;
	ftpdrive_close_and_lock_filehand(ftpi);
	drvftp_str_localname_to_remote(oldremotename, oldfilename);
	snprintf(command, sizeof(command), "RNFR %s", oldremotename);
	if(drvftp_cmdctrl_arc_send_command_get_success(ftpi, &ftpi->socket_info_session, command)) {
		drvftp_str_localname_to_remote(newremotename, newfilename);
		snprintf(command, sizeof(command), "RNTO %s", newremotename);
		if(drvftp_cmdctrl_arc_send_command_get_success(ftpi, &ftpi->socket_info_session, command)) {
			struct ftpdrive_direntry_info_s *edo, *edn;
			edo = drvftp_dircache_entry_removeby_fullname(ftpi, oldremotename);
			edn = drvftp_dircache_entry_addby_fullname(ftpi, newremotename);
			if(edo && edn) {
				edn->attrib = edo->attrib;
				edn->filesize = edo->filesize;
				pds_memcpy(&edn->fdate, &edo->fdate, sizeof(edo->fdate));
			}
			retcode = 0;
		}
	}
	ftpdrive_unlock_filehand(ftpi);
	if(retcode != 0)
		drvftp_message_write_error(ftpi->lastresptext);
	return retcode;
}

static int ftpdrive_unlink(void *drivehand_data, char *filename)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	int retcode = -1;
	char remotename[MAX_PATHNAMELEN], command[MAX_PATHNAMELEN + 8];
	ftpi->connection_retry = DRVFTP_DEFAULT_TIMEOUTRETRY_SESSION;
	ftpdrive_close_and_lock_filehand(ftpi);
	drvftp_str_localname_to_remote(remotename, filename);
	snprintf(command, sizeof(command), "DELE %s", remotename);
	if(drvftp_cmdctrl_arc_send_command_get_success(ftpi, &ftpi->socket_info_session, command)) {
		drvftp_dircache_entry_removeby_fullname(ftpi, remotename);
		retcode = 0;
	} else
		drvftp_message_write_error(ftpi->lastresptext);
	ftpdrive_unlock_filehand(ftpi);
	return retcode;
}

//---------------------------------------------------------------------
// file local routines

static long drvftp_openid_num;

static struct ftpdrive_info_s *ftpdrive_file_session_connect(void *drivehand_data, struct ftpfile_info_s *ftfi)
{
	struct ftpdrive_info_s *ftpi = NULL;
	int i;
	char *fn, path[MAX_PATHNAMELEN];

	if(drivehand_data) {		// file is connected to an opened session
		ftpi = drivehand_data;
		ftfi->ftpi_ftpdrives_ptr = ftpi->ftpdrive_info_ptr;
		pds_memcpy(&ftfi->ftpi_infos, ftpi, sizeof(ftfi->ftpi_infos));
	} else {					// search or open a session for the file
		if(ftfi->singleftpfilename[0]) {	// session has opened by a single ftp:// filename
			drvftp_str_localname_to_remote(path, ftfi->singleftpfilename);
			fn = pds_strrchr(path, '/');
			if(!fn)
				return NULL;
			*fn++ = 0;			// cut filename from path
			if(!ftpdrive_extract_logininfos_from_path(&ftfi->ftpi_infos, path))
				return NULL;
			i = pds_strcpy(&ftfi->remotefilename[0], ftfi->ftpi_infos.currremotedir_selected);
			i += pds_strcpy(&ftfi->remotefilename[i], PDS_DIRECTORY_SEPARATOR_STR_UNXFTP);
			pds_strcat(&ftfi->remotefilename[i], fn);	// assemble again fullname (without drive letter)
		}
		ftfi->ftpi_ftpdrives_ptr = NULL;
		for(i = 0; i < (DRVFTP_MAX_VIRTUAL_DRIVES + 1); i++) {	// search for an existent session by login infos
			ftpi = ftpdrives_info_ptrs[i];
			if(ftpi && ftpdrive_compare_logininfos(&ftfi->ftpi_infos, ftpi)) {
				ftfi->ftpi_ftpdrives_ptr = &ftpdrives_info_ptrs[i];
				pds_strcpy(ftpi->currremotedir_selected, ftfi->ftpi_infos.currremotedir_selected);
				break;
			}
		}
		if(!ftfi->ftpi_ftpdrives_ptr && ftfi->singleftpfilename[0]) {	// open a new (hidden) session for the single ftp:// file
			if(ftpdrives_info_ptrs[DRVFTP_VIRTUALDRV_FTPFILE])
				ftpdrive_drive_unmount(ftpdrives_info_ptrs[DRVFTP_VIRTUALDRV_FTPFILE]);	// close it if it's open (but has changed)
			ftpi = ftpdrive_drive_connect(path);
			if(!ftpi)
				return ftpi;
			funcbit_smp_pointer_put(ftpdrives_info_ptrs[DRVFTP_VIRTUALDRV_FTPFILE], ftpi);
			funcbit_smp_pointer_put(ftpi->ftpdrive_info_ptr, &ftpdrives_info_ptrs[DRVFTP_VIRTUALDRV_FTPFILE]);
			ftfi->ftpi_ftpdrives_ptr = ftpi->ftpdrive_info_ptr;
		}
	}
	return ftpi;
}

static unsigned int ftpdrive_file_data_connect(struct ftpfile_info_s *ftfi)
{
	struct ftpdrive_info_s *ftpi = (ftfi->ftpi_ftpdrives_ptr) ? *(ftfi->ftpi_ftpdrives_ptr) : NULL;
	unsigned int datatype;

	if(!ftpi)
		return 0;
	if(ftpi->file_open_id_num == DRVFTP_FILEOPENID_BUSY)	// !!! smp (multithread) (-xr)
		return 0;
	if(!funcbit_test(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_WRITE))
		if(ftfi->filesize && (ftfi->filepos >= ftfi->filesize))	// eof
			return 0;

	drvftp_dataconn_close(ftpi, &ftpi->socket_info_filehand);
	funcbit_smp_disable(ftfi->flags, DRVFTP_FTPFILE_FLAG_SEEK);
	ftfi->timeout_at_read = 0;

	if(funcbit_test(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_TEXT))
		datatype = DRVFTP_DATATYPE_ASCII;
	else
		datatype = DRVFTP_DATATYPE_BINARY;

	if(funcbit_test(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_WRITE))
		drvftp_dataconn_open(ftpi, ftfi, "STOR", ftfi->remotefilename, &ftpi->socket_info_filehand, datatype);
	else {
		funcbit_smp_value_put(ftpi->socket_bufsize, ftfi->file_bufsize);
		drvftp_dataconn_open(ftpi, ftfi, "RETR", ftfi->remotefilename, &ftpi->socket_info_filehand, datatype);
	}
	funcbit_smp_value_put(ftpi->file_open_id_num, ftfi->open_id_num);
	if(!ftpi->socket_info_filehand.socknum)
		return 0;
	return 1;
}

static unsigned int ftpdrive_file_sessiondata_reconnect(struct ftpfile_info_s *ftfi)
{
	struct ftpdrive_info_s *ftpi = (ftfi->ftpi_ftpdrives_ptr) ? *(ftfi->ftpi_ftpdrives_ptr) : NULL;
	if(!ftpi || (ftfi->connect_id_num != ftpi->connect_id_num) || !ftpdrive_compare_logininfos(&ftfi->ftpi_infos, ftpi)) {
		ftpi = ftpdrive_file_session_connect(NULL, ftfi);
		if(!ftpi)
			return 0;
		ftfi->connect_id_num = ftpi->connect_id_num;
	}
	if((ftfi->flags & DRVFTP_FTPFILE_FLAG_SEEK) || (ftpi->file_open_id_num != ftfi->open_id_num))
		if(!ftpdrive_file_data_connect(ftfi))
			return 0;
	return 1;
}

//----------------------------------
// file api
static void ftpdrive_file_close(void *filehand_data);

static unsigned int ftpdrive_file_check(void *drivehand_data, char *filename)
{
	struct ftpdrive_info_s *ftpi = drivehand_data;
	if(ftpi) {
		int drivenum = pds_getdrivenum_from_path(filename);
		if(drivenum == ftpi->drivenum)
			return 1;
	} else {
		if(ftpdrive_drive_getlowfunc_by_name(filename))
			return 1;
	}
	return 0;
}

static void *ftpdrive_file_open(void *drivehand_data, char *filename, unsigned long openmode)
{
	struct ftpdrive_info_s *ftpi;
	struct ftpfile_info_s *ftfi;

	ftfi = calloc(1, sizeof(*ftfi));
	if(!ftfi)
		return ftfi;

	if(drivehand_data)
		drvftp_str_localname_to_remote(ftfi->remotefilename, filename);
	else
		pds_strcpy(ftfi->singleftpfilename, filename);	// single ftp:// file

	ftpi = ftpdrive_file_session_connect(drivehand_data, ftfi);
	if(!ftpi)
		goto err_out_open;

	funcbit_enable(ftfi->flags, DRVFTP_FTPFILE_FLAG_READWAIT);

	switch (openmode & (O_RDONLY | O_RDWR | O_WRONLY)) {
	case O_RDONLY:
		funcbit_enable(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_READ);
		break;
	case O_WRONLY:
		funcbit_enable(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_WRITE);
		break;
	default:
		goto err_out_open;		// !!! O_RDWR not supported by FTP commands (?)
	}
	if(openmode & O_CREAT)
		funcbit_enable(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_CREATE);
	//if(openmode&O_TEXT)
	// funcbit_enable(ftfi->opentype,DRVFTP_FTPFILE_OPENTYPE_TEXT); // doesn't work (ftp server doesn't convert lf to cr/lf?)

	ftfi->connect_id_num = ftpi->connect_id_num;
	if(drvftp_openid_num >= 0x7fffffff)
		drvftp_openid_num = 0;
	ftfi->open_id_num = ++drvftp_openid_num;	// !!! begins with 1

	if(!ftpdrive_file_data_connect(ftfi))
		goto err_out_open;

	return ftfi;

  err_out_open:
	ftpdrive_file_close(ftfi);
	return NULL;
}

static void ftpdrive_file_close(void *filehand_data)
{
	struct ftpfile_info_s *ftfi = filehand_data;
	struct ftpdrive_info_s *ftpi;
	if(ftfi) {
		ftpi = (ftfi->ftpi_ftpdrives_ptr) ? *(ftfi->ftpi_ftpdrives_ptr) : NULL;
		if(ftpi) {
			if(ftpi->file_open_id_num == ftfi->open_id_num)
				drvftp_dataconn_close(ftpi, &ftpi->socket_info_filehand);
			if(funcbit_test(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_WRITE) && ftfi->filesize && ftpdrive_compare_logininfos(ftpi, &ftfi->ftpi_infos)) {
				struct ftpdrive_direntry_info_s *ed;
				ed = drvftp_dircache_entry_searchby_fullname(ftpi, ftfi->remotefilename);
				if(!ed)
					ed = drvftp_dircache_entry_addby_fullname(ftpi, ftfi->remotefilename);
				if(ed && (ed->filesize < ftfi->filesize))
					ed->filesize = ftfi->filesize;
			}
		}
		free(ftfi);
	}
}

static long ftpdrive_file_read(void *filehand_data, char *ptr, unsigned int num)
{
	struct ftpfile_info_s *ftfi = filehand_data;
	struct ftpdrive_info_s *ftpi;
	long bytes_read = 0;
	if(!funcbit_test(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_READ))
		return bytes_read;
	if(!ftpdrive_file_sessiondata_reconnect(ftfi))
		return bytes_read;
	ftpi = (ftfi->ftpi_ftpdrives_ptr) ? *(ftfi->ftpi_ftpdrives_ptr) : NULL;
	if(ftfi->filesize) {
		if(ftfi->filepos >= ftfi->filesize)
			return bytes_read;
		if((ftfi->filepos + num) >= ftfi->filesize)
			num = ftfi->filesize - ftfi->filepos;
	}
	bytes_read = drvftp_dataconn_read(ftpi, &ftpi->socket_info_filehand, ptr, num, num, ((funcbit_test(ftfi->flags, DRVFTP_FTPFILE_FLAG_READWAIT)) ? DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE : 0));
	if(bytes_read > 0) {
		funcbit_smp_filesize_put(ftfi->filepos, ftfi->filepos + bytes_read);
		ftfi->timeout_at_read = 0;
	} else {					//data-reconnect at read-timeout
		if(ftpi->connection_retry && !funcbit_test(ftfi->flags, DRVFTP_FTPFILE_FLAG_READWAIT)) {
			if(!ftfi->timeout_at_read && (bytes_read == 0))
				ftfi->timeout_at_read = pds_gettimem() + DRVFTP_DEFAULT_TIMEOUTMS_READ;
			else if((bytes_read < 0) || (pds_gettimem() > ftfi->timeout_at_read)) {
				funcbit_enable(ftfi->flags, DRVFTP_FTPFILE_FLAG_SEEK);
				ftfi->timeout_at_read = 0;
				drvftp_message_write_error("file connection lost, reconnect ...");
			}
		} else
			ftfi->timeout_at_read = 0;
	}
	return bytes_read;
}

static long ftpdrive_file_write(void *filehand_data, char *ptr, unsigned int num)
{
	struct ftpfile_info_s *ftfi = filehand_data;
	struct ftpdrive_info_s *ftpi;
	long bytes_written = 0;
	if(!funcbit_test(ftfi->opentype, DRVFTP_FTPFILE_OPENTYPE_WRITE))
		return bytes_written;
	if(!ftpdrive_file_sessiondata_reconnect(ftfi))	// ??? at write?
		return bytes_written;
	ftpi = (ftfi->ftpi_ftpdrives_ptr) ? *(ftfi->ftpi_ftpdrives_ptr) : NULL;
	bytes_written = drvftp_dataconn_write(ftpi, &ftpi->socket_info_filehand, ptr, num);
	if(bytes_written > 0) {
		funcbit_smp_filesize_put(ftfi->filepos, ftfi->filepos + bytes_written);
		if(ftfi->filesize < ftfi->filepos)
			funcbit_smp_filesize_put(ftfi->filesize, ftfi->filepos);
		funcbit_smp_enable(ftfi->flags, DRVFTP_FTPFILE_FLAG_WRITE);
	}
	return bytes_written;
}

static mpxp_filesize_t ftpdrive_file_tell(void *filehand_data)
{
	struct ftpfile_info_s *ftfi = filehand_data;
	return ftfi->filepos;
}

static mpxp_filesize_t ftpdrive_file_seek(void *filehand_data, mpxp_filesize_t pos, int fromwhere)
{
	struct ftpfile_info_s *ftfi = filehand_data;
	mpxp_filesize_t newfilepos;
	switch (fromwhere) {
	case SEEK_CUR:
		newfilepos = ftfi->filepos + pos;
		break;
	case SEEK_END:
		newfilepos = ftfi->filesize + pos;
		break;
	case SEEK_SET:
	default:
		newfilepos = pos;
		break;
	}
	if((newfilepos != ftfi->filepos)) {
		funcbit_smp_enable(ftfi->flags, DRVFTP_FTPFILE_FLAG_SEEK);
		funcbit_smp_filesize_put(ftfi->filepos, newfilepos);
	}
	return ftfi->filepos;
}

static mpxp_filesize_t ftpdrive_file_length(void *filehand_data)
{
	struct ftpfile_info_s *ftfi = filehand_data;
	return ftfi->filesize;
}

static int ftpdrive_file_eof(void *filehand_data)
{
	struct ftpfile_info_s *ftfi = filehand_data;
	if(ftfi->filepos >= ftfi->filesize)
		return 1;
	return 0;
}

//------------------------------------------------------------------------

struct mpxplay_drivehand_func_s FTPDRIVE_drivehand_funcs = {
	"FTPDRIVE",
	MPXPLAY_DRIVEHANDFUNC_INFOBIT_SLOWACCESS,	// infobits
	&ftpdrive_drive_config,
	&ftpdrive_drive_check,
	&ftpdrive_drive_mount,
	&ftpdrive_drive_unmount,
	&ftpdrive_findfirst,
	&ftpdrive_findnext,
	&ftpdrive_findclose,
	&ftpdrive_getcwd,
	&ftpdrive_chdir,
	&ftpdrive_mkdir,
	&ftpdrive_rmdir,
	&ftpdrive_rename,
	&ftpdrive_unlink,
	NULL,						// r15
	NULL,						// r16
	NULL,						// r17
	NULL,						// r18
	NULL,						// r19
	NULL,						// r20

	&ftpdrive_file_check,
	&ftpdrive_file_open,
	&ftpdrive_file_close,
	&ftpdrive_file_read,
	&ftpdrive_file_write,
	&ftpdrive_file_seek,
	&ftpdrive_file_tell,
	&ftpdrive_file_length,
	&ftpdrive_file_eof,
	NULL,						// file_chsize
	NULL						// r31
};

#else							// MPXPLAY_DRVFTP_LINK_FTPCLIENT

struct mpxplay_drivehand_func_s FTPDRIVE_drivehand_funcs = {
	"FTPDRIVE", 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

#endif

//------------------------------------------------------------------------
// lowlevel sockets
#ifdef MPXPLAY_DRVFTP_LINK_FTPCLIENT

#ifdef MPXPLAY_WIN32
#include <winsock.h>
static WSADATA wsadata;
#elif defined(MPXPLAY_LINK_SWSCK32)
#include "config.h"
#include "sws_cfg.h"
#include "sws_net.h"
#include "sws_sock.h"
#ifndef BOOL
typedef int BOOL;
#endif
typedef struct sockaddr SOCKADDR;
static unsigned int drvftp_socklib_initialized;
extern char *freeopts[MAXFREEOPTS];
#elif defined(MPXPLAY_LINK_WATTCP32)
#define MPXPLAY_WATTCP_USETICK 1
#include <tcp.h>
#include "netinet/in.h"
#include "netdb.h"
#include "sys/socket.h"
#include "sys/ioctl.h"
#ifndef BOOL
typedef int BOOL;
#endif
typedef int SOCKET;
typedef struct sockaddr SOCKADDR;
extern int _watt_do_exit;
static unsigned int drvftp_wattcp_initialized;
#endif

#ifndef SD_BOTH
#define SD_BOTH 0x02
#endif

static long ftpdrv_lowlevel_ioctl_socket(SOCKET sock, unsigned int control, unsigned long *data)
{
	if(sock) {
#if defined(MPXPLAY_LINK_WATTCP32)
#ifdef MPXPLAY_WATTCP_USETICK
		tcp_tick(&sock);
#endif
		return ioctlsocket(sock, control, (char *)data);
#else
		return ioctlsocket(sock, control, data);
#endif
	}
	return SOCKET_ERROR;
}

static int ftpdrv_lowlevel_global_init(void)
{
#ifdef MPXPLAY_WIN32
	if(!wsadata.wVersion && !wsadata.wHighVersion)
		if(WSAStartup(MAKEWORD(2, 2), &wsadata) != NO_ERROR)
			return 0;
#elif defined(MPXPLAY_LINK_WATTCP32)
	_watt_do_exit = 0;			// don't exit from the program in sock_init()
	if(!drvftp_wattcp_initialized) {
		if(sock_init() != 0)
			return 0;
		drvftp_wattcp_initialized = 1;
	}
#elif defined(MPXPLAY_LINK_SWSCK32)
	if(!drvftp_socklib_initialized) {
		if(!SWS_CfgSetName(NULL, freeopts[OPT_PROGNAME]))
			drvftp_message_write_error("SWSSOCK cfg file load failed!");
		if(SWS_SockStartup(NULL, NULL)) {
			drvftp_message_write_error("SWSSOCK lib init failed!");
			return 0;
		}
		drvftp_socklib_initialized = 1;
	}
#else
	return 0;
#endif
	return 1;
}

static void ftpdrv_lowlevel_global_deinit(void)
{
#ifdef MPXPLAY_WIN32
	if(wsadata.wVersion || wsadata.wHighVersion)
		WSACleanup();
	pds_memset(&wsadata, 0, sizeof(wsadata));
#elif defined(MPXPLAY_LINK_WATTCP32)
	if(drvftp_wattcp_initialized) {
		sock_exit();			// ???
		drvftp_wattcp_initialized = 0;
	}
#elif defined(MPXPLAY_LINK_SWSCK32)
	if(drvftp_socklib_initialized) {
		SWS_SockCleanup();
		drvftp_socklib_initialized = 0;
	}
#endif
}

#ifndef MPXPLAY_WIN32
static int ftpdrv_str_urlipnums_convert(char *urlname, mpxp_uint8_t * ipnums)
{
	unsigned int i = 0;
	char *next = urlname;
	do {
		ipnums[i] = pds_atol(next);
		next = pds_strchr(next, '.');
		if(next)
			next++;
		i++;
	} while((i < DRVFTP_IP_LEN) && next);
	if(i < DRVFTP_IP_LEN)
		return 0;
	return 1;
}
#endif

static int ftpdrv_lowlevel_addressinfo_init(struct ftpdrive_info_s *ftpi)
{
	struct hostent *ht;
	char hostname_local[MAX_PATHNAMELEN];

	if(gethostname(hostname_local, sizeof(hostname_local)) != 0)
		return 0;
	ht = gethostbyname(hostname_local);
	if(!ht || !ht->h_addr_list)
		return 0;
	pds_memcpy(ftpi->ip_local, ht->h_addr_list[0], DRVFTP_IP_LEN);	// !!!

	ht = gethostbyname(ftpi->servername);
	if(ht && ht->h_addr_list) {
		pds_memcpy(ftpi->ip_remote, ht->h_addr_list[0], DRVFTP_IP_LEN);	// !!!
		return 1;
	}
#ifndef MPXPLAY_WIN32
	if(ftpdrv_str_urlipnums_convert(ftpi->servername, &ftpi->ip_remote[0]))
		return 1;
#endif
	return 0;
}

/*static int ftpdrv_lowlevel_addressinfo_init(struct ftpdrive_info_s *ftpi)
{
 struct hostent *ht;
 char hostname_local[MAX_PATHNAMELEN];

 if(gethostname(hostname_local,sizeof(hostname_local))!=0)
  return 0;
 ht=gethostbyname(hostname_local);
 if(!ht || !ht->h_addr_list)
  return 0;
 pds_memcpy(ftpi->ip_local,ht->h_addr_list[0],DRVFTP_IP_LEN); // !!!

 ht=gethostbyname(ftpi->servername);
 if(!ht || !ht->h_addr_list)
  return 0;
 pds_memcpy(ftpi->ip_remote,ht->h_addr_list[0],DRVFTP_IP_LEN); // !!!
 return 1;
}*/

static int ftpdrv_lowlevel_socket_open(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any, unsigned long bufsize)
{
	SOCKET portsock = (SOCKET) socketinfo_any->socknum;
	int rcvbufsize = bufsize;
	unsigned long ioctlnum;
#if defined(MPXPLAY_WIN32) || defined(MPXPLAY_LINK_SWSCK32)
	BOOL optval;
#endif

	if(!portsock) {
		portsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(portsock == INVALID_SOCKET)
			return 0;
#if defined(MPXPLAY_WIN32) || defined(MPXPLAY_LINK_SWSCK32)
		optval = 1;
		setsockopt(portsock, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));
#endif
		ioctlnum = 0;			// block ioctl
		ftpdrv_lowlevel_ioctl_socket(portsock, FIONBIO, &ioctlnum);	//
		funcbit_smp_value_put(socketinfo_any->socknum, (ftpdrive_socket_t) portsock);
	}
	if(rcvbufsize) {
		setsockopt(portsock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbufsize, sizeof(rcvbufsize));
		//setsockopt(portsock,SOL_SOCKET,SO_SNDBUF,(char *)&rcvbufsize,sizeof(rcvbufsize));
	}
	return 1;
}

static void ftpdrv_lowlevel_socket_shutdown(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any)
{
	if(socketinfo_any->socknum) {
		SOCKET sock = (SOCKET) socketinfo_any->socknum;
#ifdef MPXPLAY_WATTCP_USETICK
		tcp_tick(&sock);
#endif
		shutdown(sock, SD_BOTH);
#ifdef MPXPLAY_WATTCP_USETICK
		tcp_tick(NULL);
#endif
	}
}

static void ftpdrv_lowlevel_socket_close(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any, unsigned int full)
{
	if(socketinfo_any->socknum) {
		shutdown((SOCKET) socketinfo_any->socknum, SD_BOTH);
		closesocket((SOCKET) socketinfo_any->socknum);
		funcbit_smp_value_put(socketinfo_any->socknum, 0);
	}
}

static int ftpdrv_lowlevel_socket_select(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any, unsigned int selmode)
{
	struct fd_set fds;
	const struct timeval tv = { 0, 0 };
	fds.fd_count = 1;
	fds.fd_array[0] = (SOCKET) socketinfo_any->socknum;
	return select(0, ((selmode & DRVFTP_SOCKSELECT_MODE_READ) ? &fds : NULL), ((selmode & DRVFTP_SOCKSELECT_MODE_WRITE) ? &fds : NULL), NULL, &tv);
}

static int ftpdrv_lowlevel_socket_connect(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any)
{
	SOCKET portsock = (SOCKET) socketinfo_any->socknum;
	struct sockaddr_in clientservice;
	pds_memset(&clientservice, 0, sizeof(clientservice));
	clientservice.sin_family = AF_INET;
	pds_memcpy(&(clientservice.sin_addr.s_addr), socketinfo_any->conn_ip_addr, DRVFTP_IP_LEN);
	clientservice.sin_port = htons(socketinfo_any->portnum);
	if(connect(portsock, (SOCKADDR *) & clientservice, sizeof(clientservice)) == SOCKET_ERROR)
		return 0;
#ifdef MPXPLAY_WATTCP_USETICK
	tcp_tick(NULL);
	tcp_tick(&portsock);
#endif
	return 1;
}

static int ftpdrv_lowlevel_socket_listen(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_filehand)
{
	SOCKET portsock = (SOCKET) socketinfo_filehand->socknum;
	struct sockaddr_in service, add;
	int socksize, portnum = 0, success = 0, retry = DRVFTP_DEFAULT_TIMEOUTRETRY_DATACONN;

	do {
#ifdef MPXPLAY_WATTCP_USETICK
		tcp_tick(NULL);
#endif

		pds_memset(&service, 0, sizeof(service));
		service.sin_family = AF_INET;
		pds_memcpy(&(service.sin_addr.s_addr), ftpi->ip_local, sizeof(ftpi->ip_local));
		service.sin_port = 0;
		if(bind(portsock, (SOCKADDR *) & service, sizeof(service)) == SOCKET_ERROR)
			continue;
		pds_memset(&add, 0, sizeof(add));
		socksize = sizeof(add);
		if(getsockname(portsock, (SOCKADDR *) & add, &socksize) == SOCKET_ERROR)
			continue;
		portnum = ntohs(add.sin_port);
		if(!portnum)
			continue;

		if(listen(portsock, 1) != SOCKET_ERROR) {
			//ftpdrv_lowlevel_do_select(portsock);
			success = 1;
			break;
		}
#ifdef MPXPLAY_WIN32
		Sleep(0);
#endif
	} while(--retry);

	if(success && portnum)
		funcbit_smp_value_put(socketinfo_filehand->portnum, portnum);

#ifdef MPXPLAY_WATTCP_USETICK
	tcp_tick(NULL);
#endif

	return portnum;
}

static int ftpdrv_lowlevel_socket_accept(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_filehand)
{
	SOCKET portsock = (SOCKET) socketinfo_filehand->socknum, ps;
	unsigned long nonblock;
	int retcode = 0;

#ifdef MPXPLAY_WATTCP_USETICK
	tcp_tick(NULL);
	tcp_tick(&portsock);
#endif
	nonblock = 1;				//
	ftpdrv_lowlevel_ioctl_socket(portsock, FIONBIO, &nonblock);	// else accept can freeze
	ps = accept(portsock, NULL, NULL);
	if(ps != INVALID_SOCKET) {
		closesocket(portsock);
		portsock = ps;
		nonblock = 0;			//
		ftpdrv_lowlevel_ioctl_socket(portsock, FIONBIO, &nonblock);	// else write/send can fail
#ifdef MPXPLAY_WATTCP_USETICK
		tcp_tick(NULL);
		tcp_tick(&portsock);
#endif
		retcode = 1;
	}

	if(retcode)
		funcbit_smp_value_put(socketinfo_filehand->socknum, (ftpdrive_socket_t) portsock);
#ifdef MPXPLAY_WIN32
	else
		Sleep(0);
#endif

	return retcode;
}

static long ftpdrv_lowlevel_send(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socket_info, char *data, unsigned long bytes_to_send)
{
	long total_bytes_sent = 0, leftbytes = bytes_to_send;
	mpxp_uint64_t endtime_response = pds_gettimem() + DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE;
	if(socket_info->socknum) {
		SOCKET sock = (SOCKET) socket_info->socknum;
		do {
			long sentbytes;
#ifdef MPXPLAY_WATTCP_USETICK
			tcp_tick(NULL);
			tcp_tick(&sock);
#endif
			sentbytes = send(sock, data, leftbytes, 0);
#ifdef MPXPLAY_WATTCP_USETICK
			tcp_tick(&sock);
#endif
			if(sentbytes < 0)
				break;
			if(sentbytes > 0) {
				total_bytes_sent += sentbytes;
				if(total_bytes_sent >= bytes_to_send)
					break;
				leftbytes -= sentbytes;
				data += sentbytes;
				endtime_response = pds_gettimem() + DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE;
			}
#ifdef MPXPLAY_WIN32
			Sleep(0);
#endif
		} while((pds_gettimem() <= endtime_response));
	}
	return total_bytes_sent;
}

static long ftpdrv_lowlevel_bytes_buffered(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socket_info)
{
	unsigned long bytes_stored = 0;
	if(socket_info->socknum) {
		long retcode = ftpdrv_lowlevel_ioctl_socket((SOCKET) socket_info->socknum, FIONREAD, &bytes_stored);
		if(retcode == SOCKET_ERROR)
			return -1;
	}
	return bytes_stored;
}

static long ftpdrv_lowlevel_receive(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socket_info, char *data, unsigned long buflen)
{
	long bytes_received = 0;

	if(socket_info->socknum) {
		SOCKET sock = (SOCKET) socket_info->socknum;
#ifdef MPXPLAY_WATTCP_USETICK
		tcp_tick(NULL);
		tcp_tick(&sock);
#endif
		bytes_received = recv(sock, data, buflen, 0);
		if(bytes_received < 0)
			bytes_received = 0;
	}
	return bytes_received;
}

static ftpdrive_lowlevel_func_s FTPDRV_lowlevel_funcs = {
	"ftp:",
	21,
	&ftpdrv_lowlevel_global_init,
	&ftpdrv_lowlevel_global_deinit,
	&ftpdrv_lowlevel_addressinfo_init,
	NULL,
	&ftpdrv_lowlevel_socket_open,
	&ftpdrv_lowlevel_socket_shutdown,
	&ftpdrv_lowlevel_socket_close,
	&ftpdrv_lowlevel_socket_select,
	&ftpdrv_lowlevel_socket_connect,
	NULL,
	&ftpdrv_lowlevel_socket_listen,
	&ftpdrv_lowlevel_socket_accept,
	&ftpdrv_lowlevel_send,
	&ftpdrv_lowlevel_bytes_buffered,
	&ftpdrv_lowlevel_receive,
};

//------------------------------------------------------------------------
//SSL/TLS (ftps: implicit SSL, ftpes: explicit TLS) handling with OpenSSL's SSLEAY32.DLL library (under win32 only)

#ifdef MPXPLAY_WIN32

enum ssleayfuncnums {
	FUNC_SSL_library_init,
	FUNC_SSLv2_client_method,
	FUNC_SSL_CTX_new,
	FUNC_SSL_new,
	FUNC_SSL_set_fd,
	FUNC_SSL_connect,
	FUNC_SSL_write,
	FUNC_SSL_read,
	FUNC_SSL_peek,
	FUNC_SSL_shutdown,
	//FUNC_SSL_want,
	FUNC_SSL_free,
	FUNC_SSL_CTX_free
};

static struct pds_win32dllcallfunc_t ftpsdrv_ssleay_funcs[] = {
	{"SSL_library_init", NULL, 0},
	{"SSLv2_client_method", NULL, 0},	// returns method
	{"SSL_CTX_new", NULL, 1},	// arg1=method     returns ctx
	{"SSL_new", NULL, 1},		// arg1=ctx        returns ssl
	{"SSL_set_fd", NULL, 2},	// arg1=ssl,arg2=socknum
	{"SSL_connect", NULL, 1},	// arg1=ssl        returns err
	{"SSL_write", NULL, 3},		// arg1=ssl,arg2=string,arg3=len returns err
	{"SSL_read", NULL, 3},		// arg1=ssl,arg2=buf,arg3=buflen returns err
	{"SSL_peek", NULL, 3},		// arg1=ssl,arg2=buf,arg3=buflen returns err
	{"SSL_shutdown", NULL, 1},	// arg1=ssl
	//{"SSL_want",NULL,1},               // arg1=ssl
	{"SSL_free", NULL, 1},		// arg1=ssl
	{"SSL_CTX_free", NULL, 1},	// arg1=ctx
	{NULL, NULL, 0}
};

static int ftpsdrv_llwin32_socket_ssl_connect(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any);

static HMODULE ftpsdrv_ssleay32_dllhand;
static void *ftpsdrv_ssleay_ctx;

static int ftpsdrv_llwin32_global_init(void)
{
	if(!wsadata.wVersion && !wsadata.wHighVersion)
		if(WSAStartup(MAKEWORD(2, 2), &wsadata) != NO_ERROR)
			return 0;

	if(!ftpsdrv_ssleay32_dllhand) {
		ftpsdrv_ssleay32_dllhand = newfunc_dllload_winlib_load("ssleay32.dll");
		if(!ftpsdrv_ssleay32_dllhand) {
			drvftp_message_write_error("Couldn't load OPENSSL's SSLEAY32.DLL !");
			goto err_out_ftpsinit;
		}
		if(!newfunc_dllload_winlib_getfuncs(ftpsdrv_ssleay32_dllhand, ftpsdrv_ssleay_funcs)) {
			drvftp_message_write_error("A function is missing from SSLEAY32.DLL !");
			goto err_out_ftpsinit;
		}
	}
	if(!ftpsdrv_ssleay_ctx) {
		void *meth;
		newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_library_init], NULL, NULL, NULL);
		meth = (void *)newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSLv2_client_method], NULL, NULL, NULL);
		ftpsdrv_ssleay_ctx = (void *)newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_CTX_new], meth, NULL, NULL);
	}
	if(!ftpsdrv_ssleay_ctx) {
		drvftp_message_write_error("Couldn't init SSLEAY32! (ctx=NULL)");
		goto err_out_ftpsinit;
	}
	return 1;
  err_out_ftpsinit:
	newfunc_dllload_winlib_close(ftpsdrv_ssleay32_dllhand);
	return 0;
}

static void ftpsdrv_llwin32_global_deinit(void)
{
	if(ftpsdrv_ssleay_ctx) {
		newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_CTX_free], ftpsdrv_ssleay_ctx, NULL, NULL);
		funcbit_smp_pointer_put(ftpsdrv_ssleay_ctx, NULL);
	}
	newfunc_dllload_winlib_close(ftpsdrv_ssleay32_dllhand);
	funcbit_smp_pointer_put(ftpsdrv_ssleay32_dllhand, NULL);

	if(wsadata.wVersion || wsadata.wHighVersion)
		WSACleanup();
	pds_memset(&wsadata, 0, sizeof(wsadata));
}

static int ftpesdrv_llwin32_addressinfo_init(struct ftpdrive_info_s *ftpi)
{
	funcbit_smp_enable(ftpi->socket_info_session.flags, DRVFTP_SOCKINFO_FLAG_SSL_DISABLED);
	return ftpdrv_lowlevel_addressinfo_init(ftpi);
}

static int ftpesdrv_llwin32_login_preprocess(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_session)
{
	if(!drvftp_cmdctrl_send_command_check_respcode(ftpi, socketinfo_session, "AUTH TLS", 234))
		return 0;
	funcbit_disable(socketinfo_session->flags, DRVFTP_SOCKINFO_FLAG_SSL_DISABLED);
	return ftpsdrv_llwin32_socket_ssl_connect(ftpi, socketinfo_session);
}

static int ftpsdrv_llwin32_socket_open(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any, unsigned long bufsize)
{
	if(!ftpdrv_lowlevel_socket_open(ftpi, socketinfo_any, bufsize))
		return 0;

	if(!socketinfo_any->sslhand) {
		socketinfo_any->sslhand = (void *)newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_new], ftpsdrv_ssleay_ctx, NULL, NULL);
		if(!socketinfo_any->sslhand)
			return 0;
	}
	newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_set_fd], socketinfo_any->sslhand, (void *)(socketinfo_any->socknum), NULL);
	return 1;
}

static void ftpsdrv_llwin32_socket_shutdown(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any)
{
	if(socketinfo_any->socknum) {
		ftpdrv_lowlevel_socket_shutdown(ftpi, socketinfo_any);
		if(socketinfo_any->sslhand)
			newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_shutdown], socketinfo_any->sslhand, NULL, NULL);
	}
}

static void ftpsdrv_llwin32_socket_close(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any, unsigned int full)
{
	if(socketinfo_any->sslhand)
		newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_shutdown], socketinfo_any->sslhand, NULL, NULL);
	ftpdrv_lowlevel_socket_close(ftpi, socketinfo_any, full);
	if(full) {
		if(socketinfo_any->sslhand) {
			newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_free], socketinfo_any->sslhand, NULL, NULL);
			funcbit_smp_pointer_put(socketinfo_any->sslhand, NULL);
		}
	}
}

static int ftpsdrv_llwin32_socket_ssl_connect(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_any)
{
	int err;
	unsigned long nonblock;
	mpxp_uint64_t endtime_sslconnect;
	struct fptdrive_timeoutmsg_s tos;

	if(!socketinfo_any->sslhand || funcbit_test(socketinfo_any->flags, DRVFTP_SOCKINFO_FLAG_SSL_DISABLED))
		return 1;

	nonblock = 1;				// non block ioctl
	ftpdrv_lowlevel_ioctl_socket(socketinfo_any->socknum, FIONBIO, &nonblock);	// else SSL_connect can freeze

	newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_set_fd], socketinfo_any->sslhand, (void *)(socketinfo_any->socknum), NULL);

	endtime_sslconnect = pds_gettimem() + DRVFTP_DEFAULT_TIMEOUTMS_RESPONSE;
	drvftp_message_timeout_init(&tos, endtime_sslconnect, "SSL connect retry %d sec ...");

	do {
		err = newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_connect], socketinfo_any->sslhand, NULL, NULL);
		if(err != -1) {
			nonblock = 0;		// block ioctl
			ftpdrv_lowlevel_ioctl_socket(socketinfo_any->socknum, FIONBIO, &nonblock);	// else read will fail
			return 1;
		}
		drvftp_message_timeout_write(&tos);
	} while(pds_gettimem() < endtime_sslconnect);
	drvftp_message_timeout_close(&tos);
	ftpsdrv_llwin32_socket_shutdown(ftpi, socketinfo_any);
	ftpsdrv_llwin32_socket_close(ftpi, socketinfo_any, 1);
	return 0;
}

static int ftpsdrv_llwin32_socket_accept(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socketinfo_filehand)
{
	if(!ftpdrv_lowlevel_socket_accept(ftpi, socketinfo_filehand))
		return 0;
	return ftpsdrv_llwin32_socket_ssl_connect(ftpi, socketinfo_filehand);
}

static long ftpsdrv_llwin32_send(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socket_info, char *data, unsigned long bytes_to_send)
{
	if(!socket_info->sslhand || funcbit_test(socket_info->flags, DRVFTP_SOCKINFO_FLAG_SSL_DISABLED))
		return ftpdrv_lowlevel_send(ftpi, socket_info, data, bytes_to_send);
	else {
		long retcode;
		retcode = newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_write], socket_info->sslhand, data, (void *)(bytes_to_send));
		if(retcode < 0)
			retcode = 0;
		return retcode;
	}
}

static long ftpsdrv_llwin32_bytes_buffered(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socket_info)
{
	if(!socket_info->sslhand || funcbit_test(socket_info->flags, DRVFTP_SOCKINFO_FLAG_SSL_DISABLED))
		return ftpdrv_lowlevel_bytes_buffered(ftpi, socket_info);
	else {
		long bytes_received;
		unsigned long ioctlnum;
		char data[256];

		ioctlnum = 1;			// non block ioctl
		ftpdrv_lowlevel_ioctl_socket(socket_info->socknum, FIONBIO, &ioctlnum);	//

		bytes_received = newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_peek], socket_info->sslhand, (void *)(&data[0]), (void *)(sizeof(data) - 1));

		ioctlnum = 0;			// block ioctl
		ftpdrv_lowlevel_ioctl_socket(socket_info->socknum, FIONBIO, &ioctlnum);	//

		if(bytes_received < 0)
			bytes_received = 0;
		return bytes_received;
	}
}

static long ftpsdrv_llwin32_receive(struct ftpdrive_info_s *ftpi, struct ftpdrive_socket_info_s *socket_info, char *data, unsigned long buflen)
{
	if(!socket_info->sslhand || funcbit_test(socket_info->flags, DRVFTP_SOCKINFO_FLAG_SSL_DISABLED))
		return ftpdrv_lowlevel_receive(ftpi, socket_info, data, buflen);
	else {
		long bytes_received;
		bytes_received = newfunc_dllload_winlib_callfunc(&ftpsdrv_ssleay_funcs[FUNC_SSL_read], socket_info->sslhand, data, (void *)(buflen));
		if(bytes_received < 0)
			bytes_received = 0;
		return bytes_received;
	}
}

static ftpdrive_lowlevel_func_s FTPSDRV_lowlevel_funcs = {
	"ftps:",
	990,
	&ftpsdrv_llwin32_global_init,
	&ftpsdrv_llwin32_global_deinit,
	&ftpdrv_lowlevel_addressinfo_init,
	NULL,
	&ftpsdrv_llwin32_socket_open,
	&ftpsdrv_llwin32_socket_shutdown,
	&ftpsdrv_llwin32_socket_close,
	&ftpdrv_lowlevel_socket_select,
	&ftpdrv_lowlevel_socket_connect,
	&ftpsdrv_llwin32_socket_ssl_connect,
	&ftpdrv_lowlevel_socket_listen,
	&ftpsdrv_llwin32_socket_accept,
	&ftpsdrv_llwin32_send,
	&ftpsdrv_llwin32_bytes_buffered,
	&ftpsdrv_llwin32_receive
};

static ftpdrive_lowlevel_func_s FTPESDRV_lowlevel_funcs = {
	"ftpes:",
	21,
	&ftpsdrv_llwin32_global_init,
	&ftpsdrv_llwin32_global_deinit,
	&ftpesdrv_llwin32_addressinfo_init,
	&ftpesdrv_llwin32_login_preprocess,
	&ftpsdrv_llwin32_socket_open,
	&ftpsdrv_llwin32_socket_shutdown,
	&ftpsdrv_llwin32_socket_close,
	&ftpdrv_lowlevel_socket_select,
	&ftpdrv_lowlevel_socket_connect,
	&ftpsdrv_llwin32_socket_ssl_connect,
	&ftpdrv_lowlevel_socket_listen,
	&ftpsdrv_llwin32_socket_accept,
	&ftpsdrv_llwin32_send,
	&ftpsdrv_llwin32_bytes_buffered,
	&ftpsdrv_llwin32_receive
};

#endif							// MPXPLAY_WIN32

#endif							// MPXPLAY_DRVFTP_LINK_FTPCLIENT
