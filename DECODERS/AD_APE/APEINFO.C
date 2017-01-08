//**************************************************************************
//*                   This file is part of the                             *
//*           APE decoder of Mpxplay (http://mpxplay.cjb.net)              *
//*      based on the MAC SDK v3.97 (http://www.monkeysaudio.com)          *
//*                  updated from the v3.99 (June 2004)                    *
//**************************************************************************
//*   This program is distributed in the hope that it will be useful,      *
//*   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
//*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                 *
//**************************************************************************

#include "All.h"
#include "APEInfo.h"

static int GetFileInformation_check(CAPEInfo_data_s *);
static int GetFileInformation_open(CAPEInfo_data_s *);
static int SkipToAPEHeader(CAPEInfo_data_s *);

static struct CAPEInfo_data_s *CAPEInfo_check(struct IAPEDecompress_data_s *iapedec_datas, char *pFilename);
static struct CAPEInfo_data_s *CAPEInfo_open(struct IAPEDecompress_data_s *iapedec_datas, char *pFilename);
static void CAPEInfo_close(struct CAPEInfo_data_s *);
static int CAPEInfo_getinfo(struct CAPEInfo_data_s *apeinfo_datas, enum APE_DECOMPRESS_FIELDS Field, int nParam1, int nParam2);

CAPEInfo_func_s CAPEInfo_funcs = {
	&CAPEInfo_check,
	&CAPEInfo_open,
	&CAPEInfo_close,
	&CAPEInfo_getinfo
};

static struct CAPEInfo_data_s *CAPEInfo_check(struct IAPEDecompress_data_s *iapedec_datas, char *pFilename)
{
	struct CAPEInfo_data_s *apeinfo_datas;
	apeinfo_datas = (struct CAPEInfo_data_s *)calloc(1, sizeof(struct CAPEInfo_data_s));
	if(!apeinfo_datas)
		goto err_out_check;

	apeinfo_datas->iapedec_datas = iapedec_datas;

	if(!iapedec_datas->fileio_funcs->fopen_read(iapedec_datas->fileio_datas, pFilename, 0))
		goto err_out_check;

	apeinfo_datas->apeinfo_funcs = &CAPEInfo_funcs;

	if(GetFileInformation_check(apeinfo_datas) != 0)
		goto err_out_check;

	return apeinfo_datas;

  err_out_check:
	CAPEInfo_close(apeinfo_datas);
	return NULL;
}

static struct CAPEInfo_data_s *CAPEInfo_open(struct IAPEDecompress_data_s *iapedec_datas, char *pFilename)
{
	struct CAPEInfo_data_s *apeinfo_datas;

	apeinfo_datas = (struct CAPEInfo_data_s *)calloc(1, sizeof(struct CAPEInfo_data_s));
	if(!apeinfo_datas)
		goto err_out_open;

	apeinfo_datas->iapedec_datas = iapedec_datas;

	if(!iapedec_datas->fileio_funcs->fopen_read(iapedec_datas->fileio_datas, pFilename, 0))
		goto err_out_open;

	apeinfo_datas->apeinfo_funcs = &CAPEInfo_funcs;

	if(GetFileInformation_open(apeinfo_datas) != 0)
		goto err_out_open;

	return apeinfo_datas;

  err_out_open:
	CAPEInfo_close(apeinfo_datas);
	return NULL;
}

static void CAPEInfo_close(struct CAPEInfo_data_s *apeinfo_datas)
{
	if(apeinfo_datas) {
		if(apeinfo_datas->iapedec_datas->fileio_funcs)
			apeinfo_datas->iapedec_datas->fileio_funcs->fclose(apeinfo_datas->iapedec_datas->fileio_datas);
		if(apeinfo_datas->m_spSeekBitTable)
			free(apeinfo_datas->m_spSeekBitTable);
		if(apeinfo_datas->m_spSeekByteTable)
			free(apeinfo_datas->m_spSeekByteTable);
		if(apeinfo_datas->m_APEFileInfo)
			free(apeinfo_datas->m_APEFileInfo);
		free(apeinfo_datas);
	}
}

