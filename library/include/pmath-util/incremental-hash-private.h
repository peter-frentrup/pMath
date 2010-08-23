#ifndef __PMATH_UTIL__INCREMENTAL_HASH_PRIVATE_H__
#define __PMATH_UTIL__INCREMENTAL_HASH_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <stdlib.h>
#include <pmath-config.h>

PMATH_PRIVATE 
unsigned int incremental_hash(
  const void   *data, 
  size_t        len, 
  unsigned int  hash);

#endif
