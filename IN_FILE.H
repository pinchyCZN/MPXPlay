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
// in-file and decoder definitions

#ifndef mpxplay_in_file_h
#define mpxplay_in_file_h

#include "newfunc\newfunc.h"
#include "mpxplay.h"

#ifdef MPXPLAY_LINK_FULL
 #define MPXPLAY_LINK_INFILE_AAC  1
 #define MPXPLAY_LINK_INFILE_AC3  1
 #define MPXPLAY_LINK_INFILE_APE  1
 #define MPXPLAY_LINK_INFILE_FLAC 1
 #define MPXPLAY_LINK_INFILE_MPX  1
 #define MPXPLAY_LINK_INFILE_MPC  1
 #define MPXPLAY_LINK_INFILE_OGG  1

 #define MPXPLAY_LINK_INFILE_ASF  1
 #define MPXPLAY_LINK_INFILE_AVI  1
 #define MPXPLAY_LINK_INFILE_MP4  1
 #define MPXPLAY_LINK_INFILE_WAV  1
 #define MPXPLAY_LINK_INFILE_WAVPACK 1
 //#define MPXPLAY_LINK_INFILE_FFMPG 1 // doesn't work!

 #define MPXPLAY_LINK_DECODER_AAC  1
 #define MPXPLAY_LINK_DECODER_AC3  1
 #define MPXPLAY_LINK_DECODER_ALAC 1
 #define MPXPLAY_LINK_DECODER_MPX  1
 #define MPXPLAY_LINK_DECODER_VORBIS 1

 #define MPXPLAY_LINK_DECODER_FFMPG 1 // FLAC,WMA
 #define MPXPLAY_LINK_DECODER_FLAC 1  //
 #define MPXPLAY_LINK_DECODER_WMA  1  //

 //#define MPXPLAY_LINK_VIDEO        1
#else
// #define MPXPLAY_LINK_INFILE_AAC  1
// #define MPXPLAY_LINK_INFILE_MPX  1
// #define MPXPLAY_LINK_DECODER_MPX  1
// #define MPXPLAY_LINK_INFILE_MPC  1
// #define MPXPLAY_LINK_INFILE_OGG  1

// #define MPXPLAY_LINK_INFILE_MP4  1
 #define MPXPLAY_LINK_INFILE_WAV  1
 #define MPXPLAY_LINK_INFILE_WAVPACK 1
 #define MPXPLAY_LINK_DECODER_WMA  1
// #define MPXPLAY_LINK_DECODER_AAC 1
// #define MPXPLAY_LINK_DECODER_MPX 1
// #define MPXPLAY_LINK_DECODER_VORBIS 1
#endif

typedef float MPXPLAY_PCMOUT_FLOAT_T;
typedef short MPXPLAY_PCMOUT_INT16_T;

// header filetypes in pei->entrytype and fr[].lay (decoder filetypes)
#define HFT_NULL      0x0
#define HFT_DFT       0x1  // directory & playlist
#define HFT_FILE_INT  0x2  // file handling (internal by Mpxplay)
#define HFT_FILE_DLL  0x4  // file handling (via DLL)

#define PUT_HFT(mph) ((unsigned long)mph<<28) // put HFT number to pei->entrytype
#define GET_HFT(mph) (mph>>28) // get HFT number from pei->entrytype

//adi->channelcfg (mp3,ogg)
#define CHM_LEFT    0  // -cl
#define CHM_STEREO  1  // normal (default)
#define CHM_DOWNMIX 2  // -cm (stereo to mono)
#define CHM_USERCNTRL_MASK (CHM_LEFT|CHM_STEREO|CHM_DOWNMIX)
//#define CHM_NOMCHDOWNMIX 64 // do not downmix multichannel to stereo (give multichannel output) (AAC,AC3,DTS) (controlled by the number of output/soundcard/-oc channels)

