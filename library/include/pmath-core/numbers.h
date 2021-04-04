#ifndef __PMATH_CORE__NUMBERS_H__
#define __PMATH_CORE__NUMBERS_H__

#include <pmath-core/objects-inline.h>

/**\defgroup numbers Numbers
   \brief Number objects in pMath.

   pMath supports arbitrary big integers and rational values, floating point
   numbers in machine precision or with automatic precision tracking and complex
   numbers (the latter are represented by ordinary pmath_expr_t, all other
   number types have their own internal representation).

   Note that in might be more convinient to use pmath_build_value() than the
   specialized constructors represented here, because the former supports
   Infinity and Undefined (NaN) values for C `double`s.

   The GNU Multiple Precision Library (http://gmplib.org/) is used for
   integer and rational arithmetic and the MPFR library (http://www.mpfr.org/)
   for floating point arithmetic.

  @{
 */

#define PMATH_MACHINE_PRECISION  0
#define PMATH_AUTO_PRECISION     1

/**\class pmath_number_t
   \extends pmath_t
   \brief The abstract Number class.

   Because pmath_integer_t is derived from pmath_number_t, you can use pMath
   integers wherever a pmath_number_t is accepted.

   \see objects
 */
typedef pmath_t pmath_number_t;

/**\class pmath_rational_t
   \extends pmath_number_t
   \brief The abstract Rational Number class.

   Because pmath_rational_t is derived from pmath_number_t, you can use pMath
   integers wherever a pmath_number_t is accepted.

   Use pmath_is_rational() to check for rationals.
 */
typedef pmath_number_t pmath_rational_t;

/**\class pmath_integer_t
   \extends pmath_rational_t
   \brief The Integer class.

   Because pmath_integer_t is derived from pmath_rational_t, you can use pMath
   integers wherever a pmath_rational_t is accepted.

   Use pmath_is_integer() to check for integers.
 */
typedef pmath_rational_t pmath_integer_t;

typedef pmath_rational_t pmath_mpint_t;

/**\class pmath_quotient_t
   \extends pmath_rational_t
   \brief The Quotient class.

   Because pmath_quotient_t is derived from pmath_rational_t, you can use pMath
   integers wherever a pmath_rational_t is accepted.

   Use pmath_is_quotient() to check for quotients.
 */
typedef pmath_rational_t pmath_quotient_t;

/**\class pmath_float_t
   \extends pmath_number_t
   \brief The Floating Point Number class.

   Because pmath_float_t is derived from pmath_number_t, you can use pMath
   integers wherever a pmath_number_t is accepted.

   Use pmath_is_float() to check for floating point numbers.

   There are two hidden implementations of floating point numbers in pMath. One
   operates on \c double values. The other uses MPFR for multiple precision
   numbers and provides automatic precision tracking.
 */
typedef pmath_number_t pmath_float_t;

typedef pmath_float_t pmath_mpfloat_t;

/*============================================================================*/

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ui32_slow(uint32_t ui);

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ui64_slow(uint64_t ui);

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_si64_slow(int64_t si);

/**\brief Create an integer object from an int32_t
   \memberof pmath_integer_t
   \param si An int32_t.
   \return A pMath integer with the specified value.
 */
#define pmath_integer_new_si32(si) PMATH_FROM_INT32(si)

