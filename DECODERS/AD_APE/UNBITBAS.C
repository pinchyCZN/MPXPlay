//**************************************************************************
//*                   This file is part of the                             *
//*           APE decoder of Mpxplay (http://mpxplay.cjb.net)              *
//*      based on the MAC SDK v3.97 (http://www.monkeysaudio.com)          *
//**************************************************************************
//*   This program is distributed in the hope that it will be useful,      *
//*   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
//*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                 *
//**************************************************************************

#include "All.h"
#include "maclib.h"
#include "unbitbas.h"
#include "unbitnew.h"
#ifdef BACKWARDS_COMPATIBILITY
#include "unbitold.h"
#endif

static const unsigned __int32 POWERS_OF_TWO_MINUS_ONE[33] = {
	0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535, 131071,
	262143, 524287, 1048575, 2097151, 4194303, 8388607, 16777215, 33554431, 67108863,
	134217727, 268435455, 536870911, 1073741823, 2147483647, 4294967295
};

static void *CreateUnBitArray(struct IAPEDecompress_data_s *pAPEDecompress, int nVersion, struct CUnBitArray_func_s **unbita_funcs);
static CUnBitArrayBase_data_s *UnBitArrayBase_init(struct IAPEDecompress_data_s *pAPEDecompress, int nBytes, int nVersion, struct CUnBitArrayBase_func_s **unbitbas_funcs);
static void UnBitArrayBase_close(struct CUnBitArrayBase_data_s *bitbas_data);
static int CUnBitArrayBase_AdvanceToByteBoundary(struct CUnBitArrayBase_data_s *bitbas_data);
static unsigned __int32 CUnBitArrayBase_DecodeValueXBits(struct CUnBitArrayBase_data_s *bitbas_data, unsigned __int32 nBits);
static int FillAndResetBitArray(struct CUnBitArrayBase_data_s *bitbas_data, int nFileLocation, int nNewBitIndex);
static int SeekBitArray(struct CUnBitArrayBase_data_s *bitbas_data, long newfilepos, int newbitindex);
static int FillBitArray(struct CUnBitArrayBase_data_s *bitbas_data);

struct CUnBitArrayBase_func_s CUnBitArrayBase_funcs = {
	&CreateUnBitArray,
	&UnBitArrayBase_init,
	&UnBitArrayBase_close,
	&CUnBitArrayBase_AdvanceToByteBoundary,
	&CUnBitArrayBase_DecodeValueXBits,
	&FillAndResetBitArray,
	&FillBitArray
};

static void *CreateUnBitArray(struct IAPEDecompress_data_s *pAPEDecompress, int nVersion, struct CUnBitArray_func_s **unbita_funcs)
{
	if(nVersion >= 3900)
		return (CUnBitArray_funcs.CUnBitArray_init(pAPEDecompress, nVersion, unbita_funcs));
#ifdef BACKWARDS_COMPATIBILITY
	else {
		return (CUnBitArrayOld_funcs.CUnBitArray_init(pAPEDecompress, nVersion, unbita_funcs));
	}
#endif
	return NULL;
}

static CUnBitArrayBase_data_s *UnBitArrayBase_init(struct IAPEDecompress_data_s *pAPEDecompress, int nBytes, int nVersion, struct CUnBitArrayBase_func_s **unbitbas_funcs)
{
	struct CUnBitArrayBase_data_s *bitbas_data;

	bitbas_data = (struct CUnBitArrayBase_data_s *)calloc(1, sizeof(struct CUnBitArrayBase_data_s));
	if(!bitbas_data)
		return bitbas_data;

	bitbas_data->required_bytes = nBytes;
	bitbas_data->apedec_datas = pAPEDecompress;
	bitbas_data->m_nVersion = nVersion;

	bitbas_data->m_pBitArray = (unsigned __int32 *)malloc(nBytes);
	if(!bitbas_data->m_pBitArray) {
		UnBitArrayBase_close(bitbas_data);
		return NULL;
	}
	*unbitbas_funcs = &CUnBitArrayBase_funcs;
	return bitbas_data;
}

