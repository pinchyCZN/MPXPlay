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
#ifdef BACKWARDS_COMPATIBILITY

#include "APEInfo.h"
#include "unbitbas.h"
#include "pred_old.h"
#include "prepare.h"
#include "unbitnew.h"
#include "dec_old.h"

static void CAPEDecompressOld_open(struct IAPEDecompress_data_s *apedec_maindatas);
static void CAPEDecompressOld_close(void *decoder_data);
static int CAPEDecompressOld_GetData(void *decoder_data,char *pBuffer,int nBlocks,int *pBlocksRetrieved);
static int CAPEDecompressOld_Seek(void *decoder_data,int nBlockOffset);
static int CAPEDecompressOld_GetInfo(void *decoder_data,enum APE_DECOMPRESS_FIELDS Field,int nParam1,int nParam2);

static struct CUnMAC_data_s *CUnMAC_Init(struct CAPEDecompressOld_data_s *dec_data,struct IAPEDecompress_data_s *apedec_maindatas);
static void CUnMAC_close(struct CUnMAC_data_s *unmac_data);
static int CUnMAC_DecompressFrame(struct CAPEDecompressOld_data_s *dec_data,unsigned char *pOutputData,__int32 FrameIndex);
static int CUnMAC_SeekToFrame(struct CAPEDecompressOld_data_s *dec_data,int FrameIndex);

struct CAPEDecompress_func_s CAPEDecompressOld_funcs={
 &CAPEDecompressOld_open,
 &CAPEDecompressOld_close,
 &CAPEDecompressOld_GetData,
 &CAPEDecompressOld_Seek,
 &CAPEDecompressOld_GetInfo
};

static void CAPEDecompressOld_open(struct IAPEDecompress_data_s *apedec_maindatas)
{
 struct CAPEDecompressOld_data_s *dec_data;
 int nMaximumDecompressedFrameBytes,nTotalBufferBytes;

 dec_data=(struct CAPEDecompressOld_data_s *)calloc(1,sizeof(struct CAPEDecompressOld_data_s));
 if(!dec_data)
  return;

 apedec_maindatas->decoder_funcs=&CAPEDecompressOld_funcs;
 apedec_maindatas->decoder_datas=(void *)dec_data;

 dec_data->m_spAPEInfo=apedec_maindatas->apeinfo_datas;

 dec_data->m_nBlockAlign  = CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_BLOCK_ALIGN,0,0);
 dec_data->m_nFinishBlock = CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_TOTAL_BLOCKS,0,0);

 dec_data->m_UnMAC=CUnMAC_Init(dec_data,apedec_maindatas);
 if(!dec_data->m_UnMAC)
  goto err_out;

 nMaximumDecompressedFrameBytes = dec_data->m_nBlockAlign * CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_BLOCKS_PER_FRAME,0,0);
 nTotalBufferBytes = max(65536, (nMaximumDecompressedFrameBytes + 16) * 2);
 dec_data->m_spBuffer=(char *)malloc(nTotalBufferBytes);
 if(!dec_data->m_spBuffer)
  goto err_out;

 return;

err_out:
 CAPEDecompressOld_close(dec_data);
 apedec_maindatas->decoder_datas=NULL;
}

static void CAPEDecompressOld_close(void *decoder_data)
{
 if(decoder_data){
  struct CAPEDecompressOld_data_s *dec_data=(struct CAPEDecompressOld_data_s *)decoder_data;
  CUnMAC_close(dec_data->m_UnMAC);
  if(dec_data->m_spBuffer)
   free(dec_data->m_spBuffer);
  free(decoder_data);
 }
}

