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
//function: input file buffering

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_USE_DEBUGMSG
#define MPXPLAY_DEBUG_OUTPUT NULL

#ifdef MPXPLAY_WIN32
 //#define MPXINBUF_USE_CRITSEC 1
#endif

#include "newfunc\newfunc.h"
#include "mpxinbuf.h"
#include "au_cards\au_cards.h"
#include "display\display.h"

static unsigned int mpxinbuf_alloc_fullbuffer(struct mainvars *mvp, struct frame *frp);
static unsigned int mpxinbuf_alloc_ringbuffer(struct mainvars *mvp, struct frame *frp, unsigned long blocks);
static unsigned int mpxinbuf_ringbuffer_fill(struct frame *);
static long mpxplay_mpxinbuf_ringbuffer_advance(struct frame *frp, long newpos_absolute);

static long mpxinbuf_lowlevel_file_read(struct frame *frp, char *ptr, unsigned int num);
static long mpxinbuf_lowlevel_file_seek(struct frame *frp, long newpos_absolute);
static unsigned int mpxinbuf_lowlevel_file_eof(struct frame *frp);
static int mpxinbuf_lowlevel_file_chsize(struct frame *frp, mpxp_filesize_t offset);

static unsigned int mpxplay_mpxinbuf_fopen_read(void *fbds, char *filename, unsigned long pb_blocksize);
static unsigned int mpxplay_mpxinbuf_fopen_write(void *fbds, char *filename);
static void mpxplay_mpxinbuf_fclose(void *fbds);
static unsigned long mpxplay_mpxinbuf_fread(void *fbds, void *ptr, unsigned long num);
static long mpxplay_mpxinbuf_fwrite(void *fbds, void *ptr, unsigned long num);
static long mpxplay_mpxinbuf_seek_unbuffered(struct frame *frp, long newpos_absolute);
static long mpxplay_mpxinbuf_fseek(void *fbds, long offset, int whence);
static long mpxplay_mpxinbuf_ftell(void *fbds);
static long mpxplay_mpxinbuf_filelength(void *fbds);
static int mpxplay_mpxinbuf_feof(void *fbds);
static int mpxplay_mpxinbuf_chsize(void *fbds, long offset);
static mpxp_uint32_t mpxplay_mpxinbuf_get_byte(void *fbds);
static mpxp_uint32_t mpxplay_mpxinbuf_get_le16(void *fbds);
static mpxp_uint32_t mpxplay_mpxinbuf_get_le32(void *fbds);
static mpxp_uint64_t mpxplay_mpxinbuf_get_le64(void *fbds);
static mpxp_uint32_t mpxplay_mpxinbuf_get_be16(void *fbds);
static mpxp_uint32_t mpxplay_mpxinbuf_get_be32(void *fbds);
static mpxp_uint64_t mpxplay_mpxinbuf_get_be64(void *fbds);

static struct mpxplay_filehand_buffered_func_s mpxinbuf_functions = {
	&mpxplay_mpxinbuf_fopen_read,
	&mpxplay_mpxinbuf_fopen_write,
	&mpxplay_mpxinbuf_fclose,
	&mpxplay_mpxinbuf_fread,
	&mpxplay_mpxinbuf_fwrite,
	&mpxplay_mpxinbuf_fseek,
	&mpxplay_mpxinbuf_ftell,
	&mpxplay_mpxinbuf_filelength,
	&mpxplay_mpxinbuf_feof,
	&mpxplay_mpxinbuf_chsize,
	&mpxplay_mpxinbuf_get_byte,
	&mpxplay_mpxinbuf_get_le16,
	&mpxplay_mpxinbuf_get_le32,
	&mpxplay_mpxinbuf_get_le64,
	&mpxplay_mpxinbuf_get_be16,
	&mpxplay_mpxinbuf_get_be32,
	&mpxplay_mpxinbuf_get_be64,
};

extern unsigned int prebuffertype, prebufferblocks;
extern unsigned int intsoundconfig, intsoundcontrol;
extern volatile unsigned long mpxplay_signal_events;
extern volatile unsigned int playcontrol;
#ifdef MPXINBUF_USE_CRITSEC
static CRITICAL_SECTION critsec_bufcheck, critsec_rbfill;
#endif

void mpxplay_mpxinbuf_init(struct mainvars *mvp)
{
	struct frame *frp = mvp->frp0;

	frp->filebuf_funcs = &mpxinbuf_functions;
	frp++;
	frp->filebuf_funcs = &mpxinbuf_functions;
	frp++;
	frp->filebuf_funcs = &mpxinbuf_functions;

	if(prebuffertype & PREBUFTYPE_MASK)
		mpxinbuf_alloc_ringbuffer(mvp, frp, PREBUFFERBLOCKS_SHORTRING);

	if(prebufferblocks > (0x7fffffff / PREBUFFERBLOCKSIZE_DECODE))	// int32 overflow in allocation
		prebufferblocks = 0x7fffffff / PREBUFFERBLOCKSIZE_DECODE;
#ifdef MPXINBUF_USE_CRITSEC
	InitializeCriticalSection(&critsec_bufcheck);
	InitializeCriticalSection(&critsec_rbfill);
#endif
}

void mpxplay_mpxinbuf_assign_funcs(struct frame *frp)
{
	frp->filebuf_funcs = &mpxinbuf_functions;
}

void mpxplay_mpxinbuf_prealloc(struct mainvars *mvp)
{
	if(prebuffertype & PREBUFTYPE_RING) {
		unsigned int i = 2;
		struct frame *frp = mvp->frp0;
		display_message(0, 0, "Prebuffer memory allocation (a few seconds)");
		do {
			if(!mpxinbuf_alloc_ringbuffer(mvp, frp, prebufferblocks))
				mpxplay_close_program(MPXERROR_XMS_MEM);
			frp++;
		} while(--i);
		clear_message();
	}
}

