/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: mdct.c,v 1.27 2003/09/21 00:00:00 PDSoft Exp $
**/

/*
 * Fast (I)MDCT Implementation using (I)FFT ((Inverse) Fast Fourier Transform)
 * and consists of three steps: pre-(I)FFT complex multiplication, complex
 * (I)FFT, post-(I)FFT complex multiplication,
 *
 * As described in:
 *  P. Duhamel, Y. Mahieux, and J.P. Petit, "A Fast Algorithm for the
 *  Implementation of Filter Banks Based on 'Time Domain Aliasing
 *  Cancellation’," IEEE Proc. on ICASSP‘91, 1991, pp. 2209-2212.
 *
 *
 * As of April 6th 2002 completely rewritten.
 * This (I)MDCT can now be used for any data size n, where n is divisible by 8.
 *
 */

#include "common.h"
#include "structs.h"

#include <stdlib.h>

#include "cfft.h"
#include "mdct.h"

mdct_info *faad_mdct_init(uint32_t N)
{
 mdct_info *mdct;
 uint32_t k;
 complex_t *sc;
 double cangle, sangle, c, s, scale;

 mdct = (mdct_info*)malloc(sizeof(mdct_info));
 if(!mdct)
  return mdct;

 mdct->N = N;
 mdct->sincos = (complex_t*)malloc(N/4*sizeof(complex_t));
 if(!mdct->sincos)
  goto err_out;
 mdct->Z1 = (complex_t*)malloc(N/4*sizeof(complex_t));
 if(!mdct->Z1)
  goto err_out;

 c = cos(2.0f / N * M_PI / 8.0f);
 s = sin(2.0f / N * M_PI / 8.0f);
 sangle = sin(2.0f / N * M_PI);
 cangle = cos(2.0f / N * M_PI);
 scale = sqrt(2.0f / N);

 sc=mdct->sincos[0];
 k=N/4;

 do{
  double cold;
  RE(sc[0]) = -MUL_C_C(c,scale);
  IM(sc[0]) = -MUL_C_C(s,scale);
  sc++;
  cold = c;
  c = MUL_C_C(c,cangle) - MUL_C_C(s,sangle);
  s = MUL_C_C(s,cangle) + MUL_C_C(cold,sangle);
 }while(--k);

 mdct->cfft = cffti(N/4);
 if(!mdct->cfft)
  goto err_out;

 return mdct;

err_out:
 faad_mdct_end(mdct);
 free(mdct);
 return NULL;
}

/*mdct_info *faad_mdct_init(uint32_t N)
{
 mdct_info *mdct;
 uint32_t k;
 real_t cangle, sangle, c, s, cold,scale;
 complex_t *sc;

 mdct = (mdct_info*)malloc(sizeof(mdct_info));
 if(!mdct)
  return mdct;

 mdct->N = N;
 mdct->sincos = (complex_t*)malloc(N/4*sizeof(complex_t));
 if(!mdct->sincos)
  goto err_out;
 mdct->Z1 = (complex_t*)malloc(N/4*sizeof(complex_t));
 if(!mdct->Z1)
  goto err_out;

 scale = sqrt(2.0f / N);
 cangle = cos(2.0f / N * M_PI);
 sangle = sin(2.0f / N * M_PI);
 c = cos(2.0f / N * M_PI / 8.0f);
 s = sin(2.0f / N * M_PI / 8.0f);

 sc=mdct->sincos[0];
 for (k = 0; k < N/4; k++){
  RE(sc[0]) = -1*MUL_C_C(c,scale);
  IM(sc[0]) = -1*MUL_C_C(s,scale);
  sc++;
  cold = c;
  c = MUL_C_C(c,cangle) - MUL_C_C(s,sangle);
  s = MUL_C_C(s,cangle) + MUL_C_C(cold,sangle);
 }

 mdct->cfft = cffti(N/4);
 if(!mdct->cfft)
  goto err_out;

 return mdct;

err_out:
 faad_mdct_end(mdct);
 free(mdct);
 return NULL;
}*/

void faad_mdct_end(mdct_info *mdct)
{
 if(mdct){
  cfftu(mdct->cfft);

  if(mdct->Z1)
   free(mdct->Z1);
  if(mdct->sincos)
   free(mdct->sincos);

  free(mdct);
 }
}

