#ifndef APE_PREPARE_H
#define APE_PREPARE_H

#include "apetypes.h"

#define SPECIAL_FRAME_MONO_SILENCE     1
#define SPECIAL_FRAME_LEFT_SILENCE     1
#define SPECIAL_FRAME_RIGHT_SILENCE    2
#define SPECIAL_FRAME_PSEUDO_STEREO    4

/*****************************************************************************
Manage the preparation stage of compression and decompression

Tasks:

1) convert data to 32-bit
2) convert L,R to X,Y
3) calculate the CRC
4) do simple analysis
5) check for the peak value
*****************************************************************************/

extern void apedec_UnprepareNew(int X,int Y,struct tWAVEFORMATEX *pWaveFormatEx,unsigned char *pOutput,unsigned int * pCRC);
extern int apedec_UnprepareOld(int *pInputX, int *pInputY, int nBlocks,struct tWAVEFORMATEX *pWaveFormatEx, unsigned char *pRawData, unsigned int *pCRC, int *pSpecialCodes, int nFileVersion);

#endif // #ifndef APE_PREPARE_H
