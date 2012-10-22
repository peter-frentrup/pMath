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
static pmath_mpfloat_t mp_arcsin(pmath_mpfloat_t x) {
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
  
  if( mpfr_cmp_si(PMATH_AS_MP_VALUE(x),  1) > 0 ||
      mpfr_cmp_si(PMATH_AS_MP_VALUE(x), -1) < 0)
  {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  if(min_prec == max_prec) {
    val = _pmath_create_mp_float(1 + (mpfr_prec_t)ceil(min_prec));
    
    if(pmath_is_null(val)) {
      pmath_unref(x);
      return val;
    }
    
    mpfr_asin(
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
  
  if(mpfr_cmp_si(err_x, 1) > 0) 
    mpfr_set_si(err_x, 1, MPFR_RNDN);
  
  mpfr_asin(
    right_val,
    err_x,
    MPFR_RNDU);
    
  mpfr_sub(
    err_x,
    PMATH_AS_MP_VALUE(x),
    PMATH_AS_MP_ERROR(x),
    MPFR_RNDN);
    
  if(mpfr_cmp_si(err_x, -1) < 0) 
    mpfr_set_si(err_x, -1, MPFR_RNDN);
  
  mpfr_asin(
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
  
  val = _pmath_create_mp_float(1 + (mpfr_prec_t)ceil(prec1));
  
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return val;
  }
  
  mpfr_asin(
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

static pmath_t arcsin_as_log(pmath_t x) {
  // -I Log(I x + Sqrt(1 - x^2))
  pmath_t y = TIMES(
                COMPLEX(INT(0), INT(-1)),
                LOG(
                  PLUS(
                    TIMES(
                      COMPLEX(INT(0), INT(1)),
                      pmath_ref(x)),
                    SQRT(
                      MINUS(
                        INT(1),
                        POW(
                          pmath_ref(x),
                          INT(2)))))));
  pmath_unref(x);
  return y;
}

PMATH_PRIVATE pmath_t builtin_arcsin(pmath_expr_t expr) {
  pmath_t x;
  int xclass;
  
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
    if(d < -1.0 || d > 1.0)
      return arcsin_as_log(x);
      
    pmath_unref(x);
    return PMATH_FROM_DOUBLE(asin(d));
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_unref(expr);
    
    expr = mp_arcsin(pmath_ref(x));
    if(pmath_is_null(expr))
      return arcsin_as_log(x);
    
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_expr_of(x, PMATH_SYMBOL_TIMES)) {
    pmath_t fst = pmath_expr_get_item(x, 1);
    
    if(pmath_is_number(fst)) {
      if(pmath_number_sign(fst) < 0) {
        x = pmath_expr_set_item(x, 1, pmath_number_neg(fst));
        expr = pmath_expr_set_item(expr, 1, x);
        return TIMES(INT(-1), expr);
      }
    }
    
    pmath_unref(fst);
  }
  
  xclass = _pmath_number_class(x);
  
  if(xclass & PMATH_CLASS_ZERO) {
    pmath_unref(expr);
    return x;
  }
  
  if(xclass & PMATH_CLASS_POSONE) {
    pmath_unref(expr);
    pmath_unref(x);
    return TIMES(QUOT(1, 2), pmath_ref(PMATH_SYMBOL_PI));
  }
  
  if(xclass & PMATH_CLASS_NEG) {
    x = NEG(x);
    expr = pmath_expr_set_item(expr, 1, x);
    return NEG(expr);
  }
  
  if(xclass & PMATH_CLASS_INF) {
    pmath_t infdir = _pmath_directed_infinity_direction(x);
    pmath_t re, im;
    if( _pmath_re_im(infdir, &re, &im) &&
        pmath_is_number(re) &&
        pmath_is_number(im))
    {
      int isgn = pmath_number_sign(im);
      int rsgn = pmath_number_sign(re);
      
      pmath_unref(expr);
      pmath_unref(re);
      pmath_unref(im);
      
      if(isgn < 0)
        return pmath_expr_set_item(x, 1, INT(-1));
        
      if(isgn > 0)
        return pmath_expr_set_item(x, 1, INT(1));
        
      if(rsgn < 0)
        return pmath_expr_set_item(x, 1, INT(1));
        
      if(rsgn > 0)
        return pmath_expr_set_item(x, 1, INT(-1));
        
      return pmath_expr_set_item(x, 1, INT(0));
    }
    
    pmath_unref(re);
    pmath_unref(im);
    return expr;
  }
  
  if(xclass & PMATH_CLASS_COMPLEX) {
    pmath_unref(expr);
    return arcsin_as_log(x);
  }
  
  pmath_unref(x);
  return expr;
}
