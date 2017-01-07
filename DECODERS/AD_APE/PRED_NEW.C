//**************************************************************************
//*                   This file is part of the                             *
//*           APE decoder of Mpxplay (http://mpxplay.cjb.net)              *
//*      based on the MAC SDK v3.99 (http://www.monkeysaudio.com)          *
//*                  updated from the v3.99 (June 2004)                    *
//**************************************************************************
//*   This program is distributed in the hope that it will be useful,      *
//*   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
//*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                 *
//**************************************************************************

#include "All.h"
#include "maclib.h"
#include "pred_new.h"

static void CNNFilter_close(struct CNNFilter_data_s *cnn);
static void CNNFilter_Flush(struct CNNFilter_data_s *cnn);
static void CNNFilter_AdaptNoMMX(short *pM, short *pAdapt,int nDirection,int nOrder);
static int CNNFilter_CalculateDotProductNoMMX(short * pA, short * pB, int nOrder);

#ifdef APEDEC_USE_MMX
       int apedec_GetMMXAvailable(void);
static void CNNFilter_AdaptMMX(short *pM, short *pAdapt, int nDirection, int nOrder);
static int CNNFilter_CalculateDotProductMMX(short * pA, short * pB, int nOrder);
#endif

static CNNFilter_data_s *CNNFilter_init(int nOrder, int nShift, int nVersion)
{
 struct CNNFilter_data_s *cnn;

 cnn=(struct CNNFilter_data_s *)calloc(1,sizeof(CNNFilter_data_s));
 if(!cnn)
  return cnn;

 cnn->m_nOrder = nOrder;
 cnn->m_nShift = nShift;
 cnn->m_nVersion = nVersion;

#ifdef APEDEC_USE_MMX
 cnn->m_bMMXAvailable = apedec_GetMMXAvailable();
#endif

 cnn->m_rb_size=NN_WINDOW_ELEMENTS+cnn->m_nOrder;
 cnn->m_rbInput =(short *)malloc(cnn->m_rb_size*sizeof(short));
 cnn->m_rbDeltaM=(short *)malloc(cnn->m_rb_size*sizeof(short));
 cnn->m_paryM   =(short *)malloc(cnn->m_nOrder*sizeof(short));
 if(!cnn->m_rbInput || !cnn->m_rbDeltaM || !cnn->m_paryM){
  CNNFilter_close(cnn);
  return NULL;
 }
 CNNFilter_Flush(cnn);
 return cnn;
}

static void CNNFilter_close(struct CNNFilter_data_s *cnn)
{
 if(cnn){
  if(cnn->m_rbInput)
   free(cnn->m_rbInput);
  if(cnn->m_rbDeltaM)
   free(cnn->m_rbDeltaM);
  if(cnn->m_paryM)
   free(cnn->m_paryM);
  free(cnn);
 }
}

static void CNNFilter_Flush(struct CNNFilter_data_s *cnn)
{
 memset(cnn->m_paryM,   0, cnn->m_nOrder  * sizeof(short));
 ROLLBUFF_FLUSH(cnn->m_rbInput ,cnn->m_rbp_Input ,cnn->m_nOrder,sizeof(short));
 ROLLBUFF_FLUSH(cnn->m_rbDeltaM,cnn->m_rbp_DeltaM,cnn->m_nOrder,sizeof(short));
 cnn->m_nRunningAverage=0;
}

