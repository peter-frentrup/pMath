#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/lists-private.h>


static double dx_to_accuracy(mpfr_t dx) {
  mpfr_exp_t accexp;
  double accmant;
  
  // accuracy = -Log(2, dx)
  accmant = mpfr_get_d_2exp(&accexp, dx, MPFR_RNDN);
  return -log2(fabs(accmant)) - accexp;
}

static double double_max(double a, double b) {
  if(a < b)
    return a;
  return b;
}
/*
static mpfr_prec_t get_accprec(pmath_mpfloat_t x) {
  mpfr_exp_t err_exp, val_exp;
  mpfr_prec_t prec;
  
  assert(pmath_is_mpfloat(x));
  
  err_exp = mpfr_get_exp(PMATH_AS_MP_ERROR(x)) - 1;
  
  if(mpfr_regular_p(PMATH_AS_MP_VALUE(x))) {
    val_exp = mpfr_get_exp(PMATH_AS_MP_VALUE(x));
    if(val_exp < 0)
      val_exp = 0;
  }
  else
    val_exp = 0;
  
  prec = val_exp - err_exp;
  if(prec < MPFR_PREC_MIN)
    return MPFR_PREC_MIN;
  
  return prec;
}

static pmath_t mp_cos(pmath_mpfloat_t x) {
  mpfr_prec_t     xprec;
  pmath_thread_t  thread = pmath_thread_get_current();
  pmath_mpfloat_t val;
  
  assert(pmath_is_mpfloat(x));
  
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
    MPFR_DECL_INIT(err_exp, PMATH_MP_ERROR_PREC);
    
    val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(val)) {
      pmath_unref(x);
      return val;
    }
    
    mpfr_cos(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    mpfr_set_d(err_exp, -min_prec, MPFR_RNDU);
    if(mpfr_zero_p(val)) {
      mpfr_ui_pow(
        PMATH_AS_MP_ERROR(val),
        2,
        err_exp,
        MPFR_RNDU);
    }
    else {
      mpfr_ui_pow(
        PMATH_AS_MP_ERROR(val),
        2,
        err_exp,
        MPFR_RNDU);
        
      mpfr_mul(
        PMATH_AS_MP_ERROR(val),
        PMATH_AS_MP_VALUE(val),
        MPFR_RNDA);
      
      mpfr_abs(PMATH_AS_MP_ERROR(val), PMATH_AS_MP_ERROR(val), MPFR_RNDU);
    }
    
    pmath_unref(x);
    return val;
  }
  
  xprec = get_accprec(x);
  
  half_pi = 
  
}*/

