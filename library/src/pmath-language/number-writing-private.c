#include <pmath-language/number-writing-private.h>
#include <pmath-language/number-parsing-private.h> // for _pmath_log2_of

static void pow_ui_fmpz(arb_t res, int base, const fmpz_t e, slong prec) {
  assert(base > 0);
  
  slong bits = fmpz_bits(e);
  
  arb_set_si(res, base);
  arb_pow_fmpz(res, res, e, prec + bits);
}

/** \brief Compute integers \a out_mid, \a out_rad, \a out_exp such that x is in [m-r, m+r]*base^e.
    \param out_mid An initialized fmpz integer to be set to the mid-point.
    \param out_rad An initialized fmpz integer to be set to the radius.
    \param out_exp An initialized fmpz integer to be set to the exponent.
    \param in_value A finite Arb real ball.
    \param in_base The base, between 2 and 36.
    \param in_num_digits The minimum number of digits for the larger of \a out_mid and \a out_rad.
                         This must be non-negative.

    If \a in_value is not finite or is exactly zero, \a out_mid, \a out_rad and \aout_exp will be set to zero.
    Otherwise, the larger of \a out_mid and \a out_rad will have \a in_num_digits plus a few guard digits.

    This function correpsonds to arb_get_fmpz_mid_rad_10exp() if in_base=10.
    If \a in_num_digits is too large, pMath might crash, because the underlying FLINT and Arb libraries
    do not allow to gracefully handle out-of-memory situations.
 */
static void get_fmpz_mid_rad_basis_exp(
  fmpz_t      out_mid,
  fmpz_t      out_rad,
  fmpz_t      out_exp,
  const arb_t in_value,
  int         in_base,
  slong       in_num_digits
) {
  fmpz_t e, m;
  arb_t t, u;
  arf_t r;
  slong prec;
  int roundmid, roundrad;
  
  assert(in_base >= 2 && in_base <= 36);
  assert(in_num_digits >= 0);
  
  /** TODO: Fast-pass power-of-2 base, expecially base=16. No need to calculate logarithms then.
   */
  
  if(PMATH_UNLIKELY(!arb_is_finite(in_value) || arb_is_zero(in_value))) {
    fmpz_zero(out_mid);
    fmpz_zero(out_rad);
    fmpz_zero(out_exp);
    return;
  }
  
  /* We compute m such that value * base^m ~= base^(num_digits + 5).
     If value = 2^e then m = (num_digits + 5) - e*log(2)/log(base).
  */
  fmpz_init(e);
  fmpz_init(m);
  arb_init(t);
  arb_init(u);
  arf_init(r);
  
  if(arf_cmpabs_mag(arb_midref(in_value), arb_radref(in_value)) > 0)
    fmpz_set(e, ARF_EXPREF(arb_midref(in_value)));
  else
    fmpz_set(e, ARF_EXPREF(arb_radref(in_value)));
    
  prec = fmpz_bits(e) + 15;
  arb_set_si(t, 2);
  arb_log_base_ui(t, t, in_base, prec);
  arb_mul_fmpz(t, t, e, prec);
  arb_neg(t, t);
  arb_add_ui(t, t, in_num_digits + 5, prec);
  
  arf_get_fmpz(m, arb_midref(t), ARF_RND_FLOOR);
  fmpz_neg(out_exp, m);
  
  prec = (slong)(in_num_digits * _pmath_log2_of(in_base) + 30);
  
  if(fmpz_sgn(m) >= 0) {
    pow_ui_fmpz(t, in_base, m, prec);
    arb_mul(t, in_value, t, prec);
  }
  else {
    fmpz_neg(m, m);
    pow_ui_fmpz(t, in_base, m, prec);
    arb_div(t, in_value, t, prec);
  }
  
  roundmid = arf_get_fmpz_fixed_si(out_mid, arb_midref(t), 0);
  
  arf_set_mag(r, arb_radref(t));
  roundrad = arf_get_fmpz_fixed_si(out_rad, r, 0);
  
  fmpz_add_ui(out_rad, out_rad, roundmid + roundrad);
  
  fmpz_clear(e);
  fmpz_clear(m);
  arb_clear(t);
  arb_clear(u);
  arf_clear(r);
}

