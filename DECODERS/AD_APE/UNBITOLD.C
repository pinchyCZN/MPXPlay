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
#include "unbitold.h"

static const unsigned __int32 K_SUM_MIN_BOUNDARY_OLD[32] = {0,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,2097152,4194304,8388608,16777216,33554432,67108864,134217728,268435456,536870912,1073741824,2147483648,0,0,0,0,0,0};
static const unsigned __int32 K_SUM_MAX_BOUNDARY_OLD[32] = {128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,2097152,4194304,8388608,16777216,33554432,67108864,134217728,268435456,536870912,1073741824,2147483648,0,0,0,0,0,0,0};
static const unsigned __int32 Powers_of_Two[32] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,2097152,4194304,8388608,16777216,33554432,67108864,134217728,268435456,536870912,1073741824,2147483648};
static const unsigned __int32 Powers_of_Two_Reversed[32] = {2147483648,1073741824,536870912,268435456,134217728,67108864,33554432,16777216,8388608,4194304,2097152,1048576,524288,262144,131072,65536,32768,16384,8192,4096,2048,1024,512,256,128,64,32,16,8,4,2,1};
static const unsigned __int32 Powers_of_Two_Minus_One_Reversed[33] = {4294967295,2147483647,1073741823,536870911,268435455,134217727,67108863,33554431,16777215,8388607,4194303,2097151,1048575,524287,262143,131071,65535,32767,16383,8191,4095,2047,1023,511,255,127,63,31,15,7,3,1,0};
static const unsigned __int32 K_SUM_MAX_BOUNDARY[32] = {32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,2097152,4194304,8388608,16777216,33554432,67108864,134217728,268435456,536870912,1073741824,2147483648,0,0,0,0,0};

extern const unsigned __int32 K_SUM_MIN_BOUNDARY[32] ;//= {0,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,2097152,4194304,8388608,16777216,33554432,67108864,134217728,268435456,536870912,1073741824,2147483648,0,0,0,0};

static void *CUnBitArrayOld_init(struct IAPEDecompress_data_s *pAPEDecompress,int nVersion,struct CUnBitArray_func_s **unbita_funcs);
static void CUnBitArrayOld_close(void *unbitarray_data);
static unsigned int CUnBitArrayOld_DecodeValue(void *unbitarray_data,enum DECODE_VALUE_METHOD DecodeMethod,int nParam1,int nParam2);
static void CUnBitArrayOld_GenerateArray(void *unbitarray_data,int *pOutputArray,int nElements,int nBytesRequired);
static int CUnBitArrayOld_AdvanceToByteBoundary(void *unbitarray_data);
static int CUnBitArrayOld_FillAndResetBitArray(void *unbitarray_data,int nFileLocation,int nNewBitIndex);

struct CUnBitArray_func_s CUnBitArrayOld_funcs={
 &CUnBitArrayOld_init,
 &CUnBitArrayOld_close,
 &CUnBitArrayOld_GenerateArray,
 &CUnBitArrayOld_DecodeValue,
 &CUnBitArrayOld_AdvanceToByteBoundary,
 NULL,
 NULL,
 NULL,
 NULL,
 CUnBitArrayOld_FillAndResetBitArray
};

static void *CUnBitArrayOld_init(struct IAPEDecompress_data_s *pAPEDecompress,int nVersion,struct CUnBitArray_func_s **unbita_funcs)
{
 struct CUnBitArrayOld_data_s *bita_data;
 int nBitArrayBytes;

 bita_data=(struct CUnBitArrayOld_data_s *)calloc(1,sizeof(struct CUnBitArrayOld_data_s));
 if(!bita_data)
  return bita_data;

 nBitArrayBytes=262144; // max?

 bita_data->unbitbas_datas=CUnBitArrayBase_funcs.unbitbas_init(pAPEDecompress,nBitArrayBytes,nVersion,&bita_data->unbitbas_funcs);
 if(!bita_data->unbitbas_datas)
  goto err_out;

 if(bita_data->unbitbas_datas->m_nVersion <= 3880)
  bita_data->m_nRefillBitThreshold = (nBitArrayBytes*8 - (16384 * 8));
 else
  bita_data->m_nRefillBitThreshold = (nBitArrayBytes*8 - (1024*8)); // was 64*8

 *unbita_funcs=&CUnBitArrayOld_funcs;

 return ((void *)bita_data);

err_out:
 CUnBitArrayOld_close((void *)bita_data);
 return NULL;
}

