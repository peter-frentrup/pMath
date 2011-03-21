#ifndef __PMATH_CORE__NUMBERS_H__
#define __PMATH_CORE__NUMBERS_H__

#include <pmath-core/objects-inline.h>
#include <stdlib.h>

/**\defgroup numbers Numbers
   \brief Number objects in pMath.
   
   pMath supports arbitrary big integers and rational values, floating point
   numbers in machine precision or with automatic precision tracking and complex
   numbers (the latter are represented by ordinary pmath_expr_t, all other
   number types have their own internal representation).
   
   Note that in might be more convinient to use pmath_build_value() than the
   specialized constructors represented here, because the former supports 
   Infinity and Undefined (NaN) values for C <tt>double</tt>s.
   
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
 */
typedef pmath_number_t pmath_rational_t;

/**\class pmath_integer_t
   \extends pmath_rational_t
   \brief The Integer class.
   
   Because pmath_integer_t is derived from pmath_rational_t, you can use pMath
   integers wherever a pmath_rational_t is accepted.
   
   The \ref pmath_type_t of integers is \c PMATH_TYPE_INTEGER.
 */
typedef pmath_rational_t pmath_integer_t;

typedef pmath_rational_t pmath_mpint_t;

/**\class pmath_quotient_t
   \extends pmath_rational_t
   \brief The Quotient class.
   
   Because pmath_quotient_t is derived from pmath_rational_t, you can use pMath
   integers wherever a pmath_rational_t is accepted.
   
   The \ref pmath_type_t of quotients is \c PMATH_TYPE_QUOTIENT.
 */
typedef pmath_rational_t pmath_quotient_t;

/**\class pmath_float_t
   \extends pmath_number_t
   \brief The Floating Point Number class.
   
   Because pmath_float_t is derived from pmath_number_t, you can use pMath
   integers wherever a pmath_number_t is accepted.
   
   The \ref pmath_type_t of floats is \c PMATH_TYPE_FLOAT.
   
   There are two hidden implementations of floating point numbers in pMath. One
   operates on \c double values. The other uses MPFR for multiple precision
   numbers and provides automatic precision tracking.
 */
typedef pmath_number_t pmath_float_t;

typedef pmath_float_t pmath_mpfloat_t;

/*============================================================================*/

/**\brief Create an integer object from a signed long.
   \memberof pmath_integer_t
   \param si A signed long int.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_slong(signed long int si);

/**\brief Create an integer object from an unsigned long.
   \memberof pmath_integer_t
   \param ui An unsigned long int.
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ulong(unsigned long int ui);

/**\brief Create an integer object from an int32_t
   \memberof pmath_integer_t
   \param si An int32_t.
   \return A pMath integer with the specified value.
 */

#define pmath_integer_new_si32(si) PMATH_FROM_INT32(si)


//#define pmath_integer_new_si32(si)  PMATH_FROM_INT32((int32_t)(si))