//clearbuff seektypes
#define MPX_SEEKTYPE_NORM   1 // seek in file
#define MPX_SEEKTYPE_BOF    2 // seek to begin of file (reset buffers)
#define MPX_SEEKTYPE_PAUSE  4 // seek in pause mode (can be used with BOF and NORM type)
#define MPX_SEEKTYPES_CLEARBUF (MPX_SEEKTYPE_NORM|MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE)
//fseek seektypes
//#define MPX_SEEKTYPE_TIMEMPOS 16  // seek to timepos (in millisecs)
#define MPX_SEEKTYPE_VIDEO    16 // seek to video (key)frame (else seek to audio frame)
#define MPX_SEEKTYPE_FRAME    32 // seek to (video) frame (usually with relative seek)
#define MPX_SEEKTYPE_RELATIVE 64 // -/+ value (else absolute value)
#define MPX_SEEKTYPES_FSEEK   (MPX_SEEKTYPE_VIDEO|MPX_SEEKTYPE_FRAME|MPX_SEEKTYPE_RELATIVE)

//adi->infobits
//info
//#define ADI_FLAG_FLOAT32OUT      1 //
//#define ADI_FLAG_FLOAT64OUT      2 //
//#define ADI_FLAG_FLOAT80OUT      3 //
//#define ADI_FLAG_FLOATOUTMASK    3 //
#define ADI_FLAG_FLOATOUT        1 // float audio decoder output (else integer)
#define ADI_FLAG_FPUROUND_CHOP   2 // roundtype at float output (else round to nearest)
#define ADI_FLAG_OWN_SPECTANAL   4 // own spectrum analiser
#define ADI_FLAG_BITSTREAMOUT    8 // bitstream out is enabled/supported (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
#define ADI_FLAG_BITSTREAMNOFRH 16 // bitstream out "no-frame-headers" is supported
#define ADI_FLAG_BITSTREAMHEAD  32 // write a header at the begin of file (ie: AAC,OGG) (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
#define ADI_FLAG_FLOAT64OUT     64 // 64-bit float output
#define ADI_FLAG_FLOAT80OUT     65 // 80-bit float output ???
#define ADI_FLAG_FLOATMASK      65
//control
#define ADI_CNTRLBIT_DECODERSPECINF 128 // bitstream (init) contains a decoder specific info
#define ADI_CNTRLBIT_BITSTREAMOUT   256 // bitstream out (don't decode) (write into file or soundcard decoding) (except: APE,CDW,MPC,WAV)
#define ADI_CNTRLBIT_BITSTREAMNOFRH 512 // cut frame headers (usually 4 bytes) with the frame (so don't cut it) (usually: file out with header, soundcard out without header)

// for displaying
#define MPXPLAY_ADITEXTSIZE_LONGNAME  8
#define MPXPLAY_ADITEXTSIZE_SHORTNAME 3
#define MPXPLAY_ADITEXTSIZE_BITRATE   8
#define MPXPLAY_ADITEXTSIZE_FREQ      7
#define MPXPLAY_ADITEXTSIZE_CHANNEL   8

// adi->chanmatrix, aucards_card_channelmap
#define MPXPLAY_PCMOUTCHAN_FRONT_LEFT    0
#define MPXPLAY_PCMOUTCHAN_FRONT_RIGHT   1
#define MPXPLAY_PCMOUTCHAN_FRONT_CENTER  2
#define MPXPLAY_PCMOUTCHAN_REAR_LEFT     3
#define MPXPLAY_PCMOUTCHAN_REAR_RIGHT    4
#define MPXPLAY_PCMOUTCHAN_LFE           5
#define MPXPLAY_PCMOUTCHAN_REAR_CENTER   6
#define MPXPLAY_PCMOUTCHAN_SIDE_LEFT     7
#define MPXPLAY_PCMOUTCHAN_SIDE_RIGHT    8
#define MPXPLAY_PCMOUTCHAN_DISABLED      126
#define MPXPLAY_PCMOUTCHAN_MAX           9

