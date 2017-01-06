#include <pmath-core/numbers-private.h>
#include <pmath-core/intervals-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>
#include <pmath-util/helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

#include <limits.h> // LONG_MAX


PMATH_PRIVATE
pmath_bool_t _pmath_equals_rational(pmath_t obj, int n, int d) {
  pmath_t part;

  if(!pmath_is_rational(obj))
    return FALSE;

  part = pmath_rational_numerator(obj);
  if(!pmath_is_int32(part) || n != PMATH_AS_INT32(part)) {
    pmath_unref(part);
    return FALSE;
  }

  pmath_unref(part);
  part = pmath_rational_denominator(obj);
  if(!pmath_is_int32(part) || d != PMATH_AS_INT32(part)) {
    pmath_unref(part);
    return FALSE;
  }

  pmath_unref(part);
  return TRUE;
}

PMATH_PRIVATE
pmath_bool_t _pmath_equals_rational_at(
  pmath_expr_t expr,
  size_t i,
  int n, int d
) {
  pmath_t part = pmath_expr_get_item(expr, i);

  if(_pmath_equals_rational(part, n, d)) {
    pmath_unref(part);
    return TRUE;
  }

  pmath_unref(part);
  return FALSE;
}

static pmath_integer_t _pow_i_abs(
  pmath_integer_t base,    // wont be freed
  unsigned long   exponent
) { // TODO: try to prevent overflow / gmp-out-of-memory
  pmath_mpint_t result;

  if(exponent == 1 || pmath_is_null(base))
    return pmath_ref(base);

  assert(pmath_is_integer(base));

  result = _pmath_create_mp_int(0);
  if(pmath_is_null(result))
    return PMATH_NULL;

  if(pmath_is_int32(base)) {
    if(PMATH_AS_INT32(base) < 0) {
      mpz_ui_pow_ui(
        PMATH_AS_MPZ(result),
        (unsigned) - PMATH_AS_INT32(base),
        exponent);

      if(exponent & 1)
        mpz_neg(PMATH_AS_MPZ(result), PMATH_AS_MPZ(result));
    }
    else {
      mpz_ui_pow_ui(
        PMATH_AS_MPZ(result),
        (unsigned)PMATH_AS_INT32(base),
        exponent);
    }
  }
  else {
    assert(pmath_is_mpint(base));

    mpz_pow_ui(
      PMATH_AS_MPZ(result),
      PMATH_AS_MPZ(base),
      exponent);
  }

  return _pmath_mp_int_normalize(result);
}

/* a = int_root(&new_b, old_b, c)

   means old_b^(1/c) = a * new_b^(1/c)

   if a or new_b would be 1, PMATH_NULL will be given.
   On Out-Of-Memory, both a and new_b may become PMATH_NULL
 */
static pmath_integer_t int_root(
  pmath_integer_t  *new_base,
  pmath_integer_t   old_base, // will be freed
  unsigned long     root_exp
) {
  pmath_bool_t  neg;
  pmath_mpint_t iroot;
  pmath_mpint_t prime_power;
  unsigned int  i;

  assert(pmath_is_integer(old_base));
  assert(new_base != NULL);

  if(pmath_is_int32(old_base)) {
    int32_t base = PMATH_AS_INT32(old_base);

    if(base == 1) {
      pmath_unref(old_base);
      *new_base = PMATH_NULL;
      return PMATH_NULL;
    }

    if(base == -1) {
      *new_base = old_base;
      return PMATH_NULL;
    }

    neg = base < 0;
  }
  else {
    assert(pmath_is_mpint(old_base));

    neg = mpz_sgn(PMATH_AS_MPZ(old_base)) < 0;
  }

  if(neg)
    old_base = pmath_number_neg(old_base);

  if(pmath_is_int32(old_base))
    old_base = _pmath_create_mp_int(PMATH_AS_INT32(old_base));

  iroot = _pmath_create_mp_int(0);
  if(pmath_is_null(iroot) || pmath_is_null(old_base)) {
    pmath_unref(iroot);
    pmath_unref(old_base);
    *new_base = PMATH_NULL;
    return PMATH_NULL;
  }

  if(mpz_root(PMATH_AS_MPZ(iroot), PMATH_AS_MPZ(old_base), root_exp)) {
    pmath_unref(old_base);
    if(neg)
      *new_base = PMATH_FROM_INT32(-1);
    else
      *new_base = PMATH_NULL;
    return _pmath_mp_int_normalize(iroot);
  }

  prime_power = _pmath_create_mp_int(0);
  if(pmath_is_null(prime_power)) {
    pmath_unref(iroot);
    pmath_unref(old_base);
    *new_base = PMATH_NULL;
    return PMATH_NULL;
  }

  for(i = 0; i < (unsigned int)_pmath_primes16bit_count; ++i) {
    if(mpz_cmp_ui(PMATH_AS_MPZ(iroot), i) < 0)
      break;

    if(mpz_divisible_ui_p(PMATH_AS_MPZ(old_base), _pmath_primes16bit[i])) {
      mpz_ui_pow_ui(
        PMATH_AS_MPZ(prime_power),
        _pmath_primes16bit[i],
        root_exp);

      if(mpz_divisible_p(PMATH_AS_MPZ(old_base), PMATH_AS_MPZ(prime_power))) {
        *new_base = _pmath_create_mp_int(0);

        if(pmath_is_null(*new_base))
          break;

        mpz_divexact(
          PMATH_AS_MPZ(*new_base),
          PMATH_AS_MPZ(old_base),
          PMATH_AS_MPZ(prime_power));

        pmath_unref(iroot);
        pmath_unref(prime_power);
        pmath_unref(old_base);

        if(neg)
          mpz_neg(PMATH_AS_MPZ(*new_base), PMATH_AS_MPZ(*new_base));

        *new_base = _pmath_mp_int_normalize(*new_base);
        return PMATH_FROM_INT32(_pmath_primes16bit[i]);
      }
    }
  }

  pmath_unref(iroot);
  pmath_unref(prime_power);

  if(neg)
    *new_base = pmath_number_neg(_pmath_mp_int_normalize(old_base));
  else
    *new_base = _pmath_mp_int_normalize(old_base);

  return PMATH_NULL;
}

static pmath_t _pow_qi(
  pmath_rational_t base,     // will be freed. not PMATH_NULL!
  long             exponent
) {
  pmath_integer_t num = pmath_rational_numerator(base);
  pmath_integer_t den = pmath_rational_denominator(base);

  assert(pmath_number_sign(base) != 0);

  pmath_unref(base);
  if(exponent < 0) {
    if(exponent == -exponent) { // overflow
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_POWER), 2,
               pmath_rational_new(den, num),
               pmath_number_neg(pmath_integer_new_slong(exponent)));
    }

    exponent = -exponent;
    base = num;
    num = den;
    den = base;
  }

  base = num;
  num = _pow_i_abs(base, (unsigned long)exponent);
  pmath_unref(base);

  base = den;
  den = _pow_i_abs(base, (unsigned long)exponent);
  pmath_unref(base);

  if(pmath_number_sign(den) < 0) {
    num = pmath_number_neg(num);
    den = pmath_number_neg(den);
  }

  if(pmath_equals(den, PMATH_FROM_INT32(1))) {
    pmath_unref(den);
    return num;
  }

  // GCD(n, d) = 1  =>  GCD(n^e, d^e) = 1
  // => canonicalization not needed
  return _pmath_create_quotient(num, den);
}

