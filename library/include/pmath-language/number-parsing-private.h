#ifndef __PMATH_LANGUAGE__NUMBER_PARSING_PRIVATE_H__
#define __PMATH_LANGUAGE__NUMBER_PARSING_PRIVATE_H__

#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

/** \defgroup float-format The format of floating point numbers.
    
    pMath represents arbitrary precision numbers as real balls with a midpoint, a zero or
    posivive radius and a working precision. The general format for such numbers is e.g.
    \code
16^^1a.b23[+/-3b.7*^-2]`10.2*^-15
    \endcode
    which represents the closed interval with midpoint `(1*16 + 10 + 11/16 + 2/16^2 + 3/16^3) * 16^-15`,
    radius `(3*16 + 12 + 7/16) * 16^-17` and working precision of `10.2 * ln(16)/ln(2)` bits.
    No 
    
    <ul>
      <li> The base specifier (<tt>16^^</tt>) is optional. It defaults to 10 but may be any integer between 2 and 36.
      <li> The mantissa (<tt>1a.b23</tt>) is mandatory. It is a string of digits in the given base.
      <li> The radius specification (<tt>[+-3b.7*^-2]</tt>) is again optional.
           If specified, it starts with an opening square bracket <tt>[</tt>, followed by either the string
           <tt>+/-</tt> or the unicode PLUS-MINUS SIGN character U+00B1, followed by a string of digits (<tt>3b.7</tt>) 
           in the given base, floowed by an optional exponent specifier (<tt>*^-2</tt>).
      <li> The precision specifier (<tt>`10.2</tt>) is also optional. 
           It consitst of a starting <tt>`</tt> followed by a decimal integer or floating point number.
           It defaults to the number of digits of the mantissa component (ignoring any initial 0 or 
           trailing ".0").
           
           Alternatively, the precision specifier may consist of only a single <tt>`</tt> character to
           denote machine precision. In that case, the radius should be omitted/is ignored.
       <li> The overal exponent (<tt>*^-15</tt>) wrt. the given base applies to both, mantissa and radius.
            It is optional and defaults to zero. It starts with <tt>*^</tt>, followed by an optional minus sign,
            followed by a decimal integer.
    </ul>
  @{
 */

#include <pmath-core/numbers-private.h>
#include <pmath-core/strings.h> // only for typedef pmath_string_t

/** \brief Calculate the binary logarithm of a positive integer.
    \param b The integer to take the logarithm of.
    \return The exact result if \a b is a power of 2 between 2 and 32, otherwise <c>log(b)/log(2)</c>.
 */
PMATH_PRIVATE double _pmath_log2_of(int b);

struct _pmath_real_ball_parts_t {
  fmpz_t       midpoint_mantissa;
  fmpz_t       midpoint_exponent;
  fmpz_t       radius_mantissa;
  fmpz_t       radius_exponent;
  double       precision_in_base; // -HUGE_VAL for machine precision, +HUGE_VAL for exact numbers
  int          base;
};

PMATH_PRIVATE void _pmath_real_ball_parts_init(struct _pmath_real_ball_parts_t *parts);
PMATH_PRIVATE void _pmath_real_ball_parts_clear(struct _pmath_real_ball_parts_t *parts);

/** \brief Parse a pMath number (real ball) to its components.
    \param result                Where to store the resulting parts. Must already be initialized with _pmath_real_ball_parts_init().
    \param str                   The number string.
    \param str_end               (optional) The end of the string. If this is NULL, \a str is assumed to be zero-terminated.
    \param default_min_precision The default precision if no explicit precision is specified and there are only a few digits given.
    \return The end of the parsed number, i.e. the positition of a syntax error or end of the string if no error occurs.
    
    If \a str does not specify a precision, then \a result.precision_in_base will be calculated as follows:
    - If \a str contains no decimal dot, \a result.precision_in_base will be `HUGE_VAL`.
    - If \a default_min_precision is `-HUGE_VAL` and less than `DBL_MANT_DIG * log(2) / log(out_base)` significant
      digits are given, then \a result.precision_in_base will be set to `-HUGE_VAL` if no radius specification was given, 
      and to `DBL_MANT_DIG * log(2) / log(out_base)` is an explicit radius specification (including [+/-0]) was given.
    - Otherwise, if less than `default_min_precision * log(2) / log(out_base)` significant digits are given, then
      \a result.precision_in_base will be set to `default_min_precision * log(2) / log(out_base)`.
    - Otherwise, \a result.precision_in_base will be set to the number of significant digits.
 */
PMATH_PRIVATE
const uint16_t *_pmath_parse_real_ball(
  struct _pmath_real_ball_parts_t *result,
  const uint16_t                  *str,
  const uint16_t                  *str_end,
  double                           default_min_precision);

/** \brief Create an Arb real ball, exact number or machine precision from midpoint and possibly radius.
    \param mid The midpoint. Will be freed.
    \param rad The radius. Will be freed.
    Note that \a rad is only considered, if both \a mid and \a rad are floating point numbers.
    If \a rad is non-zero and \a mid is a machine precision float (\see pmath_is_double), then
    an arbitrary precision float with DBL_MANT_DIG bits working precision is returned instead of
    a double.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_real_ball_from_midpoint_radius(pmath_t mid, pmath_t rad);
  
/** \brief Calculate mantissa*base^exponent.
    \param mantissa         An integer.
    \param exponent         An integer.
    \param base             An integer between 2 and 36.
    \param precision_digits The precision in \a base digits.
    
    The \a precision_digits may be `-HUGE_VAL` to obtain machine precision numbers, 
    or `+HUGE_VAL` to obtain an expression of integers, or a positive real number 
    to obtain a `pmath_mpfloat_t` with `precision_digits * log(2) / log(base)` bits
    of working precision.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_compose_number(
  fmpz_t  mantissa,
  fmpz_t  exponent,
  int     base,
  double  precision_digits);


/** \brief Shortcut for _pmath_parse_real_ball() followed by _pmath_compose_number() and _pmath_real_ball_from_midpoint_radius()
    \param string The number string. It will be freed.
    \return PMATH_NULL on error 
 */
PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_parse_number(pmath_string_t string);
  
/** \brief Convert an arf_t to a mag_t.
    \param y An initialized magnitude.
    \param x A valid floating point number.
    
    This function is like arf_get_mag() but unlike the latter, does not add an ulp to \a y 
    if \a x fits into a mag_t exactly (i.e. if \a x consists of a signle limb with at most MAG_BITS set).
 */
PMATH_PRIVATE
void _pmath_arf_get_mag_exact(mag_t y, const arf_t x);

/** \brief Increase an Arb radius by a given error.
    \param x The Arb number whose radius is to be increased.
    \param err An Arb number, whose absolute value should be added to the radius of \a x.
    
    Unlike arb_add_error(), this function uses _pmath_arf_get_mag_exact() instead of arf_get_mag() 
    in order to avoid increasing the radius unconditionally by 1 ulp.
 */
PMATH_PRIVATE
void _pmath_arb_add_error_exact(arb_t x, const arb_t err);

/** @} */

#endif // __PMATH_LANGUAGE__NUMBER_PARSING_PRIVATE_H__