void mpxplay_mpxinbuf_close(struct mainvars *mvp)
{
	struct frame *frp = mvp->frp0;
	unsigned int i = 3;

	do {
		if(frp->prebufferbegin) {
			free(frp->prebufferbegin);
			funcbit_smp_pointer_put(frp->prebufferbegin, NULL);
		}
		frp++;
	} while(--i);
#ifdef MPXINBUF_USE_CRITSEC
	DeleteCriticalSection(&critsec_bufcheck);
	DeleteCriticalSection(&critsec_rbfill);
#endif
}

//-----------------------------------------------------------------------

unsigned int mpxplay_mpxinbuf_alloc(struct mainvars *mvp, struct frame *frp)
{
	unsigned int ok = 0;

	if(prebuffertype & PREBUFTYPE_FULL)
		ok = mpxinbuf_alloc_fullbuffer(mvp, frp);
	if((prebuffertype & PREBUFTYPE_RING) || ((prebuffertype & PREBUFTYPE_FULL) && !ok))
		ok = mpxinbuf_alloc_ringbuffer(mvp, frp, prebufferblocks);
	if(!(prebuffertype & PREBUFTYPE_MASK)) {
		funcbit_smp_enable(frp->buffertype, PREBUFTYPE_FILLED);
		ok = 1;
	}
	funcbit_smp_filesize_put(frp->prebuffer_seek_retry, PREBUFFER_SEEKRETRY_INVALID);
	return ok;
}

#define FULLBUFFER_READ_BLOCKS 20

static unsigned int mpxinbuf_alloc_fullbuffer(struct mainvars *mvp, struct frame *frp)
{
	struct frame *frop;
	int blocksize, percent;
	char *pointer, sout[40];

	funcbit_smp_value_put(frp->buffertype, PREBUFTYPE_NONE);
	if(playcontrol & PLAYC_RUNNING) {
		AU_stop(mvp->aui);
		funcbit_enable(playcontrol, PLAYC_STARTNEXT);
	}
	frop = frp->fro;
	mpxplay_infile_close(frop);
	if(frop->prebufferbegin != NULL) {
		pds_free(frop->prebufferbegin);
		funcbit_smp_pointer_put(frop->prebufferbegin, NULL);
		funcbit_smp_value_put(frop->buffertype, 0);
	}
	if(frp->prebufferbegin != NULL)
		pds_free(frp->prebufferbegin);
	funcbit_smp_pointer_put(frp->prebufferbegin, pds_malloc(frp->filesize));
	if(frp->prebufferbegin == NULL)
		return 0;
	blocksize = PREBUFFERBLOCKSIZE_DECODE;
	mpxplay_diskdrive_file_config(frp->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_FILEBLOCKSIZE, &blocksize, NULL);
	mpxinbuf_lowlevel_file_seek(frp, 0);
	blocksize = frp->filesize / FULLBUFFER_READ_BLOCKS;
	if(blocksize) {
		pointer = frp->prebufferbegin;
		for(percent = 0; percent < 100; percent += (100 / FULLBUFFER_READ_BLOCKS)) {
			if(pds_look_extgetch() == KEY_ESC)
				break;
			sprintf(sout, "Loading song : %d%%", percent);
			display_message(0, 0, sout);
			mpxinbuf_lowlevel_file_read(frp, pointer, blocksize);
			pointer += blocksize;
		}
		if(pds_look_extgetch() == KEY_ESC) {
			pds_extgetch();
			pds_free(frp->prebufferbegin);
			frp->prebufferbegin = NULL;
			return 0;
		}
	}
	mpxinbuf_lowlevel_file_read(frp, pointer, frp->filesize - (FULLBUFFER_READ_BLOCKS * blocksize));
	funcbit_smp_value_put(frp->prebufferbytes_forward, frp->filesize);
	funcbit_smp_value_put(frp->prebufferputp, frp->filesize);
	funcbit_smp_value_put(frp->prebuffersize, frp->filesize);
	funcbit_smp_value_put(frp->buffertype, PREBUFTYPE_FULL | PREBUFTYPE_FILLED);
	return 1;
}

static unsigned int mpxinbuf_alloc_ringbuffer(struct mainvars *mvp, struct frame *frp, unsigned long blocks)
{
	funcbit_smp_value_put(frp->prebufferblocksize, PREBUFFERBLOCKSIZE_DECODE);
	funcbit_smp_value_put(frp->prebuffersize, blocks * frp->prebufferblocksize);

	if(frp->prebufferbegin == NULL) {
		funcbit_smp_pointer_put(frp->prebufferbegin, pds_malloc(frp->prebuffersize + 32));
		if(frp->prebufferbegin == NULL) {
			struct frame *frop;
			if(playcontrol & PLAYC_RUNNING) {
				AU_stop(mvp->aui);
				funcbit_enable(playcontrol, PLAYC_STARTNEXT);
			}
			frop = frp->fro;
			if(!frop || !frop->prebufferbegin)
				return 0;
			funcbit_smp_pointer_put(frp->prebufferbegin, frop->prebufferbegin);
			funcbit_smp_value_put(frp->prebuffersize, frop->prebuffersize);	// ??? have to be the same
			funcbit_smp_pointer_put(frop->prebufferbegin, NULL);
			funcbit_smp_value_put(frop->buffertype, 0);
		}
	}
	funcbit_smp_value_put(frp->buffertype, (PREBUFTYPE_RING | (prebuffertype & PREBUFTYPE_BACK)));
	mpxplay_diskdrive_file_config(frp->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_FILEBLOCKSIZE, &frp->prebufferblocksize, NULL);
	return 1;
}

