//**************************************************************************
//*                   This file is part of the                             *
//*           APE decoder of Mpxplay (http://mpxplay.cjb.net)              *
//*      based on the MAC SDK v3.97 (http://www.monkeysaudio.com)          *
//*              updated with the v3.99 (June 2004)                        *
//**************************************************************************
//*   This program is distributed in the hope that it will be useful,      *
//*   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
//*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                 *
//**************************************************************************

#include "All.h"
#include "APEInfo.h"
#include "unbitnew.h"

const unsigned __int32 K_SUM_MIN_BOUNDARY[32] = {
	0, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144,
	524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, 67108864, 134217728,
	268435456, 536870912, 1073741824, 2147483648, 2147483648, 2147483648, 2147483648, 2147483648
};

/*const unsigned __int32 K_SUM_MIN_BOUNDARY[32] = {
 0,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,
 524288,1048576,2097152,4194304,8388608,16777216,33554432,67108864,134217728,
 268435456,536870912,1073741824,2147483648,0,0,0,0};*/

static const __int32 RANGE_TOTAL_1[66] = {
	0, 14824, 28224, 39348, 47855, 53994, 58171, 60926, 62682, 63786, 64463, 64878, 65126,
	65276, 65365, 65419, 65450, 65469, 65480, 65487, 65491, 65493, 65494, 65495, 65496,
	65497, 65498, 65499, 65500, 65501, 65502, 65503, 65504, 65505, 65506, 65507, 65508,
	65509, 65510, 65511, 65512, 65513, 65514, 65515, 65516, 65517, 65518, 65519, 65520,
	65521, 65522, 65523, 65524, 65525, 65526, 65527, 65528, 65529, 65530, 65531, 65532,
	65533, 65534, 65535, 65536, 0x7fffffff
};
static const __int32 RANGE_WIDTH_1[65] = {
	14824, 13400, 11124, 8507, 6139, 4177, 2755, 1756, 1104, 677, 415, 248, 150, 89, 54, 31,
	19, 11, 7, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static const __int32 RANGE_TOTAL_2[66] = {
	0, 19578, 36160, 48417, 56323, 60899, 63265, 64435, 64971, 65232, 65351, 65416, 65447,
	65466, 65476, 65482, 65485, 65488, 65490, 65491, 65492, 65493, 65494, 65495, 65496,
	65497, 65498, 65499, 65500, 65501, 65502, 65503, 65504, 65505, 65506, 65507, 65508,
	65509, 65510, 65511, 65512, 65513, 65514, 65515, 65516, 65517, 65518, 65519, 65520,
	65521, 65522, 65523, 65524, 65525, 65526, 65527, 65528, 65529, 65530, 65531, 65532,
	65533, 65534, 65535, 65536, 0x7fffffff
};
static const __int32 RANGE_WIDTH_2[65] = {
	19578, 16582, 12257, 7906, 4576, 2366, 1170, 536, 261, 119, 65, 31, 19, 10, 6, 3, 3, 2, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1
};

#define RANGE_OVERFLOW_SHIFT			16

#define CODE_BITS 32
#define TOP_VALUE ((unsigned int ) 1 << (CODE_BITS - 1))
#define SHIFT_BITS (CODE_BITS - 9)
#define EXTRA_BITS ((CODE_BITS - 2) % 8 + 1)
#define BOTTOM_VALUE (TOP_VALUE >> 8)
#define MODEL_ELEMENTS 64


static void *CUnBitArray_init(struct IAPEDecompress_data_s *pAPEDecompress, int nVersion, struct CUnBitArray_func_s **unbita_funcs);
static void CUnBitArray_close(void *unbitarray_data);
static unsigned int DecodeValue(void *unbitarray_data, enum DECODE_VALUE_METHOD DecodeMethod, int nParam1, int nParam2);
static int DecodeValueRange(void *unbitarray_data, BIT_ARRAY_STATE * BitArrayState);
static void FlushState(BIT_ARRAY_STATE * BitArrayState);
static int FlushBitArray(void *unbitarray_data);
static void Finalize(void *unbitarray_data);
static int FillAndResetBitArray(void *unbitarray_data, int nFileLocation, int nNewBitIndex);
#ifdef BACKWARDS_COMPATIBILITY
static void GenerateArray(void *unbitarray_data, int *pOutputArray, int nElements, int nBytesRequired);
static int AdvanceToByteBoundary(void *unbitarray_data);
#endif

struct CUnBitArray_func_s CUnBitArray_funcs = {
	&CUnBitArray_init,
	&CUnBitArray_close,
#ifdef BACKWARDS_COMPATIBILITY	// version 3900-3929
	&GenerateArray,
#else
	NULL,
#endif
	&DecodeValue,
#ifdef BACKWARDS_COMPATIBILITY
	&AdvanceToByteBoundary,		//
#else
	NULL,
#endif
	&DecodeValueRange,
	&FlushState,
	&FlushBitArray,
	&Finalize,
	&FillAndResetBitArray
};

static void *CUnBitArray_init(struct IAPEDecompress_data_s *pAPEDecompress, int nVersion, struct CUnBitArray_func_s **unbita_funcs)
{
	struct CUnBitArray_data_s *bita_data;
	int nBitArrayBytes;

	bita_data = (struct CUnBitArray_data_s *)calloc(1, sizeof(struct CUnBitArray_data_s));
	if(!bita_data)
		return bita_data;

	if(nVersion >= 3930)
		nBitArrayBytes = 16384;
	else
		nBitArrayBytes = 262144;

	bita_data->unbitbas_datas = CUnBitArrayBase_funcs.unbitbas_init(pAPEDecompress, nBitArrayBytes, nVersion, &bita_data->unbitbas_funcs);
	if(!bita_data->unbitbas_datas) {
		CUnBitArray_close((void *)bita_data);
		return NULL;
	}

	*unbita_funcs = &CUnBitArray_funcs;

	return ((void *)bita_data);
}

static void CUnBitArray_close(void *unbitarray_data)
{
	struct CUnBitArray_data_s *bita_data = (struct CUnBitArray_data_s *)unbitarray_data;

	if(bita_data) {
		if(bita_data->unbitbas_funcs)
			bita_data->unbitbas_funcs->unbitbas_close(bita_data->unbitbas_datas);
		free(bita_data);
	}
}

static unsigned int DecodeValue(void *unbitarray_data, enum DECODE_VALUE_METHOD DecodeMethod, int nParam1, int nParam2)
{
	struct CUnBitArray_data_s *bita_data = (struct CUnBitArray_data_s *)unbitarray_data;

	switch (DecodeMethod) {
	case DECODE_VALUE_METHOD_UNSIGNED_INT:
		return bita_data->unbitbas_funcs->DecodeValueXBits(bita_data->unbitbas_datas, 32);
	}
	return 0;
}

static int RangeDecodeFast(struct CUnBitArray_data_s *bita_data, int nShift)
{
	struct CUnBitArrayBase_data_s *bitbas_data = bita_data->unbitbas_datas;

	if(!bita_data->m_RangeCoderInfo.range)	// to avoid endless cycle
		return 0;

	while(bita_data->m_RangeCoderInfo.range <= BOTTOM_VALUE) {
		if(bitbas_data->m_nCurrentBitIndex >= bitbas_data->stored_bits)	// overflow check
			return 0;
		bita_data->m_RangeCoderInfo.buffer =
			(bita_data->m_RangeCoderInfo.buffer << 8) | ((bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] >> (24 - (bitbas_data->m_nCurrentBitIndex & 31))) & 0xFF);
		bitbas_data->m_nCurrentBitIndex += 8;
		bita_data->m_RangeCoderInfo.low = (bita_data->m_RangeCoderInfo.low << 8) | ((bita_data->m_RangeCoderInfo.buffer >> 1) & 0xFF);
		bita_data->m_RangeCoderInfo.range <<= 8;
	}

	bita_data->m_RangeCoderInfo.range = bita_data->m_RangeCoderInfo.range >> nShift;

	if(!bita_data->m_RangeCoderInfo.range)	// to avoid div0 error
		return 0;

	return (bita_data->m_RangeCoderInfo.low / bita_data->m_RangeCoderInfo.range);
}

