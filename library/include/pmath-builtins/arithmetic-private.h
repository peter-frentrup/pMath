#ifndef __PMATH_BUILTINS__ARITHMETIC_PRIVATE_H__
#define __PMATH_BUILTINS__ARITHMETIC_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

/* This header exports all definitions of the sources in
   src/pmath-builtins/arithmetic/
 */

#include <gmp.h>
#include <mpfr.h>

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_number_t _add_nn(
  pmath_number_t numA,   // will be freed.
  pmath_number_t numB);  // will be freed.

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_number_t _mul_nn(
  pmath_number_t numA,   // will be freed.
  pmath_number_t numB);  // will be freed.

PMATH_PRIVATE
pmath_bool_t _pmath_is_infinite(pmath_t obj);

// NULL if obj is no DirectedInfinity:
PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_directed_infinity_direction(
  pmath_t obj); // wont be freed

PMATH_PRIVATE 
pmath_bool_t _pmath_re_im( // whether operation succeded
  pmath_t  z,   // will be freed
  pmath_t *re,  // optional output
  pmath_t *im); // optional output

// is z = Complex(a, b) with numbers a, b?
PMATH_PRIVATE 
pmath_bool_t _pmath_is_nonreal_complex(
  pmath_t z); // wont be freed

// If *z == x * I => *z:= x
PMATH_PRIVATE
pmath_bool_t _pmath_is_imaginary(
  pmath_t *z);

PMATH_PRIVATE 
void split_summand(
  pmath_t  summand,         // wont be freed
  pmath_t *out_num_factor,  // may also become a complex number
  pmath_t *out_rest
);


PMATH_PRIVATE 
pmath_bool_t _pmath_equals_rational(
  pmath_t obj,       // wont be freed
  int n, int d);

PMATH_PRIVATE 
pmath_bool_t _pmath_equals_rational_at(
  pmath_expr_t expr,  // wont be freed
  size_t i, 
  int n, int d);


PMATH_PRIVATE
pmath_bool_t _pmath_to_precision(
  pmath_t  obj, // wont be freed
  double         *result);

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT 
pmath_t _pmath_approximate_step(
  pmath_t obj, // will be freed
  double         prec, // -inf = MachinePrecision
  double         acc); // -inf = MachinePrecision

PMATH_PRIVATE extern double pmath_max_extra_precision;

#endif /* __PMATH_BUILTINS__ARITHMETIC_PRIVATE_H__ */
