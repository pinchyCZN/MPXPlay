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

#include "MACLib.h"
#include "pred_old.h"

#undef APEDEC_USE_MMX // not ready yet here

/***************************************************************************
* fast compression mode                                                    *
****************************************************************************/

static void CAntiPredictorFast0000To3320_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int p,pw,m,*ip,*op,*op1;

 if (NumberOfElements < 32) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 pOutputArray[0] = pInputArray[0];
 pOutputArray[1] = pInputArray[1] + pOutputArray[0];
 pOutputArray[2] = pInputArray[2] + pOutputArray[1];
 pOutputArray[3] = pInputArray[3] + pOutputArray[2];
 pOutputArray[4] = pInputArray[4] + pOutputArray[3];
 pOutputArray[5] = pInputArray[5] + pOutputArray[4];
 pOutputArray[6] = pInputArray[6] + pOutputArray[5];
 pOutputArray[7] = pInputArray[7] + pOutputArray[6];

 m = 4000;

 op1 = &pOutputArray[7];
 p = (*op1 * 2) - pOutputArray[6];
 pw = (p * m) >> 12;

 for (op = &pOutputArray[8], ip = &pInputArray[8]; ip < &pInputArray[NumberOfElements]; ip++, op++, op1++) {
  *op = *ip + pw;

  if (*ip > 0)
   m += (p > 0) ? 4 : -4;
  else
   if (*ip < 0)
    m += (p > 0) ? -4 : 4;

  p = (*op * 2) - *op1;
  pw = (p * m) >> 12;
 }
}

///////note: no output - overwrites input/////////////////
static void CAntiPredictorFast3320ToCurrent_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int p,m,*ip,IP2,IP3,OP1;

 if (NumberOfElements < 3)
  return;

 m = 375;
 IP2 = pInputArray[1];
 IP3 = pInputArray[0];
 OP1 = pInputArray[1];

 for (ip = &pInputArray[2]; ip < &pInputArray[NumberOfElements]; ip++) {

  p = IP2 + IP2 - IP3;

  IP3 = IP2;
  IP2 = *ip + ((p * m) >> 9);

  (*ip ^ p) > 0 ? m++ : m--;

  *ip = IP2 + OP1;
  OP1 = *ip;
 }
}

/***************************************************************************
* normal compression mode                                                  *
****************************************************************************/

static void CAntiPredictorNormal0000To3320_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int *ip, *op, *op1, *op2;
 int p, pw;
 int m;

 if (NumberOfElements < 32) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 memcpy(pOutputArray, pInputArray, 32);

 m = 300;
 op = &pOutputArray[8];
 op1 = &pOutputArray[7];
 op2 = &pOutputArray[6];

 p = (pOutputArray[7] * 3) - (pOutputArray[6] * 3) + pOutputArray[5];
 pw = (p * m) >> 12;

 for (ip = &pInputArray[8]; ip < &pInputArray[NumberOfElements]; ip++, op++, op1++, op2++) {

  *op = *ip + pw;

  if (*ip > 0)
   m += (p > 0) ? 4 : -4;
  else
   if (*ip < 0)
    m += (p > 0) ? -4 : 4;

  p = (*op * 3) - (*op1 * 3) + *op2;
  pw = (p * m) >> 12;
 }

 memcpy(pInputArray, pOutputArray, 32);
 m = 3000;

 op1 = &pInputArray[7];
 p = (*op1 * 2) - pInputArray[6];
 pw = (p * m) >> 12;

 for (op = &pInputArray[8], ip = &pOutputArray[8]; ip < &pOutputArray[NumberOfElements]; ip++, op++, op1++) {
  *op = *ip + pw;

  if (*ip > 0)
   m += (p > 0) ? 12 : -12;
  else
   if (*ip < 0)
    m += (p > 0) ? -12 : 12;

  p = (*op * 2) - *op1;
  pw = (p * m) >> 12;
 }

 pOutputArray[0] = pInputArray[0];
 pOutputArray[1] = pInputArray[1] + pOutputArray[0];
 pOutputArray[2] = pInputArray[2] + pOutputArray[1];
 pOutputArray[3] = pInputArray[3] + pOutputArray[2];
 pOutputArray[4] = pInputArray[4] + pOutputArray[3];
 pOutputArray[5] = pInputArray[5] + pOutputArray[4];
 pOutputArray[6] = pInputArray[6] + pOutputArray[5];
 pOutputArray[7] = pInputArray[7] + pOutputArray[6];

 m = 3900;

 p = pOutputArray[7];
 pw = (p * m) >> 12;

 for (op = &pOutputArray[8], ip = &pInputArray[8]; ip < &pInputArray[NumberOfElements]; ip++, op++) {
  *op = *ip + pw;

  if (*ip > 0)
   m += (p > 0) ? 1 : -1;
  else
   if (*ip < 0)
    m += (p > 0) ? -1 : 1;

  p = *op;
  pw = (p * m) >> 12;
 }
}

