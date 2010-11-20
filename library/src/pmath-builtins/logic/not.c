#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_not(pmath_expr_t expr){
  pmath_t item;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  item = pmath_expr_get_item(expr, 1);
  if(item == PMATH_SYMBOL_TRUE){
    pmath_unref(item);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FALSE);
  }
  
  if(item == PMATH_SYMBOL_FALSE){
    pmath_unref(item);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_TRUE);
  }
  
  if(item == PMATH_SYMBOL_UNDEFINED){
    pmath_unref(expr);
    return item;
  }
  
  if(pmath_instance_of(item, PMATH_TYPE_EXPRESSION)){
    pmath_t head = pmath_expr_get_item((pmath_expr_t)item, 0);
    if(head == PMATH_SYMBOL_NOT 
    && pmath_expr_length((pmath_expr_t)item) == 1){
      pmath_t result = pmath_expr_get_item((pmath_expr_t)item, 1);
      pmath_unref(head);
      pmath_unref(item);
      pmath_unref(expr);
      return result;
    }
    pmath_unref(head);
  }
  pmath_unref(item);
  
  return expr;
}
