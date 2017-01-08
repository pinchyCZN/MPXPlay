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
#include "dec_new.h"
#include "APEInfo.h"
#include "Prepare.h"
#include "unbitnew.h"
#include "pred_new.h"
#include "maclib.h"

static void CAPEDecompressNew_open(struct IAPEDecompress_data_s *apedec_maindatas);
static void CAPEDecompressNew_close(void *decoder_data);
static int CAPEDecompressNew_GetData(void *decoder_data, char *pBuffer, int nBlocks, int *pBlocksRetrieved);
static int CAPEDecompressNew_Seek(void *decoder_data, int nBlockOffset);
static int GetBlocks(struct CAPEDecompressNew_data_s *dec_data, unsigned char *pOutputBuffer, int *nBlocks);
static int StartFrame(struct CAPEDecompressNew_data_s *dec_data);
static int EndFrame(struct CAPEDecompressNew_data_s *dec_data);
static int SeekToFrame(struct CAPEDecompressNew_data_s *dec_data, int nFrameIndex);
static int CAPEDecompressNew_GetInfo(void *decoder_data, enum APE_DECOMPRESS_FIELDS Field, int nParam1, int nParam2);

struct CAPEDecompress_func_s CAPEDecompressNew_funcs = {
	&CAPEDecompressNew_open,
	&CAPEDecompressNew_close,
	&CAPEDecompressNew_GetData,
	&CAPEDecompressNew_Seek,
	&CAPEDecompressNew_GetInfo
};

static void CAPEDecompressNew_open(struct IAPEDecompress_data_s *apedec_maindatas)
{
	struct CAPEDecompressNew_data_s *dec_data;
	int fileversion, compr_level;

	dec_data = (struct CAPEDecompressNew_data_s *)calloc(1, sizeof(struct CAPEDecompressNew_data_s));
	if(!dec_data)
		return;

	apedec_maindatas->decoder_funcs = &CAPEDecompressNew_funcs;
	apedec_maindatas->decoder_datas = (void *)dec_data;

	dec_data->m_BitArrayStateX = (BIT_ARRAY_STATE *) calloc(1, sizeof(BIT_ARRAY_STATE));
	dec_data->m_BitArrayStateY = (BIT_ARRAY_STATE *) calloc(1, sizeof(BIT_ARRAY_STATE));
	if(!dec_data->m_BitArrayStateX || !dec_data->m_BitArrayStateY)
		goto err_out;

	dec_data->m_spAPEInfo = apedec_maindatas->apeinfo_datas;
	fileversion = CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_FILE_VERSION, 0, 0);

	CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_WAVEFORMATEX, (int)&(dec_data->m_wfeInput), 0);
	dec_data->m_nBlockAlign = CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_BLOCK_ALIGN, 0, 0);
	dec_data->m_nFinishBlock = CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_TOTAL_BLOCKS, 0, 0);

	dec_data->unbita_datas = CUnBitArrayBase_funcs.CreateUnBitArray(apedec_maindatas, fileversion, &dec_data->unbita_funcs);
	if(!dec_data->unbita_datas)
		goto err_out;

	compr_level = CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_COMPRESSION_LEVEL, 0, 0);
	if(fileversion >= 3950) {
		dec_data->predictor_datas_X = CPredictorDecompress3950toCurrent_funcs.Decompress_init(compr_level, fileversion, &dec_data->predictor_funcs_X);
		dec_data->predictor_datas_Y = CPredictorDecompress3950toCurrent_funcs.Decompress_init(compr_level, fileversion, &dec_data->predictor_funcs_Y);
	} else {
		dec_data->predictor_datas_X = CPredictorDecompressNormal3930to3950_funcs.Decompress_init(compr_level, fileversion, &dec_data->predictor_funcs_X);
		dec_data->predictor_datas_Y = CPredictorDecompressNormal3930to3950_funcs.Decompress_init(compr_level, fileversion, &dec_data->predictor_funcs_Y);
	}

	return;

  err_out:
	CAPEDecompressNew_close(dec_data);
	apedec_maindatas->decoder_datas = NULL;
}

static void CAPEDecompressNew_close(void *decoder_data)
{
	if(decoder_data) {
		struct CAPEDecompressNew_data_s *dec_data = (struct CAPEDecompressNew_data_s *)decoder_data;
		if(dec_data->unbita_funcs)
			dec_data->unbita_funcs->CUnBitArray_close(dec_data->unbita_datas);
		if(dec_data->predictor_funcs_X)
			dec_data->predictor_funcs_X->Decompress_close(dec_data->predictor_datas_X);
		if(dec_data->predictor_funcs_Y)
			dec_data->predictor_funcs_Y->Decompress_close(dec_data->predictor_datas_Y);
		if(dec_data->m_BitArrayStateX)
			free(dec_data->m_BitArrayStateX);
		if(dec_data->m_BitArrayStateY)
			free(dec_data->m_BitArrayStateY);
		free(decoder_data);
	}
}