static int get_digit_value(char ch) {
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  if(ch >= 'a' && ch <= 'z')
    return 10 + ch - 'a';
  if(ch >= 'A' && ch <= 'Z')
    return 10 + ch - 'A';
    
  assert(0 && "invalid digit");
  return 0;
}

static char get_digit_char(int value) {
  assert(value >= 0 && value <= 36);
  return "0123456789abcdefghijklmnopqrstuvwxyz"[value];
}

#define RADIUS_DIGITS 3

/**\brief Rounds a null-terminated string of digits in a given base to length at most n.
   \param s The string of digits to round. It is overwritten in-place, truncating it as necessary.
            The input should not have a leading sign or leading zero digits, but can have trailing zero digits.
   \param shift Set on output, see notes.
   \param error Set on output, see notes.
   \param base The number base, between 2 and 36.
   \param n The mximum number of digits. Must be positive.
   \param rnd The rounding mode. Can be ARF_RND_DOWN, ARF_RND_UP or ARF_RND_NEAR.

   Computes \a shift and \a error are set such that `int(input) = int(output) * base^shift + error`.
 */
static void round_digits_inplace(char *s, mp_bitcnt_t *shift, fmpz_t error, int base, slong n, arf_rnd_t rnd) {
  pmath_bool_t up;
  slong i, len;
  
  assert(base >= 2 && base <= 36);
  assert(n > 0);
  
  len = strlen(s);
  if(len <= n) {
    *shift = 0;
    fmpz_zero(error);
    return;
  }
  
  if(rnd == ARF_RND_DOWN) {
    up = FALSE;
  }
  else if(rnd == ARF_RND_UP) {
    up = FALSE;
    for(i = n; i < len; ++i) {
      if(s[i] != '0') {
        up = TRUE;
        break;
      }
    }
  }
  else {
    assert(rnd == ARF_RND_NEAR);
    
    // TODO: round-to-even
    up = 2 * get_digit_value(s[n]) >= base;
  }
  
  if(!up) { // truncate
    fmpz_set_str(error, s + n, base);
    s[n] = '\0';
    *shift = len - n;
  }
  else {
    int carry;
    int borrow = 0;
    /*  error = base^(len-n) - s[n..] with non-zero s[n..].
        So first write base's complement to the remaining digits and then negate.
     */
    
    for(i = len - 1; i >= n; --i) {
      int digit = base - get_digit_value(s[i]) - borrow;
      if(digit == base) {
        digit = 0;
        borrow = 0;
      }
      else
        borrow = 1;
        
      s[i] = get_digit_char(digit);
    }
    
    assert(borrow != 0);
    
    fmpz_set_str(error, s + n, base);
    fmpz_neg(error, error);
    
    /* add 1 ulp to the leading digits */
    carry = 1;
    for(i = n - 1; i >= 0; i--) {
      int digit =  get_digit_value(s[i]) + carry;
      
      if(digit >= base) {
        digit = 0;
        carry = 1;
      }
      else
        carry = 0;
        
      s[i] = get_digit_char(digit);
    }
    
    if(carry) { // all digits were base-1, and are thus now all '0'
      s[0] = '1';
      *shift = len - n + 1;
    }
    else
      *shift = len - n;
      
    s[n] = '\0';
  }
}

/** \brief Like arb_get_str_parts(), but in a given base.
 */