/**\brief Create an integer object from an uint32_t
   \memberof pmath_integer_t
   \param ui An uint32_t
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ui32(uint32_t ui) {
  if(ui <= INT32_MAX)
    return PMATH_FROM_INT32((int32_t)ui);
  
  return pmath_integer_new_ui32_slow(ui);
}

/**\brief Create an integer object from an int64_t.
   \memberof pmath_integer_t
   \param si An int64_t value.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_si64(int64_t si) {
  if(si <= INT32_MAX && si >= INT32_MIN)
    return PMATH_FROM_INT32((int32_t)si);
  
  return pmath_integer_new_si64_slow(si);
}

/**\brief Create an integer object from an uint64_t.
   \memberof pmath_integer_t
   \param ui A uint64_t value.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ui64(uint64_t ui) {
  if(ui <= INT32_MAX)
    return PMATH_FROM_INT32((int32_t)ui);
  
  return pmath_integer_new_ui64_slow(ui);
}

/**\brief Create an integer object from an intptr_t.
   \memberof pmath_integer_t
   \param si An intptr_t value.
   \return A pMath integer with the specified value or PMATH_NULL.
   \hideinitializer
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_siptr(intptr_t si) {
  return PMATH_CONCAT(pmath_integer_new_si, PMATH_BITSIZE)(si);
}

/**\brief Create an integer object from an uintptr_t.
   \memberof pmath_integer_t
   \param ui A uintptr_t value.
   \return A pMath integer with the specified value or PMATH_NULL.
   \hideinitializer
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_uiptr(uintptr_t ui) {
  return PMATH_CONCAT(pmath_integer_new_ui, PMATH_BITSIZE)(ui);
}


/**\brief Create an integer object from a signed long.
   \memberof pmath_integer_t
   \param si A signed long int.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_slong(long si) {
  return PMATH_CONCAT(pmath_integer_new_si, PMATH_LONG_BITSIZE)(si);
}

/**\brief Create an integer object from an unsigned long.
   \memberof pmath_integer_t
   \param ui An unsigned long int.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ulong(unsigned long ui) {
  return PMATH_CONCAT(pmath_integer_new_ui, PMATH_LONG_BITSIZE)(ui);
}


/**\brief Create an integer object from a signed int.
   \memberof pmath_integer_t
   \param si A (signed) int.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_sint(int si) {
  return PMATH_CONCAT(pmath_integer_new_si, PMATH_INT_BITSIZE)(si);
}

/**\brief Create an integer object from an unsigned int.
   \memberof pmath_integer_t
   \param ui An unsigned int.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_uint(unsigned int ui) {
  return PMATH_CONCAT(pmath_integer_new_ui, PMATH_INT_BITSIZE)(ui);
}

/**\brief Create an integer object from a data buffer.
   \memberof pmath_integer_t
   \param count The number of words to be read.
   \param order The order of the words: 1 for most significant word first or -1
          for least significant first.
   \param size The size (in bytes) of a word.
   \param endian The byte order within each word: 1 for most significant byte
          first, -1 for least significant first, or 0 for the native endianness
          of the CPU.
   \param nails The most significant \a nails bits of each word are skipped.
          This can be 0 to use the full words.
   \param data The buffer to read from.
   \return A non-negative integer.

   \see GMPs mpz_import()
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_data(
  size_t       count,
  int          order,
  int          size,
  int          endian,
  size_t       nails,
  const void  *data);

/**\brief Create an integer object from a C String.
   \memberof pmath_integer_t
   \param str A string representing the value in base \a base.
   \param base The base.
   \return A pMath integer with the specified value or PMATH_NULL.

   See GMP's mpz_set_str for mor information about the parameters.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_str(
  const char *str,
  int base);

/*============================================================================*/

/**\brief Create a rational number.
   \memberof pmath_rational_t
   \param numerator The quotient's numerator. It will be freed.
   \param denominator The quotient's denominator. It will be freed.
   \return An integer, if \a denominator divides \a numerator or a quotient
           in canonical form otherwise. If denominator is zero, PMATH_NULL will be
           returned.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_rational_t pmath_rational_new(
  pmath_integer_t  numerator,
  pmath_integer_t  denominator);

/**\brief Get the numerator of a rational number.
   \memberof pmath_rational_t
   \param rational A rational number (integer or quotient). It wont be freed.
   \return A reference to the numerator of \a rational if it is a quotient or
           \a rational itself if it is an integer. You have to destroy the
           result e.g. with pmath_unref().
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_rational_numerator(pmath_rational_t rational);

/**\brief Get the denominator of a rational number.
   \memberof pmath_rational_t
   \param rational A rational number (integer or quotient). It wont be freed.
   \return A reference to the denominator of \a rational if it is a quotient or
           1 if it is an integer. You have to destroy the result e.g. with
           pmath_unref().
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_rational_denominator(pmath_rational_t rational);

/*============================================================================*/

typedef enum {
  PMATH_PREC_CTRL_AUTO         = 0,
  PMATH_PREC_CTRL_MACHINE_PREC = 1,
  PMATH_PREC_CTRL_GIVEN_PREC   = 2,
  PMATH_PREC_CTRL_GIVEN_ACC    = 3   // deprecated
} pmath_precision_control_t;