static int CAPEDecompressNew_GetData(void *decoder_data, char *pBuffer, int nBlocks, int *pBlocksRetrieved)
{
	struct CAPEDecompressNew_data_s *dec_data = (struct CAPEDecompressNew_data_s *)decoder_data;
	int nBlocksUntilFinish, nBlocksToRetrieve, nRetVal;

	if(!dec_data)
		return APEDEC_ERROR_UNDEFINED;

	if(pBlocksRetrieved)
		*pBlocksRetrieved = 0;

	nBlocksUntilFinish = dec_data->m_nFinishBlock - dec_data->m_nCurrentBlock;
	nBlocksToRetrieve = min(nBlocks, nBlocksUntilFinish);

	nRetVal = GetBlocks(dec_data, (unsigned char *)pBuffer, &nBlocksToRetrieve);

	dec_data->m_nCurrentBlock += nBlocksToRetrieve;

	if(pBlocksRetrieved)
		*pBlocksRetrieved = nBlocksToRetrieve;

	return nRetVal;
}

// seek to frame-head (only)
static int CAPEDecompressNew_Seek(void *decoder_data, int nBlockOffset)
{
	struct CAPEDecompressNew_data_s *dec_data = (struct CAPEDecompressNew_data_s *)decoder_data;
	int nBaseFrame, blocks_per_frame, nRetVal;

	if(nBlockOffset >= dec_data->m_nFinishBlock)
		nBlockOffset = dec_data->m_nFinishBlock - 1;
	if(nBlockOffset < 0)
		nBlockOffset = 0;

	blocks_per_frame = CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_BLOCKS_PER_FRAME, 0, 0);
	nBaseFrame = nBlockOffset / blocks_per_frame;

	dec_data->m_nCurrentFrame = nBaseFrame;
	dec_data->m_nCurrentBlock = nBaseFrame * blocks_per_frame;
	dec_data->m_nBlocksProcessed = 0;

	nRetVal = SeekToFrame(dec_data, dec_data->m_nCurrentFrame);
	if(nRetVal != APEDEC_ERROR_SUCCESS) {
		dec_data->m_bCurrentFrameCorrupt = 1;
		return nRetVal;
	}
	dec_data->m_bCurrentFrameCorrupt = 0;

	return APEDEC_ERROR_SUCCESS;
}