void mpxplay_mpxinbuf_set_intsound(struct frame *frp, unsigned int intcfg)
{
	if(intcfg & INTSOUND_DECODER)
		funcbit_smp_enable(frp->buffertype, PREBUFTYPE_INT);
}

unsigned int mpxplay_mpxinbuf_buffer_check(struct frame *frp)	// returns 1 at eof/read-error
{
	unsigned int retcode = 1;
#ifdef MPXINBUF_USE_CRITSEC
	if(!TryEnterCriticalSection(&critsec_bufcheck))
		return retcode;
#endif
	funcbit_smp_enable(frp->buffertype, PREBUFTYPE_FILLED);
	if(frp->prebuffer_seek_retry >= 0) {
		if((frp->buffertype & PREBUFTYPE_RING) && !(frp->buffertype & PREBUFTYPE_WRITEPROTECT)) {
			if(mpxplay_mpxinbuf_ringbuffer_advance(frp, frp->prebuffer_seek_retry) < 0)
				goto err_out_bfc;
		} else {
			if(mpxplay_mpxinbuf_seek_unbuffered(frp, frp->prebuffer_seek_retry) < 0)
				goto err_out_bfc;
		}
		funcbit_smp_filesize_put(frp->prebuffer_seek_retry, PREBUFFER_SEEKRETRY_INVALID);
	}
	if((frp->buffertype & PREBUFTYPE_RING) && !(frp->buffertype & PREBUFTYPE_WRITEPROTECT)) {
		if((((frp->prebuffersize - frp->prebufferbytes_forward) > frp->prebufferblocksize) && !(frp->buffertype & PREBUFTYPE_BACK))
		   || ((frp->prebufferbytes_forward + frp->prebufferblocksize) < (frp->prebuffersize / 100 * PREBUFTYPE_GET_BACKBUF_PERCENT(prebuffertype)))
			) {
			if(!mpxinbuf_ringbuffer_fill(frp)) {
				if(!mpxinbuf_lowlevel_file_eof(frp))
					funcbit_smp_disable(frp->buffertype, PREBUFTYPE_FILLED);
				goto err_out_bfc;
			}
			funcbit_smp_disable(frp->buffertype, PREBUFTYPE_FILLED);
		}
		retcode = 0;
	}
  err_out_bfc:
#ifdef MPXINBUF_USE_CRITSEC
	LeaveCriticalSection(&critsec_bufcheck);
#endif
	return retcode;
}

//------------------------------------------------------------------------
//fill prebuffer

//#define MPXINBUF_DEBUG_FILL 1

static void check_buffer_overflow(struct frame *frp)
{
	if(frp->prebufferputp < 0)
		funcbit_smp_value_put(frp->prebufferputp, frp->prebufferputp + frp->prebuffersize);
	if(frp->prebufferputp < 0)
		funcbit_smp_value_put(frp->prebufferputp, 0);
	if(frp->prebufferputp >= frp->prebuffersize)
		funcbit_smp_value_put(frp->prebufferputp, frp->prebufferputp - frp->prebuffersize);
	if(frp->prebufferputp >= frp->prebuffersize)
		funcbit_smp_value_put(frp->prebufferputp, 0);
	if(frp->prebufferbytes_forward > frp->prebuffersize)
		funcbit_smp_value_put(frp->prebufferbytes_forward, frp->prebuffersize);
	if(frp->prebufferbytes_rewind > (frp->prebuffersize - frp->prebufferbytes_forward))	// ???
		funcbit_smp_value_put(frp->prebufferbytes_rewind, frp->prebuffersize - frp->prebufferbytes_forward);
}

static unsigned int mpxinbuf_ringbuffer_fill(struct frame *frp)
{
#ifdef MPXINBUF_DEBUG_FILL
	unsigned int thnum = 0;
	if(newfunc_newhandler08_is_current_thread()) {
		display_message(0, 0, "int08 rbfill start");
		thnum = 1;
	} else
		display_message(1, 0, "main rbfill start");
#endif
	if(!mpxinbuf_lowlevel_file_eof(frp)) {
		long i, j, outbytes;

#ifdef MPXINBUF_USE_CRITSEC
		if(!TryEnterCriticalSection(&critsec_rbfill))
			return 0;
#endif

		check_buffer_overflow(frp);

		i = frp->prebuffersize - frp->prebufferputp;
#ifdef MPXINBUF_DEBUG_FILL
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rbf i:%5d pbs:%d", i, frp->prebufferblocksize);
#endif
		if(i < frp->prebufferblocksize) {
#ifdef MPXINBUF_DEBUG_FILL
			mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rbf lfr i:%5d putp:%d", i, frp->prebufferputp);
#endif
			j = mpxinbuf_lowlevel_file_read(frp, &frp->prebufferbegin[frp->prebufferputp], i);
#ifdef MPXINBUF_DEBUG_FILL
			mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rbf memr j:%5d putp:%d", j, frp->prebufferputp);
#endif
			pds_smp_memrefresh(&frp->prebufferbegin[frp->prebufferputp], j);
			if(j < i) {
				funcbit_smp_value_put(frp->prebufferputp, frp->prebufferputp + j);
				i = 0;
			} else {
				funcbit_smp_value_put(frp->prebufferputp, 0);
				i = frp->prebufferblocksize - i;
			}
			outbytes = j;
		} else {
			i = frp->prebufferblocksize;
			outbytes = 0;
		}
		if(i) {
#ifdef MPXINBUF_DEBUG_FILL
			mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rbflfr2%d i:%5d p:%d g:%d f:%d r:%d", thnum, i, frp->prebufferputp, frp->prebuffergetp, frp->prebufferbytes_forward, frp->prebufferbytes_rewind);
#endif
			j = mpxinbuf_lowlevel_file_read(frp, &frp->prebufferbegin[frp->prebufferputp], i);
#ifdef MPXINBUF_DEBUG_FILL
			mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rbf%d memr2 j:%5d putp:%d", thnum, j, frp->prebufferputp);
#endif
			pds_smp_memrefresh(&frp->prebufferbegin[frp->prebufferputp], j);
			funcbit_smp_value_put(frp->prebufferputp, frp->prebufferputp + j);
			outbytes += j;
		}
		funcbit_smp_value_put(frp->prebufferbytes_forward, frp->prebufferbytes_forward + outbytes);

#ifdef MPXINBUF_DEBUG_FILL
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "rbfend%d r:%5d f:%5d p:%d g:%d", thnum, frp->prebufferbytes_rewind, frp->prebufferbytes_forward, frp->prebufferputp, frp->prebuffergetp);
#endif

		check_buffer_overflow(frp);

#ifdef MPXINBUF_DEBUG_FILL
		if(thnum)
			display_message(0, 0, "int08 rbfill end");
		else
			display_message(1, 0, "main rbfill end");
#endif

#ifdef MPXINBUF_USE_CRITSEC
		LeaveCriticalSection(&critsec_rbfill);
#endif

		return outbytes;
	}
	return 0;
}

