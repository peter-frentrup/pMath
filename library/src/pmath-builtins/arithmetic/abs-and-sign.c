#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/intervals-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/language-private.h>
#include <pmath-builtins/number-theory-private.h>

/** Assumes that for expr == f(...), the following is true:
      f(f(x)) = f(x)
      f(x * y) = f(x) * f(y)
    and tries to simplify expr uppon these assumptions.

    example: abs(-4 a Pi abs(b)) ---> 4 Pi abs(a b)

    Frees expr.
 */
static pmath_t simplify_abs_sign(pmath_expr_t expr) {
  pmath_t x;
  pmath_t head, xhead;
  size_t xlen;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(x)) {
    pmath_unref(x);
    return expr;
  }
  
  head = pmath_expr_get_item(expr, 0);
  xhead = pmath_expr_get_item(x, 0);
  xlen = pmath_expr_length(x);
  
  if(xlen == 1 && pmath_equals(xhead, head)) {
    pmath_unref(xhead);
    pmath_unref(head);
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_same(xhead, PMATH_SYMBOL_TIMES)) {
    pmath_expr_t extracted = pmath_expr_new(pmath_ref(PMATH_SYMBOL_TIMES), xlen);
    size_t ei = 0;
    size_t i;
    
    for(i = 1; i <= xlen; i++) {
      pmath_expr_t xi = pmath_expr_get_item(x, i);
      pmath_bool_t do_extract;
      
      if(pmath_is_expr(xi)
          && pmath_expr_length(xi) == 1) {
        pmath_t xi_head = pmath_expr_get_item(xi, 0);
        
        if(pmath_equals(head, xi_head)) {
          pmath_unref(xi_head);
          
          x = pmath_expr_set_item(x, i,
                                  pmath_expr_get_item(xi, 1));
          pmath_unref(xi);
          
          continue;
        }
        pmath_unref(xi_head);
      }
      
      xi = pmath_evaluate(FUNC(pmath_ref(head), xi));
      
      do_extract = !pmath_is_expr(xi) || pmath_expr_length(xi) != 1;
      
      if(!do_extract) {
        pmath_t xi_head = pmath_expr_get_item(xi, 0);
        do_extract = !pmath_equals(head, xi_head);
        pmath_unref(xi_head);
      }
      
      if(do_extract) {
        extracted = pmath_expr_set_item(extracted, ++ei, xi);
        x = pmath_expr_set_item(x, i, PMATH_UNDEFINED);
        continue;
      }
      
      pmath_unref(xi);
    }
    
    if(ei == xlen) {
      pmath_unref(xhead);
      pmath_unref(head);
      pmath_unref(expr);
      pmath_unref(x);
      return extracted;
    }
    
    if(ei > 0) {
      pmath_unref(xhead);
      pmath_unref(expr);
      
      x = _pmath_expr_shrink_associative(x, PMATH_UNDEFINED);
      expr = pmath_expr_get_item_range(extracted, 1, ei);
      pmath_unref(extracted);
      return TIMES(expr, FUNC(head, x));
    }
    
    pmath_unref(extracted);
  }
  
  pmath_unref(x);
  pmath_unref(xhead);
  pmath_unref(head);
  return expr;
}

/*============================================================================*/

PMATH_PRIVATE pmath_t builtin_abs(pmath_expr_t expr) {
  pmath_t x;
  int clazz;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_double(x)) {
    pmath_unref(expr);
    return PMATH_FROM_DOUBLE(fabs(PMATH_AS_DOUBLE(x)));
  }
  
  if(pmath_is_interval(x)) {
    pmath_unref(expr);
    return _pmath_interval_call(x, mpfi_abs);
  }
  
  clazz = _pmath_number_class(x);
  
  if(clazz & PMATH_CLASS_ZERO) {
    pmath_unref(expr);
    
    if(pmath_is_number(x))
      return x;
      
    // 0.0 + 0.0*I
    if(_pmath_is_machinenumber(x)) {
      pmath_unref(x);
      
      return PMATH_FROM_DOUBLE(0.0);
    }
    
    // 0`200 + 0`100*I
    if(_pmath_is_inexact(x)) {
      double prec = pmath_precision(x); // frees x
      
      return pmath_float_new_str("0", 2, PMATH_PREC_CTRL_GIVEN_PREC, prec);
    }
    
    pmath_unref(x);
    return INT(0);
  }
  
  if(clazz & PMATH_CLASS_POS) {
    pmath_unref(expr);
    
    return x;
  }
  
  if(clazz & PMATH_CLASS_NEG) {
    pmath_unref(expr);
    return NEG(x);
  }
  
  if(clazz & PMATH_CLASS_INF) {
    pmath_unref(expr);
    pmath_unref(x);
    return pmath_ref(_pmath_object_pos_infinity);
  }
  
  if(clazz & PMATH_CLASS_IMAGINARY) {
    pmath_unref(expr);
    return TIMES(COMPLEX(INT(0), INT(-1)), x);
  }
  
  if(_pmath_is_nonreal_complex(x)) {
    pmath_t re = pmath_expr_get_item(x, 1);
    pmath_t im = pmath_expr_get_item(x, 2);
    
    pmath_unref(x);
    pmath_unref(expr);
    return SQRT(PLUS(POW(re, INT(2)), POW(im, INT(2))));
  }
  
  if( pmath_equals(x, _pmath_object_overflow) ||
      pmath_equals(x, _pmath_object_underflow))
  {
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_is_expr_of_len(x, PMATH_SYMBOL_CONJUGATE, 1)) {
    expr = pmath_expr_set_item(expr, 1,
                               pmath_expr_get_item(x, 1));
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  return simplify_abs_sign(expr);
}

