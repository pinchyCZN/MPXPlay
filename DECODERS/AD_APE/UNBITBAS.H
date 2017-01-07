#ifndef APE_UNBITARRAYBASE_H
#define APE_UNBITARRAYBASE_H

#include "maclib.h"

typedef struct BIT_ARRAY_STATE
{
 unsigned __int32	k;
 unsigned __int32	nKSum;
}BIT_ARRAY_STATE;

enum DECODE_VALUE_METHOD
{
 DECODE_VALUE_METHOD_UNSIGNED_INT,
 DECODE_VALUE_METHOD_UNSIGNED_RICE,
 DECODE_VALUE_METHOD_X_BITS
};

typedef struct CUnBitArrayBase_data_s{
 unsigned __int32 m_nCurrentBitIndex;
 unsigned __int32 *m_pBitArray;
 unsigned __int32 required_bytes;
 unsigned __int32 stored_bytes;
 unsigned __int32 stored_elements;
 unsigned __int32 stored_bits;
 unsigned __int32 filepos_of_stored_data;
 int	          m_nVersion;
 struct IAPEDecompress_data_s *apedec_datas;
}CUnBitArrayBase_data_s;

typedef struct CUnBitArray_func_s{
 void *(*CUnBitArray_init)(struct IAPEDecompress_data_s *pAPEDecompress,int nVersion,struct CUnBitArray_func_s **unbita_funcs);
 void  (*CUnBitArray_close)(void *unbita_data);
 void  (*GenerateArray)(void *unbita_data,int *pOutputArray,int nElements,int nBytesRequired);
 unsigned int (*DecodeValue)(void *unbita_data,enum DECODE_VALUE_METHOD DecodeMethod,int nParam1,int nParam2);
 int   (*AdvanceToByteBoundary)(void *unbita_data);
 int   (*DecodeValueRange)(void *unbita_data,struct BIT_ARRAY_STATE *BitArrayState);
 void  (*FlushState)(struct BIT_ARRAY_STATE *BitArrayState);
 int   (*FlushBitArray)(void *unbita_data);
 void  (*Finalize)(void *unbita_data);
 int   (*FillAndResetBitArray)(void *unbita_data,int nFileLocation, int nNewBitIndex);
}CUnBitArray_func_s;

typedef struct CUnBitArrayBase_func_s{
 void *(*CreateUnBitArray)(struct IAPEDecompress_data_s *pAPEDecompress,int nVersion,struct CUnBitArray_func_s **unbita_funcs);
 struct CUnBitArrayBase_data_s *(*unbitbas_init)(struct IAPEDecompress_data_s *pAPEDecompress,int nBytes,int nVersion,struct CUnBitArrayBase_func_s **);
 void (*unbitbas_close)(struct CUnBitArrayBase_data_s *unbitbas_datas);
 int  (*AdvanceToByteBoundary)(struct CUnBitArrayBase_data_s *unbitbas_datas);
 unsigned __int32 (*DecodeValueXBits)(struct CUnBitArrayBase_data_s *unbitbas_datas,unsigned __int32 nBits);
 int (*FillAndResetBitArray)(struct CUnBitArrayBase_data_s *unbitbas_datas,int nFileLocation, int nNewBitIndex);
 int (*FillBitArray)(struct CUnBitArrayBase_data_s *unbitbas_datas);
}CUnBitArrayBase_func_s;

extern struct CUnBitArrayBase_func_s CUnBitArrayBase_funcs;

#endif // #ifndef APE_UNBITARRAYBASE_H
