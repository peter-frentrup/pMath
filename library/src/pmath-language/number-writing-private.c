#include <pmath-language/number-writing-private.h>
#include <pmath-language/number-parsing-private.h> // for _pmath_log2_of

static void init_number_string_raw_parts(struct _pmath_number_string_raw_parts_t *parts);

PMATH_PRIVATE void _pmath_number_string_parts_clear(struct _pmath_number_string_parts_t *parts) {
  assert(parts != NULL);
  pmath_unref(parts->midpoint_fractional_mantissa_digits);
  pmath_unref(parts->radius_fractional_mantissa_digits);
  pmath_unref(parts->radius_exponent_part_decimal_digits);
  pmath_unref(parts->precision_decimal_digits);
  pmath_unref(parts->exponent_decimal_digits);
}

static void pow_ui_fmpz(arb_t res, int base, const fmpz_t e, slong prec) {
  slong bits;
  
  assert(base > 0);
  
  bits = fmpz_bits(e);
  arb_set_si(res, base);
  arb_pow_fmpz(res, res, e, prec + bits);
}

/** \brief Compute integers `m`, `r`, `e` such that `x` is in the interval [m-r, m+r]*base^e.
    \param out_mid An initialized fmpz integer to be set to the mid-point `m`.
    \param out_rad An initialized fmpz integer to be set to the radius `r`.
    \param out_exp An initialized fmpz integer to be set to the exponent `e`.
    \param in_value A finite Arb real ball `x`.
    \param in_base The base, between 2 and 36.
    \param in_num_digits The minimum number of digits for the larger of \a out_mid and \a out_rad.
                         This must be non-negative.

    If \a in_value is not finite or is exactly zero, \a out_mid, \a out_rad and \a out_exp will be set to zero.
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
  fmpz_t binexp, mmm;
  arb_t tmp;
  arf_t rad_approx;
  int roundmid, roundrad;
  
  assert(in_base >= 2 && in_base <= 36);
  assert(in_num_digits >= 0);
  
  if(PMATH_UNLIKELY(!arb_is_finite(in_value) || arb_is_zero(in_value))) {
    fmpz_zero(out_mid);
    fmpz_zero(out_rad);
    fmpz_zero(out_exp);
    return;
  }
  
  /* We compute mmm such that  value * base^mmm ~= base^(num_digits + 5).
     If  value = 2^binexp  then  mmm = (num_digits + 5) - binexp*log(2)/log(base).
  */
  fmpz_init(binexp);
  fmpz_init(mmm);
  arb_init(tmp);
  arf_init(rad_approx);
  
  // binexp:= ceil(log2(max(abs(mid), abs(rad)))
  if(arf_cmpabs_mag(arb_midref(in_value), arb_radref(in_value)) > 0)
    fmpz_set(binexp, ARF_EXPREF(arb_midref(in_value)));
  else
    fmpz_set(binexp, MAG_EXPREF(arb_radref(in_value)));
    
  // mmm:= floor( (num_digits + 5) - binexp*log(2)/log(base) )
  // out_exp:= -floor( (num_digits + 5) - binexp*log(2)/log(base) )
  /// Then
  // tmp:= value * base^mmm  =  value * base^(-out_exp)    invalidates mmm
  if(     in_base == 1 << 1) { fmpz_neg(      mmm, binexp);     fmpz_add_ui(mmm, mmm, in_num_digits + 5); fmpz_neg(out_exp, mmm); /* Then */                           arb_mul_2exp_fmpz(tmp, in_value, mmm); }
  else if(in_base == 1 << 2) { fmpz_fdiv_q_si(mmm, binexp, -2); fmpz_add_ui(mmm, mmm, in_num_digits + 5); fmpz_neg(out_exp, mmm); /* Then */ fmpz_mul_ui(mmm, mmm, 2); arb_mul_2exp_fmpz(tmp, in_value, mmm); }
  else if(in_base == 1 << 3) { fmpz_fdiv_q_si(mmm, binexp, -3); fmpz_add_ui(mmm, mmm, in_num_digits + 5); fmpz_neg(out_exp, mmm); /* Then */ fmpz_mul_ui(mmm, mmm, 3); arb_mul_2exp_fmpz(tmp, in_value, mmm); }
  else if(in_base == 1 << 4) { fmpz_fdiv_q_si(mmm, binexp, -4); fmpz_add_ui(mmm, mmm, in_num_digits + 5); fmpz_neg(out_exp, mmm); /* Then */ fmpz_mul_ui(mmm, mmm, 4); arb_mul_2exp_fmpz(tmp, in_value, mmm); }
  else if(in_base == 1 << 5) { fmpz_fdiv_q_si(mmm, binexp, -5); fmpz_add_ui(mmm, mmm, in_num_digits + 5); fmpz_neg(out_exp, mmm); /* Then */ fmpz_mul_ui(mmm, mmm, 5); arb_mul_2exp_fmpz(tmp, in_value, mmm); }
  else {
    slong prec = fmpz_bits(binexp) + 15;
    
    // tmp:= (in_num_digits + 5) - binexp*log(2)/log(base)
    arb_set_si(tmp, 2);
    arb_log_base_ui(tmp, tmp, in_base, prec);
    arb_mul_fmpz(tmp, tmp, binexp, prec);
    arb_neg(tmp, tmp);
    arb_add_ui(tmp, tmp, in_num_digits + 5, prec);
    
    // mmm:= floor(tmp) = floor( (num_digits + 5) - binexp*log(2)/log(base) )
    arf_get_fmpz(mmm, arb_midref(tmp), ARF_RND_FLOOR);
  
    // out_exp:= -mmm
    fmpz_neg(out_exp, mmm);
    
    /// Then
    // tmp:= value * base^mmm  =  value * base^(-out_exp)    invalidates mmm
    prec = (slong)(in_num_digits * _pmath_log2_of(in_base) + 30);
  
    // arb_mul() can round even if factor t is an exact power of 2
    if(fmpz_sgn(mmm) >= 0) {
      pow_ui_fmpz(tmp, in_base, mmm, prec);
      arb_mul(tmp, in_value, tmp, prec);
    }
    else {
      fmpz_neg(mmm, mmm);
      pow_ui_fmpz(tmp, in_base, mmm, prec);
      arb_div(tmp, in_value, tmp, prec);
    }
  }
  
  // out_mid:= tmp  midpoint,  roundmid:= 1 if truncated, 0 if exact
  roundmid = arf_get_fmpz_fixed_si(out_mid, arb_midref(tmp), 0);
  
  // out_rad:= tmp  radius,    roundrad:= 1 if truncated, 0 if exact
  arf_set_mag(rad_approx, arb_radref(tmp));
  roundrad = arf_get_fmpz_fixed_si(out_rad, rad_approx, 0);
  
  fmpz_add_ui(out_rad, out_rad, roundmid + roundrad);
  
  fmpz_clear(binexp);
  fmpz_clear(mmm);
  arb_clear(tmp);
  arf_clear(rad_approx);
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
  struct _pmath_number_string_raw_parts_t *out_parts,
  const arb_t                              in_value,
  slong                                    in_max_digits,
  pmath_bool_t                             in_allow_inaccurate_digits,
  int                                      in_base
) {
  fmpz_t mid, rad, exp, err;
  
  assert(out_parts != NULL);
  
  out_parts->is_negative = FALSE;
  out_parts->base = in_base;
  if(out_parts->mid_digits) { flint_free(out_parts->mid_digits); out_parts->mid_digits = NULL; }
  if(out_parts->rad_digits) { flint_free(out_parts->rad_digits); out_parts->rad_digits = NULL; }
  fmpz_zero(out_parts->mid_exp);
  fmpz_zero(out_parts->rad_exp);
  
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
  out_parts->is_negative = arf_sgn(arb_midref(in_value)) < 0;
  fmpz_abs(mid, mid);
  
  out_parts->mid_digits = fmpz_get_str(NULL, in_base, mid);
  
  /* Truncate further so that 1 ulp error can be guaranteed (rigorous part)
     Note: mid cannot be zero here if  in_max_digits >= 1 and rad != 0. */
  if(in_max_digits > 0 && !(in_allow_inaccurate_digits || fmpz_is_zero(rad))) {
    slong mid_len, rad_len;
    
    out_parts->rad_digits = fmpz_get_str(NULL, in_base, rad);
    mid_len = strlen(out_parts->mid_digits);
    rad_len = strlen(out_parts->rad_digits);
    
    if(mid_len > rad_len) {
      /* we will truncate at in_max_digits or in_max_digits-1 */
      slong valid_digits = mid_len - rad_len;
      slong rem;
      
      /* rounding to nearest can add at most 0.5 ulp */
      /* look at first omitted digit */
      rem = get_digit_value((out_parts->mid_digits)[valid_digits]);
      if (2 * rem < in_base)
        rem = rem + 1;
      else
        rem = in_base - rem;
        
      /* and include the leading digit of the radius */
      rem = rem + get_digit_value((out_parts->rad_digits)[0]) + 1;
      
      /* if error is <= 1.0 ulp, we get to keep the extra digit */
      if (rem > in_base)
        valid_digits -= 1;
        
      in_max_digits = FLINT_MIN(in_max_digits, valid_digits);
    }
    else
      in_max_digits = 0;
      
    flint_free(out_parts->rad_digits);
    out_parts->rad_digits = NULL;
  }
  
  /* no accurate digits -- output 0 +/- rad */
  if(in_max_digits < 1) {
    fmpz_add(rad, rad, mid);
    fmpz_zero(mid);
    strcpy(out_parts->mid_digits, "0");  /* note that out_parts->mid_digits already contains a string of length >= 1 */
  }
  else {
    mp_bitcnt_t shift;
    round_digits_inplace(out_parts->mid_digits, &shift, err, in_base, in_max_digits, ARF_RND_NEAR);
    fmpz_add_ui(out_parts->mid_exp, exp, shift);
    fmpz_abs(err, err);
    fmpz_add(rad, rad, err);
  }
  
  /* write radius */
  if(fmpz_is_zero(rad)) {
    out_parts->rad_digits = fmpz_get_str(NULL, in_base, rad);
    fmpz_zero(out_parts->rad_exp);
  }
  else {
    out_parts->rad_digits = fmpz_get_str(NULL, in_base, rad);
    if((in_base & (in_base - 1)) != 0) { // not a power of 2: at most RADIUS_DIGITS many radius digits, since we round anyway.
      mp_bitcnt_t shift;
      round_digits_inplace(out_parts->rad_digits, &shift, err, in_base, RADIUS_DIGITS, ARF_RND_UP);
      fmpz_add_ui(out_parts->rad_exp, exp, shift);
    }
    else
      fmpz_set(out_parts->rad_exp, exp);
  }
  
  fmpz_clear(mid);
  fmpz_clear(rad);
  fmpz_clear(exp);
  fmpz_clear(err);
}