static void CUnBitArrayOld_close(void *unbitarray_data)
{
 struct CUnBitArrayOld_data_s *bita_data=(struct CUnBitArrayOld_data_s *)unbitarray_data;

 if(bita_data){
  if(bita_data->unbitbas_funcs)
   bita_data->unbitbas_funcs->unbitbas_close(bita_data->unbitbas_datas);
  free(bita_data);
 }
}

static unsigned __int32 GetBitsRemaining(struct CUnBitArrayBase_data_s *bitbas_data)
{
 return (bitbas_data->stored_bits - bitbas_data->m_nCurrentBitIndex);
}

static unsigned __int32 DecodeValueRiceUnsigned(struct CUnBitArrayOld_data_s *bita_data,unsigned __int32 k)
{
 struct CUnBitArrayBase_data_s *bitbas_data=bita_data->unbitbas_datas;
 unsigned __int32 v,BitInitial = bitbas_data->m_nCurrentBitIndex;

 while (!(bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] & Powers_of_Two_Reversed[(bitbas_data->m_nCurrentBitIndex++) & 31])) {}

 if(k==0)
  return (bitbas_data->m_nCurrentBitIndex - BitInitial - 1);

 v = (bitbas_data->m_nCurrentBitIndex - BitInitial - 1) << k;

 return (v | bita_data->unbitbas_funcs->DecodeValueXBits(bitbas_data,k));
}

static unsigned __int32 Get_K(unsigned __int32 x)
{
 unsigned __int32 k = 0;
 if(x == 0)
  return k;

 while(x >= Powers_of_Two[++k]) {}

 return k;
}

static unsigned int CUnBitArrayOld_DecodeValue(void *unbitarray_data,enum DECODE_VALUE_METHOD DecodeMethod,int nParam1,int nParam2)
{
 struct CUnBitArrayOld_data_s *bita_data=(struct CUnBitArrayOld_data_s *)unbitarray_data;
 switch(DecodeMethod){
  case DECODE_VALUE_METHOD_UNSIGNED_INT :return bita_data->unbitbas_funcs->DecodeValueXBits(bita_data->unbitbas_datas,32);
  case DECODE_VALUE_METHOD_UNSIGNED_RICE:return DecodeValueRiceUnsigned(bita_data,nParam1);
  case DECODE_VALUE_METHOD_X_BITS       :return bita_data->unbitbas_funcs->DecodeValueXBits(bita_data->unbitbas_datas,nParam1);
 }
 return 0;
}

