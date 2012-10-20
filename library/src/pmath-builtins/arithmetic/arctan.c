#include <pmath-core/numbers-private.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


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
static pmath_mpfloat_t mp_arctan(pmath_mpfloat_t x) {
  pmath_thread_t thread = pmath_thread_get_current();
  double min_prec, max_prec, prec1, prec2;
  pmath_mpfloat_t val;
  
  MPFR_DECL_INIT(err_x,     PMATH_MP_ERROR_PREC);
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
  
  if(mpfr_cmp_abs(PMATH_AS_MP_VALUE(x), PMATH_AS_MP_ERROR(x)) < 0)
    return x;
  
  if(min_prec == max_prec) {
    val = _pmath_create_mp_float((mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(val)) {
      pmath_unref(x);
      return val;
    }
    
    mpfr_atan(
      PMATH_AS_MP_VALUE(val),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN);
    
    mpfr_set_d(left_val, -min_prec, MPFR_RNDN);
    mpfr_ui_pow(
      err_x,
      2,
      left_val,
      MPFR_RNDU);
      
    mpfr_mul(
      PMATH_AS_MP_ERROR(val), 
      PMATH_AS_MP_VALUE(val), 
      err_x,
      MPFR_RNDA);
    mpfr_abs(
      PMATH_AS_MP_ERROR(val),
      PMATH_AS_MP_ERROR(val),
      MPFR_RNDU);
      
    pmath_unref(x);
    return val;
  }
  
  mpfr_add(
    err_x,
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDU);
    
  mpfr_atan(
    right_val,
    err_x,
    MPFR_RNDU);
    
  mpfr_sub(
    err_x,
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDU);
    
  mpfr_atan(
    left_val,
    err_x,
    MPFR_RNDD);
    
  mpfr_sub(
    diff_val,
    right_val,
    left_val,
    MPFR_RNDA);
  
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
  
  mpfr_atan(
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

PMATH_PRIVATE pmath_t builtin_arctan(pmath_expr_t expr) {
  pmath_t x;
  int xclass;
  pmath_thread_t me = pmath_thread_get_current();
  
  if(!me)
    return expr;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_equals(x, PMATH_FROM_INT32(0))) {
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_is_double(x)) {
    double d = PMATH_AS_DOUBLE(x);
    pmath_unref(expr);
    pmath_unref(x);
    
    return PMATH_FROM_DOUBLE(atan(d));
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_unref(expr);
    
    x = mp_arctan(x);
    x = _pmath_float_exceptions(x);
    return x;
  }
  
  if(pmath_is_expr(x)) {
    pmath_t head = pmath_expr_get_item(x, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_TIMES)) {
      pmath_t fst = pmath_expr_get_item(x, 1);
      
      if(pmath_is_number(fst)) {
        if(pmath_number_sign(fst) < 0) {
          x = pmath_expr_set_item(x, 1, pmath_number_neg(fst));
          expr = pmath_expr_set_item(expr, 1, x);
          return NEG(expr);
        }
      }
      
      pmath_unref(fst);
      
      if(_pmath_is_imaginary(&x)) {
        expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_ARCTANH));
        expr = pmath_expr_set_item(expr, 1, x);
        return TIMES(COMPLEX(INT(0), INT(1)), expr);
      }
    }
  }
  
  xclass = _pmath_number_class(x);
  
  if(xclass & PMATH_CLASS_ZERO) {
    pmath_unref(expr);
    return x;
  }
  
  if(xclass & PMATH_CLASS_POSONE) {
    pmath_unref(expr);
    pmath_unref(x);
    return TIMES(QUOT(1, 4), pmath_ref(PMATH_SYMBOL_PI));
  }
  
  if(xclass & PMATH_CLASS_NEG) {
    x = NEG(x);
    expr = pmath_expr_set_item(expr, 1, x);
    return NEG(expr);
  }
  
  if(xclass & PMATH_CLASS_INF) {
    pmath_t infdir = _pmath_directed_infinity_direction(x);
    if(!pmath_same(infdir, PMATH_NULL)) {
      pmath_unref(expr);
      pmath_unref(x);
      
      if(pmath_equals(infdir, PMATH_FROM_INT32(0))) {
        pmath_unref(infdir);
        return pmath_ref(PMATH_SYMBOL_UNDEFINED);
      }
      
      return TIMES3(ONE_HALF, pmath_ref(PMATH_SYMBOL_PI), INV(infdir));
    }
  }
  
  if(xclass & PMATH_CLASS_COMPLEX) {
    pmath_t re = PMATH_NULL;
    pmath_t im = PMATH_NULL;
    
    if(_pmath_re_im(x, &re, &im)) {
      if(pmath_equals(re, PMATH_FROM_INT32(0))) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(x);
        
        return TIMES(COMPLEX(INT(0), INT(1)), FUNC(pmath_ref(PMATH_SYMBOL_ARCTANH), im));
      }
      
      if(pmath_is_mpfloat(re) || pmath_is_mpfloat(im)) {
        pmath_unref(re);
        pmath_unref(im);
        pmath_unref(expr);
        
        // ArcTan(x) = -1/2 I Log((1 + I x) / (1 - I x))
        expr = TIMES(
                 COMPLEX(INT(0), QUOT(-1, 2)),
                 LOG(DIV(
                       PLUS(INT(1), TIMES(COMPLEX(INT(0), INT(1)), pmath_ref(x))),
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
