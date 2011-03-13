#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_intersection(pmath_expr_t expr){
  /* Intersection(list1, list2, ...)
   */
  pmath_expr_t list, other_list;
  pmath_t current, item;
  size_t i, j, k, exprlen;
  int cmp;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1){
    pmath_message_argxxx(exprlen, 1, SIZE_MAX);
    return expr;
  }
  
  // check arguments (expressions with same head) ...
  // item := expr[1][0]
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)){
    pmath_unref(list);
    
    pmath_message(PMATH_NULL, "nexprat", 2,
      PMATH_FROM_INT32(1),
      pmath_ref(expr));
    return expr;
  }
  
  item = pmath_expr_get_item(list, 0);
  pmath_unref(list);
  
  for(i = 1;i <= exprlen;++i){
    list = pmath_expr_get_item(expr, i);
    
    if(!pmath_is_expr(list)){
      pmath_unref(list);
      
      pmath_message(PMATH_NULL, "nexprat", 2,
        PMATH_FROM_INT32(1),
        pmath_ref(expr));
      return expr;
    }
    
    current = pmath_expr_get_item(list, 0);
    pmath_unref(list);
    
    if(!pmath_equals(current, item)){
      pmath_message(PMATH_NULL, "heads", 4,
        item,
        current,
        pmath_integer_new_size(1),
        pmath_integer_new_size(i));
      
      return expr;
    }
    
    pmath_unref(current);
  }
  
  pmath_unref(item);
  
  // sort all lists ... 
  // expr[i] := Sort(expr[i]) ...
  for(i = 1;i <= exprlen;++i){
    list = pmath_expr_get_item(expr, i);
    expr = pmath_expr_set_item(expr, i, PMATH_NULL);
    
    list = pmath_expr_sort(list);
    
    expr = pmath_expr_set_item(expr, i, list);
  }
  
  // mark all items from first list that are not in all others ...
  // list:= expr[1]
  list = pmath_expr_get_item(expr, 1);
  expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
  for(i = pmath_expr_length(list);i > 0;--i){
    item = pmath_expr_get_item(list, i);
    
    for(j = 2;j <= exprlen;++j){
      other_list = pmath_expr_get_item(expr, j);
      
      cmp = 1;
      for(k = pmath_expr_length(other_list);k > 0;--k){
        current = pmath_expr_get_item(other_list, k);
        
        cmp = pmath_compare(item, current);
        
        pmath_unref(current);
        if(cmp <= 0)
          break;
      }
      
      pmath_unref(other_list);
      
      if(cmp != 0)
        list = pmath_expr_set_item(list, i, PMATH_UNDEFINED);
    }
    
    pmath_unref(item);
  }
  
  pmath_unref(expr);
  
  // delete duplicates in first list ...
  i = 1;
  exprlen = pmath_expr_length(list);
  while(i < exprlen){
    item = pmath_expr_get_item(list, i);
    
    for(j = i + 1;j <= exprlen;++j){
      current = pmath_expr_get_item(list, j);
      
      if(pmath_equals(current, item)){
        pmath_unref(current);
        list = pmath_expr_set_item(list, j, PMATH_UNDEFINED);
      }
      else{
        pmath_unref(current);
        break;
      }
    }
    
    pmath_unref(item);
    i = j;
  }
  
  return pmath_expr_remove_all(list, PMATH_UNDEFINED);
}