static int CNNFilter_Decompress(struct CNNFilter_data_s *cnn,int nInput)
{
 int nDotProduct,nOutput;

#ifdef APEDEC_USE_MMX
 if(cnn->m_bMMXAvailable)
  nDotProduct = CNNFilter_CalculateDotProductMMX(&(cnn->m_rbp_Input[-cnn->m_nOrder]), cnn->m_paryM, cnn->m_nOrder);
 else
#endif
  nDotProduct = CNNFilter_CalculateDotProductNoMMX(&(cnn->m_rbp_Input[-cnn->m_nOrder]), cnn->m_paryM, cnn->m_nOrder);

#ifdef APEDEC_USE_MMX
 if(cnn->m_bMMXAvailable)
  CNNFilter_AdaptMMX(cnn->m_paryM, &(cnn->m_rbp_DeltaM[-cnn->m_nOrder]), -nInput, cnn->m_nOrder);
 else
#endif
  CNNFilter_AdaptNoMMX(cnn->m_paryM, &(cnn->m_rbp_DeltaM[-cnn->m_nOrder]), nInput, cnn->m_nOrder);

 nOutput = nInput + ((nDotProduct + (1 << (cnn->m_nShift - 1))) >> cnn->m_nShift);

 cnn->m_rbp_Input[0] = (short)((nOutput == (short)nOutput) ? nOutput : ((nOutput >> 31) ^ 0x7FFF));

 if(cnn->m_nVersion >= 3980){
  int nTempABS = abs(nOutput);

  if(nTempABS > (cnn->m_nRunningAverage * 3))
   cnn->m_rbp_DeltaM[0] = ((nOutput >> 25) & 64) - 32;
  else
   if(nTempABS > (cnn->m_nRunningAverage * 4) / 3)
    cnn->m_rbp_DeltaM[0] = ((nOutput >> 26) & 32) - 16;
   else
    if (nTempABS > 0)
     cnn->m_rbp_DeltaM[0] = ((nOutput >> 27) & 16) - 8;
    else
     cnn->m_rbp_DeltaM[0] = 0;

  cnn->m_nRunningAverage += (nTempABS - cnn->m_nRunningAverage) / 16;

  cnn->m_rbp_DeltaM[-1] >>= 1;
  cnn->m_rbp_DeltaM[-2] >>= 1;
  cnn->m_rbp_DeltaM[-8] >>= 1;
 }else{
  cnn->m_rbp_DeltaM[ 0] = (nOutput == 0)? 0: (((nOutput >> 28) & 8) - 4);
  cnn->m_rbp_DeltaM[-4] >>= 1;
  cnn->m_rbp_DeltaM[-8] >>= 1;
 }

 cnn->m_rbp_Input++;
 if(cnn->m_rbp_Input >= &cnn->m_rbInput[cnn->m_rb_size])
  ROLLBUFF_ROLL(cnn->m_rbInput ,cnn->m_rbp_Input ,cnn->m_nOrder,sizeof(short));

 cnn->m_rbp_DeltaM++;
 if(cnn->m_rbp_DeltaM >= &cnn->m_rbDeltaM[cnn->m_rb_size])
  ROLLBUFF_ROLL(cnn->m_rbDeltaM,cnn->m_rbp_DeltaM,cnn->m_nOrder,sizeof(short));

 return nOutput;
}

#ifdef APEDEC_USE_MMX

#ifdef __WATCOMC__

int mmx_test(void);

#pragma aux mmx_test=\
 "pushad"\
 "pushfd"\
 "pop     eax"\
 "mov     ecx, eax"\
 "xor     eax, 0x200000"\
 "push    eax"\
 "popfd"\
 "pushfd"\
 "pop     eax"\
 "cmp     eax, ecx"\
 "jz      nocpuid"\
  "mov     eax,1"\
  "CPUID"\
  "test    edx,0x800000"\
 "nocpuid:"\
 "popad"\
 "setnz   al"\
 "and     eax,1"\
 value[eax] modify[eax ecx edx];

int apedec_GetMMXAvailable(void)
{
 return mmx_test();
}

void cnn_adapt_mmx(short *pM, short *pAdapt, int nDirection,int nOrder);

#pragma aux cnn_adapt_mmx=\
 "shr ecx,4"\
 "test ecx,ecx"\
 "jz adaptdone"\
 "cmp ebx,0"\
 "jle AdaptSub"\
 "AdaptAddLoop:"\
  "movq  mm0, [eax]"\
  "paddw mm0, [edx]"\
  "movq  [eax], mm0"\
  "movq  mm1, [eax + 8]"\
  "paddw mm1, [edx + 8]"\
  "movq  [eax + 8], mm1"\
  "movq  mm2, [eax + 16]"\
  "paddw mm2, [edx + 16]"\
  "movq  [eax + 16], mm2"\
  "movq  mm3, [eax + 24]"\
  "paddw mm3, [edx + 24]"\
  "movq  [eax + 24], mm3"\
  "add   eax,32"\
  "add   edx,32"\
  "dec   ecx"\
 "jnz AdaptAddLoop"\
 "jmp adaptend"\
 "AdaptSub:"\
 "je adaptdone"\
 "AdaptSubLoop:"\
  "movq  mm0, [eax]"\
  "psubw mm0, [edx]"\
  "movq  [eax], mm0"\
  "movq  mm1, [eax + 8]"\
  "psubw mm1, [edx + 8]"\
  "movq  [eax + 8], mm1"\
  "movq  mm2, [eax + 16]"\
  "psubw mm2, [edx + 16]"\
  "movq  [eax + 16], mm2"\
  "movq  mm3, [eax + 24]"\
  "psubw mm3, [edx + 24]"\
  "movq  [eax + 24], mm3"\
  "add   eax,32"\
  "add   edx,32"\
  "dec   ecx"\
  "jnz   AdaptSubLoop"\
 "adaptend:emms"\
 "adaptdone:"\
 parm[eax][edx][ebx][ecx] modify[eax edx ebx ecx];

