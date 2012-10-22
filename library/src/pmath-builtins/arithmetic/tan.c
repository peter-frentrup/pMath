#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


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
static pmath_t mp_tan(pmath_mpfloat_t x) {
  pmath_thread_t thread = pmath_thread_get_current();
  double min_prec, max_prec, prec;
  pmath_mpfloat_t val;
  
  MPFR_DECL_INIT(err, PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(test_tan, PMATH_MP_ERROR_PREC);
  
  assert(pmath_is_mpfloat(x));
  
  // give up when error is >= 1
  if(mpfr_cmp_ui(PMATH_AS_MP_ERROR(x), 1) >= 1) {
    pmath_unref(x);
    return CINFTY;
  }
  
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
    
  if(min_prec == max_prec) {
    val = _pmath_create_mp_float(1 + (mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(val)) {
      pmath_unref(x);
      return val;
    }
    
    mpfr_tan(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    if(!mpfr_number_p(PMATH_AS_MP_VALUE(val))) {
      pmath_unref(val);
      pmath_unref(x);
      
      return CINFTY;
    }
    
    if(mpfr_cmp_abs(PMATH_AS_MP_VALUE(val), PMATH_AS_MP_ERROR(x)) <= 0) { // Tan is nearly 0
      mpfr_set(
        PMATH_AS_MP_ERROR(val), 
        PMATH_AS_MP_ERROR(x), 
        MPFR_RNDN);
    }
    else{
      mpfr_set_d(test_tan, -min_prec, MPFR_RNDN);
      mpfr_ui_pow(
        err,
        2,
        test_tan,
        MPFR_RNDU);
        
      mpfr_mul(
        PMATH_AS_MP_ERROR(val), 
        PMATH_AS_MP_VALUE(val), 
        err,
        MPFR_RNDA);
      mpfr_abs(
        PMATH_AS_MP_ERROR(val),
        PMATH_AS_MP_ERROR(val),
        MPFR_RNDU);
    }
      
    pmath_unref(x);
    return val;
  }
  
  mpfr_tan(
    test_tan,
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDZ);
  
  if(mpfr_cmp_abs(test_tan, PMATH_AS_MP_ERROR(x)) <= 0) { // Tan is nearly 0
    pmath_bool_t changed_prec = FALSE;
    prec = -log2abs(PMATH_AS_MP_ERROR(x));
    
    if(prec < min_prec) {
      prec = min_prec;
      changed_prec = TRUE;
    }
    else if(prec > max_prec) {
      prec = max_prec;
      changed_prec = TRUE;
    }
    
    val = _pmath_create_mp_float(1 + (mpfr_prec_t)ceil(prec));
    if(pmath_is_null(val)) {
      pmath_unref(x);
      return PMATH_NULL;
    }
    
    mpfr_tan(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    if(changed_prec) {
      mpfr_set_d(err, -prec, MPFR_RNDU);
      
      mpfr_ui_pow(
        PMATH_AS_MP_ERROR(val),
        2,
        err,
        MPFR_RNDU);
    }
    else{
      mpfr_set(
        PMATH_AS_MP_ERROR(val),
        PMATH_AS_MP_ERROR(x),
        MPFR_RNDN);
    }
    
    pmath_unref(x);
    
    return val;
  }
  
  // error(Tan(x)) ~= Tan'(x) error(x) = Sec(x)^2 error(x)
  // We give up whenever Abs(Tan(x)) <= Sec(x)^2 error(x)/2, because then whe 
  // are near a pole (or near 0, but that was handled above)
  
  mpfr_sec(
    err,
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDA);
  
  mpfr_mul(err, err, err, MPFR_RNDU);
  mpfr_mul(err, err, PMATH_AS_MP_ERROR(x), MPFR_RNDU);
  
  mpfr_mul_2ui(test_tan, test_tan, 1, MPFR_RNDZ);
  if( !mpfr_regular_p(test_tan) || 
      !mpfr_regular_p(err) || 
      mpfr_cmp_abs(test_tan, err) <= 0) 
  { // error too large, give up
    pmath_unref(x);
    return CINFTY;
  }
  
  prec = log2abs(test_tan) + 1 - log2abs(err); // +1 because test_tan = 2 Tan(x)
  
  if(prec < min_prec) {
    prec = min_prec;
    
    mpfr_set_d(err, -prec, MPFR_RNDU);
    mpfr_ui_pow(
      err,
      2,
      err,
      MPFR_RNDU);
  }
  else if(prec > max_prec) {
    prec = max_prec;
    
    mpfr_set_d(err, -prec, MPFR_RNDU);
    mpfr_ui_pow(
      err,
      2,
      err,
      MPFR_RNDU);
  }
  
  val = _pmath_create_mp_float(1 + (mpfr_prec_t)ceil(prec));
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  mpfr_tan(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDN);
  
  mpfr_set(
    PMATH_AS_MP_ERROR(val),
    err,
    MPFR_RNDU);
  
  pmath_unref(x);
  
  val = _pmath_float_exceptions(val);
  return val;
}

PMATH_PRIVATE pmath_t builtin_tan(pmath_expr_t expr) {
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
    double res = tan(d);
    
    pmath_unref(x);
    pmath_unref(expr);
    if(isfinite(res))
      return PMATH_FROM_DOUBLE(res);
      
    return CINFTY;
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_unref(expr);
    x = mp_tan(x);
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
  
  if(pmath_same(x, PMATH_SYMBOL_PI)) {
    pmath_unref(expr);
    pmath_unref(x);
    return INT(0);
  }
  
  if(_pmath_contains_symbol(x, PMATH_SYMBOL_DEGREE)) {
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
    if(!pmath_is_expr_of(y, PMATH_SYMBOL_TAN)) {
      pmath_unref(x);
      pmath_unref(expr);
      return y;
    }
    
    pmath_unref(y);
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
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_TANH));
        expr = pmath_expr_set_item(expr, 1, x);
        pmath_unref(fst);
        return TIMES(COMPLEX(INT(0), INT(1)), expr);
      }
      
      if(len == 2) {
        pmath_t snd = pmath_expr_get_item(x, 2);
        pmath_unref(snd);
        
        if(pmath_same(snd, PMATH_SYMBOL_PI)) {
          pmath_t cmp;
          
          if(pmath_equals(fst, _pmath_one_half)) {
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
          if( pmath_same(cmp, PMATH_SYMBOL_TRUE)  &&
              pmath_is_quotient(fst)              &&
              pmath_is_int32(PMATH_QUOT_NUM(fst)) &&
              pmath_is_int32(PMATH_QUOT_DEN(fst)) &&
              PMATH_AS_INT32(PMATH_QUOT_NUM(fst)) >= 0)
          {
            unsigned num = (unsigned)PMATH_AS_INT32(PMATH_QUOT_NUM(fst));
            unsigned den = (unsigned)PMATH_AS_INT32(PMATH_QUOT_DEN(fst));
            
            if(num <= den / 2)
              switch(den) {
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
          else if(pmath_same(cmp, PMATH_SYMBOL_FALSE)) { // 1/2 Pi <= x
            expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
            
            cmp = pmath_evaluate(
                    FUNC2(pmath_ref(PMATH_SYMBOL_LESS), pmath_ref(fst), INT(1)));
                    
            pmath_unref(cmp);
            if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // 1/2 Pi <= x < Pi
              fst = MINUS(INT(1), fst);
              
              x = pmath_expr_set_item(x, 1, fst);
              return NEG(pmath_expr_set_item(expr, 1, x)); // return -Tan((1-fst)*PI)
            }
            
            cmp = pmath_evaluate(
                    FUNC2(pmath_ref(PMATH_SYMBOL_LESS), pmath_ref(fst), INT(2)));
                    
            pmath_unref(cmp);
            if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // Pi <= x < 2 Pi
              fst = PLUS(fst, INT(-1));
              
              x = pmath_expr_set_item(x, 1, fst);
              return pmath_expr_set_item(expr, 1, x); // return Tan((fst-1)*PI)
            }
            
            cmp = pmath_evaluate(
                    pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_FLOOR), 2,
                      pmath_ref(fst),
                      PMATH_FROM_INT32(2)));
                      
            fst = PLUS(fst, TIMES(INT(-2), cmp));
            
            x    = pmath_expr_set_item(x,    1, fst);
            return pmath_expr_set_item(expr, 1, x); // return Tan((fst - Floor(fst, 2))*PI)
          }
        }
      }
      
      pmath_unref(fst);
    }
    else if( pmath_same(head, PMATH_SYMBOL_PLUS) &&
             _pmath_contains_symbol(x, PMATH_SYMBOL_PI))
    {
      size_t i;
      for(i = pmath_expr_length(x); i > 0; --i) {
        pmath_t tmp = pmath_expr_get_item(x, i);
        
        tmp = pmath_evaluate(DIV(tmp, pmath_ref(PMATH_SYMBOL_PI)));
        
        if(pmath_is_integer(tmp)) {
          expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
          x    = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
          x    = pmath_expr_remove_all(x, PMATH_UNDEFINED);
          expr = pmath_expr_set_item(expr, 1, x);
          
          pmath_unref(tmp);
          
          return expr;
        }
        
        if(pmath_is_quotient(tmp)) {
          pmath_t den = pmath_rational_denominator(tmp);
          
          if(pmath_equals(den, PMATH_FROM_INT32(2))) {
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
    else if(len == 2 && pmath_same(head, PMATH_SYMBOL_COMPLEX)) {
      pmath_t re = pmath_expr_get_item(x, 1);
      pmath_t im = pmath_expr_get_item(x, 2);
      
      if(pmath_equals(re, PMATH_FROM_INT32(0))) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(x);
        
        return TIMES(
                 COMPLEX(INT(0), INT(1)),
                 FUNC(pmath_ref(PMATH_SYMBOL_TANH), im));
      }
      
      if(pmath_is_float(re) || pmath_is_float(im)) {
        pmath_unref(expr);
        x = pmath_expr_set_item(x, 1, NEG(im));
        x = pmath_expr_set_item(x, 2, re);
        // new x = I * oldx
        
        expr = pmath_evaluate(PLUS(EXP(NEG(pmath_ref(x))), EXP(pmath_ref(x))));
        if(pmath_is_number(expr) && pmath_number_sign(expr) == 0) {
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
    else if(len == 1) {
      pmath_t u = pmath_expr_get_item(x, 1);
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOS)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = DIV(SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(2)))), pmath_ref(u));
        // Sqrt(1 - u^2)/u
        
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOT)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCSC)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = TIMES(INV(pmath_ref(u)), INVSQRT(MINUS(INT(1), POW(pmath_ref(u), INT(-2)))));
        // 1/u * 1/Sqrt(1 - 1/u^2))
        
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSEC)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = TIMES(pmath_ref(u), SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(-2)))));
        // u Sqrt(1 - 1/u^2)
        
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSIN)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = DIV(pmath_ref(u), SQRT(MINUS(INT(1), POW(pmath_ref(u), INT(2)))));
        // u / Sqrt(1 - u^2)
        
        pmath_unref(u);
        return expr;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCTAN)) {
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