static void CAntiPredictorNormal3320To3800_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int q,m1,m2,m3,m4,m5,OP0,OP1,p1,p2,p3,p4,p5,IP0,IP1;

 if (NumberOfElements < 8) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 memcpy(pOutputArray, pInputArray, 20);

 m1 = 0;
 m2 = 64;
 m3 = 28;
 OP1 = pOutputArray[4];

 p3 = (3 * (pOutputArray[4] - pOutputArray[3])) + pOutputArray[2];
 p2 = pInputArray[4] + ((pInputArray[2] - pInputArray[3]) << 3) - pInputArray[1] + pInputArray[0];
 p1 = pOutputArray[4];

 for (q = 5; q < NumberOfElements; q++) {
  OP0 = pInputArray[q] + ((p1 * m1) >> 8);
  (pInputArray[q] ^ p1) > 0 ? m1++ : m1--;
  p1 = OP0;

  pInputArray[q] = OP0 + ((p2 * m2) >> 11);
  (OP0 ^ p2) > 0 ? m2++ : m2--;
  p2 = pInputArray[q] + ((pInputArray[q - 2] - pInputArray[q - 1]) << 3) - pInputArray[q - 3] + pInputArray[q - 4];

  pOutputArray[q] = pInputArray[q] + ((p3 * m3) >> 9);
  (pInputArray[q] ^ p3) > 0 ? m3++ : m3--;
  p3 = (3 * (pOutputArray[q] - pOutputArray[q - 1])) + pOutputArray[q - 2];
 }

 m4 = 370;
 m5 = 3900;

 pOutputArray[1] = pInputArray[1] + pOutputArray[0];
 pOutputArray[2] = pInputArray[2] + pOutputArray[1];
 pOutputArray[3] = pInputArray[3] + pOutputArray[2];
 pOutputArray[4] = pInputArray[4] + pOutputArray[3];

 p4 = (2 * pInputArray[4]) - pInputArray[3];
 p5 = pOutputArray[4];

 IP1 = pInputArray[4];
 for (q = 5; q < NumberOfElements; q++) {
  IP0 = pOutputArray[q] + ((p4 * m4) >> 9);
  (pOutputArray[q] ^ p4) > 0 ? m4++ : m4--;
  p4 = (2 * IP0) - IP1;

  pOutputArray[q] = IP0 + ((p5 * m5) >> 12);
  (IP0 ^ p5) > 0 ? m5++ : m5--;
  p5 = pOutputArray[q];

  IP1 = IP0;
 }
}

#define FIRST_ELEMENT 4

static void CAntiPredictorNormal3800ToCurrent_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int q,m2,m3,m4,m5,m6,p2,p3,p4,p7,*op,*ip,IPP2,opp;

 if (NumberOfElements < 8) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 memcpy(pOutputArray, pInputArray, FIRST_ELEMENT * 4);

 m2 = 64, m3 = 115, m4 = 64, m5 = 740, m6 = 0;
 p4 = pInputArray[FIRST_ELEMENT - 1];
 p3 = (pInputArray[FIRST_ELEMENT - 1] - pInputArray[FIRST_ELEMENT - 2]) << 1;
 p2 = pInputArray[FIRST_ELEMENT - 1] + ((pInputArray[FIRST_ELEMENT - 3] - pInputArray[FIRST_ELEMENT - 2]) << 3);
 op = &pOutputArray[FIRST_ELEMENT];
 ip = &pInputArray[FIRST_ELEMENT];
 IPP2 = ip[-2];
 p7 = 2 * ip[-1] - ip[-2];
 opp = op[-1];

 for (q = 1; q < FIRST_ELEMENT; q++)
  pOutputArray[q] += pOutputArray[q - 1];

 for (; op < &pOutputArray[NumberOfElements]; op++, ip++) {
  register int o = *op, i = *ip;

  o = i + (((p2 * m2) + (p3 * m3) + (p4 * m4)) >> 11);

  if (i > 0) {
   m2 -= ((p2 >> 30) & 2) - 1;
   m3 -= ((p3 >> 28) & 8) - 4;
   m4 -= ((p4 >> 28) & 8) - 4;
  }else
   if (i < 0) {
    m2 += ((p2 >> 30) & 2) - 1;
    m3 += ((p3 >> 28) & 8) - 4;
    m4 += ((p4 >> 28) & 8) - 4;
   }


  p2 = o + ((IPP2 - p4) << 3);
  p3 = (o - p4) << 1;
  IPP2 = p4;
  p4 = o;

  o += (((p7 * m5) - (opp * m6)) >> 10);

  if (p4 > 0) {
   m5 -= ((p7 >> 29) & 4) - 2;
   m6 += ((opp >> 30) & 2) - 1;
  }else
   if (p4 < 0) {
    m5 += ((p7 >> 29) & 4) - 2;
    m6 -= ((opp >> 30) & 2) - 1;
   }

  p7 = 2 * o - opp;
  opp = o;

  *op = o + ((op[-1] * 31) >> 5);
 }
}

#undef FIRST_ELEMENT


/***************************************************************************
* high compression mode                                                    *
****************************************************************************/

static void CAntiPredictorOffset_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements, int Offset, int DeltaM)
{
 int *ip,*ipo,*op,m;

 memcpy(pOutputArray, pInputArray, Offset * 4);

 ip = &pInputArray[Offset];
 ipo = &pOutputArray[0];
 op = &pOutputArray[Offset];
 m = 0;

 for (; op < &pOutputArray[NumberOfElements]; ip++, ipo++, op++){
  *op = *ip + ((*ipo * m) >> 12);

  if((*ipo ^ *ip) > 0)
   m += DeltaM;
  else
   m -= DeltaM;
 }
}

static void CAntiPredictorHigh0000To3320_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int p,pw,q,m;

 if (NumberOfElements < 32) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 memcpy(pOutputArray, pInputArray, 32);
 m = 0;

 for (q = 8; q < NumberOfElements; q++) {
  p = (5 * pOutputArray[q - 1]) - (10 * pOutputArray[q - 2]) + (12 * pOutputArray[q - 3]) - (7 * pOutputArray[q - 4]) + pOutputArray[q - 5];

  pw = (p * m) >> 12;

  pOutputArray[q] = pInputArray[q] + pw;

  if (pInputArray[q] > 0){
   if(p > 0)
    m += 1;
   else
    m -= 1;
  }else
   if (pInputArray[q] < 0){
    if(p > 0)
     m -= 1;
    else
     m += 1;
   }
 }

 memcpy(pInputArray, pOutputArray, 32);
 m = 0;

 for (q = 8; q < NumberOfElements; q++) {
  p = (4 * pInputArray[q - 1]) - (6 * pInputArray[q - 2]) + (4 * pInputArray[q - 3]) - pInputArray[q - 4];
  pw = (p * m) >> 12;

  pInputArray[q] = pOutputArray[q] + pw;

  if (pOutputArray[q] > 0){
   if(p > 0)
    m += 2;
   else
    m -= 2;
  }else{
   if (pOutputArray[q] < 0)
    if(p > 0)
     m -= 2;
    else
     m += 2;
  }
 }

 CAntiPredictorNormal0000To3320_AntiPredict(pInputArray, pOutputArray, NumberOfElements);
}