static void CNNFilter_AdaptMMX(short *pM, short *pAdapt, int nDirection, int nOrder)
{
 cnn_adapt_mmx(pM,pAdapt,nDirection,nOrder);
}

int cnn_dotprod_mmx(short * pA, short * pB, int nOrder);

static int CNNFilter_CalculateDotProductMMX(short * pA, short * pB, int nOrder)
{
 int temp;
#pragma aux cnn_dotprod_mmx=\
 "mov dword ptr temp,0"\
 "shr ebx,4"\
 "test ebx,ebx"\
 "jz adaptdone"\
 "pxor    mm7, mm7"\
 "loopDot:"\
  "movq    mm0, [eax]"\
  "pmaddwd mm0, [edx]"\
  "paddd   mm7, mm0"\
  "movq    mm1, [eax +  8]"\
  "pmaddwd mm1, [edx +  8]"\
  "paddd   mm7, mm1"\
  "movq    mm2, [eax + 16]"\
  "pmaddwd mm2, [edx + 16]"\
  "paddd   mm7, mm2"\
  "movq    mm3, [eax + 24]"\
  "pmaddwd mm3, [edx + 24]"\
  "add     eax, 32"\
  "add     edx, 32"\
  "paddd   mm7, mm3"\
  "dec     ebx"\
 "jnz loopDot"\
 "movq   mm6, mm7"\
 "psrlq  mm7, 32"\
 "paddd  mm6, mm7"\
 "movd   dword ptr temp, mm6"\
 "emms"\
 "adaptdone:"\
 "mov eax,dword ptr temp"\
 parm[eax][edx][ebx] value[eax] modify[eax edx ebx];

 return cnn_dotprod_mmx(pA,pB,nOrder);
}

#else // if !__WATCOMC__

#if defined(_MSC_VER)

int apedec_GetMMXAvailable(void)
{
 int retval;
 __asm{
 pushad
 pushfd
 pop     eax
 mov     ecx, eax
 xor     eax, 0x200000
 push    eax
 popfd
 pushfd
 pop     eax
 cmp     eax, ecx
 jz      nocpuid
  mov     eax,1
  CPUID
  test    edx,0x800000
 nocpuid:
 popad
 setnz   al
 and     eax,1
 mov retval,eax
 }
 return retval;
}

static void CNNFilter_AdaptMMX(short *pM, short *pAdapt, int nDirection, int nOrder)
{
 __asm{
 mov eax,pM
 mov edx,pAdapt
 mov ebx,nDirection
 mov ecx,nOrder
 shr ecx,4
 test ecx,ecx
 jz adaptdone
 cmp ebx,0
 jle AdaptSub
 AdaptAddLoop:
  movq  mm0, [eax]
  paddw mm0, [edx]
  movq  [eax], mm0
  movq  mm1, [eax + 8]
  paddw mm1, [edx + 8]
  movq  [eax + 8], mm1
  movq  mm2, [eax + 16]
  paddw mm2, [edx + 16]
  movq  [eax + 16], mm2
  movq  mm3, [eax + 24]
  paddw mm3, [edx + 24]
  movq  [eax + 24], mm3
  add   eax,32
  add   edx,32
  dec   ecx
 jnz AdaptAddLoop
 jmp adaptend
 AdaptSub:
 je adaptdone
 AdaptSubLoop:
  movq  mm0, [eax]
  psubw mm0, [edx]
  movq  [eax], mm0
  movq  mm1, [eax + 8]
  psubw mm1, [edx + 8]
  movq  [eax + 8], mm1
  movq  mm2, [eax + 16]
  psubw mm2, [edx + 16]
  movq  [eax + 16], mm2
  movq  mm3, [eax + 24]
  psubw mm3, [edx + 24]
  movq  [eax + 24], mm3
  add   eax,32
  add   edx,32
  dec   ecx
  jnz   AdaptSubLoop
 adaptend:emms
 adaptdone:
 }
}

