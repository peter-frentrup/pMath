#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


#ifdef _MSC_VER
#  if _MSC_VER < 1800
#    define nexttoward _nextafter
#  endif
#endif


static pmath_t double_result(double x) {
  if(isfinite(x))
    return PMATH_FROM_DOUBLE(x);
    
  // TODO: generate overflow message?
  
  if(x < 0)
    return pmath_ref(_pmath_object_neg_infinity);
    
  return pmath_ref(_pmath_object_pos_infinity);
}

static pmath_t mp_result(pmath_mpfloat_t x) {
  int sgn;
  
  if(mpfr_number_p(PMATH_AS_MP_VALUE(x)))
    return x;
    
  // TODO: generate overflow message?
  
  sgn = mpfr_sgn(PMATH_AS_MP_VALUE(x));
  pmath_unref(x);
  
  if(sgn < 0)
    return pmath_ref(_pmath_object_neg_infinity);
    
  return pmath_ref(_pmath_object_pos_infinity);
}

static pmath_t next_toward_infinity(pmath_expr_t expr, pmath_t x, int inf_sign) {
  if(pmath_is_double(x)) {
    double x_double;
    
    pmath_unref(expr);
    
    x_double = PMATH_AS_DOUBLE(x);
    
    if(inf_sign < 0)
      x_double = nexttoward(x_double, -HUGE_VAL);
    else
      x_double = nexttoward(x_double, HUGE_VAL);
      
    return double_result(x_double);
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_mpfloat_t result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
    
    if(pmath_is_null(result)) {
      pmath_unref(x);
      return expr;
    }
    
    mpfr_set(
      PMATH_AS_MP_VALUE(result),
      PMATH_AS_MP_VALUE(x),
      MPFR_RNDN); // conversion is exact because both precisions are equal
      
    pmath_unref(x);
    pmath_unref(expr);
    
    if(inf_sign < 0)
      mpfr_nextbelow(PMATH_AS_MP_VALUE(result));
    else
      mpfr_nextabove(PMATH_AS_MP_VALUE(result));
      
    return mp_result(result);
  }
  
  return expr;
}


PMATH_PRIVATE pmath_t builtin_internal_nexttoward(pmath_expr_t expr) {
  pmath_t x, y;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(_pmath_is_infinite(x)) {
    pmath_unref(expr);
    return x;
  }
  
  y = pmath_expr_get_item(expr, 2);
  
  if(_pmath_is_infinite(y)) {
    int yclass = _pmath_number_class(y);
    pmath_unref(y);
    
    if(yclass == PMATH_CLASS_NEGINF)
      return next_toward_infinity(expr, x, -1);
      
    if(yclass == PMATH_CLASS_POSINF)
      return next_toward_infinity(expr, x, +1);
      
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_is_double(x)) {
    if(pmath_is_number(y)) {
      double y_double = pmath_number_get_d(y);
      
      pmath_unref(y);
      pmath_unref(expr);
      
      return double_result(nexttoward(PMATH_AS_DOUBLE(x), y_double));
    }
    
    pmath_unref(y);
    return expr;
  }
  
  if(pmath_is_mpfloat(x)) {
    if(pmath_is_number(y) && !pmath_is_mpfloat(y))
      y = pmath_set_precision(y, mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
      
    if(pmath_is_mpfloat(y)) {
      pmath_mpfloat_t result = _pmath_create_mp_float(mpfr_get_prec(PMATH_AS_MP_VALUE(x)));
      
      if(pmath_is_null(result)) {
        pmath_unref(x);
        pmath_unref(y);
        return expr;
      }
      
      mpfr_set(
        PMATH_AS_MP_VALUE(result),
        PMATH_AS_MP_VALUE(x),
        MPFR_RNDN); // conversion is exact because both precisions are equal
      
      mpfr_nexttoward(PMATH_AS_MP_VALUE(result), PMATH_AS_MP_VALUE(y));
      
      pmath_unref(x);
      pmath_unref(y);
      pmath_unref(expr);
      return mp_result(result);
    }
    
    pmath_unref(x);
    pmath_unref(y);
    return expr;
  }
  
  pmath_unref(x);
  pmath_unref(y);
  return expr;
}
