#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

pmath_t builtin_total(pmath_expr_t expr){
/* Total(list)    = Plus @@ list
   
   
   TODO: see Kahan summation algorithm for higher accuracy
 */
  pmath_t item;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  item = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(item)){
    pmath_unref(item);
    pmath_message(PMATH_NULL, "nexprat", 2, pmath_integer_new_si(1), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  return pmath_expr_set_item(item, 0, pmath_ref(PMATH_SYMBOL_PLUS));
}