// infile/decoder error codes
#define MPXPLAY_ERROR_INFILE_OK        0
#define MPXPLAY_ERROR_INFILE_NODATA   -1 // no input data
#define MPXPLAY_ERROR_INFILE_EOF      -2 // end of file (finish playing)
#define MPXPLAY_ERROR_INFILE_SYNCLOST -3 // sync lost
#define MPXPLAY_ERROR_INFILE_SYNC_IN  -4 // decoder is in sync state (after seeking)
#define MPXPLAY_ERROR_INFILE_RESYNC   -5 // decoder needs resync (seek to next frame)(at wrong bitstream)
#define MPXPLAY_ERROR_INFILE_CANTOPEN -10
#define MPXPLAY_ERROR_INFILE_MEMORY   -11
#define MPXPLAY_ERROR_INFILE_FILEOPEN -12
#define MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE  -21 // not supported filetype (dts,wav)
#define MPXPLAY_ERROR_INFILE_WRITETAG_TAGTYPE   -22 // not supported tagtype (APEv2)
#define MPXPLAY_ERROR_INFILE_WRITETAG_IV2_SPACE -23 // id3v2: not enough space to store id3 infos
#define MPXPLAY_ERROR_INFILE_WRITETAG_UNKNOWN   -29 // unknown error

// for text (char) conversion
#define ID3TEXTCONV_CODEPAGE   1 // convert codepage to another (winchars/doschars)
#define ID3TEXTCONV_UTF16      2 // decode UTF-16 (unicode) strings
#define ID3TEXTCONV_UTF8       4 // decode UTF-8 strings (in every tags/filetypes)
#define ID3TEXTCONV_UTF_ALL    (ID3TEXTCONV_UTF16|ID3TEXTCONV_UTF8)
#define ID3TEXTCONV_CVTYPE_ALL (ID3TEXTCONV_CODEPAGE|ID3TEXTCONV_UTF_ALL)
#define ID3TEXTCONV_UTF_AUTO   8 // auto utf16 and utf8 decoding (ID3v2,Ogg,APEv2)
#define ID3TEXTCONV_VALIDATE  16 // validate (use only) Winchars at UTF-8 decoding (if unicode>=128)
#define ID3TEXTCONV_FILENAME  32 // convert chars in filenames too (in playlists)
#define ID3TEXTCONV_GET_WINCP 64 // get source and target codepages from windows (ansi and oem)
#define ID3TEXTCONV_CP_BACK  128 // tag-list has written in DOS (-8b)

//low level file handling -> buffered file handling -> demux (infile) -> decoding

typedef struct mpxplay_filehand_low_data_s{
 int  filept;
 long filesize;
 long filepos;
 unsigned char reserved[4];
}mpxplay_filehand_low_data_s; // 4*4=16 bytes

//low level file handling (normal-file, cdw, ftp-file-source)
typedef struct mpxplay_filehand_low_func_s{
 int   (*check) (char *filename);     // true (1) or false (0) (check filename (ie:filename begins with "http:\\"), use these functions if 'true')
 void *(*open_read) (char *filename); // returns filehand_data
 void *(*open_write)(char *filename); // returns filehand_data
 void *(*open_create)(char *filename);// returns filehand_data
 void  (*close) (void *filehand_data);
 long  (*read)  (void *filehand_data,char *buf,unsigned int len);
 long  (*write) (void *filehand_data,char *buf,unsigned int len);
 long  (*seek)  (void *filehand_data,long offset,int fromwhere);
 long  (*tell)  (void *filehand_data);
 long  (*filelength)(void *filehand_data);
 int   (*eof)   (void *filehand_data);
 int   (*chsize)(void *filehand_data,long offset);
}mpxplay_filehand_low_func_s;

// buffered file handling
typedef struct mpxplay_filehand_buffered_func_s{
 unsigned int (*fopen_read)  (void *fbds,char *filename,unsigned long pb_blocksize);
 unsigned int (*fopen_write) (void *fbds,char *filename);
 void         (*fclose)      (void *fbds);
 unsigned long (*fread)(void *fbds,void *buf,unsigned long len);
 //unsigned long (*fread_exact)(void *fbds,void *buf,unsigned long len);
 long  (*fwrite) (void *fbds,void *buf,unsigned long len);
 long  (*fseek) (void *fbds,long offset,int whence);
 long  (*ftell) (void *fbds);
 long  (*filelength)(void *fbds);
 int   (*eof)(void *fbds);                  // don't modify it to feof
 int   (*chsize) (void *fbds,long offset);
 mpxp_uint32_t (*get_byte)(void *fbds);
 mpxp_uint32_t (*get_le16)(void *fbds);
 mpxp_uint32_t (*get_le32)(void *fbds);
 mpxp_uint64_t (*get_le64)(void *fbds);
 mpxp_uint32_t (*get_be16)(void *fbds);
 mpxp_uint32_t (*get_be32)(void *fbds);
 mpxp_uint64_t (*get_be64)(void *fbds);
}mpxplay_filehand_buffered_func_s;

