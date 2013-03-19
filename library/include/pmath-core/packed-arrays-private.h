#ifndef __PMATH_CORE__PACKED_ARRAYS_PRIVATE_H__
#define __PMATH_CORE__PACKED_ARRAYS_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-private.h>
#include <pmath-core/packed-arrays.h>


PMATH_PRIVATE
pmath_t _pmath_packed_element_unbox(const void *data, enum pmath_packed_type_t type);


PMATH_PRIVATE pmath_bool_t _pmath_packed_arrays_init(void);
PMATH_PRIVATE void         _pmath_packed_arrays_done(void);

#endif // __PMATH_CORE__PACKED_ARRAYS_PRIVATE_H__
