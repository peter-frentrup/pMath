#ifndef __PMATH_BUILTINS__NUMBER_THEORY_PRIVATE_H__
#define __PMATH_BUILTINS__NUMBER_THEORY_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/numbers.h>

/* This header exports all definitions of the sources in
   src/pmath-builtins/number-theory/
 */

// float or complex
PMATH_PRIVATE pmath_bool_t _pmath_is_inexact(pmath_t obj);

PMATH_PRIVATE int _pmath_number_class(pmath_t obj);

enum {
  PMATH_CLASS_ZERO      = 1 <<  0, // x = 0
  PMATH_CLASS_NEGSMALL  = 1 <<  1, // -1 < x < 0
  PMATH_CLASS_POSSMALL  = 1 <<  2, // 0 < x < 1
  PMATH_CLASS_NEGONE    = 1 <<  3, // x = -1
  PMATH_CLASS_POSONE    = 1 <<  4, // x = 1
  PMATH_CLASS_NEGBIG    = 1 <<  5, // -Infinity < x < -1
  PMATH_CLASS_POSBIG    = 1 <<  6, // 1 < x < Infinity
  PMATH_CLASS_NEGINF    = 1 <<  7, // x = -Infinity
  PMATH_CLASS_POSINF    = 1 <<  8, // x = Infinity
  PMATH_CLASS_CINF      = 1 <<  9, // x = a * Infinity (a != 0)
  PMATH_CLASS_UINF      = 1 << 10, // x = DirectedInfinity(0) (unknown direction)
  PMATH_CLASS_IMAGINARY = 1 << 11, // x is complex and Re(x) = 0
  PMATH_CLASS_OTCOMPLEX = 1 << 12, // x is complex and not real and not imaginary
  PMATH_CLASS_UNKNOWN   = 1 << 13, // x is unknown
  
  PMATH_CLASS_SMALL     = (PMATH_CLASS_NEGSMALL | PMATH_CLASS_POSSMALL),
  PMATH_CLASS_ABSONE    = (PMATH_CLASS_NEGONE   | PMATH_CLASS_POSONE),
  PMATH_CLASS_BIG       = (PMATH_CLASS_NEGBIG   | PMATH_CLASS_POSBIG),
  PMATH_CLASS_NEG       = (PMATH_CLASS_NEGSMALL | PMATH_CLASS_NEGONE | PMATH_CLASS_NEGBIG | PMATH_CLASS_NEGINF),
  PMATH_CLASS_POS       = (PMATH_CLASS_POSSMALL | PMATH_CLASS_POSONE | PMATH_CLASS_POSBIG | PMATH_CLASS_POSINF),
  PMATH_CLASS_REAL      = (PMATH_CLASS_ZERO | PMATH_CLASS_SMALL | PMATH_CLASS_ABSONE | PMATH_CLASS_BIG),
  PMATH_CLASS_COMPLEX   = (PMATH_CLASS_IMAGINARY | PMATH_CLASS_OTCOMPLEX),
  PMATH_CLASS_RINF      = (PMATH_CLASS_NEGINF | PMATH_CLASS_POSINF),
  PMATH_CLASS_INF       = (PMATH_CLASS_RINF | PMATH_CLASS_CINF | PMATH_CLASS_UINF),
  PMATH_CLASS_KNOWN     = (PMATH_CLASS_REAL | PMATH_CLASS_COMPLEX | PMATH_CLASS_INF)
};

//PMATH_PRIVATE pmath_t _pmath_assign_is_numeric(
//  pmath_t lhs,  // will be freed. form: IsNumeric(~?IsSymbol)
//  pmath_t rhs); // will be freed. form: True|False

PMATH_PRIVATE extern const uint16_t _pmath_primes16bit[];
PMATH_PRIVATE extern const int _pmath_primes16bit_count;

PMATH_PRIVATE pmath_bool_t _pmath_integer_is_prime(pmath_integer_t n);


/* called from pmath_[init|done]();
   in isnumeric.c
 */
PMATH_PRIVATE pmath_bool_t _pmath_numeric_init(void);
PMATH_PRIVATE void         _pmath_numeric_done(void);

/* called from pmath_numbers_[init|done]();
   in isprime.c
 */
PMATH_PRIVATE pmath_bool_t _pmath_primetest_init(void);
PMATH_PRIVATE void         _pmath_primetest_done(void);

#endif /* __PMATH_BUILTINS__NUMBER_THEORY_PRIVATE_H__ */
