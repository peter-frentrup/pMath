#include <pmath-core/numbers-private.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>

PMATH_PRIVATE pmath_t builtin_tan(pmath_expr_t expr){
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_instance_of(x, PMATH_TYPE_MACHINE_FLOAT)){
    double d = ((struct _pmath_machine_float_t*)x)->value;
    double res = tan(d);
    
    pmath_unref(x);
    pmath_unref(expr);
    if(isfinite(res))
      return pmath_float_new_d(res);
    
    return CINFTY;
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT)){
    struct _pmath_mp_float_t *tmp;
    
    tmp = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
    
    if(tmp){
      struct _pmath_mp_float_t *result;
      double sin_val, cos_val;
      double accmant, acc, prec;
      long accexp;
      
      mpfr_sin_cos(
        tmp->value, // sin 
        tmp->error, // cos
        ((struct _pmath_mp_float_t*)x)->value, 
        GMP_RNDN);
      
      sin_val = mpfr_get_d(tmp->value, GMP_RNDN);
      cos_val = mpfr_get_d(tmp->error, GMP_RNDN);
      
      // dy = d(tan(x)) = sec(x)^2 * dx = 1/cos(x)^2 * dx
      mpfr_pow_si(tmp->value, tmp->error, -2, GMP_RNDN);
      mpfr_mul(tmp->error, tmp->value, ((struct _pmath_mp_float_t*)x)->error, GMP_RNDN);
      
      // Precision(y) = -Log(base, y) + Accuracy(y)
      accmant = mpfr_get_d_2exp(&accexp, ((struct _pmath_mp_float_t*)tmp)->error, GMP_RNDN);
      acc  = -log2(fabs(accmant)) - accexp;
      prec = -log2(fabs(sin_val/cos_val)) + acc;
      
      if(prec > acc + pmath_max_extra_precision)
        prec = acc + pmath_max_extra_precision;
      else if(prec < 0)
        prec = 0;
      
      result = _pmath_create_mp_float((mp_prec_t)prec);
      if(result){
        mpfr_tan(result->value, ((struct _pmath_mp_float_t*)x)->value, GMP_RNDN);
        
        if(!mpfr_number_p(result->value)){
          pmath_unref((pmath_float_t)result);
          pmath_unref((pmath_float_t)tmp);
          pmath_unref(x);
          pmath_unref(expr);
          return CINFTY;
        }
        
        mpfr_abs(result->error, tmp->error, GMP_RNDU);
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
  
  if(x == PMATH_SYMBOL_PI){
    pmath_unref(expr);
    pmath_unref(x);
    return INT(0);
  }
  
  if(_pmath_contains_symbol(x, PMATH_SYMBOL_DEGREE)){
    pmath_t y = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_WITH), 2,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_LIST), 1,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_ASSIGN), 2,
          pmath_ref(PMATH_SYMBOL_DEGREE),
          TIMES(pmath_ref(PMATH_SYMBOL_PI), QUOT(1, 180)))),
        pmath_ref(expr));
    
    y = pmath_evaluate(y);
    if(!pmath_is_expr_of(y, PMATH_SYMBOL_TAN)){
      pmath_unref(x);
      pmath_unref(expr);
      return y;
    }
    
    pmath_unref(y);
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
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_TANH));
        expr = pmath_expr_set_item(expr, 1, x);
        pmath_unref(fst);
        return TIMES(COMPLEX(INT(0), INT(1)), expr);
      }
      
      if(len == 2){
        pmath_t snd = pmath_expr_get_item(x, 2);
        pmath_unref(snd);
        
        if(snd == PMATH_SYMBOL_PI){
          pmath_t cmp;
          
          if(pmath_equals(fst, _pmath_one_half)){
            pmath_unref(expr);
            pmath_unref(x);
            pmath_unref(fst);
            
            return CINFTY;
          }
          
          cmp = pmath_evaluate(
            pmath_expr_new_extended(
              pmath_ref(PMATH_SYMBOL_LESS), 2,
              pmath_ref(fst),
              pmath_ref(_pmath_one_half)));
          
          pmath_unref(cmp);
          if(cmp == PMATH_SYMBOL_TRUE
          && pmath_instance_of(fst, PMATH_TYPE_QUOTIENT)
          && pmath_integer_fits_ui((pmath_integer_t)((struct _pmath_quotient_t*)fst)->numerator)
          && pmath_integer_fits_ui((pmath_integer_t)((struct _pmath_quotient_t*)fst)->denominator)
          ){
            unsigned long num = pmath_integer_get_ui((pmath_integer_t)((struct _pmath_quotient_t*)fst)->numerator);
            unsigned long den = pmath_integer_get_ui((pmath_integer_t)((struct _pmath_quotient_t*)fst)->denominator);
            
            if(num <= den / 2)
              switch(den){
                case 3:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  return SQRT(INT(3));
                
                case 4:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                    
                  return INT(1);
                
                case 5:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  if(num == 1)
                    return SQRT(PLUS(INT(5), TIMES(INT(-2), SQRT(INT(5)))));
                    
                  return SQRT(PLUS(INT(5), TIMES(INT(2), SQRT(INT(5)))));
                
                case 6:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  return INVSQRT(INT(3));
                
                case 10:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  if(num == 1)
                    return SQRT(PLUS(INT(1), TIMES(INT(-2), INVSQRT(INT(5)))));
                  
                  return SQRT(PLUS(INT(1), TIMES(INT(2), INVSQRT(INT(5)))));
                
                case 12:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  if(num == 1)
                    return MINUS(INT(2), SQRT(INT(3)));
                  
                  return PLUS(INT(2), SQRT(INT(3)));
              }
          }
          else if(cmp == PMATH_SYMBOL_FALSE){ // 1/2 Pi <= x
            expr = pmath_expr_set_item(expr, 1, NULL);
            
            cmp = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_LESS), 2,
                pmath_ref(fst),
                pmath_integer_new_si(1)));
                
            pmath_unref(cmp);
            if(cmp == PMATH_SYMBOL_TRUE){ // 1/2 Pi <= x < Pi
              fst = MINUS(INT(1), fst);
              
              x = pmath_expr_set_item(x, 1, fst);
              return NEG(pmath_expr_set_item(expr, 1, x)); // return -Tan((1-fst)*PI)
            }
            
            cmp = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_LESS), 2,
                pmath_ref(fst),
                pmath_integer_new_si(2)));
                
            pmath_unref(cmp);
            if(cmp == PMATH_SYMBOL_TRUE){ // Pi <= x < 2 Pi
              fst = PLUS(fst, INT(-1));
              
              x = pmath_expr_set_item(x, 1, fst);
              return pmath_expr_set_item(expr, 1, x); // return Tan((fst-1)*PI)
            }
            
            cmp = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_FLOOR), 2,
                pmath_ref(fst),
                pmath_integer_new_si(2)));
            
            fst = PLUS(fst, TIMES(INT(-2), cmp));
            
            x    = pmath_expr_set_item(x,    1, fst);
            return pmath_expr_set_item(expr, 1, x); // return Tan((fst - Floor(fst, 2))*PI)
          }
        }
      }
    
      pmath_unref(fst);
    }
    else if(head == PMATH_SYMBOL_PLUS && _pmath_contains_symbol(x, PMATH_SYMBOL_PI)){
      size_t i;
      for(i = pmath_expr_length(x);i > 0;--i){
        pmath_t tmp = pmath_expr_get_item(x, i);
        
        tmp = pmath_evaluate(DIV(tmp, pmath_ref(PMATH_SYMBOL_PI)));
        
        if(pmath_is_integer(tmp)){
          expr = pmath_expr_set_item(expr, 1, NULL);
          x = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
          x = pmath_expr_remove_all(x, PMATH_UNDEFINED);
          expr = pmath_expr_set_item(expr, 1, x);
          
          pmath_unref(tmp);
          
          return expr;
        }
        
        if(pmath_instance_of(tmp, PMATH_TYPE_QUOTIENT)){
          pmath_t den = pmath_rational_denominator(tmp);
          
          if(pmath_equals(den, PMATH_NUMBER_TWO)){
            pmath_unref(den);
            pmath_unref(tmp);
            pmath_unref(expr);
            
            x = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
            x = pmath_expr_remove_all(x, PMATH_UNDEFINED);
            
            return NEG(FUNC(pmath_ref(PMATH_SYMBOL_TAN), x));
          }
          
          pmath_unref(den);
        }
        
        pmath_unref(tmp);
      }
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
          FUNC(pmath_ref(PMATH_SYMBOL_TANH), im));
      }
      
      if(pmath_instance_of(re, PMATH_TYPE_FLOAT)
      || pmath_instance_of(im, PMATH_TYPE_FLOAT)){
        pmath_unref(expr);
        x = pmath_expr_set_item(x, 1, NEG(im));
        x = pmath_expr_set_item(x, 2, re);
        // new x = I * oldx
        
        expr = pmath_evaluate(PLUS(EXP(NEG(pmath_ref(x))), EXP(pmath_ref(x))));
        if(pmath_is_number(expr) && pmath_number_sign(expr) == 0){
          pmath_unref(x);
          pmath_unref(expr);
          return CINFTY;
        }
        
        expr = TIMES3(
          COMPLEX(INT(0), INT(1)),
          MINUS(EXP(NEG(pmath_ref(x))), EXP(pmath_ref(x))),
          INV(expr));
        
        pmath_unref(x);
        return expr;
      }
      
      pmath_unref(re);
      pmath_unref(im);
    }
    else if(len == 1){
      pmath_t u = pmath_expr_get_item(x, 1);
      
      if(head == PMATH_SYMBOL_ARCCOS){
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = DIV(SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(2)))), pmath_ref(u));
        // Sqrt(1 - u^2)/u
        
        pmath_unref(u);
        return expr;
      }
      
      if(head == PMATH_SYMBOL_ARCCOT){
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(head == PMATH_SYMBOL_ARCCSC){
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = TIMES(INV(pmath_ref(u)), INVSQRT(MINUS(INT(1), POW(pmath_ref(u), INT(-2)))));
        // 1/u * 1/Sqrt(1 - 1/u^2))
        
        pmath_unref(u);
        return expr;
      }
      
      if(head == PMATH_SYMBOL_ARCSEC){
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = TIMES(pmath_ref(u), SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(-2)))));
        // u Sqrt(1 - 1/u^2)
        
        pmath_unref(u);
        return expr;
      }
      
      if(head == PMATH_SYMBOL_ARCSIN){
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = DIV(pmath_ref(u), SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(2)))));
        // u / Sqrt(1 - u^2)
        
        pmath_unref(u);
        return expr;
      }
      
      if(head == PMATH_SYMBOL_ARCTAN){
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
