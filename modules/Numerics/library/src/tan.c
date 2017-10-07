#include "stdafx.h"
#include "util.h"

extern pmath_symbol_t pmath_System_Private_AutoSimplifyTrig;

PMATH_PRIVATE pmath_t eval_System_Tan(pmath_expr_t expr) {
  pmath_t x;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(pmath_complex_try_evaluate_acb(&expr, x, acb_tan)) { // TODO: return CINFTY when applicable
    pmath_unref(x);
    return expr;
  }
    
  x = pmath_expr_new_extended(
        pmath_ref(pmath_System_Private_AutoSimplifyTrig), 2,
        pmath_expr_get_item(expr, 0),
        x);
  
  x = pmath_evaluate(x);
  if(!pmath_same(x, PMATH_SYMBOL_FAILED)) {
    pmath_unref(expr);
    return x;
  }
  
  pmath_unref(x);
  return expr;
}
