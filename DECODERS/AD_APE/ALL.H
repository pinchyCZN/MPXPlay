#ifndef APE_ALL_H
#define APE_ALL_H

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include "apetypes.h"
#include "in_file.h"

/*****************************************************************************************
Global compiler settings (useful for porting)
*****************************************************************************************/

#define APEDEC_USE_MMX
#define BACKWARDS_COMPATIBILITY

/*****************************************************************************************
Macros
*****************************************************************************************/

#define RETURN_ON_ERROR(FUNCTION) {int nRetVal = FUNCTION; if (nRetVal != 0) { return nRetVal; } }


//#define ROLLBUFF_ALLOC(ptr0,wine,hise,datatype) (ptr0=(datatype *)malloc((wine+hise)*sizeof(datatype)));

#define ROLLBUFF_FLUSH(ptr0,ptrc,len,datasize) { \
                       memset(ptr0, 0,(len + 1) * datasize); \
		       ptrc = &ptr0[len]; }

#define ROLLBUFF_ROLL(ptr0,ptrc,len,datasize) { \
                     memcpy(ptr0, &ptrc[-len],len*datasize); \
                     ptrc = &ptr0[len]; }


//#define EXPAND_8_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE
//#define EXPAND_16_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE
//#define EXPAND_32_TIMES(CODE) CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE CODE

/*****************************************************************************************
Error Codes
*****************************************************************************************/

// success
#define APEDEC_ERROR_SUCCESS	                            0

// file and i/o errors (1000's)
#define APEDEC_ERROR_IO_READ				    -1000
#define APEDEC_ERROR_IO_WRITE				    -1001
#define APEDEC_ERROR_INVALID_INPUT_FILE		            -1002
#define APEDEC_ERROR_INVALID_OUTPUT_FILE		    -1003
#define APEDEC_ERROR_INPUT_FILE_TOO_LARGE		    -1004
#define APEDEC_ERROR_INPUT_FILE_UNSUPPORTED_BIT_DEPTH	    -1005
#define APEDEC_ERROR_INPUT_FILE_UNSUPPORTED_SAMPLE_RATE     -1006
#define APEDEC_ERROR_INPUT_FILE_UNSUPPORTED_CHANNEL_COUNT   -1007
#define APEDEC_ERROR_INPUT_FILE_TOO_SMALL		    -1008
#define APEDEC_ERROR_INVALID_CHECKSUM			    -1009
#define APEDEC_ERROR_DECOMPRESSING_FRAME		    -1010
#define APEDEC_ERROR_INITIALIZING_UNMAC		            -1011
#define APEDEC_ERROR_INVALID_FUNCTION_PARAMETER	            -1012
#define APEDEC_ERROR_UNSUPPORTED_FILE_TYPE		    -1013
#define APEDEC_ERROR_UPSUPPORTED_FILE_VERSION		    -1014
#define APEDEC_ERROR_IO_SEEK                                -1100
#define APEDEC_ERROR_IO_EOF                                 -1101

// memory errors (2000's)
#define APEDEC_ERROR_INSUFFICIENT_MEMORY		    -2000

// unknown error
#define APEDEC_ERROR_UNDEFINED				    -1

#endif							// #ifndef APE_ALL_H
