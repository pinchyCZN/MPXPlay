#include <math.h>
#include "mpcdec.h"
#include "requant.h"

int mpcdec_tab_D[18+1] =
{  2,
   0,     1,     2,     3,     4,     7,    15,    31,    63,
 127,   255,   511,  1023,  2047,  4095,  8191, 16383, 32767
};

MPC_FLOAT_T mpcdec_tab_C[18+1];
MPC_FLOAT_T mpcdec_tab_SCF[6+128];

static void Calc_ScaleFactors(double start,double mult )
{
 int i;

 for(i=0; i < 128+6; i++ ) {
  mpcdec_tab_SCF [i] = (MPC_FLOAT_T) start;
  start *= mult;
 }
}

static void Calc_RequantTab_SV4_7(void)
{
 int i;

 mpcdec_tab_C[0] = 111.28596247532739441973f/32768.0; // 16384 / 255 * sqrt(3) /32768.0
 for( i = 1; i < 19; i++ )
  mpcdec_tab_C[i] = (MPC_FLOAT_T) (1.0 / ((float)mpcdec_tab_D[i] + 0.5));
}

static void Band_Limits(struct mpc_decoder_data *mpci)
{
 // default
 if (mpci->Max_Band_desired==0){
  if (!mpci->IS_used){
   if (mpci->Bitrate> 384) {mpci->Min_Band=32; mpci->Max_Band=31;}
   if (mpci->Bitrate<=384) {mpci->Min_Band=30; mpci->Max_Band=29;}
   if (mpci->Bitrate<=160) {mpci->Min_Band=27; mpci->Max_Band=26;}
   if (mpci->Bitrate<= 64) {mpci->Min_Band=21; mpci->Max_Band=20;}
   if (mpci->Bitrate==  0) {mpci->Min_Band=32; mpci->Max_Band=31;}
  }else
   if (mpci->IS_used){
    if (mpci->Bitrate<=384) {mpci->Min_Band=16; mpci->Max_Band=29;}
    if (mpci->Bitrate<=160) {mpci->Min_Band=12; mpci->Max_Band=26;}
    if (mpci->Bitrate<=112) {mpci->Min_Band= 8; mpci->Max_Band=26;}
    if (mpci->Bitrate<= 64) {mpci->Min_Band= 4; mpci->Max_Band=20;}
   }
 }else{
  // Bandbreite vom user ausgewaehlt
  if (!mpci->IS_used){
   mpci->Max_Band = mpci->Max_Band_desired;
   mpci->Min_Band = mpci->Max_Band +1;
  }else
   if (mpci->IS_used){
    if (mpci->Bitrate<=384) {mpci->Min_Band=16; mpci->Max_Band=mpci->Max_Band_desired;}
    if (mpci->Bitrate<=160) {mpci->Min_Band=12; mpci->Max_Band=mpci->Max_Band_desired;}
    if (mpci->Bitrate<=112) {mpci->Min_Band= 8; mpci->Max_Band=mpci->Max_Band_desired;}
    if (mpci->Bitrate<= 64) {mpci->Min_Band= 4; mpci->Max_Band=mpci->Max_Band_desired;}

    if (mpci->Min_Band>=mpci->Max_Band)
     mpci->Min_Band = mpci->Max_Band;
   }
 }
}

void mpcdec_initialisiere_Quantisierungstabellen(struct mpc_decoder_data *mpci)
{
 Band_Limits(mpci);
}

#define C1  1.20050805774840750476
#define C2  0.83298066476582673961
#define C3  0.501187233627272285285
#define C4  1.122018454301963435591

void mpcdec_init_requant(void)
{
 Calc_RequantTab_SV4_7 ();
 // Abdeckung von +7.936...-98.4127 dB, wobei scf[n]/scf[n-1] = 1.20050805774840750476
 Calc_ScaleFactors(1.0 * C1/(C2*C2*C2*C2*C2*C2), C2 );
}