static void CAntiPredictorHigh3320To3600_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 if (NumberOfElements < 8) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 CAntiPredictorOffset_AntiPredict(pInputArray, pOutputArray, NumberOfElements, 2, 12);
 CAntiPredictorOffset_AntiPredict(pOutputArray, pInputArray, NumberOfElements, 3, 12);

 CAntiPredictorOffset_AntiPredict(pInputArray, pOutputArray, NumberOfElements, 4, 12);
 CAntiPredictorOffset_AntiPredict(pOutputArray, pInputArray, NumberOfElements, 5, 12);

 CAntiPredictorOffset_AntiPredict(pInputArray, pOutputArray, NumberOfElements, 6, 12);
 CAntiPredictorOffset_AntiPredict(pOutputArray, pInputArray, NumberOfElements, 7, 12);

 CAntiPredictorNormal3320To3800_AntiPredict(pInputArray, pOutputArray, NumberOfElements);
}

static void CAntiPredictorHigh3600To3700_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int q,bm1,bm2,bm3,bm4,bm5,bm6,bm7,bm8,bm9,bm10,bm11,bm12,bm13;
 int m2,m3,m4,m5,m6,OP0,OP1,p2,p3,p4,p5,p6,IP0,IP1;
 int bp1,bp2,bp3,bp4,bp5,bp6,bp7,bp8,bp9,bp10,bp11,bp12,bp13;

 if (NumberOfElements < 16) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 memcpy(pOutputArray, pInputArray, 13 * 4);
 bm1 = 0; bm2 = 0;  bm3 = 0;  bm4 = 0;  bm5 = 0;  bm6 = 0; bm7 = 0;
 bm8 = 0; bm9 = 0; bm10 = 0; bm11 = 0; bm12 = 0; bm13 = 0;

 m2 = 64; m3 = 28; m4 = 16;
 OP1 = pOutputArray[12];
 p4 = pInputArray[12];
 p3 = (pInputArray[12] - pInputArray[11]) << 1;
 p2 = pInputArray[12] + ((pInputArray[10] - pInputArray[11]) << 3);
 bp1 = pOutputArray[12];
 bp2 = pOutputArray[11];
 bp3 = pOutputArray[10];
 bp4 = pOutputArray[9];
 bp5 = pOutputArray[8];
 bp6 = pOutputArray[7];
 bp7 = pOutputArray[6];
 bp8 = pOutputArray[5];
 bp9 = pOutputArray[4];
 bp10 = pOutputArray[3];
 bp11 = pOutputArray[2];
 bp12 = pOutputArray[1];
 bp13 = pOutputArray[0];

 for (q = 13; q < NumberOfElements; q++) {
  pInputArray[q] = pInputArray[q] - 1;
  OP0 = (pInputArray[q] - ((bp1 * bm1) >> 8) + ((bp2 * bm2) >> 8) - ((bp3 * bm3) >> 8) - ((bp4 * bm4) >> 8) - ((bp5 * bm5) >> 8) - ((bp6 * bm6) >> 8) - ((bp7 * bm7) >> 8) - ((bp8 * bm8) >> 8) - ((bp9 * bm9) >> 8) + ((bp10 * bm10) >> 8) + ((bp11 * bm11) >> 8) + ((bp12 * bm12) >> 8) + ((bp13 * bm13) >> 8));

  if (pInputArray[q] > 0) {
   bm1 -= bp1 > 0 ? 1 : -1;
   bm2 += bp2 >= 0 ? 1 : -1;
   bm3 -= bp3 > 0 ? 1 : -1;
   bm4 -= bp4 >= 0 ? 1 : -1;
   bm5 -= bp5 > 0 ? 1 : -1;
   bm6 -= bp6 >= 0 ? 1 : -1;
   bm7 -= bp7 > 0 ? 1 : -1;
   bm8 -= bp8 >= 0 ? 1 : -1;
   bm9 -= bp9 > 0 ? 1 : -1;
   bm10 += bp10 >= 0 ? 1 : -1;
   bm11 += bp11 > 0 ? 1 : -1;
   bm12 += bp12 >= 0 ? 1 : -1;
   bm13 += bp13 > 0 ? 1 : -1;
  }else
   if (pInputArray[q] < 0) {
    bm1 -= bp1 <= 0 ? 1 : -1;
    bm2 += bp2 < 0 ? 1 : -1;
    bm3 -= bp3 <= 0 ? 1 : -1;
    bm4 -= bp4 < 0 ? 1 : -1;
    bm5 -= bp5 <= 0 ? 1 : -1;
    bm6 -= bp6 < 0 ? 1 : -1;
    bm7 -= bp7 <= 0 ? 1 : -1;
    bm8 -= bp8 < 0 ? 1 : -1;
    bm9 -= bp9 <= 0 ? 1 : -1;
    bm10 += bp10 < 0 ? 1 : -1;
    bm11 += bp11 <= 0 ? 1 : -1;
    bm12 += bp12 < 0 ? 1 : -1;
    bm13 += bp13 <= 0 ? 1 : -1;
   }

  bp13 = bp12;
  bp12 = bp11;
  bp11 = bp10;
  bp10 = bp9;
  bp9 = bp8;
  bp8 = bp7;
  bp7 = bp6;
  bp6 = bp5;
  bp5 = bp4;
  bp4 = bp3;
  bp3 = bp2;
  bp2 = bp1;
  bp1 = OP0;

  pInputArray[q] = OP0 + ((p2 * m2) >> 11) + ((p3 * m3) >> 9) + ((p4 * m4) >> 9);

  if (OP0 > 0) {
   m2 -= p2 > 0 ? -1 : 1;
   m3 -= p3 > 0 ? -1 : 1;
   m4 -= p4 > 0 ? -1 : 1;
  }else
   if (OP0 < 0) {
    m2 -= p2 > 0 ? 1 : -1;
    m3 -= p3 > 0 ? 1 : -1;
    m4 -= p4 > 0 ? 1 : -1;
   }

  p2 = pInputArray[q] + ((pInputArray[q - 2] - pInputArray[q - 1]) << 3);
  p3 = (pInputArray[q] - pInputArray[q - 1]) << 1;
  p4 = pInputArray[q];

  pOutputArray[q] = pInputArray[q];
 }

 m4 = 370;
 m5 = 3900;

 pOutputArray[1] = pInputArray[1] + pOutputArray[0];
 pOutputArray[2] = pInputArray[2] + pOutputArray[1];
 pOutputArray[3] = pInputArray[3] + pOutputArray[2];
 pOutputArray[4] = pInputArray[4] + pOutputArray[3];
 pOutputArray[5] = pInputArray[5] + pOutputArray[4];
 pOutputArray[6] = pInputArray[6] + pOutputArray[5];
 pOutputArray[7] = pInputArray[7] + pOutputArray[6];
 pOutputArray[8] = pInputArray[8] + pOutputArray[7];
 pOutputArray[9] = pInputArray[9] + pOutputArray[8];
 pOutputArray[10] = pInputArray[10] + pOutputArray[9];
 pOutputArray[11] = pInputArray[11] + pOutputArray[10];
 pOutputArray[12] = pInputArray[12] + pOutputArray[11];

 p4 = (2 * pInputArray[12]) - pInputArray[11];
 p6 = 0;
 p5 = pOutputArray[12];
 m6 = 0;

 IP1 = pInputArray[12];
 for (q = 13; q < NumberOfElements; q++) {
  IP0 = pOutputArray[q] + ((p4 * m4) >> 9) - ((p6 * m6) >> 10);
  (pOutputArray[q] ^ p4) >= 0 ? m4++ : m4--;
  (pOutputArray[q] ^ p6) >= 0 ? m6-- : m6++;
  p4 = (2 * IP0) - IP1;
  p6 = IP0;

  pOutputArray[q] = IP0 + ((p5 * 31) >> 5);
  p5 = pOutputArray[q];

  IP1 = IP0;
 }
}