static int CAPEDecompressOld_GetData(void *decoder_data,char *pBuffer,int nBlocks,int *pBlocksRetrieved)
{
 struct CAPEDecompressOld_data_s *dec_data=(struct CAPEDecompressOld_data_s *)decoder_data;
 int nBlocksUntilFinish,nBlocksRetrieved,nTotalBytesNeeded,nBytesLeft,nBlocksDecoded;
 int retry=1;
 //char sout[100];

 if(pBlocksRetrieved)
  *pBlocksRetrieved = 0;

 nBlocksUntilFinish = dec_data->m_nFinishBlock - dec_data->m_nCurrentBlock;
 nBlocks = min(nBlocks, nBlocksUntilFinish);

 nBlocksRetrieved = 0;

 nTotalBytesNeeded = nBlocks * dec_data->m_nBlockAlign;
 nBytesLeft = nTotalBytesNeeded;
 nBlocksDecoded = 1;

 while(nBytesLeft > 0){
  int nBytesAvailable = dec_data->m_nBufferTail;
  int nIntialBytes = min(nBytesLeft, nBytesAvailable);
  if(nIntialBytes > 0){
   memcpy(&pBuffer[nTotalBytesNeeded - nBytesLeft],dec_data->m_spBuffer, nIntialBytes);

   if((dec_data->m_nBufferTail - nIntialBytes) > 0)
    memmove(dec_data->m_spBuffer,&dec_data->m_spBuffer[nIntialBytes],dec_data->m_nBufferTail - nIntialBytes);

   nBytesLeft -= nIntialBytes;
   dec_data->m_nBufferTail -= nIntialBytes;
  }

  if(nBytesLeft > 0){
   nBlocksDecoded = CUnMAC_DecompressFrame(dec_data,(unsigned char *)&dec_data->m_spBuffer[dec_data->m_nBufferTail],dec_data->m_nCurrentFrame);
   //sprintf(sout,"decoded: %d  frame:%d",nBlocksDecoded,dec_data->m_nCurrentFrame);
   //myprintf(sout);
   if(nBlocksDecoded!=APEDEC_ERROR_IO_SEEK && nBlocksDecoded!=APEDEC_ERROR_IO_READ)
    dec_data->m_nCurrentFrame++;
   if(nBlocksDecoded==0)
    break;
   if(nBlocksDecoded<0){
    if(nBlocksDecoded==APEDEC_ERROR_IO_EOF)
     break;
    if(!retry)
     break;
    retry--;
    dec_data->m_UnMAC->m_LastDecodedFrameIndex = -1;
    if(CUnMAC_SeekToFrame(dec_data,dec_data->m_nCurrentFrame)!=APEDEC_ERROR_SUCCESS){
     //myprintf("failed resync");
     break;
    }
   }else
    dec_data->m_nBufferTail += (nBlocksDecoded * dec_data->m_nBlockAlign);
  }
 }

 nBlocksRetrieved = (nTotalBytesNeeded - nBytesLeft) / dec_data->m_nBlockAlign;

 dec_data->m_nCurrentBlock += nBlocksRetrieved;

 if(pBlocksRetrieved)
  *pBlocksRetrieved = nBlocksRetrieved;

 return 0;
}

// seek to frame-head (only)
static int CAPEDecompressOld_Seek(void *decoder_data,int nBlockOffset)
{
 struct CAPEDecompressOld_data_s *dec_data=(struct CAPEDecompressOld_data_s *)decoder_data;
 int nBaseFrame,blocks_per_frame;

 if(nBlockOffset >= dec_data->m_nFinishBlock)
  nBlockOffset = dec_data->m_nFinishBlock - 1;
 if(nBlockOffset < 0)
  nBlockOffset = 0;

 dec_data->m_nBufferTail = 0;

 blocks_per_frame=CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_BLOCKS_PER_FRAME,0,0);
 nBaseFrame = nBlockOffset / blocks_per_frame;

 dec_data->m_nCurrentFrame = nBaseFrame;
 dec_data->m_nCurrentBlock = nBaseFrame*blocks_per_frame;

 RETURN_ON_ERROR(CUnMAC_SeekToFrame(dec_data,dec_data->m_nCurrentFrame));

 return 0;
}

static int CAPEDecompressOld_GetInfo(void *decoder_data,enum APE_DECOMPRESS_FIELDS Field,int nParam1,int nParam2)
{
 struct CAPEDecompressOld_data_s *dec_data=(struct CAPEDecompressOld_data_s *)decoder_data;
 int nRetVal = 0;

 switch (Field){
  case APE_DECOMPRESS_CURRENT_BLOCK:
   nRetVal = dec_data->m_nCurrentBlock;
   break;
  case APE_DECOMPRESS_CURRENT_MS:
   {
    int nSampleRate = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo,APE_INFO_SAMPLE_RATE,0,0);
    if (nSampleRate > 0)
     nRetVal = (int)((double)dec_data->m_nCurrentBlock * 1000.0 / (double)nSampleRate);
   }
   break;
  case APE_DECOMPRESS_TOTAL_BLOCKS:
   nRetVal = dec_data->m_nFinishBlock;
   break;
  case APE_DECOMPRESS_LENGTH_MS:
   {
    int nSampleRate = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo,APE_INFO_SAMPLE_RATE,0,0);
    if (nSampleRate > 0)
     nRetVal = (int)((double)(dec_data->m_nFinishBlock) * 1000.0 / (double)nSampleRate);
   }
   break;
  case APE_DECOMPRESS_CURRENT_BITRATE:
   nRetVal = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo,APE_INFO_FRAME_BITRATE,dec_data->m_nCurrentFrame,0);
   break;
  case APE_DECOMPRESS_AVERAGE_BITRATE:
   nRetVal = dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo,APE_INFO_AVERAGE_BITRATE,0,0);
   break;
  case APE_DECOMPRESS_CURRENT_FRAME:
   nRetVal = dec_data->m_nCurrentFrame;
   break;
  default:nRetVal=dec_data->m_spAPEInfo->apeinfo_funcs->GetInfo(dec_data->m_spAPEInfo,Field,nParam1,nParam2);
 }

 return nRetVal;
}

