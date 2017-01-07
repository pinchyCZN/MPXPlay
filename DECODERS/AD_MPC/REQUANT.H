#ifndef _requant_h
#define _requant_h_

#include "mpcdec.h"

extern int mpcdec_tab_D[18+1];			// Requantisierungs-Offset
#define mpcdec_D(d) mpcdec_tab_D[(d+1)]
extern MPC_FLOAT_T mpcdec_tab_C[18+1];
extern MPC_FLOAT_T mpcdec_tab_SCF[6+128];
#define mpcdec_C_mul_SCF(d,s) mpcdec_tab_C[(d+1)]*mpcdec_tab_SCF[(s+6)]

void mpcdec_initialisiere_Quantisierungstabellen(struct mpc_decoder_data *);

#endif