//-------------------------------------------------------------------------
// low level
static unsigned int mpxinbuf_lowlevel_file_open_read(struct frame *frp, char *filename)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;

	if(lowfuncs) {
		if(lowfuncs->open_read)
			frp->filehand_datas = lowfuncs->open_read(filename);
	} else {
		if(!frp->mdds)
			frp->mdds = playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(filename));
		frp->filehand_datas = mpxplay_diskdrive_file_open(frp->mdds, filename, (O_RDONLY | O_BINARY));
	}

	if(frp->filehand_datas)
		return 1;

	return 0;
}

static unsigned int mpxinbuf_lowlevel_file_open_write(struct frame *frp, char *filename)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;

	if(lowfuncs) {
		if(lowfuncs->open_write)
			frp->filehand_datas = lowfuncs->open_write(filename);
	} else {
		if(!frp->mdds)
			frp->mdds = playlist_loaddir_drivenum_to_drivemap(pds_getdrivenum_from_path(filename));
		frp->filehand_datas = mpxplay_diskdrive_file_open(frp->mdds, filename, (O_RDWR | O_BINARY));
	}

	if(frp->filehand_datas)
		return 1;

	return 0;
}

static void mpxinbuf_lowlevel_file_close(struct frame *frp)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;

	if(lowfuncs) {
		if(lowfuncs->close)
			lowfuncs->close(frp->filehand_datas);
	} else
		mpxplay_diskdrive_file_close(frp->filehand_datas);
	frp->filehand_datas = NULL;
}

static long mpxinbuf_lowlevel_file_read(struct frame *frp, char *ptr, unsigned int num)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;
	long bytes = 0;

	if(lowfuncs) {
		if(lowfuncs->read)
			bytes = lowfuncs->read(frp->filehand_datas, ptr, num);
	} else
		bytes = mpxplay_diskdrive_file_read(frp->filehand_datas, ptr, num);
	if(bytes < 0)
		bytes = 0;
	else
		funcbit_smp_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
	funcbit_smp_value_put(frp->filepos, frp->filepos + bytes);
	return bytes;
}

static long mpxinbuf_lowlevel_file_write(struct frame *frp, char *ptr, unsigned int num)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;
	long bytes = 0;

	if(lowfuncs) {
		if(lowfuncs->write)
			bytes = lowfuncs->write(frp->filehand_datas, ptr, num);
	} else
		bytes = mpxplay_diskdrive_file_write(frp->filehand_datas, ptr, num);
	if(bytes < 0)
		bytes = 0;
	else
		funcbit_smp_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
	funcbit_smp_value_put(frp->filepos, frp->filepos + bytes);
	if(frp->filepos >= frp->filesize)
		funcbit_smp_value_put(frp->filesize, frp->filepos + 1);
	return bytes;
}

static mpxp_filesize_t mpxinbuf_lowlevel_file_length(struct frame *frp)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;
	mpxp_filesize_t filelen = 0;

	if(lowfuncs) {
		if(lowfuncs->filelength)
			filelen = lowfuncs->filelength(frp->filehand_datas);
	} else
		filelen = mpxplay_diskdrive_file_length(frp->filehand_datas);
	return filelen;
}

static mpxp_filesize_t mpxinbuf_lowlevel_file_seek(struct frame *frp, long newpos_absolute)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;
	mpxp_filesize_t filepos = 0;

	if(lowfuncs) {
		if(lowfuncs->seek)
			filepos = lowfuncs->seek(frp->filehand_datas, newpos_absolute, SEEK_SET);
	} else
		filepos = mpxplay_diskdrive_file_seek(frp->filehand_datas, newpos_absolute, SEEK_SET);

	if(filepos >= 0) {
		frp->filepos = filepos;
		funcbit_enable(mpxplay_signal_events, MPXPLAY_SIGNALTYPE_DISKACCESS);
	}

	if(filepos < 0)
		return MPXPLAY_ERROR_MPXINBUF_SEEK_LOW;

	return filepos;
}