static int RangeDecodeFastWithUpdate(struct CUnBitArray_data_s *bita_data, int nShift)
{
	struct CUnBitArrayBase_data_s *bitbas_data = bita_data->unbitbas_datas;
	int nRetVal;

	if(!bita_data->m_RangeCoderInfo.range)	// to avoid endless cycle
		return 0;

	while(bita_data->m_RangeCoderInfo.range <= BOTTOM_VALUE) {
		if(bitbas_data->m_nCurrentBitIndex >= bitbas_data->stored_bits)	// overflow check
			return 0;
		bita_data->m_RangeCoderInfo.buffer =
			(bita_data->m_RangeCoderInfo.buffer << 8) | ((bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] >> (24 - (bitbas_data->m_nCurrentBitIndex & 31))) & 0xFF);
		bitbas_data->m_nCurrentBitIndex += 8;
		bita_data->m_RangeCoderInfo.low = (bita_data->m_RangeCoderInfo.low << 8) | ((bita_data->m_RangeCoderInfo.buffer >> 1) & 0xFF);
		bita_data->m_RangeCoderInfo.range <<= 8;
	}

	bita_data->m_RangeCoderInfo.range = bita_data->m_RangeCoderInfo.range >> nShift;

	if(!bita_data->m_RangeCoderInfo.range)	// to avoid div0 error
		return 0;

	nRetVal = bita_data->m_RangeCoderInfo.low / bita_data->m_RangeCoderInfo.range;
	bita_data->m_RangeCoderInfo.low -= bita_data->m_RangeCoderInfo.range * nRetVal;
	return nRetVal;
}

