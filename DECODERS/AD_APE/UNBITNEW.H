#ifndef APE_UNBITARRAY_H
#define APE_UNBITARRAY_H

#include "unbitbas.h"

typedef struct RANGE_CODER_STRUCT_DECOMPRESS {
	unsigned int range;			//length of interval
	unsigned int low;			//low end of interval
	unsigned int buffer;		//buffer for input/output
} RANGE_CODER_STRUCT_DECOMPRESS;

typedef struct CUnBitArray_data_s {
	struct RANGE_CODER_STRUCT_DECOMPRESS m_RangeCoderInfo;
	unsigned __int32 m_nRefillBitThreshold;
	struct CUnBitArrayBase_func_s *unbitbas_funcs;
	struct CUnBitArrayBase_data_s *unbitbas_datas;
} CUnBitArray_data_s;

extern struct CUnBitArray_func_s CUnBitArray_funcs;

#endif							// #ifndef APE_UNBITARRAY_H