//------------------------------------------------------------------------
static void CAPEDecompressOldCore_close(struct CAPEDecompressOldCore_data_s *core_data);
#ifdef APEDEC_USE_MMX
extern int apedec_GetMMXAvailable(void);
#endif

static struct CAPEDecompressOldCore_data_s *CAPEDecompressOldCore_init(struct CAPEDecompressOld_data_s *dec_data,struct IAPEDecompress_data_s *apedec_maindatas)
{
 struct CAPEDecompressOldCore_data_s *core_data;
 int fileversion,compr_level,blocks_per_frame;

 fileversion=CAPEDecompressOld_GetInfo(dec_data,APE_INFO_FILE_VERSION,0,0);
 if(fileversion >= 3930)
  return NULL;

 core_data=(struct CAPEDecompressOldCore_data_s *)calloc(1,sizeof(struct CAPEDecompressOldCore_data_s));
 if(!core_data)
  return core_data;

 core_data->decoder_datas=(void *)dec_data;

 core_data->m_BitArrayStateX=(BIT_ARRAY_STATE *)calloc(1,sizeof(BIT_ARRAY_STATE));
 core_data->m_BitArrayStateY=(BIT_ARRAY_STATE *)calloc(1,sizeof(BIT_ARRAY_STATE));
 if(!core_data->m_BitArrayStateX || !core_data->m_BitArrayStateY)
  goto err_out_core;

 core_data->unbita_datas=CUnBitArrayBase_funcs.CreateUnBitArray(apedec_maindatas,fileversion,&core_data->unbita_funcs);
 if(!core_data->unbita_datas)
  goto err_out_core;

 compr_level=CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_COMPRESSION_LEVEL,0,0);

 core_data->m_pAntiPredictorX = CreateAntiPredictorOld(compr_level,fileversion);
 core_data->m_pAntiPredictorY = CreateAntiPredictorOld(compr_level,fileversion);
 if(!core_data->m_pAntiPredictorX || !core_data->m_pAntiPredictorY)
  goto err_out_core;

 blocks_per_frame=CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_BLOCKS_PER_FRAME,0,0);
 core_data->m_pDataX    = (int *)malloc((blocks_per_frame+16)*sizeof(int));
 core_data->m_pDataY    = (int *)malloc((blocks_per_frame+16)*sizeof(int));
 core_data->m_pTempData = (int *)malloc((blocks_per_frame+16)*sizeof(int));
 if(!core_data->m_pDataX || ! core_data->m_pDataY || !core_data->m_pTempData)
  goto err_out_core;

 if((compr_level==COMPRESSION_LEVEL_EXTRA_HIGH) && (fileversion>=3800)){
  core_data->IPAdaptFactor=(short *)malloc((blocks_per_frame)*sizeof(short));
  core_data->IPShort      =(short *)malloc((blocks_per_frame)*sizeof(short));
  if(!core_data->IPAdaptFactor || !core_data->IPShort)
   goto err_out_core;
 }

#ifdef APEDEC_USE_MMX
 core_data->m_bMMXAvailable = apedec_GetMMXAvailable();
#endif
 return core_data;

err_out_core:
 CAPEDecompressOldCore_close(core_data);
 return NULL;
}