static int DecodeValueRange(void *unbitarray_data, BIT_ARRAY_STATE * BitArrayState)
{
	struct CUnBitArray_data_s *bita_data = (struct CUnBitArray_data_s *)unbitarray_data;
	struct CUnBitArrayBase_data_s *bitbas_data = bita_data->unbitbas_datas;
	int nRangeTotal, nOverflow, nTempK, nValue;

	if(bitbas_data->m_nCurrentBitIndex > bita_data->m_nRefillBitThreshold)
		bita_data->unbitbas_funcs->FillBitArray(bitbas_data);

	if(bitbas_data->m_nVersion >= 3990) {	// -----------------------------------

		int nPivotValue = max(BitArrayState->nKSum / 32, 1);
		int nOverflow, nRangeTotal, nBase, nShift;

		nRangeTotal = RangeDecodeFast(bita_data, RANGE_OVERFLOW_SHIFT);

		// lookup the symbol (must be a faster way than this)
		nOverflow = 0;
		while(nRangeTotal >= RANGE_TOTAL_2[nOverflow + 1])
			nOverflow++;

		// update
		bita_data->m_RangeCoderInfo.low -= bita_data->m_RangeCoderInfo.range * RANGE_TOTAL_2[nOverflow];
		bita_data->m_RangeCoderInfo.range = bita_data->m_RangeCoderInfo.range * RANGE_WIDTH_2[nOverflow];

		// get the working k
		if(nOverflow >= (MODEL_ELEMENTS - 1)) {	// modified from ==
			nOverflow = RangeDecodeFastWithUpdate(bita_data, 16);
			nOverflow <<= 16;
			nOverflow |= RangeDecodeFastWithUpdate(bita_data, 16);
		}
		// get the value
		nBase = 0;
		nShift = 0;
		if(nPivotValue >= (1 << 16)) {
			int nPivotValueBits = 0, nSplitFactor, nPivotValueA, nPivotValueB;
			int nBaseA, nBaseB;

			while((nPivotValue >> nPivotValueBits) > 0) {
				nPivotValueBits++;
			}

			nSplitFactor = 1 << (nPivotValueBits - 16);

			nPivotValueA = (nPivotValue / nSplitFactor) + 1;
			nPivotValueB = nSplitFactor;

			while(bita_data->m_RangeCoderInfo.range <= BOTTOM_VALUE) {
				bita_data->m_RangeCoderInfo.buffer =
					(bita_data->m_RangeCoderInfo.buffer << 8) | ((bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] >> (24 - (bitbas_data->m_nCurrentBitIndex & 31))) & 0xFF);
				bitbas_data->m_nCurrentBitIndex += 8;
				bita_data->m_RangeCoderInfo.low = (bita_data->m_RangeCoderInfo.low << 8) | ((bita_data->m_RangeCoderInfo.buffer >> 1) & 0xFF);
				bita_data->m_RangeCoderInfo.range <<= 8;
			}
			bita_data->m_RangeCoderInfo.range = bita_data->m_RangeCoderInfo.range / nPivotValueA;
			nBaseA = bita_data->m_RangeCoderInfo.low / bita_data->m_RangeCoderInfo.range;
			bita_data->m_RangeCoderInfo.low -= bita_data->m_RangeCoderInfo.range * nBaseA;

			while(bita_data->m_RangeCoderInfo.range <= BOTTOM_VALUE) {
				bita_data->m_RangeCoderInfo.buffer =
					(bita_data->m_RangeCoderInfo.buffer << 8) | ((bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] >> (24 - (bitbas_data->m_nCurrentBitIndex & 31))) & 0xFF);
				bitbas_data->m_nCurrentBitIndex += 8;
				bita_data->m_RangeCoderInfo.low = (bita_data->m_RangeCoderInfo.low << 8) | ((bita_data->m_RangeCoderInfo.buffer >> 1) & 0xFF);
				bita_data->m_RangeCoderInfo.range <<= 8;
			}
			bita_data->m_RangeCoderInfo.range = bita_data->m_RangeCoderInfo.range / nPivotValueB;
			nBaseB = bita_data->m_RangeCoderInfo.low / bita_data->m_RangeCoderInfo.range;
			bita_data->m_RangeCoderInfo.low -= bita_data->m_RangeCoderInfo.range * nBaseB;

			nBase = nBaseA * nSplitFactor + nBaseB;
		} else {
			int nBaseLower;
			while(bita_data->m_RangeCoderInfo.range <= BOTTOM_VALUE) {
				bita_data->m_RangeCoderInfo.buffer =
					(bita_data->m_RangeCoderInfo.buffer << 8) | ((bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] >> (24 - (bitbas_data->m_nCurrentBitIndex & 31))) & 0xFF);
				bitbas_data->m_nCurrentBitIndex += 8;
				bita_data->m_RangeCoderInfo.low = (bita_data->m_RangeCoderInfo.low << 8) | ((bita_data->m_RangeCoderInfo.buffer >> 1) & 0xFF);
				bita_data->m_RangeCoderInfo.range <<= 8;
			}

			// decode
			bita_data->m_RangeCoderInfo.range = bita_data->m_RangeCoderInfo.range / nPivotValue;
			nBaseLower = bita_data->m_RangeCoderInfo.low / bita_data->m_RangeCoderInfo.range;
			bita_data->m_RangeCoderInfo.low -= bita_data->m_RangeCoderInfo.range * nBaseLower;

			nBase = nBaseLower;
		}

		// build the value
		nValue = nBase + (nOverflow * nPivotValue);

	} else {					// up to 3980 -----------------------------------------------------------

		nRangeTotal = RangeDecodeFast(bita_data, RANGE_OVERFLOW_SHIFT);

		nOverflow = 0;
		while((nRangeTotal >= RANGE_TOTAL_1[nOverflow + 1]) && (nOverflow < 64))
			nOverflow++;

		bita_data->m_RangeCoderInfo.low -= bita_data->m_RangeCoderInfo.range * RANGE_TOTAL_1[nOverflow];
		bita_data->m_RangeCoderInfo.range = bita_data->m_RangeCoderInfo.range * RANGE_WIDTH_1[nOverflow];

		if(nOverflow >= (MODEL_ELEMENTS - 1)) {	// modified from ==
			nTempK = RangeDecodeFastWithUpdate(bita_data, 5);
			nOverflow = 0;
		} else {
			nTempK = (BitArrayState->k < 1) ? 0 : (BitArrayState->k - 1);
		}

		if((nTempK <= 16) || (bitbas_data->m_nVersion < 3910)) {
			nValue = RangeDecodeFastWithUpdate(bita_data, nTempK);
		} else {
			int nX1 = RangeDecodeFastWithUpdate(bita_data, 16);
			int nX2 = RangeDecodeFastWithUpdate(bita_data, nTempK - 16);
			nValue = nX1 | (nX2 << 16);
		}

		nValue += (nOverflow << nTempK);
	}							// ------------------------------------------------------------------

	BitArrayState->nKSum += ((nValue + 1) / 2) - ((BitArrayState->nKSum + 16) >> 5);

	if(BitArrayState->nKSum < K_SUM_MIN_BOUNDARY[BitArrayState->k]) {
		if(BitArrayState->k > 0)	// overflow check
			BitArrayState->k--;
	} else {
		if(BitArrayState->nKSum >= K_SUM_MIN_BOUNDARY[BitArrayState->k + 1])
			if(BitArrayState->k < 30)	// overflow check
				BitArrayState->k++;
	}

	return ((nValue & 1) ? ((nValue >> 1) + 1) : -(nValue >> 1));
}