static unsigned int mpxinbuf_lowlevel_file_eof(struct frame *frp)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;
	int eof_flag = -1;

	if(lowfuncs) {
		if(lowfuncs->eof)
			eof_flag = lowfuncs->eof(frp->filehand_datas);
	} else
		eof_flag = mpxplay_diskdrive_file_eof(frp->filehand_datas);

	if(eof_flag < 0)
		eof_flag = (frp->filepos >= frp->filesize) ? 1 : 0;

	return eof_flag;
}

static int mpxinbuf_lowlevel_file_chsize(struct frame *frp, mpxp_filesize_t offset)
{
	struct mpxplay_filehand_low_func_s *lowfuncs = frp->filehand_funcs;
	int success = 0;

	if(lowfuncs) {
		if(lowfuncs->chsize)
			success = lowfuncs->chsize(frp->filehand_datas, offset);
	} else
		success = mpxplay_diskdrive_file_chsize(frp->filehand_datas, offset);

	return success;
}

//-------------------------------------------------------------------------
// buffered open & close
static void mpxinbuf_reset(struct frame *frp)
{
	funcbit_smp_value_put(frp->filepos, 0);
	funcbit_smp_value_put(frp->buffertype, 0);
	funcbit_smp_value_put(frp->prebuffergetp, 0);
	funcbit_smp_value_put(frp->prebufferputp, 0);
	funcbit_smp_value_put(frp->prebufferbytes_forward, 0);
	funcbit_smp_value_put(frp->prebufferbytes_rewind, 0);
}

static unsigned int mpxplay_mpxinbuf_fopen_read(void *fbds, char *filename, unsigned long file_blocksize)
{
	struct frame *frp = fbds;
	struct mpxplay_diskdrive_data_s *mdds;
	long drive_blocksize, readwait;

	if(mpxinbuf_lowlevel_file_open_read(frp, filename)) {
		mpxinbuf_reset(frp);

		mdds = frp->mdds;
		if(!mdds)
			mdds = mpxplay_diskdrive_file_get_mdds(frp->filehand_datas);

		funcbit_smp_value_put(frp->filesize, mpxinbuf_lowlevel_file_length(frp));
		if(frp->filesize >= 8) {	// !!! (less is not a real audio file)

			drive_blocksize = mpxplay_diskdrive_drive_config(mdds, MPXPLAY_DISKDRIV_CFGFUNCNUM_GET_CHKBUFBLOCKBYTES, NULL, NULL);
			if(drive_blocksize < 0)
				drive_blocksize = PREBUFFERBLOCKSIZE_CHECK;
			if(drive_blocksize < file_blocksize)
				drive_blocksize = file_blocksize;

			if(frp->prebufferbegin) {
				if(drive_blocksize) {
					if(drive_blocksize > (frp->prebuffersize / 2))
						drive_blocksize = frp->prebuffersize / 2;
					funcbit_smp_value_put(frp->buffertype, PREBUFTYPE_RING);
				} else
					funcbit_smp_value_put(frp->buffertype, PREBUFTYPE_NONE);
				funcbit_smp_value_put(frp->prebufferblocksize, drive_blocksize);
			}

			mpxplay_diskdrive_file_config(frp->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_FILEBLOCKSIZE, &drive_blocksize, NULL);
			readwait = 1;
			mpxplay_diskdrive_file_config(frp->filehand_datas, MPXPLAY_DISKFILE_CFGFUNCNUM_SET_READWAIT, &readwait, NULL);
			return 1;
		}

		mpxplay_mpxinbuf_fclose(frp);
	}
	return 0;
}

static unsigned int mpxplay_mpxinbuf_fopen_write(void *fbds, char *filename)
{
	struct frame *frp = fbds;
	if(mpxinbuf_lowlevel_file_open_write(frp, filename)) {
		mpxinbuf_reset(frp);
		funcbit_smp_value_put(frp->filesize, mpxinbuf_lowlevel_file_length(frp));
		return 1;
	}
	return 0;
}

static void mpxplay_mpxinbuf_fclose(void *fbds)
{
	struct frame *frp = fbds;
	mpxinbuf_lowlevel_file_close(frp);
	mpxinbuf_reset(frp);
}

//-----------------------------------------------------------------------
//read & write

//#define MPXINBUF_DEBUG_FREAD 1

static long check_buffer_underrun(struct frame *frp, long num)
{
	if(frp->prebuffergetp < 0)
		funcbit_smp_value_put(frp->prebuffergetp, frp->prebuffergetp + frp->prebuffersize);
	if(frp->prebuffergetp < 0)
		funcbit_smp_value_put(frp->prebuffergetp, 0);
	if(frp->prebuffergetp >= frp->prebuffersize)
		funcbit_smp_value_put(frp->prebuffergetp, frp->prebuffergetp - frp->prebuffersize);
	if(frp->prebuffergetp >= frp->prebuffersize)
		funcbit_smp_value_put(frp->prebuffergetp, 0);

	if(frp->prebufferbytes_forward < 0)
		funcbit_smp_value_put(frp->prebufferbytes_forward, 0);
	if(frp->prebufferbytes_forward > frp->prebuffersize)
		funcbit_smp_value_put(frp->prebufferbytes_forward, frp->prebuffersize);

	if(frp->prebufferbytes_rewind > (frp->prebuffersize - frp->prebufferbytes_forward))	// ???
		funcbit_smp_value_put(frp->prebufferbytes_rewind, frp->prebuffersize - frp->prebufferbytes_forward);

	if(num > frp->prebufferbytes_forward)	// required!
		num = frp->prebufferbytes_forward;

	return num;
}