#define FIRST_ELEMENT	16

static void CAntiPredictorHigh3700To3800_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int x,y,m2,m3,m4,m5,m6,*op,*ip,p2,p3,p4,p7,IPP1,IPP2,opp,Original,q;
 int bm[FIRST_ELEMENT];

 x = 100;
 y = -25;

 if (NumberOfElements < 20) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 memcpy(pOutputArray, pInputArray, FIRST_ELEMENT * 4);

 memset(bm, 0, FIRST_ELEMENT * 4);
 m2 = 64; m3 = 115; m4 = 64; m5 = 740; m6 = 0;
 p4 = pInputArray[FIRST_ELEMENT - 1];
 p3 = (pInputArray[FIRST_ELEMENT - 1] - pInputArray[FIRST_ELEMENT - 2]) << 1;
 p2 = pInputArray[FIRST_ELEMENT - 1] + ((pInputArray[FIRST_ELEMENT - 3] - pInputArray[FIRST_ELEMENT - 2]) << 3);
 op = &pOutputArray[FIRST_ELEMENT];
 ip = &pInputArray[FIRST_ELEMENT];
 IPP2 = ip[-2];
 IPP1 = ip[-1];
 p7 = 2 * ip[-1] - ip[-2];
 opp = op[-1];

 for (q = 1; q < FIRST_ELEMENT; q++)
  pOutputArray[q] += pOutputArray[q - 1];

 for (;op < &pOutputArray[NumberOfElements]; op++, ip++) {
  Original = *ip - 1;
  *ip = Original - (((ip[-1] * bm[0]) + (ip[-2] * bm[1]) + (ip[-3] * bm[2]) + (ip[-4] * bm[3]) + (ip[-5] * bm[4]) + (ip[-6] * bm[5]) + (ip[-7] * bm[6]) + (ip[-8] * bm[7]) + (ip[-9] * bm[8]) + (ip[-10] * bm[9]) + (ip[-11] * bm[10]) + (ip[-12] * bm[11]) + (ip[-13] * bm[12]) + (ip[-14] * bm[13]) + (ip[-15] * bm[14]) + (ip[-16] * bm[15])) >> 8);

  if (Original > 0) {
   bm[0] -= ip[-1] > 0 ? 1 : -1;
   bm[1] += (((unsigned int)(ip[-2]) >> 30) & 2) - 1;
   bm[2] -= ip[-3] > 0 ? 1 : -1;
   bm[3] += (((unsigned int)(ip[-4]) >> 30) & 2) - 1;
   bm[4] -= ip[-5] > 0 ? 1 : -1;
   bm[5] += (((unsigned int)(ip[-6]) >> 30) & 2) - 1;
   bm[6] -= ip[-7] > 0 ? 1 : -1;
   bm[7] += (((unsigned int)(ip[-8]) >> 30) & 2) - 1;
   bm[8] -= ip[-9] > 0 ? 1 : -1;
   bm[9] += (((unsigned int)(ip[-10]) >> 30) & 2) - 1;
   bm[10] -= ip[-11] > 0 ? 1 : -1;
   bm[11] += (((unsigned int)(ip[-12]) >> 30) & 2) - 1;
   bm[12] -= ip[-13] > 0 ? 1 : -1;
   bm[13] += (((unsigned int)(ip[-14]) >> 30) & 2) - 1;
   bm[14] -= ip[-15] > 0 ? 1 : -1;
   bm[15] += (((unsigned int)(ip[-16]) >> 30) & 2) - 1;
  }else
   if (Original < 0) {
    bm[0] -= ip[-1] <= 0 ? 1 : -1;
    bm[1] -= (((unsigned int)(ip[-2]) >> 30) & 2) - 1;
    bm[2] -= ip[-3] <= 0 ? 1 : -1;
    bm[3] -= (((unsigned int)(ip[-4]) >> 30) & 2) - 1;
    bm[4] -= ip[-5] <= 0 ? 1 : -1;
    bm[5] -= (((unsigned int)(ip[-6]) >> 30) & 2) - 1;
    bm[6] -= ip[-7] <= 0 ? 1 : -1;
    bm[7] -= (((unsigned int)(ip[-8]) >> 30) & 2) - 1;
    bm[8] -= ip[-9] <= 0 ? 1 : -1;
    bm[9] -= (((unsigned int)(ip[-10]) >> 30) & 2) - 1;
    bm[10] -= ip[-11] <= 0 ? 1 : -1;
    bm[11] -= (((unsigned int)(ip[-12]) >> 30) & 2) - 1;
    bm[12] -= ip[-13] <= 0 ? 1 : -1;
    bm[13] -= (((unsigned int)(ip[-14]) >> 30) & 2) - 1;
    bm[14] -= ip[-15] <= 0 ? 1 : -1;
    bm[15] -= (((unsigned int)(ip[-16]) >> 30) & 2) - 1;
   }

  *op = *ip + (((p2 * m2) + (p3 * m3) + (p4 * m4)) >> 11);

  if (*ip > 0) {
   m2 -= p2 > 0 ? -1 : 1;
   m3 -= p3 > 0 ? -4 : 4;
   m4 -= p4 > 0 ? -4 : 4;
  }else
   if (*ip < 0) {
    m2 -= p2 > 0 ? 1 : -1;
    m3 -= p3 > 0 ? 4 : -4;
    m4 -= p4 > 0 ? 4 : -4;
   }

  p4 = *op;
  p2 = p4 + ((IPP2 - IPP1) << 3);
  p3 = (p4 - IPP1) << 1;

  IPP2 = IPP1;
  IPP1 = p4;

  *op += (((p7 * m5) - (opp * m6)) >> 10);

  if((IPP1 ^ p7) >= 0)
   m5+=2;
  else
   m5-=2;
  if((IPP1 ^ opp) >= 0)
   m6--;
  else
   m6++;

  p7 = 2 * *op - opp;
  opp = *op;

  *op += ((op[-1] * 31) >> 5);
 }
}

