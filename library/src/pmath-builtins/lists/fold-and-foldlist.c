#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_List;

PMATH_PRIVATE pmath_t builtin_fold(pmath_expr_t expr) {
  /* Fold(list, f, x)      === FoldList(list, f, x)[-1]
   */
  pmath_t f, x;
  pmath_expr_t list;
  size_t i;
  
  if(pmath_expr_length(expr) != 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    
    pmath_unref(list);
    return expr;
  }
  
  f = pmath_expr_get_item(expr, 2);
  x = pmath_expr_get_item(expr, 3);
  pmath_unref(expr);
  
  for(i = 1; i <= pmath_expr_length(list); ++i) {
    x = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(f), 2,
            x,
            pmath_expr_get_item(list, i)));
  }
  
  pmath_unref(f);
  pmath_unref(list);
  return x;
}

PMATH_PRIVATE pmath_t builtin_foldlist(pmath_expr_t expr) {
  /* FoldList(list, f, x)
   */
  pmath_t f, x;
  pmath_expr_t list, result;
  size_t i;
  
  if(pmath_expr_length(expr) != 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    
    pmath_unref(list);
    return expr;
  }
  
  f = pmath_expr_get_item(expr, 2);
  x = pmath_expr_get_item(expr, 3);
  pmath_unref(expr);
  
  result = pmath_expr_new(
             pmath_ref(pmath_System_List),
             pmath_expr_length(list) + 1);
             
  for(i = 1; i <= pmath_expr_length(list); ++i) {
    result = pmath_expr_set_item(result, i, pmath_ref(x));
    
    x = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(f), 2,
            x,
            pmath_expr_get_item(list, i)));
  }
  
  result = pmath_expr_set_item(
             result,
             pmath_expr_length(result),
             x);
             
  pmath_unref(f);
  pmath_unref(list);
  return result;
}
