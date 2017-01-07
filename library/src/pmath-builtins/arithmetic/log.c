#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
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
      mp_bitcnt_t result = mpz_remove(
                               PMATH_AS_MPZ(tmp),
                               PMATH_AS_MPZ(z),
                               PMATH_AS_MPZ(base));
                               
      if(mpz_cmp_ui(PMATH_AS_MPZ(tmp), 1) == 0) {
        pmath_unref(tmp);
        return pmath_integer_new_uiptr(result);
      }
      
      pmath_unref(tmp);
    }
  }
  
  return PMATH_NULL;
}

// assuming base, z > 0
// does not free base, z
static pmath_t logrr(pmath_rational_t base, pmath_rational_t z) {
  pmath_integer_t bn = pmath_rational_numerator(base);
  pmath_integer_t bd = pmath_rational_denominator(base);
  pmath_integer_t zn = pmath_rational_numerator(z);
  pmath_integer_t zd = pmath_rational_denominator(z);
  pmath_integer_t result;
  pmath_integer_t res2;
  int sign = 1;
  
  if(pmath_compare(bn, bd) < 1) {
    pmath_t tmp = bn;
    bn = bd;
    bd = tmp;
    
    sign = -sign;
  }
  
  if(pmath_compare(zn, zd) < 1) {
    pmath_t tmp = zn;
    zn = zd;
    zd = tmp;
    
    sign = -sign;
  }
  
  if(pmath_same(bd, INT(1))) {
    if(pmath_same(zd, INT(1))) {
      result = logii(bn, zn);
      
      if(!pmath_is_null(result)) {
        pmath_unref(bn);
        pmath_unref(bd);
        pmath_unref(zn);
        pmath_unref(zd);
        
        if(sign < 0)
          result = pmath_number_neg(result);
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
    
    if(sign < 0)
      result = pmath_number_neg(result);
    return result;
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  
  result = logii(bd, zn);
  res2   = logii(bn, zd);
  if(pmath_equals(result, res2)) {
    sign = -sign;
    pmath_unref(res2);
    pmath_unref(bn);
    pmath_unref(bd);
    pmath_unref(zn);
    pmath_unref(zd);
    
    if(sign < 0)
      result = pmath_number_neg(result);
    return result;
  }
  
  pmath_unref(result);
  pmath_unref(res2);
  pmath_unref(bn);
  pmath_unref(bd);
  pmath_unref(zn);
  pmath_unref(zd);
  return PMATH_NULL;
}

// x will be freed, x may be PMATH_NULL
static pmath_t mp_log(pmath_mpfloat_t x) {
  pmath_mpfloat_t val;
  
  if(pmath_is_null(x))
    return PMATH_NULL;
  
  assert(pmath_is_mpfloat(x));
  
  if(mpfr_zero_p(PMATH_AS_MP_VALUE(x))) {
    pmath_unref(x);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  if(mpfr_sgn(PMATH_AS_MP_VALUE(x)) < 0) {
    x = pmath_number_neg(x);
    
    val = mp_log(x);
    val = PLUS(val, TIMES(COMPLEX(INT(0), INT(1)), pmath_ref(PMATH_SYMBOL_PI)));
    return val;
  }
  
  return _pmath_mpfloat_call(x, mpfr_log);
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
      return pmath_ref(_pmath_object_pos_infinity);
    }
    
    if(xclass & PMATH_CLASS_ZERO) {
      pmath_unref(expr);
      pmath_unref(x);
      return pmath_ref(_pmath_object_neg_infinity);
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
