#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>


static pmath_bool_t sorted_contains(pmath_expr_t list, pmath_t item);


PMATH_PRIVATE pmath_t builtin_intersection(pmath_expr_t expr) {
  /* Intersection(list1, list2, ...)
   */
  pmath_expr_t list;
  pmath_t item;
  size_t i, j, exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1) {
    pmath_message_argxxx(exprlen, 1, SIZE_MAX);
    return expr;
  }
  
  // check arguments (expressions with same head) ...
  // item := expr[1][0]
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    
    pmath_message(PMATH_NULL, "nexprat", 2,
                  PMATH_FROM_INT32(1),
                  pmath_ref(expr));
    return expr;
  }
  
  item = pmath_expr_get_item(list, 0);
  pmath_unref(list);
  
  for(i = 1; i <= exprlen; ++i) {
    list = pmath_expr_get_item(expr, i);
    
    if(!pmath_is_expr(list)) {
      pmath_unref(list);
      
      pmath_message(PMATH_NULL, "nexprat", 2,
                    PMATH_FROM_INT32(1),
                    pmath_ref(expr));
      return expr;
    }
    
    pmath_t current = pmath_expr_get_item(list, 0);
    pmath_unref(list);
    
    if(!pmath_equals(current, item)) {
      pmath_message(PMATH_NULL, "heads", 4,
                    item,
                    current,
                    pmath_integer_new_uiptr(1),
                    pmath_integer_new_uiptr(i));
                    
      return expr;
    }
    
    pmath_unref(current);
  }
  
  pmath_unref(item);
  
  // sort all lists ...
  // expr[i] := Sort(expr[i]) ...
  for(i = 1; i <= exprlen; ++i) {
    list = pmath_expr_get_item(expr, i);
    expr = pmath_expr_set_item(expr, i, PMATH_NULL);
    
    list = pmath_expr_sort(list);
    
    expr = pmath_expr_set_item(expr, i, list);
  }
  
  list = pmath_expr_get_item(expr, 1);
  expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
  
  // put shortest list first
  size_t list_len = pmath_expr_length(list);
  for(j = 2; j <= exprlen; ++j) {
    pmath_t other_list = pmath_expr_get_item(expr, j);
    size_t other_len = pmath_expr_length(other_list);
    
    if(other_len < list_len) {
      expr = pmath_expr_set_item(expr, j, list);
      list = other_list;
      list_len = other_len; 
    }
    else
      pmath_unref(other_list);
  }
  
  size_t outlen = 0;
  i = 1;
  item = pmath_expr_get_item(list, i);
  while(i <= list_len) {
    pmath_bool_t include = TRUE;
    for(j = 2; include && j <= exprlen; ++j) {
      pmath_t other_list = pmath_expr_get_item(expr, j);
      
      include = sorted_contains(other_list, item);
      pmath_unref(other_list);
    }
    
    if(include) {
      list = pmath_expr_set_item(list, ++outlen, pmath_ref(item));
    }
    
    // find next item that is different
    while(++i <= list_len) {
      pmath_t prev_item = pmath_expr_get_item(list, i);
      if(pmath_equals(prev_item, item)) {
        pmath_unref(prev_item);
      }
      else {
        pmath_unref(item);
        item = prev_item;
        break;
      }
    }
  }
  
  pmath_unref(item);
  pmath_unref(expr);
  
  // TODO: maybe use new expression instead of expr-part if much shorter than original length ?
  return pmath_expr_get_item_range(list, 1, outlen);
}


static pmath_bool_t sorted_contains(pmath_expr_t list, pmath_t item) {
  size_t min = 1;
  size_t max = pmath_expr_length(list);
  
  while(min <= max) {
    size_t mid = min + (max - min + 1) / 2;
    
    pmath_t other = pmath_expr_get_item(list, mid);
    int cmp = pmath_compare(other, item);
    pmath_unref(other);
    
    if(cmp < 0)
      min = mid + 1;
    else if(cmp > 0)
      max = mid - 1;
    else
      return TRUE;
  }
  
  return FALSE;
}
