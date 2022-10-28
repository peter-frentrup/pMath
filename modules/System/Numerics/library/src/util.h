#ifndef __MODULE__PMATH_NUMERICS__UTIL_H__
#define __MODULE__PMATH_NUMERICS__UTIL_H__

#ifndef __MODULE__PMATH_NUMERICS__STDAFX_H__
#  error include "stdafx.h" first
#endif

// If *z == x * ImaginaryI => *z:= x
PMATH_PRIVATE pmath_bool_t pnum_is_imaginary(pmath_t *z);

PMATH_PRIVATE pmath_bool_t pnum_equals_quotient(pmath_t obj, int32_t num, int32_t den);
PMATH_PRIVATE pmath_bool_t pnum_get_small_rational(pmath_t obj, int32_t *num, int32_t *den);

// basically a copy of _pmath_contains_symbol
PMATH_PRIVATE
pmath_bool_t pnum_contains_nonhead_symbol(pmath_t obj, pmath_symbol_t sub);


#endif // __MODULE__PMATH_NUMERICS__UTIL_H__