static void UnBitArrayBase_close(struct CUnBitArrayBase_data_s *bitbas_data)
{
	if(bitbas_data) {
		if(bitbas_data->m_pBitArray)
			free(bitbas_data->m_pBitArray);
		free(bitbas_data);
	}
}

static int CUnBitArrayBase_AdvanceToByteBoundary(struct CUnBitArrayBase_data_s *bitbas_data)
{
	bitbas_data->m_nCurrentBitIndex = (bitbas_data->m_nCurrentBitIndex + 7) & (~7);
	return FillBitArray(bitbas_data);
}

static unsigned __int32 CUnBitArrayBase_DecodeValueXBits(struct CUnBitArrayBase_data_s *bitbas_data, unsigned __int32 nBits)
{
	unsigned __int32 nLeftBits, element_index, nLeftValue, nRightValue;
	int nRightBits;

	if((bitbas_data->m_nCurrentBitIndex + nBits) >= bitbas_data->stored_bits)
		FillBitArray(bitbas_data);

	nLeftBits = 32 - (bitbas_data->m_nCurrentBitIndex & 31);
	element_index = bitbas_data->m_nCurrentBitIndex >> 5;
	bitbas_data->m_nCurrentBitIndex += nBits;

	if(nLeftBits >= nBits)
		return (bitbas_data->m_pBitArray[element_index] & (POWERS_OF_TWO_MINUS_ONE[nLeftBits])) >> (nLeftBits - nBits);

	nRightBits = nBits - nLeftBits;

	nLeftValue = ((bitbas_data->m_pBitArray[element_index] & POWERS_OF_TWO_MINUS_ONE[nLeftBits]) << nRightBits);
	nRightValue = (bitbas_data->m_pBitArray[element_index + 1] >> (32 - nRightBits));
	return (nLeftValue | nRightValue);
}

static int FillAndResetBitArray(struct CUnBitArrayBase_data_s *bitbas_data, int nFileLocation, int nNewBitIndex)
{
	if(nFileLocation > 0) {
		RETURN_ON_ERROR(SeekBitArray(bitbas_data, nFileLocation, nNewBitIndex));
	} else if(nNewBitIndex >= 0)
		bitbas_data->m_nCurrentBitIndex = nNewBitIndex;

	return FillBitArray(bitbas_data);
}

static void resetbitarray(struct CUnBitArrayBase_data_s *bitbas_data)
{
	bitbas_data->m_nCurrentBitIndex = bitbas_data->stored_elements = bitbas_data->stored_bytes = bitbas_data->stored_bits = 0;
}

static int SeekBitArray(struct CUnBitArrayBase_data_s *bitbas_data, long newfilepos, int newbitindex)
{
	unsigned int do_seek = 0;

	if(!bitbas_data->filepos_of_stored_data) {	// this the first seek on this file
		do_seek = 1;
	} else {
		if(newfilepos == bitbas_data->filepos_of_stored_data) {
			bitbas_data->m_nCurrentBitIndex = 0;
		} else {
			if(newfilepos < bitbas_data->filepos_of_stored_data) {
				do_seek = 1;
			} else {
				unsigned __int32 bytes_to_skip = newfilepos - bitbas_data->filepos_of_stored_data;
				int nbitindex = (newbitindex >= 0) ? newbitindex : 0;
				if(bitbas_data->stored_bytes > (bytes_to_skip + ((nbitindex + 7) & (~7))))
					bitbas_data->m_nCurrentBitIndex = bytes_to_skip * 8;
				else
					do_seek = 1;
			}
		}
	}

	if(do_seek) {
		long retval = bitbas_data->apedec_datas->fileio_funcs->fseek(bitbas_data->apedec_datas->fileio_datas, newfilepos, SEEK_SET);
		if(retval < 0) {
			if(retval == MPXPLAY_ERROR_MPXINBUF_SEEK_EOF)
				return APEDEC_ERROR_IO_EOF;
			return APEDEC_ERROR_IO_SEEK;
		}
		resetbitarray(bitbas_data);
	} else {
		long s = (bitbas_data->filepos_of_stored_data + bitbas_data->stored_bytes);
		long retval = bitbas_data->apedec_datas->fileio_funcs->fseek(bitbas_data->apedec_datas->fileio_datas, s, SEEK_SET);
		if(retval < 0) {
			if(retval == MPXPLAY_ERROR_MPXINBUF_SEEK_EOF)
				return APEDEC_ERROR_IO_EOF;
			return APEDEC_ERROR_IO_SEEK;
		}
	}
	if(newbitindex >= 0)
		bitbas_data->m_nCurrentBitIndex += newbitindex;

	bitbas_data->filepos_of_stored_data = newfilepos;

	return APEDEC_ERROR_SUCCESS;
}