static int CNNFilter_CalculateDotProductMMX(short * pA, short * pB, int nOrder)
{
 int retval;
 __asm{
 mov eax,pA
 mov edx,pB
 mov ebx,nOrder
 mov dword ptr retval,0
 shr ebx,4
 test ebx,ebx
 jz adaptdone
 pxor    mm7, mm7
 loopDot:
  movq    mm0, [eax]
  pmaddwd mm0, [edx]
  paddd   mm7, mm0
  movq    mm1, [eax +  8]
  pmaddwd mm1, [edx +  8]
  paddd   mm7, mm1
  movq    mm2, [eax + 16]
  pmaddwd mm2, [edx + 16]
  paddd   mm7, mm2
  movq    mm3, [eax + 24]
  pmaddwd mm3, [edx + 24]
  add     eax, 32
  add     edx, 32
  paddd   mm7, mm3
  dec     ebx
 jnz loopDot
 movq   mm6, mm7
 psrlq  mm7, 32
 paddd  mm6, mm7
 movd   dword ptr retval, mm6
 emms
 adaptdone:
 }
 return retval;
}


#else // !_MSC_VER && !__WATCOMC__

int apedec_GetMMXAvailable(void)
{
 return 0;
}

static void CNNFilter_AdaptMMX(short *pM, short *pAdapt, int nDirection, int nOrder)
{
}

static int CNNFilter_CalculateDotProductMMX(short * pA, short * pB, int nOrder)
{
 return 0;
}

#endif // _MSC_VER

#endif // __WATCOMC__

#endif // APEDEC_USE_MMX

static void CNNFilter_AdaptNoMMX(short *pM, short *pAdapt, int nDirection, int nOrder)
{
 if(!(nOrder&(~15)))
  return;

 if(nDirection < 0){
  do{
   *pM += *pAdapt;pM++;pAdapt++;
  }while(--nOrder);
 }else
  if(nDirection > 0){
   do{
    *pM -= *pAdapt;pM++;pAdapt++;
   }while(--nOrder);
  }
}

static int CNNFilter_CalculateDotProductNoMMX(short * pA, short * pB, int nOrder)
{
 int nDotProduct = 0;

 if(!(nOrder&(~15)))
  return nDotProduct;

 do{
  nDotProduct += *pA++ * *pB++;
 }while(--nOrder);

 return nDotProduct;
}

/*****************************************************************************************
CPredictorDecompressNormal3930to3950
*****************************************************************************************/
static void *CPredictorDecompressNormal3930to3950_init(int nCompressionLevel,int nVersion,struct IPredictorDecompress_func_s **pred_funcs);
static void CPredictorDecompressNormal3930to3950_close(void *predictor_data);
static int CPredictorDecompressNormal3930to3950_Flush(void *predictor_data);
static int CPredictorDecompressNormal3930to3950_DecompressValue(void *predictor_data,int nInput, int nothing);

struct IPredictorDecompress_func_s CPredictorDecompressNormal3930to3950_funcs={
 &CPredictorDecompressNormal3930to3950_init,
 &CPredictorDecompressNormal3930to3950_close,
 &CPredictorDecompressNormal3930to3950_DecompressValue,
 &CPredictorDecompressNormal3930to3950_Flush
};

static void *CPredictorDecompressNormal3930to3950_init(int nCompressionLevel,int nVersion,struct IPredictorDecompress_func_s **pred_funcs)
{
 struct pred3930_data_s *pred_data;
 pred_data=(struct pred3930_data_s *)calloc(1,sizeof(struct pred3930_data_s));
 if(!pred_data)
  return pred_data;

 pred_data->m_pBuffer=(int *)malloc((HISTORY_ELEMENTS + WINDOW_BLOCKS)*sizeof(int));
 if(!pred_data->m_pBuffer)
  goto err_out3930;

 if(nCompressionLevel == COMPRESSION_LEVEL_FAST){
  pred_data->m_pNNFilter  = NULL;
 }else
  if(nCompressionLevel == COMPRESSION_LEVEL_NORMAL){
   pred_data->m_pNNFilter  = CNNFilter_init(16, 11, nVersion);
  }else
   if(nCompressionLevel == COMPRESSION_LEVEL_HIGH){
    pred_data->m_pNNFilter  = CNNFilter_init(64, 11, nVersion);
   }else
    if (nCompressionLevel == COMPRESSION_LEVEL_EXTRA_HIGH){
     pred_data->m_pNNFilter  = CNNFilter_init(256, 13, nVersion);
     pred_data->m_pNNFilter1 = CNNFilter_init( 32, 10, nVersion);
    }else
     goto err_out3930;

 CPredictorDecompressNormal3930to3950_Flush((void *)pred_data);
 *pred_funcs=&CPredictorDecompressNormal3930to3950_funcs;
 return ((void *)pred_data);

err_out3930:
 CPredictorDecompressNormal3930to3950_close((void *)pred_data);
 return NULL;
}