/** \brief Losslessly convert an Arb number to hexadecimal string representation parts.
 */
static void get_str_hex_parts(
  struct _pmath_number_string_raw_parts_t *out_parts,
  const arb_t                              in_value
) {
  fmpz_t mant;
  ulong remainder;
  arf_t tmp;
  
  assert(out_parts != NULL);
  
  out_parts->is_negative = FALSE;
  out_parts->base = 16;
  if(out_parts->mid_digits) { flint_free(out_parts->mid_digits); out_parts->mid_digits = NULL; }
  if(out_parts->rad_digits) { flint_free(out_parts->rad_digits); out_parts->rad_digits = NULL; }
  fmpz_zero(out_parts->mid_exp);
  fmpz_zero(out_parts->rad_exp);
  
  if(!arb_is_finite(in_value))
    return;
    
  out_parts->is_negative = arf_sgn(arb_midref(in_value)) < 0;
  
  fmpz_init(mant);
  
  arf_get_fmpz_2exp(mant, out_parts->mid_exp, arb_midref(in_value));
  fmpz_abs(mant, mant);
  // out_mid_exp is in power of 2 instead of 16 (1 << (1 << 2)). Convert to power of 16:
  remainder = fmpz_fdiv_ui(out_parts->mid_exp, 1 << 2);
  fmpz_fdiv_q_2exp(out_parts->mid_exp, out_parts->mid_exp, 2);
  fmpz_mul_2exp(mant, mant, remainder);
  
  out_parts->mid_digits = fmpz_get_str(NULL, 16, mant);
  
  arf_init_set_mag_shallow(tmp, arb_radref(in_value));
  arf_get_fmpz_2exp(mant, out_parts->rad_exp, tmp);
  // out_rad_exp is in power of 2 instead of 16 (2^4). Convert to power of 16:
  remainder = fmpz_fdiv_ui(out_parts->rad_exp, 1 << 2);
  fmpz_fdiv_q_2exp(out_parts->rad_exp, out_parts->rad_exp, 2);
  fmpz_mul_2exp(mant, mant, remainder);
  
  out_parts->rad_digits = fmpz_get_str(NULL, 16, mant);
  
  fmpz_clear(mant);
}


