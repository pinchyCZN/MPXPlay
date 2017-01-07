#ifndef INTTYPES_H_
#define INTTYPES_H_

#ifndef MPXPLAY_INT_TYPES
#define MPXPLAY_INT_TYPES

#ifdef _WIN32

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;

typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else

#ifdef __WATCOMC__

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;

#else

#include <stdint.h>

#endif

#endif

#endif // MPXPLAY_INT_TYPES

#endif // INTTYPES_H_
