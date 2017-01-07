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
#include "MACLib.h"
#include "dec_new.h"
#include "APEInfo.h"
#ifdef BACKWARDS_COMPATIBILITY
#include "dec_old.h"
#endif

struct IAPEDecompress_data_s *IAPEDecompress_check(struct mpxplay_filehand_buffered_func_s *fileio_funcs,void *fileio_datas,char *pFilename)
{
 struct IAPEDecompress_data_s *pAPEDecompress;

 pAPEDecompress=(IAPEDecompress_data_s *)calloc(1,sizeof(struct IAPEDecompress_data_s));
 if(!pAPEDecompress)
  return pAPEDecompress;

 pAPEDecompress->fileio_funcs=fileio_funcs;
 pAPEDecompress->fileio_datas=fileio_datas;

 pAPEDecompress->apeinfo_datas=CAPEInfo_funcs.check(pAPEDecompress,pFilename);
 if(!pAPEDecompress->apeinfo_datas){
  IAPEDecompress_close(pAPEDecompress);
  return NULL;
 }

 return pAPEDecompress;
}

struct IAPEDecompress_data_s *IAPEDecompress_open(struct mpxplay_filehand_buffered_func_s *fileio_funcs,void *fileio_datas,char *pFilename)
{
 struct IAPEDecompress_data_s *pAPEDecompress;

 pAPEDecompress=(IAPEDecompress_data_s *)calloc(1,sizeof(struct IAPEDecompress_data_s));
 if(!pAPEDecompress)
  return pAPEDecompress;

 pAPEDecompress->fileio_funcs=fileio_funcs;
 pAPEDecompress->fileio_datas=fileio_datas;

 pAPEDecompress->apeinfo_datas=CAPEInfo_funcs.open(pAPEDecompress,pFilename);
 if(!pAPEDecompress->apeinfo_datas){
  IAPEDecompress_close(pAPEDecompress);
  return NULL;
 }

 if(pAPEDecompress->apeinfo_datas->apeinfo_funcs->GetInfo(pAPEDecompress->apeinfo_datas,APE_INFO_FILE_VERSION,0,0) >= 3930)
  CAPEDecompressNew_funcs.CAPEDecompress_open(pAPEDecompress);
#ifdef BACKWARDS_COMPATIBILITY
 else
  CAPEDecompressOld_funcs.CAPEDecompress_open(pAPEDecompress);
#endif

 if(!pAPEDecompress->decoder_datas || !pAPEDecompress->decoder_funcs){
  IAPEDecompress_close(pAPEDecompress);
  pAPEDecompress=NULL;
 }

 return pAPEDecompress;
}

void IAPEDecompress_close(struct IAPEDecompress_data_s *apedec_datas)
{
 if(apedec_datas){
  if(apedec_datas->apeinfo_datas){
   if(apedec_datas->apeinfo_datas->apeinfo_funcs->GetInfo(apedec_datas->apeinfo_datas,APE_INFO_FILE_VERSION,0,0) >= 3930)
    CAPEDecompressNew_funcs.CAPEDecompress_close(apedec_datas->decoder_datas);
#ifdef BACKWARDS_COMPATIBILITY
   else
    CAPEDecompressOld_funcs.CAPEDecompress_close(apedec_datas->decoder_datas);
#endif
   CAPEInfo_funcs.close(apedec_datas->apeinfo_datas);
  }
  free(apedec_datas);
 }
}

int IAPEDecompress_GetData(struct IAPEDecompress_data_s *apedec_datas,char *pBuffer,int nBlocks,int *pBlocksRetrieved)
{
 return apedec_datas->decoder_funcs->GetData(apedec_datas->decoder_datas,pBuffer,nBlocks,pBlocksRetrieved);
}

int IAPEDecompress_Seek(struct IAPEDecompress_data_s *apedec_datas,int nBlockOffset)
{
 return apedec_datas->decoder_funcs->Seek(apedec_datas->decoder_datas,nBlockOffset);
}


int IAPEDecompress_GetInfo(struct IAPEDecompress_data_s *apedec_datas,enum APE_DECOMPRESS_FIELDS Field,int nParam1,int nParam2)
{
 return apedec_datas->decoder_funcs->GetInfo(apedec_datas->decoder_datas,Field,nParam1,nParam2);
}