static void CPredictorDecompressNormal3930to3950_close(void *predictor_data)
{
 struct pred3930_data_s *pred_data=(struct pred3930_data_s *)predictor_data;
 if(pred_data){
  CNNFilter_close(pred_data->m_pNNFilter);
  CNNFilter_close(pred_data->m_pNNFilter1);
  if(pred_data->m_pBuffer)
   free(pred_data->m_pBuffer);
  free(pred_data);
 }
}

static int CPredictorDecompressNormal3930to3950_Flush(void *predictor_data)
{
 struct pred3930_data_s *pred_data=(struct pred3930_data_s *)predictor_data;
 if(pred_data->m_pNNFilter)
  CNNFilter_Flush(pred_data->m_pNNFilter);
 if(pred_data->m_pNNFilter1)
  CNNFilter_Flush(pred_data->m_pNNFilter1);

 memset(pred_data->m_pBuffer ,0,(HISTORY_ELEMENTS + 1) * sizeof(int));
 memset(&pred_data->m_aryM[0],0, M_COUNT * sizeof(int));

 pred_data->m_aryM[0] = 360;
 pred_data->m_aryM[1] = 317;
 pred_data->m_aryM[2] = -109;
 pred_data->m_aryM[3] = 98;

 pred_data->m_pInputBuffer = &(pred_data->m_pBuffer[HISTORY_ELEMENTS]);

 pred_data->m_nLastValue = 0;
 pred_data->m_nCurrentIndex = 0;

 return 0;
}

static int CPredictorDecompressNormal3930to3950_DecompressValue(void *predictor_data,int nInput, int nothing)
{
 struct pred3930_data_s *pred_data=(struct pred3930_data_s *)predictor_data;
 int nRetVal;

 if(pred_data->m_nCurrentIndex == WINDOW_BLOCKS){
  memcpy(pred_data->m_pBuffer, &(pred_data->m_pBuffer[WINDOW_BLOCKS]), HISTORY_ELEMENTS * sizeof(int));
  pred_data->m_pInputBuffer = &pred_data->m_pBuffer[HISTORY_ELEMENTS];
  pred_data->m_nCurrentIndex = 0;
 }

 if(pred_data->m_pNNFilter1)
  nInput = CNNFilter_Decompress(pred_data->m_pNNFilter1,nInput);
 if(pred_data->m_pNNFilter)
  nInput = CNNFilter_Decompress(pred_data->m_pNNFilter ,nInput);

 {
  int p1 = pred_data->m_pInputBuffer[-1];
  int p2 = pred_data->m_pInputBuffer[-1] - pred_data->m_pInputBuffer[-2];
  int p3 = pred_data->m_pInputBuffer[-2] - pred_data->m_pInputBuffer[-3];
  int p4 = pred_data->m_pInputBuffer[-3] - pred_data->m_pInputBuffer[-4];

  pred_data->m_pInputBuffer[0] = nInput + (((p1 * pred_data->m_aryM[0])
                                          + (p2 * pred_data->m_aryM[1])
                                          + (p3 * pred_data->m_aryM[2])
                                          + (p4 * pred_data->m_aryM[3])) >> 9);

  if(nInput > 0){
   pred_data->m_aryM[0] -= ((p1 >> 30) & 2) - 1;
   pred_data->m_aryM[1] -= ((p2 >> 30) & 2) - 1;
   pred_data->m_aryM[2] -= ((p3 >> 30) & 2) - 1;
   pred_data->m_aryM[3] -= ((p4 >> 30) & 2) - 1;
  }else
   if(nInput < 0){
    pred_data->m_aryM[0] += ((p1 >> 30) & 2) - 1;
    pred_data->m_aryM[1] += ((p2 >> 30) & 2) - 1;
    pred_data->m_aryM[2] += ((p3 >> 30) & 2) - 1;
    pred_data->m_aryM[3] += ((p4 >> 30) & 2) - 1;
   }
 }

 nRetVal = pred_data->m_pInputBuffer[0] + ((pred_data->m_nLastValue * 31) >> 5);
 pred_data->m_nLastValue = nRetVal;

 pred_data->m_nCurrentIndex++;
 pred_data->m_pInputBuffer++;

 return nRetVal;
}

