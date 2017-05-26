#ifndef __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__
#define __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/numbers-private.h>
#include <pmath-core/strings.h>

struct _pmath_number_string_parts_t {
  int            base;
  pmath_bool_t   is_negative;
  pmath_string_t midpoint_fractional_mantissa_digits; // contains decimal dot, but no sign
  pmath_string_t exponent_decimal_digits; // may start with minus sign
  pmath_string_t radius_fractional_mantissa_digits; // contains decimal dot, but no sign
  pmath_string_t radius_exponent_part_decimal_digits; // may start with minus sign
  pmath_string_t precision_decimal_digits;
};

PMATH_PRIVATE
void _pmath_mpfloat_get_string_parts(
  struct _pmath_number_string_parts_t *result,
  pmath_mpfloat_t                      value,
  int                                  base,
  int                                  max_digits,
  pmath_bool_t                         allow_inaccurate_digits);

#endif // __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__