static int _mpfi_sign(mpfi_ptr rop, mpfi_srcptr op) {
  pmath_bool_t has_neg;
  pmath_bool_t has_zero;
  pmath_bool_t has_pos;
  pmath_bool_t need_init;
  
  if(mpfi_nan_p(op)) {
    mpfr_set_nan(&rop->left);
    mpfr_set_nan(&rop->right);
    mpfr_set_nanflag();
    return 0;
  }
  
  has_neg = !mpfi_is_nonneg(op);
  has_zero = mpfi_has_zero(op);
  has_pos = !mpfi_is_nonpos(op);
  
  need_init = TRUE;
  if(has_neg) {
    mpfi_set_si(rop, -1);
    need_init = FALSE;
  }
  if(has_zero) {
    if(need_init) {
      mpfi_set_si(rop, 0);
      need_init = FALSE;
    }
    else
      mpfi_put_si(rop, 0);
  }
  if(has_pos) {
    if(need_init) {
      mpfi_set_si(rop, 1);
      need_init = FALSE;
    }
    else
      mpfi_put_si(rop, 1);
  }
  
  return 0;
}

PMATH_PRIVATE pmath_t builtin_sign(pmath_expr_t expr) {
  pmath_t x;
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_number(x)) {
    int sign;
    
    pmath_unref(expr);
    sign = pmath_number_sign(x);
    pmath_unref(x);
    return PMATH_FROM_INT32(sign);
  }
  
  if(pmath_is_interval(x)) {
    pmath_unref(expr);
    if(mpfr_sgn(&PMATH_AS_MP_INTERVAL(x)->left) > 0) {
      pmath_unref(x);
      return INT(1);
    }
    else if(mpfr_sgn(&PMATH_AS_MP_INTERVAL(x)->right) < 0) {
      pmath_unref(x);
      return INT(-1);
    }
    else if(mpfi_is_zero(PMATH_AS_MP_INTERVAL(x))) {
      pmath_unref(x);
      return INT(0);
    }
    return _pmath_interval_call(x, _mpfi_sign);
  }
  
  if( pmath_equals(x, _pmath_object_overflow) ||
      pmath_equals(x, _pmath_object_underflow))
  {
    pmath_unref(expr);
    return x;
  }
  
  {
    pmath_t xinfdir = _pmath_directed_infinity_direction(x);
    if(!pmath_same(xinfdir, PMATH_NULL)) {
      pmath_unref(x);
      pmath_unref(expr);
      if(pmath_is_number(xinfdir) && pmath_number_sign(xinfdir) == 0) {
        pmath_unref(xinfdir);
        return pmath_ref(PMATH_SYMBOL_UNDEFINED);
      }
      
      return pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_SIGN), 1,
               xinfdir);
    }
  }
  
  if(pmath_is_numeric(x)) {
    int clazz = _pmath_number_class(x);
    
    if(clazz & PMATH_CLASS_POS){
      pmath_unref(expr);
      pmath_unref(x);
      
      return INT(1);
    }
  
    if(clazz & PMATH_CLASS_NEG){
      pmath_unref(expr);
      pmath_unref(x);
      
      return INT(-1);
    }
  
    if(clazz & PMATH_CLASS_ZERO){
      pmath_unref(expr);
      pmath_unref(x);
      
      return INT(0);
    }
  
    expr = pmath_expr_set_item(expr, 0, pmath_ref(PMATH_SYMBOL_ABS));
    return DIV(x, expr);
  }
  
  pmath_unref(x);
  return simplify_abs_sign(expr);
}
