#ifndef __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__
#define __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/numbers-private.h>
#include <pmath-core/strings.h>

/** \addtogroup float-format 
  @{
 */

/** \brief Represents the variable parts that constitute an arbitrary precision real ball.

  For example, the number "16^^1a.b23[+-3b.7*^-2]`10.2*^-15" consists of the parts
  - \see is_negative = FALSE
  - \see base = 16
  - \see midpoint_fractional_mantissa_digits = "1a.b23"
  - \see radius_fractional_mantissa_digits = "3b.7"
  - \see radius_exponent_part_decimal_digits = "-2"
  - \see precision_decimal_digits = "10.2"
  - \see exponent_decimal_digits = "-15"
 */
struct _pmath_number_string_parts_t {
  /** \brief Wether to prepend the number with a minus sign.
   */
  pmath_bool_t is_negative;
  
  /** \brief The number's radix. Between 2 and 36. 
    
    If it is not 10, then the base has to prepend the number string as "base^^" written in decimal.
   */
  int base;
  
  /** \brief The midpoint's digits in the given base, including he decimal point.
   */
  pmath_string_t midpoint_fractional_mantissa_digits;
  
  /** \brief The radius' digits in the given base, including he decimal point.
   */
  pmath_string_t radius_fractional_mantissa_digits;
  
  /** \brief The additional exponent for the radius as a decimal integer string.
      
      This may start with a minus sign. The exponent is an integer in decimal notation, 
      e.g. "256" instead of "ff" for base 16.
      
      If the exponent is zero, this is empty instead of "0".
      The exponent for the radius is the sum of this additional radius and the overall radius.
   */
  pmath_string_t radius_exponent_part_decimal_digits;
  
  /** \brief The precision specifier as a decimal floating point number.
   */
  pmath_string_t precision_decimal_digits;
  
  /** \brief The overall exponent w.r.t. the given base as a decimal integer string.
      
      This may start with a minus sign. The exponent is an integer in decimal notation, 
      e.g. "256" instead of "ff" for base 16.
      
      If the exponent is zero, this is empty instead of "0".
   */
  pmath_string_t exponent_decimal_digits;
};

enum {
  PMATH_BASE_FLAGS_BASE_MASK           =  0xFF,
  PMATH_BASE_FLAG_ALLOW_INEXACT_DIGITS = 0x100,
  PMATH_BASE_FLAG_ALL_DIGITS           = 0x200,
};

/** \brief Convert an arbitrary precision float to string.
    \param result     Where to store the resulting string's parts. May be uninitialized.
    \param value      The number to convert. Must not be PMATH_NULL. Won't be freed.
    \param max_digits The maximum number of digits for the midpoint's mantissa. See notes.
    \param base_flags Radix and formatting flags. 
                      The lower 8 bits specify the radix (must be between 2 and 36).
                      Can be combined with PMATH_BASE_FLAG_XXX values.
    
    If the \a value contains 0, i.e. the radius is larger than abs(midpoint), then \a max_digits 
    refers to the number digits for the radius' mantissa.
    
    The flag bits of \a base_flags have the following meaning:
    - If \a base_flags contains \see PMATH_BASE_FLAG_ALL_DIGITS, and the base is 16 (i.e. if
      `base_flags == 16 | PMATH_BASE_FLAG_ALL_DIGITS`), then all digits of both midpoint and radius
      are returned. Thse can be more than \a max_digits.
      This can be used to parse back the exact same number. [TODO: support other power-of-two bases]
    
    - If \a base_flags contains \see PMATH_BASE_FLAG_ALLOW_INEXACT_DIGITS, insignificant trailing 
      digits will be returned when the radius is larger base^(max_digits-1). 
      If this flag is not present, only significant digits (plus possibly a few guard digits) will be
      returned, which may be less than \a max_digits.
 */
PMATH_PRIVATE
void _pmath_mpfloat_get_string_parts(
  struct _pmath_number_string_parts_t *result,
  pmath_mpfloat_t                      value,
  int                                  max_digits,
  int                                  base_flags);

/** @} */

#endif // __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__