PMATH_PRIVATE
pmath_t _pow_fi( // returns struct _pmath_mp_float_t* iff null_on_errors is TRUE
  pmath_mpfloat_t base,  // will be freed. not PMATH_NULL!
  long            exponent,
  pmath_bool_t    null_on_errors
) {
  long lbaseexp;
  mpfr_rnd_t rnd = _pmath_current_rounding_mode();

  assert(pmath_is_mpfloat(base));

  if(exponent <= 0 && mpfr_zero_p(PMATH_AS_MP_VALUE(base)))
    return base;

  mpfr_get_d_2exp(&lbaseexp, PMATH_AS_MP_VALUE(base), rnd);

  if( (exponent < 0 &&
       0 == 2 * (unsigned long) - exponent) ||
      exponent * lbaseexp > MPFR_EMAX_DEFAULT)
  {
    pmath_unref(base);
    if(null_on_errors)
      return PMATH_NULL;

    pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
    return pmath_ref(_pmath_object_overflow);
  }

  if(exponent * lbaseexp < MPFR_EMIN_DEFAULT) {
    pmath_unref(base);
    if(null_on_errors)
      return PMATH_NULL;

    pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
    return pmath_ref(_pmath_object_underflow);
  }

  if(exponent == 1) {
    return base;
  }

  if(exponent == -1) {
    pmath_mpfloat_t result;

    result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(base)));

    if(!pmath_is_null(result)) {
      mpfr_ui_div(
        PMATH_AS_MP_VALUE(result),
        1,
        PMATH_AS_MP_VALUE(base),
        rnd);
    }

    pmath_unref(base);
    return _pmath_float_exceptions(result);
  }

  {
    pmath_mpfloat_t result;

    // z = x^y,    dy = 0, x > 0, y > 1
    // error    = dz = x^(y-1) * y * dx       (dy = 0)
    // bits(z)  = -log(2, dz / z) = -log(2, y * dx / x)
    //          = -log(2, y) - log(2, dx/x) = bits(x) - log(2, y)

    result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(base)));

    if(pmath_is_null(result)) {
      pmath_unref(base);
      return PMATH_NULL;
    }

    mpfr_pow_si(
      PMATH_AS_MP_VALUE(result),
      PMATH_AS_MP_VALUE(base),
      exponent,
      rnd);

    pmath_unref(base);
    return _pmath_float_exceptions(result);
  }
}

static void _mpfi_pow_si(mpfi_ptr result, mpfi_srcptr base, long exponent) {
  pmath_bool_t has_zero = mpfi_has_zero(base);
  
  if(mpfi_nan_p(base)) {
    mpfr_set_nan(&result->left);
    mpfr_set_nan(&result->right);
    return;
  }
  
  if(exponent > 0) {
    if(exponent & 1) { // positive odd, hence increasing
      mpfr_pow_si(&result->left,  &base->left,  exponent, MPFR_RNDD);
      mpfr_pow_si(&result->right, &base->right, exponent, MPFR_RNDU);
    }
    else { // even
      if( !mpfr_nan_p(&base->left) && !mpfr_nan_p(&base->right)) {
        if(mpfr_cmpabs(&base->left, &base->right) < 0) {
          mpfr_pow_si(&result->right, &base->right, exponent, MPFR_RNDU);
          if(has_zero) {
            mpfr_set_ui(&result->left, 0, MPFR_RNDD);
          }
          else {
            mpfr_pow_si(&result->left, &base->left, exponent, MPFR_RNDD);
          }
        }
        else {
          // should go to right, but that might alias base.right, we swap later
          mpfr_pow_si(&result->left, &base->left, exponent, MPFR_RNDU);
          if(has_zero) {
            // should go to left, but that might alias base.left, we swap later
            mpfr_set_ui(&result->right, 0, MPFR_RNDD);
          }
          else {
            // should go to left, but that might alias base.left, we swap later
            mpfr_pow_si(&result->right, &base->right, exponent, MPFR_RNDD);
          }
          
          // swapping left and right is needed now!
          mpfi_revert_if_needed(result);
        }
      }
    }
  }
  else if(exponent == 0) {
    mpfi_set_ui(result, 1);
    if(has_zero) {
      // TODO: what should RealInterval(0,0)^0 yield? Indeterminate like 0^0, or RealInterval(0,1) ?
      mpfi_put_ui(result, 0);
    }
  }
  else { // exponent < 0
    if(has_zero) {
      if(mpfi_is_zero(base)) {
        // TODO: what should RealInterval(0,0)^-n yield? Indeterminate like 0^-n, or RealInterval(-Infinity,Infinity) ?
        mpfr_set_inf(&result->left, -1);
        mpfr_set_inf(&result->right, 1);
        return;
      }
      
      if(!mpfi_is_nonneg(base) && !mpfi_is_nonpos(base)) { // base contains negative and positive numbers
        mpfr_set_inf(&result->left, -1);
        mpfr_set_inf(&result->right, 1);
        return;
      }
    }
    
    if(exponent & 1) { // negative odd, hence decreasing
      // should go to right, but that might alias base.right, we swap later
      mpfr_pow_si(&result->left, &base->left, exponent, MPFR_RNDU);
      
      // should go to left, but that might alias base.left, we swap later
      mpfr_pow_si(&result->right, &base->right, exponent, MPFR_RNDD);
        
      // swapping left and right is needed now!
      mpfi_revert_if_needed(result);
    }
    else { // even
      if(mpfr_sgn(&result->left) >= 0) { // right of zero, hence decreasing
        // should go to right, but that might alias base.right, we swap later
        mpfr_pow_si(&result->left, &base->left, exponent, MPFR_RNDU);
        
        // should go to left, but that might alias base.left, we swap later
        mpfr_pow_si(&result->right, &base->right, exponent, MPFR_RNDD);
        
        // swapping left and right is needed now!
        mpfi_revert_if_needed(result);
      }
      else { // left of zero, hence increasing
        mpfr_pow_si(&result->left,  &base->left,  exponent, MPFR_RNDD);
        mpfr_pow_si(&result->right, &base->right, exponent, MPFR_RNDU);
      }
    }
  }
}

static
pmath_t _pow_Ri(
  pmath_interval_t base,  // will be freed. not PMATH_NULL!
  long             exponent
) {
  pmath_bool_t has_zero = mpfi_has_zero(PMATH_AS_MP_INTERVAL(base));
  pmath_interval_t result = _pmath_create_interval_for_result(base);
  if(pmath_is_null(result)) {
    pmath_unref(base);
    return result;
  }
  
  _mpfi_pow_si(PMATH_AS_MP_INTERVAL(result), PMATH_AS_MP_INTERVAL(base), exponent);
  
  pmath_unref(base);
  return result;
}