// *************************************************************************
// audio decoder

#define MPXPLAY_SPI_STREAMTYPE_UNKNOWN  0
#define MPXPLAY_SPI_STREAMTYPE_AUDIO    1
#define MPXPLAY_SPI_STREAMTYPE_VIDEO    2
#define MPXPLAY_SPI_STREAMTYPE_SUBTITLE 3
#define MPXPLAY_SPI_STREAMTYPE_DATA     4

#define MPXPLAY_SPI_FLAG_NEED_DECODER  1 // search for a decoder by wave_id or fourcc
#define MPXPLAY_SPI_FLAG_NEED_PARSING  2 // parse audio infos by adf->parse_frame (get informations from the stream, passed in spi->bitstreambuf)
#define MPXPLAY_SPI_FLAG_CONTAINER     4 // stream data is in container (run a longer parsing)
#define MPXPLAY_SPI_FLAG_BS_ERROR     16 // bitstream error detected in demuxer

#define MPXPLAY_SPI_MINBSREADSIZE          32 // ???
#define MPXPLAY_SPI_MAXBSREADSIZE_AUDIO 32768 // pre-init sizes
#define MPXPLAY_SPI_MAXBSREADSIZE_VIDEO 65536

#define MPXPLAY_SPI_EXTRADATA_PADDING 8

typedef struct mpxplay_streampacket_info_s{
 unsigned int  flags;
 //config
 unsigned int  stream_select;  // for this streamtype
 //info
 unsigned int  streamtype;     // audio/video/other
 unsigned int  wave_id;        // or fourcc in 4C_LE32
 unsigned int  nb_streams;     // number of streams for this streamtype
 unsigned int  block_align;    // WAV,WMA

 mpxp_uint8_t  *extradata;     // decoder extra data (use as a pointer, don't alloc directly to it)
 unsigned long  extradata_size;

 //decoding
 mpxp_uint8_t  *bitstreambuf;  // from demuxer/parser to decoder
 unsigned long  bs_bufsize;    //
 unsigned long  bs_framesize;  // adf->maxbsframesize or set by parser or set by Mpxplay (MAXBSREADSIZE)
 unsigned long  bs_readsize;   // used by (raw) parsers (mp3,ac3,dts) (usually =bs_framesize)
 unsigned long  bs_leftbytes;  //
 unsigned long  bs_usedbytes;  //

 //struct mpxplay_streampacket_info_s *next_demuxerdata;
 //void                               *next_demuxerfunc;
 unsigned char reserved[6*4];
}mpxplay_streampacket_info_s; // 20*4=80 bytes

typedef struct mpxplay_audio_decoder_info_s{
 void *private_data;         // decoder
 //info
 unsigned int infobits;      // flags
 unsigned int wave_id;       // audio type (wav:0x0001,mp3:0x0055,ac3:0x2000)
 unsigned int freq;          // frequency (44100,48000,...)
 unsigned int filechannels;  // number of channels in file
 unsigned int outchannels;   // decoded (used) channels, comes out from the decoder
 mpxp_uint8_t *chanmatrix;   // output channel matrix (ie: 5.1) (not used yet)
 unsigned int bits;          // 8,16 ... (scalebits at float,filebits at integer output)
 unsigned int bytespersample;// used in au_mixer
 unsigned int bitrate;       // in kbit/s (lossy formats)(ie: mp3,ogg,ac3,dts)
 unsigned int pcm_framelen;  // comes out from the decoder (samplenum without channels!)(=bytes/bytespersample)
 mpxp_float_t replaygain;    // not used yet
 char *shortname;            // set by decoder (3 chars), can static ("MP3","OGG","AC3","WMA")
 char *bitratetext;          // set by decoder (8 chars), else displays bitrate (if exists) or bits
 char *freqtext;             // set by decoder (7 chars), else displays freq
 char *channeltext;          // set by decoder (8 chars), can static ("msStereo","i-Stereo","DualChan","c-Stereo","5.1 chan") else displays 1ch->"mono",2ch->"stereo",Nch->"n-chan")
 //control
 unsigned int  channelcfg;    // configure output channels (eq to channelmode)
 //pcm out
 mpxp_uint8_t *pcm_bufptr;    // head ptr in pcm_buffer
 unsigned int  pcm_samplenum; // got back from decoder (with ch)
 unsigned char reserved[5*4];
}mpxplay_audio_decoder_info_s;// 24*4=96 bytes

