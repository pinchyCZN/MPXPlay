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
//function: routines in newfunc.lib

#ifndef mpxplay_newfunc_h
#define mpxplay_newfunc_h

#include <stdio.h>

#ifdef __WATCOMC__
#define NEWFUNC_ASM 1
#endif

#if defined(WIN32) || defined(__WINDOWS_386__) || defined(__NT__)
#define MPXPLAY_WIN32 1
#define _WIN32_WINDOWS 0x0500
#include <windows.h>
//!!! unstable (but usually works), rather we use 1 cpu only (by SetProcessAffinityMask) for all threads
//#define MPXPLAY_USE_SMP 1
#endif

#ifdef WIN32
//#define _dos_findfirst
//#define _dos_findnext
//#define _dos_setfileattr SetFileAttributes
//#define _dos_getfileattr GetFileAttributes
#define delay(x) Sleep(x)
#define snprintf _snprintf
#define _handle _file
#define __far
#define __interrupt
#define S_IREAD        0x000400
#define S_IWRITE       0x000200
#define SH_COMPAT  _SH_DENYNO	/* compatibility mode   */
#define F_OK    0				/*  Test for existence of file  */
#define _A_VOLID 0x08
#define NAME_MAX   255
struct find_t {
	char reserved[21];			/* reserved for use by DOS    */
	char attrib;				/* attribute byte for file    */
	unsigned short wr_time;		/* time of last write to file */
	unsigned short wr_date;		/* date of last write to file */
	unsigned long size;			/* length of file in bytes    */
	char name[NAME_MAX + 1];	/* null-terminated filename   */
};

typedef __int64 mpxp_int64_t;
typedef unsigned __int64 mpxp_uint64_t;
#else							//WIN32
typedef long long mpxp_int64_t;
typedef unsigned long long mpxp_uint64_t;
#endif							//WIN32
typedef long mpxp_int32_t;
typedef unsigned long mpxp_uint32_t;
typedef short mpxp_int16_t;
typedef unsigned short mpxp_uint16_t;
typedef signed char mpxp_int8_t;
typedef unsigned char mpxp_uint8_t;
typedef float mpxp_float_t;
typedef double mpxp_double_t;
typedef char mpxp_char_t;
typedef long mpxp_filesize_t;
typedef unsigned long mpxp_ptrsize_t;	// !!! on 32 bits

#if defined(NEWFUNC_ASM) && defined(__WATCOMC__)
typedef char mpxp_float80_t[10];
void pds_float80_put(mpxp_float80_t * ptr, double val);
#pragma aux pds_float80_put parm[esi][8087] = "fstp tbyte ptr [esi]"
float pds_float80_get(mpxp_float80_t * ptr);
#pragma aux pds_float80_get parm[esi] value[8087] = "fld tbyte ptr [esi]"
#else
typedef long double mpxp_float80_t;	// !!! double (64bit) in WATCOMC
#define pds_float80_put(p,v) *(p)=(v)
#define pds_float80_get(p)   (*(p))
#endif

#ifdef NEWFUNC_ASM
 /*void pds_ftoi(mpxp_double_t,mpxp_int32_t *); // rather use -zro option at wcc386
    #pragma aux pds_ftoi parm [8087][esi] = "fistp dword ptr [esi]"
    void pds_fto64i(mpxp_double_t,mpxp_int64_t *);
    #pragma aux pds_fto64i parm [8087][esi] = "fistp qword ptr [esi]" */
#define pds_ftoi(ff,ii)   (*(ii)=(mpxp_int32_t)(ff))
#define pds_fto64i(ff,ii) (*(ii)=(mpxp_int64_t)(ff))
mpxp_uint32_t pds_bswap16(mpxp_uint32_t);
#pragma aux pds_bswap16 parm [eax] value [eax] = "xchg al,ah"
mpxp_uint32_t pds_bswap32(mpxp_uint32_t);
#if (_CPU_ >= 486) || (_M_IX86>=400)
#pragma aux pds_bswap32 parm [eax] value [eax] = "bswap eax"
#else
#pragma aux pds_bswap32 parm [eax] value [eax] = "xchg al,ah" "rol eax,16" "xchg al,ah"
#endif
void pds_cpu_hlt(void);
#pragma aux pds_cpu_hlt ="hlt"
#else
#define pds_ftoi(ff,ii)   (*ii=(mpxp_int32_t)ff)
#define pds_fto64i(ff,ii) (*ii=(mpxp_int64_t)ff)
#define pds_bswap16(a) (((a&0xff)<<8)|((a&0xff00)>>8))
#define pds_bswap32(a) ((a<<24)|((a&0xff00)<<8)|((a&0xff0000)>>8)|((a>>24)&0xff))
#define pds_cpu_hlt
#endif

#define funcbit_test(var,bit)       ((var)&(bit))
#define funcbit_enable(var,bit)     ((var)|=(bit))
#define funcbit_disable(var,bit)    ((var)&=~(bit))
#define funcbit_inverse(var,bit)    ((var)^=(bit))
#define funcbit_copy(var1,var2,bit) ((var1)|=(var2)&(bit))
//#define funcbit_copy(var1,var2,bit) ((var1)=((var1)&(~(bit)))|((var2)&(bit))

