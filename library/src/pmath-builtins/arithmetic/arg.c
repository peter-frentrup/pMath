#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>


PMATH_PRIVATE pmath_t builtin_arg(pmath_expr_t expr) {
  pmath_t z, re, im;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_float(z) || pmath_is_expr_of_len(z, PMATH_SYMBOL_COMPLEX, 2)) {
    acb_t z_val;
    slong prec;
    pmath_bool_t is_machine_prec;
    acb_init(z_val);
    if(_pmath_complex_float_extract_acb(z_val, &prec, &is_machine_prec, z)) {
      pmath_mpfloat_t result = _pmath_create_mp_float(prec);
      if(!pmath_is_null(result)) {
        pmath_unref(z);
        pmath_unref(expr);
        acb_arg(PMATH_AS_ARB(result), z_val, prec);
        acb_clear(z_val);
        if(is_machine_prec) {
          double d = arf_get_d(arb_midref(PMATH_AS_ARB(result)), ARF_RND_NEAR);
          if(isfinite(d)) {
            pmath_unref(result);
            return PMATH_FROM_DOUBLE(d);
          }
        }
        return result;
      }
    }
    acb_clear(z_val);
  }
  
  if(!pmath_is_numeric(z)) {
    pmath_unref(z);
    return expr;
  }
  
  if(_pmath_re_im(z, &re, &im)) {
    int re_sign = _pmath_numeric_sign(re);
    int im_sign = _pmath_numeric_sign(im);
    
    if(re_sign != PMATH_UNKNOWN_REAL_SIGN && im_sign != PMATH_UNKNOWN_REAL_SIGN) {
      pmath_unref(expr);
      
      if(re_sign < 0) {
        if(im_sign < 0)
          return MINUS(ARCTAN(DIV(im, re)), pmath_ref(PMATH_SYMBOL_PI));
        return PLUS(ARCTAN(DIV(im, re)), pmath_ref(PMATH_SYMBOL_PI));
      }
      if(re_sign == 0) {
        if(im_sign < 0)
          return TIMES(QUOT(-1, 2), pmath_ref(PMATH_SYMBOL_PI));
        return TIMES(ONE_HALF, pmath_ref(PMATH_SYMBOL_PI));
      }
      return ARCTAN(DIV(im, re));
    }
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return expr;
}