static void FlushState(BIT_ARRAY_STATE * BitArrayState)
{
	BitArrayState->k = 10;
	BitArrayState->nKSum = (1 << BitArrayState->k) * 16;
}

static unsigned char GetC(struct CUnBitArrayBase_data_s *bitbas_data)
{
	unsigned char nValue = (unsigned char)(bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] >> (24 - (bitbas_data->m_nCurrentBitIndex & 31)));
	bitbas_data->m_nCurrentBitIndex += 8;
	return nValue;
}

static int FlushBitArray(void *unbitarray_data)
{
	struct CUnBitArray_data_s *bita_data = (struct CUnBitArray_data_s *)unbitarray_data;
	struct CUnBitArrayBase_data_s *bitbas_data = bita_data->unbitbas_datas;

	RETURN_ON_ERROR(bita_data->unbitbas_funcs->AdvanceToByteBoundary(bitbas_data))

		bitbas_data->m_nCurrentBitIndex += 8;
	bita_data->m_RangeCoderInfo.buffer = GetC(bitbas_data);
	bita_data->m_RangeCoderInfo.low = bita_data->m_RangeCoderInfo.buffer >> (8 - EXTRA_BITS);
	bita_data->m_RangeCoderInfo.range = (unsigned int)1 << EXTRA_BITS;
	bita_data->m_nRefillBitThreshold = (bitbas_data->required_bytes * 8 - 512);

	return APEDEC_ERROR_SUCCESS;
}

