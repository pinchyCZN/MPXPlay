#ifndef apedec_dec_new_h
#define apedec_dec_new_h

#include "all.h"
#include "unbitbas.h"
#include "MACLib.h"
#include "Prepare.h"
#include "pred_new.h"

typedef struct CAPEDecompressNew_data_s{
 int m_nBlockAlign;
 int m_nCurrentFrame;
 int m_nCurrentBlock;
 int m_nFinishBlock;
 int m_nBlocksProcessed;
 unsigned int m_nCRC;
 unsigned int m_nStoredCRC;
 int m_nSpecialCodes;
 BOOL m_bCurrentFrameCorrupt;

 struct CAPEInfo_data_s *m_spAPEInfo;
 struct CUnBitArray_func_s *unbita_funcs;
 void                      *unbita_datas;
 struct BIT_ARRAY_STATE *m_BitArrayStateX;
 struct BIT_ARRAY_STATE *m_BitArrayStateY;

 struct IPredictorDecompress_func_s *predictor_funcs_X;
 void *predictor_datas_X;
 struct IPredictorDecompress_func_s *predictor_funcs_Y;
 void *predictor_datas_Y;

 int m_nLastX;
 struct tWAVEFORMATEX m_wfeInput;
}CAPEDecompress_new_data_s;

extern struct CAPEDecompress_func_s CAPEDecompressNew_funcs;

#endif // #ifndef apedec_dec_new_h
