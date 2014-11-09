#ifndef __PMATH_CORE__INTERVAL_PRIVATE_H__
#define __PMATH_CORE__INTERVAL_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/intervals.h>

#include <mpfi.h>


struct _pmath_interval_t {
  struct _pmath_t  inherited;
  mpfi_t           value;
};

#define PMATH_AS_MP_INTERVAL(obj)  (((struct _pmath_interval_t*) PMATH_AS_PTR(obj))->value)


PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_interval_t _pmath_create_interval(mpfr_prec_t precision);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_interval_exceptions(pmath_interval_t x);

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_interval_get_value(
  pmath_interval_t interval, // wont be freed
  int (*get_fn)(mpfr_ptr, mpfi_srcptr));


PMATH_PRIVATE void _pmath_intervals_memory_panic(void);

PMATH_PRIVATE pmath_bool_t _pmath_intervals_init(void);
PMATH_PRIVATE void         _pmath_intervals_done(void);

#endif // __PMATH_CORE__INTERVAL_PRIVATE_H__
