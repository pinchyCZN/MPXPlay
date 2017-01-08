#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mpcdec.h"
#include "bitstrm.h"
#include "minimax.h"
#include "requant.h"

void mpcdec_clear_datafields(struct mpc_decoder_data *);
static unsigned long random_int(void);

int mpcdec_allocate_datafields(struct mpc_decoder_data *mpci)
{
	mpci->MS_Flag = malloc(32 * sizeof(unsigned int));
	if(!mpci->MS_Flag)
		return 0;
	mpci->Q_val = malloc(32 * sizeof(QuantTyp));
	if(!mpci->Q_val)
		return 0;
	mpci->Res_L = malloc(32 * sizeof(int));
	mpci->Res_R = malloc(32 * sizeof(int));
	if(!mpci->Res_L || !mpci->Res_R)
		return 0;
	mpci->SCFI_L = malloc(32 * sizeof(int));
	mpci->SCFI_R = malloc(32 * sizeof(int));
	if(!mpci->SCFI_L || !mpci->SCFI_R)
		return 0;
	mpci->DSCF_Reference_L = malloc(32 * sizeof(int));
	mpci->DSCF_Reference_R = malloc(32 * sizeof(int));
	if(!mpci->DSCF_Reference_L || !mpci->DSCF_Reference_R)
		return 0;

	mpci->hybridout = (mpc_hybridout_t *) malloc(2 * 2 * 18 * 32 * sizeof(*mpci->hybridout));
	if(!mpci->hybridout)
		return 0;

	mpcdec_clear_datafields(mpci);
	return 1;
}

void mpcdec_clear_datafields(struct mpc_decoder_data *mpci)
{
	memset(mpci->MS_Flag, 0, 32 * sizeof(unsigned int));
	memset(mpci->Q_val, 0, 32 * sizeof(QuantTyp));
	memset(mpci->Res_L, 0, 32 * sizeof(int));
	memset(mpci->Res_R, 0, 32 * sizeof(int));
	memset(mpci->SCFI_L, 0, 32 * sizeof(int));
	memset(mpci->SCFI_R, 0, 32 * sizeof(int));
	memset(mpci->DSCF_Reference_L, 0, 32 * sizeof(int));
	memset(mpci->DSCF_Reference_R, 0, 32 * sizeof(int));
	memset(mpci->hybridout, 0, 2 * 2 * 18 * 32 * sizeof(*mpci->hybridout));	// this is not really needed
}

void mpcdec_free_datafields(struct mpc_decoder_data *mpci)
{
	if(mpci) {
		if(mpci->MS_Flag)
			free(mpci->MS_Flag);
		if(mpci->Q_val)
			free(mpci->Q_val);
		if(mpci->Res_L)
			free(mpci->Res_L);
		if(mpci->Res_R)
			free(mpci->Res_R);
		if(mpci->SCFI_L)
			free(mpci->SCFI_L);
		if(mpci->SCFI_R)
			free(mpci->SCFI_R);
		if(mpci->DSCF_Reference_L)
			free(mpci->DSCF_Reference_L);
		if(mpci->DSCF_Reference_R)
			free(mpci->DSCF_Reference_R);
		if(mpci->hybridout)
			free(mpci->hybridout);
	}
}