static void get_str_parts(
  pmath_bool_t  *out_negative,
  char         **out_mid_digits, // to be freed with flint_free()
  fmpz_t         out_mid_exp,
  char         **out_rad_digits, // to be freed with flint_free()
  fmpz_t         out_rad_exp,
  const arb_t    in_value,
  slong          in_max_digits,
  pmath_bool_t   in_allow_inaccurate_digits,
  int            in_base
) {
  fmpz_t mid, rad, exp, err;
  
  *out_negative = FALSE;
  *out_mid_digits = NULL;
  fmpz_zero(out_mid_exp);
  *out_rad_digits = NULL;
  fmpz_zero(out_rad_exp);
  
  if(!arb_is_finite(in_value))
    return;
    
  if(!in_allow_inaccurate_digits) {
    slong heuristic_valid_digits = (slong)(arb_rel_accuracy_bits(in_value) / _pmath_log2_of(in_base) + 2);
    in_max_digits = FLINT_MIN(in_max_digits, heuristic_valid_digits);
  }
  
  fmpz_init(mid);
  fmpz_init(rad);
  fmpz_init(exp);
  fmpz_init(err);
  
  get_fmpz_mid_rad_basis_exp(mid, rad, exp, in_value, in_base, FLINT_MAX(in_max_digits, 1));
  *out_negative = arf_sgn(arb_midref(in_value)) < 0;
  fmpz_abs(mid, mid);
  
  *out_mid_digits = fmpz_get_str(NULL, in_base, mid);
  
  /* Truncate further so that 1 ulp error can be guaranteed (rigorous part)
     Note: mid cannot be zero here if n >= 1 and rad != 0. */
  if(in_max_digits > 0 && !(in_allow_inaccurate_digits || fmpz_is_zero(rad))) {
    slong mid_len, rad_len;
    
    *out_rad_digits = fmpz_get_str(NULL, in_base, rad);
    mid_len = strlen(*out_mid_digits);
    rad_len = strlen(*out_rad_digits);
    
    if(mid_len > rad_len) {
      /* we will truncate at in_max_digits or in_max_digits-1 */
      slong valid_digits = mid_len - rad_len;
      slong rem;
      
      /* rounding to nearest can add at most 0.5 ulp */
      /* look at first omitted digit */
      rem = get_digit_value((*out_mid_digits)[valid_digits]);
      if (2 * rem < in_base)
        rem = rem + 1;
      else
        rem = in_base - rem;
        
      /* and include the leading digit of the radius */
      rem = rem + get_digit_value((*out_rad_digits)[0]) + 1;
      
      /* if error is <= 1.0 ulp, we get to keep the extra digit */
      if (rem > in_base)
        valid_digits -= 1;
        
      in_max_digits = FLINT_MIN(in_max_digits, valid_digits);
    }
    else
      in_max_digits = 0;
      
    flint_free(*out_rad_digits);
    *out_rad_digits = NULL;
  }
  
  /* no accurate digits -- output 0 +/- rad */
  if(in_max_digits < 1) {
    fmpz_add(rad, rad, mid);
    fmpz_zero(mid);
    strcpy(*out_mid_digits, "0");  /* note that *out_mid_digits already contains a string of length >= 1 */
  }
  else {
    mp_bitcnt_t shift;
    round_digits_inplace(*out_mid_digits, &shift, err, in_base, in_max_digits, ARF_RND_NEAR);
    fmpz_add_ui(out_mid_exp, exp, shift);
    fmpz_abs(err, err);
    fmpz_add(rad, rad, err);
  }
  
  /* write radius */
  if(fmpz_is_zero(rad)) {
    *out_rad_digits = fmpz_get_str(NULL, in_base, rad);
    fmpz_zero(out_rad_exp);
  }
  else {
    *out_rad_digits = fmpz_get_str(NULL, in_base, rad);
    if((in_base & (in_base - 1)) != 0) { // not a power of 2: at most RADIUS_DIGITS many radius digits, since we round anyway.
      mp_bitcnt_t shift;
      round_digits_inplace(*out_rad_digits, &shift, err, in_base, RADIUS_DIGITS, ARF_RND_UP);
      fmpz_add_ui(out_rad_exp, exp, shift);
    }
    else
      fmpz_set(out_rad_exp, exp);
  }
  
  fmpz_clear(mid);
  fmpz_clear(rad);
  fmpz_clear(exp);
  fmpz_clear(err);
}

PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t precision_to_string(double precision_digits, slong precision_bits) {
  double factor = precision_digits / precision_bits;
  double min = precision_digits - 0.4 * factor;
  double max = precision_digits + 0.4 * factor;
  char s[100];
  int len;
  double test;
  
  for(len = 1; len < sizeof(s); ++len) {
    snprintf(s, sizeof(s), "%.*f", len, precision_digits);
    
    // not pmath_strtod() because sprintf gives locale specific result
    test = strtod(s, NULL);
    if(min < test && test < max)
      break;
  }
  
  for(len = 0; len < sizeof(s) && s[len]; ++len) {
    if(s[len] == ',') {
      s[len] = '.';
      break;
    }
  }
  
  return PMATH_C_STRING(s);
}

