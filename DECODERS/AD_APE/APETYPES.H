#ifndef apetypes_h
#define apetypes_h

#if !defined(_MSC_VER) && !defined(_WIN32)
 typedef unsigned int BOOL;
#endif
#ifndef TRUE
 #define TRUE 1
#endif
#ifndef FALSE
 #define FALSE 0
#endif
#ifndef __int16
 #define __int16 short
#endif
#ifndef __int32
 #define __int32 long
#endif

#if !defined(tWAVEFORMATEX) && !defined(_WIN32)
 typedef struct tWAVEFORMATEX
 {
    short      wFormatTag;       // format type
    short      nChannels;        // number of channels (i.e. mono, stereo...)
    long       nSamplesPerSec;   // sample rate
    long       nAvgBytesPerSec;  // for buffer estimation
    short      nBlockAlign;      // block size of data
    short      wBitsPerSample;   // number of bits per sample of mono data
    short      cbSize;           // the count in bytes of the size of
				 // extra information (after cbSize)
 } WAVEFORMATEX;
#endif

#if !defined(min)
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

#if !defined(max)
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif

#define read_le_long(p)  *((__int32 *)p)
#define write_le_long(p,n) *((__int32 *)p)=n

#endif // apetypes_h
