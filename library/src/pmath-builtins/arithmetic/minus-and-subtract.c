#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/build-expr-private.h>


extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_Plus;


PMATH_PRIVATE pmath_t builtin_minus(pmath_expr_t expr) {
  /* Minux(x)
   */
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  pmath_t x = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  if(pmath_is_number(x))
    return pmath_number_neg(x);
  return NEG(x);
}

PMATH_PRIVATE pmath_t builtin_subtract(pmath_expr_t expr) {
  /* Subtract(x, y)
   */
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  pmath_t y = pmath_expr_extract_item(expr, 2);
  if(pmath_is_number(y))
    y = pmath_number_neg(y);
  else
    y = NEG(y);
  expr = pmath_expr_set_item(expr, 2, y);
  expr = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_Plus));
  return expr;
}
