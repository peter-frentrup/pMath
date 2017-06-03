#include <pmath-language/number-parsing-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>

#include <pmath-language/tokens.h>


static int digit_value(uint16_t ch) {
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  if(ch >= 'a' && ch <= 'z')
    return ch - 'a' + 10;
  if(ch >= 'A' && ch <= 'Z')
    return ch - 'A' + 10;
  return -1;
}

/** \brief Parse a base specifier (if any).
    \param out_base Receives the base. It is always between 2 and 36.
    \param str      The string starting possibly with a base specifier (<tt>d^^</tt> or <tt>dd^^</tt>).
    \param str_end  The string end.
    \return Position of the first character after the base specifier or \a str if there was no such
            specifier in the allowed range. In the latter case, \a out_base will be set to 10.
 */
static const uint16_t *parse_base(
  int            *out_base,
  const uint16_t *str,
  const uint16_t *str_end
) {
  const uint16_t *start = str;
  int base;
  
  assert(out_base != NULL);
  assert(str != NULL);
  assert(str_end != NULL);
  
  if(str + 2 < str_end && str[2] == '^') { // d^^... or dd^^...
    base = 0;
    
    while(str + 2 <= str_end && pmath_char_is_digit(*str)) {
      base = base * 10 + (*str - '0');
      ++str;
    }
    
    if(str + 2 < str_end && str[0] == '^' && str[1] == '^' && base >= 2 && base <= 36) {
      *out_base = base;
      return str + 2;
    }
  }
  else if(str + 2 < str_end && str[0] == '0' && str[1] == 'x') {
    *out_base = 16;
    return str + 2;
  }
  
  *out_base = 10;
  return start;
}

static const uint16_t *parse_integer_remainder(
  fmpz_t inout_value,
  const uint16_t *str,
  const uint16_t *str_end,
  int             base
) {
  assert(str != NULL);
  assert(str_end != NULL);
  assert(base >= 2 && base <= 36);
  
  while(str < str_end) {
    int val = digit_value(*str);
    if(val < 0 || val >= base)
      return str;
      
    fmpz_mul_si(inout_value, inout_value, base);
    fmpz_add_si(inout_value, inout_value, val);
    ++str;
  }
  return str;
}

/** \brief Parse a string of the form "ddd.ddd" or "ddd" to a floating point number.
 */
static const uint16_t *parse_simple_float(
  fmpz_t          out_mantissa,
  ulong          *out_frac_digits,
  ulong          *out_significant_digits,
  pmath_bool_t   *inout_is_floating_point,
  const uint16_t *str,
  const uint16_t *str_end,
  int             base
) {
  const uint16_t *int_end;
  
  assert(out_frac_digits != NULL);
  assert(out_significant_digits != NULL);
  assert(inout_is_floating_point != NULL);
  assert(str != NULL);
  assert(str_end != NULL);
  assert(base >= 2 && base <= 36);
  
  fmpz_zero(out_mantissa);
  int_end = parse_integer_remainder(out_mantissa, str, str_end, base);
  
  *out_significant_digits = int_end - str;
  *out_frac_digits = 0;
  
  if(int_end < str_end && *int_end == '.') {
    const uint16_t *frac_end = parse_integer_remainder(out_mantissa, int_end + 1, str_end, base);
    
    if(frac_end == str + 1) // do not acept sole "." without any digit
      return int_end;
      
    *inout_is_floating_point = TRUE;
    
    *out_frac_digits = frac_end - (int_end + 1);
    
    *out_significant_digits += *out_frac_digits;
    if(*str == '0') // leading 0
      --*out_significant_digits;
    else if(*out_frac_digits == 1 && int_end[1] == '0') // trailing .0
      --*out_significant_digits;
      
    return frac_end;
  }
  
  return int_end;
}

static const uint16_t *parse_exponent(
  fmpz_t          out_exponent,
  const uint16_t *str,
  const uint16_t *str_end,
  int             base
) {
  const uint16_t *start = str;
  const uint16_t *end;
  pmath_bool_t negative;
  
  assert(str != NULL);
  assert(str_end != NULL);
  
  fmpz_zero(out_exponent);
  if(str + 1 < str_end && str[0] == '*' && str[1] == '^') {
    str += 2;
  }
  else if(str < str_end && (*str == 'e' || *str == 'E'/* || *str == 'p' || *str == 'P'*/)) {
    /* TODO: like C/C++, base 16 should treat P+ddd as exponent in power of 2 instead of 16. 
       So we need to divide out_exponent by 4 and return a correction factor (exp mod 4)
     */
    if(base == 10)
      ++str;
    else
      return str;
  }
  else
    return str;
    
  if(str < str_end && *str == '-') {
    negative = TRUE;
    ++str;
  }
  else
    negative = FALSE;
    
  end = parse_integer_remainder(out_exponent, str, str_end, 10);
  if(end == str)
    return start;
    
  if(negative)
    fmpz_neg(out_exponent, out_exponent);
    
  return end;
}