static void CAntiPredictorHigh3800ToCurrent_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements)
{
 int q,m2,m3,m4,m5,m6,p2,p3,p4,p7,*op,*ip,IPP1,IPP2,opp,bm[FIRST_ELEMENT];

 if (NumberOfElements < 20){
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 memcpy(pOutputArray, pInputArray, FIRST_ELEMENT * 4);

 memset(bm, 0, FIRST_ELEMENT * 4);
 m2 = 64; m3 = 115; m4 = 64; m5 = 740; m6 = 0;
 p4 = pInputArray[FIRST_ELEMENT - 1];
 p3 = (pInputArray[FIRST_ELEMENT - 1] - pInputArray[FIRST_ELEMENT - 2]) << 1;
 p2 = pInputArray[FIRST_ELEMENT - 1] + ((pInputArray[FIRST_ELEMENT - 3] - pInputArray[FIRST_ELEMENT - 2]) << 3);
 op = &pOutputArray[FIRST_ELEMENT];
 ip = &pInputArray[FIRST_ELEMENT];
 IPP2 = ip[-2];
 IPP1 = ip[-1];
 p7 = 2 * ip[-1] - ip[-2];
 opp = op[-1];

 for (q = 1; q < FIRST_ELEMENT; q++)
  pOutputArray[q] += pOutputArray[q - 1];

 for (;op < &pOutputArray[NumberOfElements]; op++, ip++){
  unsigned int *pip = (unsigned int *) &ip[-FIRST_ELEMENT];
  int *pbm = &bm[0];
  int nDotProduct = 0;

  if (*ip > 0){
   unsigned int i=16;
   do{
    nDotProduct += *pip * *pbm; *pbm++ += ((*pip++ >> 30) & 2) - 1;
   }while(--i);
   //EXPAND_16_TIMES(nDotProduct += *pip * *pbm; *pbm++ += ((*pip++ >> 30) & 2) - 1;)
  }else
   if (*ip < 0){
    unsigned int i=16;
    do{
     nDotProduct += *pip * *pbm; *pbm++ -= ((*pip++ >> 30) & 2) - 1;
    }while(--i);
    //EXPAND_16_TIMES(nDotProduct += *pip * *pbm; *pbm++ -= ((*pip++ >> 30) & 2) - 1;)
   }else{
    unsigned int i=16;
    do{
     nDotProduct += *pip++ * *pbm++;
    }while(--i);
    //EXPAND_16_TIMES(nDotProduct += *pip++ * *pbm++;)
   }

  *ip -= (nDotProduct >> 9);

  *op = *ip + (((p2 * m2) + (p3 * m3) + (p4 * m4)) >> 11);

  if (*ip > 0){
   m2 -= ((p2 >> 30) & 2) - 1;
   m3 -= ((p3 >> 28) & 8) - 4;
   m4 -= ((p4 >> 28) & 8) - 4;
  }else
   if (*ip < 0){
    m2 += ((p2 >> 30) & 2) - 1;
    m3 += ((p3 >> 28) & 8) - 4;
    m4 += ((p4 >> 28) & 8) - 4;
   }

  p2 = *op + ((IPP2 - p4) << 3);
  p3 = (*op - p4) << 1;
  IPP2 = p4;
  p4 = *op;

  *op += (((p7 * m5) - (opp * m6)) >> 10);

  if (p4 > 0){
   m5 -= ((p7 >> 29) & 4) - 2;
   m6 += ((opp >> 30) & 2) - 1;
  }else
   if (p4 < 0){
    m5 += ((p7 >> 29) & 4) - 2;
    m6 -= ((opp >> 30) & 2) - 1;
   }

  p7 = 2 * *op - opp;
  opp = *op;

  *op += ((op[-1] * 31) >> 5);
 }
}

#undef FIRST_ELEMENT

/***************************************************************************
* extra high compression mode                                              *
****************************************************************************/

static int CAntiPredictorExtraHighHelper_ConventionalDotProduct(short *bip, short *bbm, short *pIPAdaptFactor, int op, int nNumberOfIterations)
{
 int nDotProduct = 0;
 short *pMaxBBM = &bbm[nNumberOfIterations];

 if (op == 0){
  while(bbm < pMaxBBM){
   unsigned int i=32;
   do{
    nDotProduct += *bip++ * *bbm++;
   }while(--i);
   //EXPAND_32_TIMES(nDotProduct += *bip++ * *bbm++;)
  }
 }else
  if (op > 0){
   while(bbm < pMaxBBM){
    unsigned int i=32;
    do{
     nDotProduct += *bip++ * *bbm; *bbm++ += *pIPAdaptFactor++;
    }while(--i);
    //EXPAND_32_TIMES(nDotProduct += *bip++ * *bbm; *bbm++ += *pIPAdaptFactor++;)
   }
  }else{
   while(bbm < pMaxBBM){
    unsigned int i=32;
    do{
     nDotProduct += *bip++ * *bbm; *bbm++ -= *pIPAdaptFactor++;
    }while(--i);
    //EXPAND_32_TIMES(nDotProduct += *bip++ * *bbm; *bbm++ -= *pIPAdaptFactor++;)
   }
  }

 return nDotProduct;
}

#ifdef APEDEC_USE_MMX

static int CAntiPredictorExtraHighHelper_MMXDotProduct(short *bip, short *bbm, short *pIPAdaptFactor, int op, int nNumberOfIterations)
{


}

#endif

/*****************************************************************************************
Extra high 0000 to 3320 implementation
*****************************************************************************************/

static void CAntiPredictorExtraHigh0000To3320_AntiPredictorOffset(int* Input_Array, int* Output_Array, int Number_of_Elements, int g, int dm, int Max_Order)
{
 int q,m;

 if ((g==0) || (Number_of_Elements <= Max_Order)) {
  memcpy(Output_Array, Input_Array, Number_of_Elements * 4);
  return;
 }

 memcpy(Output_Array, Input_Array, Max_Order * 4);

 m = 512;

 if (dm > 0)
  for (q = Max_Order; q < Number_of_Elements; q++) {
   Output_Array[q] = Input_Array[q] + (Output_Array[q - g] >> 3);
  }
 else
  for (q = Max_Order; q < Number_of_Elements; q++) {
   Output_Array[q] = Input_Array[q] - (Output_Array[q - g] >> 3);
  }
}

static void CAntiPredictorExtraHigh0000To3320_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements, int Iterations, unsigned int *pOffsetValueArrayA, unsigned int *pOffsetValueArrayB)
{
 int z;
 for (z = Iterations; z >= 0; z--){
  CAntiPredictorExtraHigh0000To3320_AntiPredictorOffset(pInputArray, pOutputArray, NumberOfElements, pOffsetValueArrayB[z], -1, 64);
  CAntiPredictorExtraHigh0000To3320_AntiPredictorOffset(pOutputArray, pInputArray, NumberOfElements, pOffsetValueArrayA[z], 1, 64);
 }

 CAntiPredictorHigh0000To3320_AntiPredict(pInputArray, pOutputArray, NumberOfElements);
}