//from 3980
static int AnalyzeCurrent(struct CAPEInfo_data_s *apeinfo_datas)
{
	struct APE_FILE_INFO *pInfo = apeinfo_datas->m_APEFileInfo;
	struct mpxplay_filehand_buffered_func_s *fileio_funcs = apeinfo_datas->iapedec_datas->fileio_funcs;
	void *fileio_datas = apeinfo_datas->iapedec_datas->fileio_datas;
	unsigned int nBytesRead;
	APE_DESCRIPTOR APEDescriptor;
	APE_HEADER APEHeader;

	nBytesRead = fileio_funcs->fread(fileio_datas, ((unsigned char *)&APEDescriptor) + 2, APE_DESCRIPTOR_BYTES);
	if(nBytesRead != APE_DESCRIPTOR_BYTES)
		return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;

	if(APEDescriptor.nDescriptorBytes > (APE_COMMON_HEADER_BYTES + nBytesRead))
		if(fileio_funcs->fseek(fileio_datas, APEDescriptor.nDescriptorBytes - nBytesRead, SEEK_CUR) < 0)
			return APEDEC_ERROR_IO_SEEK;

	// read the header
	nBytesRead = fileio_funcs->fread(fileio_datas, &APEHeader, sizeof(APE_HEADER));
	if(nBytesRead != sizeof(APE_HEADER))
		return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;

	if(!APEHeader.nCompressionLevel || !APEHeader.nChannels || !APEHeader.nSampleRate || !APEHeader.nBitsPerSample)
		return APEDEC_ERROR_INVALID_INPUT_FILE;

	if((APEDescriptor.nHeaderBytes - nBytesRead) > 0)
		if(fileio_funcs->fseek(fileio_datas, APEDescriptor.nHeaderBytes - nBytesRead, SEEK_CUR) < 0)
			return APEDEC_ERROR_IO_SEEK;

	// fill the APE info structure
	pInfo->nCompressionLevel = (int)APEHeader.nCompressionLevel;
	pInfo->nFormatFlags = (int)APEHeader.nFormatFlags;
	pInfo->nTotalFrames = (int)APEHeader.nTotalFrames;
	pInfo->nFinalFrameBlocks = (int)APEHeader.nFinalFrameBlocks;
	pInfo->nBlocksPerFrame = (int)APEHeader.nBlocksPerFrame;
	pInfo->nChannels = (int)APEHeader.nChannels;
	pInfo->nSampleRate = (int)APEHeader.nSampleRate;

	pInfo->nBitsPerSample = (int)APEHeader.nBitsPerSample;
	pInfo->nBytesPerSample = pInfo->nBitsPerSample / 8;
	pInfo->nBlockAlign = pInfo->nBytesPerSample * pInfo->nChannels;
	pInfo->nTotalBlocks = (APEHeader.nTotalFrames == 0) ? 0 : ((APEHeader.nTotalFrames - 1) * pInfo->nBlocksPerFrame) + APEHeader.nFinalFrameBlocks;
	if(!pInfo->nTotalBlocks)
		return APEDEC_ERROR_INVALID_INPUT_FILE;
	pInfo->nWAVHeaderBytes = (APEHeader.nFormatFlags & MAC_FORMAT_FLAG_CREATE_WAV_HEADER) ? 44 : APEDescriptor.nHeaderDataBytes;
	pInfo->nWAVTerminatingBytes = APEDescriptor.nTerminatingDataBytes;
	pInfo->nWAVDataBytes = pInfo->nTotalBlocks * pInfo->nBlockAlign;
	pInfo->nAPETotalBytes = fileio_funcs->filelength(fileio_datas);
	pInfo->nLengthMS = (int)((double)pInfo->nTotalBlocks * 1000.0 / (double)pInfo->nSampleRate);
	pInfo->nAverageBitrate = (pInfo->nLengthMS <= 0) ? 0 : (int)((double)pInfo->nAPETotalBytes * 8.0 / (double)pInfo->nLengthMS);
	pInfo->nDecompressedBitrate = (pInfo->nBlockAlign * pInfo->nSampleRate * 8) / 1000;
	apeinfo_datas->m_nSeekTableElements = APEDescriptor.nSeekTableBytes / 4;

	return APEDEC_ERROR_SUCCESS;
}