static void write_cstr_to_string(void *_res, const char *str, int len) {
  pmath_string_t *res = _res;
  
  *res = pmath_string_insert_latin1(*res, INT_MAX, str, len);
}

static pmath_string_t place_decimal_dot(const char *integer_digits, fmpz_t inout_exp) {
  pmath_string_t result = PMATH_NULL;
  int num_digits = (int)strlen(integer_digits);
  
  result = pmath_string_new(num_digits + 1); // sometimes +2
  
  _pmath_write_place_decimal_dot(integer_digits, num_digits, inout_exp, write_cstr_to_string, &result);
  
  return result;
}

static pmath_string_t precision_to_string(double precision_digits, slong precision_bits) {
  pmath_string_t result = PMATH_NULL;
  
  _pmath_write_precision(precision_digits, precision_bits, write_cstr_to_string, &result);
  
  return result;
}


PMATH_PRIVATE
void _pmath_write_precision(
  double   precision_digits, 
  slong    precision_bits, 
  void   (*writer)(void*, const char*, int), 
  void    *ctx
) {
  double factor = precision_digits / precision_bits;
  double min = precision_digits - 0.99 * factor;
  double max = precision_digits;
  char s[100];
  int len;
  double test;
  
  for(len = 0; len < sizeof(s); ++len) {
    snprintf(s, sizeof(s), "%.*f", len, precision_digits);
    
    // not pmath_strtod() because sprintf gives locale specific result
    test = strtod(s, NULL);
    if(min < test && test <= max)
      break;
  }
  
  for(len = 0; len < sizeof(s) && s[len]; ++len) {
    if(s[len] == ',') {
      s[len] = '.';
      break;
    }
  }
  
  writer(ctx, s, (int)strlen(s));
}


