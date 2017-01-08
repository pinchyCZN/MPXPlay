#ifndef	APE_UNBITOLD_H
#define APE_UNBITOLD_H

#include "All.h"

#ifdef BACKWARDS_COMPATIBILITY

#include "unbitbas.h"

typedef struct CUnBitArrayOld_data_s {
	unsigned __int32 k;
	unsigned __int32 K_Sum;
	unsigned __int32 m_nRefillBitThreshold;
	struct CUnBitArrayBase_func_s *unbitbas_funcs;
	struct CUnBitArrayBase_data_s *unbitbas_datas;
} CUnBitArrayOld_data_s;

extern struct CUnBitArray_func_s CUnBitArrayOld_funcs;

#endif							// BACKWARDS_COMPATIBILITY

#endif							/* APE_UNBITOLD_H */