/** \brief Parse a radius specification.

    A radius specification is a string of the form <tt>[+/-xxx.xxx*^ddd]</tt> with optional decimal
    exponent <tt>*^ddd</tt> or <tt>*^-ddd</tt> and mantissa <tt>xxx.xxx</tt> given as \a base digits.
 */
static const uint16_t *parse_radius(
  fmpz_t          out_radius_mantissa,
  fmpz_t          out_radius_exponent,
  pmath_bool_t   *inout_is_floating_point,
  const uint16_t *str,
  const uint16_t *str_end,
  int             base
) {
  const uint16_t *start = str;
  ulong frac_digits;
  ulong significant_digits;
  
  assert(inout_is_floating_point != NULL);
  assert(str != NULL);
  assert(str_end != NULL);
  assert(base >= 2 && base <= 36);
  
  if(str >= str_end || *str != '[')
    goto FAIL;
    
  ++str;
  if(str < str_end && *str == PMATH_CHAR_PLUSMINUS)
    ++str;
  else if(str + 2 < str_end && str[0] == '+' && str[1] == '/' && str[2] == '-')
    str += 3;
  else
    goto FAIL;
    
  str = parse_simple_float(
          out_radius_mantissa,
          &frac_digits,
          &significant_digits,
          inout_is_floating_point,
          str,
          str_end,
          base);
          
  if(significant_digits == 0)
    goto FAIL;
    
  str = parse_exponent(out_radius_exponent, str, str_end, base);
  if(str < str_end && *str == ']') {
    fmpz_sub_ui(out_radius_exponent, out_radius_exponent, frac_digits);
    return str + 1;
  }
  
FAIL:
  fmpz_zero(out_radius_mantissa);
  fmpz_zero(out_radius_exponent);
  return start;
}

PMATH_PRIVATE double _pmath_log2_of(int b) {
  if(b == 2)
    return 1.0;
  if(b == 4)
    return 2.0;
  if(b == 8)
    return 3.0;
  if(b == 16)
    return 4.0;
  if(b == 32)
    return 5.0;
  return log((double)b) / LOGE_2;
}

PMATH_PRIVATE void _pmath_real_ball_parts_init(struct _pmath_real_ball_parts_t *parts) {
  assert(parts != NULL);
  fmpz_init(parts->midpoint_mantissa);
  fmpz_init(parts->midpoint_exponent);
  fmpz_init(parts->radius_mantissa);
  fmpz_init(parts->radius_exponent);
  parts->precision_in_base = 0.0;
  parts->base = 0;
}

PMATH_PRIVATE void _pmath_real_ball_parts_clear(struct _pmath_real_ball_parts_t *parts) {
  assert(parts != NULL);
  fmpz_clear(parts->midpoint_mantissa);
  fmpz_clear(parts->midpoint_exponent);
  fmpz_clear(parts->radius_mantissa);
  fmpz_clear(parts->radius_exponent);
}

PMATH_PRIVATE
const uint16_t *_pmath_parse_real_ball(
  struct _pmath_real_ball_parts_t *result,
  const uint16_t                  *str,
  const uint16_t                  *str_end,
  double                           default_min_precision
) {
  const uint16_t *mid_mant_start;
  ulong mid_frac_digits;
  ulong mid_significant_digits;
  pmath_bool_t is_floating_point;
  
  assert(result != NULL);
  assert(str != NULL);
  
  if(str_end == NULL)
    str_end = (const uint16_t*)((uint64_t)0 - (uint64_t)1);
    
  str = parse_base(&result->base, str, str_end);
  assert(result->base >= 2 && result->base <= 36);
  
  is_floating_point = FALSE;
  mid_mant_start = str;
  str = parse_simple_float(
          result->midpoint_mantissa,
          &mid_frac_digits,
          &mid_significant_digits,
          &is_floating_point,
          str,
          str_end,
          result->base);
          
  if(str == mid_mant_start) {
    fmpz_zero(result->midpoint_exponent);
    fmpz_zero(result->radius_mantissa);
    fmpz_zero(result->radius_exponent);
    result->precision_in_base = 0.0;
    return str;
  }
  
  str = parse_radius(
          result->radius_mantissa,
          result->radius_exponent,
          &is_floating_point,
          str,
          str_end,
          result->base);
          
  if(str < str_end && *str == '`') {
    const uint16_t *prec_start = str + 1;
    slong prec_frac;
    slong prec_significant;
    
    fmpz_t prec_mant;
    fmpz_init(prec_mant);
    
    str = parse_simple_float(prec_mant, &prec_frac, &prec_significant, &is_floating_point, prec_start, str_end, 10);
    if(str == prec_start)
      result->precision_in_base = -HUGE_VAL;
    else
      result->precision_in_base = fmpz_get_d(prec_mant) / pow(10.0, (double)prec_frac);
      
    is_floating_point = TRUE;
    fmpz_clear(prec_mant);
  }
  else if(is_floating_point) {
    if(default_min_precision == -HUGE_VAL) {
      if(mid_significant_digits < DBL_MANT_DIG / _pmath_log2_of(result->base))
        result->precision_in_base = -HUGE_VAL;
      else
        result->precision_in_base = (double)mid_significant_digits;
    }
    else {
      double min_digits = default_min_precision / _pmath_log2_of(result->base);
      if(mid_significant_digits < min_digits)
        result->precision_in_base = min_digits;
      else
        result->precision_in_base = (double)mid_significant_digits;
    }
  }
  else
    result->precision_in_base = HUGE_VAL;
    
  str = parse_exponent(result->midpoint_exponent, str, str_end, result->base);
  fmpz_add(result->radius_exponent, result->radius_exponent, result->midpoint_exponent);
  fmpz_sub_ui(result->midpoint_exponent, result->midpoint_exponent, mid_frac_digits);
  return str;
}

