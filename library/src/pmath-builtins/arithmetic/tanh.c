#include <pmath-core/numbers-private.h>
#include <pmath-core/symbols.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>

PMATH_PRIVATE pmath_t builtin_tanh(pmath_expr_t expr){
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(x, PMATH_TYPE_MACHINE_FLOAT)){
    double d = ((struct _pmath_machine_float_t*)x)->value;
    double res = tanh(d);
    
    pmath_unref(x);
    if(isfinite(res)){
      pmath_unref(expr);
      return pmath_float_new_d(res);
    }
    
    x = (pmath_float_t)_pmath_create_mp_float_from_d(d);
    expr = pmath_expr_set_item(expr, 1, pmath_ref(x));
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT)){
    struct _pmath_mp_float_t *tmp;
    
    tmp = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
    
    if(tmp){
      struct _pmath_mp_float_t *result;
      double accmant, acc, prec, val;
      long accexp;
      
      mpfr_sech(
        tmp->error,
        ((struct _pmath_mp_float_t*)x)->value, 
        GMP_RNDN);
      
      // dy = d(tanh(x)) = sech(x)^2 * dx
      mpfr_mul(tmp->value, tmp->error, tmp->error, GMP_RNDN);
      mpfr_mul(tmp->error, tmp->value, ((struct _pmath_mp_float_t*)x)->error, GMP_RNDN);
      
      // Precision(y) = -Log(base, y) + Accuracy(y)
      val = mpfr_get_d(((struct _pmath_mp_float_t*)x)->value, GMP_RNDN);
      val = tanh(val);
      accmant = mpfr_get_d_2exp(&accexp, ((struct _pmath_mp_float_t*)tmp)->error, GMP_RNDN);
      acc  = -log2(fabs(accmant)) - accexp;
      prec = -log2(fabs(val)) + acc;
      
      if(prec > acc + pmath_max_extra_precision)
        prec = acc + pmath_max_extra_precision;
      else if(prec < 0)
        prec = 0;
      
      result = _pmath_create_mp_float((mp_prec_t)prec);
      if(result){
        mpfr_tanh(result->value, ((struct _pmath_mp_float_t*)x)->value, GMP_RNDN);
        mpfr_abs( result->error, tmp->error, GMP_RNDU);
      }
      
      pmath_unref(expr);
      pmath_unref(x);
      pmath_unref((pmath_float_t)tmp);
      return (pmath_float_t)result;
    }
  }
  
  if(pmath_is_number(x)){
    int sign = pmath_number_sign(x);
    
    if(sign < 0){
      expr = pmath_expr_set_item(expr, 1, NULL);
      
      return NEG(
        pmath_expr_set_item(
          expr, 1,
          pmath_number_neg(x)));
    }
    
    if(sign == 0){
      pmath_unref(expr);
      return x;
    }
    
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_expr(x)){
    size_t len = pmath_expr_length(x);
    pmath_t head = pmath_expr_get_item(x, 0);
    pmath_unref(head);
    
    if(head == PMATH_SYMBOL_TIMES){
      pmath_t fst = pmath_expr_get_item(x, 1);
      
      if(pmath_is_number(fst)){
        if(pmath_number_sign(fst) < 0){
          expr = pmath_expr_set_item(expr, 1, NULL);
          
          return NEG(
            pmath_expr_set_item(
              expr, 1,
              pmath_expr_set_item(
                x, 1,
                pmath_number_neg(fst))));
        }
      }
      
      if(_pmath_is_imaginary(&x)){
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_TAN));
        expr = pmath_expr_set_item(expr, 1, x);
        pmath_unref(fst);
        return TIMES(COMPLEX(INT(0), INT(1)), expr);
      }
      
      pmath_unref(fst);
    }
    else if(len == 2 && head == PMATH_SYMBOL_COMPLEX){
      pmath_t re = pmath_expr_get_item(x, 1);
      pmath_t im = pmath_expr_get_item(x, 2);
      
      if(pmath_equals(re, PMATH_NUMBER_ZERO)){
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(x);
        
        return TIMES(
          COMPLEX(INT(0), INT(1)),
          FUNC(pmath_ref(PMATH_SYMBOL_TAN), im));
      }
      
      if(pmath_instance_of(re, PMATH_TYPE_FLOAT)
      || pmath_instance_of(im, PMATH_TYPE_FLOAT)){
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(im);
        
        // Tanh(x) = (E^(2x) - 1) / (E^(2x) + 1)
        
        x = pmath_evaluate(EXP(TIMES(INT(2), x)));
        
        if(pmath_is_number(x) && pmath_compare(x, PMATH_NUMBER_MINUSONE) == 0){
          pmath_unref(x);
          return CINFTY;
        }
        
        expr = DIV(
          PLUS(pmath_ref(x), INT(-1)), 
          PLUS(pmath_ref(x), INT( 1)));
        pmath_unref(x);
        return expr;
      }
      
      pmath_unref(re);
      pmath_unref(im);
    }
    else if(len == 1){
      pmath_t u = pmath_expr_get_item(x, 1);
      
      if(head == PMATH_SYMBOL_ARCCOSH){
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
      
      if(head == PMATH_SYMBOL_ARCCOTH){
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(head == PMATH_SYMBOL_ARCCSCH){
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
      
      if(head == PMATH_SYMBOL_ARCSECH){
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
      
      if(head == PMATH_SYMBOL_ARCSINH){
        pmath_unref(expr);
        pmath_unref(x);
        
        // u / Sqrt(1 + u^2)
        expr = TIMES(pmath_ref(u), INVSQRT(PLUS(INT(1), POW(pmath_ref(u), INT(2)))));
        pmath_unref(u);
        return expr;
      }
      
      if(head == PMATH_SYMBOL_ARCTANH){
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
