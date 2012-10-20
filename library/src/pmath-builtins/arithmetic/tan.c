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
  double min_prec, max_prec, prec1, prec2;
  pmath_mpfloat_t val, err_x;
  
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
    
    mpfr_tan(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    if(!mpfr_number_p(PMATH_AS_MP_VALUE(val))) {
      pmath_unref(val);
      pmath_unref(x);
      
      return CINFTY;
    }
    
    mpfr_set_d(left_val, -min_prec, MPFR_RNDN);
    mpfr_ui_pow(
      right_val,
      2,
      left_val,
      MPFR_RNDU);
      
    mpfr_mul(
      PMATH_AS_MP_ERROR(val), 
      PMATH_AS_MP_VALUE(val), 
      right_val,
      MPFR_RNDA);
    mpfr_abs(
      PMATH_AS_MP_ERROR(val),
      PMATH_AS_MP_ERROR(val),
      MPFR_RNDU);
      
    pmath_unref(x);
    return val;
  }
  
  // give up when error is > Pi / 2
  if(mpfr_cmp_d(PMATH_AS_MP_ERROR(x), M_PI / 2) >= 0) {
    pmath_unref(x);
    return CINFTY;
  }
  
  err_x = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
  if(pmath_is_null(err_x)) {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  mpfr_add(
    PMATH_AS_MP_VALUE(err_x),
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDU);
    
  mpfr_tan(
    right_val,
    PMATH_AS_MP_VALUE(err_x),
    MPFR_RNDU);
    
  mpfr_sub(
    PMATH_AS_MP_VALUE(err_x),
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDD);
    
  mpfr_tan(
    left_val,
    PMATH_AS_MP_VALUE(err_x),
    MPFR_RNDD);
  
  pmath_unref(err_x); err_x = PMATH_NULL;
  
  if(mpfr_lessequal_p(right_val, left_val)) { // Tan is increasing, so we must have passed a pole
    pmath_unref(x);
    return CINFTY;
  }
    
  mpfr_sub(
    diff_val,
    right_val,
    left_val,
    MPFR_RNDA);
  
  if(!mpfr_number_p(diff_val)) {
    pmath_unref(x);
    return pmath_ref(_pmath_object_overflow);
  }
  
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
  
  mpfr_tan(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDN);
    
  _pmath_mp_float_include_error(val, right_val);
  _pmath_mp_float_include_error(val, left_val);
  
  pmath_unref(x);
  
  _pmath_mp_float_clip_error(val, min_prec, max_prec);
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