/*****************************************************************************************
Extra high 3320 to 3600 implementation
*****************************************************************************************/
static void CAntiPredictorExtraHigh3320To3600_AntiPredictorOffset(int* Input_Array, int* Output_Array, int Number_of_Elements, int g, int dm, int Max_Order)
{
 int q,m;

 if ((g==0) || (Number_of_Elements <= Max_Order)) {
  memcpy(Output_Array, Input_Array, Number_of_Elements * 4);
  return;
 }

 memcpy(Output_Array, Input_Array, Max_Order * 4);

 m = 512;

 if (dm > 0)
  for (q = Max_Order; q < Number_of_Elements; q++) {
   Output_Array[q] = Input_Array[q] + ((Output_Array[q - g] * m) >> 12);
   if((Input_Array[q] ^ Output_Array[q - g]) > 0)
    m += 8;
   else
    m -= 8;
  }
 else
  for (q = Max_Order; q < Number_of_Elements; q++) {
   Output_Array[q] = Input_Array[q] - ((Output_Array[q - g] * m) >> 12);
   if((Input_Array[q] ^ Output_Array[q - g]) > 0)
    m -= 8;
   else
    m += 8;
  }
}

static void CAntiPredictorExtraHigh3320To3600_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements, int Iterations, unsigned int *pOffsetValueArrayA, unsigned int *pOffsetValueArrayB)
{
 int z;
 for (z = Iterations; z >= 0; z--){
  CAntiPredictorExtraHigh3320To3600_AntiPredictorOffset(pInputArray, pOutputArray, NumberOfElements, pOffsetValueArrayB[z], -1, 32);
  CAntiPredictorExtraHigh3320To3600_AntiPredictorOffset(pOutputArray, pInputArray, NumberOfElements, pOffsetValueArrayA[z], 1, 32);
 }

 CAntiPredictorHigh0000To3320_AntiPredict(pInputArray, pOutputArray, NumberOfElements);
}

/*****************************************************************************************
Extra high 3600 to 3700 implementation
*****************************************************************************************/
static void CAntiPredictorExtraHigh3600To3700_AntiPredictorOffset(int* Input_Array, int* Output_Array, int Number_of_Elements, int g1, int g2, int Max_Order)
{
 int q,m,m2;

 if ((g1==0) || (g2==0) || (Number_of_Elements <= Max_Order)) {
  memcpy(Output_Array, Input_Array, Number_of_Elements * 4);
  return;
 }

 memcpy(Output_Array, Input_Array, Max_Order * 4);

 m = 64;
 m2 = 64;

 for (q = Max_Order; q < Number_of_Elements; q++) {
  Output_Array[q] = Input_Array[q] + ((Output_Array[q - g1] * m) >> 9) - ((Output_Array[q - g2] * m2) >> 9);
  (Input_Array[q] ^ Output_Array[q - g1]) > 0 ? m++ : m--;
  (Input_Array[q] ^ Output_Array[q - g2]) > 0 ? m2-- : m2++;
 }
}

