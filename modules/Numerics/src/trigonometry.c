#include "stdafx.h"
#include "trigonometry.h"
#include "util.h"


PMATH_PRIVATE
pmath_bool_t pnum_try_simplify_degree(pmath_t *expr, pmath_t x) {
  pmath_t y;
  pmath_symbol_t head;
  
  assert(pmath_is_expr(*expr));
  
  if(!pnum_contains_nonhead_symbol(x, PMATH_SYMBOL_DEGREE))
    return FALSE;
  
  head = pmath_expr_get_item(*expr, 0);
  if(!pmath_is_symbol(head)) {
    pmath_unref(head);
    return FALSE;
  }
  
  y = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_WITH), 2,
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_LIST), 1,
          pmath_expr_new_extended(
            pmath_ref(PMATH_SYMBOL_ASSIGN), 2,
            pmath_ref(PMATH_SYMBOL_DEGREE),
            TIMES(pmath_ref(PMATH_SYMBOL_PI), QUOT(1, 180)))),
        pmath_ref(*expr));
        
  y = pmath_evaluate(y);
  if(!pmath_is_expr_of(y, head)) {
    pmath_unref(head);
    pmath_unref(*expr);
    *expr = y;
    return TRUE;
  }
  
  pmath_unref(head);
  pmath_unref(y);
  return FALSE;
}