static unsigned long mpxinbuf_buffer_read(struct frame *frp, char *ptr, unsigned int num)
{
	long i = check_buffer_underrun(frp, num);
	if(i) {
		if((frp->prebuffergetp + i) >= frp->prebuffersize) {
			long j = frp->prebuffersize - frp->prebuffergetp;
			pds_memcpy(ptr, &frp->prebufferbegin[frp->prebuffergetp], j);
			pds_memcpy(ptr + j, &frp->prebufferbegin[0], i - j);
			funcbit_smp_value_put(frp->prebuffergetp, i - j);
		} else {
			pds_memcpy(ptr, &frp->prebufferbegin[frp->prebuffergetp], i);
			funcbit_smp_value_put(frp->prebuffergetp, frp->prebuffergetp + i);
		}
		funcbit_smp_value_put(frp->prebufferbytes_forward, frp->prebufferbytes_forward - i);
		funcbit_smp_value_put(frp->prebufferbytes_rewind, frp->prebufferbytes_rewind + i);
	}
	if(i < num)
		pds_memset(ptr + i, 0, num - i);
	return i;
}

static unsigned long mpxplay_mpxinbuf_read_unbuffered(struct frame *frp, char *ptr, unsigned int num)
{
	unsigned long bytes;
	funcbit_smp_value_put(frp->prebuffergetp, 0);
	funcbit_smp_value_put(frp->prebufferputp, 0);
	funcbit_smp_value_put(frp->prebufferbytes_rewind, 0);
	funcbit_smp_value_put(frp->prebufferbytes_forward, 0);
	bytes = mpxinbuf_lowlevel_file_read(frp, ptr, num);
	return bytes;
}

static unsigned long mpxplay_mpxinbuf_read_buffered(struct frame *frp, char *ptr, unsigned int num)
{
	unsigned long bytes = 0;
	while(num) {
		unsigned int curr;
		if((num > frp->prebufferbytes_forward) && (num < frp->prebufferblocksize))	// !!!
			mpxplay_mpxinbuf_buffer_check(frp);
		curr = mpxinbuf_buffer_read(frp, ptr + bytes, num);
		if(!curr) {
			// !!! assumes that the file pointer is at the end_of_buffer+1 (the end of buffer is at the file pointer)
			bytes += mpxplay_mpxinbuf_read_unbuffered(frp, ptr + bytes, num);
			break;
		}
		num -= curr;
		bytes += curr;
	}
	return bytes;
}

static unsigned long mpxplay_mpxinbuf_fread(void *fbds, void *ptr, unsigned long num)
{
	struct frame *frp = fbds;
	unsigned long bytes = 0;
#ifdef MPXINBUF_DEBUG_FREAD
	if(num > 4)
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "freadbeg pp:%5d gp:%5d r:%5d f:%5d n:%d fp:%d", frp->prebufferputp, frp->prebuffergetp, frp->prebufferbytes_rewind, frp->prebufferbytes_forward, num,
					   mpxplay_mpxinbuf_ftell(frp));
#endif
	switch (frp->buffertype & PREBUFTYPE_MASK) {
	case PREBUFTYPE_NONE:
		bytes = mpxplay_mpxinbuf_read_unbuffered(frp, ptr, num);
		break;
	case PREBUFTYPE_RING:
//#ifndef MPXINBUF_USE_CRITSEC // for FFMPEG/MKV
//#ifdef __DOS__
		if(frp->buffertype & PREBUFTYPE_INT)
			bytes = mpxinbuf_buffer_read(frp, ptr, num);
		else
//#endif
			bytes = mpxplay_mpxinbuf_read_buffered(frp, ptr, num);
		break;
	case PREBUFTYPE_FULL:
		bytes = mpxinbuf_buffer_read(frp, ptr, num);
		break;
	}
#ifdef MPXINBUF_DEBUG_FREAD
	if(num > 4)
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "freadend pp:%5d gp:%5d r:%5d f:%5d n:%d", frp->prebufferputp, frp->prebuffergetp, frp->prebufferbytes_rewind, frp->prebufferbytes_forward, num);
#endif
	return bytes;
}

static long mpxplay_mpxinbuf_fwrite(void *fbds, void *ptr, unsigned long num)
{
	struct frame *frp = fbds;
	return mpxinbuf_lowlevel_file_write(frp, ptr, num);
}

//-------------------------------------------------------------------------
// seek
//#define MPXINBUF_DEBUG_SEEK 1

static long mpxplay_mpxinbuf_seek_unbuffered(struct frame *frp, long newpos_absolute)
{
#ifdef MPXINBUF_DEBUG_SEEK
	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mpxinbuf seek unbuffered");
#endif
	funcbit_smp_value_put(frp->prebuffergetp, 0);
	funcbit_smp_value_put(frp->prebufferputp, 0);
	funcbit_smp_value_put(frp->prebufferbytes_rewind, 0);
	funcbit_smp_value_put(frp->prebufferbytes_forward, 0);
	return mpxinbuf_lowlevel_file_seek(frp, newpos_absolute);
}