//--------------------------------------------------------------------------
void mpcdec_Requantisierung(struct mpc_decoder_data *mpci, int Last_Band, mpc_hybridout_t * hout)
{
	int Band, n;
	MPC_FLOAT_T facL, facR;
	mpc_hybridout_t *YL;
	mpc_hybridout_t *YR;
	int *L;
	int *R;

	for(Band = 0; Band <= Last_Band; ++Band) {
		YL = &hout[Band];
		YR = &hout[18 * 32 + Band];
		L = mpci->Q_val[Band].L;
		R = mpci->Q_val[Band].R;
		/************************** MS-coded **************************/
		if(mpci->MS_Flag[Band]) {
			if(mpci->Res_L[Band]) {
				if(mpci->Res_R[Band]) {	// M!=0, S!=0
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][0]);
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][0]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T tempr = *(R++) * facR;
						MPC_FLOAT_T templ = *(L++) * facL;
						MPCDEC_PUT_HOUT(YL, (templ + tempr));
						MPCDEC_PUT_HOUT(YR, (templ - tempr));
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][1]);
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][1]);
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(L++) * facL;
						MPC_FLOAT_T tempr = *(R++) * facR;
						MPCDEC_PUT_HOUT(YL, (templ + tempr));
						MPCDEC_PUT_HOUT(YR, (templ - tempr));
					}
					YL += 18 * 32;
					YR += 18 * 32;
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(L++) * facL;
						MPC_FLOAT_T tempr = *(R++) * facR;
						MPCDEC_PUT_HOUT(YL, (templ + tempr));
						MPCDEC_PUT_HOUT(YR, (templ - tempr));
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][2]);
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][2]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(L++) * facL;
						MPC_FLOAT_T tempr = *(R++) * facR;
						MPCDEC_PUT_HOUT(YL, (templ + tempr));
						MPCDEC_PUT_HOUT(YR, (templ - tempr));
					}
				} else {		// M!=0, S==0
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][0]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(L++) * facL;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, templ);
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][1]);
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(L++) * facL;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, templ);
					}
					YL += 18 * 32;
					YR += 18 * 32;
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(L++) * facL;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, templ);
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][2]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(L++) * facL;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, templ);
					}
				}
			} else {
				if(mpci->Res_R[Band]) {	// M==0, S!=0
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][0]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(R++) * facR;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, -templ);
					}
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][1]);
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(R++) * facR;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, -templ);
					}
					YL += 18 * 32;
					YR += 18 * 32;
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(R++) * facR;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, -templ);
					}
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][2]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPC_FLOAT_T templ = *(R++) * facR;
						MPCDEC_PUT_HOUT(YL, templ);
						MPCDEC_PUT_HOUT(YR, -templ);
					}
				} else {		// M==0, S==0
					n = 18;
					do {
						MPCDEC_PUT_HOUT(YL, 0.0);
						MPCDEC_PUT_HOUT(YR, 0.0);
						YL += 32;
						YR += 32;
					} while(--n);
					YL += 18 * 32;
					YR += 18 * 32;
					n = 18;
					do {
						MPCDEC_PUT_HOUT(YL, 0.0);
						MPCDEC_PUT_HOUT(YR, 0.0);
						YL += 32;
						YR += 32;
					} while(--n);
				}
			}
		} else {
   /************************** LR-coded **************************/
			if(mpci->Res_L[Band]) {
				if(mpci->Res_R[Band]) {	// L!=0, R!=0
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][0]);
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][0]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][1]);
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][1]);
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
					YL += 18 * 32;
					YR += 18 * 32;
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][2]);
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][2]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
				} else {		// L!=0, R==0
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][0]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, 0.0f);
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][1]);
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, 0.0f);
					}
					YL += 18 * 32;
					YR += 18 * 32;
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, 0.0f);
					}
					facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][2]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, *(L++) * facL);
						MPCDEC_PUT_HOUT(YR, 0.0f);
					}
				}
			} else {
				if(mpci->Res_R[Band]) {	// L==0, R!=0
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][0]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, 0.0f);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][1]);
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, 0.0f);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
					YL += 18 * 32;
					YR += 18 * 32;
					for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, 0.0f);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
					facR = mpcdec_C_mul_SCF(mpci->Res_R[Band], mpci->SCF_Index_R[Band][2]);
					for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
						MPCDEC_PUT_HOUT(YL, 0.0f);
						MPCDEC_PUT_HOUT(YR, *(R++) * facR);
					}
				} else {		// L==0, R==0
					n = 18;
					do {
						MPCDEC_PUT_HOUT(YL, 0.0);
						MPCDEC_PUT_HOUT(YR, 0.0);
						YL += 32;
						YR += 32;
					} while(--n);
					YL += 18 * 32;
					YR += 18 * 32;
					n = 18;
					do {
						MPCDEC_PUT_HOUT(YL, 0.0);
						MPCDEC_PUT_HOUT(YR, 0.0);
						YL += 32;
						YR += 32;
					} while(--n);
				}
			}
		}
	}

	while(Band < 32) {			// required at new file and under crossfade
		YL = &hout[Band];
		YR = &hout[18 * 32 + Band];
		n = 18;
		do {
			MPCDEC_PUT_HOUT(YL, 0.0);
			MPCDEC_PUT_HOUT(YR, 0.0);
			YL += 32;
			YR += 32;
		} while(--n);
		YL += 18 * 32;
		YR += 18 * 32;
		n = 18;
		do {
			MPCDEC_PUT_HOUT(YL, 0.0);
			MPCDEC_PUT_HOUT(YR, 0.0);
			YL += 32;
			YR += 32;
		} while(--n);
		Band++;
	}

}

