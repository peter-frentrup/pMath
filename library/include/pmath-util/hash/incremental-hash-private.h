#ifndef PMATH_UTIL__HASH__INCREMENTAL_HASH_PRIVATE_H__INCLUDED
#define PMATH_UTIL__HASH__INCREMENTAL_HASH_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-config.h>

#include <stdlib.h>


PMATH_PRIVATE
unsigned int _pmath_incremental_hash(
  const void   *data,
  size_t        len,
  unsigned int  hash);

#endif
