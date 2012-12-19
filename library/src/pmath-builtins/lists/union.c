#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-util/debug.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_union(pmath_expr_t expr) {
  /* Union(list1, list2, ...)
   */
  const pmath_t *list_items;
  pmath_expr_t list;
  pmath_t current, item;
  size_t i, j, exprlen, joined_length;
  pmath_bool_t have_duplicates;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 0) {
    pmath_unref(expr);
    return pmath_ref(_pmath_object_emptylist);
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
  
  joined_length = 0;
  for(i = 1; i <= exprlen; ++i) {
    list = pmath_expr_get_item(expr, i);
    
    if(!pmath_is_expr(list)) {
      pmath_unref(list);
      
      pmath_message(PMATH_NULL, "nexprat", 2,
                    PMATH_FROM_INT32(1),
                    pmath_ref(expr));
      return expr;
    }
    
    joined_length += pmath_expr_length(list);
    current = pmath_expr_get_item(list, 0);
    pmath_unref(list);
    
    if(!pmath_equals(current, item)) {
      pmath_message(PMATH_NULL, "heads", 4,
                    item,
                    current,
                    PMATH_FROM_INT32(1),
                    pmath_integer_new_uiptr(i));
                    
      return expr;
    }
    
    pmath_unref(current);
  }
  
  // join all lists ...
  // list := Join(expr[1], expr[2], ...)
  // frees expr, item
  if(exprlen > 1) {
    struct _pmath_expr_t *joined_list;
    size_t k;
    
    joined_list = _pmath_expr_new_noinit(joined_length);
    if(!joined_list) {
      pmath_unref(item);
      return expr;
    }
    
    joined_list->items[0] = item;
    k = 1;
    for(i = 1; i <= exprlen; ++i) {
      size_t len;
      list = pmath_expr_get_item(expr, i);
      
      list_items = pmath_expr_read_item_data(list);
      
      if(list_items) {
        len = pmath_expr_length(list);
        for(j = 0; j < len; ++j, ++k)
          joined_list->items[k] = pmath_ref(list_items[j]);
      }
      else
        pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
        
      pmath_unref(list);
    }
    
    assert(k == joined_length + 1);
    
    pmath_unref(expr);
    list = PMATH_FROM_PTR(joined_list);
  }
  else {
    list = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    pmath_unref(item);
  }
  
  // sort ...
  list = pmath_expr_sort(list);
  
  // delete duplicates ...
  have_duplicates = FALSE;
  list_items      = pmath_expr_read_item_data(list);
  
  if(list_items) {
    i       = 1;
    exprlen = pmath_expr_length(list);
    
    while(i < exprlen) {
      item = list_items[i - 1];
      
      for(j = i + 1; j <= exprlen; ++j) {
        current = list_items[j - 1];
        
        if(pmath_equals(current, item)) {
          have_duplicates = TRUE;
          
          list       = pmath_expr_set_item(list, j, PMATH_UNDEFINED);
          list_items = pmath_expr_read_item_data(list);
          
          if(!list_items) {
            pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
            pmath_unref(list);
            return PMATH_NULL;
          }
        }
        else
          break;
      }
      
      i = j;
    }
  }
  else
    pmath_debug_print("[pmath_expr_read_item_data() gave NULL in %s:%d]\n", __FILE__, __LINE__);
    
  if(have_duplicates)
    return pmath_expr_remove_all(list, PMATH_UNDEFINED);
    
  return list;
}