static pmath_number_t evaluate_natural_power_of_number(
  pmath_number_t base, // will be freed
  unsigned long  exponent
) {
  assert(exponent <= LONG_MAX);

  if(pmath_is_double(base)) {
    double d = pow(PMATH_AS_DOUBLE(base), exponent);

    if( isfinite(d) &&
        ((d == 0) == (PMATH_AS_DOUBLE(base) == 0)))
    {
      pmath_unref(base);
      return PMATH_FROM_DOUBLE(d);
    }

    base = _pmath_convert_to_mp_float(base);
    if(pmath_is_null(base))
      return base;

    return _pow_fi(base, (long)exponent, TRUE);
  }

  if(pmath_is_int32(base)) {
    pmath_integer_t result = _pow_i_abs(base, exponent);
    pmath_unref(base);
    return result;
  }

  if(!pmath_is_pointer(base))
    return PMATH_NULL;

  switch(PMATH_AS_PTR(base)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT: {
        pmath_integer_t result = _pow_i_abs(base, exponent);
        pmath_unref(base);
        return result;
      }

    case PMATH_TYPE_SHIFT_QUOTIENT: {
        pmath_integer_t num = _pow_i_abs(PMATH_QUOT_NUM(base), exponent);
        pmath_integer_t den = _pow_i_abs(PMATH_QUOT_DEN(base), exponent);

        pmath_unref(base);
        // GCD(n, d) = 1  =>  GCD(n^e, d^e) = 1
        // => canonicalization not needed
        return _pmath_create_quotient(num, den);
      }

    case PMATH_TYPE_SHIFT_MP_FLOAT: {
        return _pow_fi(base, (long)exponent, TRUE);
      }
  }

  assert("not a number type" && 0);

  return base;
}

static pmath_number_t divide(
  pmath_number_t a, // will be freed
  pmath_number_t b  // will be freed
) {
  if(pmath_is_null(a) || pmath_is_null(b)) {
    pmath_unref(a);
    pmath_unref(b);
    return PMATH_NULL;
  }

  if(pmath_is_double(b)) {
    double y = PMATH_AS_DOUBLE(b);

    if(y == 0) {
      pmath_unref(b);
      return a;
    }

    y = 1 / y;
    if(isfinite(y) && y != 0) {
      pmath_unref(b);
      return _mul_nn(a, PMATH_FROM_DOUBLE(y));
    }

    b = _pmath_convert_to_mp_float(b);
    if(pmath_is_null(b))
      return a;

    b = _pow_fi(b, -1, TRUE);
    return _mul_nn(a, b);
  }

  if(pmath_is_int32(b)) {
    if(PMATH_AS_INT32(b) == 0) {
      pmath_unref(b);
      return a;
    }

    return _mul_nn(a, _pmath_create_quotient(PMATH_FROM_INT32(1), b));
  }

  assert(pmath_is_pointer(b));

  switch(PMATH_AS_PTR(b)->type_shift) {
    case PMATH_TYPE_SHIFT_MP_INT: {
        if(mpz_sgn(PMATH_AS_MPZ(b)) == 0) {
          pmath_unref(b);
          return a;
        }

        return _mul_nn(a, _pmath_create_quotient(PMATH_FROM_INT32(1), b));
      } break;

    case PMATH_TYPE_SHIFT_QUOTIENT: {
        pmath_integer_t num = pmath_rational_numerator(b);
        pmath_integer_t den = pmath_rational_denominator(b);

        pmath_unref(b);

        return _mul_nn(a, _pmath_create_quotient(den, num));
      } break;

    case PMATH_TYPE_SHIFT_MP_FLOAT: {
        b = _pow_fi(b, -1, TRUE);
        return _mul_nn(a, b);
      }
  }

  assert("not a number type" && 0);

  pmath_unref(a);
  pmath_unref(b);
  return PMATH_NULL;
}

static void expand_natural_power_of_complex(
  pmath_number_t *re_ptr,
  pmath_number_t *im_ptr,
  unsigned long   exponent
) {
  pmath_mpint_t bin;
  unsigned long k;
  pmath_number_t x, y, z;
  pmath_number_t dst[4];

  if( exponent == 1 ||
      !pmath_is_number(*re_ptr) ||
      !pmath_is_number(*im_ptr) ||
      pmath_number_sign(*re_ptr) == 0)
  {
    return;
  }

  if(exponent >= LONG_MAX) {
    pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
    pmath_unref(*re_ptr);
    pmath_unref(*im_ptr);
    *re_ptr = pmath_ref(_pmath_object_overflow);
    *im_ptr = pmath_ref(_pmath_object_overflow);
    return;
  }

  // (x+y)^n = Sum(Binomial(n, k) * x^(n-k) * y^k, k->0..n)
  // Binomial(n, k+1) = Binomial(n,k) * (n-k) / (k+1)

  x = *re_ptr;
  y = *im_ptr;

  dst[0] = PMATH_FROM_INT32(0);
  dst[1] = PMATH_FROM_INT32(0);
  dst[2] = PMATH_FROM_INT32(0);
  dst[3] = PMATH_FROM_INT32(0);

  z = evaluate_natural_power_of_number(pmath_ref(x), exponent);
  bin = _pmath_create_mp_int(1);
  if(pmath_is_null(bin)) {
    return;
  }

  for(k = 0; k < exponent; ++k) {
    dst[k & 3] = _add_nn(
                   dst[k & 3],
                   _mul_nn(
                     _pmath_mp_int_normalize(pmath_ref(bin)),
                     pmath_ref(z)));

    mpz_mul_ui(PMATH_AS_MPZ(bin), PMATH_AS_MPZ(bin), exponent - k);
    mpz_divexact_ui(PMATH_AS_MPZ(bin), PMATH_AS_MPZ(bin), k + 1);

    z = divide(_mul_nn(z, pmath_ref(y)), pmath_ref(x));
  }

  dst[exponent & 3] = _add_nn(dst[exponent & 3], _mul_nn(bin, z));

  pmath_unref(x);
  pmath_unref(y);

  *re_ptr = _add_nn(dst[0], pmath_number_neg(dst[2]));
  *im_ptr = _add_nn(dst[1], pmath_number_neg(dst[3]));
}

PMATH_PRIVATE
pmath_integer_t _pmath_factor_gcd_int(
  pmath_integer_t *a,
  pmath_integer_t *b)
{
  pmath_mpint_t aa, bb, dd, xx, yy;

  if(pmath_is_int32(*a))
    aa = _pmath_create_mp_int(PMATH_AS_INT32(*a));
  else
    aa = pmath_ref(*a);

  if(pmath_is_int32(*b))
    bb = _pmath_create_mp_int(PMATH_AS_INT32(*b));
  else
    bb = pmath_ref(*b);

  dd = _pmath_create_mp_int(0);
  xx = _pmath_create_mp_int(0);
  yy = _pmath_create_mp_int(0);
  if( !pmath_is_null(aa) &&
      !pmath_is_null(bb) &&
      !pmath_is_null(dd) &&
      !pmath_is_null(xx) &&
      !pmath_is_null(yy))
  {
    mpz_gcd(PMATH_AS_MPZ(dd), PMATH_AS_MPZ(aa), PMATH_AS_MPZ(bb));

    mpz_divexact(PMATH_AS_MPZ(xx), PMATH_AS_MPZ(aa), PMATH_AS_MPZ(dd));
    mpz_divexact(PMATH_AS_MPZ(yy), PMATH_AS_MPZ(bb), PMATH_AS_MPZ(dd));

    pmath_unref(aa);
    pmath_unref(*a);

    pmath_unref(bb);
    pmath_unref(*b);

    *a   = _pmath_mp_int_normalize(xx);
    *b   = _pmath_mp_int_normalize(yy);
    return _pmath_mp_int_normalize(dd);
  }

  pmath_unref(aa);
  pmath_unref(bb);
  pmath_unref(dd);
  pmath_unref(xx);
  pmath_unref(yy);

  return PMATH_NULL;
}

