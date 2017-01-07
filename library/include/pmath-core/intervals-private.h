#ifndef __PMATH_CORE__INTERVAL_PRIVATE_H__
#define __PMATH_CORE__INTERVAL_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/intervals.h>
#include <pmath-core/numbers-private.h>

#include <mpfi.h>


struct _pmath_interval_t {
  struct _pmath_t  inherited;
  mpfi_t           value;
};

#define PMATH_AS_MP_INTERVAL(obj)  (((struct _pmath_interval_t*) PMATH_AS_PTR(obj))->value)


/** Create a new interval object with given precision.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_interval_t _pmath_create_interval(mpfr_prec_t precision);

/** Obtain a new interval object compatible with a given one.
    \param val A non-NULL interval object. It won't be freed.
    \return A writable interval object, possibly \a val itself.
    
    If \a val has a reference count of 1, then its reference count is increased to 2 and it is returned.
    Otherwise, a new interval object with same precision as \a val will be returned.
    
    You can use the returned object migh be an alias for \a val, you can only use \a val once
    in a call like mpfi_add(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(val), other) but not afterwards.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_interval_t _pmath_create_interval_for_result(pmath_interval_t val);

/** Obtain a new interval object compatible with a given one and precision
    \param val            A non-NULL interval object. It won't be freed.
    \param min_precision  Minimum precision of the returned interval.
    \return A writable interval object, possibly \a val itself.
    
    If \a val has a reference count of 1 and its precision is at least \a min_precision, 
    then its reference count is increased to 2 and it is returned.
    Otherwise, a new interval object with same precision as \a val will be returned.
    
    You can use the returned object migh be an alias for \a val, you can only use \a val once
    in a call like mpfi_add(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(val), other) but not afterwards.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_interval_t _pmath_create_interval_for_result_with_prec(pmath_interval_t val, mp_prec_t min_precision);

PMATH_PRIVATE
pmath_bool_t _pmath_interval_set_point(
  mpfi_ptr result,
  pmath_t  value);  // wont be freed

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_interval_exceptions(pmath_interval_t x);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_interval_get_value(
  pmath_interval_t   interval, // wont be freed
  int              (*get_fn)(mpfr_ptr, mpfi_srcptr));

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_interval_call(
  pmath_interval_t   arg, // will be freed
  int              (*func)(mpfi_ptr, mpfi_srcptr));


PMATH_PRIVATE void _pmath_intervals_memory_panic(void);

PMATH_PRIVATE pmath_bool_t _pmath_intervals_init(void);
PMATH_PRIVATE void         _pmath_intervals_done(void);

#endif // __PMATH_CORE__INTERVAL_PRIVATE_H__