static void GenerateArrayOld(struct CUnBitArrayOld_data_s *bita_data,int *Output_Array,unsigned __int32 Number_of_Elements,int Minimum_nCurrentBitIndex_Array_Bytes)
{
 struct CUnBitArrayBase_data_s *bitbas_data=bita_data->unbitbas_datas;
 unsigned __int32 K_Sum,q,kmin,kmax,k,Max;
 unsigned __int32 Max_Bits_Needed = Number_of_Elements * 50;
 unsigned __int32 v, Bit_Array_Index;
 int *p1, *p2;

 if(Minimum_nCurrentBitIndex_Array_Bytes > 0)
  Max_Bits_Needed = ((Minimum_nCurrentBitIndex_Array_Bytes + 4) * 8);

 if(Max_Bits_Needed > GetBitsRemaining(bitbas_data))
  bita_data->unbitbas_funcs->FillBitArray(bitbas_data);

 Max = (Number_of_Elements < 5) ? Number_of_Elements : 5;

 for (q = 0; q < Max; q++)
  Output_Array[q] = DecodeValueRiceUnsigned(bita_data,10);

 if (Number_of_Elements <= 5){
  for (p2 = &Output_Array[0]; p2 < &Output_Array[Number_of_Elements]; p2++)
   *p2 = (*p2 & 1) ? (*p2 >> 1) + 1 : -(*p2 >> 1);
  return;
 }

 K_Sum = Output_Array[0] + Output_Array[1] + Output_Array[2] + Output_Array[3] + Output_Array[4];
 k = Get_K(K_Sum / 10);

 Max = (Number_of_Elements < 64) ? Number_of_Elements : 64;
 for (q = 5; q < Max; q++){
  Output_Array[q] = DecodeValueRiceUnsigned(bita_data,k);
  K_Sum += Output_Array[q];
  k = Get_K(K_Sum / (q  + 1) / 2);
 }

 if (Number_of_Elements <= 64){
  for (p2 = &Output_Array[0]; p2 < &Output_Array[Number_of_Elements]; p2++)
   *p2 = (*p2 & 1) ? (*p2 >> 1) + 1 : -(*p2 >> 1);
  return;
 }

 k = Get_K(K_Sum >> 7);
 kmin = K_SUM_MIN_BOUNDARY_OLD[k];
 kmax = K_SUM_MAX_BOUNDARY_OLD[k];
 p1 = &Output_Array[64];
 p2 = &Output_Array[0];

 for (p1 = &Output_Array[64], p2 = &Output_Array[0]; p1 < &Output_Array[Number_of_Elements]; p1++, p2++){
  unsigned __int32 Bit_Initial = bitbas_data->m_nCurrentBitIndex;
  unsigned int Bit_Index,Left_Value;
  int Left_Extra_Bits;

  while (!(bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] & Powers_of_Two_Reversed[bitbas_data->m_nCurrentBitIndex++ & 31])) {}

  if (k == 0){
   v = (bitbas_data->m_nCurrentBitIndex - Bit_Initial - 1);
  }else{
   v = (bitbas_data->m_nCurrentBitIndex - Bit_Initial - 1) << k;

   Bit_Array_Index = bitbas_data->m_nCurrentBitIndex >> 5;
   Bit_Index = bitbas_data->m_nCurrentBitIndex & 31;
   bitbas_data->m_nCurrentBitIndex += k;

   Left_Extra_Bits = (32 - k) - Bit_Index;
   Left_Value = bitbas_data->m_pBitArray[Bit_Array_Index] & Powers_of_Two_Minus_One_Reversed[Bit_Index];

   if (Left_Extra_Bits >= 0)
    v |= (Left_Value >> Left_Extra_Bits);
   else
    v |= (Left_Value << -Left_Extra_Bits) | (bitbas_data->m_pBitArray[Bit_Array_Index + 1] >> (32 + Left_Extra_Bits));
  }

  *p1 = v;
  K_Sum += *p1 - *p2;

  *p2 = (*p2 % 2) ? (*p2 >> 1) + 1 : -(*p2 >> 1);

  if ((K_Sum < kmin) || (K_Sum >= kmax)){
   if (K_Sum < kmin)
    while (K_Sum < K_SUM_MIN_BOUNDARY_OLD[--k]) {}
   else
    while (K_Sum >= K_SUM_MAX_BOUNDARY_OLD[++k]) {}

   kmax = K_SUM_MAX_BOUNDARY_OLD[k];
   kmin = K_SUM_MIN_BOUNDARY_OLD[k];
  }
 }

 for (; p2 < &Output_Array[Number_of_Elements]; p2++)
  *p2 = (*p2 & 1) ? (*p2 >> 1) + 1 : -(*p2 >> 1);
}