/*****************************************************************************************
CPredictorDecompress3950toCurrent
*****************************************************************************************/
static void *CPredictorDecompress3950toCurrent_init(int nCompressionLevel,int nVersion,struct IPredictorDecompress_func_s **pred_funcs);
static void CPredictorDecompress3950toCurrent_close(void *predictor_data);
static int CPredictorDecompress3950toCurrent_Flush(void *predictor_data);
static int CPredictorDecompress3950toCurrent_DecompressValue(void *predictor_data,int nA, int nB);

struct IPredictorDecompress_func_s CPredictorDecompress3950toCurrent_funcs={
 &CPredictorDecompress3950toCurrent_init,
 &CPredictorDecompress3950toCurrent_close,
 &CPredictorDecompress3950toCurrent_DecompressValue,
 &CPredictorDecompress3950toCurrent_Flush
};

static void *CPredictorDecompress3950toCurrent_init(int nCompressionLevel,int nVersion,struct IPredictorDecompress_func_s **pred_funcs)
{
 struct pred3950_data_s *pred_data;
 pred_data=(struct pred3950_data_s *)calloc(1,sizeof(struct pred3950_data_s));
 if(!pred_data)
  return pred_data;

 if(nCompressionLevel == COMPRESSION_LEVEL_FAST){
  pred_data->m_pNNFilter  = NULL;
 }else
  if(nCompressionLevel == COMPRESSION_LEVEL_NORMAL){
   pred_data->m_pNNFilter  = CNNFilter_init(16, 11, nVersion);
  }else
   if(nCompressionLevel == COMPRESSION_LEVEL_HIGH){
    pred_data->m_pNNFilter  = CNNFilter_init(64, 11, nVersion);
   }else
    if(nCompressionLevel == COMPRESSION_LEVEL_EXTRA_HIGH){
     pred_data->m_pNNFilter  = CNNFilter_init(256, 13, nVersion);
     pred_data->m_pNNFilter1 = CNNFilter_init( 32, 10, nVersion);
    }else
     if(nCompressionLevel == COMPRESSION_LEVEL_INSANE){
      pred_data->m_pNNFilter  = CNNFilter_init(1024+256, 15, nVersion);
      pred_data->m_pNNFilter1 = CNNFilter_init(     256, 13, nVersion);
      pred_data->m_pNNFilter2 = CNNFilter_init(      16, 11, nVersion);
     }else
      goto err_out3950;

 pred_data->m_rbPredictionA=(int *)malloc((WINDOW_BLOCKS+8)*sizeof(int));
 pred_data->m_rbPredictionB=(int *)malloc((WINDOW_BLOCKS+8)*sizeof(int));
 pred_data->m_rbAdaptA=(int *)malloc((WINDOW_BLOCKS+8)*sizeof(int));
 pred_data->m_rbAdaptB=(int *)malloc((WINDOW_BLOCKS+8)*sizeof(int));
 if(!pred_data->m_rbPredictionA || !pred_data->m_rbPredictionB || !pred_data->m_rbAdaptA || !pred_data->m_rbAdaptB)
  goto err_out3950;

 *pred_funcs=&CPredictorDecompress3950toCurrent_funcs;
 CPredictorDecompress3950toCurrent_Flush((void *)pred_data);
 return ((void *)pred_data);

err_out3950:
 CPredictorDecompress3950toCurrent_close((void *)pred_data);
 return NULL;
}

static void CPredictorDecompress3950toCurrent_close(void *predictor_data)
{
 struct pred3950_data_s *pred_data=(struct pred3950_data_s *)predictor_data;
 if(pred_data){
  CNNFilter_close(pred_data->m_pNNFilter );
  CNNFilter_close(pred_data->m_pNNFilter1);
  CNNFilter_close(pred_data->m_pNNFilter2);
  if(pred_data->m_rbPredictionA)
   free(pred_data->m_rbPredictionA);
  if(pred_data->m_rbPredictionB)
   free(pred_data->m_rbPredictionB);
  if(pred_data->m_rbAdaptA)
   free(pred_data->m_rbAdaptA);
  if(pred_data->m_rbAdaptB)
   free(pred_data->m_rbAdaptB);
  free(pred_data);
 }
}

