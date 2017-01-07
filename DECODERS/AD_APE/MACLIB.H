#ifndef APE_MACLIB_H
#define APE_MACLIB_H

#include "apetypes.h"

/*****************************************************************************************
Defines
*****************************************************************************************/

#define COMPRESSION_LEVEL_FAST		1000
#define COMPRESSION_LEVEL_NORMAL	2000
#define COMPRESSION_LEVEL_HIGH		3000
#define COMPRESSION_LEVEL_EXTRA_HIGH	4000
#define COMPRESSION_LEVEL_INSANE	5000

#define MAC_FORMAT_FLAG_8_BIT		    1  // is 8-bit
#define MAC_FORMAT_FLAG_CRC		    2  // uses the new CRC32 error detection
#define MAC_FORMAT_FLAG_HAS_PEAK_LEVEL	    4  // unsigned __int32 Peak_Level after the header
#define MAC_FORMAT_FLAG_24_BIT		    8  // is 24-bit
#define MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS  16  // has the number of seek elements after the peak level
#define MAC_FORMAT_FLAG_CREATE_WAV_HEADER  32  // create the wave header on decompression (not stored)

/*****************************************************************************************
APE header structure (what's at the front of an APE file)
*****************************************************************************************/

typedef struct APE_COMMON_HEADER
{
 char cID[4];                            // should equal 'MAC '
 unsigned __int16 nVersion;              // version number * 1000 (3.81 = 3810)
}APE_COMMON_HEADER;

#define APE_COMMON_HEADER_BYTES 6

typedef struct APE_DESCRIPTOR
{
 //char    cID[4];                             // not used
 unsigned __int16  nope;                     // not used, for 4 byte align only
 unsigned __int16  nVersion_high;            // not used (empty space in MAC header?)
 unsigned __int32  nDescriptorBytes;         // the number of descriptor bytes (allows later expansion of this header)
 unsigned __int32  nHeaderBytes;             // the number of header APE_HEADER bytes
 unsigned __int32  nSeekTableBytes;          // the number of bytes of the seek table
 unsigned __int32  nHeaderDataBytes;         // the number of header data bytes (from original file)
 unsigned __int32  nAPEFrameDataBytes;       // the number of bytes of APE frame data
 unsigned __int32  nAPEFrameDataBytesHigh;   // the high order number of APE frame data bytes
 unsigned __int32  nTerminatingDataBytes;    // the terminating data of the file (not including tag data)
 unsigned char  cFileMD5[16];                // the MD5 hash of the file (see notes for usage... it's a littly tricky)
}APE_DESCRIPTOR;

#define APE_DESCRIPTOR_BYTES 46

typedef struct APE_HEADER
{
 unsigned __int16  nCompressionLevel;    // the compression level (see defines I.E. COMPRESSION_LEVEL_FAST)
 unsigned __int16  nFormatFlags;         // any format flags (for future use)
 unsigned __int32  nBlocksPerFrame;      // the number of audio blocks in one frame
 unsigned __int32  nFinalFrameBlocks;    // the number of audio blocks in the final frame
 unsigned __int32  nTotalFrames;         // the total number of frames
 unsigned __int16  nBitsPerSample;       // the bits per sample (typically 16)
 unsigned __int16  nChannels;            // the number of channels (1 or 2)
 unsigned __int32  nSampleRate;          // the sample rate (typically 44100)
}APE_HEADER;

//#define APE_HEADER_BYTES 24

typedef struct APE_HEADER_OLD
{
 //char cID[4];                             // not used
 unsigned __int16  nVersion;              // not used, for 4 byte align only
 unsigned __int16  nCompressionLevel;     // the compression level
 unsigned __int16  nFormatFlags;          // any format flags (for future use)
 unsigned __int16  nChannels;             // the number of channels (1 or 2)
 unsigned __int32  nSampleRate;           // the sample rate (typically 44100)
 unsigned __int32  nHeaderBytes;          // the bytes after the MAC header that compose the WAV header
 unsigned __int32  nTerminatingBytes;     // the bytes after that raw data (for extended info)
 unsigned __int32  nTotalFrames;          // the number of frames in the file
 unsigned __int32  nFinalFrameBlocks;     // the number of samples in the final frame
}APE_HEADER_OLD;

#define APE_HEADER_OLD_BYTES 26  // 28-2