static void Finalize(void *unbitarray_data)
{
	struct CUnBitArray_data_s *bita_data = (struct CUnBitArray_data_s *)unbitarray_data;
	struct CUnBitArrayBase_data_s *bitbas_data = bita_data->unbitbas_datas;

	if(bita_data->m_RangeCoderInfo.range) {	// to avoid endless cycle
		while(bita_data->m_RangeCoderInfo.range <= BOTTOM_VALUE) {
			if(bitbas_data->m_nCurrentBitIndex >= bitbas_data->stored_bits)	// overflow check
				break;
			bitbas_data->m_nCurrentBitIndex += 8;
			bita_data->m_RangeCoderInfo.range <<= 8;
		}
	}

	if(bitbas_data->m_nVersion <= 3950)
		bitbas_data->m_nCurrentBitIndex -= 16;
}

static int FillAndResetBitArray(void *unbitarray_data, int nFileLocation, int nNewBitIndex)
{
	struct CUnBitArray_data_s *bita_data = (struct CUnBitArray_data_s *)unbitarray_data;
	return bita_data->unbitbas_funcs->FillAndResetBitArray(bita_data->unbitbas_datas, nFileLocation, nNewBitIndex);
}

#ifdef BACKWARDS_COMPATIBILITY

static void GenerateArrayRange(void *unbitarray_data, int *pOutputArray, int nElements)
{
	int z;
	BIT_ARRAY_STATE BitArrayState;

	FlushState(&BitArrayState);
	FlushBitArray(unbitarray_data);

	for(z = 0; z < nElements; z++)
		pOutputArray[z] = DecodeValueRange(unbitarray_data, &BitArrayState);

	Finalize(unbitarray_data);
}

static void GenerateArray(void *unbitarray_data, int *pOutputArray, int nElements, int nBytesRequired)
{
	GenerateArrayRange(unbitarray_data, pOutputArray, nElements);
}

static int AdvanceToByteBoundary(void *unbitarray_data)
{
	struct CUnBitArray_data_s *bita_data = (struct CUnBitArray_data_s *)unbitarray_data;
	return bita_data->unbitbas_funcs->AdvanceToByteBoundary(bita_data->unbitbas_datas);
}

#endif