static int CPredictorDecompress3950toCurrent_Flush(void *predictor_data)
{
 struct pred3950_data_s *pred_data=(struct pred3950_data_s *)predictor_data;

 if(pred_data->m_pNNFilter)
  CNNFilter_Flush(pred_data->m_pNNFilter);
 if(pred_data->m_pNNFilter1)
  CNNFilter_Flush(pred_data->m_pNNFilter1);
 if(pred_data->m_pNNFilter2)
  CNNFilter_Flush(pred_data->m_pNNFilter2);

 memset(pred_data->m_aryMA, 0, sizeof(pred_data->m_aryMA));
 memset(pred_data->m_aryMB, 0, sizeof(pred_data->m_aryMB));

 ROLLBUFF_FLUSH(pred_data->m_rbPredictionA,pred_data->m_rbp_PredictionA,HISTORY_ELEMENTS,sizeof(int));
 ROLLBUFF_FLUSH(pred_data->m_rbPredictionB,pred_data->m_rbp_PredictionB,HISTORY_ELEMENTS,sizeof(int));
 ROLLBUFF_FLUSH(pred_data->m_rbAdaptA,pred_data->m_rbp_AdaptA,HISTORY_ELEMENTS,sizeof(int));
 ROLLBUFF_FLUSH(pred_data->m_rbAdaptB,pred_data->m_rbp_AdaptB,HISTORY_ELEMENTS,sizeof(int));

 pred_data->m_aryMA[0] = 360;
 pred_data->m_aryMA[1] = 317;
 pred_data->m_aryMA[2] = -109;
 pred_data->m_aryMA[3] = 98;

 pred_data->m_Stage1FilterA_lastval=0;
 pred_data->m_Stage1FilterB_lastval=0;

 pred_data->m_nLastValueA = 0;
 pred_data->m_nCurrentIndex = 0;

 return 0;
}

static int scaled_compress(int *last,int nInput)
{
 int nRetVal = nInput - ((*last * 31) >> 5);
 *last = nInput;
 return nRetVal;
}

static int scaled_decompress(int *last,int nInput)
{
 int nRetVal = nInput + ((*last * 31) >> 5);
 *last=nRetVal;
 return nRetVal;
}