enum APE_DECOMPRESS_FIELDS
{
 APE_INFO_FILE_VERSION = 1000,		// version of the APE file * 1000 (3.93 = 3930) [ignored, ignored]
 APE_INFO_COMPRESSION_LEVEL = 1001,	// compression level of the APE file [ignored, ignored]
 APE_INFO_FORMAT_FLAGS = 1002,		// format flags of the APE file [ignored, ignored]
 APE_INFO_SAMPLE_RATE = 1003,		// sample rate (Hz) [ignored, ignored]
 APE_INFO_BITS_PER_SAMPLE = 1004,	// bits per sample [ignored, ignored]
 APE_INFO_BYTES_PER_SAMPLE = 1005,	// number of bytes per sample [ignored, ignored]
 APE_INFO_CHANNELS = 1006,		// channels [ignored, ignored]
 APE_INFO_BLOCK_ALIGN = 1007,		// block alignment [ignored, ignored]
 APE_INFO_BLOCKS_PER_FRAME = 1008,	// number of blocks in a frame (frames are used internally)  [ignored, ignored]
 APE_INFO_FINAL_FRAME_BLOCKS = 1009,	// blocks in the final frame (frames are used internally) [ignored, ignored]
 APE_INFO_TOTAL_FRAMES = 1010,		// total number frames (frames are used internally) [ignored, ignored]
 //APE_INFO_WAV_HEADER_BYTES = 1011,	// header bytes of the decompressed WAV [ignored, ignored]
 APE_INFO_WAV_TERMINATING_BYTES = 1012,	// terminating bytes of the decompressed WAV [ignored, ignored]
 APE_INFO_WAV_DATA_BYTES = 1013,	// data bytes of the decompressed WAV [ignored, ignored]
 //APE_INFO_WAV_TOTAL_BYTES = 1014,	// total bytes of the decompressed WAV [ignored, ignored]
 APE_INFO_APE_TOTAL_BYTES = 1015,	// total bytes of the APE file [ignored, ignored]
 APE_INFO_TOTAL_BLOCKS = 1016,		// total blocks of audio data [ignored, ignored]
 APE_INFO_LENGTH_MS = 1017,		// length in ms (1 sec = 1000 ms) [ignored, ignored]
 APE_INFO_AVERAGE_BITRATE = 1018,	// average bitrate of the APE [ignored, ignored]
 APE_INFO_FRAME_BITRATE = 1019,		// bitrate of specified APE frame [frame index, ignored]
 APE_INFO_DECOMPRESSED_BITRATE = 1020,	// bitrate of the decompressed WAV [ignored, ignored]
 APE_INFO_PEAK_LEVEL = 1021,		// peak audio level (-1 is unknown) [ignored, ignored]
 APE_INFO_SEEK_BIT = 1022,		// bit offset [frame index, ignored]
 APE_INFO_SEEK_BYTE = 1023,		// byte offset [frame index, ignored]
 //APE_INFO_WAV_HEADER_DATA = 1024,	// error code [buffer *, max bytes]
 //APE_INFO_WAV_TERMINATING_DATA = 1025,// error code [buffer *, max bytes]
 APE_INFO_WAVEFORMATEX = 1026,		// error code [waveformatex *, ignored]
 //APE_INFO_IO_SOURCE = 1027,		// I/O source (CIO *) [ignored, ignored]
 APE_INFO_FRAME_BYTES = 1028,		// bytes (compressed) of the frame [frame index, ignored]
 APE_INFO_FRAME_BLOCKS = 1029,		// blocks in a given frame [frame index, ignored]
 //APE_INFO_TAG = 1030,			// point to tag (CAPETag *) [ignored, ignored]

 APE_DECOMPRESS_CURRENT_BLOCK = 2000,	// current block location [ignored, ignored]
 APE_DECOMPRESS_CURRENT_MS = 2001,	// current millisecond location [ignored, ignored]
 APE_DECOMPRESS_TOTAL_BLOCKS = 2002,	// total blocks in the decompressors range [ignored, ignored]
 APE_DECOMPRESS_LENGTH_MS = 2003,	// total blocks in the decompressors range [ignored, ignored]
 APE_DECOMPRESS_CURRENT_BITRATE = 2004,	// current bitrate [ignored, ignored]
 APE_DECOMPRESS_AVERAGE_BITRATE = 2005,	// average bitrate (works with ranges) [ignored, ignored]
 APE_DECOMPRESS_CURRENT_FRAME = 2006,   // current frame
};

/*************************************************************************************************
IAPEDecompress - interface for working with existing APE files (decoding, seeking, analyzing, etc.)
*************************************************************************************************/
#include "apeinfo.h"

typedef struct CAPEDecompress_func_s{
 void (*CAPEDecompress_open)(struct IAPEDecompress_data_s *apedec_datas);
 void (*CAPEDecompress_close)(void *decoder_datas);
 int (*GetData)(void *decoder_datas,char *pBuffer, int nBlocks,int *pBlocksRetrieved);
 int (*Seek)(void *decoder_datas,int nBlockOffset);
 int (*GetInfo)(void *decoder_datas,enum APE_DECOMPRESS_FIELDS Field,int nParam1,int nParam2);
}CAPEDecompress_func_s;

typedef struct IAPEDecompress_data_s{
 void  *decoder_datas;                         // get from the decoder
 struct CAPEDecompress_func_s *decoder_funcs;  // -"-
 struct CAPEInfo_data_s *apeinfo_datas;        // -"-
 struct mpxplay_filehand_buffered_func_s *fileio_funcs; // give to the decoder
 void                                    *fileio_datas; // -"-
}IAPEDecompress_data_s;

extern struct IAPEDecompress_data_s *IAPEDecompress_check(struct mpxplay_filehand_buffered_func_s *fileio_funcs,void *fileio_datas,char *pFilename);
extern struct IAPEDecompress_data_s *IAPEDecompress_open(struct mpxplay_filehand_buffered_func_s *fileio_funcs,void *fileio_datas,char *pFilename);
extern void IAPEDecompress_close(struct IAPEDecompress_data_s *apedec_datas);
extern int IAPEDecompress_GetData(struct IAPEDecompress_data_s *apedec_datas,char *pBuffer,int nBlocks,int *pBlocksRetrieved);
extern int IAPEDecompress_Seek(struct IAPEDecompress_data_s *apedec_datas,int nBlockOffset);
extern int IAPEDecompress_GetInfo(struct IAPEDecompress_data_s *apedec_datas,enum APE_DECOMPRESS_FIELDS Field,int nParam1,int nParam2);

#endif // #ifndef APE_MACLIB_H