typedef struct mpxplay_audio_decoder_type_s{
 unsigned int wave_id;  // 0x0055,????,0x2000,0x0160
 char *wave_name;       // "MP3","OGG","AC3","WMA"
}mpxplay_audio_decoder_type_s;

typedef struct mpxplay_audio_decoder_func_s{
 unsigned long flags;

 void (*preinit)(void); // initialize static data fields (mp3)
 void (*deinit) (void); // close audio decoder (cdw)
 void (*config_decoder)(void *notused); // not used yet

 int  (*open)  (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 void (*close) (struct mpxplay_audio_decoder_info_s *adi);
 int  (*parse_extra) (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 int  (*parse_frame) (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 int  (*decode)(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 void (*clearbuf)(struct mpxplay_audio_decoder_info_s *adi,unsigned int mpx_seektype);

 void (*get_analiser_bands)(struct mpxplay_audio_decoder_info_s *adi,unsigned int bandnum,unsigned long *banddataptr);
 void (*set_eq)(struct mpxplay_audio_decoder_info_s *adi,unsigned int bandnum,unsigned long *band_freqs,float *band_powers);

 unsigned long maxbsframesize;  // max. size of input (bitstream) frames
 unsigned long maxpcmblocksamplenum; // max. pcm block (frame) size (!!! have to multiply with channelnum and bytes_per_sample)
 mpxplay_audio_decoder_type_s decodertypes[];
}mpxplay_audio_decoder_func_s;

// ***********************************************************************
// video decoder

#include "videoout\videoout.h" // PIC_FMT defs

//control
#define VDI_CNTRLBIT_DECODEVIDEO 1   // user configured
#define VDI_CNTRLBIT_SHOWVIDEO   32  // depending on the display type
//#define VDI_CNTRLBIT_DIRECTWRITE 64  // no postprocess, data can be writed directly to screen
//info
#define VDI_FLAG_NEWPIC      256 // there's a new picture in the buffer
//#define VDI_FLAG_DIRECTWRITE 512 // data has writed directly to screen

typedef struct mpxplay_video_decoder_info_s{
 void *private_data;
 unsigned long flags;
 unsigned char *picture;
 unsigned long picture_size;      // in bytes
 unsigned long video_frames;      // number of video frames
 unsigned long video_framepos;    // current frame position
 unsigned int  video_fps;         // frames/sec
 unsigned long video_pixelformat;
 unsigned long video_res_x;
 unsigned long video_res_y;
 unsigned long video_bitrate;     // just info
 unsigned long video_bpp;         //
 //unsigned char *screen_ptr;
 unsigned long screen_res_x;
 unsigned long screen_res_y;
 unsigned long output_res_x;
 unsigned long output_res_y;
}mpxplay_video_decoder_info_s;

typedef struct mpxplay_video_decoder_func_s{
 char *fourcc;
 unsigned long flags;

 int  (*open)  (struct mpxplay_video_decoder_info_s *vdi,struct mpxplay_streampacket_info_s *spi);
 void (*close) (struct mpxplay_video_decoder_info_s *vdi);
 int  (*decode)(struct mpxplay_video_decoder_info_s *vdi,struct mpxplay_streampacket_info_s *spi);
 void (*flush) (struct mpxplay_video_decoder_info_s *vdi,unsigned int mpx_seektype);

 unsigned int maxbsframesize;// max. size of input (bitstream) frames
}mpxplay_video_decoder_func_s;

// ***********************************************************************

/*typedef struct mpxplay_id3tag_func_s{
 void *pass_psi;
 void *pass_pei;
 mpxp_ptrsize_t passinforeserved[6];
 unsigned int (*add_id3tag)(void *pass_id3tagfuncs,unsigned int i3i_num,unsigned int src_textenctype,char *tagstr);
 unsigned int (*get_id3tag)(void *pass_id3tagfuncs,unsigned int i3i_num,unsigned int dest_textenctype,char *destbuf,unsigned int buflen);
 unsigned int (*del_id3tag)(void *pass_id3tagfuncs,unsigned int i3i_num);
 mpxp_ptrsize_t funcreserved[5];
}mpxplay_id3tag_func_s;*/

typedef struct mpxplay_textconv_func_s{
 unsigned int *control;
 unsigned int convdone;
 //decoding (win to dos)
 unsigned int (*utf16LE_to_char)(unsigned char *str,unsigned int datalen);
 unsigned int (*utf16BE_to_char)(unsigned char *str,unsigned int datalen);
 unsigned int (*utf8_to_char)(unsigned char *str,unsigned int datalen);
 unsigned int (*cp_to_cp_in)(unsigned char *str,unsigned int datalen);
 unsigned int (*all_to_char)(unsigned char *str,unsigned int datalen,unsigned int doneconv);
 //encoding (dos to win)
 unsigned int (*char_to_utf16LE)(unsigned char *dest,unsigned char *src,unsigned int dest_buflen); // dest_buflen have to be 2x src_len (inc. terminating null)
 unsigned int (*char_to_utf16BE)(unsigned char *dest,unsigned char *src,unsigned int dest_buflen); //
 unsigned int (*char_to_utf8)(unsigned char *dest,unsigned char *src,unsigned int dest_buflen); // dest_buflen have to be 3x src_len (inc. terminating null)
 unsigned int (*cp_to_cp_out)(unsigned char *dest,unsigned char *src);
 char        *(*char_to_all)(unsigned char *dest,unsigned char *src); // cp conversion only (returns dest)
}mpxplay_textconv_func_s;

typedef struct mpxplay_infile_info_s{
 long filesize;     //
 long timemsec;     // length in msecs
 long allframes;    // in mpxplay frames (1 sec = ca. 38 frames)
 void *private_data;// of infile_func (demuxer)
 char *longname;    // set by file-parser (8 chars), can static ("LayerIII","OgVorbis","DolbyAC3","WinMediA")
 unsigned int seektype; // MPX_SEEKTYPE_xxx (decoder/demuxer clearbuf,infile_fseek)

 struct mpxplay_streampacket_info_s  *audio_stream;        // static!
 struct mpxplay_audio_decoder_info_s *audio_decoder_infos; // static! don't clear!
 struct mpxplay_audio_decoder_func_s *audio_decoder_funcs;

 struct mpxplay_streampacket_info_s  *video_stream;        // static!
 struct mpxplay_video_decoder_info_s *video_decoder_infos; // static! don't clear!
 struct mpxplay_video_decoder_func_s *video_decoder_funcs;

 unsigned long standard_id3tag_support;
 unsigned char reserved[3*4];
}mpxplay_infile_info_s;

typedef struct mpxplay_infile_func_s{
 unsigned long flags;

 void (*preinit)(void); // initialize static data fields (mp3)
 void (*deinit) (void); // close audio decoder (cdw)

 int  (*detect)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *); // return MPXPLAY_ERROR_INFILE_xxx
 int  (*check) (struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *); //
 int  (*open)  (struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *); //
 void (*close) (struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *);
 int  (*decode)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *);// return MPXPLAY_ERROR_INFILE_xxx
 long (*fseek) (struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *,long newmpxframenum); // return MPXPLAY_ERROR_INFILE_xxx or newmpxframenum
 void (*seek_postprocess)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,unsigned int mpx_seektype);

 char *(*get_id3tag)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,char *id3p,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs); // return id3p
 int   (*write_id3tag)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char **id3ip,struct mpxplay_textconv_func_s *mpxplay_textconv_funcs); // return error code

 //int   (*get_id3tag)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,struct mpxplay_id3tag_func_s *ifs); // return error code
 //int   (*write_id3tag)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,struct mpxplay_id3tag_func_s *ifs); // return error code

 struct mpxplay_filehand_low_func_s *own_filehand_funcs; // to set an own low level file handling (CDW)

 char *file_extensions[]; // {"MP2","MP3",NULL} {"OGG",NULL} (the first extension have to be a 3-char file description too (ie: MP4, ASF))
}mpxplay_infile_func_s;