/**\brief Create a floating point number from a string.
   \memberof pmath_number_t
   \relates pmath_float_t
   \param str A C-string representing the value in a given \a base. It should
          have the form "ddd.ddd" or simply "ddd". An exponent can be appended
          with "ennn" or if \a base &ne; 10 with "@nnn".
   \param base The base between 2 and 36.
   \param precision_control flag for controling the precision.
   \param base_precision_accuracy given precision or accuracy. depending on the
          value of the above flag.
   \return a new pMath floating point number or PMATH_NULL on error or the 
           integer 0 (see below when this happens).

   \remarks
    \a precision_control may have one of the following values:
    - \c PMATH_PREC_CTRL_AUTO: \n
      The precision is specified by the number of digits given in str. It may
      result in a pMath machine float, mulit-precision float or integer. \n
      The value of \a base_precision_accuracy will be ignored.

    - \c PMATH_PREC_CTRL_MACHINE_PREC: \n
      The result is a pMath machine float. \n
      The value of \a base_precision_accuracy will be ignored.

    - \c PMATH_PREC_CTRL_GIVEN_PREC: \n
      If the number's value is 0, the \em integer 0 will be returned. \n
      The precision is given by \a base_precision_accuracy (interpreted in
      the given base).

    - \c PMATH_PREC_CTRL_GIVEN_ACC: \n
      \a base_precision_accuracy specifies the accuracy (the number of known
      \a base -digits after the point). The precision is calculated
      appropriately.

    For a multiprecision float `x &ne; 0` with absolute error `dx`, `accuracy` and 
    `precision`  are:

        accuracy  = -Log(base, dx)
        precision = -Log(base, dx / Abs(x))

    So `precision = accuracy + Log(base, Abs(x))`.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_number_t pmath_float_new_str(
  const char               *str, // digits.digits
  int                       base,
  pmath_precision_control_t precision_control,
  double                    base_precision_accuracy);

/** \brief Parse a simple floating point number to its mantissa and exponent.
    \param out_mantissa         Receives the mantissa, i.e the given number as if it had no decimal dot.
    \param out_factional_digits Receives the number of digits after the decimal dot or 0.
    \param str                  The floating point number of the form `ddd.ddd` or `ddd` with digits `d` in the given \a base.
    \param base                 The number base of the digits. May be between 2 and 36 (inclusive)
    \return The end in \a str of the parsed floating point number.
    
    Parsing stops at the first non-valid digit or non-digit (or decimal point if that is not followed by a valid digit).
    Hence, if \a out_factional_digits is 0 on output, \a str did not contain a decimal point before the end.
 */
PMATH_API
const char *pmath_rational_parse_floating_point(
  pmath_integer_t *out_mantissa,
  intptr_t        *out_factional_digits,
  const char      *str,
  int              base);

/*============================================================================*/

/**\brief Check whether a pMath integer is in range -2^31 .. 2^31-1.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for an int32_t.
 */
#define pmath_integer_fits_si32(integer)  pmath_is_int32(integer)

/**\brief Check whether a pMath integer is in range 0 .. 2^32-1.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for an uint32_t.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_integer_fits_ui32(pmath_integer_t integer);

/**\brief Check whether a pMath integer is in range -2^63 .. 2^63-1.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for an int64_t.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_integer_fits_si64(pmath_integer_t integer);

/**\brief Check whether a pMath integer is in range 0 .. 2^64-1.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for an uint64_t.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_integer_fits_ui64(pmath_integer_t integer);

/**\brief Check whether a pMath integer fits into an intptr_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough.
   \hideinitializer
 */
#define pmath_integer_fits_siptr(integer)  PMATH_CONCAT(pmath_integer_fits_si, PMATH_BITSIZE)(integer)

/**\brief Check whether a pMath integer fits into an uintptr_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough.
   \hideinitializer
 */
#define pmath_integer_fits_uiptr(integer)  PMATH_CONCAT(pmath_integer_fits_ui, PMATH_BITSIZE)(integer)

/**\brief Check whether a pMath integer fits into an slong.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough.
   \hideinitializer
 */
#define pmath_integer_fits_slong(integer) PMATH_CONCAT(pmath_integer_fits_si, PMATH_LONG_BITSIZE)(integer)

/**\brief Check whether a pMath integer fits into an ulong.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough.
   \hideinitializer
 */
#define pmath_integer_fits_ulong(integer) PMATH_CONCAT(pmath_integer_fits_ui, PMATH_LONG_BITSIZE)(integer)

