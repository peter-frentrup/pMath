#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


static pmath_integer_t logii(pmath_integer_t base, pmath_integer_t z) {
  if(pmath_is_int32(base)) {
    base = _pmath_create_mp_int(PMATH_AS_INT32(base));
    
    z = logii(base, z);
    pmath_unref(base);
    return z;
  }
  
  if(pmath_is_int32(z)) {
    z = _pmath_create_mp_int(PMATH_AS_INT32(z));
    
    base = logii(base, z);
    pmath_unref(z);
    return base;
  }
  
  if( !pmath_is_null(base) &&
      !pmath_is_null(z) &&
      mpz_cmp_ui(PMATH_AS_MPZ(base), 1) >= 0)
  {
    pmath_mpint_t tmp = _pmath_create_mp_int(0);
    
    if(!pmath_is_null(tmp)) {
      unsigned long result = mpz_remove(
                               PMATH_AS_MPZ(tmp),
                               PMATH_AS_MPZ(z),
                               PMATH_AS_MPZ(base));
                               
      if(mpz_cmp_ui(PMATH_AS_MPZ(tmp), 1) == 0) {
        pmath_unref(tmp);
        return pmath_integer_new_ulong(result);
      }
      
      pmath_unref(tmp);
    }
  }
  
  return PMATH_NULL;
}

