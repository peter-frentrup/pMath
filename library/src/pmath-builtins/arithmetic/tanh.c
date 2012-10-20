#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


static double dx_to_accuracy(mpfr_t dx) {
  mpfr_exp_t accexp;
  double accmant;
  
  // accuracy = -Log(2, dx)
  accmant = mpfr_get_d_2exp(&accexp, dx, MPFR_RNDN);
  return -log2(fabs(accmant)) - accexp;
}

// x will be freed, x may be PMATH_NULL
static pmath_mpfloat_t mp_tanh(pmath_mpfloat_t x) {
  pmath_thread_t thread = pmath_thread_get_current();
  double min_prec, max_prec, prec;
  pmath_mpfloat_t val;
  
  MPFR_DECL_INIT(err_x,    PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(min_val,  PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(max_val,  PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(diff_val, PMATH_MP_ERROR_PREC);
  
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
    
    mpfr_tanh(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    mpfr_set_d(err_x, -min_prec, MPFR_RNDN);
    
    mpfr_ui_pow(
      PMATH_AS_MP_ERROR(val),
      2,
      err_x,
      MPFR_RNDU);
      
    pmath_unref(x);
    return val;
  }
  
  mpfr_add(
    err_x,
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDN);
    
  mpfr_tanh(
    max_val,
    err_x,
    MPFR_RNDN);
    
  mpfr_sub(
    err_x,
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDN);
    
  mpfr_tanh(
    min_val,
    err_x,
    MPFR_RNDN);
    
  mpfr_sub(
    diff_val,
    max_val,
    min_val,
    MPFR_RNDA);
  mpfr_abs(diff_val, diff_val, MPFR_RNDU);
  mpfr_div_2ui(diff_val, diff_val, 1, MPFR_RNDU);
  
  // -1 <= Tanh(x) <= 1  => precision === Log(2, |y|) + accuracy <= accuracy
  prec = ceil(dx_to_accuracy(diff_val));
  
  if(prec > max_prec)
    prec  = max_prec;
  else if(!(prec > min_prec))
    prec         = min_prec;
  
  val = _pmath_create_mp_float((mpfr_prec_t)ceil(prec));
  
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return val;
  }
  
  mpfr_tanh(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDN);
    
  _pmath_mp_float_include_error(val, max_val);
  _pmath_mp_float_include_error(val, min_val);
  
  pmath_unref(x);
  return val;
}

PMATH_PRIVATE pmath_t builtin_tanh(pmath_expr_t expr) {
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
    double res = tanh(d);
    
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
    x = mp_tanh(x);
    return x;
  }
  
  if(pmath_is_number(x)) {
    int sign = pmath_number_sign(x);
    
    if(sign < 0) {
      expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
      
      return NEG(
               pmath_expr_set_item(
                 expr, 1,
                 pmath_number_neg(x)));
    }
    
    if(sign == 0) {
      pmath_unref(expr);
      return x;
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
          
          return NEG(
                   pmath_expr_set_item(
                     expr, 1,
                     pmath_expr_set_item(
                       x, 1,
                       pmath_number_neg(fst))));
        }
      }
      
      if(_pmath_is_imaginary(&x)) {
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_TAN));
        expr = pmath_expr_set_item(expr, 1, x);
        pmath_unref(fst);
        return TIMES(COMPLEX(INT(0), INT(1)), expr);
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
        
        return TIMES(
                 COMPLEX(INT(0), INT(1)),
                 FUNC(pmath_ref(PMATH_SYMBOL_TAN), im));
      }
      
      if(pmath_is_float(re) || pmath_is_float(im)) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(im);
        
        // Tanh(x) = (E^(2x) - 1) / (E^(2x) + 1)
        
        x = pmath_evaluate(EXP(TIMES(INT(2), x)));
        
        if(pmath_is_number(x) && pmath_compare(x, PMATH_FROM_INT32(-1)) == 0) {
          pmath_unref(x);
          return CINFTY;
        }
        
        expr = DIV(
                 PLUS(pmath_ref(x), INT(-1)),
                 PLUS(pmath_ref(x), INT(1)));
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
        
        // (1+u) Sqrt((-1+u)/(1+u)) /u
        x = PLUS(INT(1), pmath_ref(u));
        expr = TIMES3(
                 pmath_ref(x),
                 SQRT(DIV(
                        PLUS(INT(-1), pmath_ref(u)),
                        pmath_ref(x))),
                 INV(pmath_ref(u)));
        pmath_unref(x);
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOTH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCSCH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        // 1 / (Sqrt(1 + 1/u^2) u)
        expr = INV(
                 TIMES(
                   SQRT(
                     PLUS(
                       INT(1),
                       POW(
                         pmath_ref(u),
                         INT(-2)))),
                   pmath_ref(u)));
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSECH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        // (1+u) Sqrt((1-u)/(1+u))
        x = PLUS(INT(1), pmath_ref(u));
        expr = TIMES(
                 pmath_ref(x),
                 SQRT(DIV(
                        MINUS(INT(1), u),
                        pmath_ref(x))));
        pmath_unref(x);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSINH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        // u / Sqrt(1 + u^2)
        expr = TIMES(pmath_ref(u), INVSQRT(PLUS(INT(1), POW(pmath_ref(u), INT(2)))));
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCTANH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return u;
      }
      
      pmath_unref(u);
    }
  }
  
  pmath_unref(x);
  return expr;
}
