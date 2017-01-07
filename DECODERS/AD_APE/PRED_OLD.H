#ifndef APE_ANTIPREDICTOR_NEW_H
#define APE_ANTIPREDICTOR_NEW_H

typedef void (*predoldfunc_fnh_t)(int *,int *,int); // fast,normal,high 0000-3929
typedef void (*predoldfunc_eh_0000to3799_t)(int *,int *,int,int,unsigned int *,unsigned int *); // extra high 0000-3799
typedef void (*predoldfunc_eh_3800toCurrent_t)(int *,int *,int,BOOL,int,short *,short *); //extra high 3800-3929

extern void *CreateAntiPredictorOld(int nCompressionLevel, int nVersion);

#endif // #ifndef APE_ANTIPREDICTOR_NEW_H