static int CPredictorDecompress3950toCurrent_DecompressValue(void *predictor_data,int nA, int nB)
{
 struct pred3950_data_s *pred_data=(struct pred3950_data_s *)predictor_data;
 int nPredictionA,nPredictionB,nCurrentA,nRetVal;

 if(pred_data->m_nCurrentIndex == WINDOW_BLOCKS){
  ROLLBUFF_ROLL(pred_data->m_rbPredictionA,pred_data->m_rbp_PredictionA,HISTORY_ELEMENTS,sizeof(int));
  ROLLBUFF_ROLL(pred_data->m_rbPredictionB,pred_data->m_rbp_PredictionB,HISTORY_ELEMENTS,sizeof(int));
  ROLLBUFF_ROLL(pred_data->m_rbAdaptA,pred_data->m_rbp_AdaptA,HISTORY_ELEMENTS,sizeof(int));
  ROLLBUFF_ROLL(pred_data->m_rbAdaptB,pred_data->m_rbp_AdaptB,HISTORY_ELEMENTS,sizeof(int));

  pred_data->m_nCurrentIndex = 0;
 }

 if(pred_data->m_pNNFilter2)
  nA = CNNFilter_Decompress(pred_data->m_pNNFilter2,nA);
 if(pred_data->m_pNNFilter1)
  nA = CNNFilter_Decompress(pred_data->m_pNNFilter1,nA);
 if(pred_data->m_pNNFilter)
  nA = CNNFilter_Decompress(pred_data->m_pNNFilter ,nA);

 pred_data->m_rbp_PredictionA[ 0] = pred_data->m_nLastValueA;
 pred_data->m_rbp_PredictionA[-1] = pred_data->m_rbp_PredictionA[0]
                                  - pred_data->m_rbp_PredictionA[-1];

 pred_data->m_rbp_PredictionB[ 0] = scaled_compress(&pred_data->m_Stage1FilterB_lastval,nB);
 pred_data->m_rbp_PredictionB[-1] = pred_data->m_rbp_PredictionB[0]
                                  - pred_data->m_rbp_PredictionB[-1];

 nPredictionA = (pred_data->m_rbp_PredictionA[ 0] * pred_data->m_aryMA[0])
              + (pred_data->m_rbp_PredictionA[-1] * pred_data->m_aryMA[1])
              + (pred_data->m_rbp_PredictionA[-2] * pred_data->m_aryMA[2])
              + (pred_data->m_rbp_PredictionA[-3] * pred_data->m_aryMA[3]);
 nPredictionB = (pred_data->m_rbp_PredictionB[ 0] * pred_data->m_aryMB[0])
              + (pred_data->m_rbp_PredictionB[-1] * pred_data->m_aryMB[1])
              + (pred_data->m_rbp_PredictionB[-2] * pred_data->m_aryMB[2])
              + (pred_data->m_rbp_PredictionB[-3] * pred_data->m_aryMB[3])
              + (pred_data->m_rbp_PredictionB[-4] * pred_data->m_aryMB[4]);

 nCurrentA = nA + ((nPredictionA + (nPredictionB >> 1)) >> 10);

 pred_data->m_rbp_AdaptA[ 0] = (pred_data->m_rbp_PredictionA[ 0])? ((pred_data->m_rbp_PredictionA[ 0] >> 30) & 2) - 1 : 0;
 pred_data->m_rbp_AdaptA[-1] = (pred_data->m_rbp_PredictionA[-1])? ((pred_data->m_rbp_PredictionA[-1] >> 30) & 2) - 1 : 0;

 pred_data->m_rbp_AdaptB[ 0] = (pred_data->m_rbp_PredictionB[ 0])? ((pred_data->m_rbp_PredictionB[ 0] >> 30) & 2) - 1 : 0;
 pred_data->m_rbp_AdaptB[-1] = (pred_data->m_rbp_PredictionB[-1])? ((pred_data->m_rbp_PredictionB[-1] >> 30) & 2) - 1 : 0;

 if (nA > 0){
  pred_data->m_aryMA[0] -= pred_data->m_rbp_AdaptA[ 0];
  pred_data->m_aryMA[1] -= pred_data->m_rbp_AdaptA[-1];
  pred_data->m_aryMA[2] -= pred_data->m_rbp_AdaptA[-2];
  pred_data->m_aryMA[3] -= pred_data->m_rbp_AdaptA[-3];

  pred_data->m_aryMB[0] -= pred_data->m_rbp_AdaptB[ 0];
  pred_data->m_aryMB[1] -= pred_data->m_rbp_AdaptB[-1];
  pred_data->m_aryMB[2] -= pred_data->m_rbp_AdaptB[-2];
  pred_data->m_aryMB[3] -= pred_data->m_rbp_AdaptB[-3];
  pred_data->m_aryMB[4] -= pred_data->m_rbp_AdaptB[-4];
 }else
  if (nA < 0){
   pred_data->m_aryMA[0] += pred_data->m_rbp_AdaptA[ 0];
   pred_data->m_aryMA[1] += pred_data->m_rbp_AdaptA[-1];
   pred_data->m_aryMA[2] += pred_data->m_rbp_AdaptA[-2];
   pred_data->m_aryMA[3] += pred_data->m_rbp_AdaptA[-3];

   pred_data->m_aryMB[0] += pred_data->m_rbp_AdaptB[ 0];
   pred_data->m_aryMB[1] += pred_data->m_rbp_AdaptB[-1];
   pred_data->m_aryMB[2] += pred_data->m_rbp_AdaptB[-2];
   pred_data->m_aryMB[3] += pred_data->m_rbp_AdaptB[-3];
   pred_data->m_aryMB[4] += pred_data->m_rbp_AdaptB[-4];
  }

 nRetVal = scaled_decompress(&pred_data->m_Stage1FilterA_lastval,nCurrentA);
 pred_data->m_nLastValueA = nCurrentA;

 pred_data->m_rbp_PredictionA++;
 pred_data->m_rbp_PredictionB++;
 pred_data->m_rbp_AdaptA++;
 pred_data->m_rbp_AdaptB++;

 pred_data->m_nCurrentIndex++;

 return nRetVal;
}
