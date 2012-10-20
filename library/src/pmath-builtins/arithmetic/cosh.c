#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


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
static pmath_mpfloat_t mp_cosh(pmath_mpfloat_t x) {
  pmath_thread_t thread = pmath_thread_get_current();
  double min_prec, max_prec, prec1, prec2;
  pmath_mpfloat_t val;
  
  MPFR_DECL_INIT(err_x,     PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(left_val,  PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(right_val, PMATH_MP_ERROR_PREC);
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
  
  if(min_prec == max_prec) {
    val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(val)) {
      pmath_unref(x);
      return val;
    }
    
    mpfr_cosh(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    mpfr_set_d(left_val, -min_prec, MPFR_RNDN);
    mpfr_ui_pow(
      err_x,
      2,
      left_val,
      MPFR_RNDU);
      
    mpfr_mul(
      PMATH_AS_MP_ERROR(val), 
      PMATH_AS_MP_VALUE(val), 
      err_x,
      MPFR_RNDA);
//    mpfr_abs(
//      PMATH_AS_MP_ERROR(val),
//      PMATH_AS_MP_ERROR(val),
//      MPFR_RNDU);
      
    pmath_unref(x);
    return val;
  }
  
  if(mpfr_sgn(PMATH_AS_MP_VALUE(x)) >= 0){
    mpfr_add(
      err_x,
      PMATH_AS_MP_VALUE(x),
      PMATH_AS_MP_ERROR(x),
      MPFR_RNDN);
      
    mpfr_cosh(
      right_val,
      err_x,
      MPFR_RNDU);
      
    mpfr_cosh(
      left_val,
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDD);
  }
  else {
    mpfr_sub(
      err_x,
      PMATH_AS_MP_VALUE(x),
      PMATH_AS_MP_ERROR(x),
      MPFR_RNDN);
      
    mpfr_cosh(
      left_val,
      err_x,
      MPFR_RNDU);
      
    mpfr_cosh(
      right_val,
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDD);
  }
    
  mpfr_sub(
    diff_val,
    right_val,
    left_val,
    MPFR_RNDA);
  
  // precision === Log(2, |y|) + accuracy
  // accuracy = -Log(2, dy)
  prec1 = log2abs(left_val);
  prec2 = log2abs(right_val);
  
  if(prec1 < prec2)
    prec1 = prec2;
  
  prec1-= log2abs(diff_val);
  
  if(prec1 > max_prec)
    prec1  = max_prec;
  else if(!(prec1 > min_prec))
    prec1         = min_prec;
  
  val = _pmath_create_mp_float((mpfr_prec_t)ceil(prec1));
  
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return val;
  }
  
  mpfr_cosh(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDN);
    
  _pmath_mp_float_include_error(val, right_val);
  _pmath_mp_float_include_error(val, left_val);
  
  if(mpfr_cmp_abs(PMATH_AS_MP_VALUE(x), PMATH_AS_MP_ERROR(x)) < 0) {
    MPFR_DECL_INIT(one, PMATH_MP_ERROR_PREC);
    mpfr_set_si(one, 1, MPFR_RNDN);
    
    _pmath_mp_float_include_error(val, one);
  }
  
  pmath_unref(x);
  
  _pmath_mp_float_clip_error(val, min_prec, max_prec);
  val = _pmath_float_exceptions(val);
  return val;
}

PMATH_PRIVATE pmath_t builtin_cosh(pmath_expr_t expr) {
  pmath_t x;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return expr;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_is_double(x)) {
    double d = PMATH_AS_DOUBLE(x);
    double res = cosh(d);
    
    pmath_unref(x);
    if(isfinite(res)) {
      pmath_unref(expr);
      return PMATH_FROM_DOUBLE(res);
    }
    
    x = _pmath_create_mp_float_from_d(d);
    expr = pmath_expr_set_item(expr, 1, pmath_ref(x));
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_unref(expr);
    x = mp_cosh(x);
    return x;
  }
  
  if(pmath_is_number(x)) {
    int sign = pmath_number_sign(x);
    
    if(sign < 0) {
      expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
      
      return pmath_expr_set_item(
               expr, 1,
               pmath_number_neg(x));
    }
    
    if(sign == 0) {
      pmath_unref(expr);
      pmath_unref(x);
      return INT(1);
    }
    
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_expr(x)) {
    size_t len = pmath_expr_length(x);
    pmath_t head = pmath_expr_get_item(x, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES)) {
      pmath_t fst = pmath_expr_get_item(x, 1);
      
      if(pmath_is_number(fst)) {
        if(pmath_number_sign(fst) < 0) {
          expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
          
          return pmath_expr_set_item(
                   expr, 1,
                   pmath_expr_set_item(
                     x, 1,
                     pmath_number_neg(fst)));
        }
      }
      
      if(_pmath_is_imaginary(&x)) {
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_COS));
        expr = pmath_expr_set_item(expr, 1, x);
        pmath_unref(fst);
        return expr;
      }
      
      pmath_unref(fst);
    }
    else if(len == 2 && pmath_same(head, PMATH_SYMBOL_COMPLEX)) {
      pmath_t re = pmath_expr_get_item(x, 1);
      pmath_t im = pmath_expr_get_item(x, 2);
      
      if(pmath_equals(re, PMATH_FROM_INT32(0))) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(x);
        
        return FUNC(pmath_ref(PMATH_SYMBOL_COS), im);
      }
      
      if(pmath_is_float(re) || pmath_is_float(im)) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(im);
        
        expr = TIMES(
                 ONE_HALF,
                 PLUS(
                   EXP(NEG(pmath_ref(x))),
                   EXP(pmath_ref(x))));
                   
        pmath_unref(x);
        return expr;
      }
      
      pmath_unref(re);
      pmath_unref(im);
    }
    else if(len == 1) {
      pmath_t u = pmath_expr_get_item(x, 1);
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOSH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return u;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOTH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INVSQRT(MINUS(INT(1), POW(u, INT(-2)))); // 1/Sqrt(1 - 1/u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCSCH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return SQRT(PLUS(INT(1), POW(u, INT(-2)))); // Sqrt(1 + 1/u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSECH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSINH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return SQRT(PLUS(INT(1), POW(u, INT(2)))); // Sqrt(1 + u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCTANH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INVSQRT(MINUS(INT(1), POW(u, INT(2)))); // 1 / Sqrt(1 - u^2)
      }
      
      pmath_unref(u);
    }
  }
  
  pmath_unref(x);
  return expr;
}