PMATH_ATTRIBUTE_USE_RESULT
static pmath_string_t split_digits(const char *integer_digits, int length, int frac_digits) {
  pmath_string_t result;
  
  assert(integer_digits != NULL);
  assert(integer_digits > 0);
  assert(frac_digits >= 0);
  
  result = pmath_string_new(length + 2);
  result = pmath_string_insert_latin1(result, INT_MAX, integer_digits, length - frac_digits);
  result = pmath_string_insert_latin1(result, INT_MAX, ".", 1);
  result = pmath_string_insert_latin1(result, INT_MAX, integer_digits + length - frac_digits, frac_digits);
  if(frac_digits == 0)
    result = pmath_string_insert_latin1(result, INT_MAX, "0", 1);
    
  return result;
}

PMATH_PRIVATE
void _pmath_mpfloat_get_string_parts(
  struct _pmath_number_string_parts_t *result,
  pmath_mpfloat_t                      value,
  int                                  base,
  int                                  max_digits,
  pmath_bool_t                         allow_inaccurate_digits
) {
  char *mid_digits;
  char *rad_digits;
  fmpz_t mid_exp;
  fmpz_t rad_exp;
  double precision_digits;
  int num_mid_digits;
  int num_rad_digits;
  int num_mid_frac_digits;
  int num_rad_frac_digits;
  
  assert(result != NULL);
  assert(base >= 2 && base <= 36);
  assert(pmath_is_mpfloat(value));
  assert(max_digits >= 0);
  
  fmpz_init(mid_exp);
  fmpz_init(rad_exp);
  
  precision_digits = PMATH_AS_ARB_WORKING_PREC(value) / _pmath_log2_of(base);
  //max_digits = (int)FLINT_MIN((slong)max_digits, (slong)ceil(precision_digits));
  
  result->base = base;
  get_str_parts(
    &result->is_negative,
    &mid_digits,
    mid_exp,
    &rad_digits,
    rad_exp,
    PMATH_AS_ARB(value),
    max_digits,
    allow_inaccurate_digits,
    base);
    
  result->precision_decimal_digits = precision_to_string(precision_digits, PMATH_AS_ARB_WORKING_PREC(value));
  
  result->midpoint_fractional_mantissa_digits = PMATH_NULL;
  result->exponent_decimal_digits = PMATH_NULL;
  result->radius_fractional_mantissa_digits = PMATH_NULL;
  result->radius_exponent_part_decimal_digits = PMATH_NULL;
  
  if(!mid_digits || !rad_digits) {
    fmpz_clear(mid_exp);
    fmpz_clear(rad_exp);
    flint_free(mid_digits);
    flint_free(rad_digits);
    return;
  }
  
  num_mid_digits = (int)strlen(mid_digits);
  num_rad_digits = (int)strlen(rad_digits);
  
  assert(num_mid_digits > 0);
  assert(num_rad_digits > 0);
  
  num_mid_frac_digits = num_mid_digits - 1;
  fmpz_add_si(mid_exp, mid_exp, num_mid_frac_digits);
  result->midpoint_fractional_mantissa_digits = split_digits(mid_digits, num_mid_digits, num_mid_frac_digits);
  flint_free(mid_digits);
  
  fmpz_sub(rad_exp, rad_exp, mid_exp);
  
  num_rad_frac_digits = num_rad_digits - 1;
  fmpz_add_si(rad_exp, rad_exp, num_rad_frac_digits);
  result->radius_fractional_mantissa_digits = split_digits(rad_digits, num_rad_digits, num_rad_frac_digits);
  flint_free(rad_digits);
  
  if(fmpz_is_zero(mid_exp)) {
    result->exponent_decimal_digits = PMATH_C_STRING("");
  }
  else {
    mid_digits = fmpz_get_str(NULL, 10, mid_exp);
    result->exponent_decimal_digits = PMATH_C_STRING(mid_digits);
    flint_free(mid_digits);
  }
  fmpz_clear(mid_exp);
  
  if(fmpz_is_zero(rad_exp)) {
    result->radius_exponent_part_decimal_digits = PMATH_C_STRING("");
  }
  else {
    rad_digits = fmpz_get_str(NULL, 10, rad_exp);
    result->radius_exponent_part_decimal_digits = PMATH_C_STRING(rad_digits);
    flint_free(rad_digits);
  }
  fmpz_clear(rad_exp);
}
