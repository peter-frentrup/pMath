#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>


PMATH_PRIVATE pmath_t builtin_sinh(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_is_double(x)) {
    double d = PMATH_AS_DOUBLE(x);
    double res = sinh(d);
    
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
    return _pmath_mpfloat_call(x, mpfr_sinh);
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
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_SIN));
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
                 FUNC(pmath_ref(PMATH_SYMBOL_SIN), im));
      }
      
      if(pmath_is_float(re) || pmath_is_float(im)) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(im);
        
        expr = TIMES(
                 ONE_HALF,
                 MINUS(
                   EXP(pmath_ref(x)),
                   EXP(NEG(pmath_ref(x)))));
                   
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
        
        // (1+u) Sqrt((-1+u)/(1+u))
        x = PLUS(INT(1), pmath_ref(u));
        expr = TIMES(
                 pmath_ref(x),
                 SQRT(DIV(
                        PLUS(INT(-1), u),
                        pmath_ref(x))));
        pmath_unref(x);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOTH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        // 1 / (Sqrt(1 - 1/u^2) u)
        expr = INV(
                 TIMES(
                   SQRT(
                     MINUS(
                       INT(1),
                       POW(
                         pmath_ref(u),
                         INT(-2)))),
                   pmath_ref(u)));
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCSCH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSECH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        // (1+u) Sqrt((1-u)/(1+u)) / u
        x = PLUS(INT(1), pmath_ref(u));
        expr = TIMES3(
                 pmath_ref(x),
                 SQRT(DIV(
                        MINUS(INT(1), pmath_ref(u)),
                        pmath_ref(x))),
                 INV(pmath_ref(u)));
        pmath_unref(x);
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSINH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return u;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCTANH)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        // u / Sqrt(1 - u^2)
        expr = TIMES(pmath_ref(u), INVSQRT(MINUS(INT(1), POW(pmath_ref(u), INT(2)))));
        pmath_unref(u);
        return expr;
      }
      
      pmath_unref(u);
    }
  }
  
  pmath_unref(x);
  return expr;
}
