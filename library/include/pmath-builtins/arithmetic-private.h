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


#define PMATH_UNKNOWN_REAL_SIGN  (2)

/** \brief Try to get the sign of a real numeric number.
    \param x A numeric expression. It won't be freed.
    \return \c PMATH_UNKNOWN_REAL_SIGN if the sign could not be determined or is complex, 
            otherwise the real sign (-1,0,1) of \a x.
 */
PMATH_PRIVATE
int _pmath_numeric_sign(pmath_t x);

// If *z == x * ImaginaryI => *z:= x
PMATH_PRIVATE
pmath_bool_t _pmath_is_imaginary(
  pmath_t *z);
  
/** \brief Convert a real or complex floating point number to an Arb complex ball.
    \param result          An initialized Arb complex ball reference to take the value.
    \param precision       Optional pointer to an slong taking the working precision of \a complex.
    \param is_machine_prec Optional pointer to a boolean taking whether \a complex is machine precision.
    \param complex         A real or complex number. It won't be freed.
    \return Whether the conversion was successfull.
 */
PMATH_PRIVATE
pmath_bool_t _pmath_complex_float_extract_acb(
  acb_t         result, 
  slong        *precision, 
  pmath_bool_t *is_machine_prec, 
  pmath_t       complex);

/** \brief Convert a real or complex number to an Arb complex ball, approximating to a given precision if necessary.
    \param result     An initialized Arb complex ball reference to take the value.
    \param complex    A real or complex number. It won't be freed.
    \param precision  The precision to use for converting exact to floating point numbers.    
    \return Whether the conversion was successfull.
 */
PMATH_PRIVATE
pmath_bool_t _pmath_complex_float_extract_acb_for_precision(
  acb_t         result, 
  pmath_t       complex,
  slong         precision);

/** \brief Create a floating point real or complex number object from an Arb complex ball.
    \param value           A valid Arb complex ball. It will be cleared.
    \param prec_or_double The working precision or a negative value to get machine floating point numbers.
    \return A new pMath object.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_complex_new_from_acb_destructive(acb_t value, slong prec_or_double);

PMATH_PRIVATE
void _pmath_split_summand(
  pmath_t  summand,         // wont be freed
  pmath_t *out_num_factor,  // may also become a complex number
  pmath_t *out_rest);

/** \brief Evaluate a function by converting degrees to radians.
    \param expr  Pointer to the function expression. On success, this will be replaced by the evaluation result.
    \param x     The function argument. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_PRIVATE
pmath_bool_t _pmath_try_simplify_degree(pmath_t *expr, pmath_t x);

PMATH_PRIVATE
pmath_bool_t _pmath_to_precision(
  pmath_t   obj, // wont be freed
  double   *result); // precision in bits

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_from_precision(double prec_bits);


#endif /* __PMATH_BUILTINS__ARITHMETIC_PRIVATE_H__ */