static void CAPEDecompressOldCore_close(struct CAPEDecompressOldCore_data_s *core_data)
{
 if(core_data){
  if(core_data->unbita_funcs)
   core_data->unbita_funcs->CUnBitArray_close(core_data->unbita_datas);
  if(core_data->m_BitArrayStateX)
   free(core_data->m_BitArrayStateX);
  if(core_data->m_BitArrayStateY)
   free(core_data->m_BitArrayStateY);
  if(core_data->m_pDataX)
   free(core_data->m_pDataX);
  if(core_data->m_pDataY)
   free(core_data->m_pDataY);
  if(core_data->m_pTempData)
   free(core_data->m_pTempData);
  if(core_data->IPAdaptFactor)
   free(core_data->IPAdaptFactor);
  if(core_data->IPShort)
   free(core_data->IPShort);
  free(core_data);
 }
}

static void CAPEDecompressOldCore_GenerateDecodedArray(struct CAPEDecompressOldCore_data_s *core_data,int *Input_Array, unsigned __int32 Number_of_Elements,int Frame_Index,void *pAntiPredictor)
{
 int nFrameBytes,fileversion;
 unsigned int z,nNumberOfCoefficients,aryCoefficientsA[64],aryCoefficientsB[64];

#define GET_COEFFICIENTS(NumberOfCoefficientsBits, ValueBits) \
 nNumberOfCoefficients = core_data->unbita_funcs->DecodeValue(core_data->unbita_datas,DECODE_VALUE_METHOD_X_BITS,NumberOfCoefficientsBits,0); \
 for(z = 0; z <= nNumberOfCoefficients; z++){                                                             \
  aryCoefficientsA[z] = core_data->unbita_funcs->DecodeValue(core_data->unbita_datas,DECODE_VALUE_METHOD_X_BITS,ValueBits,0); \
  aryCoefficientsB[z] = core_data->unbita_funcs->DecodeValue(core_data->unbita_datas,DECODE_VALUE_METHOD_X_BITS,ValueBits,0); \
 }																									\

 nFrameBytes = CAPEDecompressOld_GetInfo(core_data->decoder_datas,APE_INFO_FRAME_BYTES,Frame_Index,0);
 fileversion = CAPEDecompressOld_GetInfo(core_data->decoder_datas,APE_INFO_FILE_VERSION,0,0);

 switch(CAPEDecompressOld_GetInfo(core_data->decoder_datas,APE_INFO_COMPRESSION_LEVEL,0,0)){
  case COMPRESSION_LEVEL_FAST:
   if(fileversion < 3320){
    core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
    ((predoldfunc_fnh_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements);
   }else{
    core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,Input_Array, Number_of_Elements, nFrameBytes);
    ((predoldfunc_fnh_t)pAntiPredictor)(Input_Array, NULL, Number_of_Elements);
   }
   break;
  case COMPRESSION_LEVEL_NORMAL:
   core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
   ((predoldfunc_fnh_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements);
   break;
  case COMPRESSION_LEVEL_HIGH:
   core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
   ((predoldfunc_fnh_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements);
   break;
  case COMPRESSION_LEVEL_EXTRA_HIGH:
   if(fileversion < 3320){
    GET_COEFFICIENTS(4, 6)
    core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
    ((predoldfunc_eh_0000to3799_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements, nNumberOfCoefficients, &aryCoefficientsA[0], &aryCoefficientsB[0]);
   }else
    if(fileversion < 3600){
     GET_COEFFICIENTS(3, 5)
     core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
     ((predoldfunc_eh_0000to3799_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements, nNumberOfCoefficients, &aryCoefficientsA[0], &aryCoefficientsB[0]);
    }else
     if (fileversion < 3700){
      GET_COEFFICIENTS(3, 6)
      core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
      ((predoldfunc_eh_0000to3799_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements, nNumberOfCoefficients, &aryCoefficientsA[0], &aryCoefficientsB[0]);
     }else
      if (fileversion < 3800){
       GET_COEFFICIENTS(3, 6)
       core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
       ((predoldfunc_eh_0000to3799_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements, nNumberOfCoefficients, &aryCoefficientsA[0], &aryCoefficientsB[0]);
      }else{
       core_data->unbita_funcs->GenerateArray(core_data->unbita_datas,core_data->m_pTempData, Number_of_Elements, nFrameBytes);
       ((predoldfunc_eh_3800toCurrent_t)pAntiPredictor)(core_data->m_pTempData, Input_Array, Number_of_Elements,core_data->m_bMMXAvailable,fileversion,core_data->IPAdaptFactor,core_data->IPShort);
      }
   break;
 }
}

static void CAPEDecompressOldCore_GenerateDecodedArrays(struct CAPEDecompressOldCore_data_s *core_data,int nBlocks,int nSpecialCodes,int nFrameIndex)
{
 if(CAPEDecompressOld_GetInfo(core_data->decoder_datas,APE_INFO_CHANNELS,0,0) == 2){
  if((nSpecialCodes & SPECIAL_FRAME_LEFT_SILENCE) && (nSpecialCodes & SPECIAL_FRAME_RIGHT_SILENCE)){
   memset(core_data->m_pDataX, 0, nBlocks * 4);
   memset(core_data->m_pDataY, 0, nBlocks * 4);
  }else
   if(nSpecialCodes & SPECIAL_FRAME_PSEUDO_STEREO){
    CAPEDecompressOldCore_GenerateDecodedArray(core_data,core_data->m_pDataX,nBlocks,nFrameIndex,core_data->m_pAntiPredictorX);
    memset(core_data->m_pDataY, 0, nBlocks * 4);
   }else{
    CAPEDecompressOldCore_GenerateDecodedArray(core_data,core_data->m_pDataX,nBlocks,nFrameIndex,core_data->m_pAntiPredictorX);
    CAPEDecompressOldCore_GenerateDecodedArray(core_data,core_data->m_pDataY,nBlocks,nFrameIndex,core_data->m_pAntiPredictorY);
   }
 }else{
  if(nSpecialCodes & SPECIAL_FRAME_LEFT_SILENCE){
   memset(core_data->m_pDataX, 0, nBlocks * 4);
  }else{
   CAPEDecompressOldCore_GenerateDecodedArray(core_data,core_data->m_pDataX,nBlocks,nFrameIndex,core_data->m_pAntiPredictorX);
  }
 }
}

//-------------------------------------------------------------------------

static struct CUnMAC_data_s *CUnMAC_Init(struct CAPEDecompressOld_data_s *dec_data,struct IAPEDecompress_data_s *apedec_maindatas)
{
 struct CUnMAC_data_s *unmac_data;
 unmac_data=(struct CUnMAC_data_s *)calloc(1,sizeof(struct CUnMAC_data_s));
 if(!unmac_data)
  return unmac_data;

 unmac_data->core_datas = CAPEDecompressOldCore_init(dec_data,apedec_maindatas);
 if(!unmac_data->core_datas){
  CUnMAC_close(unmac_data);
  return NULL;
 }

 unmac_data->m_LastDecodedFrameIndex = -1;
 CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_WAVEFORMATEX,(int)&unmac_data->m_wfeInput,0); // ???

 return unmac_data;
}

static void CUnMAC_close(struct CUnMAC_data_s *unmac_data)
{
 if(unmac_data){
  CAPEDecompressOldCore_close(unmac_data->core_datas);
  free(unmac_data);
 }
}

static unsigned __int32 CUnMAC_CalculateOldChecksum(int *pDataX,int *pDataY,int nChannels,int nBlocks)
{
 unsigned __int32 nChecksum = 0;
 int z;

 if (nChannels == 2){
  for (z = 0; z < nBlocks; z++){
   int R = pDataX[z] - (pDataY[z] / 2);
   int L = R + pDataY[z];
   nChecksum += (labs(R) + labs(L));
  }
 }else
  if (nChannels == 1){
   for (z = 0; z < nBlocks; z++)
    nChecksum += labs(pDataX[z]);
  }

 return nChecksum;
}

static int CUnMAC_DecompressFrame(struct CAPEDecompressOld_data_s *dec_data,unsigned char *pOutputData,__int32 FrameIndex)
{
 struct CUnMAC_data_s *unmac_data=dec_data->m_UnMAC;
 struct CAPEDecompressOldCore_data_s *core_data=unmac_data->core_datas;
 int nBlocks;
 unsigned int nSpecialCodes;
 unsigned __int32 nStoredCRC,CRC;

 if(FrameIndex >= CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_TOTAL_FRAMES,0,0))
  return 0;

 nBlocks = ((FrameIndex + 1) >= CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_TOTAL_FRAMES,0,0))? CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_FINAL_FRAME_BLOCKS,0,0) : CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_BLOCKS_PER_FRAME,0,0);
 if(nBlocks == 0)
  return -1;

 RETURN_ON_ERROR(CUnMAC_SeekToFrame(dec_data,FrameIndex));

 nSpecialCodes = 0;
 nStoredCRC = 0;

 if(GET_USES_CRC(dec_data->m_spAPEInfo) == FALSE){
  nStoredCRC = core_data->unbita_funcs->DecodeValue(core_data->unbita_datas,DECODE_VALUE_METHOD_UNSIGNED_RICE,30,0);
  if(nStoredCRC == 0)
   nSpecialCodes = SPECIAL_FRAME_LEFT_SILENCE | SPECIAL_FRAME_RIGHT_SILENCE;
 }else{
  nStoredCRC = core_data->unbita_funcs->DecodeValue(core_data->unbita_datas,DECODE_VALUE_METHOD_UNSIGNED_INT,0,0);

  nSpecialCodes = 0;
  if(GET_USES_SPECIAL_FRAMES(dec_data->m_spAPEInfo)){
   if(nStoredCRC & 0x80000000)
    nSpecialCodes = core_data->unbita_funcs->DecodeValue(core_data->unbita_datas,DECODE_VALUE_METHOD_UNSIGNED_INT,0,0);
   nStoredCRC &= 0x7fffffff;
  }
 }

 CRC = 0xffffffff;

 if(CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_CHANNELS,0,0)==2){
  CAPEDecompressOldCore_GenerateDecodedArrays(core_data,nBlocks,nSpecialCodes,FrameIndex);
  apedec_UnprepareOld(core_data->m_pDataX,core_data->m_pDataY,nBlocks,&unmac_data->m_wfeInput,pOutputData,(unsigned int *)&CRC,(int *)&nSpecialCodes,CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_FILE_VERSION,0,0));
 }else
  if(CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_CHANNELS,0,0)==1){
   CAPEDecompressOldCore_GenerateDecodedArrays(core_data,nBlocks,nSpecialCodes,FrameIndex);
   apedec_UnprepareOld(core_data->m_pDataX,NULL,nBlocks,&unmac_data->m_wfeInput,pOutputData,(unsigned int *)&CRC,(int *)&nSpecialCodes,CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_FILE_VERSION,0,0));
  }

 if(GET_USES_SPECIAL_FRAMES(dec_data->m_spAPEInfo))
  CRC >>= 1;

 if(GET_USES_CRC(dec_data->m_spAPEInfo) == FALSE){
  unsigned __int32 nChecksum = CUnMAC_CalculateOldChecksum(core_data->m_pDataX,core_data->m_pDataY,CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_CHANNELS,0,0),nBlocks);
  if (nChecksum != nStoredCRC)
   return APEDEC_ERROR_INVALID_CHECKSUM;
 }else{
  if (CRC != nStoredCRC)
   return APEDEC_ERROR_INVALID_CHECKSUM;
 }

 unmac_data->m_LastDecodedFrameIndex = FrameIndex;
 return nBlocks;
}

