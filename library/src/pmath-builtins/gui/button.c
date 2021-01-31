#include <pmath-util/evaluation.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/messages.h>


PMATH_PRIVATE pmath_t builtin_button(pmath_expr_t expr) {
  size_t i;
  
  size_t exprlen = pmath_expr_length(expr);
  if(exprlen < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  if(exprlen >= 2) {
    pmath_t item;
    
    for(i = exprlen; i > 2; --i) {
      expr = pmath_expr_set_item(
               expr, i,
               pmath_evaluate(
                 pmath_expr_get_item(expr, i)));
    }
    
    item = pmath_expr_get_item(expr, 2);
    if(pmath_is_rule(item)) {
      item = pmath_evaluate(item);
      expr = pmath_expr_set_item(expr, 2, item);
    }
    else
      pmath_unref(item);
  }
  
  if(exprlen > 2)
    pmath_unref(pmath_options_extract(expr, 2));
  
  return expr;
}
