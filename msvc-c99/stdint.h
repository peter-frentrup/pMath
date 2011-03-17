#ifndef __MSVC_C99__STDINT_H__
#define __MSVC_C99__STDINT_H__

#include <stddef.h>

typedef          __int8   int8_t;
typedef unsigned __int8   uint8_t;
typedef          __int16  int16_t;
typedef unsigned __int16  uint16_t;
typedef          __int32  int32_t;
typedef unsigned __int32  uint32_t;
typedef          __int64  int64_t;
typedef unsigned __int64  uint64_t;

typedef int64_t  intmax_t;

#define INT32_MAX 2147483647
#define INT32_MIN (-INT32_MAX - 1)

#define UINT32_MAX 0xFFFFFFFF

//typedef ptrdiff_t  intptr_t;
//typedef size_t     uintptr_t;

#ifdef _WIN64
//  typedef int64_t  intptr_t;
//  typedef uint64_t uintptr_t;
  #define UINTPTR_MAX  (0xFFFFFFFFFFFFFFFFULL)
#else
//  typedef int32_t  intptr_t;
//  typedef uint32_t uintptr_t;
  #define UINTPTR_MAX  (0xFFFFFFFFU)
#endif

#endif
