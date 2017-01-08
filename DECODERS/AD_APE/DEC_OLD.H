#ifndef apedec_dec_old_h
#define apedec_dec_old_h

#include "all.h"
#include "apeinfo.h"
#include "pred_old.h"
#include "prepare.h"

typedef struct CAPEDecompressOldCore_data_s {
	int *m_pTempData;
	int *m_pDataX;
	int *m_pDataY;

	void *m_pAntiPredictorX;	// functions!
	void *m_pAntiPredictorY;	//

	struct CUnBitArray_func_s *unbita_funcs;
	void *unbita_datas;

	struct BIT_ARRAY_STATE *m_BitArrayStateX;
	struct BIT_ARRAY_STATE *m_BitArrayStateY;

	void *decoder_datas;

	BOOL m_bMMXAvailable;

	short *IPAdaptFactor;		// tmp for extra high 3800-3929
	short *IPShort;				// -"-

} CAPEDecompressOldCore_data_s;

typedef struct CUnMAC_data_s {
	struct CAPEDecompressOldCore_data_s *core_datas;
	int m_LastDecodedFrameIndex;
	WAVEFORMATEX m_wfeInput;
} CUnMAC_data_s;

typedef struct CAPEDecompressOld_data_s {
	char *m_spBuffer;
	int m_nBufferTail;

	int m_nBlockAlign;
	int m_nCurrentFrame;
	int m_nCurrentBlock;
	int m_nFinishBlock;

	struct CUnMAC_data_s *m_UnMAC;
	struct CAPEInfo_data_s *m_spAPEInfo;
} CAPEDecompressOld_data_s;

extern struct CAPEDecompress_func_s CAPEDecompressOld_funcs;

#endif							//apedec_dec_old_h