static long mpxplay_mpxinbuf_ringbuffer_seek(struct frame *frp, long newpos_absolute, long newpos_relative)
{
	long retcode, pbr = frp->prebufferbytes_rewind, pbf = frp->prebufferbytes_forward, pbg;
	if(((newpos_relative > 0) && (newpos_relative < pbf))
	   || ((newpos_relative < 0) && ((-newpos_relative) <= pbr))) {
		pbr += newpos_relative;
		pbf -= newpos_relative;
		pbg = frp->prebuffergetp + newpos_relative;
		if(pbg >= frp->prebuffersize)	// at forward
			pbg -= frp->prebuffersize;
		if(pbg < 0)				// at rewind
			pbg += frp->prebuffersize;
		funcbit_smp_value_put(frp->prebuffergetp, pbg);
		retcode = newpos_absolute;
	} else {
		pbf = pbr = 0;
		retcode = MPXPLAY_ERROR_MPXINBUF_SEEK_BUF;
	}
	funcbit_smp_value_put(frp->prebufferbytes_rewind, pbr);
	funcbit_smp_value_put(frp->prebufferbytes_forward, pbf);
#ifdef MPXINBUF_DEBUG_SEEK
	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "ringbuffer seek g:%5d r:%5d f:%5d", frp->prebuffergetp, frp->prebufferbytes_rewind, frp->prebufferbytes_forward);
#endif
	return retcode;
}

static long mpxplay_mpxinbuf_fullbuffer_seek(struct frame *frp, long newpos_absolute)
{
	if(newpos_absolute < frp->prebuffersize) {
		funcbit_smp_value_put(frp->prebuffergetp, newpos_absolute);
		funcbit_smp_value_put(frp->prebufferbytes_rewind, newpos_absolute);
		funcbit_smp_value_put(frp->prebufferbytes_forward, (frp->prebuffersize - newpos_absolute));
		return newpos_absolute;
	}
	return MPXPLAY_ERROR_MPXINBUF_SEEK_EOF;
}

static long mpxplay_mpxinbuf_ringbuffer_advance(struct frame *frp, long newpos_absolute)
{
#ifdef MPXINBUF_DEBUG_SEEK
	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mpxinbuf advance begin");
#endif
	if((newpos_absolute > frp->filepos) && (newpos_absolute < (frp->filepos + frp->prebufferblocksize))) {
		long advance = newpos_absolute - frp->filepos;
		long newgp = frp->prebufferputp, pbr;
		pbr = frp->prebufferbytes_rewind + frp->prebufferbytes_forward;
		if(pbr > (frp->prebuffersize - frp->prebufferblocksize))
			pbr = frp->prebuffersize - frp->prebufferblocksize;
		funcbit_smp_value_put(frp->prebufferbytes_rewind, pbr);
		funcbit_smp_value_put(frp->prebufferbytes_forward, 0);
#ifdef MPXINBUF_DEBUG_SEEK
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "advance_rbf pbr:%5d ad:%d", pbr, advance);
#endif
		mpxinbuf_ringbuffer_fill(frp);
		if(frp->prebufferbytes_forward <= advance)
			return MPXPLAY_ERROR_MPXINBUF_SEEK_LOW;
		funcbit_smp_value_put(frp->prebuffergetp, (newgp + advance));
		funcbit_smp_value_put(frp->prebufferbytes_forward, (frp->prebufferbytes_forward - advance));
		funcbit_smp_value_put(frp->prebufferbytes_rewind, (frp->prebufferbytes_rewind + advance));
	} else {
#ifdef MPXINBUF_DEBUG_SEEK
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "advance_su na:%d", newpos_absolute);
#endif
		if(mpxplay_mpxinbuf_seek_unbuffered(frp, newpos_absolute) < 0)
			return MPXPLAY_ERROR_MPXINBUF_SEEK_LOW;
		mpxinbuf_ringbuffer_fill(frp);
		if(!frp->prebufferbytes_forward)
			return MPXPLAY_ERROR_MPXINBUF_SEEK_LOW;
	}
	return newpos_absolute;
}

static long mpxplay_mpxinbuf_fseek(void *fbds, long offset, int whence)
{
	struct frame *frp = fbds;
	long newpos_absolute, newpos_relative, np;

	switch (whence) {
	case SEEK_SET:
		newpos_absolute = offset;
		newpos_relative = offset - mpxplay_mpxinbuf_ftell(frp);
		break;
	case SEEK_CUR:
		newpos_absolute = mpxplay_mpxinbuf_ftell(frp) + offset;
		newpos_relative = offset;
		break;
	case SEEK_END:
		newpos_absolute = mpxplay_mpxinbuf_filelength(frp) + offset;
		newpos_relative = newpos_absolute - mpxplay_mpxinbuf_ftell(frp);
		break;
	default:
		return MPXPLAY_ERROR_MPXINBUF_SEEK_EOF;	// program should never reach this point
	}

#ifdef MPXINBUF_DEBUG_SEEK
	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "fseekbeg na:%5d nr:%5d pp:%5d gp:%5d r:%5d f:%5d", newpos_absolute, newpos_relative, frp->prebufferputp, frp->prebuffergetp, frp->prebufferbytes_rewind,
				   frp->prebufferbytes_forward);
#endif

	if(!newpos_relative && !(frp->buffertype & PREBUFTYPE_FULL))
		goto err_out_fseek;

	if((newpos_absolute < 0) || (newpos_absolute > mpxplay_mpxinbuf_filelength(frp))) {
		newpos_absolute = MPXPLAY_ERROR_MPXINBUF_SEEK_EOF;
		goto err_out_fseek;
	}

	switch (frp->buffertype & PREBUFTYPE_MASK) {
	case PREBUFTYPE_NONE:
		return mpxplay_mpxinbuf_seek_unbuffered(frp, newpos_absolute);
	case PREBUFTYPE_FULL:
		return mpxplay_mpxinbuf_fullbuffer_seek(frp, newpos_absolute);
	case PREBUFTYPE_RING:
		np = mpxplay_mpxinbuf_ringbuffer_seek(frp, newpos_absolute, newpos_relative);
		if(np >= 0) {
			newpos_absolute = np;
			goto err_out_fseek;
		}
//#ifndef MPXINBUF_USE_CRITSEC // for FFMPEG/MKV
//#ifdef __DOS__
		if(frp->buffertype & PREBUFTYPE_INT) {
			funcbit_smp_filesize_put(frp->prebuffer_seek_retry, newpos_absolute);
			return np;
		}
//#endif
		if(frp->buffertype & PREBUFTYPE_WRITEPROTECT)
			return mpxplay_mpxinbuf_seek_unbuffered(frp, newpos_absolute);
		newpos_absolute = mpxplay_mpxinbuf_ringbuffer_advance(frp, newpos_absolute);
#ifdef MPXINBUF_DEBUG_SEEK
		mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mpxinbuf rba_end:%d", newpos_absolute);
#endif
		return newpos_absolute;
	}

  err_out_fseek:
#ifdef MPXINBUF_DEBUG_SEEK
	mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "fseekend na:%5d nr:%5d pp:%5d gp:%5d r:%5d f:%5d fp:%d", newpos_absolute, newpos_relative, frp->prebufferputp, frp->prebuffergetp, frp->prebufferbytes_rewind,
				   frp->prebufferbytes_forward, mpxplay_mpxinbuf_ftell(frp));
#endif
	return newpos_absolute;
}

long mpxplay_mpxinbuf_ftell(void *fbds)
{
	struct frame *frp = fbds;
	return (frp->filepos - frp->prebufferbytes_forward);
}

static long mpxplay_mpxinbuf_filelength(void *fbds)
{
	struct frame *frp = fbds;
	return (frp->filesize);
}

static int mpxplay_mpxinbuf_feof(void *fbds)
{
	struct frame *frp = fbds;
	return ((frp->filepos >= frp->filesize && !frp->prebufferbytes_forward) ? 1 : 0);
}

static int mpxplay_mpxinbuf_chsize(void *fbds, long offset)
{
	struct frame *frp = fbds;
	return mpxinbuf_lowlevel_file_chsize(frp, offset);
}

//-----------------------------------------------------------------------

static mpxp_uint32_t mpxplay_mpxinbuf_get_byte(void *fbds)
{
	mpxp_uint8_t tmp;
	if(mpxplay_mpxinbuf_fread(fbds, &tmp, 1) != 1)
		return 0;
	return ((mpxp_uint32_t) tmp);
}

static mpxp_uint32_t mpxplay_mpxinbuf_get_le16(void *fbds)
{
	mpxp_uint16_t tmp;
	if(mpxplay_mpxinbuf_fread(fbds, &tmp, 2) != 2)
		return 0;
	return ((mpxp_uint32_t) tmp);
}

static mpxp_uint32_t mpxplay_mpxinbuf_get_le32(void *fbds)
{
	mpxp_uint32_t tmp;
	if(mpxplay_mpxinbuf_fread(fbds, &tmp, 4) != 4)
		return 0;
	return tmp;
}

static mpxp_uint64_t mpxplay_mpxinbuf_get_le64(void *fbds)
{
	mpxp_uint64_t tmp;
	if(mpxplay_mpxinbuf_fread(fbds, &tmp, 8) != 8)
		return 0;
	return tmp;
}

static mpxp_uint32_t mpxplay_mpxinbuf_get_be16(void *fbds)
{
	mpxp_uint16_t tmp;
	if(mpxplay_mpxinbuf_fread(fbds, &tmp, 2) != 2)
		return 0;
	return (pds_bswap16(tmp));
}

static mpxp_uint32_t mpxplay_mpxinbuf_get_be32(void *fbds)
{
	mpxp_uint32_t tmp;
	if(mpxplay_mpxinbuf_fread(fbds, &tmp, 4) != 4)
		return 0;
	return (pds_bswap32(tmp));
}

static mpxp_uint64_t mpxplay_mpxinbuf_get_be64(void *fbds)
{
	mpxp_uint64_t tmp;
	tmp = ((mpxp_uint64_t) mpxplay_mpxinbuf_get_be32(fbds)) << 32;
	tmp |= (mpxp_uint64_t) mpxplay_mpxinbuf_get_be32(fbds);
	return tmp;
}

//------------------------------------------------------------------------
static struct frame fr_seek_helper;

struct frame *mpxplay_mpxinbuf_seekhelper_init(struct frame *frp)
{
	unsigned int intsoundcntrl_save;
	MPXPLAY_INTSOUNDDECODER_DISALLOW;
	pds_memcpy((void *)&fr_seek_helper, (void *)frp, sizeof(struct frame));
	MPXPLAY_INTSOUNDDECODER_ALLOW;
	funcbit_disable(fr_seek_helper.buffertype, PREBUFTYPE_INT);
	funcbit_enable(fr_seek_helper.buffertype, PREBUFTYPE_WRITEPROTECT);
	return (&fr_seek_helper);
}

void mpxplay_mpxinbuf_seekhelper_close(struct frame *frp)
{
	unsigned int intsoundcntrl_save;
	MPXPLAY_INTSOUNDDECODER_DISALLOW;
	funcbit_smp_value_put(frp->prebuffergetp, fr_seek_helper.prebuffergetp);
	funcbit_smp_value_put(frp->prebufferputp, fr_seek_helper.prebufferputp);
	funcbit_smp_value_put(frp->prebufferbytes_rewind, fr_seek_helper.prebufferbytes_rewind);
	funcbit_smp_value_put(frp->prebufferbytes_forward, fr_seek_helper.prebufferbytes_forward);
	funcbit_smp_value_put(frp->filepos, fr_seek_helper.filepos);
	MPXPLAY_INTSOUNDDECODER_ALLOW;
}
