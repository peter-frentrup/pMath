#ifndef PMATH__UTIL__DATA_TYPES__BYTE_ARRAYS_PRIVATE_H__INCLUDED
#define PMATH__UTIL__DATA_TYPES__BYTE_ARRAYS_PRIVATE_H__INCLUDED

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-util/data-types/byte-arrays.h>

PMATH_PRIVATE pmath_bool_t _pmath_byte_arrays_init(void);
PMATH_PRIVATE void _pmath_byte_arrays_done(void);

#endif // PMATH__UTIL__DATA_TYPES__BYTE_ARRAYS_PRIVATE_H__INCLUDED
