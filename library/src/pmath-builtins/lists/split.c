#include <pmath-core/numbers.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_split(pmath_expr_t expr) {
  /* Split(list, test)
     Split(list)        == Split(expr, Identical)
   */
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  pmath_t list, head, test;
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  head = pmath_expr_get_item(list, 0);
  
  if(exprlen == 1) {
    pmath_gather_begin(PMATH_NULL);
    
    pmath_unref(expr);
    i = 1;
    while(i <= pmath_expr_length(list)) {
      size_t j;
      pmath_t a = pmath_expr_get_item(list, i);
      
      pmath_gather_begin(PMATH_NULL);
      pmath_emit(pmath_ref(a), PMATH_NULL);
      
      for(j = i + 1;; ++j) {
        pmath_t b;
        
        if(j > pmath_expr_length(list)) {
          b = pmath_gather_end();
          b = pmath_expr_set_item(b, 0, pmath_ref(head));
          pmath_emit(b, PMATH_NULL);
          break;
        }
        
        b = pmath_expr_get_item(list, j);
        
        if(!pmath_equals(a, b)) {
          pmath_unref(b);
          
          b = pmath_gather_end();
          b = pmath_expr_set_item(b, 0, pmath_ref(head));
          pmath_emit(b, PMATH_NULL);
          break;
        }
        else
          pmath_emit(b, PMATH_NULL);
      }
      
      pmath_unref(a);
      i = j;
    }
    
    pmath_unref(list);
    list = pmath_gather_end();
    list = pmath_expr_set_item(list, 0, head);
    return list;
  }
  
  test = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  pmath_gather_begin(PMATH_NULL);
  
  i = 1;
  while(i <= pmath_expr_length(list)) {
    size_t j;
    pmath_t a = pmath_expr_get_item(list, i);
    
    pmath_gather_begin(PMATH_NULL);
    pmath_emit(pmath_ref(a), PMATH_NULL);
    
    for(j = i + 1;; ++j) {
      pmath_t b;
      pmath_t test_result;
      
      if(j > pmath_expr_length(list)) {
        b = pmath_gather_end();
        b = pmath_expr_set_item(b, 0, pmath_ref(head));
        pmath_emit(b, PMATH_NULL);
        break;
      }
      
      b = pmath_expr_get_item(list, j);
      
      test_result = pmath_evaluate(pmath_expr_new_extended(
                                     pmath_ref(test), 2,
                                     pmath_ref(a),
                                     pmath_ref(b)));
      pmath_unref(test_result);
      
      if(!pmath_same(test_result, PMATH_SYMBOL_FALSE)) {
        pmath_unref(b);
        
        b = pmath_gather_end();
        b = pmath_expr_set_item(b, 0, pmath_ref(head));
        pmath_emit(b, PMATH_NULL);
        break;
      }
      else
        pmath_emit(b, PMATH_NULL);
    }
    
    pmath_unref(a);
    i = j;
  }
  
  pmath_unref(list);
  pmath_unref(test);
  list = pmath_gather_end();
  list = pmath_expr_set_item(list, 0, head);
  return list;
}