// note LE: lowest byte first, highest byte last
#define PDS_GETB_8S(p)   *((mpxp_int8_t *)p)	// signed 8 bit (1 byte)
#define PDS_GETB_8U(p)   *((mpxp_uint8_t *)p)	// unsigned 8 bit (1 byte)
#define PDS_GETB_LE16(p) *((mpxp_int16_t *)p)	// 2bytes LE to short
#define PDS_GETB_LEU16(p)*((mpxp_uint16_t *)p)	// 2bytes LE to unsigned short
#define PDS_GETB_BE16(p) pds_bswap16(*((mpxp_uint16_t *)p))	// 2bytes BE to unsigned short
#define PDS_GETB_LE32(p) *((mpxp_int32_t *)p)	// 4bytes LE to long
#define PDS_GETB_BE32(p) pds_bswap32(*((mpxp_uint32_t *)p))	// 4bytes BE to unsigned long
#define PDS_GETB_LE24(p) ((PDS_GETB_LE32(p))&0x00ffffff)
#define PDS_GETB_BE24(p) ((PDS_GETB_BE32(p))&0x00ffffff)
#define PDS_GETB_LE64(p) *((mpxp_int64_t *)p)	// 8bytes LE to int64
#define PDS_GET4C_LE32(a,b,c,d) ((mpxp_uint32_t)a | ((mpxp_uint32_t)b << 8) | ((mpxp_uint32_t)c << 16) | ((mpxp_uint32_t)d << 24))

#define PDS_PUTB_LE16(p,v) *((mpxp_int16_t *)p)=v	//
#define PDS_PUTB_BE16(p,v) *((mpxp_int16_t *)p)=pds_bswap16(v)	//
#define PDS_PUTB_LE32(p,v) *((mpxp_int32_t *)p)=v	// long to 4bytes LE
#define PDS_PUTB_BE32(p,v) *((mpxp_int32_t *)p)=pds_bswap32(v)	// long to 4bytes BE

#define pds_newfunc_regp_clear(regp) pds_memset(regp,0,sizeof(union REGPACK))
#define pds_newfunc_regs_clear(regs) pds_memset(regs,0,sizeof(union REGS))
#define pds_newfunc_sregs_clear(sregs) pds_memset(sregs,0,sizeof(struct SREGS))

#define TEXTCURSORSHAPE_NORMAL  0x0607
#define TEXTCURSORSHAPE_FULLBOX 0x0007
#define TEXTCURSORSHAPE_HIDDEN  0x2000

#define PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN '\\'
#define PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP '/'
#define PDS_DIRECTORY_SEPARATOR_STR_DOSWIN  "\\"
#define PDS_DIRECTORY_SEPARATOR_STR_UNXFTP  "/"

#if defined(__DOS__) || defined(WIN32) || defined(_WIN32) || defined(MPXPLAY_WIN32)
#define PDS_DIRECTORY_SEPARATOR_CHAR PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN
#define PDS_DIRECTORY_SEPARATOR_CHRO PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP
#define PDS_DIRECTORY_SEPARATOR_STR  PDS_DIRECTORY_SEPARATOR_STR_DOSWIN
#define PDS_DIRECTORY_ROOTDIR_STR "c:\\"
#else							// Linux
#define PDS_DIRECTORY_SEPARATOR_CHAR PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP
#define PDS_DIRECTORY_SEPARATOR_CHRO PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN
#define PDS_DIRECTORY_SEPARATOR_STR  PDS_DIRECTORY_SEPARATOR_STR_UNXFTP
#define PDS_DIRECTORY_ROOTDIR_STR "c:/"
#endif

#define PDS_DIRECTORY_DRIVESTRLENMAX 10	// have to be enough ("ftpes:" = 6)
#define PDS_DIRECTORY_DRIVE_STR   "c:"
#define PDS_DIRECTORY_ALLDIR_STR  "*.*"
#define PDS_DIRECTORY_ALLFILE_STR "*.*"

//uselfn
#define USELFN_ENABLED         1	// lfn is enabled
#define USELFN_AUTO_SFN_TO_LFN 2	// auto sfn to lfn conversion at playlists

typedef struct dosmem_t {
	unsigned short selector;
	unsigned short segment;
	char *linearptr;
} dosmem_t;

//for DOS interrupt callings
typedef struct rminfo {
	long EDI;
	long ESI;
	long EBP;
	long reserved_by_system;
	long EBX;
	long EDX;
	long ECX;
	long EAX;
	short flags;
	short ES, DS, FS, GS, IP, CS, SP, SS;
} rminfo;

#define RMINFO_FLAG_CARRY 1

typedef struct pds_fdate_t {
	unsigned short twosecs:5;
	unsigned short minutes:6;
	unsigned short hours:5;
	unsigned short day:5;
	unsigned short month:4;
	unsigned short year:7;
} pds_fdate_t;

typedef struct pds_find_t {
	void *ff_data;				// lfn_find_t or pds_dos_find_t
	long ff_handler;			// _findfirst
	unsigned int attrib;		// attribute byte for file
	struct pds_fdate_t cdate;	// creation date/time
	struct pds_fdate_t fdate;	// date of last write to file (modification time)
	struct pds_fdate_t adate;	// access date/time
	mpxp_filesize_t size;		// filesize
	char name[300];				// null-terminated filename
} pds_find_t;