/*typedef struct pcmdecbuffer_s{
 mpxp_uint8_t *pcmdec_buffer;
 unsigned int pcmdec_bufsize;
 unsigned int pcmdec_storedsamples;
 unsigned int pcmdec_leftsamples;
}pcmdecbuffer_s;

typedef struct pcmoutbuffer_s{
 mpxp_uint8_t *pcmout_buffer;
 unsigned int pcmout_bufsize;        // in bytes
 unsigned int pcmout_blocksize;      // samplenum (with ch)
 unsigned int pcmout_storedsamples;  // samplenum (with ch)
}pcmoutbuffer_s;*/

typedef struct frame {
 long frameNum;
 long allframes;
 long timesec;
 unsigned long framecounter;
 long index_start; // =0         if not indexed
 long index_end;   // =allframes if not indexed
 long index_len;   // =allframes if not indexed (avoids inline calculation)

 unsigned int filetype;     // HFT_XXX

 //file/source infos
 int  mpxfilept;
 mpxp_filesize_t filesize;
 mpxp_filesize_t filepos;
 struct mpxplay_diskdrive_data_s *mdds;
 struct mpxplay_filehand_low_func_s *filehand_funcs;
 void *filehand_datas; // private datas of filehand_funcs
 mpxp_uint64_t readwait_endtime;

 //file-buffer variables
 unsigned int buffertype;
 char *prebufferbegin;
 long prebufferblocksize;
 long prebuffersize;   // frp->prebufferblocksize * prebufferblocks (4 or 32)
 long prebuffergetp;
 long prebufferputp;
 long prebufferbytes_rewind;
 long prebufferbytes_forward;
 mpxp_filesize_t prebuffer_seek_retry;
 struct mpxplay_filehand_buffered_func_s *filebuf_funcs; // static!

 //decoder infos
 struct mpxplay_infile_info_s *infile_infos; // !!! static! don't clear!
 struct mpxplay_infile_func_s *infile_funcs;
 //void                         *infile_datas;

 //pcm (decoder,mixer,card)
 mpxp_uint8_t *pcmdec_buffer;
 unsigned int pcmdec_bufsize;
 unsigned int pcmdec_storedsamples;
 unsigned int pcmdec_leftsamples;
 mpxp_uint8_t *pcmout_buffer;
 unsigned int pcmout_bufsize;        // in bytes
 unsigned int pcmout_blocksize;      // samplenum (with ch)
 unsigned int pcmout_storedsamples;  // samplenum (with ch)

 struct frame *fro; // crosslink (fr[0]->fr[1],fr[1]->fr[0])
}frame;