//up to 3970
static int AnalyzeOld(struct CAPEInfo_data_s *apeinfo_datas)
{
	int nPeakLevel;
	struct APE_FILE_INFO *loc_apefileinfo;
	struct APE_HEADER_OLD APEHeader;

	if(apeinfo_datas->iapedec_datas->fileio_funcs->fread(apeinfo_datas->iapedec_datas->fileio_datas, ((unsigned char *)&APEHeader) + 2, APE_HEADER_OLD_BYTES) != APE_HEADER_OLD_BYTES)
		return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;

	if(!APEHeader.nChannels || !APEHeader.nSampleRate || !APEHeader.nCompressionLevel)
		return APEDEC_ERROR_INVALID_INPUT_FILE;

	nPeakLevel = -1;
	if(APEHeader.nFormatFlags & MAC_FORMAT_FLAG_HAS_PEAK_LEVEL) {
		if(apeinfo_datas->iapedec_datas->fileio_funcs->fread(apeinfo_datas->iapedec_datas->fileio_datas, (unsigned char *)&nPeakLevel, 4) != 4)
			return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;
	}

	if(APEHeader.nFormatFlags & MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS) {
		if(apeinfo_datas->iapedec_datas->fileio_funcs->fread(apeinfo_datas->iapedec_datas->fileio_datas, (unsigned char *)&(apeinfo_datas->m_nSeekTableElements), 4) != 4)
			return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;
	} else
		apeinfo_datas->m_nSeekTableElements = APEHeader.nTotalFrames;

	loc_apefileinfo = apeinfo_datas->m_APEFileInfo;

	loc_apefileinfo->nCompressionLevel = (int)APEHeader.nCompressionLevel;
	loc_apefileinfo->nFormatFlags = (int)APEHeader.nFormatFlags;
	loc_apefileinfo->nTotalFrames = (int)APEHeader.nTotalFrames;
	loc_apefileinfo->nFinalFrameBlocks = (int)APEHeader.nFinalFrameBlocks;
	loc_apefileinfo->nBlocksPerFrame = ((loc_apefileinfo->nVersion >= 3900) || ((loc_apefileinfo->nVersion >= 3800) && (APEHeader.nCompressionLevel == COMPRESSION_LEVEL_EXTRA_HIGH))) ? 73728 : 9216;
	if((loc_apefileinfo->nVersion >= 3950))
		loc_apefileinfo->nBlocksPerFrame = 73728 * 4;
	loc_apefileinfo->nChannels = (int)APEHeader.nChannels;
	loc_apefileinfo->nSampleRate = (int)APEHeader.nSampleRate;
	loc_apefileinfo->nBitsPerSample = (loc_apefileinfo->nFormatFlags & MAC_FORMAT_FLAG_8_BIT) ? 8 : ((loc_apefileinfo->nFormatFlags & MAC_FORMAT_FLAG_24_BIT) ? 24 : 16);
	loc_apefileinfo->nBytesPerSample = loc_apefileinfo->nBitsPerSample / 8;
	loc_apefileinfo->nBlockAlign = loc_apefileinfo->nBytesPerSample * loc_apefileinfo->nChannels;
	loc_apefileinfo->nTotalBlocks = (APEHeader.nTotalFrames == 0) ? 0 : ((APEHeader.nTotalFrames - 1) * loc_apefileinfo->nBlocksPerFrame) + APEHeader.nFinalFrameBlocks;
	if(!loc_apefileinfo->nTotalBlocks)
		return APEDEC_ERROR_INVALID_INPUT_FILE;
	loc_apefileinfo->nWAVHeaderBytes = (APEHeader.nFormatFlags & MAC_FORMAT_FLAG_CREATE_WAV_HEADER) ? 44 : APEHeader.nHeaderBytes;
	loc_apefileinfo->nWAVTerminatingBytes = (int)APEHeader.nTerminatingBytes;
	loc_apefileinfo->nWAVDataBytes = loc_apefileinfo->nTotalBlocks * loc_apefileinfo->nBlockAlign;
	loc_apefileinfo->nAPETotalBytes = apeinfo_datas->iapedec_datas->fileio_funcs->filelength(apeinfo_datas->iapedec_datas->fileio_datas);
	loc_apefileinfo->nLengthMS = (int)(((double)(loc_apefileinfo->nTotalBlocks) * 1000.0) / (double)(loc_apefileinfo->nSampleRate));
	loc_apefileinfo->nAverageBitrate = (loc_apefileinfo->nLengthMS <= 0) ? 0 : (int)(((double)(loc_apefileinfo->nAPETotalBytes) * 8.0) / (double)(loc_apefileinfo->nLengthMS));
	loc_apefileinfo->nDecompressedBitrate = (loc_apefileinfo->nBlockAlign * loc_apefileinfo->nSampleRate * 8) / 1000;
	loc_apefileinfo->nPeakLevel = nPeakLevel;

	return APEDEC_ERROR_SUCCESS;
}