typedef struct pds_subdirscan_t {
	struct pds_find_t *ff;		// for finding files in current dir
	struct pds_find_t *subdir_ff_datas;	// for finding next (sub)dir
	char **subdir_names;		// saved subdir names (to rebuild currdir at level change)
	char **subdir_masks;		// non constant directory names given in startdir
	unsigned int nb_subdirmasks;
	int subdir_level, subdir_reach, subfile_reach;
	unsigned int flags;			// DIRSCAN_FLAG_
	unsigned int scan_attrib;	// argument
	char subdirmasks[300];		// subdir_masks are pointed here
	char startdir[300];			// argument
	char scan_names[300];		// argument (ie: *.*)
	char currdir[300];			// in search
	char prevdir[300];
	char fullname[300];			// currdir + ff->name
} pds_subdirscan_t;

//newfunc.c
extern void newfunc_init(char *);
extern void newfunc_close(void);

//bitstrm.c
typedef struct mpxplay_bitstreambuf_s {
	unsigned long bitpos;		// we assume that we never store more than 4Gbits
	unsigned long storedbits;	// detto
	unsigned char *buffer;		// begin of buffer
	unsigned long bufsize;
} mpxplay_bitstreambuf_s;

extern struct mpxplay_bitstreambuf_s *mpxplay_bitstream_alloc(unsigned int required_bufsize);
extern void mpxplay_bitstream_free(struct mpxplay_bitstreambuf_s *bs);
extern void mpxplay_bitstream_init(struct mpxplay_bitstreambuf_s *bs, unsigned char *data, unsigned int bytes);
extern int mpxplay_bitstream_fill(struct mpxplay_bitstreambuf_s *bs, struct mpxplay_filehand_buffered_func_s *fbfs, void *fbds, unsigned int needbytes);	// fill with a file-read
extern unsigned int mpxplay_bitstream_putbytes(struct mpxplay_bitstreambuf_s *bs, unsigned char *srcbuf, unsigned int newbytes);	// fill with (new) bytes from a buffer
extern void mpxplay_bitstream_reset(struct mpxplay_bitstreambuf_s *bs);

extern unsigned char *mpxplay_bitstream_getbufpos(struct mpxplay_bitstreambuf_s *bs);
extern int mpxplay_bitstream_lookbytes(struct mpxplay_bitstreambuf_s *bs, unsigned char *destbuf, unsigned int needbytes);
extern int mpxplay_bitstream_readbytes(struct mpxplay_bitstreambuf_s *bs, unsigned char *destbuf, unsigned int needbytes);
extern int mpxplay_bitstream_skipbytes(struct mpxplay_bitstreambuf_s *bs, int skipbytes);
extern long mpxplay_bitstream_leftbytes(struct mpxplay_bitstreambuf_s *bs);