/**\brief Create an integer object from an uint32_t.
   \memberof pmath_integer_t
   \param si An uint32_t
   \return A pMath integer with the specified value or PMATH_NULL.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ui32(uint32_t ui){
  if(ui >> 31)
    return pmath_integer_new_ulong(ui);
  return PMATH_FROM_INT32((int32_t)ui);
}

PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_si64(int64_t si);

/**\brief Create an integer object from an size_t.
   \memberof pmath_integer_t
   \param size A size_t value.
   \return A pMath integer with the specified value or PMATH_NULL.
   
   Note that on Win64, sizeof(long) == 4, but sizeof(size_t) == 8.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_integer_t pmath_integer_new_ui64(uint64_t ui);

#define pmath_integer_new_siptr  PMATH_CONCAT(pmath_integer_new_si, PMATH_BITSIZE)
#define pmath_integer_new_uiptr  PMATH_CONCAT(pmath_integer_new_ui, PMATH_BITSIZE)

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
   
   \see GMP's mpz_import()
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

typedef enum{
  PMATH_PREC_CTRL_AUTO         = 0,
  PMATH_PREC_CTRL_MACHINE_PREC = 1,
  PMATH_PREC_CTRL_GIVEN_PREC   = 2,
  PMATH_PREC_CTRL_GIVEN_ACC    = 3
}pmath_precision_control_t;

/**\brief Create a floating point number from a string.
   \memberof pmath_number_t
   \relates pmath_float_t
   \param str A C-string representing the value in a given \a base. It should 
          have the form "ddd.ddd" or simply "ddd". An exponent can be appended
          with "ennn" or if \a base != 10 "@nnn".
   \param base The base between 2 and 36.
   \param precision_control flag for controling the precision.
   \param base_precision_accuracy given precinion or accuracy. depending on the 
          value of the above flag.
   \return a new pMath floating point number or PMATH_NULL on error or the integer 0 
           (see below when this happens).
   
   \remarks
     \a precision_control may have one of the following values:
      <ul>
       <li> \c PMATH_PREC_CTRL_AUTO: \n
         The precision is specified by the number of digits given in str. It may
         result in a pMath machine float, mulit-precision float or integer. \n
         The value of \a base_precision_accuracy will be ignored.
     
     
       <li> \c PMATH_PREC_CTRL_MACHINE_PREC: \n
         The result is a pMath machine float. \n
         The value of \a base_precision_accuracy will be ignored.
     
     
       <li> \c PMATH_PREC_CTRL_GIVEN_PREC: \n
         If the number's value is 0, the \em integer 0 will be returned. \n
         The precision is given by \a base_precision_accuracy (interpreted in 
         the given base).
     
     
       <li> \c PMATH_PREC_CTRL_GIVEN_ACC: \n
         \a base_precision_accuracy specifies the accuracy (the number of known 
         \a base -digits after the point). The precision is calculated 
         appropriately.
      </ul>
     
     For a multiprecision float <tt> x != 0 </tt> with absolute error \c dx, 
     \c accuracy and \c precision  are:
     
     \code
accuracy  = -Log(base, dx)
precision = -Log(base, dx / Abs(x))
     \endcode
     
     So <tt>precision = accuracy + Log(base, Abs(x))</tt>.
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_number_t pmath_float_new_str(
  const char *              str, // digits.digits
  int                       base,
  pmath_precision_control_t precision_control,
  double                    base_precision_accuracy);
  
/*============================================================================*/

/**\brief Find out whether a pMath integer fits into a signed long int.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for a signed long int.
 */
#define pmath_integer_fits_si32(integer)  pmath_is_int32(integer)

/**\brief Find out whether a pMath integer fits into a unsigned long int.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for a unsigned long int.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_integer_fits_ui32(pmath_integer_t integer);

/**\brief Find out whether a pMath integer fits into an int64_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for an int64_t.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_integer_fits_si64(pmath_integer_t integer);

/**\brief Find out whether a pMath integer fits into an uint64_t.
   \memberof pmath_integer_t
   \param integer A pMath integer. It wont be freed.
   \return TRUE iff the value is small enough for an uint64_t.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_integer_fits_ui64(pmath_integer_t integer);

#define pmath_integer_fits_siptr  PMATH_CONCAT(pmath_integer_fits_si, PMATH_BITSIZE)
#define pmath_integer_fits_uiptr  PMATH_CONCAT(pmath_integer_fits_ui, PMATH_BITSIZE)

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

#define pmath_integer_get_siptr  PMATH_CONCAT(pmath_integer_get_si, PMATH_BITSIZE)
#define pmath_integer_get_uiptr  PMATH_CONCAT(pmath_integer_get_ui, PMATH_BITSIZE)

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
   \return The number's sign (-1, 0 or 1)
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

/** @} */

#endif /* __PMATH_CORE__NUMBERS_H__ */
