#ifndef _mpcdec_h_
#define _mpcdec_h_

#include "..\ad_mp3\mp3dec.h"

typedef hybridout_t mpc_hybridout_t;

#define MPCDEC_PUT_HOUT MPXDEC_PUT_HOUT
#define MPCDEC_GET_HOUT MPXDEC_GET_HOUT

#ifdef USE_80_HYBRIDOUT
#define MPC_FLOAT_T double
#else
#define MPC_FLOAT_T float
#endif

#define MPC_HEADERSIZE 25		// in bytes

typedef struct quantisierte_Samples {
	int L[36];
	int R[36];
} QuantTyp;

typedef struct mpc_decoder_data {
	unsigned long *framebeginbitpos;	// it's good for < 512mb file
	unsigned int resync_counter;

	unsigned int *bitstream;
	unsigned int bs_bitpos;
	unsigned int bs_dword;
	unsigned int bs_elementptr;
	unsigned int bs_elemcount;
	unsigned int bs_putbyteptr;
	unsigned long bs_rewindbits;
	unsigned long bs_forwardbits;

	unsigned int Bitrate;
	unsigned int OverallFrames;
	unsigned int decodedframes;
	unsigned int StreamVersion;

	unsigned int IS_used;
	unsigned int MS_used;
	unsigned int Max_Band_desired;
	unsigned int Min_Band;
	unsigned int Max_Band;

	hybridout_t *hybridout;
	void *synthdata;

	int *Res_L;
	int *Res_R;
	int *SCFI_L;
	int *SCFI_R;
	int *DSCF_Reference_L;
	int *DSCF_Reference_R;
	unsigned int *MS_Flag;
	QuantTyp *Q_val;
	int SCF_Index_L[32][3];
	int SCF_Index_R[32][3];

} mpc_decoder_data;

extern int mpcdec_allocate_datafields(struct mpc_decoder_data *);
extern void mpcdec_clear_datafields(struct mpc_decoder_data *);
extern void mpcdec_free_datafields(struct mpc_decoder_data *);
extern void mpcdec_init_requant(void);

extern void mpcdec_Decode_Bitstream_SV7(struct mpc_decoder_data *);
extern void mpcdec_Intensity_Stereo_Decode(struct mpc_decoder_data *, mpc_hybridout_t *, unsigned int, unsigned int);
extern void mpcdec_Requantisierung(struct mpc_decoder_data *, int, mpc_hybridout_t *);

#endif