//load main header infos only
static int GetFileInformation_check(struct CAPEInfo_data_s *apeinfo_datas)
{
	APE_COMMON_HEADER CommonHeader;

	RETURN_ON_ERROR(SkipToAPEHeader(apeinfo_datas));

	apeinfo_datas->m_APEFileInfo = (struct APE_FILE_INFO *)calloc(1, sizeof(struct APE_FILE_INFO));
	if(!apeinfo_datas->m_APEFileInfo)
		return APEDEC_ERROR_INSUFFICIENT_MEMORY;

	if(apeinfo_datas->iapedec_datas->fileio_funcs->fread(apeinfo_datas->iapedec_datas->fileio_datas, (unsigned char *)&CommonHeader, APE_COMMON_HEADER_BYTES) != APE_COMMON_HEADER_BYTES)
		return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;

	if(read_le_long(&CommonHeader.cID[0]) != ' CAM')
		return APEDEC_ERROR_INVALID_INPUT_FILE;

	apeinfo_datas->m_APEFileInfo->nVersion = CommonHeader.nVersion;

	if((CommonHeader.nVersion < 1024) || (CommonHeader.nVersion > 3990))	// !!!
		return APEDEC_ERROR_INVALID_INPUT_FILE;

	if(CommonHeader.nVersion >= 3980)
		return AnalyzeCurrent(apeinfo_datas);

	return AnalyzeOld(apeinfo_datas);
}