#define MPXPLAY_INFILE_STRUCTFRAME_REALSIZE (sizeof(struct frame)-4) // !!! because it's 8 bytes aligned now

//in_file.c
int mpxplay_infile_decode(struct mpxplay_audioout_info_s *aui);
unsigned int mpxplay_infile_check_extension(char *filename,struct mpxplay_diskdrive_data_s *mdds);
unsigned long mpxplay_infile_get_header_by_ext(struct frame *frp,struct mpxplay_diskdrive_data_s *mdds,char *filename);
char *mpxplay_infile_get_id3tag(struct frame *frp,char **id3ip,char *id3p);
int  mpxplay_infile_write_id3tag(struct frame *frp,char *filename,char **id3ip);
long mpxplay_infile_fseek(struct frame *frp,long newframenum);

unsigned int mpxplay_infile_get_samplenum_per_frame(unsigned int freq);
void miis_to_frp(struct mpxplay_infile_info_s *miis,struct frame *frp);
void mpxplay_infile_video_config_open(struct mpxplay_videoout_info_s *voi,struct mpxplay_video_decoder_info_s *vdi);
int  mpxplay_infile_open(struct frame *frp,char *filename);
void mpxplay_infile_close(struct frame *frp);
void mpxplay_infile_reset(struct frame *frp);
unsigned int mpxplay_infile_realloc_pcmoutdata(struct mpxplay_audio_decoder_info_s *adi,struct frame *frp);

void mpxplay_infile_init(struct mainvars *mvp);
void mpxplay_infile_deinit(void);

struct frame *mpxplay_infile_frame_alloc(struct frame *frp);
void mpxplay_infile_frame_free(struct frame *frp);

#endif // mpxplay_in_file_h
