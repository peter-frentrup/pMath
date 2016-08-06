#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers.h>

#include <pmath-builtins/lists-private.h>

#include <pmath-util/debug.h>
#include <pmath-util/messages.h>


// check arguments, sum their lengths
// does not free expr
static pmath_bool_t check_all_same_head(pmath_expr_t expr, size_t *joined_length) {
  pmath_expr_t list;
  pmath_t head;
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  
  assert(joined_length != NULL);
  *joined_length = 0;
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    
    pmath_message(PMATH_NULL, "nexprat", 2,
                  PMATH_FROM_INT32(1),
                  pmath_ref(expr));
    return FALSE;
  }
  
  *joined_length = pmath_expr_length(list);
  head = pmath_expr_get_item(list, 0);
  pmath_unref(list);
  
  for(i = 2; i <= exprlen; ++i) {
    pmath_t other_head;
    
    list = pmath_expr_get_item(expr, i);
    if(!pmath_is_expr(list)) {
      pmath_unref(list);
      pmath_unref(head);
      
      pmath_message(PMATH_NULL, "nexprat", 2,
                    PMATH_FROM_INT32(1),
                    pmath_ref(expr));
      return FALSE;
    }
    
    *joined_length += pmath_expr_length(list);
    other_head = pmath_expr_get_item(list, 0);
    pmath_unref(list);
    
    if(!pmath_equals(other_head, head)) {
      pmath_message(PMATH_NULL, "heads", 4,
                    head,
                    other_head,
                    PMATH_FROM_INT32(1),
                    pmath_integer_new_uiptr(i));
                    
      return FALSE;
    }
    
    pmath_unref(other_head);
  }
  
  return TRUE;
}

// list := Join(expr[1], expr[2], ...), frees expr
static pmath_expr_t join_expr(pmath_expr_t expr, size_t joined_length) {
  size_t i, j, k, exprlen;
  pmath_t list;
  pmath_t result;
  
  list = pmath_expr_get_item(expr, 1);
  result = _pmath_expr_create_similar(list, joined_length);
  pmath_unref(list);
  
  j = 1;
  exprlen = pmath_expr_length(expr);
  for(i = 1; i <= exprlen; ++i) {
    list = pmath_expr_get_item(expr, i);
    joined_length = pmath_expr_length(list);
    
    for(k = 1; k <= joined_length; ++k, ++j) {
      result = pmath_expr_set_item(result, j, pmath_expr_get_item(list, k));
    }
    
    pmath_unref(list);
  }
  
  pmath_unref(expr);
  return result;
}

// frees list
static pmath_expr_t remove_duplicate_sequences(pmath_expr_t list) {
  size_t i, j;
  size_t len = pmath_expr_length(list);
  pmath_t first = pmath_expr_get_item(list, 1);
  
  i = 1;
  while(i <= len) {
    pmath_t next;
    
    ++i;
    next = pmath_expr_get_item(list, i);
    if(pmath_equals(first, next)) {
      --i;
      pmath_unref(next);
      break;
    }
    
    pmath_unref(first);
    first = next;
  }
  
  j = i-1;
  while(i <= len) {
    pmath_t next = PMATH_NULL;
    do {
      pmath_unref(next);
      ++i;
      next = pmath_expr_get_item(list, i);
    } while(i <= len && pmath_equals(first, next));
    
    list = pmath_expr_set_item(list, ++j, first);
    first = next;
  }
  
  pmath_unref(first);
  first = pmath_expr_get_item_range(list, 1, j);
  pmath_unref(list);
  return first;
}

PMATH_PRIVATE pmath_t builtin_union(pmath_expr_t expr) {
  /* Union(list1, list2, ...)
   */
  pmath_expr_t list;
  size_t exprlen, joined_length;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 0) {
    pmath_unref(expr);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  if(!check_all_same_head(expr, &joined_length))
    return expr;
  
  list = join_expr(expr, joined_length);
  list = pmath_expr_sort(list);
  list = remove_duplicate_sequences(list);
  
  return list;
}