PMATH_PRIVATE
pmath_rational_t _pmath_factor_rationals(
  pmath_rational_t *a,
  pmath_rational_t *b)
{
  pmath_integer_t a_den, b_den;
  pmath_mpint_t mm, aa, bb;

  if(pmath_is_integer(*a) && pmath_is_integer(*b))
    return _pmath_factor_gcd_int(a, b);

  a_den = pmath_rational_denominator(*a);
  b_den = pmath_rational_denominator(*b);

  if(pmath_is_int32(a_den))
    a_den = _pmath_create_mp_int(PMATH_AS_INT32(a_den));

  if(pmath_is_int32(b_den))
    b_den = _pmath_create_mp_int(PMATH_AS_INT32(b_den));

  mm = _pmath_create_mp_int(0);
  aa = _pmath_create_mp_int(0);
  bb = _pmath_create_mp_int(0);
  if( !pmath_is_null(a_den) &&
      !pmath_is_null(b_den) &&
      !pmath_is_null(mm) &&
      !pmath_is_null(aa) &&
      !pmath_is_null(bb))
  {
    pmath_integer_t result, a_num, b_num;

    mpz_lcm(PMATH_AS_MPZ(mm), PMATH_AS_MPZ(a_den), PMATH_AS_MPZ(b_den));

    mpz_divexact(PMATH_AS_MPZ(aa), PMATH_AS_MPZ(mm), PMATH_AS_MPZ(a_den));
    mpz_divexact(PMATH_AS_MPZ(bb), PMATH_AS_MPZ(mm), PMATH_AS_MPZ(b_den));

    a_num = pmath_rational_numerator(*a);
    b_num = pmath_rational_numerator(*b);

    a_num = _mul_ii(a_num, aa);
    b_num = _mul_ii(b_num, bb);

    aa = PMATH_NULL;
    bb = PMATH_NULL;

    result = _pmath_factor_gcd_int(&a_num, &b_num);

    if(!pmath_is_null(result)) {

      pmath_unref(a_den);
      pmath_unref(b_den);
      pmath_unref(*a);
      pmath_unref(*b);

      *a = a_num;
      *b = b_num;

      return pmath_rational_new(result, mm);
    }

    pmath_unref(a_num);
    pmath_unref(b_num);
  }

  pmath_unref(a_den);
  pmath_unref(b_den);
  pmath_unref(mm);
  pmath_unref(aa);
  pmath_unref(bb);

  return PMATH_NULL;
}

static pmath_rational_t factor_complex(pmath_expr_t *z) {
  pmath_t re, im;

  if( _pmath_re_im(pmath_ref(*z), &re, &im) &&
      pmath_is_rational(re) &&
      pmath_is_rational(im))
  {
    pmath_rational_t result = _pmath_factor_rationals(
                                (pmath_rational_t *)&re,
                                (pmath_rational_t *)&im);

    pmath_unref(*z);
    *z = COMPLEX(re, im);
    return result;
  }

  pmath_unref(re);
  pmath_unref(im);
  return INT(1);
}

static pmath_t expand_numeric_power_of_product(pmath_expr_t power) {
  pmath_expr_t product  = pmath_expr_get_item(power, 1);
  pmath_expr_t exponent = pmath_expr_get_item(power, 2);

  size_t numeric_factor_index = 0;
  pmath_bool_t has_symbolic_factor = 0;

  size_t i;
  for(i = pmath_expr_length(product); i > 0; --i) {
    pmath_t factor = pmath_expr_get_item(product, i);

    int factor_class = _pmath_number_class(factor);

    if(factor_class & PMATH_CLASS_NEGONE) {
      pmath_unref(factor);
    }
    else if(factor_class & PMATH_CLASS_REAL) {
      pmath_unref(factor);

      if(numeric_factor_index == 0)
        numeric_factor_index = i;

      if(has_symbolic_factor)
        break;

//      if(factor_class & PMATH_CLASS_NEG) {
//        has_symbolic_factor = TRUE;
//        break;
//      }
    }
    else if(pmath_is_expr_of_len(factor, PMATH_SYMBOL_COMPLEX, 2)) {
      pmath_rational_t gcd = factor_complex(&factor);

      pmath_unref(factor);

      has_symbolic_factor = TRUE;
      if(!pmath_same(gcd, INT(1))) {
        pmath_unref(gcd);
        numeric_factor_index = i;
        break;
      }
    }
    else {
      pmath_unref(factor);

      has_symbolic_factor = TRUE;

      if(numeric_factor_index > 0)
        break;
    }
  }

  if(numeric_factor_index > 0 && has_symbolic_factor) {
    pmath_t numeric_factors;

    pmath_gather_begin(PMATH_NULL);

    for(i = numeric_factor_index; i > 0; --i) {
      pmath_t factor = pmath_expr_get_item(product, i);

      int factor_class = _pmath_number_class(factor);

      if(factor_class & (PMATH_CLASS_NEG & ~PMATH_CLASS_NEGONE)) {
        pmath_emit(NEG(factor), PMATH_NULL);
        product = pmath_expr_set_item(product, i, INT(-1));
      }
      else if(factor_class & (PMATH_CLASS_REAL & ~PMATH_CLASS_NEGONE)) {
        pmath_emit(factor, PMATH_NULL);
        product = pmath_expr_set_item(product, i, INT(1));
      }
      else if(pmath_is_expr_of_len(factor, PMATH_SYMBOL_COMPLEX, 2)) {
        pmath_rational_t gcd = factor_complex(&factor);

        if(pmath_same(gcd, INT(1))) {
          pmath_unref(factor);
        }
        else {
          pmath_emit(gcd, PMATH_NULL);
          product = pmath_expr_set_item(product, i, factor);
        }
      }
      else
        pmath_unref(factor);
    }

    numeric_factors = pmath_gather_end();
    numeric_factors = pmath_expr_set_item(numeric_factors, 0, pmath_ref(PMATH_SYMBOL_TIMES));
    numeric_factors = POW(numeric_factors, exponent);
    power = pmath_expr_set_item(power, 1, product);
    power = TIMES(numeric_factors, power);
    return power;
  }

  for(i = numeric_factor_index; i > 0; --i) {
    pmath_t factor = pmath_expr_get_item(product, i);

    int factor_class = _pmath_number_class(factor);

    if(factor_class & PMATH_CLASS_REAL) {
      if(factor_class & PMATH_CLASS_NEG)
        factor = NEG(factor);

      factor = POW(factor, pmath_expr_get_item(power, 2));
      factor = pmath_evaluate(factor);

      if(!pmath_is_expr_of(factor, PMATH_SYMBOL_POWER)) {
        if(factor_class & PMATH_CLASS_NEG)
          product = pmath_expr_set_item(product, i, INT(-1));
        else
          product = pmath_expr_set_item(product, i, INT(1));

        power = pmath_expr_set_item(power, 1, product);
        power = TIMES(factor, power);
        return power;
      }
    }

    pmath_unref(factor);
  }

  pmath_unref(product);
  pmath_unref(exponent);

  return power;
}