static pmath_integer_t logrr(pmath_rational_t base, pmath_rational_t z) {
  pmath_integer_t bn = pmath_rational_numerator(base);
  pmath_integer_t bd = pmath_rational_denominator(base);
  pmath_integer_t zn = pmath_rational_numerator(z);
  pmath_integer_t zd = pmath_rational_denominator(z);
  pmath_integer_t result;
  pmath_integer_t res2;
  
  if(pmath_equals(bn, PMATH_FROM_INT32(1))) {
    if(pmath_equals(zn, PMATH_FROM_INT32(1))) {
      result = logii(bd, zd);
      
      if(!pmath_is_null(result)) {
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return result;
      }
      
      pmath_unref(result);
    }
    
    if(pmath_equals(zd, PMATH_FROM_INT32(1))) {
      result = logii(bd, zn);
      
      if(!pmath_is_null(result)) {
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return pmath_number_neg(result);
      }
      
      pmath_unref(result);
    }
    
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return PMATH_NULL;
  }
  
  if(pmath_equals(bd, PMATH_FROM_INT32(1))) {
    if(pmath_equals(zn, PMATH_FROM_INT32(1))) {
      result = logii(bn, zd);
      
      if(!pmath_is_null(result)) {
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return pmath_number_neg(result);
      }
      
      pmath_unref(result);
    }
    
    if(pmath_equals(zd, PMATH_FROM_INT32(1))) {
      result = logii(bn, zn);
      
      if(!pmath_is_null(result)) {
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        return result;
      }
      
      pmath_unref(result);
    }
    
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return PMATH_NULL;
  }
  
  result = logii(bn, zn);
  res2   = logii(bd, zd);
  if(pmath_equals(result, res2) && !pmath_is_null(result)) {
    pmath_unref(res2);
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return result;
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  
  result = logii(bd, zn);
  res2   = logii(bn, zd);
  if(pmath_equals(result, res2)) {
    pmath_unref(res2);
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    return pmath_number_neg(result);
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  pmath_unref(bn);
  pmath_unref(bd);
  pmath_unref(zn);
  pmath_unref(zd);
  return PMATH_NULL;
}

static double log2abs(mpfr_t x) {
  mpfr_exp_t accexp;
  double accmant;
  
  if(mpfr_zero_p(x))
    return -HUGE_VAL;
  
  // accuracy = -Log(2, dx)
  accmant = mpfr_get_d_2exp(&accexp, x, MPFR_RNDN);
  return log2(fabs(accmant)) + accexp;
}

// x will be freed, x may be PMATH_NULL
static pmath_t mp_log(pmath_mpfloat_t x) {
  pmath_thread_t thread = pmath_thread_get_current();
  double min_prec, max_prec, prec;
  pmath_mpfloat_t val, err_x;
  
  MPFR_DECL_INIT(right_val, PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(left_val,  PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(diff_val,  PMATH_MP_ERROR_PREC);
  
  if(pmath_is_null(x))
    return PMATH_NULL;
    
  if(!thread) {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  min_prec = thread->min_precision;
  max_prec = thread->max_precision;
  
  if(min_prec < 0)
    min_prec  = 0;
    
  if(min_prec > PMATH_MP_PREC_MAX)
    min_prec  = PMATH_MP_PREC_MAX;
    
  if(max_prec > PMATH_MP_PREC_MAX)
    max_prec  = PMATH_MP_PREC_MAX;
    
  if(max_prec < min_prec)
    max_prec  = min_prec;
    
  assert(pmath_is_mpfloat(x));
  
  if(mpfr_sgn(PMATH_AS_MP_VALUE(x)) < 0) {
    x = pmath_number_neg(x);
    
    val = mp_log(x);
    val = PLUS(val, TIMES(COMPLEX(INT(0), INT(1)), pmath_ref(PMATH_SYMBOL_PI)));
    return val;
  }
  
  if(mpfr_cmp_abs(PMATH_AS_MP_VALUE(x), PMATH_AS_MP_ERROR(x)) <= 0) {
    pmath_unref(x);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  if(min_prec == max_prec) {
    val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(val)) {
      pmath_unref(x);
      return val;
    }
    
    mpfr_log(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    mpfr_set_d(left_val, -min_prec, MPFR_RNDN);
    mpfr_ui_pow(
      right_val,
      2,
      left_val,
      MPFR_RNDU);
      
    mpfr_mul(
      PMATH_AS_MP_ERROR(val), 
      PMATH_AS_MP_VALUE(val), 
      right_val,
      MPFR_RNDA);
    mpfr_abs(
      PMATH_AS_MP_ERROR(val),
      PMATH_AS_MP_ERROR(val),
      MPFR_RNDU);
      
    pmath_unref(x);
    val = _pmath_float_exceptions(val);
    return val;
  }
  
  err_x = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
  if(pmath_is_null(err_x)) {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  mpfr_log(
    right_val,
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDU);
  
  mpfr_sub(
    PMATH_AS_MP_VALUE(err_x),
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDD);
    
  mpfr_log(
    left_val,
    PMATH_AS_MP_VALUE(err_x),
    MPFR_RNDD);
  
  pmath_unref(err_x); err_x = PMATH_NULL;
  
  mpfr_sub(
    diff_val,
    right_val,
    left_val,
    MPFR_RNDA);
  
  // precision === Log(2, |y|) + accuracy
  // accuracy = -Log(2, dy)
  prec = log2abs(right_val) - log2abs(diff_val);
  
  if(prec > max_prec)
    prec  = max_prec;
  else if(!(prec > min_prec))
    prec         = min_prec;
  
  val = _pmath_create_mp_float((mpfr_prec_t)ceil(prec));
  
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return val;
  }
  
  mpfr_log(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDN);
    
  _pmath_mp_float_include_error(val, left_val);
  
  pmath_unref(x);
  
  _pmath_mp_float_clip_error(val, min_prec, max_prec);
  val = _pmath_float_exceptions(val);
  return val;
}

PMATH_PRIVATE pmath_t builtin_log(pmath_expr_t expr) {
  /* Log(base, x)
     Log(x)       = Log(E, x)
   */
  pmath_t base, x;
  int xclass;
  
  if(pmath_expr_length(expr) == 1) {
    x = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_double(x) && PMATH_AS_DOUBLE(x) > 0) {
      pmath_unref(expr);
      return PMATH_FROM_DOUBLE(log(PMATH_AS_DOUBLE(x)));
    }
    
    base = pmath_ref(PMATH_SYMBOL_E);
  }
  else if(pmath_expr_length(expr) == 2) {
    base = pmath_expr_get_item(expr, 1);
    x    = pmath_expr_get_item(expr, 2);
  }
  else {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if( pmath_is_rational(base)     &&
      pmath_is_rational(x)        &&
      pmath_number_sign(base) > 0 &&
      pmath_number_sign(x)    > 0)
  {
    pmath_t result = logrr(base, x);
    
    if(!pmath_is_null(result)) {
      pmath_unref(expr);
      pmath_unref(base);
      pmath_unref(x);
      return result;
    }
  }
  
  if( pmath_equals(base, x) &&
      !pmath_equals(base, PMATH_FROM_INT32(1)))
  {
    pmath_unref(expr);
    pmath_unref(base);
    pmath_unref(x);
    return INT(1);
  }
  
  if(pmath_is_expr_of_len(x, PMATH_SYMBOL_POWER, 2)) {
    pmath_t b = pmath_expr_get_item(x, 1);
    
    if(pmath_equals(base, b)) {
      pmath_unref(b);
      b = pmath_expr_get_item(x, 2);
      
      if(pmath_is_numeric(b)) {
        pmath_unref(expr);
        pmath_unref(base);
        pmath_unref(x);
        return b;
      }
    }
    
    pmath_unref(b);
  }
  
  if(pmath_expr_length(expr) == 2) {
    pmath_unref(expr);
    return DIV(LOG(x), LOG(base));
  }
  pmath_unref(base); base = PMATH_NULL;
  
  if(pmath_is_mpfloat(x)) {
    pmath_unref(expr);
    
    x = mp_log(x);
    return x;
  }
  
  if(_pmath_is_nonreal_complex(x)) {
    pmath_t re = pmath_expr_get_item(x, 1);
    pmath_t im = pmath_expr_get_item(x, 2);
    
    if(pmath_equals(re, PMATH_FROM_INT32(0))) {
      pmath_unref(x);
      pmath_unref(re);
      
      if(pmath_number_sign(im) < 0) {
        re = TIMES(
               pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
                 INT(0),
                 QUOT(-1, 2)),
               pmath_ref(PMATH_SYMBOL_PI));
      }
      else {
        re = TIMES(
               pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
                 INT(0),
                 ONE_HALF),
               pmath_ref(PMATH_SYMBOL_PI));
      }
      
      expr = pmath_expr_set_item(expr, 1, im);
      return PLUS(re, expr);
    }
    
    if( _pmath_is_inexact(re) ||
        _pmath_is_inexact(im))
    {
      pmath_unref(re);
      pmath_unref(im);
      
      expr = pmath_expr_set_item(expr, 1, ABS(pmath_ref(x)));
      
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_COMPLEX), 2,
               expr,
               ARG(x));
    }
  }
  
  if(pmath_is_number(x) || pmath_is_expr_of_len(x, PMATH_SYMBOL_COMPLEX, 2)) {
    xclass = _pmath_number_class(x);
    if(xclass & PMATH_CLASS_INF) {
      pmath_unref(expr);
      pmath_unref(x);
      return pmath_ref(_pmath_object_infinity);
    }
    
    if(xclass & PMATH_CLASS_ZERO) {
      pmath_unref(expr);
      pmath_unref(x);
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_DIRECTEDINFINITY), 1,
               INT(-1));
    }
    
    if(xclass & PMATH_CLASS_POSSMALL) {
      return NEG(pmath_expr_set_item(expr, 1, INV(x)));
    }
    
    if(xclass & PMATH_CLASS_NEG) {
      expr = pmath_expr_set_item(expr, 1, NEG(x));
      return PLUS(
               expr,
               TIMES(
                 pmath_ref(PMATH_SYMBOL_I),
                 pmath_ref(PMATH_SYMBOL_PI)));
    }
    
    if(xclass == PMATH_CLASS_POSONE) {
      pmath_unref(expr);
      pmath_unref(x);
      return INT(0);
    }
  }
  
  pmath_unref(x);
  return expr;
}
