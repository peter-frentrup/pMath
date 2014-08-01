#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


// x will be freed, x may be PMATH_NULL
static pmath_mpfloat_t mp_arcsin(pmath_mpfloat_t x) {
  pmath_mpfloat_t val;
  
  if(pmath_is_null(x))
    return PMATH_NULL;
    
  assert(pmath_is_mpfloat(x));
  
  if( mpfr_cmp_si(PMATH_AS_MP_VALUE(x),  1) > 0 ||
      mpfr_cmp_si(PMATH_AS_MP_VALUE(x), -1) < 0)
  {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  val = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
  
  if(pmath_is_null(val)) {
    pmath_unref(x);
    return val;
  }
  
  mpfr_asin(
    PMATH_AS_MP_VALUE(val),
    PMATH_AS_MP_VALUE(x),
    _pmath_current_rounding_mode());
    
  pmath_unref(x);
  
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