/*****************************************************************************************
Decodes blocks of data
*****************************************************************************************/
static int GetBlocks(struct CAPEDecompressNew_data_s *dec_data, unsigned char *pOutputBuffer, int *nBlocksGet)
{
	int nRetVal, nBlocksLeft;

	if(*nBlocksGet <= 0)
		return 0;

	nBlocksLeft = *nBlocksGet;
	nRetVal = APEDEC_ERROR_SUCCESS;

	while(nBlocksLeft > 0) {
		int nBlocksUntilEndOfFrame, nBlocksThisPass, nBlocksProcessed;

		if(!dec_data->m_bCurrentFrameCorrupt) {
			nRetVal = dec_data->unbita_funcs->FillAndResetBitArray(dec_data->unbita_datas, -1, -1);
			if(nRetVal != APEDEC_ERROR_SUCCESS)
				break;
		}

		if(dec_data->m_nBlocksProcessed == 0) {
			nRetVal = StartFrame(dec_data);
			if(nRetVal != APEDEC_ERROR_SUCCESS)
				break;
		}

		nBlocksUntilEndOfFrame = CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_BLOCKS_PER_FRAME, 0, 0) - dec_data->m_nBlocksProcessed;
		nBlocksThisPass = min(nBlocksLeft, nBlocksUntilEndOfFrame);

		if(dec_data->m_wfeInput.nChannels == 2) {
			if((dec_data->m_nSpecialCodes & SPECIAL_FRAME_LEFT_SILENCE) && (dec_data->m_nSpecialCodes & SPECIAL_FRAME_RIGHT_SILENCE)) {
				for(nBlocksProcessed = 0; nBlocksProcessed < nBlocksThisPass; nBlocksProcessed++) {
					apedec_UnprepareNew(0, 0, &(dec_data->m_wfeInput), pOutputBuffer, &(dec_data->m_nCRC));
					pOutputBuffer += dec_data->m_nBlockAlign;
				}
			} else if(dec_data->m_nSpecialCodes & SPECIAL_FRAME_PSEUDO_STEREO) {
				for(nBlocksProcessed = 0; nBlocksProcessed < nBlocksThisPass; nBlocksProcessed++) {
					int X = dec_data->predictor_funcs_X->DecompressValue(dec_data->predictor_datas_X, dec_data->unbita_funcs->DecodeValueRange(dec_data->unbita_datas, dec_data->m_BitArrayStateX), 0);
					apedec_UnprepareNew(X, 0, &(dec_data->m_wfeInput), pOutputBuffer, &(dec_data->m_nCRC));
					pOutputBuffer += dec_data->m_nBlockAlign;
				}
			} else {
				if(dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo, APE_INFO_FILE_VERSION, 0, 0) >= 3950) {
					for(nBlocksProcessed = 0; nBlocksProcessed < nBlocksThisPass; nBlocksProcessed++) {
						int nY = dec_data->unbita_funcs->DecodeValueRange(dec_data->unbita_datas, dec_data->m_BitArrayStateY);
						int nX = dec_data->unbita_funcs->DecodeValueRange(dec_data->unbita_datas, dec_data->m_BitArrayStateX);
						int Y = dec_data->predictor_funcs_Y->DecompressValue(dec_data->predictor_datas_Y, nY, dec_data->m_nLastX);
						int X = dec_data->predictor_funcs_X->DecompressValue(dec_data->predictor_datas_X, nX, Y);
						dec_data->m_nLastX = X;
						apedec_UnprepareNew(X, Y, &(dec_data->m_wfeInput), pOutputBuffer, &(dec_data->m_nCRC));
						pOutputBuffer += dec_data->m_nBlockAlign;
					}
				} else {
					for(nBlocksProcessed = 0; nBlocksProcessed < nBlocksThisPass; nBlocksProcessed++) {
						int X =
							dec_data->predictor_funcs_X->DecompressValue(dec_data->predictor_datas_X, dec_data->unbita_funcs->DecodeValueRange(dec_data->unbita_datas, dec_data->m_BitArrayStateX), 0);
						int Y =
							dec_data->predictor_funcs_Y->DecompressValue(dec_data->predictor_datas_Y, dec_data->unbita_funcs->DecodeValueRange(dec_data->unbita_datas, dec_data->m_BitArrayStateY), 0);
						apedec_UnprepareNew(X, Y, &(dec_data->m_wfeInput), pOutputBuffer, &(dec_data->m_nCRC));
						pOutputBuffer += dec_data->m_nBlockAlign;
					}
				}
			}
		} else {
			if(dec_data->m_nSpecialCodes & SPECIAL_FRAME_MONO_SILENCE) {
				for(nBlocksProcessed = 0; nBlocksProcessed < nBlocksThisPass; nBlocksProcessed++) {
					apedec_UnprepareNew(0, 0, &(dec_data->m_wfeInput), pOutputBuffer, &(dec_data->m_nCRC));
					pOutputBuffer += dec_data->m_nBlockAlign;
				}
			} else {
				for(nBlocksProcessed = 0; nBlocksProcessed < nBlocksThisPass; nBlocksProcessed++) {
					int X = dec_data->predictor_funcs_X->DecompressValue(dec_data->predictor_datas_X, dec_data->unbita_funcs->DecodeValueRange(dec_data->unbita_datas, dec_data->m_BitArrayStateX), 0);
					apedec_UnprepareNew(X, 0, &(dec_data->m_wfeInput), pOutputBuffer, &(dec_data->m_nCRC));
					pOutputBuffer += dec_data->m_nBlockAlign;
				}
			}
		}

		dec_data->m_nBlocksProcessed += nBlocksThisPass;
		nBlocksLeft -= nBlocksThisPass;

		if(dec_data->m_nBlocksProcessed == CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_BLOCKS_PER_FRAME, 0, 0))
			nRetVal = EndFrame(dec_data);
	}

	*nBlocksGet -= nBlocksLeft;

	return nRetVal;
}