#define MAX_INTEGER_DIGITS  6
#define MAX_LEADING_ZEROS   5

PMATH_PRIVATE
void _pmath_write_place_decimal_dot(
  const char *integer_digits, 
  int         num_digits,
  fmpz_t      inout_exp, 
  void      (*writer)(void*, const char*, int), 
  void       *ctx
) {
  int int_digits = 1;
  int frac_digits;
  
  if(num_digits < 0)
    num_digits = (int)strlen(integer_digits);
  
  if(num_digits >= 2 && fmpz_sgn(inout_exp) < 0) {
    if(fmpz_cmp_si(inout_exp, -num_digits) <= 0 && fmpz_cmp_si(inout_exp, -(num_digits + MAX_LEADING_ZEROS)) >= 0) { 
      // -(num_digits + MAX_LEADING_ZEROS) <= inout_exp <= -num_digits
      int leading_zeros = -(int)fmpz_get_si(inout_exp) - num_digits;
      
      int trailing_zeros = 0;
      while(trailing_zeros < FLINT_MIN(num_digits, MAX_LEADING_ZEROS) && integer_digits[num_digits - trailing_zeros - 1] == '0')
        ++trailing_zeros;
      
      if(leading_zeros < trailing_zeros) {
        writer(ctx, "0.", 2);
        writer(ctx, integer_digits + num_digits - leading_zeros, leading_zeros);
        writer(ctx, integer_digits, num_digits - leading_zeros - 1);
        fmpz_zero(inout_exp);
        return;
      }
    }
    else if(fmpz_cmp_si(inout_exp, - num_digits) > 0) { // -num_digits < inout_exp < 0
      frac_digits = (int)-fmpz_get_si(inout_exp);
      int_digits = num_digits - frac_digits;
      if(int_digits > MAX_INTEGER_DIGITS)
        int_digits = 1;
    }
  }
  
  frac_digits = num_digits - int_digits;
  fmpz_add_si(inout_exp, inout_exp, num_digits - int_digits);
  
  writer(ctx, integer_digits, int_digits);
  if(int_digits < num_digits) {
    writer(ctx, ".", 1);
    writer(ctx, integer_digits + int_digits, num_digits - int_digits);
  }
  else
    writer(ctx, ".0", 2);
}

