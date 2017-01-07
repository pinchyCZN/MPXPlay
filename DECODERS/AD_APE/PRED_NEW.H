#ifndef APE_NEWPREDICTOR_H
#define APE_NEWPREDICTOR_H

#include "all.h"
#include "pred_bas.h"

#define NN_WINDOW_ELEMENTS	512

#define WINDOW_BLOCKS		512
#define BUFFER_COUNT		1
#define HISTORY_ELEMENTS	8
#define M_COUNT			8

typedef struct CNNFilter_data_s{
 int m_nOrder;
 int m_nShift;
 int m_nVersion;
 short *m_paryM;

 int m_rb_size;
 short *m_rbInput;
 short *m_rbDeltaM;
 short *m_rbp_Input;
 short *m_rbp_DeltaM;
 int m_nRunningAverage;

 BOOL m_bMMXAvailable;
}CNNFilter_data_s;

typedef struct pred3930_data_s{
 int *m_pBuffer;
 int *m_pInputBuffer;
 int  m_nCurrentIndex;
 int  m_nLastValue;
 struct CNNFilter_data_s *m_pNNFilter;
 struct CNNFilter_data_s *m_pNNFilter1;
 int m_aryM[M_COUNT];
}pred3930_data_s;

typedef struct pred3950_data_s{
 int m_nCurrentIndex;
 int m_nLastValueA;

 int *m_rbPredictionA;      // buffer
 int *m_rbp_PredictionA;    // pointer
 int *m_rbPredictionB;
 int *m_rbp_PredictionB;

 int *m_rbAdaptA;
 int *m_rbp_AdaptA;
 int *m_rbAdaptB;
 int *m_rbp_AdaptB;

 int m_Stage1FilterA_lastval;
 int m_Stage1FilterB_lastval;

 struct CNNFilter_data_s *m_pNNFilter;
 struct CNNFilter_data_s *m_pNNFilter1;
 struct CNNFilter_data_s *m_pNNFilter2;

 int m_aryMA[M_COUNT];
 int m_aryMB[M_COUNT];
}pred3950_data_s;

extern struct IPredictorDecompress_func_s CPredictorDecompressNormal3930to3950_funcs;
extern struct IPredictorDecompress_func_s CPredictorDecompress3950toCurrent_funcs;

#endif // #ifndef APE_NEWPREDICTOR_H