void mpcdec_Intensity_Stereo_Decode(struct mpc_decoder_data *mpci, mpc_hybridout_t * hout, unsigned int Min_Band, unsigned int Max_Band)
{
	int Band, n;
	MPC_FLOAT_T facL, facR;
	const MPC_FLOAT_T m_isd = (MPC_FLOAT_T) 0.840896415253714543018917;
	mpc_hybridout_t *YL;
	mpc_hybridout_t *YR;
	int *L;
	int *R;

	for(Band = Min_Band; Band <= Max_Band; ++Band) {
		YL = &hout[Band];
		YR = &hout[18 * 32 + Band];
		L = mpci->Q_val[Band].L;
		R = mpci->Q_val[Band].R;
		if(mpci->Res_L[Band]) {
			facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][0]) * m_isd;
			facR = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_R[Band][0]) * m_isd;
			for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
				MPCDEC_PUT_HOUT(YL, *(L++) * facL);
				MPCDEC_PUT_HOUT(YR, *(R++) * facR);
				YL += 32;
				YR += 32;
			}
			facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][1]) * m_isd;
			facR = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_R[Band][1]) * m_isd;
			for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
				MPCDEC_PUT_HOUT(YL, *(L++) * facL);
				MPCDEC_PUT_HOUT(YR, *(R++) * facR);
			}
			YL += 18 * 32;
			YR += 18 * 32;
			for(n = 0; n < 6; ++n, YL += 32, YR += 32) {
				MPCDEC_PUT_HOUT(YL, *(L++) * facL);
				MPCDEC_PUT_HOUT(YR, *(R++) * facR);
			}
			facL = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_L[Band][2]) * m_isd;
			facR = mpcdec_C_mul_SCF(mpci->Res_L[Band], mpci->SCF_Index_R[Band][2]) * m_isd;
			for(n = 0; n < 12; ++n, YL += 32, YR += 32) {
				MPCDEC_PUT_HOUT(YL, *(L++) * facL);
				MPCDEC_PUT_HOUT(YR, *(R++) * facR);
				YL += 32;
				YR += 32;
			}
		} else {
			n = 18;
			do {
				MPCDEC_PUT_HOUT(YL, 0.0);
				MPCDEC_PUT_HOUT(YR, 0.0);
				YL += 32;
				YR += 32;
			} while(--n);
			YL += 18 * 32;
			YR += 18 * 32;
			n = 18;
			do {
				MPCDEC_PUT_HOUT(YL, 0.0);
				MPCDEC_PUT_HOUT(YR, 0.0);
				YL += 32;
				YR += 32;
			} while(--n);
		}
	}
}


#define Decode_DSCF()   mpcdec_Huffman_Decode_fast(mpci,HuffDSCF)

#define HUFFMAN_DECODE_FASTER(h,l,n) mpcdec_Huffman_Decode_faster(mpci,h)
#define HUFFMAN_DECODE_FASTEST(h,l,n) mpcdec_Huffman_Decode_faster(mpci,h)


