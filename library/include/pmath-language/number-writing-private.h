#ifndef __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__
#define __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-core/numbers-private.h>
#include <pmath-core/strings.h>

/** \addtogroup numbers 
  @{
 */

/**\brief Rounds a null-terminated string of digits in a given base to length at most n.
   \memberof pmath_mpfloat_t
   \param s The string of digits to round. It is overwritten in-place, truncating it as necessary.
            The input should not have a leading sign or leading zero digits, but can have trailing zero digits.
   \param shift      Set on output, see notes.
   \param error      Set on output, see notes.
   \param base       The number base, between 2 and 36.
   \param max_digits The mximum number of digits. Must be positive.
   \param rnd        The rounding mode. Can be ARF_RND_DOWN, ARF_RND_UP or ARF_RND_NEAR.

   Computes \a shift and \a error are set such that `int(input) = int(output) * base^shift + error`.
 */
PMATH_PRIVATE 
void _pmath_number_round_digits_inplace(
  char        *s, 
  mp_bitcnt_t *shift, 
  fmpz_t       error, 
  int          base, 
  slong        max_digits, 
  arf_rnd_t    rnd);

/**\brief Write a precision as a decimal string.
   \memberof pmath_mpfloat_t
 */
PMATH_PRIVATE
void _pmath_write_precision(
  double   precision_digits, 
  slong    precision_bits, 
  void   (*writer)(void*, const char*, int), 
  void    *ctx);


/**\class _pmath_raw_number_parts_t
   \brief Represents a number 0.mmmm[mmm+/-rrr]*B^EEE
   
   The "new" representation.
 */
struct _pmath_raw_number_parts_t {
  char         *mid_digits;
  char         *rad_digits;
  fmpz_t        exponent;
  fmpz_t        rad_exponent_extra; //< Only valid if `needs_radius_exponent`
  int           total_significant;
  int           total_insignificant;
  int           mid_leading_zeros;
  int           num_integer_digits; //< Initialized to 0, needs to be in sync with the exponent
  int           base;
  pmath_bool_t  is_negative;
  
  /** Whether the number can only be displayed with explicit radius exponent XX
      as in  0.mmmmm[+/-0.rrr*B^XX]*B^EE  or  0.mmmmm*B^EE+/-0.rrr*B^(XX+EE)
      Note that XX = -(number of significant digits) if the radius is > 0.
      This flag may be set if XX is too large to show so many digits.
   */
  pmath_bool_t  needs_radius_exponent;
};

/** Convert an arbitrary precision float to string.
    \memberof pmath_mpfloat_t
    \param result     Where to store the resulting string's parts. Should be uninitialized.
                      Must be freed with _pmath_raw_number_parts_clear().
    \param value      The number to convert. Must not be PMATH_NULL. Won't be freed.
    \param base       Radix between 2 and 36.
    \param rad_digits Maximum number of insignificant digits.
 */
PMATH_PRIVATE void _pmath_mpfloat_get_raw_number_parts(
  struct _pmath_raw_number_parts_t *result,
  pmath_mpfloat_t                   value,
  int                               base,
  int                               rad_digits);

/** Move the implied decimal point 
    \memberof _pmath_raw_number_parts_t
    \param parts              The number's parts as returned by _pmath_mpfloat_get_raw_number_parts().
    \param num_integer_digits The new position of the decimal point. 
                              Must be <= parts->total_significant.
                              Negative values cause the decimal point to move to the left, inserting
                              leading fractional zero digits after the decimal point.
 */
PMATH_PRIVATE 
void _pmath_raw_number_parts_set_decimal_point(struct _pmath_raw_number_parts_t *parts, int num_integer_digits);

/** Move the implied decimal point to an automatically chosen position
    \memberof _pmath_raw_number_parts_t
    \param parts  The number's parts as returned by _pmath_mpfloat_get_raw_number_parts().
 */
PMATH_PRIVATE 
void _pmath_raw_number_parts_set_decimal_point_automatic(struct _pmath_raw_number_parts_t *parts);

/** Free memory after using a number parts structure.
    \memberof _pmath_raw_number_parts_t
 */
PMATH_PRIVATE void _pmath_raw_number_parts_clear(struct _pmath_raw_number_parts_t *parts);

enum _pmath_number_part_t{
  PMATH_NUMBER_PART_BASE,
  PMATH_NUMBER_PART_SIGNIFICANT,               ///< MM.MMMM___  of  MM.MMMM(mmm+/-rrr) * base^EE  including the dot
  PMATH_NUMBER_PART_SIGNIFICANT_INT_DIGITS,    ///< MM________  of  MM.mmmm(mmm+/-rrr) * base^EE
  PMATH_NUMBER_PART_SIGNIFICANT_FRAC_DIGITS,   ///< ___MMMM___  of  mm.MMMM(mmm+/-rrr) * base^EE
  PMATH_NUMBER_PART_MID_INSIGNIFICANT_DIGITS,  ///< _______MMM  of  mm.mmmm(MMM+/-rrr) * base^EE
  PMATH_NUMBER_PART_EXPONENT,                  ///< exponent EE of  mm.mmmm(mmm+/-rrr) * base^EE  or empty if 0
  PMATH_NUMBER_PART_RADIUS_DIGITS,             ///< rrr
  PMATH_NUMBER_PART_RADIUS_DIGITS_1,           ///< r.rr
  PMATH_NUMBER_PART_RADIUS_EXTRA_EXPONENT,     ///< exponent XX s.t. number = mm.mmmmm * base^EE +/- 0.rrr * base^(EE + XX) = mm.mm[mmm+/-rrr]*base^EE   I.e. XX = -(number of fractional significant digits). Or empty if 0.
  PMATH_NUMBER_PART_RADIUS_EXTRA_EXPONENT_1,   ///< exponent YY s.t. number = mm.mmmmm * base^EE +/- r.rr * base^(EE + YY)   I.e. YY = -(1 + number of fractional significant digits). Or empty if 0.
  PMATH_NUMBER_PART_RADIUS_EXPONENT,           ///< exponent XX s.t. number = midpoint +/- 0.rrr * base^XX  or empty if 0
  PMATH_NUMBER_PART_RADIUS_EXPONENT_1,         ///< exponent YY s.t. number = midpoint +/- r.rr * base^YY   or empty if 0
};

/**\brief Write part of a number to a file/stream.
   \memberof _pmath_raw_number_parts_t
 */
PMATH_PRIVATE
void _pmath_write_number_part(
  const struct _pmath_raw_number_parts_t  *parts,
  enum _pmath_number_part_t                which,
  void                                   (*writer)(void*, const char*, int), 
  void                                    *ctx);

/** @} */

#endif // __PMATH_LANGUAGE__NUMBER_WRITING_PRIVATE_H__