// load all informations
static int GetFileInformation_open(struct CAPEInfo_data_s *apeinfo_datas)
{
	struct APE_FILE_INFO *loc_apefileinfo;
	struct mpxplay_filehand_buffered_func_s *fileio_funcs = apeinfo_datas->iapedec_datas->fileio_funcs;
	void *fileio_datas = apeinfo_datas->iapedec_datas->fileio_datas;
	long filesize, filepos;
	char id3tag[4];
	struct APETag_s apetag;

	RETURN_ON_ERROR(GetFileInformation_check(apeinfo_datas));

	loc_apefileinfo = apeinfo_datas->m_APEFileInfo;

	if(loc_apefileinfo->nVersion >= 3980) {
		// get the seek tables
		apeinfo_datas->m_spSeekByteTable = (unsigned long *)malloc(apeinfo_datas->m_nSeekTableElements * sizeof(unsigned long));
		if(!apeinfo_datas->m_spSeekByteTable)
			return APEDEC_ERROR_INSUFFICIENT_MEMORY;

		if(fileio_funcs->fread(fileio_datas, (unsigned char *)apeinfo_datas->m_spSeekByteTable, 4 * apeinfo_datas->m_nSeekTableElements) != (4 * apeinfo_datas->m_nSeekTableElements))
			return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;

		// skip the wave header data (Mpxplay doesn't use it)
		if(!(loc_apefileinfo->nFormatFlags & MAC_FORMAT_FLAG_CREATE_WAV_HEADER))
			if(fileio_funcs->fseek(fileio_datas, loc_apefileinfo->nWAVHeaderBytes, SEEK_CUR) < 0)
				return APEDEC_ERROR_IO_SEEK;

	} else {
		// skip the wave header data (Mpxplay doesn't use it)
		if(!(loc_apefileinfo->nFormatFlags & MAC_FORMAT_FLAG_CREATE_WAV_HEADER))
			if(fileio_funcs->fseek(fileio_datas, loc_apefileinfo->nWAVHeaderBytes, SEEK_CUR) < 0)
				return APEDEC_ERROR_IO_SEEK;

		// get the seek tables
		apeinfo_datas->m_spSeekByteTable = (unsigned long *)malloc(apeinfo_datas->m_nSeekTableElements * sizeof(unsigned long));
		if(!apeinfo_datas->m_spSeekByteTable)
			return APEDEC_ERROR_INSUFFICIENT_MEMORY;

		if(fileio_funcs->fread(fileio_datas, (unsigned char *)apeinfo_datas->m_spSeekByteTable, 4 * apeinfo_datas->m_nSeekTableElements) != (4 * apeinfo_datas->m_nSeekTableElements))
			return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;

		if(loc_apefileinfo->nVersion <= 3800) {
			apeinfo_datas->m_spSeekBitTable = (unsigned char *)malloc(apeinfo_datas->m_nSeekTableElements);
			if(!apeinfo_datas->m_spSeekBitTable)
				return APEDEC_ERROR_INSUFFICIENT_MEMORY;
			if(fileio_funcs->fread(fileio_datas, (unsigned char *)apeinfo_datas->m_spSeekBitTable, apeinfo_datas->m_nSeekTableElements) != apeinfo_datas->m_nSeekTableElements)
				return APEDEC_ERROR_INPUT_FILE_TOO_SMALL;
		}
	}

	filesize = fileio_funcs->filelength(fileio_datas);

	//check id3tag v1
	filepos = filesize - 128;
	if(fileio_funcs->fseek(fileio_datas, filepos, SEEK_SET) < 0)
		return APEDEC_ERROR_IO_SEEK;
	if(fileio_funcs->fread(fileio_datas, id3tag, 3) != 3)
		return APEDEC_ERROR_IO_READ;
	if(id3tag[0] == 'T' && id3tag[1] == 'A' && id3tag[2] == 'G')
		apeinfo_datas->tagsize = 128;
	else
		filepos = filesize;

	//check apetagex
	if(fileio_funcs->fseek(fileio_datas, filepos - ((long)sizeof(struct APETag_s)), SEEK_SET) < 0)
		return APEDEC_ERROR_IO_SEEK;
	if(fileio_funcs->fread(fileio_datas, (char *)&apetag, sizeof(struct APETag_s)) != sizeof(struct APETag_s))
		return APEDEC_ERROR_IO_READ;
	if(strncmp(apetag.ID, "APETAGEX", sizeof(apetag.ID)) == 0) {
		long version = read_le_long(apetag.Version);
		if(version == 1000 || version == 2000) {
			long taglen = read_le_long(apetag.Length);
			if((taglen > sizeof(struct APETag_s)) && (taglen < filesize))
				apeinfo_datas->tagsize += taglen;
		}
	}

	return APEDEC_ERROR_SUCCESS;
}

#define APE_HEADBUF_SIZE 256
#define APE_SYNC_RETRY (65536/APE_HEADBUF_SIZE)	// 65536 bytes long check

#define ID3V2_HEADSIZE 10
#define ID3V2_FOOTERSIZE 10

