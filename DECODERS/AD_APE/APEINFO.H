#ifndef APE_APEINFO_H
#define APE_APEINFO_H

#include "all.h"
#include "MACLib.h"

typedef struct APE_FILE_INFO {
	int nVersion;				// file version number * 1000 (3.93 = 3930)
	int nCompressionLevel;		// the compression level
	int nFormatFlags;			// format flags
	int nTotalFrames;			// the total number frames (frames are used internally)
	int nBlocksPerFrame;		// the samples in a frame (frames are used internally)
	int nFinalFrameBlocks;		// the number of samples in the final frame
	int nChannels;				// audio channels
	int nSampleRate;			// audio samples per second
	int nBitsPerSample;			// audio bits per sample
	int nBytesPerSample;		// audio bytes per sample
	int nBlockAlign;			// audio block align (channels * bytes per sample)
	int nWAVHeaderBytes;		// header bytes of the original WAV
	int nWAVDataBytes;			// data bytes of the original WAV
	int nWAVTerminatingBytes;	// terminating bytes of the original WAV
	//int nWAVTotalBytes;     // total bytes of the original WAV
	int nAPETotalBytes;			// total bytes of the APE file
	int nTotalBlocks;			// the total number audio blocks
	int nLengthMS;				// the length in milliseconds
	int nAverageBitrate;		// the kbps (i.e. 637 kpbs)
	int nDecompressedBitrate;	// the kbps of the decompressed audio (i.e. 1440 kpbs for CD audio)
	int nPeakLevel;				// the peak audio level (-1 if unknown)
} APE_FILE_INFO;

#define GET_USES_CRC(APE_INFO) (((APE_INFO)->apeinfo_funcs->GetInfo(APE_INFO,APE_INFO_FORMAT_FLAGS,0,0) & MAC_FORMAT_FLAG_CRC) ? TRUE : FALSE)
#define GET_FRAMES_START_ON_BYTES_BOUNDARIES(APE_INFO) (((APE_INFO)->apeinfo_funcs->GetInfo(APE_INFO,APE_INFO_FILE_VERSION,0,0) > 3800) ? TRUE : FALSE)
#define GET_USES_SPECIAL_FRAMES(APE_INFO) (((APE_INFO)->apeinfo_funcs->GetInfo(APE_INFO,APE_INFO_FILE_VERSION,0,0) > 3820) ? TRUE : FALSE)

typedef struct CAPEInfo_func_s {
	struct CAPEInfo_data_s *(*check) (struct IAPEDecompress_data_s * iapedec_datas, char *pFilename);
	struct CAPEInfo_data_s *(*open) (struct IAPEDecompress_data_s * iapedec_datas, char *pFilename);
	void (*close) (struct CAPEInfo_data_s *);
	int (*GetInfo) (struct CAPEInfo_data_s *, enum APE_DECOMPRESS_FIELDS Field, int nParam1, int nParam2);
} CAPEInfo_func_s;

typedef struct CAPEInfo_data_s {
	struct IAPEDecompress_data_s *iapedec_datas;
	struct APE_FILE_INFO *m_APEFileInfo;
	struct CAPEInfo_func_s *apeinfo_funcs;
	int m_nExtraHeaderBytes;	// used for ID3v2, etc.
	int m_nSeekTableElements;
	unsigned long *m_spSeekByteTable;
	unsigned char *m_spSeekBitTable;
	unsigned long tagsize;
	APE_DESCRIPTOR *spAPEDescriptor;
} CAPEInfo_data_s;

extern CAPEInfo_func_s CAPEInfo_funcs;

typedef struct APETag_s {
	unsigned char ID[8];		// should equal 'APETAGEX'
	unsigned char Version[4];	// currently 1000 (version 1.000)
	unsigned char Length[4];	// the complete size of the tag, including this footer
	unsigned char TagCount[4];	// the number of fields in the tag
	unsigned char Flags[4];		// the tag flags (none currently defined)
	unsigned char Reserved[8];	// reserved for later use
} APETag_s;

#endif							// #ifndef APE_APEINFO_H