// x will be freed, x may be PMATH_NULL
PMATH_PRIVATE void _pmath_sin_cos(
  pmath_mpfloat_t  x,
  pmath_mpfloat_t *sin_val,
  pmath_mpfloat_t *cos_val
) {
  pmath_thread_t thread = pmath_thread_get_current();
  double min_prec, max_prec;
  double sin_prec, cos_prec;
  pmath_mpfloat_t err_x;
  
  MPFR_DECL_INIT(left_sin,  PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(left_cos,  PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(right_sin, PMATH_MP_ERROR_PREC);
  MPFR_DECL_INIT(right_cos, PMATH_MP_ERROR_PREC);


  *sin_val = PMATH_NULL;
  *cos_val = PMATH_NULL;
  
  if(pmath_is_null(x))
    return;
    
  if(!thread) {
    pmath_unref(x);
    return;
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
    MPFR_DECL_INIT(err, PMATH_MP_ERROR_PREC);
    
    *sin_val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    *cos_val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(*sin_val) || pmath_is_null(*cos_val)) {
      pmath_unref(*sin_val); *sin_val = PMATH_NULL;
      pmath_unref(*cos_val); *cos_val = PMATH_NULL;
      pmath_unref(x);
      return;
    }
    
    mpfr_sin_cos(
      PMATH_AS_MP_VALUE(*sin_val),
      PMATH_AS_MP_VALUE(*cos_val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
      
    mpfr_set_d(err, -min_prec, MPFR_RNDN);
    mpfr_ui_pow(
      err,
      2,
      err,
      MPFR_RNDU);
      
    if(mpfr_cmp_abs(PMATH_AS_MP_VALUE(*sin_val), err) < 0) {
      mpfr_set(
        PMATH_AS_MP_ERROR(*sin_val),
        err,
        MPFR_RNDU);
    }
    else {
      mpfr_mul(
        PMATH_AS_MP_ERROR(*sin_val),
        PMATH_AS_MP_VALUE(*sin_val),
        err,
        MPFR_RNDA);
      mpfr_abs(
        PMATH_AS_MP_ERROR(*sin_val),
        PMATH_AS_MP_ERROR(*sin_val),
        MPFR_RNDU);
    }
    
    if(mpfr_cmp_abs(PMATH_AS_MP_VALUE(*cos_val), err) < 0) {
      mpfr_set(
        PMATH_AS_MP_ERROR(*cos_val),
        err,
        MPFR_RNDU);
    }
    else {
      mpfr_mul(
        PMATH_AS_MP_ERROR(*cos_val),
        PMATH_AS_MP_VALUE(*cos_val),
        err,
        MPFR_RNDA);
      mpfr_abs(
        PMATH_AS_MP_ERROR(*cos_val),
        PMATH_AS_MP_ERROR(*cos_val),
        MPFR_RNDU);
    }
    
    pmath_unref(x);
    return;
  }
  
  // give up when error is > 1.0 (= some arbitaray threshold < Pi)
  if(mpfr_cmp_d(PMATH_AS_MP_ERROR(x), 1.0) >= 0) {
    *sin_val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    *cos_val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(*sin_val) || pmath_is_null(*cos_val)) {
      pmath_unref(*sin_val); *sin_val = PMATH_NULL;
      pmath_unref(*cos_val); *cos_val = PMATH_NULL;
      pmath_unref(x);
      return;
    }
    
    mpfr_set_ui(PMATH_AS_MP_VALUE(*sin_val), 0, MPFR_RNDN);
    mpfr_set_ui(PMATH_AS_MP_ERROR(*sin_val), 1, MPFR_RNDN);
    
    mpfr_set_ui(PMATH_AS_MP_VALUE(*cos_val), 0, MPFR_RNDN);
    mpfr_set_ui(PMATH_AS_MP_ERROR(*cos_val), 1, MPFR_RNDN);
    
    pmath_unref(x);
    return;
  }
  
  err_x = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
  if(pmath_is_null(err_x)) {
    pmath_unref(x);
    return;
  }
  
  mpfr_add(
    PMATH_AS_MP_VALUE(err_x),
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDU);
    
  mpfr_sin_cos(
    right_sin,
    right_cos,
    PMATH_AS_MP_VALUE(err_x),
    MPFR_RNDN);
    
  mpfr_sub(
    PMATH_AS_MP_VALUE(err_x),
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDD);
    
  mpfr_sin_cos(
    left_sin,
    left_cos,
    PMATH_AS_MP_VALUE(err_x),
    MPFR_RNDN);
    
  pmath_unref(err_x); err_x = PMATH_NULL;
  
  // -1 <= Sin(x), Cos(x) <= 1  => precision === Log(2, |y|) + accuracy <= accuracy
  // accuracy = -Log(2, dx)
  if(mpfr_sgn(left_sin) < 0 && 0 < mpfr_sgn(right_sin)) {
    double left_cos_d  = mpfr_get_d(left_cos,  MPFR_RNDN);
    double right_cos_d = mpfr_get_d(right_cos, MPFR_RNDN);
    
    // Cos(x) ~= +1
    cos_prec = -log2(double_max(fabs(+1 - left_cos_d), fabs(+1 - right_cos_d)));
  }
  else if(mpfr_sgn(left_sin) > 0 && 0 > mpfr_sgn(right_sin)) {
    double left_cos_d  = mpfr_get_d(left_cos,  MPFR_RNDN);
    double right_cos_d = mpfr_get_d(right_cos, MPFR_RNDN);
    
    // Cos(x) ~= -1
    cos_prec = -log2(double_max(fabs(-1 - left_cos_d), fabs(-1 - right_cos_d)));
  }
  else {
    MPFR_DECL_INIT(diff_cos, PMATH_MP_ERROR_PREC);
    
    mpfr_sub(
      diff_cos,
      right_cos,
      left_cos,
      MPFR_RNDA);
      
    cos_prec = dx_to_accuracy(diff_cos);
  }
  
  if(mpfr_sgn(left_cos) < 0 && 0 < mpfr_sgn(right_cos)) {
    double left_sin_d  = mpfr_get_d(left_sin,  MPFR_RNDN);
    double right_sin_d = mpfr_get_d(right_sin, MPFR_RNDN);
    
    // Sin(x) ~= -1
    sin_prec = -log2(double_max(fabs(-1 - left_sin_d), fabs(-1 - right_sin_d)));
  }
  else if(mpfr_sgn(left_cos) > 0 && 0 > mpfr_sgn(right_cos)) {
    double left_sin_d  = mpfr_get_d(left_sin,  MPFR_RNDN);
    double right_sin_d = mpfr_get_d(right_sin, MPFR_RNDN);
    
    // Sin(x) ~= +1
    sin_prec = -log2(double_max(fabs(+1 - left_sin_d), fabs(+1 - right_sin_d)));
  }
  else {
    MPFR_DECL_INIT(diff_sin, PMATH_MP_ERROR_PREC);
    
    mpfr_sub(
      diff_sin,
      right_sin,
      left_sin,
      MPFR_RNDA);
      
    sin_prec = dx_to_accuracy(diff_sin);
  }
  
  
  if(sin_prec < min_prec)
    sin_prec  = min_prec;
  else if(sin_prec > max_prec)
    sin_prec       = max_prec;
    
  if(cos_prec < min_prec)
    cos_prec  = min_prec;
  else if(cos_prec > max_prec)
    cos_prec       = max_prec;
    
  *sin_val = _pmath_create_mp_float((mpfr_prec_t)ceil(sin_prec));
  *cos_val = _pmath_create_mp_float((mpfr_prec_t)ceil(cos_prec));
  
  if(pmath_is_null(*sin_val) || pmath_is_null(*cos_val)) {
    pmath_unref(*sin_val); *sin_val = PMATH_NULL;
    pmath_unref(*cos_val); *cos_val = PMATH_NULL;
    pmath_unref(x);
    return;
  }
  
  mpfr_sin_cos(
    PMATH_AS_MP_VALUE(*sin_val),
    PMATH_AS_MP_VALUE(*cos_val),
    PMATH_AS_MP_VALUE(x),
    MPFR_RNDN);
    
  _pmath_mp_float_include_error(*sin_val, left_sin);
  _pmath_mp_float_include_error(*sin_val, right_sin);
  
  _pmath_mp_float_include_error(*cos_val, left_cos);
  _pmath_mp_float_include_error(*cos_val, right_cos);
  
  if(mpfr_sgn(left_sin) < 0 && 0 < mpfr_sgn(right_sin)) {
    // Cos(x) ~= +1
    MPFR_DECL_INIT(one, PMATH_MP_ERROR_PREC);
    mpfr_set_si(one, 1, MPFR_RNDN);
    
    _pmath_mp_float_include_error(*cos_val, one);
  }
  else if(mpfr_sgn(left_sin) > 0 && 0 > mpfr_sgn(right_sin)) {
    // Cos(x) ~= -1
    MPFR_DECL_INIT(neg_one, PMATH_MP_ERROR_PREC);
    mpfr_set_si(neg_one, -1, MPFR_RNDN);
    
    _pmath_mp_float_include_error(*cos_val, neg_one);
  }
  
  if(mpfr_sgn(left_cos) < 0 && 0 < mpfr_sgn(right_cos)) {
    // Sin(x) ~= -1
    MPFR_DECL_INIT(neg_one, PMATH_MP_ERROR_PREC);
    mpfr_set_si(neg_one, -1, MPFR_RNDN);
    
    _pmath_mp_float_include_error(*sin_val, neg_one);
  }
  else if(mpfr_sgn(left_cos) > 0 && 0 > mpfr_sgn(right_cos)) {
    // Sin(x) ~= +1
    MPFR_DECL_INIT(one, PMATH_MP_ERROR_PREC);
    mpfr_set_si(one, 1, MPFR_RNDN);
    
    _pmath_mp_float_include_error(*sin_val, one);
  }
  
  pmath_unref(x);
  
  _pmath_mp_float_clip_error(*sin_val, min_prec, max_prec);
  _pmath_mp_float_clip_error(*cos_val, min_prec, max_prec);
  
  //*si = _pmath_float_exceptions(*si);
  //*co = _pmath_float_exceptions(*co);
}

PMATH_PRIVATE pmath_t builtin_cos(pmath_expr_t expr) {
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
    double res = cos(d);
    
    pmath_unref(x);
    pmath_unref(expr);
    return PMATH_FROM_DOUBLE(res);
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_mpfloat_t si, co;
    
    pmath_unref(expr);
    _pmath_sin_cos(x, &si, &co);
    pmath_unref(si);
    
    return _pmath_float_exceptions(co);
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
  
  if(pmath_same(x, PMATH_SYMBOL_PI)) {
    pmath_unref(expr);
    pmath_unref(x);
    return INT(-1);
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
    if(!pmath_is_expr_of(y, PMATH_SYMBOL_COS)) {
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
          
          return pmath_expr_set_item(
                   expr, 1,
                   pmath_expr_set_item(
                     x, 1,
                     pmath_number_neg(fst)));
        }
      }
      
      if(_pmath_is_imaginary(&x)) {
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_COSH));
        expr = pmath_expr_set_item(expr, 1, x);
        pmath_unref(fst);
        return expr;
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
            
            return INT(0);
          }
          
          cmp = pmath_evaluate(
                  pmath_expr_new_extended(
                    pmath_ref(PMATH_SYMBOL_LESS), 2,
                    pmath_ref(fst),
                    pmath_ref(_pmath_one_half)));
                    
          pmath_unref(cmp);
          if( pmath_same(cmp, PMATH_SYMBOL_TRUE)       &&
              pmath_is_quotient(fst)                   &&
              pmath_is_int32(PMATH_QUOT_NUM(fst))      &&
              pmath_is_int32(PMATH_QUOT_DEN(fst))      &&
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
                  
                  return ONE_HALF;
                  
                case 4:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  return INVSQRT(INT(2));
                  
                case 5:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  if(num == 1)
                    return PLUS(QUOT(1, 4), TIMES(QUOT(1, 4), SQRT(INT(5))));
                    
                  return PLUS(QUOT(-1, 4), TIMES(QUOT(1, 4), SQRT(INT(5))));
                  
                case 6:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  return TIMES(ONE_HALF, SQRT(INT(3)));
                  
                case 10:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  if(num == 1)
                    return SQRT(PLUS(QUOT(5, 8), TIMES(QUOT(1, 8), SQRT(INT(5)))));
                    
                  return SQRT(PLUS(QUOT(5, 8), TIMES(QUOT(-1, 8), SQRT(INT(5)))));
                  
                case 12:
                  pmath_unref(expr);
                  pmath_unref(x);
                  pmath_unref(fst);
                  
                  if(num == 1)
                    return TIMES3(ONE_HALF, PLUS(INT(1), SQRT(INT(3))), INVSQRT(INT(2)));
                    
                  return TIMES3(ONE_HALF, PLUS(INT(-1), SQRT(INT(3))), INVSQRT(INT(2)));
              }
          }
          else if(pmath_same(cmp, PMATH_SYMBOL_FALSE)) { // 1/2 Pi <= x
            expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
            
            cmp = pmath_evaluate(
                    pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_LESS), 2,
                      pmath_ref(fst),
                      PMATH_FROM_INT32(1)));
                      
            pmath_unref(cmp);
            if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // 1/2 Pi <= x < Pi
              fst = MINUS(INT(1), fst);
              
              x = pmath_expr_set_item(x, 1, fst);
              return NEG(pmath_expr_set_item(expr, 1, x)); // return -Cos((1-fst)*PI)
            }
            
            cmp = pmath_evaluate(
                    pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_LESS), 2,
                      pmath_ref(fst),
                      PMATH_FROM_INT32(2)));
                      
            pmath_unref(cmp);
            if(pmath_same(cmp, PMATH_SYMBOL_TRUE)) { // Pi <= x < 2 Pi
              fst = MINUS(INT(2), fst);
              
              x = pmath_expr_set_item(x, 1, fst);
              return pmath_expr_set_item(expr, 1, x); // return Cos((2-fst)*PI)
            }
            
            cmp = pmath_evaluate(
                    pmath_expr_new_extended(
                      pmath_ref(PMATH_SYMBOL_FLOOR), 2,
                      pmath_ref(fst),
                      PMATH_FROM_INT32(2)));
                      
            fst = MINUS(fst, cmp);
            
            x    = pmath_expr_set_item(x,    1, fst);
            return pmath_expr_set_item(expr, 1, x); // return Cos((fst - Floor(fst, 2))*PI)
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
          tmp = POW(INT(-1), tmp);
          
          expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
          x = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
          x = pmath_expr_remove_all(x, PMATH_UNDEFINED);
          expr = pmath_expr_set_item(expr, 1, x);
          
          return TIMES(tmp, expr);
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
        
        return FUNC(pmath_ref(PMATH_SYMBOL_COSH), im);
      }
      
      if(pmath_is_float(re) || pmath_is_float(im)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        expr = TIMES(
                 ONE_HALF,
                 PLUS(
                   EXP(COMPLEX(pmath_ref(im),  NEG(pmath_ref(re)))),
                   EXP(COMPLEX(NEG(pmath_ref(im)),     pmath_ref(re)))));
                   
        pmath_unref(re);
        pmath_unref(im);
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
        
        return u;
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCOT)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INVSQRT(PLUS(INT(1), POW(u, INT(-2)))); // 1/Sqrt(1 + 1/u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCCSC)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return SQRT(MINUS(INT(1), POW(u, INT(-2)))); // Sqrt(1 - 1/u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSEC)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INV(u); // 1/u
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCSIN)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return SQRT(MINUS(INT(1), POW(u, INT(2)))); // Sqrt(1 - u^2)
      }
      
      if(pmath_same(head, PMATH_SYMBOL_ARCTAN)) {
        pmath_unref(expr);
        pmath_unref(x);
        
        return INVSQRT(PLUS(INT(1), POW(u, INT(2)))); // 1 / Sqrt(1 + u^2)
      }
      
      pmath_unref(u);
    }
  }
  
  pmath_unref(x);
  return expr;
}
