#ifndef APE_PREDICTOR_BASE_H
#define APE_PREDICTOR_BASE_H

#include "all.h"

typedef struct IPredictorDecompress_func_s {
	void *(*Decompress_init) (int nCompressionLevel, int nVersion, struct IPredictorDecompress_func_s ** funcs);
	void (*Decompress_close) (void *pred_data);
	int (*DecompressValue) (void *pred_data, int nA, int nB);
	int (*Flush) (void *pred_data);
} IPredictorDecompress_func_s;

#endif							// #ifndef APE_PREDICTOR_BASE_H