PMATH_PRIVATE
void _pmath_mpfloat_get_string_parts(
  struct _pmath_number_string_parts_t *result,
  pmath_mpfloat_t                      value,
  int                                  max_digits,
  int                                  base_flags
) {
  struct _pmath_number_string_raw_parts_t raw_parts;
  double precision_digits;
  
  assert(result != NULL);
  assert(pmath_is_mpfloat(value));
  
  _pmath_mpfloat_get_string_raw_parts(&raw_parts, value, max_digits, base_flags);
  
  precision_digits = PMATH_AS_ARB_WORKING_PREC(value) / _pmath_log2_of(raw_parts.base);
  
  result->is_negative = raw_parts.is_negative;
  result->base        = raw_parts.base;
  
  result->precision_decimal_digits = precision_to_string(precision_digits, PMATH_AS_ARB_WORKING_PREC(value));
  
  result->midpoint_fractional_mantissa_digits = PMATH_NULL;
  result->exponent_decimal_digits             = PMATH_NULL;
  result->radius_fractional_mantissa_digits   = PMATH_NULL;
  result->radius_exponent_part_decimal_digits = PMATH_NULL;
  
  if(!raw_parts.mid_digits || !raw_parts.rad_digits) {
    _pmath_number_string_raw_parts_clear(&raw_parts);
    return;
  }
  
  result->midpoint_fractional_mantissa_digits = place_decimal_dot(raw_parts.mid_digits, raw_parts.mid_exp);
  
  if(raw_parts.rad_digits[0] == '0' && raw_parts.rad_digits[1] == '\0') {
    fmpz_zero(raw_parts.rad_exp);
  }
  else
    fmpz_sub(raw_parts.rad_exp, raw_parts.rad_exp, raw_parts.mid_exp);
  
  result->radius_fractional_mantissa_digits = place_decimal_dot(raw_parts.rad_digits, raw_parts.rad_exp);
  
  if(fmpz_is_zero(raw_parts.mid_exp)) {
    result->exponent_decimal_digits = PMATH_C_STRING("");
  }
  else {
    char *tmp = fmpz_get_str(NULL, 10, raw_parts.mid_exp);
    result->exponent_decimal_digits = PMATH_C_STRING(tmp);
    flint_free(tmp);
  }
  
  if(fmpz_is_zero(raw_parts.rad_exp)) {
    result->radius_exponent_part_decimal_digits = PMATH_C_STRING("");
  }
  else {
    char *tmp = fmpz_get_str(NULL, 10, raw_parts.rad_exp);
    result->radius_exponent_part_decimal_digits = PMATH_C_STRING(tmp);
    flint_free(tmp);
  }
  
  _pmath_number_string_raw_parts_clear(&raw_parts);
}

PMATH_PRIVATE
void _pmath_mpfloat_get_string_raw_parts(
  struct _pmath_number_string_raw_parts_t *result,
  pmath_mpfloat_t                          value,
  int                                      max_digits,
  int                                      base_flags
) {
  int base = base_flags & PMATH_BASE_FLAGS_BASE_MASK;
  pmath_bool_t allow_inaccurate_digits = (base_flags & PMATH_BASE_FLAG_ALLOW_INEXACT_DIGITS) != 0;
  
  assert(result != NULL);
  assert(base >= 2 && base <= 36);
  assert(pmath_is_mpfloat(value));
  assert(max_digits >= 0);
  
  init_number_string_raw_parts(result);
  
  result->base = base;
  if(base == 16 && (base_flags & PMATH_BASE_FLAG_ALL_DIGITS)) {
    get_str_hex_parts(
      result,
      PMATH_AS_ARB(value));
  }
  else {
    get_str_parts(
      result,
      PMATH_AS_ARB(value),
      max_digits,
      allow_inaccurate_digits,
      base);
  }
}


static void init_number_string_raw_parts(struct _pmath_number_string_raw_parts_t *parts) {
  fmpz_init(parts->mid_exp);
  fmpz_init(parts->rad_exp);
  parts->mid_digits = NULL;
  parts->rad_digits = NULL;
}

PMATH_PRIVATE void _pmath_number_string_raw_parts_clear(struct _pmath_number_string_raw_parts_t *parts) {
  fmpz_clear(parts->mid_exp);
  fmpz_clear(parts->rad_exp);
  flint_free(parts->mid_digits);
  flint_free(parts->rad_digits);
}