static int SkipToAPEHeader(struct CAPEInfo_data_s *apeinfo_datas)
{
	struct mpxplay_filehand_buffered_func_s *fileio_funcs = apeinfo_datas->iapedec_datas->fileio_funcs;
	void *fileio_datas = apeinfo_datas->iapedec_datas->fileio_datas;
	unsigned int bufferbytes, retry;
	unsigned char *bufp, ape_headbuf[APE_HEADBUF_SIZE];

	bufferbytes = fileio_funcs->fread(fileio_datas, ape_headbuf, ID3V2_HEADSIZE);
	if(bufferbytes != ID3V2_HEADSIZE)
		return APEDEC_ERROR_IO_READ;
	bufp = &ape_headbuf[0];

	if(bufp[0] == 'I' && bufp[1] == 'D' && bufp[2] == '3') {
		unsigned int id3v2totalsize, footer_present;

		id3v2totalsize = (bufp[6] & 127) << 21;
		id3v2totalsize += (bufp[7] & 127) << 14;
		id3v2totalsize += (bufp[8] & 127) << 7;
		id3v2totalsize += (bufp[9] & 127);

		if(bufp[5] & 16) {
			footer_present = TRUE;
			apeinfo_datas->m_nExtraHeaderBytes = ID3V2_HEADSIZE + id3v2totalsize + ID3V2_FOOTERSIZE;
		} else {
			footer_present = FALSE;
			apeinfo_datas->m_nExtraHeaderBytes = ID3V2_HEADSIZE + id3v2totalsize;
		}

		if(bufp[5] & 64) {
			//extended headers?
		}

		if(apeinfo_datas->m_nExtraHeaderBytes >= fileio_funcs->filelength(fileio_datas))	// invalid id3tag-len
			apeinfo_datas->m_nExtraHeaderBytes = ID3V2_HEADSIZE;
		else if(fileio_funcs->fseek(fileio_datas, apeinfo_datas->m_nExtraHeaderBytes, SEEK_SET) < 0)
			return APEDEC_ERROR_IO_SEEK;

		bufferbytes = fileio_funcs->fread(fileio_datas, ape_headbuf, APE_HEADBUF_SIZE);
		if(bufferbytes != APE_HEADBUF_SIZE)	// too short file
			return APEDEC_ERROR_IO_READ;
		bufp = &ape_headbuf[0];

		// scan for padding
		if(!footer_present) {
			retry = APE_SYNC_RETRY;
			do {
				if(bufp[0] && bufp[0] != 0xff)
					break;
				apeinfo_datas->m_nExtraHeaderBytes++;
				bufp++;
				bufferbytes--;
				if(bufferbytes < 4) {
					if(!(--retry))
						return APEDEC_ERROR_INVALID_INPUT_FILE;
					memcpy(ape_headbuf, bufp, bufferbytes);
					bufp = ape_headbuf;
					bufferbytes += fileio_funcs->fread(fileio_datas, &ape_headbuf[bufferbytes], APE_HEADBUF_SIZE - bufferbytes);
					if(bufferbytes < 4)
						return APEDEC_ERROR_IO_READ;
				}
			} while(1);
		}
	}
	//search for 'MAC ' header
	retry = APE_SYNC_RETRY;
	do {
		if(read_le_long(bufp) == ' CAM')
			break;
		apeinfo_datas->m_nExtraHeaderBytes++;
		bufp++;
		bufferbytes--;
		if(bufferbytes < 4) {
			if(!(--retry))
				return APEDEC_ERROR_INVALID_INPUT_FILE;
			memcpy(ape_headbuf, bufp, bufferbytes);
			bufp = ape_headbuf;
			bufferbytes += fileio_funcs->fread(fileio_datas, &ape_headbuf[bufferbytes], APE_HEADBUF_SIZE - bufferbytes);
			if(bufferbytes < 4)
				return APEDEC_ERROR_IO_READ;
		}
	} while(1);

	if(fileio_funcs->fseek(fileio_datas, apeinfo_datas->m_nExtraHeaderBytes, SEEK_SET) < 0)
		return APEDEC_ERROR_IO_SEEK;

	return APEDEC_ERROR_SUCCESS;
}

