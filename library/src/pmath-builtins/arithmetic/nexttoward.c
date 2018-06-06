#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

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
    pmath_mpfloat_t result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    fmpz_t mant, exp;
    if(pmath_is_null(result)) {
      pmath_unref(x);
      return expr;
    }
    fmpz_init(exp);
    fmpz_init(mant);
    fmpz_sub_si(exp, ARF_EXPREF(arb_midref(PMATH_AS_ARB(x))), PMATH_AS_ARB_WORKING_PREC(x));
    fmpz_set_si(mant, inf_sign);
    arb_add_fmpz_2exp(PMATH_AS_ARB(result), PMATH_AS_ARB(x), mant, exp, ARF_PREC_EXACT);
    fmpz_clear(mant);
    fmpz_clear(exp);
    
    pmath_unref(x);
    pmath_unref(expr);
    return result;
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
      y = pmath_set_precision(y, PMATH_AS_ARB_WORKING_PREC(x));
      
    if(pmath_is_mpfloat(y)) {
      if(arb_lt(PMATH_AS_ARB(x), PMATH_AS_ARB(y))) {
        pmath_unref(y);
        return next_toward_infinity(expr, x, +1);
      }
      if(arb_gt(PMATH_AS_ARB(x), PMATH_AS_ARB(y))) {
        pmath_unref(y);
        return next_toward_infinity(expr, x, -1);
      }
      pmath_unref(y);
      pmath_unref(expr);
      return x;
    }
    
    pmath_unref(x);
    pmath_unref(y);
    return expr;
  }
  
  pmath_unref(x);
  pmath_unref(y);
  return expr;
}
