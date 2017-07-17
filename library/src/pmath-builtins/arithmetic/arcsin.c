#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>


/** \brief Try to evaluate ArcSin(x) of an infinite value x
    \param expr  Pointer to the ArcSin-expression. On success, this will be replaced by the evaluation result.
    \param x     A pMath object. It won't be freed.
    \return Whether the evaluation succeeded. If TRUE is returned, \a expr will hold the result, otherwise it
            remains unchanged.
 */
static pmath_bool_t try_arcsin_of_infinity(pmath_t *expr, pmath_t x) {
  pmath_t infdir = _pmath_directed_infinity_direction(x);
  pmath_t re, im;
  if(_pmath_re_im(infdir, &re, &im) && pmath_is_number(re) && pmath_is_number(im)) {
    int isgn = pmath_number_sign(im);
    int rsgn = pmath_number_sign(re);
    
    pmath_unref(*expr);
    pmath_unref(re);
    pmath_unref(im);
    
    if(isgn < 0)
      *expr = pmath_expr_set_item(pmath_ref(x), 1, COMPLEX(INT(0), INT(-1)));
    else if(isgn > 0)
      *expr = pmath_expr_set_item(pmath_ref(x), 1, COMPLEX(INT(0), INT(1)));
    else if(rsgn < 0)
      *expr = pmath_expr_set_item(pmath_ref(x), 1, COMPLEX(INT(0), INT(1)));
    else if(rsgn > 0)
      *expr = pmath_expr_set_item(pmath_ref(x), 1, COMPLEX(INT(0), INT(-1)));
    else
      *expr = pmath_expr_set_item(pmath_ref(x), 1, INT(0));
    return TRUE;
  }
  
  pmath_unref(re);
  pmath_unref(im);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_arcsin(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  
  if(_pmath_complex_try_evaluate_acb(&expr, x, acb_asin)) {
    pmath_unref(x);
    return expr;
  }
  
  if(pmath_same(x, PMATH_FROM_INT32(0))) {
    pmath_unref(expr);
    return x;
  }
  
  if(pmath_same(x, PMATH_FROM_INT32(1))) {
    pmath_unref(expr);
    pmath_unref(x);
    return TIMES(QUOT(1, 2), pmath_ref(PMATH_SYMBOL_PI));
  }
  
  if(pmath_is_number(x) && pmath_number_sign(x) < 0) {
    x = pmath_number_neg(x);
    expr = pmath_expr_set_item(expr, 1, x);
    return NEG(expr);
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
  
  if(try_arcsin_of_infinity(&expr, x)) {
    pmath_unref(x);
    return expr;
  }
  
  pmath_unref(x);
  return expr;
}
