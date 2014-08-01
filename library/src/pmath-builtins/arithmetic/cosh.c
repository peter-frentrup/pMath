#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


// x will be freed, x may be PMATH_NULL
static pmath_mpfloat_t mp_cosh(pmath_mpfloat_t x) {
  pmath_mpfloat_t val;
  
  if(pmath_is_null(x))
    return PMATH_NULL;
  
  assert(pmath_is_mpfloat(x));
  
  val = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
  
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return val;
  }
  
  mpfr_cosh(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    _pmath_current_rounding_mode());
    
  pmath_unref(x);
  
  val = _pmath_float_exceptions(val);
  return val;
}

PMATH_PRIVATE pmath_t builtin_cosh(pmath_expr_t expr) {
  pmath_t x;
  
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
