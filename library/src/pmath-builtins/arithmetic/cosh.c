#include <pmath-core/numbers-private.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

PMATH_PRIVATE pmath_t builtin_cosh(pmath_expr_t expr){
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_is_double(x)){
    double d = PMATH_AS_DOUBLE(x);
    double res = cosh(d);
    
    pmath_unref(x);
    if(isfinite(res)){
      pmath_unref(expr);
      return pmath_float_new_d(res);
    }
    
    x = (pmath_float_t)PMATH_FROM_PTR(_pmath_create_mp_float_from_d(d));
    expr = pmath_expr_set_item(expr, 1, pmath_ref(x));
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT)){
    pmath_float_t tmp = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
    
    if(!pmath_is_null(tmp)){
      pmath_float_t result;
      double accmant, acc, prec, val;
      long accexp;
      
      mpfr_sinh(
        PMATH_AS_MP_VALUE(tmp),
        PMATH_AS_MP_VALUE(x),
        MPFR_RNDN);
      
      // dy = d(cosh(x)) = sinh(x) * dx
      mpfr_mul(
        PMATH_AS_MP_ERROR(tmp), 
        PMATH_AS_MP_VALUE(tmp), 
        PMATH_AS_MP_ERROR(x), 
        MPFR_RNDN);
      
      // Precision(y) = -Log(base, y) + Accuracy(y)
      val = mpfr_get_d(PMATH_AS_MP_VALUE(x), MPFR_RNDN);
      val = cosh(val);
      accmant = mpfr_get_d_2exp(&accexp, PMATH_AS_MP_ERROR(tmp), MPFR_RNDN);
      acc  = -log2(fabs(accmant)) - accexp;
      prec = -log2(fabs(val)) + acc;
      
      if(prec > acc + pmath_max_extra_precision)
        prec = acc + pmath_max_extra_precision;
      else if(prec < 0)
        prec = 0;
      
      result = _pmath_create_mp_float((mp_prec_t)prec);
      if(!pmath_is_null(result)){
        mpfr_cosh(PMATH_AS_MP_VALUE(result), PMATH_AS_MP_VALUE(x),   MPFR_RNDN);
        mpfr_abs( PMATH_AS_MP_ERROR(result), PMATH_AS_MP_ERROR(tmp), MPFR_RNDU);
      }
      
      pmath_unref(expr);
      pmath_unref(x);
      pmath_unref(tmp);
      return result;
    }
  }
  
  if(pmath_is_number(x)){
    int sign = pmath_number_sign(x);
    
    if(sign < 0){
      expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
      
      return pmath_expr_set_item(
        expr, 1,
        pmath_number_neg(x));
    }
    
    if(sign == 0){
      pmath_unref(expr);
      pmath_unref(x);
      return INT(1);
    }
    
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_expr(x)){
    size_t len = pmath_expr_length(x);
    pmath_t head = pmath_expr_get_item(x, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES)){
      pmath_t fst = pmath_expr_get_item(x, 1);
      
      if(pmath_is_number(fst)){
        if(pmath_number_sign(fst) < 0){
          expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
          
          return pmath_expr_set_item(
            expr, 1,
            pmath_expr_set_item(
              x, 1,
              pmath_number_neg(fst)));
        }
      }
      
      if(_pmath_is_imaginary(&x)){
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_COS));
        expr = pmath_expr_set_item(expr, 1, x);
        pmath_unref(fst);
        return expr;
      }
      
      pmath_unref(fst);
    }
    else if(len == 2 && pmath_same(head, PMATH_SYMBOL_COMPLEX)){
      pmath_t re = pmath_expr_get_item(x, 1);
      pmath_t im = pmath_expr_get_item(x, 2);
      
      if(pmath_equals(re, PMATH_NUMBER_ZERO)){
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(x);
        
        return FUNC(pmath_ref(PMATH_SYMBOL_COS), im);
      }
      
      if(pmath_instance_of(re, PMATH_TYPE_FLOAT)
      || pmath_instance_of(im, PMATH_TYPE_FLOAT)){
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(im);
        
        expr = TIMES(
          ONE_HALF, 
          PLUS(
            EXP(NEG(pmath_ref(x))),
            EXP(    pmath_ref(x))));
        
        pmath_unref(x);
        return expr;
      }
      
      pmath_unref(re);
      pmath_unref(im);
    }
    else if(len == 1){
      pmath_t u = pmath_expr_get_item(x, 1);
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOSH)){
        pmath_unref(expr);
        pmath_unref(x);
        
        return u;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOTH)){
        pmath_unref(expr);
        pmath_unref(x);
        
        return INVSQRT(MINUS(INT(1), POW(u, INT(-2)))); // 1/Sqrt(1 - 1/u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCSCH)){
        pmath_unref(expr);
        pmath_unref(x);
        
        return SQRT(PLUS(INT(1), POW(u, INT(-2)))); // Sqrt(1 + 1/u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSECH)){
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSINH)){
        pmath_unref(expr);
        pmath_unref(x);
        
        return SQRT(PLUS(INT(1), POW(u, INT(2)))); // Sqrt(1 + u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCTANH)){
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
