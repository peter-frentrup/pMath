#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>


#ifdef _MSC_VER
#  if _MSC_VER < 1800
#    define copysign _copysign
static pmath_bool_t signbit(double num) {
  return copysign(1.0, num) < 0;
}
#  endif
#endif


// 0 on error, -1 if sign bit set, +1 if sign bit unset
static int _pmath_signbit(pmath_t x) {
  if(pmath_is_double(x)) {
    if(signbit(PMATH_AS_DOUBLE(x)))
      return -1;
      
    return +1;
  }
  
  if(pmath_is_mpfloat(x)) {
    if(arb_is_positive(PMATH_AS_ARB(x)))
      return +1;
    if(arb_is_negative(PMATH_AS_ARB(x)))
      return -1;
    return 0;
  }
  
  if(pmath_is_rational(x)) {
    int sign_x = pmath_number_sign(x);
    
    return sign_x < 0 ? -1 : +1;
  }
  
  if(_pmath_is_infinite(x)) {
    int xclass = _pmath_number_class(x);
    
    if(xclass == PMATH_CLASS_NEGINF)
      return -1;
      
    if(xclass == PMATH_CLASS_POSINF)
      return +1;
      
    return 0;
  }
  
  return 0;
}

PMATH_PRIVATE pmath_t builtin_internal_copysign(pmath_expr_t expr) {
  pmath_t x;
  int new_sb, old_sb;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 2);
  new_sb = _pmath_signbit(x);
  pmath_unref(x);
  
  if(new_sb == 0)
    return expr;
    
  x = pmath_expr_get_item(expr, 1);
  if(pmath_is_double(x)) {
    pmath_unref(expr);
    
    return PMATH_FROM_DOUBLE(copysign(PMATH_AS_DOUBLE(x), new_sb));
  }
  
  if(pmath_is_mpfloat(x)) {
    pmath_mpfloat_t result = _pmath_create_mp_float(PMATH_AS_ARB_WORKING_PREC(x));
    if(pmath_is_null(result)) {
      pmath_unref(x);
      return expr;
    }
    
    if(new_sb > 0) {
      arb_abs(PMATH_AS_ARB(result), PMATH_AS_ARB(x));
    }
    else if(new_sb < 0) {
      arb_abs(PMATH_AS_ARB(result), PMATH_AS_ARB(x));
      arb_neg(PMATH_AS_ARB(result), PMATH_AS_ARB(result));
    }
    else
      arb_zero(PMATH_AS_ARB(result));
    
    pmath_unref(x);
    pmath_unref(expr);
    return result;
  }
  
  // Handle rationals & infinities.
  old_sb = _pmath_signbit(x);
  if(old_sb != 0) {
    pmath_unref(expr);
    
    if(old_sb == new_sb)
      return x;
      
    return NEG(x);
  }
  
  pmath_unref(x);
  return expr;
}

PMATH_PRIVATE pmath_t builtin_internal_signbit(pmath_expr_t expr) {
  pmath_t x;
  int sb;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  sb = _pmath_signbit(x);
  
  pmath_unref(x);
  if(sb == 0)
    return expr;
    
  pmath_unref(expr);
  if(sb < 0)
    return pmath_ref(PMATH_SYMBOL_TRUE);
    
  return pmath_ref(PMATH_SYMBOL_FALSE);
}