/**\brief Convert a pMath integer to a signed long int.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.

   \see pmath_integer_fits_si32
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
int32_t pmath_integer_get_si32(pmath_integer_t integer);

/**\brief Convert a pMath integer to a unsigned long int.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.

   \see pmath_integer_fits_ui32
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
uint32_t pmath_integer_get_ui32(pmath_integer_t integer);

/**\brief Convert a pMath integer to an int64_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.

   \see pmath_integer_fits_si32
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
int64_t pmath_integer_get_si64(pmath_integer_t integer);

/**\brief Convert a pMath integer to a uint64_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.

   \see pmath_integer_fits_ui32
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
uint64_t pmath_integer_get_ui64(pmath_integer_t integer);

/**\brief Convert a pMath integer to a intptr_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.
   \hideinitializer

   \see pmath_integer_fits_siptr
 */
#define pmath_integer_get_siptr  PMATH_CONCAT(pmath_integer_get_si, PMATH_BITSIZE)

/**\brief Convert a pMath integer to a uintptr_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.
   \hideinitializer

   \see pmath_integer_fits_uiptr
 */
#define pmath_integer_get_uiptr  PMATH_CONCAT(pmath_integer_get_ui, PMATH_BITSIZE)

/**\brief Convert a pMath integer to a slong.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.
   \hideinitializer

   \see pmath_integer_fits_slong
 */
#define pmath_integer_get_slong  PMATH_CONCAT(pmath_integer_get_si, PMATH_LONG_BITSIZE)

/**\brief Convert a pMath integer to a ulong.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return The integer's value if it fits.
   \hideinitializer

   \see pmath_integer_fits_ulong
 */
#define pmath_integer_get_ulong  PMATH_CONCAT(pmath_integer_get_ui, PMATH_LONG_BITSIZE)

/**\brief Convert a pMath number to a double.
   \memberof pmath_number_t
   \param number A pMath number. It wont be freed.
   \return The number's value if it fits.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
double pmath_number_get_d(pmath_number_t number);

/*============================================================================*/

/**\brief Get a number's sign.
   \memberof pmath_number_t
   \param num A pMath number. It wont be freed.
   \return +1 if the number is positive, -1 if it is negative, 0 otherwise (if 
           it is zero or if it is a real ball containing zero).
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
int pmath_number_sign(pmath_number_t num);

/**\brief Get a number's negative.
   \memberof pmath_number_t
   \param num A pMath number. It will be freed, do not use it afterwards.
   \return -num
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_number_t pmath_number_neg(pmath_number_t num);

#ifdef ACB_H


/** \brief Try to evaluate a function F(x) with floating point real or complex x.
    \param expr  Pointer to the F-expression. On success, this will be replaced by the evaluation result.
    \param x     The only argument of \a expr. It won't be freed.
    \param func  An function for evaluating F(x) with complex ball.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_API
pmath_bool_t pmath_complex_try_evaluate_acb(pmath_t *expr, pmath_t x, void (*func)(acb_t, const acb_t, slong));


/** \brief Try to evaluate a function F(x, y) with floating point real or complex x and/or y.
    \param expr  Pointer to the F-expression. On success, this will be replaced by the evaluation result.
    \param x     The first argument of \a expr. It won't be freed.
    \param y     The second argument of \a expr. It won't be freed.
    \param func  An function for evaluating F(x,y) with complex ball.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_API
pmath_bool_t pmath_complex_try_evaluate_acb_2(pmath_t *expr, pmath_t x, pmath_t y, void (*func)(acb_t, const acb_t, const acb_t, slong));


/** \brief Try to evaluate a function F(...) with floating point real or complex arguments.
    \param expr    Pointer to the F-expression. On success, this will be replaced by the evaluation result.
    \param args    The argument list. It won't be freed.
    \param func    An function for evaluating F(...) with complex ball.
    \param context An additionbal pointer argument to supply to \a func.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
PMATH_API
pmath_bool_t pmath_complex_try_evaluate_acb_ex(
  pmath_t *expr,
  pmath_t args, // won't be freed
  void (*func)(acb_t, const acb_ptr args, size_t nargs, slong prec, void *context),
  void *context);


#endif

/** @} */

#endif /* __PMATH_CORE__NUMBERS_H__ */