static int StartFrame(struct CAPEDecompressNew_data_s *dec_data)
{
	if(dec_data->m_bCurrentFrameCorrupt)
		RETURN_ON_ERROR(SeekToFrame(dec_data, dec_data->m_nCurrentFrame))

			dec_data->m_nCRC = 0xFFFFFFFF;
	dec_data->m_nStoredCRC = dec_data->unbita_funcs->DecodeValue(dec_data->unbita_datas, DECODE_VALUE_METHOD_UNSIGNED_INT, 0, 0);

	dec_data->m_nSpecialCodes = 0;
	if(GET_USES_SPECIAL_FRAMES(dec_data->m_spAPEInfo)) {
		if(dec_data->m_nStoredCRC & 0x80000000) {
			dec_data->m_nSpecialCodes = dec_data->unbita_funcs->DecodeValue(dec_data->unbita_datas, DECODE_VALUE_METHOD_UNSIGNED_INT, 0, 0);
		}
		dec_data->m_nStoredCRC &= 0x7fffffff;
	}

	dec_data->predictor_funcs_X->Flush(dec_data->predictor_datas_X);
	dec_data->predictor_funcs_Y->Flush(dec_data->predictor_datas_Y);

	dec_data->unbita_funcs->FlushState(dec_data->m_BitArrayStateX);
	dec_data->unbita_funcs->FlushState(dec_data->m_BitArrayStateY);
	dec_data->unbita_funcs->FlushBitArray(dec_data->unbita_datas);

	dec_data->m_bCurrentFrameCorrupt = FALSE;
	dec_data->m_nLastX = 0;

	return APEDEC_ERROR_SUCCESS;
}

static int EndFrame(struct CAPEDecompressNew_data_s *dec_data)
{
	int nRetVal = APEDEC_ERROR_SUCCESS;

	dec_data->m_nCurrentFrame++;
	dec_data->m_nBlocksProcessed = 0;

	if(dec_data->m_bCurrentFrameCorrupt == FALSE) {
		dec_data->unbita_funcs->Finalize(dec_data->unbita_datas);
		dec_data->m_nCRC = dec_data->m_nCRC ^ 0xFFFFFFFF;
		dec_data->m_nCRC >>= 1;
		if(dec_data->m_nCRC != dec_data->m_nStoredCRC) {
			nRetVal = APEDEC_ERROR_INVALID_CHECKSUM;
			dec_data->m_bCurrentFrameCorrupt = TRUE;
		}
	}

	return nRetVal;
}

/*****************************************************************************************
Seek to the proper frame (if necessary) and do any alignment of the bit array
*****************************************************************************************/
static int SeekToFrame(struct CAPEDecompressNew_data_s *dec_data, int nFrameIndex)
{
	int nSeekRemainder = (CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_SEEK_BYTE, nFrameIndex, 0) - CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_SEEK_BYTE, 0, 0)) % 4;
	return dec_data->unbita_funcs->FillAndResetBitArray(dec_data->unbita_datas, CAPEDecompressNew_GetInfo((void *)dec_data, APE_INFO_SEEK_BYTE, nFrameIndex, 0) - nSeekRemainder, nSeekRemainder * 8);
}

/*****************************************************************************************
Get information from the decompressor
*****************************************************************************************/
static int CAPEDecompressNew_GetInfo(void *decoder_data, enum APE_DECOMPRESS_FIELDS Field, int nParam1, int nParam2)
{
	struct CAPEDecompressNew_data_s *dec_data = (struct CAPEDecompressNew_data_s *)decoder_data;
	int nRetVal = 0;

	switch (Field) {
	case APE_DECOMPRESS_CURRENT_BLOCK:
		nRetVal = dec_data->m_nCurrentBlock;
		break;
	case APE_DECOMPRESS_CURRENT_MS:
		{
			int nSampleRate = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo, APE_INFO_SAMPLE_RATE, 0, 0);
			if(nSampleRate > 0)
				nRetVal = (int)(((double)(dec_data->m_nCurrentBlock) * 1000.0) / ((double)nSampleRate));
			break;
		}
	case APE_DECOMPRESS_TOTAL_BLOCKS:
		nRetVal = dec_data->m_nFinishBlock;
		break;
	case APE_DECOMPRESS_LENGTH_MS:
		{
			int nSampleRate = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo, APE_INFO_SAMPLE_RATE, 0, 0);
			if(nSampleRate > 0)
				nRetVal = (int)(((double)(dec_data->m_nFinishBlock) * 1000.0) / ((double)nSampleRate));
			break;
		}
	case APE_DECOMPRESS_CURRENT_BITRATE:
		nRetVal = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo, APE_INFO_FRAME_BITRATE, dec_data->m_nCurrentFrame, 0);
		break;
	case APE_DECOMPRESS_AVERAGE_BITRATE:
		nRetVal = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo, APE_INFO_AVERAGE_BITRATE, 0, 0);
		break;
	case APE_DECOMPRESS_CURRENT_FRAME:
		nRetVal = dec_data->m_nCurrentFrame;
		break;
	default:
		nRetVal = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo, Field, nParam1, nParam2);
	}

	return nRetVal;
}