static int CUnMAC_SeekToFrame(struct CAPEDecompressOld_data_s *dec_data,int FrameIndex)
{
 struct CUnMAC_data_s *unmac_data=dec_data->m_UnMAC;
 struct CAPEDecompressOldCore_data_s *core_data=unmac_data->core_datas;

 if(GET_FRAMES_START_ON_BYTES_BOUNDARIES(dec_data->m_spAPEInfo)){
  if((unmac_data->m_LastDecodedFrameIndex == -1) || ((FrameIndex - 1) != unmac_data->m_LastDecodedFrameIndex)){
   int SeekRemainder = (CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_SEEK_BYTE,FrameIndex,0) - CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_SEEK_BYTE, 0,0)) % 4;
   return core_data->unbita_funcs->FillAndResetBitArray(core_data->unbita_datas,CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_SEEK_BYTE,FrameIndex,0) - SeekRemainder, SeekRemainder * 8);
  }else{
   return core_data->unbita_funcs->AdvanceToByteBoundary(core_data->unbita_datas);
  }
 }else{
  if((unmac_data->m_LastDecodedFrameIndex == -1) || ((FrameIndex - 1) != unmac_data->m_LastDecodedFrameIndex)){
   int seekbytes=CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_SEEK_BYTE,FrameIndex,0);
   int seekbits=CAPEDecompressOld_GetInfo((void *)dec_data,APE_INFO_SEEK_BIT,FrameIndex,0);
   return core_data->unbita_funcs->FillAndResetBitArray(core_data->unbita_datas,seekbytes,seekbits);
  }
 }

 return core_data->unbita_funcs->FillAndResetBitArray(core_data->unbita_datas,-1,-1);
}

#endif // #ifdef BACKWARDS_COMPATIBILITY