static void CAntiPredictorExtraHigh3600To3700_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements, int Iterations, unsigned int *pOffsetValueArrayA, unsigned int *pOffsetValueArrayB)
{
 int z;
 for (z = Iterations; z >= 0; ){

  CAntiPredictorExtraHigh3600To3700_AntiPredictorOffset(pInputArray, pOutputArray, NumberOfElements, pOffsetValueArrayA[z], pOffsetValueArrayB[z], 64);
  z--;

  if (z >= 0) {
   CAntiPredictorExtraHigh3600To3700_AntiPredictorOffset(pOutputArray, pInputArray, NumberOfElements, pOffsetValueArrayA[z], pOffsetValueArrayB[z], 64);
   z--;
  }else {
   memcpy(pInputArray, pOutputArray, NumberOfElements * 4);
   goto Exit_Loop;
   //z--;
  }
 }

Exit_Loop:
 CAntiPredictorHigh3600To3700_AntiPredict(pInputArray, pOutputArray, NumberOfElements);
}

/*****************************************************************************************
Extra high 3700 to 3800 implementation
*****************************************************************************************/
static void CAntiPredictorExtraHigh3700To3800_AntiPredictorOffset(int* Input_Array, int* Output_Array, int Number_of_Elements, int g1, int g2, int Max_Order)
{
 int q,m,m2;

 if ((g1==0) || (g2==0) || (Number_of_Elements <= Max_Order)) {
  memcpy(Output_Array, Input_Array, Number_of_Elements * 4);
  return;
 }

 memcpy(Output_Array, Input_Array, Max_Order * 4);

 m = 64;
 m2 = 64;

 for (q = Max_Order; q < Number_of_Elements; q++) {
  Output_Array[q] = Input_Array[q] + ((Output_Array[q - g1] * m) >> 9) - ((Output_Array[q - g2] * m2) >> 9);
  (Input_Array[q] ^ Output_Array[q - g1]) > 0 ? m++ : m--;
  (Input_Array[q] ^ Output_Array[q - g2]) > 0 ? m2-- : m2++;
 }
}

static void CAntiPredictorExtraHigh3700To3800_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements, int Iterations, unsigned int *pOffsetValueArrayA, unsigned int *pOffsetValueArrayB)
{
 int z;
 for (z = Iterations; z >= 0; ) {

  CAntiPredictorExtraHigh3700To3800_AntiPredictorOffset(pInputArray, pOutputArray, NumberOfElements, pOffsetValueArrayA[z], pOffsetValueArrayB[z], 64);
  z--;

  if (z >= 0) {
   CAntiPredictorExtraHigh3700To3800_AntiPredictorOffset(pOutputArray, pInputArray, NumberOfElements, pOffsetValueArrayA[z], pOffsetValueArrayB[z], 64);
   z--;
  }else {
   memcpy(pInputArray, pOutputArray, NumberOfElements * 4);
   goto Exit_Loop;
   //z--;
  }
 }

Exit_Loop:
 CAntiPredictorHigh3700To3800_AntiPredict(pInputArray, pOutputArray, NumberOfElements);
}

