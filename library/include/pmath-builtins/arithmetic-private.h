#ifndef __PMATH_BUILTINS__ARITHMETIC_PRIVATE_H__
#define __PMATH_BUILTINS__ARITHMETIC_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>

/* This header exports all definitions of the sources in
   src/pmath-builtins/arithmetic/
 */

#include <pmath-core/numbers-private.h>

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
pmath_integer_t _pmath_factor_gcd_int(
  pmath_integer_t *a,   // not PMATH_NULL!  never PMATH_NULL on output
  pmath_integer_t *b);  // not PMATH_NULL!  never PMATH_NULL on output

PMATH_PRIVATE
pmath_rational_t _pmath_factor_rationals(
  pmath_rational_t *a,   // not PMATH_NULL!  integer on successful output
  pmath_rational_t *b);  // not PMATH_NULL!  integer on successful output

PMATH_PRIVATE
PMATH_ATTRIBUTE_PURE
pmath_bool_t _pmath_is_infinite(pmath_t obj);

// PMATH_NULL if obj is no DirectedInfinity:
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_directed_infinity_direction(
  pmath_t obj); // wont be freed

PMATH_PRIVATE
pmath_bool_t _pmath_re_im( // whether operation succeded
  pmath_t  z,   // will be freed
  pmath_t *re,  // optional output
  pmath_t *im); // optional output

/**\brief Check if z = Complex(a, b) with numbers a, b
   \param z A pMath expression. It won't be freed.
   
   Note that despite this function's name, z = Complex(1.0, 0.0) will yield TRUE.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_PURE
pmath_bool_t _pmath_is_nonreal_complex_number(pmath_t z);

/**\brief Check if z = Complex(a, b) with numbers or intervals (RealInterval) a, b
   \param z A pMath expression. It won't be freed.
   
   Note that despite this function's name, z = Complex(1.0, 0.0) will yield TRUE.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_PURE
pmath_bool_t _pmath_is_nonreal_complex_interval_or_number(pmath_t z);
  
// If *z == x * I => *z:= x
PMATH_PRIVATE
pmath_bool_t _pmath_is_imaginary(
  pmath_t *z);

/** \brief Convert a real or complex number to an Arb complex ball.
    \param result          An initialized Arb complex ball reference to take the value.
    \param precision       Pointer to an slong taking the working precision of \a complex.
    \param is_machine_prec Pointer to a boolean taking whether \a complex is machine precision.
    \param complex         A real or complex number.
    \return Whether the conversion was successfull.
 */
PMATH_PRIVATE
pmath_bool_t _pmath_complex_float_extract_acb(
  acb_t         result, 
  slong        *precision, 
  pmath_bool_t *is_machine_prec, 
  pmath_t       complex);

/** \brief Create a floating point real or complex number object from an Arb complex ball.
    \param value           A valid Arb complex ball.
    \param prec_or_double The working precision or a negative value to get machine floating point numbers.
    \return A new pMath object.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_complex_new_from_acb(const acb_t value, slong prec_or_double);
  
PMATH_PRIVATE
void _pmath_split_summand(
  pmath_t  summand,         // wont be freed
  pmath_t *out_num_factor,  // may also become a complex number
  pmath_t *out_rest);


PMATH_PRIVATE
pmath_bool_t _pmath_to_precision(
  pmath_t   obj, // wont be freed
  double   *result); // precision in bits

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_from_precision(double prec_bits);


#endif /* __PMATH_BUILTINS__ARITHMETIC_PRIVATE_H__ */