static void FillWaveFormatEx(WAVEFORMATEX * pWaveFormatEx, int nSampleRate, int nBitsPerSample, int nChannels)
{
	pWaveFormatEx->cbSize = 0;
	pWaveFormatEx->nSamplesPerSec = nSampleRate;
	pWaveFormatEx->wBitsPerSample = nBitsPerSample;
	pWaveFormatEx->nChannels = nChannels;
	pWaveFormatEx->wFormatTag = 1;
	pWaveFormatEx->nBlockAlign = (pWaveFormatEx->wBitsPerSample / 8) * pWaveFormatEx->nChannels;
	pWaveFormatEx->nAvgBytesPerSec = pWaveFormatEx->nBlockAlign * pWaveFormatEx->nSamplesPerSec;
}

int CAPEInfo_getinfo(struct CAPEInfo_data_s *apeinfo_datas, enum APE_DECOMPRESS_FIELDS Field, int nParam1, int nParam2)
{
	int nRetVal = -1;

	switch (Field) {
	case APE_INFO_FILE_VERSION:
		nRetVal = apeinfo_datas->m_APEFileInfo->nVersion;
		break;
	case APE_INFO_COMPRESSION_LEVEL:
		nRetVal = apeinfo_datas->m_APEFileInfo->nCompressionLevel;
		break;
	case APE_INFO_FORMAT_FLAGS:
		nRetVal = apeinfo_datas->m_APEFileInfo->nFormatFlags;
		break;
	case APE_INFO_SAMPLE_RATE:
		nRetVal = apeinfo_datas->m_APEFileInfo->nSampleRate;
		break;
	case APE_INFO_BITS_PER_SAMPLE:
		nRetVal = apeinfo_datas->m_APEFileInfo->nBitsPerSample;
		break;
	case APE_INFO_BYTES_PER_SAMPLE:
		nRetVal = apeinfo_datas->m_APEFileInfo->nBytesPerSample;
		break;
	case APE_INFO_CHANNELS:
		nRetVal = apeinfo_datas->m_APEFileInfo->nChannels;
		break;
	case APE_INFO_BLOCK_ALIGN:
		nRetVal = apeinfo_datas->m_APEFileInfo->nBlockAlign;
		break;
	case APE_INFO_BLOCKS_PER_FRAME:
		nRetVal = apeinfo_datas->m_APEFileInfo->nBlocksPerFrame;
		break;
	case APE_INFO_FINAL_FRAME_BLOCKS:
		nRetVal = apeinfo_datas->m_APEFileInfo->nFinalFrameBlocks;
		break;
	case APE_INFO_TOTAL_FRAMES:
		nRetVal = apeinfo_datas->m_APEFileInfo->nTotalFrames;
		break;
	case APE_INFO_WAV_TERMINATING_BYTES:
		nRetVal = apeinfo_datas->m_APEFileInfo->nWAVTerminatingBytes;
		break;
	case APE_INFO_WAV_DATA_BYTES:
		nRetVal = apeinfo_datas->m_APEFileInfo->nWAVDataBytes;
		break;
	case APE_INFO_APE_TOTAL_BYTES:
		nRetVal = apeinfo_datas->m_APEFileInfo->nAPETotalBytes;
		break;
	case APE_INFO_TOTAL_BLOCKS:
		nRetVal = apeinfo_datas->m_APEFileInfo->nTotalBlocks;
		break;
	case APE_INFO_LENGTH_MS:
		nRetVal = apeinfo_datas->m_APEFileInfo->nLengthMS;
		break;
	case APE_INFO_AVERAGE_BITRATE:
		nRetVal = apeinfo_datas->m_APEFileInfo->nAverageBitrate;
		break;
	case APE_INFO_FRAME_BITRATE:
		{
			int nFrame = nParam1;
			int nFrameBytes = CAPEInfo_getinfo(apeinfo_datas, APE_INFO_FRAME_BYTES, nFrame, 0);
			int nFrameBlocks = CAPEInfo_getinfo(apeinfo_datas, APE_INFO_FRAME_BLOCKS, nFrame, 0);
			nRetVal = 0;

			if((nFrameBytes > 0) && (nFrameBlocks > 0) && apeinfo_datas->m_APEFileInfo->nSampleRate > 0) {
				int nFrameMS = (nFrameBlocks * 1000) / apeinfo_datas->m_APEFileInfo->nSampleRate;
				if(nFrameMS != 0) {
					nRetVal = (nFrameBytes * 8) / nFrameMS;
				}
			}
			break;
		}
	case APE_INFO_DECOMPRESSED_BITRATE:
		nRetVal = apeinfo_datas->m_APEFileInfo->nDecompressedBitrate;
		break;
	case APE_INFO_PEAK_LEVEL:
		nRetVal = apeinfo_datas->m_APEFileInfo->nPeakLevel;
		break;
	case APE_INFO_SEEK_BIT:
		{
			int nFrame = nParam1;
			if(GET_FRAMES_START_ON_BYTES_BOUNDARIES(apeinfo_datas)) {
				nRetVal = 0;
			} else {
				if(nFrame < 0 || nFrame >= apeinfo_datas->m_APEFileInfo->nTotalFrames)
					nRetVal = 0;
				else
					nRetVal = apeinfo_datas->m_spSeekBitTable[nFrame];
			}
			break;
		}
	case APE_INFO_SEEK_BYTE:
		{
			int nFrame = nParam1;
			if(nFrame < 0 || nFrame >= apeinfo_datas->m_APEFileInfo->nTotalFrames)
				nRetVal = 0;
			else
				nRetVal = apeinfo_datas->m_spSeekByteTable[nFrame] + apeinfo_datas->m_nExtraHeaderBytes;
			break;
		}
	case APE_INFO_WAVEFORMATEX:
		{
			WAVEFORMATEX *pWaveFormatEx = (WAVEFORMATEX *) nParam1;
			FillWaveFormatEx(pWaveFormatEx, apeinfo_datas->m_APEFileInfo->nSampleRate, apeinfo_datas->m_APEFileInfo->nBitsPerSample, apeinfo_datas->m_APEFileInfo->nChannels);
			nRetVal = 0;
			break;
		}
	case APE_INFO_FRAME_BYTES:
		{
			int nFrame = nParam1;

			if((nFrame < 0) || (nFrame >= apeinfo_datas->m_APEFileInfo->nTotalFrames)) {
				nRetVal = -1;
			} else {
				if(nFrame != (apeinfo_datas->m_APEFileInfo->nTotalFrames - 1))
					nRetVal = CAPEInfo_getinfo(apeinfo_datas, APE_INFO_SEEK_BYTE, nFrame + 1, 0) - CAPEInfo_getinfo(apeinfo_datas, APE_INFO_SEEK_BYTE, nFrame, 0);
				else
					// ??? good?
					nRetVal =
						apeinfo_datas->iapedec_datas->fileio_funcs->filelength(apeinfo_datas->iapedec_datas->fileio_datas) - apeinfo_datas->tagsize -
						apeinfo_datas->m_APEFileInfo->nWAVTerminatingBytes - CAPEInfo_getinfo(apeinfo_datas, APE_INFO_SEEK_BYTE, nFrame, 0);
				// nRetVal = apeinfo_datas->iapedec_datas->fileio_funcs->filelength(apeinfo_datas->iapedec_datas->fileio_datas) - apeinfo_datas->m_spAPETag->apetag_funcs->GetTagBytes(apeinfo_datas->m_spAPETag) - apeinfo_datas->m_APEFileInfo->nWAVTerminatingBytes - CAPEInfo_getinfo(apeinfo_datas,APE_INFO_SEEK_BYTE, nFrame,0);
			}
			break;
		}
	case APE_INFO_FRAME_BLOCKS:
		{
			int nFrame = nParam1;

			if((nFrame < 0) || (nFrame >= apeinfo_datas->m_APEFileInfo->nTotalFrames)) {
				nRetVal = -1;
			} else {
				if(nFrame != (apeinfo_datas->m_APEFileInfo->nTotalFrames - 1))
					nRetVal = apeinfo_datas->m_APEFileInfo->nBlocksPerFrame;
				else
					nRetVal = apeinfo_datas->m_APEFileInfo->nFinalFrameBlocks;
			}
			break;
		}
	}

	return nRetVal;
}