PMATH_PRIVATE pmath_t builtin_power(pmath_expr_t expr) {
  pmath_t base;
  pmath_t exponent;
  int base_class;
  int exp_class;

  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  base =     pmath_expr_get_item(expr, 1);
  exponent = pmath_expr_get_item(expr, 2);

  if(pmath_is_int32(exponent)) {
    if(pmath_is_double(base) && PMATH_AS_DOUBLE(base) != 0) {
      double result = pow(PMATH_AS_DOUBLE(base), PMATH_AS_INT32(exponent));

      if(isfinite(result)) {
        pmath_unref(expr);
        pmath_unref(base);
        return PMATH_FROM_DOUBLE(result);
      }

      base = _pmath_convert_to_mp_float(base);
    }

    if(pmath_is_mpfloat(base)) {
      if( PMATH_AS_INT32(exponent) > 0 ||
          !mpfr_zero_p(PMATH_AS_MP_VALUE(base)))
      {
        pmath_unref(expr);
        return _pow_fi(base, PMATH_AS_INT32(exponent), FALSE);
      }
    }
    
    if(pmath_is_interval(base)) {
      pmath_unref(expr);
      return _pow_Ri(base, PMATH_AS_INT32(exponent));
    }

    if( pmath_is_rational(base) &&
        !pmath_equals(base, PMATH_FROM_INT32(0)))
    { // (p / q) ^ n
      pmath_unref(expr);
      return _pow_qi(base, PMATH_AS_INT32(exponent));
    }

    if(pmath_is_expr_of_len(base, PMATH_SYMBOL_COMPLEX, 2)) {
      pmath_t re = pmath_expr_get_item(base, 1);
      pmath_t im = pmath_expr_get_item(base, 2);

      if(pmath_is_number(re) && pmath_is_number(im)) {
        if(pmath_number_sign(re) == 0) {
          // (I im)^n = I^n im^n
          int lexp4 = PMATH_AS_INT32(exponent) % 4;

          if(lexp4 < 0)
            lexp4 += 4;

          pmath_unref(re);
          pmath_unref(base);
          expr = pmath_expr_set_item(expr, 1, im);
          switch(lexp4) {
            case 1: return COMPLEX(INT(0), expr);
            case 2: return NEG(expr);
            case 3: return COMPLEX(INT(0), NEG(expr));
          }

          return expr;
        }

        // (re + I im)^n
        if(PMATH_AS_INT32(exponent) > 0) {
          pmath_unref(expr);
          expand_natural_power_of_complex(&re, &im, (unsigned long)PMATH_AS_INT32(exponent));

          base = pmath_expr_set_item(base, 1, re);
          base = pmath_expr_set_item(base, 2, im);
          return base;
        }

        if(PMATH_AS_INT32(exponent) < 0) {
          pmath_unref(expr);
          pmath_unref(base);

          expand_natural_power_of_complex(&re, &im, (unsigned long) - PMATH_AS_INT32(exponent));

          base = pmath_evaluate(
                   PLUS(
                     TIMES(pmath_ref(re), pmath_ref(re)),
                     TIMES(pmath_ref(im), pmath_ref(im))));

          expr = COMPLEX(DIV(re, pmath_ref(base)), NEG(DIV(im, pmath_ref(base))));
          pmath_unref(base);
          return expr;
        }
      }

      pmath_unref(re);
      pmath_unref(im);
    }

    if( PMATH_AS_INT32(exponent) > 0 &&
        pmath_is_expr_of(base, PMATH_SYMBOL_TIMES))
    { // (x * y) ^ n
      size_t i;

      pmath_unref(expr);
      for(i = pmath_expr_length(base); i > 0; --i) {
        base = pmath_expr_set_item(
                 base, i,
                 pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_POWER), 2,
                   pmath_expr_get_item(base, i),
                   exponent));  // exponent is no pointer, so pmath_ref() not necessary
      }

      return base;
    }
  }

  if(pmath_is_mpint(exponent)) { // not fitting to int32_t !
    if(pmath_is_number(base)) {
      pmath_unref(expr);

      if(pmath_is_int32(base)) {
        if(PMATH_AS_INT32(base) == -1) {
          if(mpz_odd_p(PMATH_AS_MPZ(exponent))) {
            pmath_unref(exponent);
            return base;
          }

          pmath_unref(exponent);
          return PMATH_FROM_INT32(1);
        }

        if(PMATH_AS_INT32(base) == 0 || PMATH_AS_INT32(base) == 1) {
          pmath_unref(exponent);
          return base;
        }
      }

      if(pmath_is_double(base)) {
        double d = PMATH_AS_DOUBLE(base);

        if(d == -1) {
          if(mpz_odd_p(PMATH_AS_MPZ(exponent))) {
            pmath_unref(exponent);
            return base;
          }

          pmath_unref(exponent);
          pmath_unref(base);
          return PMATH_FROM_DOUBLE(1.0);
        }

        if(d == 0 || d == 1) {
          pmath_unref(exponent);
          return base;
        }
      }

      pmath_unref(base);
      if(mpz_sgn(PMATH_AS_MPZ(exponent)) < 0) {
        pmath_unref(exponent);
        pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
        return pmath_ref(_pmath_object_underflow);
      }

      pmath_unref(exponent);
      pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
      return pmath_ref(_pmath_object_overflow);
    }

    if(_pmath_is_nonreal_complex(base)) {
      pmath_unref(expr);

      expr = pmath_expr_get_item(base, 1); // real part
      if(pmath_is_number(expr) && pmath_number_sign(expr) == 0) {
        pmath_unref(expr);

        expr = pmath_expr_get_item(base, 2); // imaginary part
        if(pmath_is_int32(expr)) {
          int32_t si = PMATH_AS_INT32(expr);

          if(si == 1 || si == -1) {
            unsigned exp_mod4 = 3u & mpz_get_ui(PMATH_AS_MPZ(exponent)); // truncateing

            if(exp_mod4 != 0 && mpz_sgn(PMATH_AS_MPZ(exponent)) < 0) {
              exp_mod4 = 4 - exp_mod4;
            }

            pmath_unref(expr);
            pmath_unref(base);
            pmath_unref(exponent);
            switch(exp_mod4) {
              case 0: return INT(1);
              case 1: return COMPLEX(INT(0), INT(si));
              case 2: return INT(-1);
              case 3: return COMPLEX(INT(0), INT(-si));
            }

            assert("unreachable code reached" && 0);
            return PMATH_NULL;
          }
        }
        else if(pmath_is_double(expr)) {
          double d = PMATH_AS_DOUBLE(expr);

          if(d == 1 || d == -1) {
            unsigned exp_mod4 = 3u & mpz_get_ui(PMATH_AS_MPZ(exponent));

            if(exp_mod4 != 0 && mpz_sgn(PMATH_AS_MPZ(exponent)) < 0) {
              exp_mod4 = 4 - exp_mod4;
            }

            pmath_unref(expr);
            pmath_unref(base);
            pmath_unref(exponent);
            switch(exp_mod4) {
              case 0: return PMATH_FROM_DOUBLE(1.0);
              case 1: return COMPLEX(INT(0), PMATH_FROM_DOUBLE(d));
              case 2: return PMATH_FROM_DOUBLE(-1.0);
              case 3: return COMPLEX(INT(0), PMATH_FROM_DOUBLE(-d));
            }

            assert("unreachable code reached" && 0);
            return PMATH_NULL;
          }
        }
      }

      pmath_unref(expr);
      expr = pmath_evaluate(FUNC(pmath_ref(PMATH_SYMBOL_ABS), base));

      if(pmath_is_number(expr) && pmath_number_sign(expr) < 0) {
        pmath_unref(expr);

        if(mpz_sgn(PMATH_AS_MPZ(exponent)) < 0) {
          pmath_unref(exponent);
          pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
          return pmath_ref(_pmath_object_overflow);
        }

        pmath_unref(exponent);
        pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
        return pmath_ref(_pmath_object_underflow);
      }
      else if(mpz_sgn(PMATH_AS_MPZ(exponent)) < 0) {
        pmath_unref(expr);
        pmath_unref(exponent);
        pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
        return pmath_ref(_pmath_object_underflow);
      }

      pmath_unref(expr);
      pmath_unref(exponent);
      pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
      return pmath_ref(_pmath_object_overflow);
    }

    pmath_unref(base);
    pmath_unref(exponent);
    return expr;
  }

  if(pmath_is_quotient(exponent)) {
    if(pmath_is_rational(base) && !pmath_equals(base, INT(0))) { // (a/b)^(c/d)
      pmath_integer_t exp_num;
      pmath_integer_t exp_den;

      if(pmath_number_sign(exponent) < 0) {
        if(pmath_is_integer(base)) {
          pmath_t expr2 = POW(base, pmath_number_neg(exponent));

          expr2 = pmath_evaluate(expr2);
          if(pmath_is_expr_of(expr2, PMATH_SYMBOL_POWER)) {
            pmath_unref(expr2);
            return expr;
          }

          pmath_unref(expr);
          if(pmath_is_rational(expr2) && pmath_number_sign(expr2) != 0) {
            exp_num = pmath_rational_numerator(expr2);
            exp_den = pmath_rational_denominator(expr2);

            pmath_unref(expr2);
            return pmath_rational_new(exp_den, exp_num);
          }

          return INV(expr2);
        }

        pmath_unref(expr);

        expr = POW(
                 pmath_rational_new(
                   pmath_rational_denominator(base),
                   pmath_rational_numerator(base)),
                 pmath_number_neg(exponent));

        pmath_unref(base);
        return expr;
      }

      exp_num = pmath_rational_numerator(exponent);
      exp_den = pmath_rational_denominator(exponent);

      assert(pmath_is_integer(exp_num));
      assert(pmath_is_integer(exp_den));

      if(pmath_is_int32(exp_den)) {
        pmath_integer_t base_num;
        pmath_integer_t base_den;
        pmath_integer_t base_num_root;
        pmath_integer_t base_den_root;
        pmath_bool_t outside_one;

        assert(PMATH_AS_INT32(exp_den) > 0);

        if(pmath_is_int32(exp_num)) {
          outside_one = PMATH_AS_INT32(exp_num) >  PMATH_AS_INT32(exp_den) ||
                        PMATH_AS_INT32(exp_num) < -PMATH_AS_INT32(exp_den);
        }
        else {
          assert(pmath_is_mpint(exp_num));

          outside_one = 0 < mpz_cmpabs_ui(PMATH_AS_MPZ(exp_num), (unsigned long)PMATH_AS_INT32(exp_den));
        }

        if(outside_one) {
          pmath_integer_t qexp;
          pmath_integer_t rexp;
          pmath_unref(exponent);

          if(pmath_is_int32(exp_num))
            exp_num = _pmath_create_mp_int(PMATH_AS_INT32(exp_num));

          qexp = _pmath_create_mp_int(0);
          rexp = _pmath_create_mp_int(0);
          if(pmath_is_null(exp_num) || pmath_is_null(qexp) || pmath_is_null(rexp)) {
            pmath_unref(qexp);
            pmath_unref(rexp);
            pmath_unref(exp_num);
            pmath_unref(base);
            pmath_unref(expr);
            return PMATH_NULL;
          }

          mpz_tdiv_qr_ui(
            PMATH_AS_MPZ(qexp),
            PMATH_AS_MPZ(rexp),
            PMATH_AS_MPZ(exp_num),
            PMATH_AS_INT32(exp_den));

          pmath_unref(exp_num);

          qexp = _pmath_mp_int_normalize(qexp);
          rexp = _pmath_mp_int_normalize(rexp);

          expr = pmath_expr_set_item(
                   expr, 2,
                   _pmath_create_quotient(rexp, exp_den));

          return TIMES(POW(base, qexp), expr);
        }

        if( PMATH_AS_INT32(exp_den) == 2 &&
            pmath_number_sign(base) < 0)
        {
          // so exp_num = 1 or -1

          pmath_unref(expr);
          pmath_unref(exp_den);

          // 1/I = -I
          expr = COMPLEX(INT(0), INT(pmath_number_sign(exp_num)));

          pmath_unref(exp_num);
          base = pmath_number_neg(base);

          return TIMES(POW(base, exponent), expr);
        }

        base_num = pmath_rational_numerator(base);
        base_den = pmath_rational_denominator(base);

        base_num_root = int_root(&base_num, base_num, (unsigned long)PMATH_AS_INT32(exp_den));
        base_den_root = int_root(&base_den, base_den, (unsigned long)PMATH_AS_INT32(exp_den));

        if(!pmath_is_null(base_num_root) || !pmath_is_null(base_den_root)) {
          pmath_t result;

          if(pmath_is_null(base_den_root)) {
            result = base_num_root;
          }
          else if(pmath_is_null(base_num_root)) {
            result = _pmath_create_quotient(
                       PMATH_FROM_INT32(1),
                       base_den_root);
          }
          else {
            result = _pmath_create_quotient(
                       base_num_root,
                       base_den_root);
          }

          pmath_unref(expr);

          // base_num_root, base_den_root, expr invalid now

          if(pmath_equals(exp_num, PMATH_FROM_INT32(1))) {
            pmath_unref(exp_num);
          }
          else {
            result = pmath_expr_new_extended(
                       pmath_ref(PMATH_SYMBOL_POWER), 2,
                       result,
                       exp_num);
          }

          // exp_num invalid now

          pmath_unref(base);
          if(!pmath_is_null(base_num)) {
            if(pmath_is_null(base_den))
              base = base_num;
            else
              base = _pmath_create_quotient(base_num, base_den);
          }
          else if(!pmath_is_null(base_den)) {
            exponent = pmath_number_neg(exponent);
            base = base_den;
          }
          else
            base = PMATH_NULL;

          // base_num, base_den invalid now

          if(!pmath_is_null(base))
            result = TIMES(result, POW(base, exponent));
          else
            pmath_unref(exponent);

          return result;
        }

        pmath_unref(base_num);
        pmath_unref(base_den);
        pmath_unref(base_num_root);
        pmath_unref(base_den_root);
      }
      else {
        assert(pmath_is_mpint(exp_den));

        if(pmath_is_int32(exp_num))
          exp_num = _pmath_create_mp_int(PMATH_AS_INT32(exp_num));

        if( !pmath_is_null(exp_num) &&
            0 < mpz_cmpabs(PMATH_AS_MPZ(exp_num), PMATH_AS_MPZ(exp_den)))
        {
          pmath_integer_t qexp;
          pmath_integer_t rexp;
          pmath_unref(exponent);

          qexp = _pmath_create_mp_int(0);
          rexp = _pmath_create_mp_int(0);
          if(pmath_is_null(qexp) || pmath_is_null(rexp)) {
            pmath_unref(qexp);
            pmath_unref(rexp);
            pmath_unref(exp_num);
            pmath_unref(exp_den);
            pmath_unref(base);
            pmath_unref(expr);
            return PMATH_NULL;
          }

          mpz_tdiv_qr(
            PMATH_AS_MPZ(qexp),
            PMATH_AS_MPZ(rexp),
            PMATH_AS_MPZ(exp_num),
            PMATH_AS_MPZ(exp_den));

          pmath_unref(exp_num);
          qexp = _pmath_mp_int_normalize(qexp);
          rexp = _pmath_mp_int_normalize(rexp);

          expr = pmath_expr_set_item(
                   expr, 2,
                   _pmath_create_quotient(rexp, exp_den));

          return TIMES(POW(base, qexp), expr);
        }
      }

      pmath_unref(exp_num);
      pmath_unref(exp_den);
    }

//    if(pmath_is_expr_of(base, PMATH_SYMBOL_TIMES)) { // (x * y) ^ (p/q)
//      pmath_unref(base);
//      pmath_unref(exponent);
//      return expand_numeric_power_of_product(expr);
//    }
  }

  if(pmath_is_quotient(base)) {
    pmath_integer_t num = pmath_rational_numerator(base);

    if(pmath_equals(num, PMATH_FROM_INT32(1))) {
      pmath_unref(num);
      expr = pmath_expr_set_item(expr, 1, pmath_rational_denominator(base));
      pmath_unref(base);

      return pmath_expr_set_item(expr, 2, NEG(exponent));
    }

    pmath_unref(num);
  }

  if(_pmath_is_inexact(exponent)) {
    if(pmath_equals(base, PMATH_SYMBOL_E)) {
      if(pmath_is_mpfloat(exponent)) {
        // dy = d(e^x) = e^x dx
        pmath_mpfloat_t result;

        result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(exponent)));
        if(!pmath_is_null(result)) {
          mpfr_exp(
            PMATH_AS_MP_VALUE(result),
            PMATH_AS_MP_VALUE(exponent),
            _pmath_current_rounding_mode());

          pmath_unref(exponent);
          pmath_unref(base);
          pmath_unref(expr);

          return _pmath_float_exceptions(result);
        }
      }

      if(_pmath_is_nonreal_complex(exponent)) { // E^(x + I y) = E^x (Cos(y) + I Sin(y))
        pmath_t re = pmath_expr_get_item(exponent, 1);
        pmath_t im = pmath_expr_get_item(exponent, 2);

        expr = pmath_expr_set_item(expr, 2, re);

        expr = TIMES(expr, COMPLEX(COS(pmath_ref(im)), SIN(pmath_ref(im))));

        pmath_unref(im);
        pmath_unref(exponent);
        pmath_unref(base);
        return expr;
      }
    }

    if( (pmath_is_double(base) && pmath_is_number(exponent)) ||
        (pmath_is_number(base) && pmath_is_double(exponent)))
    {
      double b = pmath_number_get_d(base);
      double e = pmath_number_get_d(exponent);

      if(b < 0) {
        double re = cos(M_PI * e);
        double im = sin(M_PI * e);
        double r = pow(-b, e);

        re *= r;
        im *= r;
        if(isfinite(re) && isfinite(im)) {
          pmath_unref(expr);
          pmath_unref(base);
          pmath_unref(exponent);
          return pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
                   PMATH_FROM_DOUBLE(re),
                   PMATH_FROM_DOUBLE(im));
        }
      }
      else {
        double result = pow(b, e);

        if(isfinite(result)) {
          pmath_unref(expr);
          pmath_unref(base);
          pmath_unref(exponent);
          return PMATH_FROM_DOUBLE(result);
        }
      }

      expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
      base = pmath_set_precision(base, LOG10_2 * DBL_MANT_DIG);
      expr = pmath_expr_set_item(expr, 1, base);

      if(pmath_is_double(exponent)) {
        expr = pmath_expr_set_item(expr, 2, PMATH_NULL);
        exponent = pmath_set_precision(exponent, LOG10_2 * DBL_MANT_DIG);
        expr = pmath_expr_set_item(expr, 2, exponent);
      }

      return expr;
    }

    if( pmath_is_mpfloat(base) &&
        pmath_is_mpfloat(exponent))
    {
      int basesign = mpfr_sgn(PMATH_AS_MP_VALUE(base));

      if(basesign < 0 && !mpfr_integer_p(PMATH_AS_MP_VALUE(exponent))) {
        // (-x)^y = E^(y Log(-x)) = E^(y (I Pi + Log(x))) = x^y * E^(I Pi y)
        //        = x^y * (Cos(Pi y) + I Sin(Pi y))
        pmath_unref(expr);

        expr = COMPLEX(
                 COS(TIMES(pmath_ref(exponent), pmath_ref(PMATH_SYMBOL_PI))),
                 SIN(TIMES(pmath_ref(exponent), pmath_ref(PMATH_SYMBOL_PI))));

        if(mpfr_cmp_si(PMATH_AS_MP_VALUE(base), -1) == 0) {
          pmath_unref(base);
          pmath_unref(exponent);
          return expr;
        }

        return TIMES(POW(pmath_number_neg(base), exponent), expr);
      }

      if(basesign != 0) {
        pmath_mpfloat_t result;
        mpfr_prec_t prec;

        prec = min_prec(mpfr_get_prec(PMATH_AS_MP_VALUE(base)),
                        mpfr_get_prec(PMATH_AS_MP_VALUE(exponent)));

        result = _pmath_create_mp_float(prec);
        if(!pmath_is_null(result)) {
//          if(mpfr_zero_p(PMATH_AS_MP_ERROR(result))) {
//            pmath_unref(result);
//            pmath_unref(exponent);
//            pmath_unref(base);
//            pmath_unref(expr);
//            pmath_message(PMATH_SYMBOL_GENERAL, "unfl", 0);
//            return pmath_ref(_pmath_object_underflow);
//          }
//
//          if(!mpfr_number_p(PMATH_AS_MP_ERROR(result))) {
//            pmath_unref(result);
//            pmath_unref(exponent);
//            pmath_unref(base);
//            pmath_unref(expr);
//            pmath_message(PMATH_SYMBOL_GENERAL, "ovfl", 0);
//            return pmath_ref(_pmath_object_overflow);
//          }

          mpfr_pow(
            PMATH_AS_MP_VALUE(result),
            PMATH_AS_MP_VALUE(base),
            PMATH_AS_MP_VALUE(exponent),
            _pmath_current_rounding_mode());

          pmath_unref(exponent);
          pmath_unref(base);
          pmath_unref(expr);

          result = _pmath_float_exceptions(result);

          return result;
        }

      }
    }

    if( _pmath_is_nonreal_complex(exponent) ||
        _pmath_is_nonreal_complex(base))
    {
      // x^y = Exp(y Log(x))

      expr = pmath_expr_set_item(expr, 1, pmath_ref(PMATH_SYMBOL_E));
      expr = pmath_expr_set_item(expr, 2, TIMES(exponent, LOG(base)));
      return expr;
    }

    if(!_pmath_is_inexact(base) && !pmath_same(base, PMATH_FROM_INT32(0))) {
      double prec = pmath_precision(exponent); // frees exponent

      base = pmath_set_precision(base, prec);
      expr = pmath_expr_set_item(expr, 1, base);
      return expr;
    }
  }
  else if(_pmath_is_inexact(base)) {
    if(!_pmath_is_inexact(exponent)) {
      double prec = pmath_precision(base); // frees base

      exponent = pmath_set_precision(exponent, prec);
      expr     = pmath_expr_set_item(expr, 2, exponent);
      return expr;
    }
  }

  if(pmath_is_expr_of_len(base, PMATH_SYMBOL_POWER, 2)) { // (x^y)^exponent
    pmath_t inner_exp = pmath_expr_get_item(base, 2);
    pmath_t inner_base;

    if(pmath_is_integer(exponent)) {
      pmath_unref(expr);
      return pmath_expr_set_item(base, 2, TIMES(exponent, inner_exp));
    }

    if(pmath_is_rational(inner_exp)) {
      int sign = pmath_number_sign(inner_exp);

      if(sign * pmath_compare(inner_exp, INT(sign)) < 0) {
        pmath_unref(expr);
        return pmath_expr_set_item(base, 2, TIMES(exponent, inner_exp));
      }
    }

    inner_base = pmath_expr_get_item(base, 1);
    if( (_pmath_number_class(inner_base) & PMATH_CLASS_POS) &&
        (_pmath_number_class(inner_exp) & PMATH_CLASS_REAL))
    {
      pmath_unref(expr);
      pmath_unref(inner_base);
      return pmath_expr_set_item(base, 2, TIMES(inner_exp, exponent));
    }

    pmath_unref(inner_base);
    pmath_unref(inner_exp);
  }

  base_class = _pmath_number_class(base);
  exp_class  = _pmath_number_class(exponent);

  if(exp_class & (PMATH_CLASS_CINF | PMATH_CLASS_UINF)) {
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  if(exp_class & PMATH_CLASS_POSINF) {
    if(base_class & (PMATH_CLASS_ZERO | PMATH_CLASS_SMALL)) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return INT(0);
    }

    if(base_class & (PMATH_CLASS_INF | PMATH_CLASS_NEGBIG)) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }

    if(base_class & (PMATH_CLASS_POSBIG)) {
      pmath_unref(base);
      pmath_unref(expr);
      return exponent;
    }

    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  if(exp_class & PMATH_CLASS_ZERO) {
    if(base_class & (PMATH_CLASS_ZERO | PMATH_CLASS_INF)) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_message(PMATH_NULL, "indet", 1, expr);
      return pmath_ref(PMATH_SYMBOL_UNDEFINED);
    }

    pmath_unref(base);
    pmath_unref(exponent);
    pmath_unref(expr);
    return INT(1);
  }

  if(base_class & PMATH_CLASS_ZERO) {
    if(exp_class & PMATH_CLASS_NEG) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_message(PMATH_NULL, "infy", 1, expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }

    if(exp_class & PMATH_CLASS_POS) {
      pmath_unref(exponent);
      pmath_unref(expr);
      return base;
    }

    if(exp_class & PMATH_CLASS_UNKNOWN) {
      pmath_unref(base);
      pmath_unref(exponent);
      return expr;
    }

    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  if(base_class & (PMATH_CLASS_NEGINF | PMATH_CLASS_CINF)) {
    if(exp_class & PMATH_CLASS_NEG) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return INT(0);
    }

    if(exp_class & PMATH_CLASS_POSINF) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }

    if(exp_class & PMATH_CLASS_POS) {
      expr = pmath_expr_set_item(
               expr, 1,
               pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_SIGN), 1,
                 base));
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1, expr);
    }

    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  if(base_class & PMATH_CLASS_POSINF) {
    if(exp_class & PMATH_CLASS_NEG) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return INT(0);
    }

    if(exp_class & PMATH_CLASS_POS) {
      pmath_unref(exponent);
      pmath_unref(expr);
      return base;
    }

    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  if(exp_class & PMATH_CLASS_NEGINF) {
    if(base_class & (PMATH_CLASS_BIG | PMATH_CLASS_INF)) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return INT(0);
    }

    if(base_class & PMATH_CLASS_NEGSMALL) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_complex_infinity);
    }

    if(base_class & PMATH_CLASS_POSSMALL) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return pmath_ref(_pmath_object_pos_infinity);
    }

    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  if(base_class & PMATH_CLASS_POSONE) {
    pmath_unref(exponent);
    pmath_unref(expr);
    return base;
  }

  if(base_class & PMATH_CLASS_UINF) {
    if(exp_class & PMATH_CLASS_NEG) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_unref(expr);
      return INT(0);
    }

    if(exp_class & PMATH_CLASS_POS) {
      pmath_unref(exponent);
      pmath_unref(expr);
      return base;
    }

    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  if(exp_class & PMATH_CLASS_POSONE) {
    pmath_unref(exponent);
    pmath_unref(expr);
    return base;
  }

  if(exp_class & (PMATH_CLASS_REAL | PMATH_CLASS_COMPLEX)) {
    if(pmath_is_expr_of(base, PMATH_SYMBOL_TIMES)) {
      pmath_unref(base);
      pmath_unref(exponent);
      return expand_numeric_power_of_product(expr);
    }

    if(pmath_is_expr_of_len(base, PMATH_SYMBOL_COMPLEX, 2)) {
      pmath_rational_t factor = factor_complex(&base);

      pmath_unref(exponent);
      if(pmath_same(factor, INT(1))) {
        pmath_unref(base);
        return expr;
      }

      expr = pmath_expr_set_item(expr, 1, base);
      return TIMES(factor, expr);
    }
  }

  if(pmath_is_expr_of(base, PMATH_SYMBOL_TIMES)) { // (x y)^z = x^z y^z, if x > 0
    pmath_unref(base);
    pmath_unref(exponent);
    return expand_numeric_power_of_product(expr);
  }

  if( pmath_equals(exponent, _pmath_object_overflow) ||
      pmath_equals(exponent, _pmath_object_underflow) ||
      pmath_equals(base, _pmath_object_overflow) ||
      pmath_equals(base, _pmath_object_underflow))
  {
    pmath_unref(expr);

    if(pmath_is_rational(exponent)) {
      if(pmath_number_sign(exponent) < 0) {
        pmath_unref(exponent);
        if(pmath_equals(base, _pmath_object_overflow)) {
          pmath_unref(base);
          return pmath_ref(_pmath_object_underflow);
        }
        pmath_unref(base);
        return pmath_ref(_pmath_object_overflow);
      }

      pmath_unref(exponent);
      return base;
    }

    pmath_unref(base);
    pmath_unref(exponent);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }

  pmath_unref(base);
  pmath_unref(exponent);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_exp(pmath_expr_t expr) {
  pmath_t x;

  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  x = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_POWER), 2,
           pmath_ref(PMATH_SYMBOL_E),
           x);
}

