#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

PMATH_PRIVATE pmath_t builtin_arctan(pmath_expr_t expr){
  pmath_t x;
  int xclass;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_equals(x, PMATH_NUMBER_ZERO)){
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MACHINE_FLOAT)){
    double d = pmath_number_get_d(x);
    pmath_unref(expr);
    pmath_unref(x);
    
    return pmath_float_new_d(atan(d));
  }
  
  if(pmath_instance_of(x, PMATH_TYPE_MP_FLOAT)){
    struct _pmath_mp_float_t *tmp;
    
    tmp = _pmath_create_mp_float(PMATH_MP_ERROR_PREC);
    if(tmp){
      struct _pmath_mp_float_t *result;
      double accmant, acc, prec;
      long accexp;
      
      // dy = dx / (1+x^2)
      mpfr_mul(
        tmp->error,
        ((struct _pmath_mp_float_t*)PMATH_AS_PTR(x))->value,
        ((struct _pmath_mp_float_t*)PMATH_AS_PTR(x))->value,
        GMP_RNDD);
      
      mpfr_add_ui(
        tmp->value,
        tmp->error,
        1,
        GMP_RNDD);
      
      mpfr_div(
        tmp->error,
        ((struct _pmath_mp_float_t*)PMATH_AS_PTR(x))->error,
        tmp->value,
        GMP_RNDU);
      
      // Precision(y) = -Log(base, y) + Accuracy(y)
      accmant = mpfr_get_d_2exp(&accexp, ((struct _pmath_mp_float_t*)tmp)->error, GMP_RNDN);
      acc  = -log2(fabs(accmant)) - accexp;
      prec = mpfr_get_d(((struct _pmath_mp_float_t*)PMATH_AS_PTR(x))->value, GMP_RNDN);
      prec = -log2(fabs(atan(prec))) + acc;
      
      if(prec > acc + pmath_max_extra_precision)
        prec = acc + pmath_max_extra_precision;
      else if(prec < 0)
        prec = 0;
      
      result = _pmath_create_mp_float((mp_prec_t)prec);
      if(result){
        mpfr_swap(result->error, tmp->error);
        
        mpfr_atan(
          result->value, 
          ((struct _pmath_mp_float_t*)PMATH_AS_PTR(x))->value,
          GMP_RNDN);
        
        pmath_unref(x);
        pmath_unref(expr);
        pmath_unref((pmath_float_t)PMATH_FROM_PTR(tmp));
        return (pmath_float_t)PMATH_FROM_PTR(result);
      }
      
      pmath_unref((pmath_float_t)PMATH_FROM_PTR(tmp));
    }
  }
  
  if(pmath_is_expr(x)){
    pmath_t head = pmath_expr_get_item(x, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES)){
      pmath_t fst = pmath_expr_get_item(x, 1);
      
      if(pmath_is_number(fst)){
        if(pmath_number_sign(fst) < 0){
          x = pmath_expr_set_item(x, 1, pmath_number_neg(fst));
          expr = pmath_expr_set_item(expr, 1, x);
          return NEG(expr);
        }
      }
      
      pmath_unref(fst);
      
      if(_pmath_is_imaginary(&x)){
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_ARCTANH));
        expr = pmath_expr_set_item(expr, 1, x);
        return TIMES(COMPLEX(INT(0), INT(1)), expr);
      }
    }
  }
  
  if(pmath_is_expr_of(x, PMATH_SYMBOL_TIMES)){
    pmath_t fst = pmath_expr_get_item(x, 1);
    
    if(pmath_is_number(fst)){
      if(pmath_number_sign(fst) < 0){
        x = pmath_expr_set_item(x, 1, pmath_number_neg(fst));
        expr = pmath_expr_set_item(expr, 1, x);
        return TIMES(INT(-1), expr);
      }
    }
    
    pmath_unref(fst);
  }
  
  xclass = _pmath_number_class(x);
  
  if(xclass & PMATH_CLASS_ZERO){
    pmath_unref(expr);
    return x;
  }
  
  if(xclass & PMATH_CLASS_POSONE){
    pmath_unref(expr);
    pmath_unref(x);
    return TIMES(QUOT(1, 4), pmath_ref(PMATH_SYMBOL_PI));
  }
  
  if(xclass & PMATH_CLASS_NEG){
    x = NEG(x);
    expr = pmath_expr_set_item(expr, 1, x);
    return NEG(expr);
  }
  
  if(xclass & PMATH_CLASS_INF){
    pmath_t infdir = _pmath_directed_infinity_direction(x);
    if(!pmath_same(infdir, PMATH_NULL)){
      pmath_unref(expr);
      pmath_unref(x);
      
      if(pmath_equals(infdir, PMATH_NUMBER_ZERO)){
        pmath_unref(infdir);
        return pmath_ref(PMATH_SYMBOL_UNDEFINED);
      }
      
      return TIMES3(ONE_HALF, pmath_ref(PMATH_SYMBOL_PI), INV(infdir));
    }
  }
  
  if(xclass & PMATH_CLASS_COMPLEX){
    pmath_t re = PMATH_NULL;
    pmath_t im = PMATH_NULL;
    if(_pmath_re_im(x, &re, &im)){
      if(pmath_equals(re, PMATH_NUMBER_ZERO)){
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(x);
        
        return TIMES(COMPLEX(INT(0), INT(1)), FUNC(pmath_ref(PMATH_SYMBOL_ARCTANH), im));
      }
      
      if(pmath_instance_of(re, PMATH_TYPE_FLOAT)
      || pmath_instance_of(im, PMATH_TYPE_FLOAT)){
        pmath_unref(re);
        pmath_unref(im);
        pmath_unref(expr);
        
        // ArcTan(x) = -1/2 I Log((1 + I x) / (1 - I x))
        expr = TIMES(
          COMPLEX(INT(0), QUOT(-1, 2)),
          LOG(DIV(
            PLUS(INT(1), TIMES(COMPLEX(INT(0), INT( 1)), pmath_ref(x))),
            PLUS(INT(1), TIMES(COMPLEX(INT(0), INT(-1)), pmath_ref(x))))));
        pmath_unref(x);
        return expr;
      }
    }
    
    pmath_unref(re);
    pmath_unref(im);
  }
  
  pmath_unref(x);
  return expr;
}