/** \brief Set \a result to mantissa*base^exponent.
 */
static void compose_arb(arb_t result, fmpz_t mantissa, fmpz_t exponent, int base, slong prec) {
  arb_t tmp;
  assert(base > 0);
  
  if(fmpz_is_zero(mantissa)) {
    arb_zero(result);
    return;
  }
  if(fmpz_is_zero(exponent)) {
    arb_set_round_fmpz(result, mantissa, prec);
    return;
  }
  
  arb_init(tmp);
  arb_set_ui(tmp, (ulong)base);
  arb_set_fmpz(result, mantissa);
  
  if(fmpz_sgn(exponent) > 0) {
    arb_pow_fmpz_binexp(tmp, tmp, exponent, prec + 6);
    arb_mul(result, result, tmp, prec);
  }
  else {
    fmpz_neg(exponent, exponent);
    arb_pow_fmpz_binexp(tmp, tmp, exponent, prec + 6);
    arb_div(result, result, tmp, prec);
    fmpz_neg(exponent, exponent);
  }
  
  arb_clear(tmp);
}

PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_compose_number(
  fmpz_t  mantissa,
  fmpz_t  exponent,
  int     base,
  double  precision_digits
) {
  assert(base > 0 && base <= 36);
  
  if(precision_digits == -HUGE_VAL) {
    pmath_t result = _pmath_compose_number(mantissa, exponent, base, DBL_MANT_DIG);
    if(pmath_is_mpfloat(result)) {
      double value = arf_get_d(arb_midref(PMATH_AS_ARB(result)), ARF_RND_NEAR);
      if(isfinite(value)) {
        pmath_unref(result);
        return PMATH_FROM_DOUBLE(value);
      }
    }
    return result;
  }
  
  if(precision_digits < HUGE_VAL) {
    double prec_bits = ceil(precision_digits * _pmath_log2_of(base));
    slong prec;
    pmath_mpfloat_t result;
    
    if(prec_bits > 2 && prec_bits < PMATH_MP_PREC_MAX)
      prec = (slong)prec_bits;
    else if(prec_bits >= PMATH_MP_PREC_MAX)
      prec = PMATH_MP_PREC_MAX;
    else
      prec = 2;
      
    result = _pmath_create_mp_float(prec);
    if(!pmath_is_null(result)) {
      compose_arb(PMATH_AS_ARB(result), mantissa, exponent, base, PMATH_AS_ARB_WORKING_PREC(result));
      arf_get_mpfr(PMATH_AS_MP_VALUE(result), arb_midref(PMATH_AS_ARB(result)), MPFR_RNDN);
    }
    return result;
  }
  
  return TIMES(_pmath_integer_from_fmpz(mantissa), POW(INT(base), _pmath_integer_from_fmpz(exponent)));
}

PMATH_PRIVATE
void _pmath_arf_get_mag_exact(mag_t y, const arf_t x) {
  if(ARF_SIZE(x) == 1) { // non-zero x with exactly one limb
    mp_limb_t limb, topmost;
    ARF_GET_TOP_LIMB(limb, x); // FLINT_BITS many bits in the limb,
    topmost = (limb >> (FLINT_BITS - MAG_BITS));
    if(limb == (topmost << (FLINT_BITS - MAG_BITS))) { // lower FLINT_BITS - MAG_BITS bits were 0
      fmpz_set(MAG_EXPREF(y), ARF_EXPREF(x));
      MAG_MAN(y) = topmost;
      return;
    }
  }
  arf_get_mag(y, x);
}

static void _pmath_arb_get_mag_exact(mag_t z, const arb_t x) {
  mag_t t;
  mag_init(t);
  _pmath_arf_get_mag_exact(t, arb_midref(x));
  mag_add(z, t, arb_radref(x));
  mag_clear(t);
}

PMATH_PRIVATE
void _pmath_arb_add_error_exact(arb_t x, const arb_t err) {
  mag_t u;
  
  if(arb_is_zero(err))
    return;
    
  if(mag_is_zero(arb_radref(x))) {
    _pmath_arb_get_mag_exact(arb_radref(x), err);
    return;
  }
  
  mag_init(u);
  _pmath_arb_get_mag_exact(u, err);
  mag_add(arb_radref(x), arb_radref(x), u);
  mag_clear(u);
}