PMATH_PRIVATE pmath_t builtin_sqrt(pmath_expr_t expr) {
  pmath_t x;

  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }

  x = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_POWER), 2,
           x,
           pmath_ref(_pmath_one_half));
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_power(
  pmath_t *obj,
  double prec,
  pmath_bool_t interval
) {
  pmath_t base, exp;

  if(!pmath_is_expr_of_len(*obj, PMATH_SYMBOL_POWER, 2))
    return FALSE;

  base = pmath_expr_extract_item(*obj, 1);
  exp  = pmath_expr_get_item(*obj, 2);

  if(pmath_is_rational(exp)) {
    pmath_unref(exp);

    if(interval)
      base = pmath_set_precision_interval(base, prec);
    else
      base = pmath_set_precision(base, prec);

    *obj = pmath_expr_set_item(*obj, 1, base);
    return TRUE;
  }

  if(interval) {
    base = pmath_set_precision_interval(base, prec);
    exp  = pmath_set_precision_interval(exp, prec);
  }
  else {
    base = pmath_set_precision(base, prec);
    exp  = pmath_set_precision(exp, prec);
  }

  *obj = pmath_expr_set_item(*obj, 1, base);
  *obj = pmath_expr_set_item(*obj, 2, exp);
  return TRUE;
}
