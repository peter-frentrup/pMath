#include <pmath-language/number-writing-private.h>
#include <pmath-language/number-parsing-private.h> // for _pmath_log2_of

#include <pmath-util/memory.h>
#include <string.h>


static void free_fmpz_str(char *str);

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

static void write_ascii_zeros(
  int    num_zeros,
  void (*writer)(void*, const char*, int), 
  void  *ctx
) {
  const char *zeros = "0000000000";
  int buflen = 10;
  
  while(num_zeros > buflen) {
    writer(ctx, zeros, buflen);
    num_zeros-= buflen;
  }
  
  if(num_zeros)
    writer(ctx, zeros, num_zeros);
}

PMATH_PRIVATE 
void _pmath_number_round_digits_inplace(
  char        *s, 
  mp_bitcnt_t *shift, 
  fmpz_t       error, 
  int          base, 
  slong        max_digits, 
  arf_rnd_t    rnd
) {
  pmath_bool_t up;
  slong i, len;
  
  assert(base >= 2 && base <= 36);
  assert(max_digits >= 0);
  
  len = strlen(s);
  if(len <= max_digits) {
    *shift = 0;
    fmpz_zero(error);
    return;
  }
  
  if(rnd == ARF_RND_DOWN) {
    up = FALSE;
  }
  else if(rnd == ARF_RND_UP) {
    up = FALSE;
    for(i = max_digits; i < len; ++i) {
      if(s[i] != '0') {
        up = TRUE;
        break;
      }
    }
  }
  else {
    assert(rnd == ARF_RND_NEAR);
    
    // TODO: round-to-even
    up = 2 * get_digit_value(s[max_digits]) >= base;
  }
  
  if(!up) { // truncate
    fmpz_set_str(error, s + max_digits, base);
    s[max_digits] = '\0';
    *shift = len - max_digits;
  }
  else {
    int carry;
    int borrow = 0;
    /*  error = base^(len-n) - s[n..] with non-zero s[n..].
        So first write base's complement to the remaining digits and then negate.
     */
    
    for(i = len - 1; i >= max_digits; --i) {
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
    
    fmpz_set_str(error, s + max_digits, base);
    fmpz_neg(error, error);
    
    /* add 1 ulp to the leading digits */
    carry = 1;
    for(i = max_digits - 1; i >= 0; i--) {
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
      *shift = len - max_digits + 1;
    }
    else
      *shift = len - max_digits;
      
    s[max_digits] = '\0';
  }
}


static void raw_number_parts_init(struct _pmath_raw_number_parts_t *parts);

static void get_str_parts_new(
  struct _pmath_raw_number_parts_t *out_parts,
  const arb_t                in_value,
  int                        in_base,
  int                        in_max_rad_digits,
  slong                      in_precision_bits
) {
  fmpz_t mid, rad, exp, err;
  int max_digits;
  int rad_len;
  int mid_len;
  int mid_trailing_zeros;
  int rad_trailing_zeros;
  
  assert(out_parts != NULL);
  
  if(out_parts->mid_digits) { free_fmpz_str(out_parts->mid_digits); out_parts->mid_digits = NULL; }
  if(out_parts->rad_digits) { free_fmpz_str(out_parts->rad_digits); out_parts->rad_digits = NULL; }
  fmpz_zero(out_parts->exponent);
  fmpz_zero(out_parts->rad_exponent_extra);
  out_parts->total_significant     = 0;
  out_parts->total_insignificant   = 0;
  out_parts->mid_leading_zeros     = 0;
  out_parts->num_integer_digits    = 0;
  out_parts->base                  = in_base;
  out_parts->is_negative           = FALSE;
  out_parts->needs_radius_exponent = FALSE;
  
  if(!arb_is_finite(in_value))
    return;
  
  {
    slong valid_bits = arb_rel_accuracy_bits(in_value); // may be negative if |mid| < |rad|
    if(valid_bits < 0)
      valid_bits = 0;
    else if(valid_bits > in_precision_bits)
      valid_bits = in_precision_bits;
    
    max_digits = (slong)(valid_bits / _pmath_log2_of(in_base) + 2) + in_max_rad_digits;
  }
  
  fmpz_init(mid);
  fmpz_init(rad);
  fmpz_init(exp);
  fmpz_init(err);
  
  get_fmpz_mid_rad_basis_exp(mid, rad, exp, in_value, in_base, max_digits);
  out_parts->is_negative = arf_sgn(arb_midref(in_value)) < 0;
  fmpz_abs(mid, mid);
  
  out_parts->mid_digits = fmpz_get_str(NULL, in_base, mid);
  mid_len = strlen(out_parts->mid_digits);
  rad_len = fmpz_sizeinbase(rad, in_base);
  
  mid_trailing_zeros = 0;
  
  if(rad_len > in_max_rad_digits) {
    mid_len = mid_len - rad_len + in_max_rad_digits;
    if(mid_len > 0) {
      mp_bitcnt_t shift;
      
      _pmath_number_round_digits_inplace(out_parts->mid_digits, &shift, err, in_base, mid_len, ARF_RND_NEAR);
      
      //fmpz_add_ui(out_parts->mid_exp, exp, shift);....
      mid_trailing_zeros += shift;
      fmpz_abs(err, err);
      fmpz_add(rad, rad, err);
      rad_len = fmpz_sizeinbase(rad, in_base);
    }
    else {
      mid_len = 0;
      out_parts->mid_digits[0] = '\0';
      mid_trailing_zeros = rad_len;
      
      fmpz_add(rad, rad, mid);
      rad_len = fmpz_sizeinbase(rad, in_base);
    }
  }
  
  rad_trailing_zeros = 0;
  out_parts->rad_digits = fmpz_get_str(NULL, in_base, rad);
  assert(rad_len == strlen(out_parts->rad_digits));
  
  if(rad_len > in_max_rad_digits) {
    mp_bitcnt_t shift;
    _pmath_number_round_digits_inplace(out_parts->rad_digits, &shift, err, in_base, in_max_rad_digits, ARF_RND_UP);
    
    rad_trailing_zeros+= shift;// + in_max_rad_digits - rad_len;
    
    rad_len = in_max_rad_digits;
  }
  
  if(mid_len + mid_trailing_zeros > rad_len + rad_trailing_zeros) {
    out_parts->total_significant = mid_len + mid_trailing_zeros - (rad_len + rad_trailing_zeros);
    
    out_parts->mid_leading_zeros = 0;
    fmpz_add_si_inline(out_parts->exponent, exp, mid_len + mid_trailing_zeros);
    //fmpz_zero(out_parts->rad_exponent_extra);
  }
  else {
    out_parts->total_significant = 0;
    
    out_parts->mid_leading_zeros = (rad_len + rad_trailing_zeros) - (mid_len + mid_trailing_zeros);
    //fmpz_zero(out_parts->exponent);
    //fmpz_add_si_inline(out_parts->rad_exponent_extra, exp, rad_len + rad_trailing_zeros);
    fmpz_add_si_inline(out_parts->exponent, exp, mid_len + mid_trailing_zeros);
    //fmpz_zero(out_parts->rad_exponent_extra);
    
    //out_parts->needs_radius_exponent = TRUE;
  }
  
  while(mid_len > 0 && out_parts->mid_digits[mid_len-1] == '0') {
    --mid_len;
    ++mid_trailing_zeros;
  }
  out_parts->mid_digits[mid_len] = '\0';
  
  while(rad_len > 0 && out_parts->rad_digits[rad_len-1] == '0') {
    --rad_len;
    ++rad_trailing_zeros;
  }
  out_parts->rad_digits[rad_len] = '\0';
  
  while(mid_trailing_zeros > 0 && rad_trailing_zeros > 0) {
    --mid_trailing_zeros;
    --rad_trailing_zeros;
  }
  
  out_parts->total_insignificant = rad_len + rad_trailing_zeros;
  
  fmpz_clear(mid);
  fmpz_clear(rad);
  fmpz_clear(exp);
  fmpz_clear(err);
}

/** \brief Losslessly convert an Arb number to base 2^n string representation parts.
 */
static void get_str_2exp_parts_new(
  struct _pmath_raw_number_parts_t *out_parts,
  const arb_t                in_value,
  int                        in_baselog
) {
  fmpz_t mant;
  int midlen, radlen;
  
  assert(out_parts != NULL);
  assert(1 <= in_baselog && in_baselog <= 5);
  
  if(out_parts->mid_digits) { free_fmpz_str(out_parts->mid_digits); out_parts->mid_digits = NULL; }
  if(out_parts->rad_digits) { free_fmpz_str(out_parts->rad_digits); out_parts->rad_digits = NULL; }
  fmpz_zero(out_parts->exponent);
  fmpz_zero(out_parts->rad_exponent_extra);
  out_parts->total_significant     = 0;
  out_parts->total_insignificant   = 0;
  out_parts->mid_leading_zeros     = 0;
  out_parts->num_integer_digits    = 0;
  out_parts->base                  = 1 << in_baselog;
  out_parts->is_negative           = FALSE;
  out_parts->needs_radius_exponent = FALSE;
  
  if(!arb_is_finite(in_value))
    return;
  
  out_parts->is_negative = arf_sgn(arb_midref(in_value)) < 0;
  
  fmpz_init(mant);
  
  /// Midpoint ...
  arf_get_fmpz_2exp(mant, out_parts->exponent, arb_midref(in_value));
  fmpz_abs(mant, mant);
  // Convert exponent from power of 2 to power of 2^n:
  if(in_baselog > 1) {
    ulong remainder = fmpz_fdiv_ui(out_parts->exponent, in_baselog);
    fmpz_fdiv_q_ui(out_parts->exponent, out_parts->exponent, in_baselog);
    fmpz_mul_2exp(mant, mant, remainder);
  }
  
  out_parts->mid_digits = fmpz_get_str(NULL, 1 << in_baselog, mant);
  midlen = strlen(out_parts->mid_digits);
  fmpz_add_si_inline(out_parts->exponent, out_parts->exponent, midlen);
  
  /// Radius ...
  {
    arf_t tmp_rad;
    arf_init_set_mag_shallow(tmp_rad, arb_radref(in_value));
    arf_get_fmpz_2exp(mant, out_parts->rad_exponent_extra, tmp_rad);
  }
  // Convert exponent from power of 2 to power of 2^n:
  if(in_baselog > 1) {
    ulong remainder = fmpz_fdiv_ui(out_parts->rad_exponent_extra, in_baselog);
    fmpz_fdiv_q_ui(out_parts->rad_exponent_extra, out_parts->rad_exponent_extra, in_baselog);
    fmpz_mul_2exp(mant, mant, remainder);
  }
  
  out_parts->rad_digits = fmpz_get_str(NULL, 1 << in_baselog, mant);
  radlen = strlen(out_parts->rad_digits);
  fmpz_add_si_inline(out_parts->rad_exponent_extra, out_parts->rad_exponent_extra, radlen);
  
  fmpz_sub(out_parts->rad_exponent_extra, out_parts->rad_exponent_extra, out_parts->exponent);
  
  out_parts->needs_radius_exponent = TRUE;
  
  out_parts->total_insignificant = radlen;
  out_parts->total_significant = midlen;
  
  /// Align digits ...
  if(fmpz_cmp_ui(out_parts->rad_exponent_extra, 0) < 0) {
    // 0.MMmmmm
    //   0.rrr    extra radius exp. XX = -2
    //     rrr0   => significant:= 2, insignificant := 4
    //
    // 0.MMmm
    //   0.rrr    XX = -2
    //   MMmm0    => significant:= 2, insignificant := 3
    
    const int max_extra_zeros = 10;
    if(fmpz_cmp_si(out_parts->rad_exponent_extra, -(midlen + max_extra_zeros)) >= 0) {
      out_parts->total_significant = -fmpz_get_si(out_parts->rad_exponent_extra);
      
      if(out_parts->total_insignificant < midlen - out_parts->total_significant)
        out_parts->total_insignificant = midlen - out_parts->total_significant;
      
      out_parts->needs_radius_exponent = FALSE;
    }
  }
  else { // all digits are insignificant
    //   0.mmm
    // 0.rrr     extra radius exp. XX = 2
    
    const int max_extra_radius_zeros = 10;
    if(fmpz_cmp_si(out_parts->rad_exponent_extra, radlen + max_extra_radius_zeros - midlen) <= 0) {
      out_parts->total_significant = 0;
      out_parts->mid_leading_zeros = fmpz_get_si(out_parts->rad_exponent_extra);
      
      if(out_parts->total_insignificant < out_parts->mid_leading_zeros + midlen)
        out_parts->total_insignificant = out_parts->mid_leading_zeros + midlen;
        
      out_parts->needs_radius_exponent = FALSE;
    }
  }
  
  fmpz_clear(mant);
}

PMATH_PRIVATE 
void _pmath_raw_number_parts_set_decimal_point(struct _pmath_raw_number_parts_t *parts, int num_integer_digits) {
  assert(num_integer_digits <= parts->total_significant);
  
  int exp_delta = parts->num_integer_digits - num_integer_digits;
  parts->num_integer_digits = num_integer_digits;
  fmpz_add_si_inline(parts->exponent, parts->exponent, exp_delta);  
}

PMATH_PRIVATE 
void _pmath_raw_number_parts_set_decimal_point_automatic(struct _pmath_raw_number_parts_t *parts) {
  if(fmpz_fits_si(parts->exponent)) {
    slong exp_i = fmpz_get_si(parts->exponent);
    
    if(-4 <= exp_i && exp_i <= 6) {
      if(exp_i < parts->total_significant) {
        _pmath_raw_number_parts_set_decimal_point(parts, exp_i);
        return;
      }
    }
  }
  
  if(parts->total_significant >= 1)
    _pmath_raw_number_parts_set_decimal_point(parts, 1);
}

#define RADIUS_DIGITS 3

PMATH_PRIVATE 
void _pmath_mpfloat_get_raw_number_parts(
  struct _pmath_raw_number_parts_t *result,
  pmath_mpfloat_t                   value,
  int                               base,
  int                               rad_digits
) {
  assert(result != NULL);
  assert(base >= 2 && base <= 36);
  assert(pmath_is_mpfloat(value));
  
  raw_number_parts_init(result);
  
  result->base = base;
  if(rad_digits == INT_MAX) {
    switch(base) {
      case  2: get_str_2exp_parts_new(result, PMATH_AS_ARB(value), 1); return;
      case  4: get_str_2exp_parts_new(result, PMATH_AS_ARB(value), 2); return;
      case  8: get_str_2exp_parts_new(result, PMATH_AS_ARB(value), 3); return;
      case 16: get_str_2exp_parts_new(result, PMATH_AS_ARB(value), 4); return;
      case 32: get_str_2exp_parts_new(result, PMATH_AS_ARB(value), 5); return;
    }
  }
  
  if(rad_digits > 10)
    rad_digits = 10;
  
  get_str_parts_new(
    result,
    PMATH_AS_ARB(value),
    base,
    rad_digits,//allow_inaccurate_digits ? RADIUS_DIGITS : 0,
    PMATH_AS_ARB_WORKING_PREC(value));
}

PMATH_PRIVATE
void _pmath_write_number_part(
  const struct _pmath_raw_number_parts_t  *parts,
  enum _pmath_number_part_t                which,
  void                                   (*writer)(void*, const char*, int), 
  void                                    *ctx
) {
  switch(which) {
    case PMATH_NUMBER_PART_BASE: {
      char buf[3];
      snprintf(buf, sizeof(buf), "%d", parts->base);
      writer(ctx, buf, strlen(buf));
    } break;
    
    case PMATH_NUMBER_PART_SIGNIFICANT: {
      if(parts->num_integer_digits <= 0) {
        writer(ctx, "0.", 2);
      }
      else {
        _pmath_write_number_part(parts, PMATH_NUMBER_PART_SIGNIFICANT_INT_DIGITS, writer, ctx);
        writer(ctx, ".", 1);
      }
      
      _pmath_write_number_part(parts, PMATH_NUMBER_PART_SIGNIFICANT_FRAC_DIGITS, writer, ctx);
    } break;
    
    case PMATH_NUMBER_PART_SIGNIFICANT_INT_DIGITS: {
      int mid_len = strlen(parts->mid_digits);
      int num_int_digits = parts->num_integer_digits;

      assert(0 <= parts->total_significant);
      assert(num_int_digits <= parts->total_significant);
      
      if(num_int_digits > 0) {
        if(mid_len >= num_int_digits) {
          writer(ctx, parts->mid_digits, num_int_digits);
        }
        else {
          writer(ctx, parts->mid_digits, mid_len);
          write_ascii_zeros(num_int_digits - mid_len, writer, ctx);
        }
      }
    } break;
    
    case PMATH_NUMBER_PART_SIGNIFICANT_FRAC_DIGITS: {
      int mid_len = strlen(parts->mid_digits);
      int num_int_digits = parts->num_integer_digits;
      
      assert(0 <= parts->total_significant);
      assert(num_int_digits <= parts->total_significant);
      
      if(num_int_digits < 0) {
        write_ascii_zeros(-num_int_digits, writer, ctx);
        num_int_digits = 0;
      }
      
      if(mid_len >= parts->total_significant) {
        writer(ctx, parts->mid_digits + num_int_digits, parts->total_significant - num_int_digits);
      }
      else if(mid_len >= num_int_digits) {
        writer(ctx, parts->mid_digits + num_int_digits, mid_len - num_int_digits);
        write_ascii_zeros(parts->total_significant - mid_len, writer, ctx);
      }
      else {
        write_ascii_zeros(parts->total_significant - num_int_digits, writer, ctx);
      }
    } break;
    
    case PMATH_NUMBER_PART_MID_INSIGNIFICANT_DIGITS: {
      int mid_len = strlen(parts->mid_digits);
      
      assert(parts->mid_leading_zeros >= 0);
      assert(parts->total_significant + parts->total_insignificant >= mid_len + parts->mid_leading_zeros);
        
      if(parts->mid_leading_zeros > 0) {
        assert(parts->total_significant == 0);
        
        write_ascii_zeros(parts->mid_leading_zeros, writer, ctx);
      }
      
      if(mid_len > parts->total_significant) {
        writer(ctx, parts->mid_digits + parts->total_significant, mid_len - parts->total_significant);
        write_ascii_zeros(parts->total_significant + parts->total_insignificant - mid_len - parts->mid_leading_zeros, writer, ctx);
      }
      else {
        write_ascii_zeros(parts->total_insignificant, writer, ctx);
      }
    } break;
    
    case PMATH_NUMBER_PART_RADIUS_DIGITS: {
      pmath_bool_t fill_with_zeros = !parts->needs_radius_exponent;
      int rad_len = strlen(parts->rad_digits);
      
      if(fill_with_zeros) {
        assert(rad_len <= parts->total_insignificant);
      }
      
      writer(ctx, parts->rad_digits, rad_len);
      if(fill_with_zeros) {
        write_ascii_zeros(parts->total_insignificant - rad_len, writer, ctx);
      }
    } break;
    
    case PMATH_NUMBER_PART_RADIUS_DIGITS_1: {
      pmath_bool_t fill_with_zeros = !parts->needs_radius_exponent;
      int rad_len = strlen(parts->rad_digits);
      
      if(fill_with_zeros) {
        assert(rad_len == parts->total_insignificant || rad_len + 1 == parts->total_insignificant);
      }
      
      if(rad_len == 0) {
        writer(ctx, "0.", 2);
        if(fill_with_zeros && parts->total_insignificant > 1) {
          write_ascii_zeros(parts->total_insignificant - 1, writer, ctx);
        }
        else
          writer(ctx, "0", 1);
      }
      else {
        writer(ctx, parts->rad_digits, 1);
        writer(ctx, ".", 1);
        writer(ctx, parts->rad_digits + 1, rad_len - 1);
        if(fill_with_zeros && parts->total_insignificant > rad_len) {
          write_ascii_zeros(parts->total_insignificant - rad_len, writer, ctx);
        }
        else if(rad_len == 1)
          writer(ctx, "0", 1);
      }
    } break;
    
    case PMATH_NUMBER_PART_RADIUS_EXTRA_EXPONENT:
    case PMATH_NUMBER_PART_RADIUS_EXTRA_EXPONENT_1: {
      if(parts->needs_radius_exponent) {
        if(which == PMATH_NUMBER_PART_RADIUS_EXTRA_EXPONENT_1) {
          if(!fmpz_equal_ui(parts->rad_exponent_extra, 1)) {
            fmpz_t exp;
            fmpz_init(exp);
            fmpz_add_si_inline(exp, exp, -1);
            {
              char *tmp = fmpz_get_str(NULL, 10, exp);
              writer(ctx, tmp, strlen(tmp));
              free_fmpz_str(tmp);
            }
            fmpz_clear(exp);
          }
        }
        else {
          if(!fmpz_is_zero(parts->rad_exponent_extra)) {
            char *tmp = fmpz_get_str(NULL, 10, parts->rad_exponent_extra);
            writer(ctx, tmp, strlen(tmp));
            free_fmpz_str(tmp);
          }
        }
      }
      else {
        int exp = parts->num_integer_digits - parts->total_significant;
        if(which == PMATH_NUMBER_PART_RADIUS_EXTRA_EXPONENT_1)
          exp-= 1;
        
        if(exp != 0) {
          char tmp[10];
          snprintf(tmp, sizeof(tmp), "%d", exp);
          writer(ctx, tmp, strlen(tmp));
        }
      }
    } break;
    
    case PMATH_NUMBER_PART_RADIUS_EXPONENT:
    case PMATH_NUMBER_PART_RADIUS_EXPONENT_1: {
      fmpz_t exp;
      
      fmpz_init(exp);
      if(parts->needs_radius_exponent)
        fmpz_add_inline(exp, parts->exponent, parts->rad_exponent_extra);
      else
        fmpz_add_si_inline(exp, parts->exponent, parts->num_integer_digits - parts->total_significant);
      
      if(which == PMATH_NUMBER_PART_RADIUS_EXPONENT_1)
        fmpz_sub_si_inline(exp, exp, 1);
      
      if(!fmpz_is_zero(exp)) {
        char *tmp = fmpz_get_str(NULL, 10, exp);
        writer(ctx, tmp, strlen(tmp));
        free_fmpz_str(tmp);
      }
      
      fmpz_clear(exp);
    } break;

    case PMATH_NUMBER_PART_EXPONENT: {
      if(!fmpz_is_zero(parts->exponent)) {
        char *tmp = fmpz_get_str(NULL, 10, parts->exponent);
        writer(ctx, tmp, strlen(tmp));
        free_fmpz_str(tmp);
      }
    } break;
  }
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

static void raw_number_parts_init(struct _pmath_raw_number_parts_t *parts) {
  memset(parts, 0, sizeof(*parts));
  //fmpz_init(parts->exponent);
  //fmpz_init(parts->rad_exponent_extra);
  //parts->mid_digits = NULL;
  //parts->rad_digits = NULL;
}

PMATH_PRIVATE void _pmath_raw_number_parts_clear(struct _pmath_raw_number_parts_t *parts) {
  fmpz_clear(parts->exponent);
  fmpz_clear(parts->rad_exponent_extra);
  free_fmpz_str(parts->mid_digits);
  free_fmpz_str(parts->rad_digits);
}

static void free_fmpz_str(char *str) {
  // fmpz_get_str() uses mpz_get_str() which uses our memory functions (see memory.c)
  // But it might be that we fail to redirect flint_free() [too old flint lib]. Then flint_free(str) 
  // would call the system free() which would segfault.
  //
  // Note: this is not true any more in newer FLINT (e.g. 2.8.0). Those use flint_alloc() in 
  // fmpz_get_str() before calling mpz_get_str(). 
  // Thus we would segfault when using pmath_mem_free() instead of flint_free() here and
  // not redirecting in memory.c
  //
  // It would be better to either ditch old FLINT versions (which is the minimum version?) 
  // and call flint_free() here, or only call fmpz_get_str() with a preallocated buffer
  
  //flint_free(str);

  pmath_mem_free(str);
}