static int DecodeValueNew(struct CUnBitArrayOld_data_s *bita_data,BOOL bCapOverflow)
{
 struct CUnBitArrayBase_data_s *bitbas_data=bita_data->unbitbas_datas;
 unsigned __int32 Bit_Initial;
 unsigned int v;
 int nOverflow;

 if(bitbas_data->m_nCurrentBitIndex > bita_data->m_nRefillBitThreshold)
  bita_data->unbitbas_funcs->FillBitArray(bitbas_data);

 Bit_Initial = bitbas_data->m_nCurrentBitIndex;
 while(!(bitbas_data->m_pBitArray[bitbas_data->m_nCurrentBitIndex >> 5] & Powers_of_Two_Reversed[bitbas_data->m_nCurrentBitIndex++ & 31])) {}

 nOverflow = (bitbas_data->m_nCurrentBitIndex - Bit_Initial - 1);

 if(bCapOverflow){
  while (nOverflow >= 16){
   bita_data->k += 4;
   nOverflow -= 16;
  }
 }

 if (bita_data->k != 0){
  unsigned int Bit_Array_Index = bitbas_data->m_nCurrentBitIndex >> 5;
  unsigned int Bit_Index = bitbas_data->m_nCurrentBitIndex & 31;
  unsigned int Left_Value;
  int Left_Extra_Bits;

  bitbas_data->m_nCurrentBitIndex += bita_data->k;

  Left_Extra_Bits = (32 - bita_data->k) - Bit_Index;
  Left_Value = bitbas_data->m_pBitArray[Bit_Array_Index] & Powers_of_Two_Minus_One_Reversed[Bit_Index];

  v = nOverflow << bita_data->k;
  if (Left_Extra_Bits >= 0){
   v |= (Left_Value >> Left_Extra_Bits);
  }else{
   v |= (Left_Value << -Left_Extra_Bits) | (bitbas_data->m_pBitArray[Bit_Array_Index + 1] >> (32 + Left_Extra_Bits));
  }
 }else{
  v = nOverflow;
 }

 bita_data->K_Sum += v - ((bita_data->K_Sum + 8) >> 4);

 if(bita_data->K_Sum < K_SUM_MIN_BOUNDARY[bita_data->k]){
  bita_data->k--;
 }else
  if(bita_data->K_Sum >= K_SUM_MAX_BOUNDARY[bita_data->k]){
   bita_data->k++;
  }

 return ((v&1)? (v>>1)+1 : -((int)(v>>1)));
}

static void GenerateArrayRice(struct CUnBitArrayOld_data_s *bita_data,int *Output_Array,unsigned __int32 Number_of_Elements,int Minimum_nCurrentBitIndex_Array_Bytes)
{
 bita_data->k = 10;
 bita_data->K_Sum = 1024 * 16;

 if(bita_data->unbitbas_datas->m_nVersion <= 3880){
  int *p1;
  for (p1 = &Output_Array[0]; p1 < &Output_Array[Number_of_Elements]; p1++)
   *p1 = DecodeValueNew(bita_data,FALSE);
 }else{
  int *p1;
  for (p1 = &Output_Array[0]; p1 < &Output_Array[Number_of_Elements]; p1++)
   *p1 = DecodeValueNew(bita_data,TRUE);
 }
}

static void CUnBitArrayOld_GenerateArray(void *unbitarray_data,int *pOutputArray,int nElements,int nBytesRequired)
{
 struct CUnBitArrayOld_data_s *bita_data=(struct CUnBitArrayOld_data_s *)unbitarray_data;

 if(bita_data->unbitbas_datas->m_nVersion < 3860)
  GenerateArrayOld(bita_data,pOutputArray, nElements, nBytesRequired);
 else
  GenerateArrayRice(bita_data,pOutputArray, nElements, nBytesRequired);
}

static int CUnBitArrayOld_AdvanceToByteBoundary(void *unbitarray_data)
{
 struct CUnBitArrayOld_data_s *bita_data=(struct CUnBitArrayOld_data_s *)unbitarray_data;
 return bita_data->unbitbas_funcs->AdvanceToByteBoundary(bita_data->unbitbas_datas);
}

static int CUnBitArrayOld_FillAndResetBitArray(void *unbitarray_data,int nFileLocation,int nNewBitIndex)
{
 struct CUnBitArrayOld_data_s *bita_data=(struct CUnBitArrayOld_data_s *)unbitarray_data;
 return bita_data->unbitbas_funcs->FillAndResetBitArray(bita_data->unbitbas_datas,nFileLocation,nNewBitIndex);
}

#endif // #ifdef BACKWARDS_COMPATIBILITY