void faad_imdct(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
 uint32_t k;

 complex_t *Z1 = mdct->Z1;
 complex_t *sincos = mdct->sincos;

 uint32_t N  = mdct->N;
 uint32_t N2 = N >> 1;
 uint32_t N4 = N >> 2;
 uint32_t N8 = N >> 3;

 real_t *rex=&X_in[ 0];
 real_t *imx=&X_in[N2-1];

 k=N4;
 do{
  RE(Z1[0]) = MUL_R_C(*imx, RE(sincos[0])) - MUL_R_C(*rex, IM(sincos[0]));
  IM(Z1[0]) = MUL_R_C(*rex, RE(sincos[0])) + MUL_R_C(*imx, IM(sincos[0]));
  rex+=2;
  imx-=2;
  sincos++;
  Z1++;
 }while(--k);
 Z1 = mdct->Z1;

 cfftb(mdct->cfft, Z1);

 sincos = mdct->sincos;
 k=N4;
 do{
  double rex = RE(Z1[0]);
  double imx = IM(Z1[0]);

  RE(Z1[0]) = MUL_R_C(rex, RE(sincos[0])) - MUL_R_C(imx, IM(sincos[0]));
  IM(Z1[0]) = MUL_R_C(imx, RE(sincos[0])) + MUL_R_C(rex, IM(sincos[0]));
  Z1++;
  sincos++;
 }while(--k);

 Z1 = mdct->Z1;
 for (k = 0; k < N8; k++){
  uint16_t n = k << 1;
  X_out[              n] =  IM(Z1[N8 +     k]);
  X_out[          1 + n] = -RE(Z1[N8 - 1 - k]);
  X_out[N4 +          n] =  RE(Z1[         k]);
  X_out[N4 +      1 + n] = -IM(Z1[N4 - 1 - k]);
  X_out[N2 +          n] =  RE(Z1[N8 +     k]);
  X_out[N2 +      1 + n] = -IM(Z1[N8 - 1 - k]);
  X_out[N2 + N4 +     n] = -IM(Z1[         k]);
  X_out[N2 + N4 + 1 + n] =  RE(Z1[N4 - 1 - k]);
 }
}

#ifdef LTP_DEC
void faad_mdct(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    complex_t x;
    complex_t *Z1 = mdct->Z1;
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

	real_t scale = REAL_CONST(N);

    /* pre-FFT complex multiplication */
    for (k = 0; k < N8; k++)
    {
        uint16_t n = k << 1;
        RE(x) = X_in[N - N4 - 1 - n] + X_in[N - N4 +     n];
        IM(x) = X_in[    N4 +     n] - X_in[    N4 - 1 - n];

        RE(Z1[k]) = -MUL_R_C(RE(x), RE(sincos[k])) - MUL_R_C(IM(x), IM(sincos[k]));
        IM(Z1[k]) = -MUL_R_C(IM(x), RE(sincos[k])) + MUL_R_C(RE(x), IM(sincos[k]));

        RE(x) =  X_in[N2 - 1 - n] - X_in[        n];
        IM(x) =  X_in[N2 +     n] + X_in[N - 1 - n];

        RE(Z1[k + N8]) = -MUL_R_C(RE(x), RE(sincos[k + N8])) - MUL_R_C(IM(x), IM(sincos[k + N8]));
        IM(Z1[k + N8]) = -MUL_R_C(IM(x), RE(sincos[k + N8])) + MUL_R_C(RE(x), IM(sincos[k + N8]));
    }

    /* complex FFT */
    cfftf(mdct->cfft, Z1);

    /* post-FFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        uint16_t n = k << 1;
        RE(x) = MUL(MUL_R_C(RE(Z1[k]), RE(sincos[k])) + MUL_R_C(IM(Z1[k]), IM(sincos[k])), scale);
        IM(x) = MUL(MUL_R_C(IM(Z1[k]), RE(sincos[k])) - MUL_R_C(RE(Z1[k]), IM(sincos[k])), scale);

        X_out[         n] =  RE(x);
        X_out[N2 - 1 - n] = -IM(x);
        X_out[N2 +     n] =  IM(x);
        X_out[N  - 1 - n] = -RE(x);
    }
}
#endif