void mpcdec_Decode_Bitstream_SV7(struct mpc_decoder_data *mpci)
{
	static signed char idx30[] = { -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1, -1, 0, 1 };
	static signed char idx31[] = { -1, -1, -1, 0, 0, 0, 1, 1, 1, -1, -1, -1, 0, 0, 0, 1, 1, 1, -1, -1, -1, 0, 0, 0, 1, 1, 1 };
	static signed char idx32[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	static signed char idx50[] = { -2, -1, 0, 1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2 };
	static signed char idx51[] = { -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2 };
	int Band, diff, Max_Used_Band = -1;
	unsigned int k, idx, tmp;
	HuffmanTyp *Table;
	int *L, *R;
	int *ResL, *ResR;

 /********* Lese Auflösung und LR/MS für Subband 0 *******************************************************/
	ResL = mpci->Res_L;
	ResR = mpci->Res_R;

	*ResL = mpcdec_Bitstream_read(mpci, 4);
	*ResR = 0;
	if(!mpci->IS_used || mpci->Min_Band > 0) {
		*ResR = mpcdec_Bitstream_read(mpci, 4);
		mpci->MS_Flag[0] = 0;	// ???????????????????????????????
		if(mpci->MS_used && (*ResL || *ResR))
			mpci->MS_Flag[0] = mpcdec_Bitstream_read(mpci, 1);
	}
	if(*ResL || *ResR)
		Max_Used_Band = 0;

	ResL++;
	ResR++;

 /********* Lese Auflösung und LR/MS für folgende Subbänder und bestimme letztes Subband *****************/

	Table = HuffHdr;
	for(Band = 1; Band <= mpci->Max_Band; Band++, ResL++, ResR++) {

		diff = mpcdec_Huffman_Decode(mpci, Table);
		*ResL = (diff != 4) ? *(ResL - 1) + diff : mpcdec_Bitstream_read(mpci, 4);
		*ResR = 0;

		// Nicht lesen für IS für Bänder ab MinBand+1
		if(!mpci->IS_used || mpci->Min_Band > Band) {
			diff = mpcdec_Huffman_Decode(mpci, Table);
			*ResR = (diff != 4) ? *(ResR - 1) + diff : mpcdec_Bitstream_read(mpci, 4);
			mpci->MS_Flag[Band] = 0;	// ???????????????????????????
			if(mpci->MS_used && (*ResL || *ResR))
				mpci->MS_Flag[Band] = mpcdec_Bitstream_read(mpci, 1);
		}
		// Bestimme letztes benutztes Subband (folgende Operationen werden nur noch bis zu diesem ausgeführt)
		if(*ResL || *ResR)
			Max_Used_Band = Band;
	}

 /********* Lese verwendetes Scalebandfactor-Splitting der 36 Samples pro Subband ************************/
	L = mpci->SCFI_L;
	R = mpci->SCFI_R;
	ResL = mpci->Res_L;
	ResR = mpci->Res_R;
	Table = HuffSCFI;
	for(Band = 0; Band <= Max_Used_Band; Band++, L++, R++, ResL++, ResR++) {
		if(*ResL)
			*L = mpcdec_Huffman_Decode(mpci, Table);
		if(*ResR || (*ResL && mpci->IS_used && Band >= mpci->Min_Band))
			*R = mpcdec_Huffman_Decode(mpci, Table);
	}

 /********* Lese Scalefaktors für alle Subbänder dreimal für jeweils 12 Samples **************************/
	ResL = mpci->Res_L;
	ResR = mpci->Res_R;
	L = &(mpci->SCF_Index_L[0][0]);
	R = &(mpci->SCF_Index_R[0][0]);
	Table = HuffDSCF;
	for(Band = 0; Band <= Max_Used_Band; Band++, ResL++, ResR++, L += 3, R += 3) {
		if(*ResL) {
			switch (mpci->SCFI_L[Band]) {
			case 0:
				diff = Decode_DSCF();
				L[0] = diff != 8 ? L[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				L[1] = diff != 8 ? L[0] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				L[2] = diff != 8 ? L[1] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			case 1:
				diff = Decode_DSCF();
				L[0] = diff != 8 ? L[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				L[1] = L[2] = diff != 8 ? L[0] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			case 2:
				diff = Decode_DSCF();
				L[0] = L[1] = diff != 8 ? L[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				L[2] = diff != 8 ? L[1] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			case 3:
				diff = Decode_DSCF();
				L[0] = L[1] = L[2] = diff != 8 ? L[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			default:
				return;
			}
		}

		if(*ResR || (*ResL && mpci->IS_used && Band >= mpci->Min_Band)) {
			switch (mpci->SCFI_R[Band]) {
			case 0:
				diff = Decode_DSCF();
				R[0] = diff != 8 ? R[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				R[1] = diff != 8 ? R[0] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				R[2] = diff != 8 ? R[1] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			case 1:
				diff = Decode_DSCF();
				R[0] = diff != 8 ? R[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				R[1] = R[2] = diff != 8 ? R[0] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			case 2:
				diff = Decode_DSCF();
				R[0] = R[1] = diff != 8 ? R[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				diff = Decode_DSCF();
				R[2] = diff != 8 ? R[1] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			case 3:
				diff = Decode_DSCF();
				R[0] = R[1] = R[2] = diff != 8 ? R[2] + diff : mpcdec_Bitstream_read(mpci, 6);
				break;
			default:
				return;
			}
		}
	}

	ResL = mpci->Res_L;
	ResR = mpci->Res_R;
	L = mpci->Q_val[0].L;
	R = mpci->Q_val[0].R;
	for(Band = 0; Band <= Max_Used_Band; Band++, ResL++, ResR++, L += 36, R += 36) {
  /************** links **************/
		switch (*ResL) {
		case -2:
		case -3:
		case -4:
		case -5:
		case -6:
		case -7:
		case -8:
		case -9:
		case -10:
		case -11:
		case -12:
		case -13:
		case -14:
		case -15:
		case -16:
		case -17:
			L += 36;
			break;
		case -1:
			for(k = 0; k < 36; k++) {
				tmp = random_int();
				*L++ = ((tmp >> 24) & 0xFF) + ((tmp >> 16) & 0xFF) + ((tmp >> 8) & 0xFF) + ((tmp >> 0) & 0xFF) - 510;
			}
			break;
		case 0:
			L += 36;			// increase pointer
			break;
		case 1:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][1];
			for(k = 0; k < 12; ++k) {
				idx = mpcdec_Huffman_Decode_fast(mpci, Table);
				*L++ = idx30[idx];
				*L++ = idx31[idx];
				*L++ = idx32[idx];
			}
			break;
		case 2:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][2];
			for(k = 0; k < 18; ++k) {
				idx = mpcdec_Huffman_Decode_fast(mpci, Table);
				*L++ = idx50[idx];
				*L++ = idx51[idx];
			}
			break;
		case 3:
		case 4:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][*ResL];
			for(k = 0; k < 36; ++k)
				*L++ = mpcdec_Huffman_Decode_faster(mpci, Table);
			break;
		case 5:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][*ResL];
			for(k = 0; k < 36; ++k)
				*L++ = mpcdec_Huffman_Decode_fast(mpci, Table);
			break;
		case 6:
		case 7:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][*ResL];
			for(k = 0; k < 36; ++k)
				*L++ = mpcdec_Huffman_Decode(mpci, Table);
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
			for(k = 0; k < 36; ++k)
				*L++ = (int)mpcdec_Bitstream_read(mpci, *ResL - 1) - mpcdec_D(*ResL);
			break;
		default:
			return;
		}
  /************** rechts **************/
		switch (*ResR) {
		case -2:
		case -3:
		case -4:
		case -5:
		case -6:
		case -7:
		case -8:
		case -9:
		case -10:
		case -11:
		case -12:
		case -13:
		case -14:
		case -15:
		case -16:
		case -17:
			R += 36;
			break;
		case -1:
			for(k = 0; k < 36; k++) {
				tmp = random_int();
				*R++ = ((tmp >> 24) & 0xFF) + ((tmp >> 16) & 0xFF) + ((tmp >> 8) & 0xFF) + ((tmp >> 0) & 0xFF) - 510;
			}
			break;
		case 0:
			R += 36;			// increase pointer
			break;
		case 1:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][1];
			for(k = 0; k < 12; ++k) {
				idx = mpcdec_Huffman_Decode_fast(mpci, Table);
				*R++ = idx30[idx];
				*R++ = idx31[idx];
				*R++ = idx32[idx];
			}
			break;
		case 2:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][2];
			for(k = 0; k < 18; ++k) {
				idx = mpcdec_Huffman_Decode_fast(mpci, Table);
				*R++ = idx50[idx];
				*R++ = idx51[idx];
			}
			break;
		case 3:
		case 4:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][*ResR];
			for(k = 0; k < 36; ++k)
				*R++ = mpcdec_Huffman_Decode_faster(mpci, Table);
			break;
		case 5:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][*ResR];
			for(k = 0; k < 36; ++k)
				*R++ = mpcdec_Huffman_Decode_fast(mpci, Table);
			break;
		case 6:
		case 7:
			Table = HuffQ[mpcdec_Bitstream_read(mpci, 1)][*ResR];
			for(k = 0; k < 36; ++k)
				*R++ = mpcdec_Huffman_Decode(mpci, Table);
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
			for(k = 0; k < 36; ++k)
				*R++ = (int)mpcdec_Bitstream_read(mpci, *ResR - 1) - mpcdec_D(*ResR);
			break;
		default:
			return;
		}
	}
}

static const unsigned char Parity[256] = {
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};

static unsigned long __r1 = 1;
static unsigned long __r2 = 1;

static unsigned long random_int(void)
{
	unsigned long t1, t2, t3, t4;

	t3 = t1 = __r1;
	t4 = t2 = __r2;
	t1 &= 0xF5;
	t2 >>= 25;
	t1 = Parity[t1];
	t2 &= 0x63;
	t1 <<= 31;
	t2 = Parity[t2];

	return (__r1 = (t3 >> 1) | t1) ^ (__r2 = (t4 + t4) | t2);
}
