#include <pmath-core/numbers-private.h>

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


extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_DirectedInfinity;
extern pmath_symbol_t pmath_System_ExponentialE;
extern pmath_symbol_t pmath_System_General;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Sign;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_Undefined;

static void ui_power_of_complex_rational(fmpq_t re, fmpq_t im, ulong exponent) {
  acb_t num_c;
  fmpz_t num;
  fmpz_t den;
  
//  ulong k;
//  fmpq_t binomial;
//  fmpq_t dst[4];
//  fmpq_t reim;
//  fmpq_t im_over_re;

  if(exponent >= LONG_MAX) {
    pmath_message(pmath_System_General, "ovfl", 0);
    return;
  }
  
  if(fmpq_is_zero(im)) {
    fmpq_pow_si(re, re, (slong)exponent);
    return;
  }
  
  if(fmpq_is_zero(re)) {
    switch(exponent & 3) {
      case 0:
        fmpq_pow_si(re, im, (slong)exponent);
        fmpq_set_si(im, 0, 1);
        return;
        
      case 1:
        fmpq_pow_si(im, im, (slong)exponent);
        fmpq_set_si(re, 0, 1);
        return;
        
      case 2:
        fmpq_pow_si(re, im, (slong)exponent);
        fmpq_neg(re, re);
        fmpq_set_si(im, 0, 1);
        return;
        
      case 3:
        fmpq_pow_si(im, im, (slong)exponent);
        fmpq_neg(im, im);
        fmpq_set_si(re, 0, 1);
        return;
    }
    assert(0 && "impossible exponent % 4 (= exponent & 3) value");
    return;
  }
  
  acb_init(num_c);
  fmpz_init(num);
  fmpz_init(den);
  
  fmpz_mul(num, fmpq_numref(re), fmpq_denref(im));
  arb_set_fmpz(acb_realref(num_c), num);
  fmpz_mul(num, fmpq_numref(im), fmpq_denref(re));
  arb_set_fmpz(acb_imagref(num_c), num);
  
  fmpz_mul(den, fmpq_denref(re), fmpq_denref(im));
  acb_pow_ui(num_c, num_c, exponent, ARF_PREC_EXACT);
  
  arf_get_fmpz(num, arb_midref(acb_realref(num_c)), ARF_RND_NEAR); // ARF_PREC_EXACT
  fmpq_set_fmpz_frac(re, num, den);
  
  arf_get_fmpz(num, arb_midref(acb_imagref(num_c)), ARF_RND_NEAR); // ARF_PREC_EXACT
  fmpq_set_fmpz_frac(im, num, den);
  
  acb_clear(num_c);
  fmpz_clear(num);
  fmpz_clear(den);
  
//  // (x+y)^n = Sum(Binomial(n, k) * x^(n-k) * y^k, k->0..n)
//  // Binomial(n, k+1) = Binomial(n,k) * (n-k) / (k+1)
//  fmpq_init(binomial);
//  fmpq_init(dst[0]);
//  fmpq_init(dst[1]);
//  fmpq_init(dst[2]);
//  fmpq_init(dst[3]);
//  fmpq_init(reim);
//  fmpq_init(im_over_re);
//
//  fmpz_set_ui(fmpq_numref(binomial), 1);
//  fmpz_set_ui(fmpq_denref(binomial), 1);
//
//  fmpq_pow_si(reim, re, (slong)exponent); // = x^(n-k) * y^k
//  fmpq_div(im_over_re, im, re);
//
//  for(k = 0; k < exponent; ++k) {
//    fmpq_addmul(dst[k & 3], binomial, reim);
//
//    fmpz_mul_ui(fmpq_numref(binomial), exponent - k);
//    fmpz_divexact_ui(fmpq_numref(binomial), fmpq_numref(binomial), k + 1);
//    fmpq_mul(reim, reim, im_over_re);
//  }
//  fmpq_addmul(dst[exponent & 3], binomial, reim);
//
//  fmpq_sub(re, dst[0], dst[2]);
//  fmpq_sub(im, dst[1], dst[3]);
//
//  fmpq_clear(binomial);
//  fmpq_clear(dst[0]);
//  fmpq_clear(dst[1]);
//  fmpq_clear(dst[2]);
//  fmpq_clear(dst[3]);
//  fmpq_clear(reim);
//  fmpq_clear(im_over_re);
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

/** calculate (base^(1/radix))^exponent assuming gcd(exponent, radix) == 1 and exponent > 0 */
static pmath_t integer_root(const fmpz_t base, const ulong radix, const fmpz_t exponent, pmath_bool_t *any_simplifications) {
  fmpz_factor_t factors;
  fmpz_t tmp;
  fmpz_t root;
  fmpz_t reduced_exp;
  pmath_t result;
  slong i;
  
  *any_simplifications = FALSE;
  
  assert(fmpz_sgn(exponent) > 0);
  assert(radix > 0);
  if(radix == 1) {
    *any_simplifications = TRUE;
    return _pmath_integer_from_fmpz(base);
  }
    
  pmath_gather_begin(PMATH_NULL); // gather remaining factors
  
  fmpz_factor_init(factors);
  fmpz_init(tmp);
  fmpz_init(root);
  fmpz_init(reduced_exp);
  fmpz_factor(factors, base);
  
  if(fmpz_cmp_ui(exponent, radix) > 0) {
    pmath_t factor;
    fmpz_set_ui(tmp, radix);
    fmpz_tdiv_qr(tmp, reduced_exp, exponent, tmp);
    if(fmpz_fits_si(tmp)) {
      fmpz_pow_ui(tmp, base, fmpz_get_ui(tmp));
      factor = _pmath_integer_from_fmpz(tmp);
      *any_simplifications = TRUE;
    }
    else
      factor = POW(_pmath_integer_from_fmpz(base), _pmath_integer_from_fmpz(tmp));
      
    pmath_emit(factor, PMATH_NULL);
  }
  else
    fmpz_set(reduced_exp, exponent);
    
  if(fmpz_sgn(base) < 0) {
    pmath_t factor;
    if(radix == 2) {
      factor = COMPLEX(INT(0), INT(1));
      *any_simplifications = TRUE;
    }
    else {
      factor = POW(INT(-1),
                   pmath_rational_new(
                     _pmath_integer_from_fmpz(reduced_exp),
                     pmath_integer_new_uiptr(radix)));
    }
    pmath_emit(factor, PMATH_NULL);
  }
  
  fmpz_set_ui(root, 1);
  for(i = 0; i < factors->num; ++i) {
    ulong q = factors->exp[i] / radix;
    ulong r = factors->exp[i] % radix;
    
    if(q > 0) {
      fmpz_pow_ui(tmp, &factors->p[i], q);
      fmpz_mul(root, root, tmp);
    }
    if(r > 0) {
      pmath_t factor;
      pmath_t new_exp;
      
      fmpz_mul_ui(tmp, reduced_exp, r);
      new_exp = pmath_rational_new(_pmath_integer_from_fmpz(tmp), pmath_integer_new_uiptr(radix));
      if(!*any_simplifications) {
        if(r > 1) {
          *any_simplifications = TRUE;
        }
        else {
          pmath_t new_exp_den = pmath_rational_denominator(new_exp);
          if(pmath_integer_fits_ulong(new_exp_den) && pmath_integer_get_ulong(new_exp_den) != radix)
            *any_simplifications = TRUE;
          
          pmath_unref(new_exp_den);
        }
      }
      
      factor = POW(_pmath_integer_from_fmpz(&factors->p[i]), new_exp);
      pmath_emit(factor, PMATH_NULL);
    }
  }
  
  if(!fmpz_is_one(root)) {
    pmath_t factor;
    if(fmpz_fits_si(reduced_exp)) {
      fmpz_pow_ui(root, root, fmpz_get_ui(reduced_exp));
      factor = _pmath_integer_from_fmpz(root);
      *any_simplifications = TRUE;
    }
    else
      factor = POW(_pmath_integer_from_fmpz(root), _pmath_integer_from_fmpz(reduced_exp));
    pmath_emit(factor, PMATH_NULL);
  }
  
  result = pmath_gather_end();
  if(pmath_expr_length(result) == 0) {
    pmath_unref(result);
    result = INT(1);
  }
  else if(pmath_expr_length(result) == 1) {
    pmath_t factor = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    result = factor;
  }
  else
    result = pmath_expr_set_item(result, 0, pmath_ref(pmath_System_Times));
    
  fmpz_clear(reduced_exp);
  fmpz_clear(root);
  fmpz_clear(tmp);
  fmpz_factor_clear(factors);
  
  return result;
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
    else if(pmath_is_expr_of_len(factor, pmath_System_Complex, 2)) {
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
      else if(pmath_is_expr_of_len(factor, pmath_System_Complex, 2)) {
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
    numeric_factors = pmath_expr_set_item(numeric_factors, 0, pmath_ref(pmath_System_Times));
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
      
      if(!pmath_is_expr_of(factor, pmath_System_Power)) {
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

/** \brief Test whether a real or complex number object is zero (respectively contains zero for the case of real/complex balls)
    \param x The object to test. It won't be freed. If it is not a number or Complex(~,~) expression, FALSE will be returned.
    \return Whether \a x represents the number zero or a real/complex ball containing zero.
 */
static pmath_bool_t number_contains_zero(pmath_t x) {
  if(pmath_is_int32(x))
    return PMATH_AS_INT32(x) == 0;
  if(pmath_is_rational(x))
    return FALSE;
  if(pmath_is_double(x))
    return PMATH_AS_DOUBLE(x) == 0;
  if(pmath_is_mpfloat(x))
    return arb_contains_zero(PMATH_AS_ARB(x));
  if(pmath_is_expr_of_len(x, pmath_System_Complex, 2)) {
    pmath_t re = pmath_expr_get_item(x, 1);
    pmath_t im = pmath_expr_get_item(x, 2);
    pmath_bool_t zero = number_contains_zero(re) && number_contains_zero(im);
    pmath_unref(re);
    pmath_unref(im);
    return zero;
  }
  return FALSE;
}

/** \brief convert an integer object to a writable bigint object.
    \param i The integer object (int32 or bigint). It will be freed.
    \return A bigint with the value of \a i and a reference count of 1, i.e. it is directly writable.
            On out-of-memory, PMATH_NULL is returned.
 */
static pmath_mpint_t to_writable_bigint(pmath_integer_t i) {
  pmath_mpint_t result;
  if(pmath_is_int32(i))
    return _pmath_create_mp_int(PMATH_AS_INT32(i));
  if(pmath_is_null(i))
    return PMATH_NULL;
    
  assert(pmath_is_mpint(i));
  if(pmath_refcount(i) == 1)
    return i;
    
  result = _pmath_create_mp_int(0);
  if(!pmath_is_null(result))
    mpz_set(PMATH_AS_MPZ(result), PMATH_AS_MPZ(i));
    
  pmath_unref(i);
  return result;
}

static pmath_expr_t spread_over_factors(pmath_expr_t product, pmath_t exponent) {
  size_t i = pmath_expr_length(product);
  for(; i > 0; --i) {
    pmath_t factor = pmath_expr_extract_item(product, i);
    factor = POW(factor, pmath_ref(exponent));
    product = pmath_expr_set_item(product, i, factor);
  }
  pmath_unref(exponent);
  return product;
}

/** \brief Try to evaluate an expression of the form Power(~, exponent) with small integer exponent.
    \param expr     Pointer to the Power-expression. On success, this will be replaced by the evaluation result.
    \param exponent An int32 exponent. It may be zero.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_integer_power(pmath_t *expr, int32_t exponent) {
  pmath_t base = pmath_expr_get_item(*expr, 1);
  
  if(number_contains_zero(base)) {
    if(exponent < 0) { // TODO: return interval?
      pmath_unref(base);
      pmath_message(PMATH_NULL, "infy", 1, *expr);
      *expr = pmath_ref(_pmath_object_complex_infinity);
      return TRUE;
    }
  }
  
  if(exponent == 1) {
    pmath_unref(*expr);
    *expr = base;
    return TRUE;
  }
  
  if(pmath_is_double(base)) {
    double result = pow(PMATH_AS_DOUBLE(base), exponent);
    if(isfinite(result)) {
      pmath_unref(base);
      pmath_unref(*expr);
      *expr = PMATH_FROM_DOUBLE(result);
      return TRUE;
    }
    base = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(base));
  }
  
  if(pmath_is_mpfloat(base)) {
    pmath_mpfloat_t result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(base));
    if(pmath_is_null(result)) {
      pmath_unref(base);
      return FALSE;
    }
    
    if(exponent > 0) {
      arb_pow_ui(PMATH_AS_ARB(result), PMATH_AS_ARB(base), (ulong)exponent, PMATH_AS_ARB_WORKING_PREC(result));
    }
    else {
      arb_pow_ui(PMATH_AS_ARB(result), PMATH_AS_ARB(base), -(ulong)exponent, PMATH_AS_ARB_WORKING_PREC(result));
      arb_ui_div(PMATH_AS_ARB(result), 1, PMATH_AS_ARB(result), PMATH_AS_ARB_WORKING_PREC(result));
    }
    
    pmath_unref(base);
    pmath_unref(*expr);
    *expr = result;
    return TRUE;
  }
  
  if(pmath_is_rational(base)) {
    uint32_t abs_exponent = (exponent < 0) ? -(uint32_t)exponent : (uint32_t)exponent;
    pmath_integer_t base_num = pmath_rational_numerator(base);
    pmath_integer_t base_den = pmath_rational_denominator(base);
    pmath_mpint_t result_num;
    pmath_mpint_t result_den;
    pmath_unref(base);
    pmath_unref(*expr);
    
    result_num = to_writable_bigint(base_num);
    result_den = to_writable_bigint(base_den);
    if(pmath_is_null(result_num) || pmath_is_null(result_den)) {
      pmath_unref(result_num);
      pmath_unref(result_den);
      *expr = PMATH_NULL;
      return TRUE;
    }
    
    mpz_pow_ui(PMATH_AS_MPZ(result_num), PMATH_AS_MPZ(result_num), abs_exponent);
    mpz_pow_ui(PMATH_AS_MPZ(result_den), PMATH_AS_MPZ(result_den), abs_exponent);
    
    result_num = _pmath_mp_int_normalize(result_num);
    result_den = _pmath_mp_int_normalize(result_den);
    if(exponent < 0)
      *expr = pmath_rational_new(result_den, result_num);
    else
      *expr = pmath_rational_new(result_num, result_den);
    return TRUE;
  }
  
  if(pmath_is_expr_of_len(base, pmath_System_Complex, 2)) {
    pmath_t re = pmath_expr_get_item(base, 1);
    pmath_t im = pmath_expr_get_item(base, 2);
    if(pmath_is_number(re) && pmath_is_number(im)) {
      if(number_contains_zero(re) && number_contains_zero(im)) {
        if(exponent < 0) { // TODO: return interval?
          pmath_unref(re);
          pmath_unref(im);
          pmath_unref(base);
          pmath_message(PMATH_NULL, "infy", 1, *expr);
          *expr = pmath_ref(_pmath_object_complex_infinity);
          return TRUE;
        }
      }
      
      if(pmath_is_float(re) || pmath_is_float(im)) {
        acb_t z;
        slong precision;
        pmath_bool_t is_machine_precision;
        acb_init(z);
        
        if(_pmath_complex_float_extract_acb(z, &precision, &is_machine_precision, base)) {
          acb_pow_si(z, z, exponent, precision);
          pmath_unref(base);
          pmath_unref(re);
          pmath_unref(im);
          pmath_unref(*expr);
          *expr = _pmath_complex_new_from_acb_destructive(z, is_machine_precision ? -1 : precision);
          return TRUE;
        }
        
        acb_clear(z);
      }
      
      if(pmath_is_rational(re) && pmath_is_rational(im)) {
        uint32_t abs_exponent = (exponent < 0) ? -(uint32_t)exponent : (uint32_t)exponent;
        fmpq_t re_q;
        fmpq_t im_q;
        
        fmpq_init(re_q);
        fmpq_init(im_q);
        
        _pmath_rational_get_fmpq(re_q, re);
        _pmath_rational_get_fmpq(im_q, im);
        ui_power_of_complex_rational(re_q, im_q, abs_exponent);
        if(exponent < 0) { // 1/(x+iy) = (x-iy) / (x^2+y^2)
          fmpq_t norm;
          fmpq_init(norm);
          
          fmpq_mul(norm, re_q, re_q);
          fmpq_addmul(norm, im_q, im_q);
          fmpq_div(re_q, re_q, norm);
          fmpq_div(im_q, im_q, norm);
          fmpq_neg(im_q, im_q);
          
          fmpq_clear(norm);
        }
        
        pmath_unref(base);
        pmath_unref(*expr);
        pmath_unref(re);
        pmath_unref(im);
        if(fmpq_is_zero(im_q)) {
          *expr = _pmath_rational_from_fmpq(re_q);
        }
        else {
          re = _pmath_rational_from_fmpq(re_q);
          im = _pmath_rational_from_fmpq(im_q);
          *expr = COMPLEX(re, im);
        }
        fmpq_clear(re_q);
        fmpq_clear(im_q);
        return TRUE;
      }
    }
    pmath_unref(re);
    pmath_unref(im);
  }
  
  if(pmath_is_expr_of(base, pmath_System_Times)) {
    pmath_unref(*expr);
    *expr = spread_over_factors(base, PMATH_FROM_INT32(exponent));
    return TRUE;
  }
  
  pmath_unref(base);
  return FALSE;
}

// assuming, that exponent does not fit to int32
/** \brief Try to evaluate an expression of the form Power(~, exponent) with large (in absulute value) integer exponent.
    \param expr     Pointer to the Power-expression. On success, this will be replaced by the evaluation result.
    \param exponent An big integer exponent. It is assumed, that this does not fit an int32_t.
                    In particular it is not zero.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_bigint_power(pmath_t *expr, mpz_srcptr exponent) {
  pmath_t base = pmath_expr_get_item(*expr, 1);
  
  if(number_contains_zero(base)) {
    if(mpz_sgn(exponent) <= 0) { // 0 cannot happen for pmath_mpint_t. TODO: return interval?
      pmath_unref(base);
      pmath_message(PMATH_NULL, "infy", 1, *expr);
      *expr = pmath_ref(_pmath_object_complex_infinity);
      return TRUE;
    }
  }
  
  if(pmath_is_int32(base)) {
    switch(PMATH_AS_INT32(base)) {
      case -1:
        pmath_unref(*expr);
        *expr = PMATH_FROM_INT32(mpz_odd_p(exponent) ? -1 : 1);
        return TRUE;
      case 0:
        pmath_unref(*expr);
        if(mpz_sgn(exponent) < 0) {
          pmath_message(PMATH_NULL, "infy", 1, *expr);
          *expr = pmath_ref(_pmath_object_complex_infinity);
        }
        else
          *expr = base;
        return TRUE;
      case 1:
        pmath_unref(*expr);
        *expr = base;
        return TRUE;
    }
  }
  else if(pmath_is_double(base)) {
    double exp = mpz_get_d(exponent);
    double result = pow(PMATH_AS_DOUBLE(base), exp);
    
    if(isfinite(result)) {
      pmath_unref(*expr);
      *expr = PMATH_FROM_DOUBLE(result);
      return TRUE;
    }
    
    base = _pmath_create_mp_float_from_d(PMATH_AS_DOUBLE(base));
  }
  
  if(pmath_is_mpfloat(base)) {
    pmath_mpfloat_t result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(base));
    pmath_mpfloat_t exp = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(base));
    if(pmath_is_null(result) || pmath_is_null(exp)) {
      pmath_unref(base);
      pmath_unref(result);
      pmath_unref(exp);
      return FALSE;
    }
    
    arf_set_mpz(arb_midref(PMATH_AS_ARB(exp)), exponent);
    mag_set_ui(arb_radref(PMATH_AS_ARB(exp)), 0);
    
    arb_pow(PMATH_AS_ARB(result), PMATH_AS_ARB(base), PMATH_AS_ARB(exp), PMATH_AS_ARB_WORKING_PREC(result));
    
    pmath_unref(base);
    pmath_unref(*expr);
    *expr = result;
    return TRUE;
  }
  
  if(pmath_is_expr_of_len(base, pmath_System_Complex, 2)) {
    acb_t z;
    slong precision;
    pmath_bool_t is_machine_precision;
    
    acb_init(z);
    if(_pmath_complex_float_extract_acb(z, &precision, &is_machine_precision, base)) {
      acb_t exp;
      
      acb_init(exp); // init to extact 0+0*ImaginaryI
      arf_set_mpz(arb_midref(acb_realref(exp)), exponent);
      
      acb_pow(z, z, exp, precision);
      
      pmath_unref(base);
      pmath_unref(*expr);
      acb_clear(exp);
      *expr = _pmath_complex_new_from_acb_destructive(z, is_machine_precision ? -1 : precision);
      return TRUE;
    }
    
    // TODO: any exact complex root of 1 should be no problem, e.g. I^n
    
    acb_clear(z);
  }
  
  // TODO: overflow message for numbers
  
  pmath_unref(base);
  return FALSE;
}

/** \brief Test if an expression is of the form Power(~, exponent) with a given rational exponent.
    \param expr     The expression to test. It won't be freed.
    \param exponent A rational number. May also be integer.
 */
static pmath_bool_t is_rational_power(pmath_t expr, const fmpq_t exponent) {
  pmath_t exp;
  fmpq_t exp_q;
  
  if(!pmath_is_expr_of_len(expr, pmath_System_Power, 2))
    return FALSE;
    
  exp = pmath_expr_get_item(expr, 2);
  if(!pmath_is_rational(exp)) {
    pmath_unref(exp);
    return FALSE;
  }
  
  fmpq_init(exp_q);
  _pmath_rational_get_fmpq(exp_q, exp);
  pmath_unref(exp);
  if(!fmpq_equal(exp_q, exponent)) {
    fmpq_clear(exp_q);
    return FALSE;
  }
  fmpq_clear(exp_q);
  return TRUE;
}

/** \brief Try to evaluate an expression of the form Power(~, exponent) with non-integer rational exponent.
    \param expr     Pointer to the Power-expression. On success, this will be replaced by the evaluation result.
    \param exponent A rational exponent. This function assumes, that the exponent is non-integer.
                    In particular it is not zero.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_rational_power(pmath_t *expr, const fmpq_t exponent) {
  pmath_t base = pmath_expr_get_item(*expr, 1);
  acb_t z;
  slong precision;
  pmath_bool_t is_machine_precision;
  
  if(number_contains_zero(base)) {
    if(fmpq_sgn(exponent) <= 0) { // 0 cannot happen for non-integer exponent. TODO: return interval?
      pmath_unref(base);
      pmath_message(PMATH_NULL, "infy", 1, *expr);
      *expr = pmath_ref(_pmath_object_complex_infinity);
      return TRUE;
    }
  }
  
  if( pmath_is_rational(base) &&
      fmpz_fits_si(fmpq_denref(exponent)) &&
      fmpz_sgn(fmpq_denref(exponent)) > 0)
  {
    ulong radix = fmpz_get_ui(fmpq_denref(exponent));
    pmath_t root_num;
    pmath_t root_den;
    pmath_bool_t simplified_num = FALSE;
    pmath_bool_t simplified_den = FALSE;
    fmpq_t base_quot;
    fmpz_t exp;
    
    fmpz_init(exp);
    fmpz_abs(exp, fmpq_numref(exponent));
    fmpq_init(base_quot);
    _pmath_rational_get_fmpq(base_quot, base);
    if(fmpq_sgn(exponent) > 0) {
      root_num = integer_root(fmpq_numref(base_quot), radix, exp, &simplified_num);
      root_den = integer_root(fmpq_denref(base_quot), radix, exp, &simplified_den);
    }
    else {
      root_den = integer_root(fmpq_numref(base_quot), radix, exp, &simplified_num);
      root_num = integer_root(fmpq_denref(base_quot), radix, exp, &simplified_den);
    }
    fmpq_clear(base_quot);
    fmpz_clear(exp);
    
    if(simplified_num || simplified_den) {
      if(pmath_same(root_den, PMATH_FROM_INT32(1))) {
        if(!is_rational_power(root_num, exponent)) {
          pmath_unref(base);
          pmath_unref(*expr);
          *expr = root_num;
          return TRUE;
        }
      }
      else if(pmath_is_integer(root_num) && pmath_is_integer(root_den)) {
        pmath_unref(base);
        pmath_unref(*expr);
        *expr = pmath_rational_new(root_num, root_den);
        return TRUE;
      }
      else if( !is_rational_power(root_num, exponent) || !is_rational_power(root_den, exponent)) {
        pmath_unref(base);
        if(pmath_same(root_num, PMATH_FROM_INT32(1))) {
          if(pmath_is_expr_of(root_den, pmath_System_Power)) {
            pmath_unref(root_num);
            pmath_unref(root_den);
            return FALSE;
          }
          pmath_unref(*expr);
          *expr = INV(root_den);
          return TRUE;
        }
        pmath_unref(*expr);
        *expr = DIV(root_num, root_den);
        return TRUE;
      }
    }
    
    pmath_unref(root_num);
    pmath_unref(root_den);
//    fmpq_t quot;
//    fmpq_t root;
//    fmpq_t remainder;
//    fmpz_t exp_int;
//    fmpz_t exp_rem;
//
//    slong exp_rem_si;
//    pmath_t int_factor, root_factor, rest_factor;
//
//    fmpq_init(quot);
//    fmpq_init(root);
//    fmpq_init(remainder);
//    fmpz_init(exp_int);
//    fmpz_init(exp_rem);
//    fmpz_tdiv_qr(exp_int, exp_rem, fmpq_numref(exponent), fmpq_denref(exponent));
//    exp_rem_si = fmpz_get_si(exp_rem);
//
//    // TODO: simplify 4^(1/4) = 2^(1/2)
//
//    _pmath_rational_get_fmpq(quot, base);
//    simplify_integer_root(fmpq_numref(root), fmpq_numref(remainder), fmpq_numref(quot), radix);
//    simplify_integer_root(fmpq_denref(root), fmpq_denref(remainder), fmpq_denref(quot), radix);
//    fmpq_canonicalise(root);
//    fmpq_canonicalise(remainder);
//
//    if(fmpz_fits_si(exp_int)) {
//      fmpq_pow_si(quot, quot, fmpz_get_si(exp_int));
//      fmpz_set_ui(exp_int, 1);
//    }
//
//    int_factor = _pmath_rational_from_fmpq(quot);
//    if(!fmpz_is_one(exp_int))
//      int_factor = POW(int_factor, _pmath_integer_from_fmpz(exp_int));
//
//    fmpq_pow_si(root, root, exp_rem_si);
//    if(fmpz_is_one(exp_int)) {
//      pmath_unref(int_factor);
//      int_factor = PMATH_FROM_INT32(1);
//      fmpq_mul(root, root, quot);
//    }
//    root_factor = _pmath_rational_from_fmpq(root);
//
//    rest_factor = _pmath_rational_from_fmpq(remainder);
//    if(!pmath_same(rest_factor, PMATH_FROM_INT32(1))) {
//      if(fmpq_sgn(remainder) < 0) {
//        root_factor = COMPLEX(INT(0), root_factor);
//        rest_factor = pmath_number_neg(rest_factor);
//      }
//      if(!pmath_same(rest_factor, PMATH_FROM_INT32(1))) {
//        rest_factor = POW(
//                        rest_factor,
//                        pmath_rational_new(
//                          pmath_integer_new_siptr(exp_rem_si),
//                          pmath_integer_new_uiptr(radix)));
//      }
//    }
//
//    fmpz_clear(exp_rem);
//    fmpz_clear(exp_int);
//    fmpq_clear(remainder);
//    fmpq_clear(root);
//    fmpq_clear(quot);
//
//    if(!pmath_same(int_factor, PMATH_FROM_INT32(1)) || !pmath_same(root_factor, PMATH_FROM_INT32(1))) {
//      pmath_unref(base);
//      pmath_unref(*expr);
//      if(pmath_same(rest_factor, PMATH_FROM_INT32(1))) {
//        if(pmath_same(int_factor, PMATH_FROM_INT32(1)))
//          *expr = root_factor;
//        else if(pmath_same(root_factor, PMATH_FROM_INT32(1)))
//          *expr = int_factor;
//        else
//          *expr = TIMES(int_factor, root_factor);
//      }
//      else {
//        if(pmath_same(int_factor, PMATH_FROM_INT32(1)))
//          *expr = TIMES(root_factor, rest_factor);
//        else if(pmath_same(root_factor, PMATH_FROM_INT32(1)))
//          *expr = TIMES(int_factor, rest_factor);
//        else
//          *expr = TIMES3(int_factor, root_factor, rest_factor);
//      }
//      return TRUE;
//    }
//
//    pmath_unref(int_factor);
//    pmath_unref(root_factor);
//    pmath_unref(rest_factor);
  }
  
  if(pmath_is_double(base)) {
    double exp = fmpz_get_d(fmpq_numref(exponent)) / fmpz_get_d(fmpq_denref(exponent));
    double d = pow(PMATH_AS_DOUBLE(base), exp);
    if(isfinite(d)) {
      pmath_unref(base);
      pmath_unref(*expr);
      *expr = PMATH_FROM_DOUBLE(d);
      return TRUE;
    }
  }
  
  acb_init(z);
  if(_pmath_complex_float_extract_acb(z, &precision, &is_machine_precision, base)) {
    arb_t exp;
    arb_init(exp);
    
    arb_set_fmpq(exp, exponent, precision);
    acb_pow_arb(z, z, exp, precision);
    arb_clear(exp);
    
    pmath_unref(base);
    pmath_unref(*expr);
    *expr = _pmath_complex_new_from_acb_destructive(z, is_machine_precision ? -1 : precision);
    return TRUE;
  }
  acb_clear(z);
  
  // TODO: exact complex base
  
  pmath_unref(base);
  return FALSE;
}

/** \brief Compute a complex floating point power.
    \param expr                 The corresponding Power(...) expression. It will be freed.
    \param base                 The base complex number. It will be cleared.
    \param base                 The exponent complex number. It will be cleared.
    \param precision            The working precision of the operation in bits.
    \param is_machine_precision Whether to return doubles or an arbitrary precision result.
    \return The result of the operation. A real or complex number, infinity or Undefined.
 */
static pmath_t complex_power(pmath_t expr, acb_t base, acb_t exponent, slong precision, pmath_bool_t is_machine_precision) {
  acb_pow(base, base, exponent, precision);
  
  if(acb_is_finite(base)) {
    pmath_unref(expr);
    acb_clear(exponent);
    return _pmath_complex_new_from_acb_destructive(base, is_machine_precision ? -1 : precision);
  }
  
  if(arf_is_nan(arb_midref(acb_realref(base))) || arf_is_nan(arb_midref(acb_imagref(base)))) {
    pmath_message(PMATH_NULL, "indet", 1, expr);
    expr = pmath_ref(pmath_System_Undefined);
  }
  else {
    pmath_message(PMATH_NULL, "infy", 1, expr);
    expr = pmath_ref(_pmath_object_complex_infinity);
  }
  
  acb_clear(base);
  acb_clear(exponent);
  return expr;
}

/** \brief Try to evaluate an expression of the form Power(~, exponent) with floating point real or complex exponent.
    \param expr     Pointer to the Power-expression. On success, this will be replaced by the evaluation result.
    \param exponent A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_inexact_power(pmath_t *expr, pmath_t exponent) {
  acb_t        exp_z;
  slong        exp_prec;
  pmath_bool_t exp_is_machine_prec;
  acb_init(exp_z);
  if(_pmath_complex_float_extract_acb(exp_z, &exp_prec, &exp_is_machine_prec, exponent)) {
    pmath_t base = pmath_expr_get_item(*expr, 1);
    acb_t        base_z;
    slong        base_prec;
    pmath_bool_t base_is_machine_prec;
    
    if(pmath_same(base, pmath_System_ExponentialE)) {
      acb_exp(exp_z, exp_z, exp_prec);
      pmath_unref(base);
      pmath_unref(*expr);
      *expr = _pmath_complex_new_from_acb_destructive(exp_z, exp_is_machine_prec ? -1 : exp_prec);
      return TRUE;
    }
    
    acb_init(base_z);
    if(_pmath_complex_float_extract_acb(base_z, &base_prec, &base_is_machine_prec, base)) {
      pmath_unref(base);
      *expr = complex_power(*expr, base_z, exp_z, FLINT_MAX(base_prec, exp_prec), base_is_machine_prec || exp_is_machine_prec);
      return TRUE;
    }
    
    base = pmath_set_precision(base, exp_is_machine_prec ? -HUGE_VAL : (double)exp_prec);
    if(_pmath_complex_float_extract_acb(base_z, &base_prec, &base_is_machine_prec, base)) {
      pmath_unref(base);
      *expr = complex_power(*expr, base_z, exp_z, FLINT_MAX(base_prec, exp_prec), base_is_machine_prec || exp_is_machine_prec);
      return TRUE;
    }
    
    acb_clear(base_z);
    pmath_unref(base);
  }
  acb_clear(exp_z);
  return FALSE;
}

/** \brief Try to evaluate an expression of the form Power(base, ~) with floating point real or complex base.
    
    This function assumes that the exponent is not an explicit, i.e. \see try_inexact_power(), \see try_rational_power()
    and similar failed.
    
    \param expr Pointer to the Power-expression. On success, this will be replaced by the evaluation result.
    \param base A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_power_of_inexact(pmath_t *expr, pmath_t base) {
  acb_t        base_z;
  slong        base_prec;
  pmath_bool_t base_is_machine_prec;
  acb_init(base_z);
  if(_pmath_complex_float_extract_acb(base_z, &base_prec, &base_is_machine_prec, base)) {
    pmath_t exponent = pmath_expr_get_item(*expr, 2);
    acb_t        exp_z;
    slong        exp_prec;
    pmath_bool_t exp_is_machine_prec;
    
    exponent = pmath_set_precision(exponent, base_is_machine_prec ? -HUGE_VAL : (double)base_prec);
    
    acb_init(exp_z);
    if(_pmath_complex_float_extract_acb(exp_z, &exp_prec, &exp_is_machine_prec, exponent)) {
      pmath_unref(exponent);
      *expr = complex_power(*expr, base_z, exp_z, FLINT_MAX(base_prec, exp_prec), base_is_machine_prec || exp_is_machine_prec);
      return TRUE;
    }
    acb_clear(exp_z);
    pmath_unref(exponent);
  }
  acb_clear(base_z);
  return FALSE;
}

/** \brief Try to evaluate an expression of the form Power(base, ~) with non-integer rational base.
    \param expr     Pointer to the Power-expression. On success, this will be replaced by the evaluation result.
    \param base     A non-integer rational number. It won't be freed.
    \param exponent A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_power_of_rational(pmath_t *expr, pmath_quotient_t base, pmath_t exponent) {
  pmath_integer_t num;
  
  assert(pmath_is_quotient(base));
  num = pmath_rational_numerator(base);
  if(pmath_same(num, PMATH_FROM_INT32(1))) {
    pmath_unref(num);
    *expr = pmath_expr_set_item(*expr, 1, pmath_rational_denominator(base));
    *expr = pmath_expr_set_item(*expr, 2, NEG(pmath_ref(exponent)));
    return TRUE;
  }
  
  if(pmath_is_expr_of(exponent, pmath_System_Times)) {
    pmath_t factor = pmath_expr_get_item(exponent, 1);
    if(pmath_is_number(factor) && pmath_number_sign(factor) < 0) {
      pmath_integer_t den = pmath_rational_denominator(base);
      factor = pmath_number_neg(factor);
      *expr = pmath_expr_set_item(*expr, 1, pmath_rational_new(den, num));
      *expr = pmath_expr_set_item(*expr, 2, pmath_expr_set_item(pmath_ref(exponent), 1, factor));
      return TRUE;
    }
    pmath_unref(factor);
  }
  
  pmath_unref(num);
  return FALSE;
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
  
  exponent = pmath_expr_get_item(expr, 2);
  if(pmath_is_int32(exponent) && try_integer_power(&expr, PMATH_AS_INT32(exponent)))
    return expr;
    
  if(pmath_is_mpint(exponent) && try_bigint_power(&expr, PMATH_AS_MPZ(exponent))) {
    pmath_unref(exponent);
    return expr;
  }
  
  if(pmath_is_quotient(exponent)) {
    fmpq_t quot;
    fmpq_init(quot);
    _pmath_rational_get_fmpq(quot, exponent);
    if(try_rational_power(&expr, quot)) {
      fmpq_clear(quot);
      pmath_unref(exponent);
      return expr;
    }
    fmpq_clear(quot);
  }
  
  if(try_inexact_power(&expr, exponent)) {
    pmath_unref(exponent);
    return expr;
  }
  
  base = pmath_expr_get_item(expr, 1);
  
  if(try_power_of_inexact(&expr, base)) {
    pmath_unref(base);
    pmath_unref(exponent);
    return expr;
  }
  
  if(pmath_is_quotient(base) && try_power_of_rational(&expr, base, exponent)) {
    pmath_unref(base);
    pmath_unref(exponent);
    return expr;
  }
  
  if(pmath_is_expr_of_len(base, pmath_System_Power, 2)) { // (x^y)^exponent
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
    return pmath_ref(pmath_System_Undefined);
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
    return pmath_ref(pmath_System_Undefined);
  }
  
  if(exp_class & PMATH_CLASS_ZERO) {
    if(base_class & PMATH_CLASS_INF) {
      pmath_unref(base);
      pmath_unref(exponent);
      pmath_message(PMATH_NULL, "indet", 1, expr);
      return pmath_ref(pmath_System_Undefined);
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
                 pmath_ref(pmath_System_Sign), 1,
                 base));
      return pmath_expr_new_extended(
               pmath_ref(pmath_System_DirectedInfinity), 1, expr);
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
    pmath_message(PMATH_NULL, "indet", 1, expr);
    return pmath_ref(pmath_System_Undefined);
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
    return pmath_ref(pmath_System_Undefined);
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
    return pmath_ref(pmath_System_Undefined);
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
    return pmath_ref(pmath_System_Undefined);
  }
  
  if(exp_class & PMATH_CLASS_POSONE) {
    pmath_unref(exponent);
    pmath_unref(expr);
    return base;
  }
  
  if(exp_class & (PMATH_CLASS_REAL | PMATH_CLASS_COMPLEX)) {
    if(pmath_is_expr_of(base, pmath_System_Times)) {
      pmath_unref(base);
      pmath_unref(exponent);
      return expand_numeric_power_of_product(expr);
    }
    
    if(pmath_is_expr_of_len(base, pmath_System_Complex, 2)) {
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
  
  if(pmath_is_expr_of(base, pmath_System_Times)) { // (x y)^z = x^z y^z, if x > 0
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
    return pmath_ref(pmath_System_Undefined);
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
           pmath_ref(pmath_System_Power), 2,
           pmath_ref(pmath_System_ExponentialE),
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
           pmath_ref(pmath_System_Power), 2,
           x,
           pmath_ref(_pmath_one_half));
}

PMATH_PRIVATE pmath_bool_t builtin_approximate_power(pmath_t *obj, double prec) {
  pmath_t base, exp;
  
  if(!pmath_is_expr_of_len(*obj, pmath_System_Power, 2))
    return FALSE;
    
  base = pmath_expr_extract_item(*obj, 1);
  exp  = pmath_expr_extract_item(*obj, 2);
  
  if(pmath_same(base, pmath_System_ExponentialE)) {
    exp = pmath_set_precision(exp, prec);
  }
  else if(pmath_is_rational(exp)) {
    base = pmath_set_precision(base, prec);
  }
  else {
    base = pmath_set_precision(base, prec);
    exp = pmath_set_precision(exp, prec);
  }
  *obj = pmath_expr_set_item(*obj, 1, base);
  *obj = pmath_expr_set_item(*obj, 2, exp);
  return TRUE;
}
