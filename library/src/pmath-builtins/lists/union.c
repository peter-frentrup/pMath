#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/messages.h>
#include <pmath-util/helpers.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/objects-private.h>
#include <pmath-core/expressions-private.h>

#include <pmath-builtins/lists-private.h>
#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_union(pmath_expr_t expr){
  /* Union(list1, list2, ...)
   */
  pmath_expr_t list;
  pmath_t current, item;
  size_t i, j, exprlen;
  pmath_bool_t have_duplicates;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen == 0){
    pmath_unref(expr);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  // check arguments (expressions with same head) ...
  // item := expr[1][0]
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
    pmath_unref(list);
    
    pmath_message(NULL, "nexprat", 2,
      pmath_integer_new_ui(1),
      pmath_ref(expr));
    return expr;
  }
  
  item = pmath_expr_get_item(list, 0);
  pmath_unref(list);
  
  for(i = 1;i <= exprlen;++i){
    list = pmath_expr_get_item(expr, i);
    
    if(!pmath_instance_of(list, PMATH_TYPE_EXPRESSION)){
      pmath_unref(list);
      
      pmath_message(NULL, "nexprat", 2,
        pmath_integer_new_ui(1),
        pmath_ref(expr));
      return expr;
    }
    
    current = pmath_expr_get_item(list, 0);
    pmath_unref(list);
    
    if(!pmath_equals(current, item)){
      pmath_message(NULL, "heads", 4,
        item,
        current,
        pmath_integer_new_size(1),
        pmath_integer_new_size(i));
      
      return expr;
    }
    
    pmath_unref(current);
  }
  
  // join all lists ... 
  // list := Join(expr[1], expr[2], ...)
  // frees expr, item
  if(exprlen > 1){
    pmath_gather_begin(NULL);
    
    for(i = 1;i <= exprlen;++i){
      list = pmath_expr_get_item(expr, i);
      
      for(j = 1;j <= pmath_expr_length(list);++j){
        current = pmath_expr_get_item(list, j);
        pmath_emit(current, NULL);
      }
      
      pmath_unref(list);
    }
    
    pmath_unref(expr);
    
    list = pmath_gather_end();
    list = pmath_expr_set_item(list, 0, item);
  }
  else{
    list = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    pmath_unref(item);
  }
  
  // sort ...
  list = pmath_expr_sort(list);
  
  // delete duplicates ...
  i = 1;
  have_duplicates = FALSE;
  exprlen = pmath_expr_length(list);
  while(i < exprlen){
    item = pmath_expr_get_item(list, i);
    
    for(j = i + 1;j <= exprlen;++j){
      current = pmath_expr_get_item(list, j);
      
      if(pmath_equals(current, item)){
        pmath_unref(current);
        list = pmath_expr_set_item(list, j, PMATH_UNDEFINED);
        have_duplicates = TRUE;
      }
      else{
        pmath_unref(current);
        break;
      }
    }
    
    pmath_unref(item);
    i = j;
  }
  
  if(have_duplicates)
    return pmath_expr_remove_all(list, PMATH_UNDEFINED);
  
  return list;
}
