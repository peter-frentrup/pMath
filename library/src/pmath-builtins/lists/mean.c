#include <pmath-core/numbers.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_mean(pmath_expr_t expr) {
  /* Mean(list) = (Plus @@ list) / Length(list)
   */
  pmath_t item;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  item = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr_of(item, PMATH_SYMBOL_LIST)
      || pmath_expr_length(item) == 0) {
    pmath_unref(item);
    return expr;
  }
  
  pmath_unref(expr);
  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_TIMES), 2,
           pmath_rational_new(
             PMATH_FROM_INT32(1),
             pmath_integer_new_uiptr(pmath_expr_length(item))),
           pmath_expr_set_item(item, 0, pmath_ref(PMATH_SYMBOL_PLUS)));
}