extern mpxp_uint32_t mpxplay_bitstream_get_byte(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_le16(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_le32(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint64_t mpxplay_bitstream_get_le64(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_be16(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_be32(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint64_t mpxplay_bitstream_get_be64(struct mpxplay_bitstreambuf_s *bs);

extern int mpxplay_bitstream_getbit1_be(struct mpxplay_bitstreambuf_s *bs);
extern long mpxplay_bitstream_getbits_be24(struct mpxplay_bitstreambuf_s *bs, unsigned int bits);
extern mpxp_uint32_t mpxplay_bitstream_getbits_ube32(struct mpxplay_bitstreambuf_s *bs, unsigned int bits);
extern mpxp_int64_t mpxplay_bitstream_getbits_be64(struct mpxplay_bitstreambuf_s *bs, unsigned int bits);
extern int mpxplay_bitstream_skipbits(struct mpxplay_bitstreambuf_s *bs, int bits);	// bits can be negative, return MPXPLAY_ERROR_MPXINBUF_nnn
extern long mpxplay_bitstream_leftbits(struct mpxplay_bitstreambuf_s *bs);

//cpu.c
#define CPU_X86_STDCAP_MSR     (1<< 5)	// Model-Specific Registers, RDMSR, WRMSR
#define CPU_X86_STDCAP_MTRR    (1<<12)	// Memory Type Range Registers
#define CPU_X86_STDCAP_PGE     (1<<13)	// Page Global Enable
#define CPU_X86_STDCAP_PAT     (1<<16)	// Page Attribute Table
#define CPU_X86_STDCAP_MMX     (1<<23)
#define CPU_X86_STDCAP_SSE1    (1<<25)
#define CPU_X86_STDCAP_SSE2    (1<<26)
#define CPU_X86_EXTCAP_MMXEXT  (1<<23)	// ??? 22 or 23
#define CPU_X86_EXTCAP_3DNOW2  (1<<30)
#define CPU_X86_EXTCAP_3DNOW1  (1<<31)
#define CPU_X86_MMCAP_MMX      0x0001	// standard MMX
#define CPU_X86_MMCAP_MMXEXT   0x0002	// SSE integer functions or AMD MMX ext
#define CPU_X86_MMCAP_3DNOW    0x0004	// AMD 3DNOW
#define CPU_X86_MMCAP_SSE      0x0008	// SSE functions
#define CPU_X86_MMCAP_SSE2     0x0010	// PIV SSE2 functions
#define CPU_X86_MMCAP_3DNOWEXT 0x0020	// AMD 3DNowExt

extern int mpxplay_cpu_capables_std, mpxplay_cpu_capables_ext, mpxplay_cpu_capables_mm;
extern void newfunc_cpu_init(void);
extern unsigned int pds_cpu_cpuid_test(void);
extern int pds_cpu_cpuid_get(unsigned int eax_p, int *ebx_p, int *ecx_p, int *edx_p);
extern unsigned int pds_cpu_mtrr_enable_wc(unsigned long phys_ptr, unsigned long size_kb);
extern void pds_cpu_mtrr_disable_wc(unsigned long phys_ptr);
extern void pds_fpu_setround_near(void);
extern void pds_fpu_setround_chop(void);

//dpmi.c
#define PDS_INT2X_DOSMEM_SIZE  512
extern long pds_dpmi_segment_to_selector(unsigned int segment);
extern void far *pds_dpmi_getrmvect(unsigned int intno);
extern void pds_dpmi_setrmvect(unsigned int intno, unsigned int segment, unsigned int offset);
extern void far *pds_dpmi_getexcvect(unsigned int intno);
extern void pds_dpmi_setexcvect(unsigned int intno, void far * vect);
extern void far *pds_dos_getvect(unsigned int intno);
extern void pds_dos_setvect(unsigned int intno, void far * vect);
extern int pds_dpmi_dos_allocmem(dosmem_t *, unsigned int size);
extern void pds_dpmi_dos_freemem(dosmem_t *);
extern void pds_dpmi_realmodeint_call(unsigned int intnum, struct rminfo *rmi);
extern unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr, unsigned long memsize);
extern void pds_dpmi_unmap_physycal_memory(unsigned long linear_address);
#define pds_dpmi_rmi_clear(rmi) pds_memset(rmi,0,sizeof(struct rminfo))

//drivehnd.c
#define DRIVE_TYPE_NONE    0	// compatible values with win32 GetDriveType()
#define DRIVE_TYPE_NOROOT  1
#define DRIVE_TYPE_FLOPPY  2	// removable
#define DRIVE_TYPE_HD      3
#define DRIVE_TYPE_NETWORK 4	// remote
#define DRIVE_TYPE_CD      5
#define DRIVE_TYPE_RAMDISK 6
#define DRIVE_TYPE_VIRTUAL 32	// far remote
#define SUBDIRSCAN_FLAG_SUBDIR  1	// we was not here yet
#define SUBDIRSCAN_FLAG_UPDIR   2	//
extern unsigned int pds_int2x_dosmems_allocate(void);
extern void pds_int2x_dosmems_free(void);
extern void pds_check_lfnapi(char *prgname);
//extern void pds_dir_save(unsigned int *savedrive,char *savedir);
//extern void pds_dir_restore(unsigned int savedrive,char *savedir);
extern void pds_fullpath(char *fullname, char *name);
extern void pds_getcwd(char *pathbuf);	// pathbuf must be min. 300 chars long
extern void pds_getdcwd(int drive, char *pathbuf);	// pathbuf must be min. 300 chars long
extern int pds_chdir(char *setdir);
extern int pds_mkdir(char *newdirname);
extern int pds_rmdir(char *dirname);
extern int pds_rename(char *oldfilename, char *newfilename);
extern int pds_unlink(char *filename);
extern int pds_fileattrib_reset(char *filename, unsigned int clearflags);
extern int pds_fileattrib_get(char *filename);
extern unsigned int pds_findfirst(char *path, int attrib, struct pds_find_t *ffblk);
extern unsigned int pds_findnext(struct pds_find_t *ffblk);
extern void pds_findclose(struct pds_find_t *ffblk);
extern unsigned int pds_subdirscan_open(char *path_and_filename, unsigned int attrib, struct pds_subdirscan_t *dsi);
extern int pds_subdirscan_findnextfile(struct pds_subdirscan_t *dsi);
extern void pds_subdirscan_close(struct pds_subdirscan_t *dsi);
extern mpxp_filesize_t pds_getfilesize(char *filename);
extern void pds_truename_dos(char *shortname, char *name);
extern unsigned int pds_getdrive(void);
extern void pds_setdrive(int drivenum);
extern void pds_drivesinfo(unsigned int *currdrive, unsigned int *lastdrive);
extern void pds_drive_getvolumelabel(unsigned int drive, char *labelbuf, unsigned int labbuflen);
extern unsigned int pds_chkdrive(unsigned int drive);
extern unsigned int pds_dir_exists(char *path);
extern unsigned int pds_network_check(void);
extern int pds_network_query_assign(unsigned int index, char *drvname, unsigned int maxdrvlen, char *volname, unsigned int maxvollen);
extern void pds_drives_flush(void);
extern void pds_sfn_limit(char *fn);
extern int pds_access(char *path, int amode);

//errorhnd.c
extern void newfunc_errorhnd_int24_init(void);
extern void newfunc_error_handlers_init(void);
extern void newfunc_error_handlers_close(void);
extern void newfunc_exception_handlers_close(void);
//extern void pds_mswin_previousinstance_close(void);

//filehand.c
#define MPXPLAY_ERROR_FILEHAND_OK             0
#define MPXPLAY_ERROR_FILEHAND_USERABORT   -200
#define MPXPLAY_ERROR_FILEHAND_MEMORY      -201
#define MPXPLAY_ERROR_FILEHAND_CANTOPEN    -202
#define MPXPLAY_ERROR_FILEHAND_CANTCREATE  -203
#define MPXPLAY_ERROR_FILEHAND_CANTREAD    -204
#define MPXPLAY_ERROR_FILEHAND_CANTWRITE   -205
#define MPXPLAY_ERROR_FILEHAND_CANTSEEK    -206
#define MPXPLAY_ERROR_FILEHAND_DELETE      -210
#define MPXPLAY_ERROR_FILEHAND_RENAME      -211
#define MPXPLAY_ERROR_FILEHAND_REMOVEDIR   -212
#define MPXPLAY_ERROR_FILEHAND_CHANGEATTR  -213
#define MPXPLAY_ERROR_FILEHAND_CANTPERFORM -214
#define MPXPLAY_ERROR_FILEHAND_CANTCOPY    -215
#define MPXPLAY_ERROR_FILEHAND_COPYDIR     -216
#define MPXPLAY_ERROR_FILEHAND_SAMEFILE    -217
#define MPXPLAY_ERROR_FILEHAND_SAMEDIR     -218
#define MPXPLAY_ERROR_FILEHAND_MULTITO1    -219
#define MPXPLAY_ERROR_FILEHAND_SKIPFILE    -220	// not error, just sign

extern void pds_indosflag_init(void);
extern unsigned int pds_indos_flag(void);
extern unsigned int pds_filehand_check_infilehand(void);
extern unsigned int pds_filehand_check_entrance(void);
extern void pds_filehand_lock_entrance(void);
extern void pds_filehand_unlock_entrance(void);
extern int pds_open_read(char *filename, unsigned int mode);	// mode: O_RDONLY| O_BINARY|O_TEXT
extern int pds_open_write(char *filename, unsigned int mode);	//       O_RDWR| O_BINARY|O_TEXT
extern int pds_open_create(char *filename, unsigned int mode);	//      O_RDWR| O_BINARY|O_TEXT
extern int pds_dos_read(int, char *, unsigned int);
extern int pds_dos_write(int, char *, unsigned int);
extern void pds_close(int);
extern mpxp_filesize_t pds_lseek(int, mpxp_filesize_t, int);
extern mpxp_filesize_t pds_tell(int);
extern int pds_eof(int);
extern mpxp_filesize_t pds_filelength(int);
extern int pds_chsize(int filehand, mpxp_filesize_t size);
extern FILE *pds_fopen(char *, char *);
extern int pds_fclose(FILE * fp);

//keyboard.c
extern void newfunc_keyboard_init(void);
extern unsigned int pds_kbhit(void);
extern unsigned int pds_extgetch(void);
extern unsigned int pds_look_extgetch(void);
extern void pds_pushkey(unsigned int);
extern unsigned int newfunc_keyboard_char_to_extkey(char c);
extern char newfunc_keyboard_extkey_to_char(unsigned short extkey);
extern unsigned int newfunc_keyboard_winkey_to_extkey(unsigned int control, unsigned int virtkeycode, char asciicode);

//memory.c
extern void newfunc_memory_init(void);
#ifdef NEWFUNC_ASM
extern void pds_memcpy(void *, void *, unsigned int);
extern void pds_memset(void *, int, unsigned int);
extern void pds_qmemreset(void *, unsigned int);
extern void pds_qmemcpy(void *, void *, unsigned int);
extern void pds_qmemcpyr(void *, void *, unsigned int);
#else
#include <string.h>
#define newfunc_memory_init() {}
#define pds_memcpy(t,s,l)   memcpy(t,s,l)
#define pds_memset(t,v,l)   memset(t,v,l)
#define pds_qmemreset(t,l)  memset(t,0,l*4)
#define pds_qmemcpy(t,s,l)  memcpy(t,s,l*4)
#define pds_qmemcpyr(t,s,l) memmove(t,s,l*4)
#endif
extern void pds_memxch(char *, char *, unsigned int);
extern void pds_mem_reverse(char *addr, unsigned int len);
#ifdef MPXPLAY_USE_SMP
extern void pds_smp_memcpy(char *addr_dest, char *addr_src, unsigned int len);
extern void pds_smp_memxch(char *addr1, char *addr2, unsigned int len);
extern void pds_smp_memset(char *addr, int value, unsigned int len);
#define pds_smp_memrefresh(addr,len) pds_smp_memcpy(addr,addr,len)
#else
#define pds_smp_memrefresh(addr,len)
#define pds_smp_memcpy(t,s,l) pds_memcpy(t,s,l)
#define pds_smp_memxch(t,s,l) pds_memxch(t,s,l)
#define pds_smp_memset(t,v,l) pds_memset(t,v,l)
#endif
extern void *pds_malloc(unsigned int bufsize);
extern void *pds_calloc(unsigned int nitems, unsigned int itemsize);
extern void *pds_realloc(void *bufptr, unsigned int bufsize);
extern void pds_free(void *bufptr);

//mixed.c
extern unsigned long pds_rand(int);
extern unsigned int pds_mswin_getver(void);
extern void pds_mswin_setapplicationtitle(char *);
extern void pds_shutdown_atx(void);

//string.c
extern unsigned int pds_strcpy(char *, char *);	// returns the length of string!
extern unsigned int pds_strmove(char *dest, char *src);	// returns the length of string
extern unsigned int pds_strncpy(char *dest, char *src, unsigned int maxlen);	// returns the length of string!
extern unsigned int pds_strcat(char *, char *);	// returns the lenght of string!
extern int pds_strcmp(char *strp1, char *strp2);
extern int pds_stricmp(char *, char *);
extern int pds_strricmp(char *, char *);
extern int pds_strlicmp(char *, char *);
extern int pds_strncmp(char *, char *, unsigned int);
extern int pds_strnicmp(char *strp1, char *strp2, unsigned int counter);
extern unsigned int pds_strlen(char *strp);
extern unsigned int pds_strlenc(char *strp, char seek);
//extern unsigned int pds_strlencn(char *strp,char seek,unsigned int maxlen);
extern char *pds_strchr(char *, char);
extern char *pds_strrchr(char *, char);
extern char *pds_strnchr(char *strp, char seek, unsigned int len);
extern char *pds_strstr(char *s1, char *s2);
extern char *pds_strstri(char *s1, char *s2);
extern unsigned int pds_strcutspc(char *src);	// returns (new)len
extern unsigned int pds_str_fixlenc(char *str, unsigned int newlen, char c);	// returns (new)len
extern void pds_listline_slice(char **listparts, char *cutchars, char *listline);
extern unsigned int pds_chkstr_uppercase(char *str);
extern void pds_ltoa(int, char *);
extern void pds_ltoa16(int, char *);
extern long pds_atol(char *);
extern mpxp_int64_t pds_atoi64(char *);
extern long pds_atol16(char *);
extern char *pds_getfilename_from_fullname(char *fullname);
extern void pds_getfilename_noext_from_fullname(char *strout, char *fullname);
extern char *pds_getpath_from_fullname(char *path, char *fullname);
extern char *pds_filename_get_extension(char *fullname);
extern unsigned int pds_filename_conv_slashes_to_local(char *filename);
extern unsigned int pds_filename_conv_slashes_to_unxftp(char *filename);
extern void pds_filename_conv_forbidden_chars(char *filename);
extern int pds_getdrivenum_from_path(char *path);
extern unsigned int pds_path_is_dir(char *path);
extern unsigned int pds_filename_check_absolutepath(char *path);
extern unsigned int pds_filename_remove_relatives(char *filename);
extern unsigned int pds_filename_build_fullpath(char *destbuf, char *currdir, char *filename);
extern unsigned int pds_filename_assemble_fullname(char *destbuf, char *path, char *name);
extern unsigned int pds_filename_wildchar_chk(char *filename);
extern unsigned int pds_filename_wildchar_cmp(char *fullname, char *mask);
extern unsigned int pds_stri_wildchar_cmp(char *str, char *mask);
extern unsigned int pds_filename_wildchar_rename(char *destname, char *srcname, char *mask);

//textdisp.c
#define TEXTSCREEN_DEFAULT_MODE   3	// 80x25
#define TEXTSCREEN_BYTES_PER_CHAR 2	// char + color
extern void newfunc_textdisplay_init(void);
extern void newfunc_textdisplay_close(void);
extern void pds_textdisplay_charxy(unsigned int, unsigned int, unsigned int, char);
#define pds_textdisplay_charxybk(color,bkcolor,outx,outy,c) (pds_textdisplay_charxy((bkcolor<<4)|color,outx,outy,c))
extern unsigned int pds_textdisplay_textxy(unsigned int, unsigned int, unsigned int, char *);
#define pds_textdisplay_textxybk(color,bkcolor,outx,outy,string) (pds_textdisplay_textxy((bkcolor<<4)|color,outx,outy,string))
extern unsigned int pds_textdisplay_textxyn(unsigned int color, unsigned int outx, unsigned int outy, unsigned int maxlen, char *string_s);
#define pds_textdisplay_textxynbk(color,bkcolor,outx,outy,maxlen,string) (pds_textdisplay_textxyn((bkcolor<<4)|color,outx,outy,maxlen,string))
extern void pds_textdisplay_textxyan(unsigned int x, unsigned int y, char *text, unsigned short *attribs, unsigned int len);
extern void pds_textdisplay_clrscr(void);
extern void pds_textdisplay_scrollup(unsigned int);
extern void pds_textdisplay_printf(char *);
extern unsigned int pds_textdisplay_getbkcolorxy(unsigned int, unsigned int);
extern unsigned int pds_textdisplay_lowlevel_getbkcolorxy(unsigned int, unsigned int);
extern void pds_textdisplay_setcolorxy(unsigned int, unsigned int, unsigned int);
extern void pds_textdisplay_setbkcolorxy(unsigned int, unsigned int, unsigned int);
extern void pds_textdisplay_lowlevel_setbkcolorxy(unsigned int, unsigned int, unsigned int);
extern void pds_textdisplay_spacecxyn(unsigned int, unsigned int, unsigned int, unsigned int);
#define pds_textdisplay_char2buf(color,bufptr,c) *(bufptr)=((unsigned short)c)|((unsigned short)color<<8)
#define pds_textdisplay_char2bufbk(color,bkcolor,bufptr,c) *(bufptr)=((unsigned short)c)|((unsigned short)color<<8)|((unsigned short)bkcolor<<12)
extern unsigned int pds_textdisplay_text2buf(unsigned int color, unsigned short *buf, unsigned int buflen, char *string_s);
#define pds_textdisplay_text2bufbk(color,bkcolor,buf,buflen,string) (pds_textdisplay_text2buf((bkcolor<<4)|color,buf,buflen,string))
extern unsigned int pds_textdisplay_text2field(unsigned int color, unsigned short *buf, char *string_s);
#define pds_textdisplay_text2fieldbk(color,bkcolor,buf,string) (pds_textdisplay_text2field((bkcolor<<4)|color,buf,string))
extern void pds_textdisplay_textbufxyn(unsigned int outx, unsigned int outy, unsigned short *buf, unsigned int maxlen);
extern void pds_textdisplay_consolevidmem_merge(unsigned short *attribs, char *text, char *destbuf, unsigned int bufsize);
extern void pds_textdisplay_consolevidmem_separate(unsigned short *attribs, char *text, char *srcbuf, unsigned int bufsize);
extern void pds_textdisplay_consolevidmem_read(unsigned short *attribs, char *text, unsigned int maxsize);
extern void pds_textdisplay_consolevidmem_write(unsigned short *attribs, char *text, unsigned int maxsize);
extern void pds_textdisplay_consolevidmem_xywrite(unsigned int x, unsigned int y, unsigned short *attribs, char *text, unsigned int len);
extern void pds_textdisplay_vidmem_save(void);
extern void pds_textdisplay_vidmem_restore(void);
extern unsigned int pds_textdisplay_setmode(unsigned int mode);
extern unsigned int pds_textdisplay_getmode(void);
extern unsigned int pds_textdisplay_setlastmode(void);
extern void pds_textdisplay_setresolution(unsigned int lines);
extern void pds_textdisplay_getresolution(void);
extern unsigned int pds_textdisplay_getcursor_y(void);
extern void pds_textdisplay_setcursor_position(unsigned int x, unsigned int y);
extern void pds_textdisplay_setcursorshape(long);
extern void pds_textdisplay_resetcolorpalette(void);
//extern void redefine_fonts_8x8(void);
//extern void redefine_fonts_8x16(void);

//time.c
extern unsigned long pds_gettimeh(void);	// hsec
extern mpxp_uint64_t pds_gettimem(void);	// msec
extern mpxp_uint64_t pds_gettimeu(void);	// usec
extern unsigned long pds_gettime(void);
extern unsigned long pds_getdate(void);
extern unsigned long pds_strtime_to_hextime(char *timestr, unsigned int houralign);
extern unsigned long pds_strtime_to_hexhtime(char *timestr);
#define PDS_HEXTIME_TO_SECONDS(t) ((t>>16)*3600+((t>>8)&0xff)*60+(t&0xff))
#define PDS_HEXHTIME_TO_HSECONDS(t) ((t>>24)*360000+((t>>16)&0xff)*6000+((t>>8)&0xff)*100+(t&0xff))
extern void pds_delay_10us(unsigned int ticks);

//timer.c
#define MPXPLAY_TIMERTYPE_WAKEUP    0	// wake up once (delete after running)
#define MPXPLAY_TIMERTYPE_REPEAT    1	// repeat forever (don't delete after running)
#define MPXPLAY_TIMERTYPE_INT08     4	// run from int08 (else run from main_cycle)
#define MPXPLAY_TIMERTYPE_SIGNAL    8	// signalling (call func at mpxplay_signal_events) (cannot use it with INT08)

#define MPXPLAY_TIMERFLAG_MULTIPLY  64	// multiply instance (don't overwrite previous configured (same) function)
#define MPXPLAY_TIMERFLAG_LOWPRIOR 128	// runs in lower priority (check buffers fullness)
#define MPXPLAY_TIMERFLAG_BUSY     256	// busy flag (no re-entrance)
#define MPXPLAY_TIMERFLAG_OWNSTACK 512	// use a separated stack for the function (for int08 functions)
#define MPXPLAY_TIMERFLAG_STI     1024	// disable maskable interrupts while function running (for int08 functions)
#define MPXPLAY_TIMERFLAG_INDOS   2048	// function calls DOS routines (check indos flag before)
#define MPXPLAY_TIMERFLAG_MVPDATA 8192	// passdata=mvp

//mtf->signal_flags
#define MPXPLAY_SIGNALEVENT_EXECUTE_ONCE 0	// execute at event
#define MPXPLAY_SIGNALEVENT_EXECUTE_MORE 1	//
#define MPXPLAY_SIGNALEVENT_DELAY_START  8	// execute delay (timer) after event
#define MPXPLAY_SIGNALEVENT_DELAY_STOP  16	//
#define MPXPLAY_SIGNALEVENT_DELAY_RESET 32	//

//events at TIMERTYPE_SIGNAL (mpxplay_signal_events)
#define MPXPLAY_SIGNALTYPE_KEYBOARD      1
#define MPXPLAY_SIGNALTYPE_MOUSE         2
#define MPXPLAY_SIGNALTYPE_DISPMESSAGE  16	// display_message called
#define MPXPLAY_SIGNALTYPE_CLEARMESSAGE 32	// clear message called
#define MPXPLAY_SIGNALTYPE_DESKTOP      (MPXPLAY_SIGNALTYPE_DISPMESSAGE|MPXPLAY_SIGNALTYPE_CLEARMESSAGE)
#define MPXPLAY_SIGNALTYPE_NEWFILE     256
#define MPXPLAY_SIGNALTYPE_USER  (MPXPLAY_SIGNALTYPE_KEYBOARD|MPXPLAY_SIGNALTYPE_MOUSE)
#define MPXPLAY_SIGNALMASK_TIMER (MPXPLAY_SIGNALTYPE_KEYBOARD|MPXPLAY_SIGNALTYPE_MOUSE|MPXPLAY_SIGNALTYPE_DESKTOP|MPXPLAY_SIGNALTYPE_NEWFILE)
//other events (not used at TIMERTYPE_SIGNAL)
#define MPXPLAY_SIGNALTYPE_REALTIME   4096	// realtime (delay=0) function is in the timer (don't use cpu-halt)
#define MPXPLAY_SIGNALTYPE_DISKACCESS 8192	// file read/write
#define MPXPLAY_SIGNALMASK_OTHER (MPXPLAY_SIGNALTYPE_REALTIME|MPXPLAY_SIGNALTYPE_DISKACCESS)

extern unsigned long mpxplay_timer_secs_to_counternum(unsigned long secs);
extern int mpxplay_timer_addfunc(void *callback_func, void *callback_data, unsigned int timer_flags, unsigned int refresh_delay);	// returns -1 on error, else returns a handlernum_index value
extern int mpxplay_timer_modifyfunc(void *callback_func, int flags, int refresh_delay);	// if flags==-1 or refresh_delay==-1, then do not modify that value
extern int mpxplay_timer_modifyhandler(void *callback_func, int handlernum_index, int timer_flags, int refresh_delay);
extern void mpxplay_timer_deletefunc(void *callback_func, void *callback_data);
extern void mpxplay_timer_deletehandler(void *callback_func, int handlernum_index);	// used at TIMERFLAG_MULTIPLY
extern void mpxplay_timer_executefunc(void *func);
extern unsigned int mpxplay_timer_lowpriorstart_wait(void);
extern void mpxplay_timer_reset_counters(void);
extern void mpxplay_timer_execute_maincycle_funcs(void);
extern void mpxplay_timer_execute_int08_funcs(void);
extern void newfunc_newhandler08_init(void);
extern unsigned int newfunc_newhandler08_is_current_thread(void);
#ifdef MPXPLAY_WIN32
extern void newfunc_newhandler08_waitfor_threadend(void);
#else
#define newfunc_newhandler08_waitfor_threadend()
#endif
extern unsigned int newfunc_newhandler08_maincycles_init(struct mainvars *mvp, void *cycle1, void *cycle2);
extern void newfunc_newhandler08_close(void);

// safe funcbit handling for multi processors
#ifdef MPXPLAY_USE_SMP
#define funcbit_smp_test(var,bit)       (InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))&(bit))
#define funcbit_smp_enable(var,bit)     InterlockedExchange((LONG *)&(var),InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))|(bit))
#define funcbit_smp_disable(var,bit)    InterlockedExchange((LONG *)&(var),InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))&(~(bit)))
#define funcbit_smp_inverse(var,bit)    InterlockedExchange((LONG *)&(var),InterlockedCompareExchange((LONG *)&(var),(LONG)(var),(LONG)(var))^(bit))
#define funcbit_smp_copy(var1,var2,bit) InterlockedExchange((LONG *)&(var1), \
         InterlockedCompareExchange((LONG *)&(var1),(LONG)(var1),(LONG)(var1))| \
         (InterlockedCompareExchange((LONG *)&(var2),(LONG)(var2),(LONG)(var2))&(bit)))
//#define funcbit_smp_value_get(var)       InterlockedExchange((LONG *)&(var),(LONG)(var))
//#define funcbit_smp_pointer_get(var)     (void *)InterlockedExchange((LONG *)&(var),(LONG)(var))
#define funcbit_smp_value_get(var)       (var)
#define funcbit_smp_pointer_get(var)     (var)
#define funcbit_smp_value_put(var,val)   InterlockedExchange((LONG *)&(var),(LONG)(val))
#define funcbit_smp_int64_put(var,val)   if(sizeof(val)==sizeof(mpxp_int64_t)){ InterlockedExchange((LONG *)&(var),(LONG)((val)&0xffffffff));InterlockedExchange((LONG *)(&(var)+4),(LONG)((val)>>32));}else funcbit_smp_value_put(var,val);
#define funcbit_smp_filesize_put(var,val) funcbit_smp_value_put(var,val)
#define funcbit_smp_pointer_put(var,val) InterlockedExchange((LONG *)&(var),(LONG)(val))
#define funcbit_smp_value_increment(var) InterlockedIncrement((LONG *)&(var))
#define funcbit_smp_value_decrement(var) InterlockedDecrement((LONG *)&(var))

#else
#define funcbit_smp_test(var,bit)       funcbit_test(var,bit)
#define funcbit_smp_enable(var,bit)     funcbit_enable(var,bit)
#define funcbit_smp_disable(var,bit)    funcbit_disable(var,bit)
#define funcbit_smp_inverse(var,bit)    funcbit_disable(var,bit)
#define funcbit_smp_copy(var1,var2,bit) funcbit_copy(var1,var2,bit)
#define funcbit_smp_value_get(var)       (var)
#define funcbit_smp_pointer_get(var)     (var)
#define funcbit_smp_value_put(var,val)   (var)=(val)
#define funcbit_smp_int64_put(var,val)   (var)=(val)
#define funcbit_smp_filesize_put(var,val) funcbit_smp_value_put(var,val)
#define funcbit_smp_pointer_put(var,val) (var)=(val)
#define funcbit_smp_value_increment(var) var++
#define funcbit_smp_value_decrement(var) var--
#endif

#ifdef MPXPLAY_USE_DEBUGF

#include <stdarg.h>

static void mpxplay_debugf(FILE * fp, const char *format, ...)
{
	static FILE *flog = 0;
	va_list ap;
	char sout[1024];

	va_start(ap, format);
	vsprintf(sout, format, ap);
	va_end(ap);

	if(fp) {
		//if(flog==0)
		//flog=fopen("c:\mpxlog.txt","a");
		if(flog != 0) {
			fprintf(flog, "%s\n", sout);
			fflush(flog);
		}
		printf("%s\n",sout);

	} else
#ifdef MPXPLAY_USE_DEBUGMSG
		display_message(1, 0, sout);
#else
		pds_textdisplay_printf(sout);
#endif
}

#else
	#ifdef __WATCOMC__
		#define mpxplay_debugf()
	#else
		#define mpxplay_debugf (void)sizeof
	#endif
#endif							// MPXPLAY_USE_DEBUGF

#endif							// mpxplay_newfunc_h
