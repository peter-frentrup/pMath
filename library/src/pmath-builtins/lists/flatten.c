#include <pmath-util/messages.h>

#include <pmath-builtins/lists-private.h>


PMATH_PRIVATE pmath_t builtin_flatten(pmath_expr_t expr) {
  /* Flatten(list)
     Flatten(list, tolevel)
     Flatten(list, tolevel, head)
   */
  pmath_expr_t list;
  pmath_t head;
  size_t depth, exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    return expr;
  }
  
  depth = SIZE_MAX;
  if(exprlen >= 2) {
    pmath_t depth_arg = pmath_expr_get_item(expr, 2);
    if(!extract_number(depth_arg, SIZE_MAX, &depth)) {
      pmath_unref(list);
      pmath_unref(depth_arg);
      return expr;
    }
    pmath_unref(depth_arg);
  }

  if(exprlen == 3)
    head = pmath_expr_get_item(expr, 3);
  else
    head = pmath_expr_get_item(list, 0);
    
  pmath_unref(expr);
  return pmath_expr_flatten(list, head, depth);
}