static int FillBitArray(struct CUnBitArrayBase_data_s *bitbas_data)
{
	unsigned __int32 element_index = bitbas_data->m_nCurrentBitIndex / 32;
	int nBytesToRead, currframe, frameusedbytes, frameleftbytes;
	int (*GetInfo) (void *decoder_datas, enum APE_DECOMPRESS_FIELDS Field, int nParam1, int nParam2);
	void *decoder_datas;
	//char sout[100];

	if(element_index) {
		if(element_index < bitbas_data->stored_elements) {
			memmove((void *)(bitbas_data->m_pBitArray), (const void *)(bitbas_data->m_pBitArray + element_index), (bitbas_data->stored_elements - element_index) * 4);
			nBytesToRead = element_index * 4;
			bitbas_data->m_nCurrentBitIndex &= 31;
			bitbas_data->stored_bytes -= nBytesToRead;
			bitbas_data->filepos_of_stored_data += nBytesToRead;
		} else {
			nBytesToRead = bitbas_data->required_bytes;
			resetbitarray(bitbas_data);
			bitbas_data->filepos_of_stored_data = bitbas_data->apedec_datas->fileio_funcs->ftell(bitbas_data->apedec_datas->fileio_datas);
		}
	} else {
		nBytesToRead = bitbas_data->required_bytes - bitbas_data->stored_bytes;
	}
	if(nBytesToRead > 0)
		bitbas_data->stored_bytes +=
			bitbas_data->apedec_datas->fileio_funcs->fread(bitbas_data->apedec_datas->fileio_datas, (unsigned char *)(bitbas_data->m_pBitArray) + bitbas_data->stored_bytes, nBytesToRead);
	bitbas_data->stored_elements = bitbas_data->stored_bytes / 4;
	bitbas_data->stored_bits = bitbas_data->stored_elements * 32;

	if(bitbas_data->stored_bytes >= bitbas_data->required_bytes)	// have to be no greater, max equal
		return APEDEC_ERROR_SUCCESS;
	GetInfo = bitbas_data->apedec_datas->decoder_funcs->GetInfo;
	decoder_datas = bitbas_data->apedec_datas->decoder_datas;
	currframe = GetInfo(decoder_datas, APE_DECOMPRESS_CURRENT_FRAME, 0, 0);
	frameusedbytes = (int)bitbas_data->filepos_of_stored_data - GetInfo(decoder_datas, APE_INFO_SEEK_BYTE, currframe, 0);	//&(~3);
	frameleftbytes = GetInfo(decoder_datas, APE_INFO_FRAME_BYTES, currframe, 0);
	frameleftbytes -= frameusedbytes;
	if(bitbas_data->stored_bytes >= frameleftbytes)
		return APEDEC_ERROR_SUCCESS;
	if(bitbas_data->apedec_datas->fileio_funcs->eof(bitbas_data->apedec_datas->fileio_datas))
		return APEDEC_ERROR_IO_EOF;
	return APEDEC_ERROR_IO_READ;
	/*sprintf(sout,"fill rb:%d sb:%d fb:%d fp:%d",
	   bitbas_data->required_bytes,
	   bitbas_data->stored_bytes,
	   GetInfo(decoder_datas,APE_INFO_FRAME_BYTES,currframe,0),
	   bitbas_data->apedec_datas->fileio_funcs->ftell(bitbas_data->apedec_datas->fileio_datas)
	   );
	   myprintf(sout); */
}
