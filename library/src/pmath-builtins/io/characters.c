#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_characters(pmath_expr_t expr) {
  /* Characters(string)
   */
  pmath_string_t string;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  string = pmath_expr_get_item(expr, 1);
  
  if(pmath_is_string(string)) {
    size_t i;
    
    pmath_unref(expr);
    expr = pmath_expr_new(
             pmath_ref(PMATH_SYMBOL_LIST),
             (size_t)pmath_string_length(string));
             
    for(i = pmath_expr_length(expr); i > 0; --i)
      expr = pmath_expr_set_item(
               expr, i,
               pmath_string_part(pmath_ref(string), i - 1, 1));
  }
  
  pmath_unref(string);
  return expr;
}