/*****************************************************************************************
Extra high 3800 to Current
*****************************************************************************************/
static void CAntiPredictorExtraHigh3800ToCurrent_AntiPredict(int *pInputArray, int *pOutputArray, int NumberOfElements, BOOL bMMXAvailable, int nVersion, short *IPAdaptFactor,short *IPShort)
{
 int nFilterStageElements,nFilterStageShift,nMaxElements,nFirstElement,nStageCShift;
 int m2,m3,m4,m5,m6, p2,p3,p4,p7, *op,*ip, IPP1,IPP2, opp, Original,q;
 int FM[9],FP[9];
 short bm[256];

 nMaxElements = (nVersion < 3830) ? 134 : 262;
 if(NumberOfElements < nMaxElements) {
  memcpy(pOutputArray, pInputArray, NumberOfElements * 4);
  return;
 }

 nFilterStageElements = (nVersion < 3830) ? 128 : 256;
 nFilterStageShift = (nVersion < 3830) ? 11 : 12;
 nFirstElement = (nVersion < 3830) ? 128 : 256;
 nStageCShift = (nVersion < 3830) ? 10 : 11;

 memset(IPAdaptFactor,0,NumberOfElements * sizeof(short));
 memset(IPShort,      0,NumberOfElements * sizeof(short));

 memcpy(pOutputArray, pInputArray, nFirstElement * 4);

 memset(bm, 0, 256 * 2);
 m2 = 64; m3 = 115; m4 = 64; m5 = 740; m6 = 0;
 p4 = pInputArray[nFirstElement - 1];
 p3 = (pInputArray[nFirstElement - 1] - pInputArray[nFirstElement - 2]) << 1;
 p2 = pInputArray[nFirstElement - 1] + ((pInputArray[nFirstElement - 3] - pInputArray[nFirstElement - 2]) << 3);// - pInputArray[3] + pInputArray[2];
 op = &pOutputArray[nFirstElement];
 ip = &pInputArray[nFirstElement];
 IPP2 = ip[-2];
 IPP1 = ip[-1];
 p7 = 2 * ip[-1] - ip[-2];
 opp = op[-1];

 for (q = 1; q < nFirstElement; q++) {
  pOutputArray[q] += pOutputArray[q - 1];
 }

 for (q = 0; q < nFirstElement; q++) {
  IPAdaptFactor[q] = ((pInputArray[q] >> 30) & 2) - 1;
  IPShort[q] = (short)pInputArray[q];
 }

 memset(&FM[0], 0, 9 * 4);
 memset(&FP[0], 0, 9 * 4);

 for (q = nFirstElement; op < &pOutputArray[NumberOfElements]; op++, ip++, q++) {

  if (nVersion >= 3830){
   int *pFP = &FP[8];
   int *pFM = &FM[8];
   int nDotProduct = 0;

   FP[0] = ip[0];

   if(FP[0] == 0){
    unsigned int i=8;
    do{
     nDotProduct += *pFP * *pFM--; *pFP-- = *(pFP - 1);
    }while(--i);
    //EXPAND_8_TIMES(nDotProduct += *pFP * *pFM--; *pFP-- = *(pFP - 1);)
   }else
    if(FP[0] > 0){
     unsigned int i=8;
     do{
      nDotProduct += *pFP * *pFM; *pFM-- += ((*pFP >> 30) & 2) - 1; *pFP-- = *(pFP - 1);
     }while(--i);
     //EXPAND_8_TIMES(nDotProduct += *pFP * *pFM; *pFM-- += ((*pFP >> 30) & 2) - 1; *pFP-- = *(pFP - 1);)
    }else{
     unsigned int i=8;
     do{
      nDotProduct += *pFP * *pFM; *pFM-- -= ((*pFP >> 30) & 2) - 1; *pFP-- = *(pFP - 1);
     }while(--i);
     //EXPAND_8_TIMES(nDotProduct += *pFP * *pFM; *pFM-- -= ((*pFP >> 30) & 2) - 1; *pFP-- = *(pFP - 1);)
    }

    *ip -= nDotProduct >> 9;
  }

  Original = *ip;

  IPShort[q] = (short)(*ip);
  IPAdaptFactor[q] = ((ip[0] >> 30) & 2) - 1;

#ifdef APEDEC_USE_MMX
  if (bMMXAvailable && (Original != 0)){
   *ip -= (CAntiPredictorExtraHighHelper_MMXDotProduct(&IPShort[q-nFirstElement], &bm[0], &IPAdaptFactor[q-nFirstElement], Original, nFilterStageElements) >> nFilterStageShift);
  }else{
   *ip -= (CAntiPredictorExtraHighHelper_ConventionalDotProduct(&IPShort[q-nFirstElement], &bm[0], &IPAdaptFactor[q-nFirstElement], Original, nFilterStageElements) >> nFilterStageShift);
  }
#else
  *ip -= (CAntiPredictorExtraHighHelper_ConventionalDotProduct(&IPShort[q-nFirstElement], &bm[0], &IPAdaptFactor[q-nFirstElement], Original, nFilterStageElements) >> nFilterStageShift);
#endif

  IPShort[q] = (short)(*ip);
  IPAdaptFactor[q] = ((ip[0] >> 30) & 2) - 1;

  *op = *ip + (((p2 * m2) + (p3 * m3) + (p4 * m4)) >> 11);

  if (*ip > 0) {
   m2 -= ((p2 >> 30) & 2) - 1;
   m3 -= ((p3 >> 28) & 8) - 4;
   m4 -= ((p4 >> 28) & 8) - 4;
  }else
   if (*ip < 0) {
    m2 += ((p2 >> 30) & 2) - 1;
    m3 += ((p3 >> 28) & 8) - 4;
    m4 += ((p4 >> 28) & 8) - 4;
   }


  p2 = *op + ((IPP2 - p4) << 3);
  p3 = (*op - p4) << 1;
  IPP2 = p4;
  p4 = *op;

  *op += (((p7 * m5) - (opp * m6)) >> nStageCShift);

  if (p4 > 0) {
   m5 -= ((p7 >> 29) & 4) - 2;
   m6 += ((opp >> 30) & 2) - 1;
  }else
   if (p4 < 0) {
    m5 += ((p7 >> 29) & 4) - 2;
    m6 -= ((opp >> 30) & 2) - 1;
   }

  p7 = 2 * *op - opp;
  opp = *op;

  *op += ((op[-1] * 31) >> 5);
 }
}

void *CreateAntiPredictorOld(int nCompressionLevel, int nVersion)
{
 void *pAntiPredictor = NULL;

 switch (nCompressionLevel){
  case COMPRESSION_LEVEL_FAST:
	if (nVersion < 3320)
	 pAntiPredictor = &CAntiPredictorFast0000To3320_AntiPredict;
	else
	 pAntiPredictor = &CAntiPredictorFast3320ToCurrent_AntiPredict;
	break;
  case COMPRESSION_LEVEL_NORMAL:
	if (nVersion < 3320)
	 pAntiPredictor = &CAntiPredictorNormal0000To3320_AntiPredict;
	else
	 if (nVersion < 3800)
	  pAntiPredictor = &CAntiPredictorNormal3320To3800_AntiPredict;
	 else
	  pAntiPredictor = &CAntiPredictorNormal3800ToCurrent_AntiPredict;
	break;
  case COMPRESSION_LEVEL_HIGH:
	if (nVersion < 3320)
	 pAntiPredictor = &CAntiPredictorHigh0000To3320_AntiPredict;
	else
	 if (nVersion < 3600)
	  pAntiPredictor = &CAntiPredictorHigh3320To3600_AntiPredict;
	 else
	  if (nVersion < 3700)
	   pAntiPredictor = &CAntiPredictorHigh3600To3700_AntiPredict;
	  else
	   if (nVersion < 3800)
	    pAntiPredictor = &CAntiPredictorHigh3700To3800_AntiPredict;
	   else
	    pAntiPredictor = &CAntiPredictorHigh3800ToCurrent_AntiPredict;
	break;
  case COMPRESSION_LEVEL_EXTRA_HIGH:
	if (nVersion < 3320)
	 pAntiPredictor = &CAntiPredictorExtraHigh0000To3320_AntiPredict;
	else
	 if (nVersion < 3600)
	  pAntiPredictor = &CAntiPredictorExtraHigh3320To3600_AntiPredict;
	 else
	  if (nVersion < 3700)
	   pAntiPredictor = &CAntiPredictorExtraHigh3600To3700_AntiPredict;
	  else
	   if (nVersion < 3800)
	    pAntiPredictor = &CAntiPredictorExtraHigh3700To3800_AntiPredict;
	   else
	    pAntiPredictor = &CAntiPredictorExtraHigh3800ToCurrent_AntiPredict;
        break;
 }

 return pAntiPredictor;
}

#endif // #ifdef BACKWARDS_COMPATIBILITY
